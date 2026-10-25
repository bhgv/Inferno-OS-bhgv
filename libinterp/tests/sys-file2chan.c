#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "pool.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static union { uvlong align; uchar bytes[32]; } returnslot;
static int returnoffset;
static void *watchedowner;
static Osenv environment;
static char *raised;
char exNomem[] = "malloc", exHeap[] = "heap", exNegsize[] = "negative", exNilref[] = "nil";
static char injected[] = "injected", registration[] = "registration";
static int allocations, failure, throwing, live, checks, released, nativecalls, nativekind;
static struct { void *pointer; ulong size; } blocks[256];
FD_DECLARATION
static Type *TFD, *TFileIO, *FioTread, *FioTwrite, *Tdir;
static uchar FDmap[] = Sys_FD_map, FileIOmap[] = Sys_FileIO_map;
static uchar rmap[] = Sys_FileIO_read_map, wmap[] = Sys_FileIO_write_map, dmap[] = Sys_Dir_map;
static Type *Trdchan, *Twrchan;
static struct { Type *Rread, *Rwrite; ulong pathgen; } dev;

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
	assert(worker.nerr > 0);
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

	assert(!released);
	if(++allocations == failure){
		if(throwing)
			error(injected);
		return nil;
	}
	assert(size <= 1024*1024);
	pointer = malloc(size+32);
	assert(pointer);
	memset(pointer, zero ? 0 : 0xa5, size);
	memset((uchar*)pointer+size, 0xa5, 32);
	for(i = 0; i < 256 && blocks[i].pointer; i++);
	assert(i < 256);
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
	assert(i < 256);
	for(j = 0; j < 32; j++)
		assert(((uchar*)pointer)[blocks[i].size+j] == 0xa5);
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
		assert(head->prog == nil);
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
	if(value==watchedowner){
		Sys_FileIO *published;
		memmove(&published,returnslot.bytes+8+returnoffset,sizeof(published));
		assert(published!=watchedowner);
		watchedowner=H;
	}
	assert(!released);
	h = D2H(value);
	assert(h->ref > 0);
	if(--h->ref)
		return;
	type = h->t;
	assert(type && type->free);
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
	assert(!released);
	released = 1;
}

/* Construction failures and native exceptions must restore VM ownership. */
void
acquire(void)
{
	assert(released);
	released = 0;
}

/* Check real request-ring initialization before simulating atomic native registration. */
int
srvf2c(char *directory, char *name, Sys_FileIO *file)
{
	Sys_FileIO_read *read;
	Sys_FileIO_write *write;
	int i;

	assert(released && file->read != H && file->write != H);
	assert(strcmp(directory, "/chan") == 0 || strcmp(directory, "/ch\303\251n") == 0);
	assert(strcmp(name, "service") == 0 || strcmp(name, "svc\360\237\230\200") == 0);
	nativecalls++;
	assert(file->read->mid.t == FioTread && file->write->mid.t == FioTwrite);
	assert(file->read->buf->len == 16 && file->write->buf->len == 16);
	read = (Sys_FileIO_read*)file->read->buf->data;
	write = (Sys_FileIO_write*)file->write->buf->data;
	for(i = 0; i < 16; i++){
		assert(read[i].t3 == H);
		assert(write[i].t1 == H && write[i].t3 == H);
	}
	if(nativekind == 2)
		error(registration);
	if(nativekind == 1){
		strcpy(environment.errstr, registration);
		return -1;
	}
	return 0;
}

#define malloc(n) allocate(n, 0)
#define mallocz(n,z) allocate(n, z)
#define free(p) deallocate(p)
#define BIT(bt, nb) ((bt)&(1<<(nb)))
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
	Sys_file2chan(&frame);
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
	Sys_FileIO *old;
	int ok, count, baseline;

	failure = 0;
	baseline = live;
	frame.dir = wide ? c2string("/ch\303\251n", 6) : c2string("/chan", 5);
	frame.file = wide ? c2string("svc\360\237\230\200", 7) : c2string("service", 7);
	old = returned = oldresult ? previous() : H;
	memset(returnslot.bytes,0xa5,sizeof(returnslot.bytes));
	memmove(returnslot.bytes+8+returnoffset,&returned,sizeof(returned));
	frame.ret = (void*)(returnslot.bytes+8+returnoffset);
	watchedowner=old;
	allocations = nativecalls = 0;
	failure = fail;
	raised = nil;
	ok = kprotect(invoke, nil);
	memmove(&returned,returnslot.bytes+8+returnoffset,sizeof(returned));
	assert(returned==old || watchedowner==H);
	{
		uchar expected[32];
		memset(expected,0xa5,sizeof(expected));
		memmove(expected+8+returnoffset,&returned,sizeof(returned));
		assert(!memcmp(expected,returnslot.bytes,sizeof(expected)));
	}
	watchedowner=H;
	count = allocations;
	failure = 0;
	assert(worker.nerr == 0 && !released);
	if(fail > 0 && fail <= count){
		assert(!ok && raised != nil && returned == old);
		assert(strcmp(environment.errstr, raised) == 0);
		assert(nativecalls == 0);
	}else if(nativekind == 2){
		assert(!ok && raised == registration && returned == old);
		assert(strcmp(environment.errstr, registration) == 0 && nativecalls == 1);
	}else{
		assert(ok && raised == nil && nativecalls == 1);
		if(nativekind == 1){
			assert(returned == H && strcmp(environment.errstr, registration) == 0);
		}else{
			assert(returned != H && returned != old);
			assert(returned->read != H && returned->write != H && returned->read != returned->write);
		}
	}
	destroy(returned);
	destroy(frame.dir);
	destroy(frame.file);
	assert(live == baseline);
	assert(TFileIO->ref == 1 && FioTread->ref == 1 && FioTwrite->ref == 1);
	assert(Tchannel.ref == 1 && Tarray.ref == 1 && Tstring.ref == 1);
	checks++;
	return count;
}

/* Cover layout maps, acquired UTF caches, partial channels, old owners and native errors. */
int
main(int argc, char **argv)
{
	int wide, old, fail, count, i, normal, wideonly;
	Heap *reply;
	Type *types[9];

	normal = argc > 1 && strcmp(argv[1], "normal") == 0;
	wideonly = argc > 1 && strcmp(argv[1], "wide") == 0;
	worker.env = &environment;
	environment.errstr = environment.errbuf0;
	sysinit();
	srvinit();
	assert(TFileIO && TFileIO->size == sizeof(Sys_FileIO));
	assert(FioTread && FioTread->size == sizeof(Sys_FileIO_read));
	assert(FioTwrite && FioTwrite->size == sizeof(Sys_FileIO_write));
	assert(dev.Rread && dev.Rread->size == sizeof(Sys_Rread));
	assert(dev.Rwrite && dev.Rwrite->size == sizeof(Sys_Rwrite));
	assert(FioTread->map[0] == (0x80 >> (offsetof(Sys_FileIO_read, t3)/sizeof(void*))));
	reply = heap(dev.Rread);
	assert(H2D(Sys_Rread*, reply)->t0 == H && H2D(Sys_Rread*, reply)->t1 == H);
	destroy(H2D(void*, reply));
	reply = heap(dev.Rwrite);
	assert(H2D(Sys_Rwrite*, reply)->t1 == H);
	destroy(H2D(void*, reply));
	checks++;
	if(argc == 1 || strcmp(argv[1], "layout") != 0)
	for(nativekind = 0; nativekind < (wideonly ? 1 : normal ? 2 : 3); nativekind++)
	for(returnoffset=0;returnoffset<8;returnoffset++)
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
		assert(types[i] && types[i]->ref == 1);
		freetype(types[i]);
	}
	assert(live == 0);
	printf("Sys file2chan: %d layout/channel/string/registration/failure cases\n", checks);
	return 0;
}
