#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "pool.h"
#include <pthread.h>
#include <stdio.h>


#define filetest(test) filepredicate(!!(test), __LINE__) // Keep ownership checks active with NDEBUG.


/* Fail ownership and boundary predicates in optimized NDEBUG builds too. */
static void
filepredicate(int condition, int line)
{
	if(!condition){
		fprintf(stderr, "Sys file2chan predicate failed at line %d\n", line);
		abort();
	}
}

static Proc worker;

static union {
	uvlong align; // Host-aligned backing for byte-shifted pointer results.
	uchar bytes[32]; // Complete result span and both guard regions.
} returnslot;
static int returnoffset;
static void *watchedowner;
static Osenv environment;
static char *raised;
char exNomem[] = "malloc", exHeap[] = "heap", exNegsize[] = "negative", exNilref[] = "nil", exBounds[] = "bounds";
static char injected[] = "injected", registration[] = "registration";
static int allocations, failure, throwing, live, checks, released, nativecalls, nativekind;

static struct {
	void *pointer; // Guarded allocation base.
	ulong size; // Exact payload extent before the guard.
} blocks[256];
FD_DECLARATION
static Type *TFD, *TFileIO, *FioTread, *FioTwrite, *Tdir;
static uchar FDmap[] = Sys_FD_map, FileIOmap[] = Sys_FileIO_map;
static uchar rmap[] = Sys_FileIO_read_map, wmap[] = Sys_FileIO_write_map, dmap[] = Sys_Dir_map;
static Type *Trdchan, *Twrchan;

static struct {
	Type *Rread, *Rwrite; // Native service reply descriptors.
	ulong pathgen; // Service path sequence.
} dev;

static union {
	uvlong align; // Host-aligned backing for packed call frames.
	uchar bytes[sizeof(F_Sys_file2chan)+32]; // Complete frame and guards.
} frameslot;
static uchar expectedframe[sizeof(F_Sys_file2chan)+32];
static void *activeframe;
static int frameoffset, phase, armed, mutated;


/* Poison the original borrowed frame at one real allocator or scheduler boundary. */
static void
capture(int boundary)
{
	if(!armed || mutated)
		return;
	if(phase == 6 && boundary == 1){
		Sys_FileIO *empty = H;

		memmove(returnslot.bytes+8+returnoffset, &empty, sizeof(empty));
		mutated = 1;
		return;
	}
	if(phase != boundary)
		return;
	memset(activeframe, 0xa7, sizeof(F_Sys_file2chan));
	memset(expectedframe+8+frameoffset, 0xa7, sizeof(F_Sys_file2chan));
	mutated = 1;
}


/* Supply the actual host runtime's error-frame stack. */
Proc*
getup(void)
{
	return &worker;
}


/* Propagate errors through nested production protection frames. */
void
nexterror(void)
{
	filetest(worker.nerr > 0);
	siglongjmp(worker.estack[--worker.nerr], 1);
}


/* Preserve error identity independently of cleanup routines that change errstr. */
void
error(char *message)
{
	raised = message;
	strcpy(environment.errstr, message);
	nexterror();
}


/* All blocks and guards are tracked; LSan process inspection is unavailable here. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Fail either by returning nil or throwing before each tracked allocation. */
static void*
allocate(ulong size, int zero)
{
	void *pointer;
	int i;

	filetest(!released);
	capture(1);
	if(++allocations == failure){
		if(throwing)
			error(injected);
		return nil;
	}
	filetest(size <= 1024*1024);
	pointer = malloc(size+32);
	filetest(pointer != nil);
	memset(pointer, zero ? 0 : 0xa5, size);
	memset((uchar*)pointer+size, 0xa5, 32);
	for(i = 0; i < 256 && blocks[i].pointer; i++);
	filetest(i < 256);
	blocks[i].pointer = pointer;
	blocks[i].size = size;
	live++;
	return pointer;
}


/* Verify exact allocation boundaries before releasing any native or heap block. */
static void
deallocate(void *pointer)
{
	int i, j;

	if(pointer == nil)
		return;
	for(i = 0; i < 256 && blocks[i].pointer != pointer; i++);
	filetest(i < 256);
	for(j = 0; j < 32; j++)
		filetest(((uchar*)pointer)[blocks[i].size+j] == 0xa5);
	blocks[i].pointer = nil;
	free(pointer);
	live--;
}


/* Native caches and heap capacity calculations see exact fixture allocation sizes. */
ulong
msize(void *pointer)
{
	int i;

	for(i = 0; i < 256; i++)
		if(blocks[i].pointer == pointer)
			return blocks[i].size;
	abort();
}


/* Match the allocator interface used by native VM heap constructors. */
void*
poolalloc(Pool *pool, ulong size)
{
	(void)pool;
	return allocate(size, 0);
}


/* Report precise heap extents for production string constructors. */
int
hmsize(void *heap)
{
	return msize(heap);
}

Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
void freechan(Heap*, int);
void freearray(Heap*, int);
void freestring(Heap*, int);
void freeFileIO(Heap*, int);
Type Tarray = {1, freearray, 0, sizeof(Array)};
Type Tchannel = {1, freechan, 0, sizeof(Channel), 1, 0, 0, {0x80}};
Type Tstring = {1, freestring, 0, sizeof(String)};


/* No collector is needed, but descriptors retain their production mark callback. */
void
markheap(Type *type, void *object)
{
	(void)type;
	(void)object;
}


/* The mover identity selects typed channel buffering in the real constructor. */
void
movtmp(void)
{
	abort();
}


/* Queues are empty during construction; cleanup must release both allocated heads. */
void
killcomm(Progq **queue)
{
	Progq *head;

	while((head = *queue) != nil){
		filetest(head->prog == nil);
		*queue = head->next;
		deallocate(head);
	}
}


/* Execute actual finalizers and pointer maps before freeing each guarded owner. */
void
destroy(void *value)
{
	Heap *h;
	Type *type;

	if(value == H)
		return;
	if(value == watchedowner){
		capture(5);
		Sys_FileIO *published;
		memmove(&published, returnslot.bytes+8+returnoffset, sizeof(published));
		filetest(published != watchedowner);
		watchedowner = H;
	}
	filetest(!released);
	h = D2H(value);
	filetest(h->ref > 0);
	if(--h->ref)
		return;
	type = h->t;
	filetest(type && type->free);
	type->free(h, 0);
	if(type == TFileIO)
		strcpy(environment.errstr, "cleanup changed error");
	freetype(type);
	deallocate(h);
}


/* No descriptor owner is created by this channel construction fixture. */
void
freeFD(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}


/* The native registration adapter does not retype the read channel. */
static void
freerdchan(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}


/* The native registration adapter does not retype the write channel. */
static void
freewrchan(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}


/* Native registration runs released; every allocator and string conversion runs acquired. */
void
release(void)
{
	filetest(!released);
	released = 1;
	capture(2);
}


/* Construction failures and native exceptions must restore VM ownership. */
void
acquire(void)
{
	filetest(released);
	released = 0;
	capture(4);
}


/* Check real request-ring initialization before simulating atomic native registration. */
int
srvf2c(char *directory, char *name, Sys_FileIO *file)
{
	Sys_FileIO_read *read;
	Sys_FileIO_write *write;
	int i;

	filetest(released && file->read != H && file->write != H);
	filetest(strcmp(directory, "/chan") == 0 || strcmp(directory, "/ch\303\251n") == 0);
	filetest(strcmp(name, "service") == 0 || strcmp(name, "svc\360\237\230\200") == 0);
	nativecalls++;
	capture(3);
	filetest(file->read->mid.t == FioTread && file->write->mid.t == FioTwrite);
	filetest(file->read->buf->len == 16 && file->write->buf->len == 16);
	read = (Sys_FileIO_read*)file->read->buf->data;
	write = (Sys_FileIO_write*)file->write->buf->data;
	for(i = 0; i < 16; i++){
		filetest(read[i].t3 == H);
		filetest(write[i].t1 == H && write[i].t3 == H);
	}
	if(nativekind == 2)
		error(registration);
	if(nativekind == 1){
		strcpy(environment.errstr, registration);
		return -1;
	}
	return 0;
}

#define malloc(n) allocate(n, 0) // Inject tracked native allocation failures.
#define mallocz(n,z) allocate(n, z) // Preserve production zeroing semantics.
#define free(p) deallocate(p) // Check allocation guards before freeing.
#define BIT(bt, nb) ((bt)&(1<<(nb))) // Production pointer-map bit selection.
IMPLEMENTATION
#undef malloc
#undef mallocz
#undef free

static F_Sys_file2chan frame;
static Sys_FileIO *returned;


/* Invoke the actual builtin without moving frame state across a nonlocal jump. */
static int
invoke(void *unused)
{
	(void)unused;
	Sys_file2chan(activeframe);
	return 1;
}


/* Prepare a previous result with two references to the same channel. */
static Sys_FileIO*
previous(void)
{
	Sys_FileIO *file;

	file = H2D(Sys_FileIO*, heap(TFileIO));
	file->read = cnewc(FioTread, movtmp, 16);
	file->write = file->read;
	D2H(file->read)->ref++;
	return file;
}


/* Compare one run's ownership and error behavior with the requested failure boundary. */
static int
one(int wide, int oldresult, int fail)
{
	Sys_FileIO *old, *expectedold;
	int ok, count, baseline, oldreleased;

	armed = 0;
	failure = 0;
	baseline = live;
	frame.dir = wide ? c2string("/ch\303\251n", 6) : c2string("/chan", 5);
	frame.file = wide ? c2string("svc\360\237\230\200", 7) : c2string("service", 7);
	old = returned = oldresult ? previous() : H;
	memset(returnslot.bytes, 0xa5, sizeof(returnslot.bytes));
	memmove(returnslot.bytes+8+returnoffset, &returned, sizeof(returned));
	frame.ret = (void*)(returnslot.bytes+8+returnoffset);
	watchedowner = old;
	memset(frameslot.bytes, 0xa5, sizeof(frameslot.bytes));
	activeframe = frameslot.bytes+8+frameoffset;
	memmove(activeframe, &frame, sizeof(frame));
	memmove(expectedframe, frameslot.bytes, sizeof(expectedframe));
	armed = 1;
	mutated = 0;
	allocations = nativecalls = 0;
	failure = fail;
	raised = nil;
	ok = kprotect(invoke, nil);
	armed = 0;
	filetest(!memcmp(expectedframe, frameslot.bytes, sizeof(expectedframe)));
	memmove(&returned, returnslot.bytes+8+returnoffset, sizeof(returned));
	expectedold = phase == 6 && mutated ? H : old;
	oldreleased = watchedowner == H;
	filetest(returned == expectedold || oldreleased);
	{
		uchar expected[32];
		memset(expected, 0xa5, sizeof(expected));
		memmove(expected+8+returnoffset, &returned, sizeof(returned));
		filetest(!memcmp(expected, returnslot.bytes, sizeof(expected)));
	}
	watchedowner = H;
	count = allocations;
	failure = 0;
	filetest(worker.nerr == 0 && !released);
	if(fail > 0 && fail <= count){
		filetest(!ok && raised != nil && returned == expectedold);
		filetest(strcmp(environment.errstr, raised) == 0);
		filetest(nativecalls == 0);
	}else if(nativekind == 2){
		filetest(!ok && raised == registration && returned == expectedold);
		filetest(strcmp(environment.errstr, registration) == 0 && nativecalls == 1);
	}else{
		filetest(ok && raised == nil && nativecalls == 1);
		filetest(oldreleased);
		if(nativekind == 1){
			filetest(returned == H && strcmp(environment.errstr, registration) == 0);
		}else{
			filetest(returned != H && returned != old);
			filetest(returned->read != H && returned->write != H && returned->read != returned->write);
		}
	}
	destroy(returned);
	if(expectedold != old && !oldreleased)
		destroy(old);
	destroy(frame.dir);
	destroy(frame.file);
	filetest(live == baseline);
	filetest(TFileIO->ref == 1 && FioTread->ref == 1 && FioTwrite->ref == 1);
	filetest(Tchannel.ref == 1 && Tarray.ref == 1 && Tstring.ref == 1);
	checks++;
	return count;
}


/* Reject malformed frames, complete result spans and old owners before construction. */
static void
preflight(int mode)
{
	Sys_FileIO *old, *bad;
	uchar expected[sizeof(returnslot.bytes)];
	char *expectederror;
	int baseline, ok;

	armed = failure = nativecalls = allocations = 0;
	watchedowner = H;
	old = previous();
	baseline = live;
	memset(&frame, 0, sizeof(frame));
	frame.ret = (void*)(returnslot.bytes+8+returnoffset);
	frame.dir = frame.file = H;
	bad = old;
	expectederror = exNilref;
	if(mode == 2 || mode == 5)
		expectederror = exBounds;
	if(mode >= 7)
		expectederror = exHeap;
	if(mode == 3)
		frame.ret = nil;
	if(mode == 4)
		frame.ret = H;
	if(mode == 5)
		frame.ret = (void*)((uintptr)-1-sizeof(void*)+1);
	if(mode == 6)
		bad = nil;
	if(mode == 7)
		bad = (void*)sizeof(Heap);
	if(mode == 8)
		bad = (void*)((uchar*)old+1);
	if(mode == 9)
		D2H(old)->ref = 0;
	memset(returnslot.bytes, 0xa5, sizeof(returnslot.bytes));
	memmove(returnslot.bytes+8+returnoffset, &bad, sizeof(bad));
	memmove(expected, returnslot.bytes, sizeof(expected));
	memset(frameslot.bytes, 0xa5, sizeof(frameslot.bytes));
	activeframe = frameslot.bytes+8+frameoffset;
	memmove(activeframe, &frame, sizeof(frame));
	memmove(expectedframe, frameslot.bytes, sizeof(expectedframe));
	if(mode == 0)
		activeframe = nil;
	if(mode == 1)
		activeframe = H;
	if(mode == 2)
		activeframe = (void*)((uintptr)-1-sizeof(frame)+1);
	allocations = 0;
	raised = nil;
	ok = kprotect(invoke, nil);
	filetest(!ok && raised == expectederror);
	filetest(!memcmp(expected, returnslot.bytes, sizeof(expected)));
	filetest(!memcmp(expectedframe, frameslot.bytes, sizeof(expectedframe)));
	filetest(allocations == 0 && nativecalls == 0 && live == baseline);
	filetest(worker.nerr == 0 && !released);
	filetest(D2H(old)->ref == (mode == 9 ? 0 : 1));
	filetest(old->read == old->write && D2H(old->read)->ref == 2);
	if(mode == 9)
		D2H(old)->ref = 1;
	destroy(old);
	filetest(TFileIO->ref == 1 && FioTread->ref == 1 && FioTwrite->ref == 1);
	filetest(Tchannel.ref == 1 && Tarray.ref == 1 && Tstring.ref == 1);
	checks++;
}


/* Cover layout maps, acquired UTF caches, partial channels, old owners and native errors. */
static int
filematrix(int argc, char **argv)
{
	int wide, old, fail, count, i, normal, wideonly;
	int phasefirst, phaselimit, framefirst, framelimit;
	Heap *reply;
	Type *types[9];

	if(getenv("FILE2CHAN_MODE"))
		filetest(atoi(getenv("FILE2CHAN_MODE")) >= 0 && atoi(getenv("FILE2CHAN_MODE")) < 10);
	phasefirst = getenv("FILE2CHAN_PHASE") ? atoi(getenv("FILE2CHAN_PHASE")) : 0;
	phaselimit = getenv("FILE2CHAN_PHASE") ? phasefirst+1 : getenv("FILE2CHAN_CAPTURE") ? 7 : 1;
	framefirst = getenv("FILE2CHAN_FRAME") ? atoi(getenv("FILE2CHAN_FRAME")) : 0;
	framelimit = getenv("FILE2CHAN_FRAME") ? framefirst+1 : getenv("FILE2CHAN_CAPTURE") ? 8 : 1;
	filetest(phasefirst >= 0 && phaselimit <= 7 && framefirst >= 0 && framelimit <= 8);
	normal = argc > 1 && strcmp(argv[1], "normal") == 0;
	wideonly = argc > 1 && strcmp(argv[1], "wide") == 0;
	worker.env = &environment;
	environment.errstr = environment.errbuf0;
	sysinit();
	srvinit();
	filetest(TFileIO && TFileIO->size == sizeof(Sys_FileIO));
	filetest(FioTread && FioTread->size == sizeof(Sys_FileIO_read));
	filetest(FioTwrite && FioTwrite->size == sizeof(Sys_FileIO_write));
	filetest(dev.Rread && dev.Rread->size == sizeof(Sys_Rread));
	filetest(dev.Rwrite && dev.Rwrite->size == sizeof(Sys_Rwrite));
	filetest(FioTread->map[0] == (0x80 >> (offsetof(Sys_FileIO_read, t3)/sizeof(void*))));
	reply = heap(dev.Rread);
	filetest(H2D(Sys_Rread*, reply)->t0 == H && H2D(Sys_Rread*, reply)->t1 == H);
	destroy(H2D(void*, reply));
	reply = heap(dev.Rwrite);
	filetest(H2D(Sys_Rwrite*, reply)->t1 == H);
	destroy(H2D(void*, reply));
	checks++;
	if(getenv("FILE2CHAN_PREFLIGHT")){
		for(frameoffset = 0; frameoffset < 8; frameoffset++)
		for(returnoffset = 0; returnoffset < 8; returnoffset++)
		for(i = 0; i < 10; i++)
			if(!getenv("FILE2CHAN_MODE") || i == atoi(getenv("FILE2CHAN_MODE")))
				preflight(i);
	}
	if(!getenv("FILE2CHAN_PREFLIGHT") && (argc == 1 || strcmp(argv[1], "layout") != 0))
	for(nativekind = 0; nativekind < (wideonly ? 1 : normal ? 2 : 3); nativekind++)
	for(phase = phasefirst; phase < phaselimit; phase++)
	for(frameoffset = framefirst; frameoffset < framelimit; frameoffset++)
	for(returnoffset = 0; returnoffset < 8; returnoffset++)
	for(wide = wideonly; wide < 2; wide++)for(old = 0; old < 2; old++){
		throwing = 0;
		count = one(wide, old, 0);
		for(throwing = 0; throwing < (normal || wideonly ? 0 : 2); throwing++)for(fail = 1; fail <= count+1; fail++)
			one(wide, old, fail);
	}
	types[0] = TFD; types[1] = TFileIO; types[2] = FioTread; types[3] = FioTwrite;
	types[4] = Tdir; types[5] = Trdchan; types[6] = Twrchan;
	types[7] = dev.Rread; types[8] = dev.Rwrite;
	for(i = 0; i < 9; i++){
		filetest(types[i] && types[i]->ref == 1);
		freetype(types[i]);
	}
	filetest(live == 0);
	printf("Sys file2chan: %d layout/channel/string/registration/failure cases\n", checks);
	return 0;
}

/* Preserve selectors while executing nested VM errors on a real pthread stack. */
struct FileArguments {
	int argc; // Number of selectors passed to the fixture.
	char **argv; // Original test arguments.
};


/* Run complete constructors, finalizers and real error frames on the worker stack. */
static void*
fileworker(void *argument)
{
	struct FileArguments *args = argument;

	filetest(filematrix(args->argc, args->argv) == 0);
	return nil;
}


/* Join before returning so every ownership and guard check has completed. */
int
main(int argc, char **argv)
{
	struct FileArguments args;
	pthread_t thread;

	args.argc = argc;
	args.argv = argv;
	filetest(pthread_create(&thread, nil, fileworker, &args) == 0);
	filetest(pthread_join(thread, nil) == 0);
	return 0;
}
