#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
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

/* Each failure must return (-1,nil), release old/partial results and free native entries. */
static void
one(int count, int fail)
{
	F_Sys_dirread frame = {0};
	Sys_FD fd = {17};
	struct { WORD t0; Array *t1; } result;
	Sys_Dir *record;
	int i, budget;

	failure = 0;
	result.t0 = -77;
	result.t1 = H2D(Array*, heaparray(Tdir, 1));
	record = (Sys_Dir*)result.t1->data;
	retstr("previous", &record->name);
	frame.fd = &fd;
	frame.ret = (void*)&result;
	nativecount = count;
	allocations = 0;
	failure = fail;
	raised = nil;
	assert(kprotect(invoke, &frame));
	assert(worker.nerr == 0 && !released && !nativelive);
	failure = 0;
	budget = count > 0 ? 1+(pattern ? 3 : 4)*count : 0;
	if(fail > 0 && fail <= budget){
		assert(result.t0 == -1 && result.t1 == H && raised == exHeap);
	}else if(count <= 0){
		assert(result.t0 == count && result.t1 == H);
	}else{
		assert(result.t0 == count && result.t1 != H && result.t1->len == count);
		assert(result.t1->t == Tdir);
		record = (Sys_Dir*)result.t1->data;
		for(i = 0; i < count; i++){
			checkstring(record[i].name, texts[0]);
			checkstring(record[i].uid, pattern ? nil : texts[1]);
			checkstring(record[i].gid, texts[2]);
			checkstring(record[i].muid, texts[3]);
			assert(record[i].qid.path == ((uvlong)1<<48)+i);
			assert(record[i].qid.vers == 123+i && record[i].qid.qtype == (i&1));
			assert(record[i].mode == 0640+i && record[i].atime == 1000+i && record[i].mtime == 2000+i);
			assert(record[i].length == ((vlong)1<<45)+i);
			assert(record[i].dtype == 7+i && record[i].dev == 11+i);
		}
	}
	destroy(result.t1);
	assert(!live && Tdir->ref == 1 && Tstring.ref == 1 && Tarray.ref == 1);
	checks++;
}

/* Cover EOF, native errors, every field allocation and multi-record native strides. */
int
main(int argc, char **argv)
{
	int count, fail, maximum;

	sysinit();
	if(argc == 1 || strcmp(argv[1], "layout") == 0)
		assert(Tdir->size == sizeof(Sys_Dir));
	if(argc == 1 || strcmp(argv[1], "failure") == 0 || strcmp(argv[1], "content") == 0){
		for(pattern = 0; pattern < 2; pattern++)for(count = -1; count <= 16; count++){
			maximum = argc > 1 && strcmp(argv[1], "content") == 0 ? 0 : (count > 0 ? 4*count+2 : 2);
			for(fail = 0; fail <= maximum; fail++)
				one(count, fail);
		}
	}
	printf("Sys dirread: %d layout/metadata/ownership/failure cases\n", checks+1);
	return 0;
}
