#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "raise.h"
#include <pthread.h>
#include <stdio.h>

#define fdtest(test) fdpredicate(!!(test), __LINE__) // Retain all ownership predicates under NDEBUG.
static int captureactive, capturephase, captureframeoffset, capturemutated;
static int capturearrayoffset, capturearrayactive, capturekeepframe;
static uchar capturearray[sizeof(Array)+32], capturearrayimage[sizeof(capturearray)];
static uchar *capturearrayoriginal;
static uchar captureframe[sizeof(F_Sys_listen)+sizeof(F_Sys_dial)+sizeof(F_Sys_create)+32];
static uchar captureimage[sizeof(captureframe)];
static uchar *captureoriginal;
static uintptr capturesize;
static WORD modearg = 2;


/* Report failures without depending on assertion build flags. */
static void
fdpredicate(int valid, int line)
{
	if(!valid) {
		fprintf(stderr, "Sys FD predicate failed at %d phase=%d\n", line, capturephase);
		abort();
	}
}


/* Mutate the borrowed frame at allocation, finalization and scheduler boundaries. */
static void
capturecallback(int point)
{
	if(!captureactive || capturephase != point || capturemutated)
		return;
	capturemutated = 1;
	if(!capturekeepframe)
		memset(captureoriginal, 0xa7, capturesize);
	if(capturearrayactive) {
		WORD length = 0;
		uchar *data = H;
		memmove(capturearrayoriginal+O(Array, len), &length, sizeof(length));
		memmove(capturearrayoriginal+O(Array, data), &data, sizeof(data));
		memmove(capturearrayimage+8+capturearrayoffset+O(Array, len), &length, sizeof(length));
		memmove(capturearrayimage+8+capturearrayoffset+O(Array, data), &data, sizeof(data));
	}
}


/* Keep the complete original frame available for post-error image comparisons. */
static void
capturecall(void (*fn)(void*), void *frame, uintptr size)
{
	Array *array;

	capturearrayactive = 0;
	capturesize = size;
	captureoriginal = captureframe+8+captureframeoffset;
	memset(captureframe, 0x3c, sizeof(captureframe));
	memmove(captureoriginal, frame, size);
	if(fn == Sys_pipe) {
		memmove(&array, (uchar*)frame+O(F_Sys_pipe, fds), sizeof(array));
		if(array != nil && array != H) {
			memset(capturearray, 0x4d, sizeof(capturearray));
			capturearrayoriginal = capturearray+8+capturearrayoffset;
			memmove(capturearrayoriginal, array, sizeof(*array));
			memmove(capturearrayimage, capturearray, sizeof(capturearrayimage));
			array = (Array*)capturearrayoriginal;
			memmove(captureoriginal+O(F_Sys_pipe, fds), &array, sizeof(array));
			capturearrayactive = 1;
		}
	}
	memmove(captureimage, captureframe, sizeof(captureimage));
	captureactive = 1;
	capturemutated = 0;
	fn(captureoriginal);
}


/* Check callbacks even when a real VM allocation error escaped through kprotect. */
static void
capturefinish(void)
{
	if(!captureactive)
		return;
	captureactive = 0;
	if(capturemutated && !capturekeepframe)
		memset(captureimage+(captureoriginal-captureframe), 0xa7, capturesize);
	fdtest(!memcmp(captureframe, captureimage, sizeof(captureimage)));
	if(capturearrayactive)
		fdtest(!memcmp(capturearray, capturearrayimage, sizeof(capturearrayimage)));
}


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
	fdtest(worker.nerr > 0);
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

	capturecallback(1);
	if(++allocations == failure)
		error(exHeap);
	fdtest(size >= 0 && size < 4096);
	heap = malloc(sizeof(Heap)+size+32);
	fdtest(heap);
	memset(heap, 0, sizeof(Heap));
	memset(heap+1, 0xa5, size+32);
	heap->ref = 1;
	for(i = 0; i < 256 && blocks[i].heap; i++);
	fdtest(i < 256);
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
		fdtest(published.dfd != connectionold[0] && published.dfd != connectionold[1]);
		fdtest(published.cfd != connectionold[0] && published.cfd != connectionold[1]);
		fdtest(published.dir != connectionold[2]);
	}
	if(pipewatch && (value == pipewatchold[0] || value == pipewatchold[1])){
		void *published[2];
		memmove(published,pipewatchslot,sizeof(published));
		fdtest(published[0] != pipewatchold[0] && published[0] != pipewatchold[1]);
		fdtest(published[1] != pipewatchold[0] && published[1] != pipewatchold[1]);
	}
	if(value == watchedowner){
		void *published;
		memmove(&published,watchedslot,sizeof(published));
		fdtest(published == H);
		watchedowner = H;
	}
	capturecallback(2);
	heap = D2H(value);
	for(i = 0; i < 256 && blocks[i].heap != heap; i++);
	fdtest(i < 256 && heap->ref > 0);
	for(j = 0; j < 32; j++)
		fdtest(((uchar*)(heap+1))[blocks[i].size+j] == 0xa5);
	if(--heap->ref)
		return;
	if(heap->t == TFD){
		freeFD(heap, 0);
		TFD->ref--;
	}else{
		fdtest(heap->t == &Tstring);
		fdtest(((String*)value)->tmp == nil);
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

	fdtest(type == TFD);
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
	fdtest(reference == &group.r);
	return ++reference->ref;
}


/* Finalization must release each retained file-group reference. */
void
closefgrp(Fgrp *fgrp)
{
	fdtest(fgrp == &group && fgrp->r.ref > 1);
	fgrp->r.ref--;
}


/* Native I/O and closing descriptors may run only outside the VM scheduler lock. */
void
release(void)
{
	fdtest(!released);
	released = 1;
	capturecallback(3);
}


/* Every native transition must restore scheduler ownership even during error cleanup. */
void
acquire(void)
{
	fdtest(released);
	capturecallback(4);
	released = 0;
}


/* Allocate distinct native descriptors, including descriptor zero. */
static int
newfd(void)
{
	int fd;

	fd = nextfd++;
	fdtest(fd >= 0 && fd < 256 && !active[fd]);
	active[fd] = 1;
	return fd;
}


/* Detect leaks and double closes; deliberately replace the error string during close. */
int
kfgrpclose(Fgrp *fgrp, int fd)
{
	fdtest(released && fgrp == &group && fd >= 0 && fd < 256 && active[fd]);
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
	fdtest(released);
	capturecallback(5);
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
	fdtest(operation == 1 && strcmp(path, "address") == 0 && mode == (int)modearg);
	if(!nativebegin())
		return -1;
	return issued[0] = newfd();
}


/* Simulate native create with the exact arguments passed by the real builtin. */
int
kcreate(char *path, int mode, ulong permissions)
{
	fdtest(operation == 2 && strcmp(path, "address") == 0 && mode == (int)modearg && permissions == 0640);
	if(!nativebegin())
		return -1;
	return issued[0] = newfd();
}


/* Preserve a normal argument error without throwing through an ownership transition. */
void
seterror(char *message,...)
{
	fdtest(message==Ebadarg);
	strcpy(environment.errstr,message);
}


/* Simulate duplication of a numeric descriptor into a new native slot. */
int
kdup(int fd, int destination)
{
	fdtest(operation == 3 && fd == (int)descriptorarg && destination == -1);
	if(!nativebegin())
		return -1;
	return issued[0] = newfd();
}


/* A successful pipe returns two independent descriptors that must both be adopted. */
int
kpipe(int *fd)
{
	fdtest(operation == 4);
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
	fdtest(operation == 5 && strcmp(address, "address") == 0 && strcmp(local, "local") == 0);
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
	fdtest(operation == 6 && strcmp(address, "address") == 0);
	if(!nativebegin())
		return -1;
	strcpy(directory, returnedpath);
	return issued[1] = newfd();
}


/* Listen may borrow its input directory from the previous result. */
int
klisten(char *input, char *directory)
{
	fdtest(operation == 7 && strcmp(input, "previous directory") == 0);
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
static Type pipeelement; // Explicit pointer-element layout for the borrowed pipe array.
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
		openframe.mode = modearg;
		capturecall(Sys_open, &openframe, sizeof(openframe));
		break;
	case 2:
		createframe.ret = (void*)(returnslot.bytes+8+returnoffset);
		createframe.s = address;
		createframe.mode = modearg;
		createframe.perm = 0640;
		capturecall(Sys_create, &createframe, sizeof(createframe));
		break;
	case 3:
		fildesframe.ret = (void*)(returnslot.bytes+8+returnoffset);
		fildesframe.fd = descriptorarg;
		capturecall(Sys_fildes, &fildesframe, sizeof(fildesframe));
		break;
	case 4:
		pipeframe.ret = (void*)(returnslot.bytes+8+returnoffset);
		pipeframe.fds = pipebad == 1 ? nil : pipesize < 0 ? H : &pipearray;
		capturecall(Sys_pipe, &pipeframe, sizeof(pipeframe));
		break;
	case 5:
		dialframe.ret = (void*)(connectionreturn.bytes+returnoffset);
		dialframe.addr = address;
		dialframe.local = local;
		capturecall(Sys_dial, &dialframe, sizeof(dialframe));
		break;
	case 6:
		announceframe.ret = (void*)(connectionreturn.bytes+returnoffset);
		announceframe.addr = address;
		capturecall(Sys_announce, &announceframe, sizeof(announceframe));
		break;
	case 7:
		listenframe.ret = (void*)(connectionreturn.bytes+returnoffset);
		listenframe.c = connection.t1;
		capturecall(Sys_listen, &listenframe, sizeof(listenframe));
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

	fdtest(s != H && s->len == -12);
	for(i = 0; i < 12; i++)
		fdtest(s->Srune[i] == expected[i]);
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

	fdtest(live == 0 && group.r.ref == 1);
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
	pipeelement.size = sizeof(Sys_FD*);
	pipeelement.np = 1;
	pipeelement.map[0] = 0x80;
	pipearray.t = &pipeelement;
	if(pipebad == 5) {
		pipearray.t = nil;
	}
	if(pipebad == 6) {
		pipearray.t = (Type*)H;
	}
	if(pipebad == 7) {
		pipeelement.size = 1;
	}
	if(pipebad == 8) {
		pipeelement.np = 0;
	}
	if(pipebad == 9) {
		pipeelement.map[0] = 0x40;
	}
	if(pipebad == 10) {
		pipearray.t = (Type*)1;
	}
	if(pipebad == 11) {
		pipearray.t = (Type*)((uintptr)-1-1);
	}
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
		(operation != 3 || (WORD)(int)descriptorarg==descriptorarg) &&
		((operation != 1 && operation != 2) || (WORD)(int)modearg==modearg);
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
	capturefinish();
	connectionwatch = 0;
	if(operation >= 5){
		uchar expected[sizeof(connectionreturn.bytes)];
		memmove(&connection,connectionreturn.bytes+returnoffset,sizeof(connection));
		memset(expected,0xa5,sizeof(expected));memmove(expected+returnoffset,&connection,sizeof(connection));
		memmove(expected+returnoffset+sizeof(WORD),initialconnection+sizeof(WORD),connectionoffset-sizeof(WORD));
		fdtest(!memcmp(expected,connectionreturn.bytes,sizeof(expected)));
	}
	pipewatch = 0;
	if(operation >= 1 && operation <= 3){
		uchar expected[32];
		memmove(&single,returnslot.bytes+8+returnoffset,sizeof(single));
		memset(expected,0xa5,sizeof(expected));
		memmove(expected+8+returnoffset,&single,sizeof(single));
		fdtest(!memcmp(returnslot.bytes,expected,sizeof(expected)) && watchedowner == H);
	}
	if(operation == 4){
		uchar expected[sizeof(pipeslots.bytes)], guards[32];
		memmove(slots,pipeslots.bytes+returnoffset,sizeof(slots));
		memset(expected,0xa5,sizeof(expected));memmove(expected+returnoffset,slots,sizeof(slots));
		fdtest(!memcmp(pipeslots.bytes,expected,sizeof(expected)));
		memmove(&status,returnslot.bytes+8+returnoffset,sizeof(status));
		memset(guards,0xa5,sizeof(guards));memmove(guards+8+returnoffset,&status,sizeof(status));
		fdtest(!memcmp(guards,returnslot.bytes,sizeof(guards)));
	}
	fdtest(worker.nerr == 0 && !released);
	failure = 0;
	if(eligible && (!nativefail || operation == 0) && fail > 0 && fail <= budget){
		fdtest(!ok && raised == exHeap && strcmp(environment.errstr, exHeap) == 0);
		if(operation == 4)
			fdtest(slots[0] == old[0] && slots[1] == old[1]);
		if(operation >= 5)
			fdtest(memcmp(&previous, &connection.t1, sizeof(previous)) == 0);
	}else{
		fdtest(ok && raised == nil);
		if(!eligible){
			fdtest(nativecalls == 0);
			if(operation>=1 && operation<=3)
				fdtest(single==H && strcmp(environment.errstr,Ebadarg)==0);
			else{
				fdtest(status == -1);
				fdtest(slots[0] == old[0] && slots[1] == old[1]);
			}
		}else if(nativefail && operation > 0){
			fdtest(strcmp(environment.errstr, "native failure") == 0);
			if(operation <= 3)
				fdtest(single == H);
			else if(operation == 4)
				fdtest(status == -1 && slots[0] == old[0] && slots[1] == old[1]);
			else
				fdtest(connection.t0 == -1 && connection.t1.dfd == H && connection.t1.cfd == H && connection.t1.dir == H);
		}else if(operation <= 3){
			fdtest(single != H && active[single->fd] && single->fd == issued[0]);
		}else if(operation == 4){
			fdtest(status == 0 && slots[0] != H && slots[1] != H && slots[0] != slots[1]);
			fdtest(slots[0]->fd == issued[0] && slots[1]->fd == issued[1]);
		}else{
			fdtest(connection.t0 == 0 && connection.t1.cfd != H);
			fdtest((connection.t1.dfd != H) == (operation == 5));
			fdtest(connection.t1.cfd->fd == issued[1]);
			if(operation == 5)
				fdtest(connection.t1.dfd->fd == issued[0]);
			checkpath(connection.t1.dir);
		}
	}
	if(operation == 4)
		fdtest(slots[2] == old[2]);
	destroy(single);
	for(i = 0; i < 3; i++)
		destroy(slots[i]);
	destroy(connection.t1.dfd);
	destroy(connection.t1.cfd);
	destroy(connection.t1.dir);
	destroy(address);
	destroy(local);
	for(i = 0; i < 256; i++)
		fdtest(!active[i]);
	fdtest(live == 0 && TFD->ref == 1 && Tstring.ref == 1 && group.r.ref == 1);
	checks++;
}




/* Invoke deliberately invalid creation frames before the fixture can copy them. */
static int
rawcreation(void *frame)
{
	if(operation == 1)Sys_open(frame);
	else if(operation == 2)Sys_create(frame);
	else Sys_fildes(frame);
	return 1;
}


/* Preserve the old FD and every result byte when frame or owner preflight fails. */
static void
creationpreflight(int mode)
{
	F_Sys_open openframe = {0};
	F_Sys_create createframe = {0};
	F_Sys_fildes fildesframe = {0};
	Sys_FD *old, *candidate;
	void *frame, *ret;
	uchar before[32];
	char *expected;
	int ok, i;

	fdtest(live == 0 && group.r.ref == 1);
	captureactive = 0;
	failure = 0;
	memset(active, 0, sizeof(active));
	nextfd = 100;
	old = mkfd(newfd());
	candidate = old;
	memset(returnslot.bytes, 0xa5, sizeof(returnslot.bytes));
	ret = returnslot.bytes+8;
	if(mode == 6)candidate = nil;
	if(mode == 7)candidate = (Sys_FD*)1;
	if(mode == 8)candidate = (Sys_FD*)((uchar*)old+1);
	if(mode == 9)D2H(old)->ref = 0;
	memmove(ret, &candidate, sizeof(candidate));
	memmove(before, returnslot.bytes, sizeof(before));
	if(mode == 3)ret = nil;
	if(mode == 4)ret = H;
	if(mode == 5)ret = (void*)((uintptr)-1-sizeof(old)+1);
	openframe.ret = ret;
	openframe.mode = 2;
	createframe.ret = ret;
	createframe.mode = 2;
	createframe.perm = 0640;
	fildesframe.ret = ret;
	fildesframe.fd = 17;
	frame = operation == 1 ? (void*)&openframe : operation == 2 ? (void*)&createframe : (void*)&fildesframe;
	if(mode == 0)frame = nil;
	if(mode == 1)frame = H;
	if(mode == 2)frame = (void*)((uintptr)-1-1);
	expected = mode == 2 || mode == 5 ? exBounds : mode >= 7 ? exHeap : exNilref;
	allocations = nativecalls = 0;
	watchedowner = H;
	raised = nil;
	ok = kprotect(rawcreation, frame);
	fdtest(!ok && raised == expected && !released && !nativecalls && !allocations && worker.nerr == 0);
	fdtest(!memcmp(returnslot.bytes, before, sizeof(before)) && live == 1 && active[100]);
	fdtest(TFD->ref == 2 && group.r.ref == 2);
	if(mode == 9)D2H(old)->ref = 1;
	destroy(old);
	for(i=0; i<256; i++)fdtest(!active[i]);
	fdtest(!live && TFD->ref == 1 && group.r.ref == 1);
	checks++;
}




/* Enter directly when invalid multi-owner frames must reach production validation. */
static int
rawmulti(void *frame)
{
	if(operation == 4)Sys_pipe(frame);
	else if(operation == 5)Sys_dial(frame);
	else if(operation == 6)Sys_announce(frame);
	else Sys_listen(frame);
	return 1;
}


/* Preserve all previous handles, strings and tuple bytes on multi-owner preflight errors. */
static void
multipreflight(int mode)
{
	F_Sys_pipe pipeframe = {0};
	F_Sys_dial dialframe = {0};
	F_Sys_announce announceframe = {0};
	F_Sys_listen listenframe = {0};
	Sys_Connection previous;
	Sys_FD *ends[2];
	Array array;
	void *frame, *ret, *slot, *candidate;
	uchar before[sizeof(connectionreturn.bytes)], beforeends[sizeof(ends)];
	WORD status = -77;
	char *expected;
	int ok, i;

	fdtest(live == 0 && group.r.ref == 1);
	captureactive = 0;
	failure = 0;
	memset(active, 0, sizeof(active));
	nextfd = 100;
	previous.dfd = mkfd(newfd());
	previous.cfd = mkfd(newfd());
	previous.dir = c2string("previous directory", 18);
	ends[0] = previous.dfd;
	ends[1] = previous.cfd;
	memset(&array, 0, sizeof(array));
	array.len = 2;
	array.t = &pipeelement;
	array.data = (uchar*)ends;
	memset(connectionreturn.bytes, 0xa5, sizeof(connectionreturn.bytes));
	ret = connectionreturn.bytes;
	memmove(ret, &status, sizeof(status));
	slot = operation == 4 ? (void*)ends : (void*)connectionresultslot(ret);
	if(operation != 4)
		memmove(slot, &previous, sizeof(previous));
	candidate = previous.dfd;
	if(mode == 6)candidate = nil;
	if(mode == 7)candidate = (void*)1;
	if(mode == 8)candidate = (uchar*)previous.dfd+1;
	if(mode >= 6 && mode <= 8)
		memmove(slot, &candidate, sizeof(candidate));
	if(mode == 9)D2H(previous.dfd)->ref = 0;
	if(mode == 10)
		memmove((uchar*)slot+sizeof(void*), &previous.dfd, sizeof(previous.dfd));
	candidate = previous.dir;
	if(mode == 11)candidate = nil;
	if(mode == 12)candidate = (void*)1;
	if(mode == 13)candidate = (uchar*)previous.dir+1;
	if(mode >= 11 && mode <= 13)
		memmove((uchar*)slot+2*sizeof(void*), &candidate, sizeof(candidate));
	if(mode == 14)D2H(previous.dir)->ref = 0;
	memmove(before, connectionreturn.bytes, sizeof(before));
	memmove(beforeends, ends, sizeof(ends));
	if(mode == 3)ret = nil;
	if(mode == 4)ret = H;
	if(mode == 5)ret = (void*)((uintptr)-1-(operation == 4 ? sizeof(WORD)-1 : sizeof(WORD)+1));
	pipeframe.ret = ret;
	pipeframe.fds = &array;
	if(mode == 15)pipeframe.fds = (Array*)((uintptr)-1-1);
	if(mode == 16)pipeframe.fds = (Array*)1;
	dialframe.ret = ret;
	dialframe.addr = (String*)1;
	dialframe.local = (String*)1;
	announceframe.ret = ret;
	announceframe.addr = (String*)1;
	listenframe.ret = ret;
	listenframe.c = previous;
	switch(operation) {
	case 4: frame = &pipeframe; break;
	case 5: frame = &dialframe; break;
	case 6: frame = &announceframe; break;
	default: frame = &listenframe; break;
	}
	if(mode == 0)frame = nil;
	if(mode == 1)frame = H;
	if(mode == 2)frame = (void*)((uintptr)-1-1);
	if(mode == 2 || mode == 5 || mode >= 15)
		expected = exBounds;
	else if((mode >= 7 && mode <= 10) || (mode >= 12 && mode <= 14))
		expected = exHeap;
	else
		expected = exNilref;
	allocations = nativecalls = 0;
	watchedowner = H;
	pipewatch = connectionwatch = 0;
	raised = nil;
	ok = kprotect(rawmulti, frame);
	fdtest(!ok && raised == expected && !released && !nativecalls && !allocations && worker.nerr == 0);
	fdtest(!memcmp(connectionreturn.bytes, before, sizeof(before)) && !memcmp(ends, beforeends, sizeof(ends)));
	fdtest(live == 3 && active[100] && active[101] && TFD->ref == 3 && Tstring.ref == 2 && group.r.ref == 3);
	if(mode == 9)D2H(previous.dfd)->ref = 1;
	if(mode == 14)D2H(previous.dir)->ref = 1;
	destroy(previous.dfd);
	destroy(previous.cfd);
	destroy(previous.dir);
	for(i=0; i<256; i++)fdtest(!active[i]);
	fdtest(!live && TFD->ref == 1 && Tstring.ref == 1 && group.r.ref == 1);
	checks++;
}


/* Cover direct adoption, open/create/dup, pipes and connection construction at every failure. */
static int
fdmatrix(int argc, char **argv)
{
	int fail, shared, first, limit, length, argument, mode;
	WORD arguments[]={17,0,-1,0x7fffffff,(-0x7fffffff-1),(WORD)((uvlong)1<<32),(WORD)((uvlong)1<<40),(WORD)(-((vlong)1<<40))};

	worker.env = &environment;
	environment.fgrp = &group;
	environment.errstr = environment.errbuf0;
	group.r.ref = 1;
	pipeelement.size = sizeof(Sys_FD*);
	pipeelement.np = 1;
	pipeelement.map[0] = 0x80;
	if(getenv("FD_OWNER_PIPE_TYPES")) {
		operation = 4;
		pipesize = 2;
		nativefail = 0;
		returnoffset = 0;
		for(pipebad = 5; pipebad <= 11; pipebad++) {
			one(0, 0, 0);
		}
		fdtest(checks == 7);
		printf("PASS Sys pipe element preflight: %d cases\n", checks);
		return 0;
	}
	capturekeepframe = getenv("FD_OWNER_KEEP_FRAME") != nil;
	if(getenv("FD_OWNER_MULTI_PREFLIGHT")) {
		for(operation=4; operation<8; operation++)
		for(mode=0; mode<17; mode++) {
			if((operation == 4 && mode >= 11 && mode <= 14) || (operation != 4 && mode >= 15))continue;
			if(getenv("FD_OWNER_OPERATION") && operation != atoi(getenv("FD_OWNER_OPERATION")))continue;
			if(getenv("FD_OWNER_ERROR") && mode != atoi(getenv("FD_OWNER_ERROR")))continue;
			multipreflight(mode);
		}
		fdtest(checks != 0);
		printf("PASS Sys multi-owner preflight: %d cases\n", checks);
		return 0;
	}
	if(getenv("FD_OWNER_PREFLIGHT")) {
		for(operation=1; operation<4; operation++)
		for(mode=0; mode<10; mode++) {
			if(getenv("FD_OWNER_OPERATION") && operation != atoi(getenv("FD_OWNER_OPERATION")))continue;
			if(getenv("FD_OWNER_ERROR") && mode != atoi(getenv("FD_OWNER_ERROR")))continue;
			creationpreflight(mode);
		}
		fdtest(checks != 0);
		printf("PASS Sys creation preflight: %d cases\n", checks);
		return 0;
	}
	for(capturephase=0; capturephase<((getenv("FD_OWNER_CAPTURE") || getenv("FD_OWNER_MULTI")) ? 7 : 1); capturephase++)
	for(captureframeoffset=0; captureframeoffset<((getenv("FD_OWNER_CAPTURE") || getenv("FD_OWNER_MULTI")) ? 8 : 1); captureframeoffset++)
	for(operation = 0; operation < 8; operation++){
		if(getenv("FD_OWNER_OPERATION") && operation != atoi(getenv("FD_OWNER_OPERATION")))
			continue;
		if(getenv("FD_OWNER_PHASE") && capturephase != atoi(getenv("FD_OWNER_PHASE")))
			continue;
		if(getenv("FD_OWNER_FRAME") && captureframeoffset != atoi(getenv("FD_OWNER_FRAME")))
			continue;
		if(argc > 1 && strcmp(argv[1], "all") != 0 && strcmp(argv[1], operation == 0 ? "single" : operation == 4 ? "pipe" : operation >= 5 ? "connection" : "files") != 0)
			continue;
		for(capturearrayoffset=0; capturearrayoffset<(getenv("FD_OWNER_MULTI") && operation==4 ? 8 : 1); capturearrayoffset++)
		for(argument=0;argument<(operation==3 || (getenv("FD_OWNER_CAPTURE") && (operation==1 || operation==2)) ? nelem(arguments) : 1);argument++){
			if(getenv("FD_OWNER_ARGUMENT") && argument != atoi(getenv("FD_OWNER_ARGUMENT")))continue;
			if(getenv("FD_OWNER_ARRAY_SHIFT") && capturearrayoffset != atoi(getenv("FD_OWNER_ARRAY_SHIFT")))continue;
			descriptorarg=arguments[argument];
			modearg=getenv("FD_OWNER_CAPTURE") ? arguments[argument] : 2;
			for(returnoffset=0;returnoffset<(operation >= 1 ? 8 : 1);returnoffset++)
			for(nativefail = 0; nativefail < (operation == 0 ? 1 : 2); nativefail++)
			for(shared = 0; shared < (operation >= 4 ? 2 : 1); shared++)
			for(first = 0; first < 2; first++){
				limit = operation == 4 ? 3 : operation == 5 ? 4 : operation >= 6 ? 3 : 2;
				for(length = 0; length < (operation == 4 ? 9 : 1); length++){
					if(getenv("FD_OWNER_MULTI") && operation==4 && length!=3 && length!=4)continue;
					pipesize = length < 5 ? length-1 : 2;
					pipebad = length < 5 ? 0 : length-4;
					for(fail = 0; fail <= (argc > 2 && strcmp(argv[2], "normal") == 0 ? 0 : limit); fail++)
						one(fail, shared, first ? 10 : 0);
				}
			}
		}
	}
	fdtest(checks > 0);
	printf("Sys FD owners: %d adoption/pipe/connection/failure cases\n", checks);
	return 0;
}

/* Preserve normal test arguments while executing VM errors on a tracked pthread stack. */
struct FdArguments {
	int argc; // Number of fixture command-line arguments.
	char **argv; // Fixture family and failure-budget selections.
};


/* Run the complete owner matrix without losing its real nested VM error frames. */
static void*
fdworker(void *argument)
{
	struct FdArguments *args = argument;

	fdtest(fdmatrix(args->argc, args->argv) == 0);
	return nil;
}


/* Join before returning so all heap, descriptor and file-group checks complete. */
int
main(int argc, char **argv)
{
	struct FdArguments args;
	pthread_t thread;

	args.argc = argc;
	args.argv = argv;
	fdtest(pthread_create(&thread, nil, fdworker, &args) == 0);
	fdtest(pthread_join(thread, nil) == 0);
	return 0;
}
