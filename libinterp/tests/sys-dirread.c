#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static Array *watchedarray;
static uchar *watchedstatus, *watchedslot;
static char *raised;
char exHeap[] = "heap", exNegsize[] = "negative", exBounds[] = "bounds", exNomem[] = "memory", exNilref[] = "nil";
Type Tstring = {1}, Tarray = {1};
void freearray(Heap*, int);
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
	nexterror();
}

/* Heap blocks are counted explicitly; process inspection for LSan is unavailable. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Poison payloads so cleanup cannot rely on uninitialized list heads being nil. */
Heap*
nheap(int size)
{
	Heap *heap;
	int i;

	if(++allocations == failure)
		error(exHeap);
	assert(size >= 0 && size < 65536);
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

/* Validate guards and head/tail ownership while releasing every completed word. */
void
destroy(void *value)
{
	Heap *heap;

	int i, j;

	if(value == H)
		return;
	if(value==watchedarray){
		Array *published;
		WORD status;
		memmove(&published,watchedslot,sizeof(published));
		memmove(&status,watchedstatus,sizeof(status));
		assert(published==H && status==-1);
		watchedarray=H;
	}
	heap = D2H(value);
	for(i = 0; i < 256 && blocks[i].heap != heap; i++);
	assert(i < 256 && heap->ref > 0);
	for(j = 0; j < 32; j++)
		assert(((uchar*)(heap+1))[blocks[i].size+j] == 0xa5);
	if(--heap->ref)
		return;
	if(heap->t == &Tarray){
		freearray(heap, 0);
		Tarray.ref--;
	}else{
		assert(heap->t == &Tstring);
		assert(((String*)value)->tmp == nil);
		Tstring.ref--;
	}
	blocks[i].heap = nil;
	free(heap);
	live--;
}

static Type *TFD, *TFileIO, *FioTread, *FioTwrite, *Tdir;
static uchar FDmap[] = Sys_FD_map, FileIOmap[] = Sys_FileIO_map;
static uchar rmap[] = Sys_FileIO_read_map, wmap[] = Sys_FileIO_write_map, dmap[] = Sys_Dir_map;
typedef struct FD FD;
struct FD { Sys_FD fd; Fgrp *grp; };
static int nativecount, nativelive, released, pattern;
static void *nativebuffer;
static char *texts[] = {"record", "u\303\251", "group\360\237\230\200", ""};

/* Descriptor construction retains the actual generated maps and native size expressions. */
Type*
dtype(void (*freefn)(Heap*, int), int size, uchar *map, int np)
{
	static Type types[5];
	static int used;
	Type *type;

	assert(used < 5 && np <= sizeof(types[0].map));
	type = &types[used++];
	type->ref = 1;
	type->size = size;
	type->np = np;
	memmove(type->map, map, np);
	(void)freefn;
	return type;
}

/* These unrelated descriptor callbacks are never invoked by directory conversion. */
void freeFD(Heap *h, int swept) { (void)h; (void)swept; abort(); }
void freeFileIO(Heap *h, int swept) { (void)h; (void)swept; abort(); }
void freeheap(Heap *h, int swept) { (void)h; (void)swept; abort(); }

/* Enforce the scheduler ownership boundary around the native directory read. */
void release(void) { assert(!released); released = 1; }
void acquire(void) { assert(released); released = 0; }

/* Create independent native records with metadata outside 32-bit scalar ranges. */
long
kdirread(int fd, Dir **entries)
{
	Dir *dir;
	int i;

	assert(released && fd == 17 && !nativelive);
	*entries = nil;
	if(nativecount <= 0)
		return nativecount;
	dir = calloc(nativecount, sizeof(Dir));
	assert(dir);
	nativebuffer = dir;
	nativelive = 1;
	for(i = 0; i < nativecount; i++){
		dir[i].name = texts[0];
		dir[i].uid = pattern ? nil : texts[1];
		dir[i].gid = texts[2];
		dir[i].muid = texts[3];
		dir[i].qid.path = ((uvlong)1<<48)+i;
		dir[i].qid.vers = 123+i;
		dir[i].qid.type = i&1;
		dir[i].mode = 0640+i;
		dir[i].atime = 1000+i;
		dir[i].mtime = 2000+i;
		dir[i].length = ((vlong)1<<45)+i;
		dir[i].type = 7+i;
		dir[i].dev = 11+i;
	}
	*entries = dir;
	return nativecount;
}

/* Track native-buffer ownership independently of every VM heap allocation. */
static void
nativefree(void *pointer)
{
	if(pointer == nil)
		return;
	assert(nativelive && pointer == nativebuffer);
	nativelive = 0;
	nativebuffer = nil;
	free(pointer);
}
#define BIT(bt, nb) ((bt)&(1<<(nb)))
#define fdchk(x) ((x) == (Sys_FD*)H ? -1 : (x)->fd)
#define free nativefree
IMPLEMENTATION
#undef free

/* Invoke the builtin under an outer frame to detect unexpected escaping errors. */
static int
invoke(void *frame)
{
	frame = dirbegin(frame);
	Sys_dirread(frame);
	return 1;
}


/* Compare converted characters with a separately decoded expected string. */
static void
checkstring(String *string, char *text)
{
	Rune rune;
	int i, n;

	if(text == nil){
		assert(string == H);
		return;
	}
	i = 0;
	while(*text){
		n = chartorune(&rune, text);
		assert(string != H);
		assert((string->len < 0 ? string->Srune[i] : (uchar)string->Sascii[i]) == rune);
		i++;
		text += n;
	}
	assert(string != H && (string->len < 0 ? -string->len : string->len) == i);
}

static int dirphase, dirframeoffset, dirfdoffset, diractive, dirmutated, dirbadfd, dirbadargs;
static WORD dirnumber = 17;
static int dirsentinel;
static uchar dirframe[sizeof(F_Sys_dirread)+32], dirimage[sizeof(dirframe)];
static uchar dirhandle[sizeof(Sys_FD)+8];
static uchar *diroriginal;


/* Keep all allocation, metadata and owner predicates active under NDEBUG. */
static void
dirpredicate(int valid, int line)
{
	if(!valid) {
		fprintf(stderr, "Sys dirread predicate failed at %d phase=%d\n", line, dirphase);
		abort();
	}
}


/* Supply a small owned buffer while reporting an intentionally impossible native count. */
static int
dircountreport(Dir **entries)
{
	Dir *dir;

	if(!dirreportactive)
		return 0;
	dir = calloc(1, sizeof(Dir));
	dirtest(dir != nil);
	nativebuffer = dir;
	nativelive = 1;
	*entries = dir;
	return 1;
}


/* Observe descriptor narrowing failures before native entry and allocation. */
void
seterror(char *message, ...)
{
	dirtest(message == Ebadarg && !released);
	dirbadargs++;
}


/* Preserve changes to the original frame and borrowed FD at every reached boundary. */
static void
dircallback(int point)
{
	WORD number = 99;

	if(!diractive || dirphase != point || dirmutated)
		return;
	dirmutated = 1;
	memset(diroriginal, 0xa7, sizeof(F_Sys_dirread));
	memmove(dirhandle+dirfdoffset+O(Sys_FD, fd), &number, sizeof(number));
}


/* Supply packed fixed frames and byte-readable FD views to the actual wrapper. */
static void*
dirbegin(void *frame)
{
	Sys_FD handle;
	Sys_FD *pointer;

	diroriginal = dirframe+8+dirframeoffset;
	memset(dirframe, 0x3c, sizeof(dirframe));
	memmove(diroriginal, frame, sizeof(F_Sys_dirread));
	memset(&handle, 0, sizeof(handle));
	handle.fd = dirnumber;
	memmove(dirhandle+dirfdoffset, &handle, sizeof(handle));
	pointer = dirsentinel ? (Sys_FD*)H : (Sys_FD*)(dirhandle+dirfdoffset);
	memmove(diroriginal+O(F_Sys_dirread, fd), &pointer, sizeof(pointer));
	memmove(dirimage, dirframe, sizeof(dirimage));
	diractive = 1;
	dirmutated = dirbadargs = 0;
	return diroriginal;
}


/* Compare complete original frame images even after partial-conversion cleanup. */
static void
dirfinish(void)
{
	diractive = 0;
	if(dirmutated)
		memset(dirimage+(diroriginal-dirframe), 0xa7, sizeof(F_Sys_dirread));
	dirtest(!memcmp(dirframe, dirimage, sizeof(dirimage)));
	dirtest(dirbadargs == (dirbadfd || dirreportactive));
}


static int returnoffset;


/* Each failure must return (-1,nil), release old/partial results and free native entries. */
static void
one(int count, int fail)
{
	F_Sys_dirread frame = {0};
	Sys_FD fd = {17};
	struct {
		WORD t0; // Leading status from the generated tuple layout.
		Array *t1; // Pointer-aligned previous or published VM array.
	} result;
	Sys_Dir *record;
	union {
		uvlong align; // Align the tuple backing store before applying byte shifts.
		uchar bytes[64]; // Keep canaries around the complete result tuple.
	} returnslot;
	uchar expected[64];
	int i, budget;

	failure = 0;
	memset(&result,0xa5,sizeof(result));
	result.t0 = -77;
	result.t1 = H2D(Array*, heaparray(Tdir, 1));
	record = (Sys_Dir*)result.t1->data;
	retstr("previous", &record->name);
	frame.fd = &fd;
	memset(returnslot.bytes,0xa5,sizeof(returnslot.bytes));
	memmove(returnslot.bytes+8+returnoffset,&result,sizeof(result));
	memmove(expected,returnslot.bytes,sizeof(expected));
	frame.ret = (void*)(returnslot.bytes+8+returnoffset);
	watchedstatus=returnslot.bytes+8+returnoffset;
	watchedslot=watchedstatus+((uchar*)&result.t1-(uchar*)&result);
	watchedarray=result.t1;
	nativecount = count;
	allocations = 0;
	failure = fail;
	raised = nil;
	dirtest(kprotect(invoke, &frame));
	dirfinish();
	dirtest(watchedarray==H);
	memmove(&result,returnslot.bytes+8+returnoffset,sizeof(result));
	memmove(expected+8+returnoffset,&result.t0,sizeof(result.t0));
	memmove(expected+8+returnoffset+((uchar*)&result.t1-(uchar*)&result),&result.t1,sizeof(result.t1));
	dirtest(!memcmp(expected,returnslot.bytes,sizeof(expected)));
	dirtest(worker.nerr == 0 && !released && !nativelive);
	failure = 0;
	budget = !dirbadfd && count > 0 ? 1+(pattern ? 3 : 4)*count : 0;
	if(fail > 0 && fail <= budget){
		dirtest(result.t0 == -1 && result.t1 == H && raised == exHeap);
	}else if(count <= 0 || dirbadfd){
		dirtest(result.t0 == (dirbadfd ? -1 : count) && result.t1 == H);
	}else{
		dirtest(result.t0 == count && result.t1 != H && result.t1->len == count);
		dirtest(result.t1->t == Tdir);
		record = (Sys_Dir*)result.t1->data;
		for(i = 0; i < count; i++){
			checkstring(record[i].name, texts[0]);
			checkstring(record[i].uid, pattern ? nil : texts[1]);
			checkstring(record[i].gid, texts[2]);
			checkstring(record[i].muid, texts[3]);
			dirtest(record[i].qid.path == ((uvlong)1<<48)+i);
			dirtest(record[i].qid.vers == 123+i && record[i].qid.qtype == (i&1));
			dirtest(record[i].mode == 0640+i && record[i].atime == 1000+i && record[i].mtime == 2000+i);
			dirtest(record[i].length == ((vlong)1<<45)+i);
			dirtest(record[i].dtype == 7+i && record[i].dev == 11+i);
		}
	}
	destroy(result.t1);
	dirtest(!live && Tdir->ref == 1 && Tstring.ref == 1 && Tarray.ref == 1);
	checks++;
}


/* Enter directly when deliberately invalid frames must reach production preflight. */
static int
rawdirectory(void *frame)
{
	Sys_dirread(frame);
	return 1;
}


/* Keep the previous result and every tuple byte intact when metadata preflight fails. */
static void
dirpreflight(int mode)
{
	F_Sys_dirread frame = {0};
	Sys_FD fd = {17};
	struct {
		WORD status; // Preserve the complete leading status sentinel.
		Array *array; // Retain the old owner throughout invalid metadata checks.
	} result;
	Array *old, *candidate;
	uchar bytes[64], before[64];
	void *fp, *ret;
	uintptr offset;
	char *expected;
	int ok;

	failure = 0;
	old = H2D(Array*, heaparray(Tdir, 1));
	memset(&result, 0xa5, sizeof(result));
	result.status = -77;
	result.array = old;
	memset(bytes, 0x3c, sizeof(bytes));
	memmove(bytes+8, &result, sizeof(result));
	offset = (uchar*)&result.array-(uchar*)&result;
	ret = bytes+8;
	candidate = old;
	if(mode == 6)candidate = nil;
	if(mode == 7)candidate = (Array*)1;
	if(mode == 8)candidate = (Array*)((uchar*)old+1);
	if(mode == 9)D2H(old)->ref = 0;
	memmove((uchar*)ret+offset, &candidate, sizeof(candidate));
	memmove(before, bytes, sizeof(before));
	if(mode == 3)ret = nil;
	if(mode == 4)ret = H;
	if(mode == 5)ret = (void*)((uintptr)-1-sizeof(WORD)-1);
	frame.ret = ret;
	frame.fd = &fd;
	if(mode == 10)frame.fd = nil;
	if(mode == 11)frame.fd = (Sys_FD*)((uintptr)-1-1);
	fp = &frame;
	if(mode == 0)fp = nil;
	if(mode == 1)fp = H;
	if(mode == 2)fp = (void*)((uintptr)-1-1);
	expected = mode == 2 || mode == 5 || mode == 11 ? exBounds : mode >= 7 && mode <= 9 ? exHeap : exNilref;
	diractive = 0;
	dirbadargs = allocations = 0;
	watchedarray = H;
	raised = nil;
	ok = kprotect(rawdirectory, fp);
	dirtest(!ok && raised == expected && worker.nerr == 0 && !released);
	dirtest(!nativelive && !allocations && !dirbadargs && !memcmp(bytes, before, sizeof(bytes)));
	if(mode == 9)D2H(old)->ref = 1;
	destroy(old);
	dirtest(!live && Tdir->ref == 1 && Tarray.ref == 1 && Tstring.ref == 1);
	checks++;
}


/* Cover EOF, native errors, every field allocation and multi-record native strides. */
static int
dirmatrix(int argc, char **argv)
{
	int count, fail, maximum, index, mode;
	vlong reports[] = {2147483648LL, -2147483649LL, 4294967297LL,
		-1099511627776LL, 9223372036854775807LL, -9223372036854775807LL-1};
	vlong numbers[] = {17, 0, -1, 0x7fffffff, -0x7fffffff-1, -1,
		2147483648LL, -2147483649LL, 4294967313LL, -1099511627776LL,
		9223372036854775807LL, -9223372036854775807LL-1};
	char *family = getenv("DIRREAD_FAMILY");

	sysinit();
	dirtest(Tdir->size == sizeof(Sys_Dir));
	if(family != nil && !strcmp(family, "preflight")) {
		for(mode=0; mode<12; mode++) {
			if(getenv("DIRREAD_ERROR") && mode != atoi(getenv("DIRREAD_ERROR")))continue;
			dirpreflight(mode);
		}
	} else if(family != nil && !strcmp(family, "nativecount")) {
		dirtest(sizeof(long) >= 8);
		dirreportactive = 1;
		for(index=0; index<6; index++)
		for(dirphase=0; dirphase<7; dirphase++)
		for(dirframeoffset=0; dirframeoffset<8; dirframeoffset++)
		for(dirfdoffset=0; dirfdoffset<8; dirfdoffset++)
		for(returnoffset=0; returnoffset<8; returnoffset++) {
			if(getenv("DIRREAD_REPORT") && index != atoi(getenv("DIRREAD_REPORT")))continue;
			dirreported = reports[index];
			one(-1, 0);
		}
	} else if(family != nil && !strcmp(family, "nativefd")) {
		for(index=0; index<(sizeof(WORD)>4 ? 12 : 6); index++)
		for(dirframeoffset=0; dirframeoffset<8; dirframeoffset++)
		for(dirfdoffset=0; dirfdoffset<8; dirfdoffset++)
		for(returnoffset=0; returnoffset<8; returnoffset++)
		for(pattern=0; pattern<2; pattern++)
		for(fail=0; fail<12; fail++) {
			if(getenv("DIRREAD_NUMBER") && index != atoi(getenv("DIRREAD_NUMBER")))continue;
			dirphase = getenv("DIRREAD_PHASE") == nil ? 1 : atoi(getenv("DIRREAD_PHASE"));
			dirnumber = numbers[index];
			dirsentinel = index == 5;
			dirbadfd = index >= 6;
			dirwantfd = dirbadfd ? 17 : (int)numbers[index];
			one(2, fail);
		}
	} else {
	if(argc == 1 || strcmp(argv[1], "failure") == 0 || strcmp(argv[1], "content") == 0){
		for(dirphase=0; dirphase<(getenv("DIRREAD_CAPTURE") ? 7 : 1); dirphase++)
		for(dirframeoffset=0; dirframeoffset<(getenv("DIRREAD_CAPTURE") ? 8 : 1); dirframeoffset++)
		for(dirfdoffset=0; dirfdoffset<(getenv("DIRREAD_CAPTURE") ? 8 : 1); dirfdoffset++)
		for(returnoffset=0;returnoffset<8;returnoffset++)
		for(pattern = 0; pattern < 2; pattern++)for(count = -1; count <= (getenv("DIRREAD_CAPTURE") ? 3 : 16); count++){
			if(getenv("DIRREAD_PHASE") && dirphase != atoi(getenv("DIRREAD_PHASE")))continue;
			if(getenv("DIRREAD_FRAME") && dirframeoffset != atoi(getenv("DIRREAD_FRAME")))continue;
			if(getenv("DIRREAD_FD_SHIFT") && dirfdoffset != atoi(getenv("DIRREAD_FD_SHIFT")))continue;
			maximum = argc > 1 && strcmp(argv[1], "content") == 0 ? 0 : (count > 0 ? 4*count+2 : 2);
			for(fail = 0; fail <= maximum; fail++)
				one(count, fail);
		}
	}
	}
	dirtest(checks > 0 || (argc > 1 && !strcmp(argv[1], "layout")));
	printf("Sys dirread: %d layout/metadata/ownership/failure cases\n", checks+1);
	return 0;
}

/* Preserve command-line families while exercising real VM errors on a tracked stack. */
struct DirectoryArguments {
	int argc; // Number of layout/content/failure selectors.
	char **argv; // Native fixture arguments.
};


/* Joinable workers retain every real nested protection frame and owner check. */
static void*
dirworker(void *argument)
{
	struct DirectoryArguments *args = argument;

	dirtest(dirmatrix(args->argc, args->argv) == 0);
	return nil;
}


/* Return success only after all native and VM ownership checks finish. */
int
main(int argc, char **argv)
{
	struct DirectoryArguments args;
	pthread_t thread;

	args.argc = argc;
	args.argv = argv;
	dirtest(pthread_create(&thread, nil, dirworker, &args) == 0);
	dirtest(pthread_join(thread, nil) == 0);
	return 0;
}
