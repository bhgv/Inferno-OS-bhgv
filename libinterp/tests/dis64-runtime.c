#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#define RuntimeBlocks 256 // Maximum simultaneously tracked fixture allocations.

struct RuntimeBlock {
	void *pointer; // Native allocation returned by the fixture allocator.
	size_t bytes; // Exact requested extent, followed by checked guard bytes.
};

extern void __real_free(void*); // Linker-provided ordinary allocator release.

static struct RuntimeBlock blocks[RuntimeBlocks]; // Tracked runtime-owned allocations.
static int live, allocations, failat, checks; // Allocation-failure injection and active predicates.


/* Keep checks active in optimized and NDEBUG builds. */
static void
require(int condition)
{
	if(!condition){
		fprintf(stderr, "Dis64 runtime check failed after %d predicates\n", checks);
		abort();
	}
	checks++;
}


/* Bound each allocation and inject every partial-construction failure. */
static void*
trackedallocate(size_t bytes, int clear)
{
	unsigned char *memory;
	int i;

	allocations++;
	if(allocations == failat || bytes > 2*1024*1024)
		return NULL;
	memory = malloc(bytes+32);
	require(memory != NULL);
	memset(memory, clear ? 0 : 0xa5, bytes);
	memset(memory+bytes, 0xa5, 32);
	for(i=0; i<RuntimeBlocks && blocks[i].pointer != NULL; i++)
		;
	require(i < RuntimeBlocks);
	blocks[i].pointer = memory;
	blocks[i].bytes = bytes;
	live++;
	return memory;
}


/* Check exact allocation guards before releasing a partial or complete owner. */
static void
trackedfree(void *pointer)
{
	unsigned int j;
	int i;

	if(pointer == NULL)
		return;
	for(i=0; i<RuntimeBlocks && blocks[i].pointer != pointer; i++)
		;
	require(i < RuntimeBlocks);
	for(j=0; j<32; j++)
		require(((unsigned char*)pointer)[blocks[i].bytes+j] == 0xa5);
	blocks[i].pointer = NULL;
	__real_free(pointer);
	live--;
}


/* Intercept ordinary free calls without rewriting the Type.free callback member. */
void
__wrap_free(void *pointer)
{
	trackedfree(pointer);
}

#define TEST_LEAF_DISPATCH_RUNTIME // Link the authoritative complete-entry executor.
#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "dat.h"
#include "fns.h"
#include "pool.h"
#include "dis64-pilot.h"
#include "vmfetch.h"

static Proc worker; // Actual emulator error-stack storage.
static char message[128]; // Last raised runtime message, including empty IEXIT.
static int raised; // Count actual instruction error transfers.
REG R;
Module *modules;
Type Tarray = {1}, Tstring = {1};
void freemodlink(Heap*, int); // Actual destructor body is linked below.
Type Tmodlink = {1, freemodlink, markheap, -1, 1, 0, 0, {0x80}};
Pool *heapmem;
int mutator, nprop, gchalt;
void (*heapmonitor)(int, void*, ulong);
void (*optab[256])(void);
extern void (*dec[256])(void);
char exHeap[]="heap", exNomem[]="memory", exBounds[]="bounds";
char exModule[]="module", exNilref[]="nil", exInval[]="invalid";
char exCompile[]="compile", exOp[]="opcode", exZdiv[]="division by zero";


/* Provide the ordinary runtime allocator with bounded fixture storage. */
void*
mallocz(ulong bytes, int clear)
{
	return trackedallocate(bytes, clear);
}


/* Preserve owned path allocation and its explicit failure boundary. */
static char*
trackedstrdup(const char *text)
{
	size_t bytes;
	char *copy;

	bytes = strlen(text)+1;
	copy = trackedallocate(bytes, 0);
	if(copy != nil)
		memmove(copy, text, bytes);
	return copy;
}

#define strdup(text) trackedstrdup(text) // Track the module's owned path string.


/* Expose the actual Proc error stack to production kprotect. */
Proc*
getup(void)
{
	return &worker;
}


/* Transfer through the actual emulator error-frame representation. */
void
nexterror(void)
{
	require(worker.nerr > 0);
	siglongjmp(worker.estack[--worker.nerr], 1);
}


/* Observe IEXIT and arithmetic errors without replacing the instruction handler. */
void
error(char *text)
{
	snprintf(message, sizeof(message), "%s", text);
	raised++;
	nexterror();
}


/* Capture constructor diagnostics without changing the error-stack depth. */
void
kwerrstr(char *format, ...)
{
	snprintf(message, sizeof(message), "%s", format);
}


/* Scalar objects have no marked fields; a collector traversal is outside this fixture. */
void
markheap(Type *type, void *object)
{
	(void)type;
	(void)object;
}


/* Return an actual pool block header so production destroy validates its identity. */
void*
poolalloc(Pool *pool, ulong bytes)
{
	Bhdr *block;

	(void)pool;
	block = trackedallocate(offsetof(Bhdr, u.data)+bytes, 0);
	if(block == nil)
		return nil;
	block->magic = MAGIC_A;
	return B2D(block);
}


/* Release the allocator block after production reference-count and type cleanup. */
void
poolfree(Pool *pool, void *pointer)
{
	(void)pool;
	trackedfree((uchar*)pointer-O(Bhdr, u.data));
}


/* A pool identity failure is never an accepted malformed-input outcome. */
static void
badpool(void *pointer, char *text, ulong caller)
{
	(void)pointer;
	(void)text;
	(void)caller;
	require(0);
}

void (*poolfault)(void*, char*, ulong) = badpool;


/* Supply only the host diagnostic return-address boundary. */
uintptr
getcallerpc(void *argument)
{
	(void)argument;
	return 0;
}


/* This constructor creates no exports, so dynamic-link teardown must stay empty. */
void
destroylinks(Module *module)
{
	require(module->ext == nil);
}


/* Dynamic modules are outside the scalar Dis64 constructor. */
void
freedyncode(Module *module)
{
	(void)module;
	require(0);
}


/* Dynamic data is never constructed by this scalar path. */
void
freedyndata(Modlink *module)
{
	(void)module;
	require(0);
}

#define OP(name) void name(void) // Declare actual extracted scalar handlers.
#define JMP(reg) branchpc(R.reg, exBounds) // Keep production checked branch publication.
IMPLEMENTATION


/* Execute the actual VM loop through the actual protected error boundary. */
static int
execute(void *argument)
{
	xec(argument);
	return 1;
}


/* Set up an actual root extent, execute to IEXIT, then unwind actual owned storage. */
static void
runmodule(Module *module, unsigned int width, unsigned long long expected)
{
	Modlink *link;
	Frame *entry;
	Prog process;
	unsigned long long result;
	Frame *source;
	int before, failure;
	unsigned int scalarwidth;

	memset(&process, 0, sizeof(process));
	module->ref++;
	link = mklinkmod(module, 0);
	link->MP = module->origmp;
	if(link->MP != H)
		D2H(link->MP)->ref++;
	process.R.M = link;
	process.R.PC = module->entry;
	entry = mallocz(module->entryt->size, 1);
	require(entry != nil);
	entry->t = module->entryt;
	source = entry;
	R.s = &source;
	newstack(&process);
	free(entry);
	process.quanta = 1000;
	failure = width/100;
	scalarwidth = width%100;
	before = raised;
	message[0] = 'x';
	require(kprotect(execute, &process) == 0);
	require(raised == before+1 && worker.nerr == 0);
	require(failure == 1 ? strcmp(message, exZdiv) == 0 :
		failure == 2 ? strcmp(message, exInval) == 0 : message[0] == 0);
	result = 0;
	if(scalarwidth == 1)
		require(module->origmp == H);
	else if(scalarwidth == 4){
		unsigned int word;

		memmove(&word, module->origmp, sizeof(word));
		result = word;
	}else
		memmove(&result, module->origmp, sizeof(result));
	require(result == expected);
	destroystack(&R);
	require(R.EX == nil && R.FP == nil && R.M == H && module->ref == 1);
	/* The retained Module pins original MP after the instance and stack are gone. */
	if(scalarwidth != 1){
		require(D2H(module->origmp)->ref == 1);
		if(scalarwidth == 4){
			unsigned int snapshot;

			memmove(&snapshot, module->origmp, sizeof(snapshot));
			require(snapshot == expected);
		}else{
			memmove(&result, module->origmp, sizeof(result));
			require(result == expected);
		}
	}
}


/* Exercise genuine construction, execution, rejection and every allocation failure. */
int
main(int argc, char **argv)
{
	uchar image[262144], before[262144];
	unsigned int bytes, width;
	unsigned long long expected;
	FILE *file;
	Module *module;
	Dis64Pilot semantic;
	int jit, failure, count;

	if(argc == 3 && strcmp(argv[1], "--unsupported") == 0){
		require(sizeof(WORD) != 4);
		file = fopen(argv[2], "rb");
		require(file != nil);
		bytes = fread(image, 1, sizeof(image), file);
		require(!ferror(file) && feof(file) && fclose(file) == 0);
		require(dis64pilotcheck(image, bytes, 4096, 64, &semantic));
		require(prepare64pilot("fixture", image, bytes, 0) == nil && allocations == 0);
		printf("PASS unchanged legacy model rejects Dis64 construction before allocation\n");
		return 0;
	}
	require(argc == 4 && sizeof(WORD) == 4 && sizeof(void*) == 8);
	width = strtoul(argv[2], nil, 10);
	expected = strtoull(argv[3], nil, 16);
	file = fopen(argv[1], "rb");
	require(file != nil);
	bytes = fread(image, 1, sizeof(image), file);
	require(!ferror(file) && feof(file));
	require(fclose(file) == 0);
	memmove(before, image, bytes);
	require(prepare64pilot(nil, image, bytes, 0) == nil && allocations == 0);
	require(prepare64pilot("fixture", (uchar*)H, bytes, 0) == nil && allocations == 0);
	require(prepare64pilot("fixture", image, bytes, -1) == nil && allocations == 0);
	require(prepare64pilot("fixture", image, bytes, 2) == nil && allocations == 0);
	require(prepare64pilot("fixture", image, 8*1024*1024+1, 0) == nil && allocations == 0);
	require(prepare64pilot("fixture", image, (ulong)-1, 0) == nil && allocations == 0);
	HANDLERS
	for(jit=0; jit<2; jit++){
		allocations = 0;
		module = prepare64pilot("fixture", image, bytes, jit);
		if(width == 0){
			require(module == nil && allocations == 0 && live == 0);
			continue;
		}
		require(module != nil && modules == nil && module->compiled == 0);
		require(module->ntype == 2 && module->type[0]->np == 0 && module->type[1]->np == 0);
		require((module->jit != nil) == jit);
		count = allocations;
		runmodule(module, width, expected);
		unload(module);
		require(live == 0 && worker.nerr == 0);
		for(failure=1; failure<=count; failure++){
			allocations = 0;
			failat = failure;
			module = prepare64pilot("fixture", image, bytes, jit);
			require(module == nil && live == 0 && worker.nerr == 0 && modules == nil);
		}
		failat = 0;
	}
	require(memcmp(image, before, bytes) == 0);
	printf("PASS Dis64 runtime %u-byte image: %d predicates; %s; scheduler process lifecycle not run\n",
		bytes, checks, width == 0 ? "rejected before allocation" : "actual heap/Inst/xec/error/unwind C/RX");
	return 0;
}
