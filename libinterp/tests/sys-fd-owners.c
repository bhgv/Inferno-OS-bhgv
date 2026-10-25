#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
char Ebadarg[]="bad argument";
static WORD descriptorarg=17;
static char *raised;
char exHeap[] = "heap", exNegsize[] = "negative", exBounds[] = "bounds", exNomem[] = "memory", exNilref[] = "nil";
FD_DECLARATION
void freeFD(Heap*, int);
Type Tstring = {1};
static Type descriptor = {1, 0, 0, sizeof(FD)};
static Type *TFD = &descriptor;
static Osenv environment;
static Fgrp group;
int mutator, nprop;
static int allocations, failure, live, checks;
static struct { Heap *heap; int size; } blocks[256];

/* Retain real runtime error frames without starting an emulator process. */
Proc*
getup(void)
{
	return &worker;
}

/* Route allocation errors through the runtime's nested nonlocal error frames. */
void
nexterror(void)
{
	assert(worker.nerr > 0);
	siglongjmp(worker.estack[--worker.nerr], 1);
}

/* Keep the original VM error observable after cleanup and rethrow. */
void
error(char *message)
{
	raised = message;
	strcpy(environment.errstr, message);
	nexterror();
}

/* Heap blocks are counted explicitly; process inspection for LSan is unavailable. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Poison exact-sized payloads and fail individual VM-owner allocations. */
Heap*
nheap(int size)
{
	Heap *heap;
	int i;

	if(++allocations == failure)
		error(exHeap);
	assert(size >= 0 && size < 4096);
	heap = malloc(sizeof(Heap)+size+32);
	assert(heap);
	memset(heap, 0, sizeof(Heap));
	memset(heap+1, 0xa5, size+32);
	heap->ref = 1;
	for(i = 0; i < 256 && blocks[i].heap; i++);
	assert(i < 256);
	blocks[i].heap = heap;
	blocks[i].size = size;
	live++;
	return heap;
}

/* Expose exact payload bounds to the production string constructors. */
int
hmsize(void *heap)
{
	int i;

	for(i = 0; i < 256; i++)
		if(blocks[i].heap == heap)
			return sizeof(Heap)+blocks[i].size;
	abort();
}

static int returnoffset;
static union { uvlong align; uchar bytes[32]; } returnslot;
static void *watchedowner;
static uchar *watchedslot, *pipewatchslot;
static void *pipewatchold[2];
static int pipewatch, connectionwatch;
static uchar *connectionslot;
static void *connectionold[3];

/* Validate heap guards and run the real descriptor finalizer exactly once. */
void
destroy(void *value)
{
	Heap *heap;

	int i, j;

	if(value == H)
		return;
	if(connectionwatch && (value == connectionold[0] || value == connectionold[1] || value == connectionold[2])){
		Sys_Connection published;
		memmove(&published,connectionslot,sizeof(published));
		assert(published.dfd != connectionold[0] && published.dfd != connectionold[1]);
		assert(published.cfd != connectionold[0] && published.cfd != connectionold[1]);
		assert(published.dir != connectionold[2]);
	}
	if(pipewatch && (value == pipewatchold[0] || value == pipewatchold[1])){
		void *published[2];
		memmove(published,pipewatchslot,sizeof(published));
		assert(published[0] != pipewatchold[0] && published[0] != pipewatchold[1]);
		assert(published[1] != pipewatchold[0] && published[1] != pipewatchold[1]);
	}
	if(value == watchedowner){
		void *published;
		memmove(&published,watchedslot,sizeof(published));
		assert(published == H);
		watchedowner = H;
	}
	heap = D2H(value);
	for(i = 0; i < 256 && blocks[i].heap != heap; i++);
	assert(i < 256 && heap->ref > 0);
	for(j = 0; j < 32; j++)
		assert(((uchar*)(heap+1))[blocks[i].size+j] == 0xa5);
	if(--heap->ref)
		return;
	if(heap->t == TFD){
		freeFD(heap, 0);
		TFD->ref--;
	}else{
		assert(heap->t == &Tstring);
		assert(((String*)value)->tmp == nil);
		Tstring.ref--;
	}
	blocks[i].heap = nil;
	free(heap);
	live--;
}

static int active[256], nextfd, nativefail, released, operation, nativecalls;
static int issued[2];
static char returnedpath[] = "/net/t\303\251st/17";

/* Descriptor heap allocation is the fallible boundary used by the real constructor. */
Heap*
heap(Type *type)
{
	Heap *h;

	assert(type == TFD);
	h = nheap(type->size);
	h->t = type;
	type->ref++;
	return h;
}

/* No tested input string needs a rune-to-byte conversion cache. */
ulong
msize(void *pointer)
{
	(void)pointer;
	abort();
}

/* A descriptor owner retains precisely one reference to its native file group. */
int
incref(Ref *reference)
{
	assert(reference == &group.r);
	return ++reference->ref;
}

/* Finalization must release each retained file-group reference. */
void
closefgrp(Fgrp *fgrp)
{
	assert(fgrp == &group && fgrp->r.ref > 1);
	fgrp->r.ref--;
}

/* Native I/O and closing descriptors may run only outside the VM scheduler lock. */
void
release(void)
{
	assert(!released);
	released = 1;
}

/* Every native transition must restore scheduler ownership even during error cleanup. */
void
acquire(void)
{
	assert(released);
	released = 0;
}

/* Allocate distinct native descriptors, including descriptor zero. */
static int
newfd(void)
{
	int fd;

	fd = nextfd++;
	assert(fd >= 0 && fd < 256 && !active[fd]);
	active[fd] = 1;
	return fd;
}

/* Detect leaks and double closes; deliberately replace the error string during close. */
int
kfgrpclose(Fgrp *fgrp, int fd)
{
	assert(released && fgrp == &group && fd >= 0 && fd < 256 && active[fd]);
	active[fd] = 0;
	strcpy(environment.errstr, "close changed error");
	return 0;
}

/* Raw descriptors belong to the same current file group as newly wrapped handles. */
int
kclose(int fd)
{
	return kfgrpclose(&group, fd);
}

/* Native errors produce no descriptors and expose a recognizable error string. */
static int
nativebegin(void)
{
	assert(released);
	nativecalls++;
	if(nativefail){
		strcpy(environment.errstr, "native failure");
		return 0;
	}
	return 1;
}

/* Simulate opening one data descriptor without replacing the production Sys wrapper. */
int
kopen(char *path, int mode)
{
	assert(operation == 1 && strcmp(path, "address") == 0 && mode == 2);
	if(!nativebegin())
		return -1;
	return issued[0] = newfd();
}

/* Simulate native create with the exact arguments passed by the real builtin. */
int
kcreate(char *path, int mode, ulong permissions)
{
	assert(operation == 2 && strcmp(path, "address") == 0 && mode == 2 && permissions == 0640);
	if(!nativebegin())
		return -1;
	return issued[0] = newfd();
}

/* Preserve a normal argument error without throwing through an ownership transition. */
void
seterror(char *message,...)
{
	assert(message==Ebadarg);
	strcpy(environment.errstr,message);
}

/* Simulate duplication of a numeric descriptor into a new native slot. */
int
kdup(int fd, int destination)
{
	assert(operation == 3 && fd == (int)descriptorarg && destination == -1);
	if(!nativebegin())
		return -1;
	return issued[0] = newfd();
}

/* A successful pipe returns two independent descriptors that must both be adopted. */
int
kpipe(int *fd)
{
	assert(operation == 4);
	if(!nativebegin())
		return -1;
	fd[0] = issued[0] = newfd();
	fd[1] = issued[1] = newfd();
	return 0;
}

/* Dial supplies both data and control owners plus a UTF-8 directory. */
int
kdial(char *address, char *local, char *directory, int *controlfd)
{
	assert(operation == 5 && strcmp(address, "address") == 0 && strcmp(local, "local") == 0);
	if(!nativebegin())
		return -1;
	strcpy(directory, returnedpath);
	*controlfd = issued[1] = newfd();
	return issued[0] = newfd();
}

/* Announce supplies only a control descriptor. */
int
kannounce(char *address, char *directory)
{
	assert(operation == 6 && strcmp(address, "address") == 0);
	if(!nativebegin())
		return -1;
	strcpy(directory, returnedpath);
	return issued[1] = newfd();
}

/* Listen may borrow its input directory from the previous result. */
int
klisten(char *input, char *directory)
{
	assert(operation == 7 && strcmp(input, "previous directory") == 0);
	if(!nativebegin())
		return -1;
	strcpy(directory, returnedpath);
	return issued[1] = newfd();
}

IMPLEMENTATION

static Sys_FD *single, *slots[3];
static String *address, *local;
static struct { WORD t0; Sys_Connection t1; } connection;
static union { uvlong align; uchar bytes[sizeof(connection)+16]; } connectionreturn;
static WORD status;
static Array pipearray;
static int pipesize, pipebad;
static union { uvlong align; uchar bytes[3*sizeof(Sys_FD*)+16]; } pipeslots;

/* Dispatch actual generated Sys frames and retain output state outside nonlocal jumps. */
static int
invoke(void *unused)
{
	F_Sys_open openframe = {0};
	F_Sys_create createframe = {0};
	F_Sys_fildes fildesframe = {0};
	F_Sys_pipe pipeframe = {0};
	F_Sys_dial dialframe = {0};
	F_Sys_announce announceframe = {0};
	F_Sys_listen listenframe = {0};

	(void)unused;
	switch(operation){
	case 0:
		issued[0] = newfd();
		single = mkfd(issued[0]);
		break;
	case 1:
		openframe.ret = (void*)(returnslot.bytes+8+returnoffset);
		openframe.s = address;
		openframe.mode = 2;
		Sys_open(&openframe);
		break;
	case 2:
		createframe.ret = (void*)(returnslot.bytes+8+returnoffset);
		createframe.s = address;
		createframe.mode = 2;
		createframe.perm = 0640;
		Sys_create(&createframe);
		break;
	case 3:
		fildesframe.ret = (void*)(returnslot.bytes+8+returnoffset);
		fildesframe.fd = descriptorarg;
		Sys_fildes(&fildesframe);
		break;
	case 4:
		pipeframe.ret = (void*)(returnslot.bytes+8+returnoffset);
		pipeframe.fds = pipebad == 1 ? nil : pipesize < 0 ? H : &pipearray;
		Sys_pipe(&pipeframe);
		break;
	case 5:
		dialframe.ret = (void*)(connectionreturn.bytes+returnoffset);
		dialframe.addr = address;
		dialframe.local = local;
		Sys_dial(&dialframe);
		break;
	case 6:
		announceframe.ret = (void*)(connectionreturn.bytes+returnoffset);
		announceframe.addr = address;
		Sys_announce(&announceframe);
		break;
	case 7:
		listenframe.ret = (void*)(connectionreturn.bytes+returnoffset);
		listenframe.c = connection.t1;
		Sys_listen(&listenframe);
		break;
	}
	return 1;
}

/* Compare the converted UTF-8 path independently of the production string decoder. */
static void
checkpath(String *s)
{
	Rune expected[] = {'/', 'n', 'e', 't', '/', 't', 0xe9, 's', 't', '/', '1', '7'};
	int i;

	assert(s != H && s->len == -12);
	for(i = 0; i < 12; i++)
		assert(s->Srune[i] == expected[i]);
}

/* Preserve old multi-owner results on VM failure; release every new raw/managed owner. */
static void
one(int fail, int shared, int firstfd)
{
	Sys_FD *old[3];
	Sys_Connection previous;
	int i, ok, budget, eligible;
	uchar initialconnection[sizeof(connection)];
	int connectionoffset=(uchar*)&connection.t1-(uchar*)&connection;

	assert(live == 0 && group.r.ref == 1);
	nextfd = 100;
	memset(active, 0, sizeof(active));
	failure = 0;
	address = c2string("address", 7);
	local = c2string("local", 5);
	single = H;
	connection.t0 = -77;
	connection.t1.dfd = connection.t1.cfd = H;
	connection.t1.dir = H;
	for(i = 0; i < 3; i++)
		slots[i] = H;
	if(operation >= 1 && operation <= 3)
		single = mkfd(newfd());
	if(operation >= 4){
		old[0] = mkfd(newfd());
		old[1] = shared ? old[0] : mkfd(newfd());
		if(shared)
			D2H(old[0])->ref++;
		if(operation == 4){
			slots[0] = old[0]; slots[1] = old[1];
			slots[2] = old[2] = mkfd(newfd());
		}else{
			connection.t1.dfd = old[0];
			connection.t1.cfd = old[1];
			connection.t1.dir = c2string("previous directory", 18);
		}
	}
	previous = connection.t1;
	pipearray.len = pipesize;
	memset(pipeslots.bytes,0xa5,sizeof(pipeslots.bytes));
	memmove(pipeslots.bytes+returnoffset,slots,sizeof(slots));
	pipearray.data = pipebad == 2 ? nil : pipebad == 3 ? H :
		pipebad == 4 ? (uchar*)(uintptr)-8 : pipeslots.bytes+returnoffset;
	status = -77;
	nextfd = firstfd;
	allocations = nativecalls = 0;
	issued[0] = issued[1] = -1;
	failure = fail;
	raised = nil;
	strcpy(environment.errstr, "unchanged");
	eligible = (operation != 4 || (pipesize >= 2 && pipebad == 0)) &&
		(operation != 3 || (WORD)(int)descriptorarg==descriptorarg);
	budget = operation == 4 ? 2 : operation == 5 ? 3 : operation >= 6 ? 2 : 1;
	memset(returnslot.bytes,0xa5,sizeof(returnslot.bytes));
	if(operation == 4)memmove(returnslot.bytes+8+returnoffset,&status,sizeof(status));
	else memmove(returnslot.bytes+8+returnoffset,&single,sizeof(single));
	watchedowner = operation >= 1 && operation <= 3 ? single : H;
	watchedslot = returnslot.bytes+8+returnoffset;
	pipewatch = operation == 4 && eligible;
	pipewatchslot = pipeslots.bytes+returnoffset;
	pipewatchold[0] = slots[0];pipewatchold[1] = slots[1];
	memset(connectionreturn.bytes,0xa5,sizeof(connectionreturn.bytes));
	memmove(connectionreturn.bytes+returnoffset,&connection,sizeof(connection));
	memmove(initialconnection,&connection,sizeof(connection));
	connectionwatch = operation >= 5;
	connectionslot = connectionreturn.bytes+returnoffset+connectionoffset;
	connectionold[0] = connection.t1.dfd;connectionold[1] = connection.t1.cfd;connectionold[2] = connection.t1.dir;
	ok = kprotect(invoke, nil);
	connectionwatch = 0;
	if(operation >= 5){
		uchar expected[sizeof(connectionreturn.bytes)];
		memmove(&connection,connectionreturn.bytes+returnoffset,sizeof(connection));
		memset(expected,0xa5,sizeof(expected));memmove(expected+returnoffset,&connection,sizeof(connection));
		memmove(expected+returnoffset+sizeof(WORD),initialconnection+sizeof(WORD),connectionoffset-sizeof(WORD));
		assert(!memcmp(expected,connectionreturn.bytes,sizeof(expected)));
	}
	pipewatch = 0;
	if(operation >= 1 && operation <= 3){
		uchar expected[32];
		memmove(&single,returnslot.bytes+8+returnoffset,sizeof(single));
		memset(expected,0xa5,sizeof(expected));
		memmove(expected+8+returnoffset,&single,sizeof(single));
		assert(!memcmp(returnslot.bytes,expected,sizeof(expected)) && watchedowner == H);
	}
	if(operation == 4){
		uchar expected[sizeof(pipeslots.bytes)], guards[32];
		memmove(slots,pipeslots.bytes+returnoffset,sizeof(slots));
		memset(expected,0xa5,sizeof(expected));memmove(expected+returnoffset,slots,sizeof(slots));
		assert(!memcmp(pipeslots.bytes,expected,sizeof(expected)));
		memmove(&status,returnslot.bytes+8+returnoffset,sizeof(status));
		memset(guards,0xa5,sizeof(guards));memmove(guards+8+returnoffset,&status,sizeof(status));
		assert(!memcmp(guards,returnslot.bytes,sizeof(guards)));
	}
	assert(worker.nerr == 0 && !released);
	failure = 0;
	if(eligible && (!nativefail || operation == 0) && fail > 0 && fail <= budget){
		assert(!ok && raised == exHeap && strcmp(environment.errstr, exHeap) == 0);
		if(operation == 4)
			assert(slots[0] == old[0] && slots[1] == old[1]);
		if(operation >= 5)
			assert(memcmp(&previous, &connection.t1, sizeof(previous)) == 0);
	}else{
		assert(ok && raised == nil);
		if(!eligible){
			assert(nativecalls == 0);
			if(operation==3)
				assert(single==H && strcmp(environment.errstr,Ebadarg)==0);
			else{
				assert(status == -1);
				assert(slots[0] == old[0] && slots[1] == old[1]);
			}
		}else if(nativefail && operation > 0){
			assert(strcmp(environment.errstr, "native failure") == 0);
			if(operation <= 3)
				assert(single == H);
			else if(operation == 4)
				assert(status == -1 && slots[0] == old[0] && slots[1] == old[1]);
			else
				assert(connection.t0 == -1 && connection.t1.dfd == H && connection.t1.cfd == H && connection.t1.dir == H);
		}else if(operation <= 3){
			assert(single != H && active[single->fd] && single->fd == issued[0]);
		}else if(operation == 4){
			assert(status == 0 && slots[0] != H && slots[1] != H && slots[0] != slots[1]);
			assert(slots[0]->fd == issued[0] && slots[1]->fd == issued[1]);
		}else{
			assert(connection.t0 == 0 && connection.t1.cfd != H);
			assert((connection.t1.dfd != H) == (operation == 5));
			assert(connection.t1.cfd->fd == issued[1]);
			if(operation == 5)
				assert(connection.t1.dfd->fd == issued[0]);
			checkpath(connection.t1.dir);
		}
	}
	if(operation == 4)
		assert(slots[2] == old[2]);
	destroy(single);
	for(i = 0; i < 3; i++)
		destroy(slots[i]);
	destroy(connection.t1.dfd);
	destroy(connection.t1.cfd);
	destroy(connection.t1.dir);
	destroy(address);
	destroy(local);
	for(i = 0; i < 256; i++)
		assert(!active[i]);
	assert(live == 0 && TFD->ref == 1 && Tstring.ref == 1 && group.r.ref == 1);
	checks++;
}

/* Cover direct adoption, open/create/dup, pipes and connection construction at every failure. */
int
main(int argc, char **argv)
{
	int fail, shared, first, limit, length, argument;
	WORD arguments[]={17,0,-1,0x7fffffff,(-0x7fffffff-1),(WORD)((uvlong)1<<32),(WORD)((uvlong)1<<40),(WORD)(-((vlong)1<<40))};

	worker.env = &environment;
	environment.fgrp = &group;
	environment.errstr = environment.errbuf0;
	group.r.ref = 1;
	for(operation = 0; operation < 8; operation++){
		if(argc > 1 && strcmp(argv[1], "all") != 0 && strcmp(argv[1], operation == 0 ? "single" : operation == 4 ? "pipe" : operation >= 5 ? "connection" : "files") != 0)
			continue;
		for(argument=0;argument<(operation==3 ? nelem(arguments) : 1);argument++){
			descriptorarg=arguments[argument];
			for(returnoffset=0;returnoffset<(operation >= 1 ? 8 : 1);returnoffset++)
			for(nativefail = 0; nativefail < (operation == 0 ? 1 : 2); nativefail++)
			for(shared = 0; shared < (operation >= 4 ? 2 : 1); shared++)
			for(first = 0; first < 2; first++){
				limit = operation == 4 ? 3 : operation == 5 ? 4 : operation >= 6 ? 3 : 2;
				for(length = 0; length < (operation == 4 ? 9 : 1); length++){
					pipesize = length < 5 ? length-1 : 2;
					pipebad = length < 5 ? 0 : length-4;
					for(fail = 0; fail <= (argc > 2 && strcmp(argv[2], "normal") == 0 ? 0 : limit); fail++)
						one(fail, shared, first ? 10 : 0);
				}
			}
		}
	}
	assert(checks > 0);
	printf("Sys FD owners: %d adoption/pipe/connection/failure cases\n", checks);
	return 0;
}
