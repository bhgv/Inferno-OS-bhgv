/* Compile actual RV64 sidecars on the host; never execute generated RV code here. */
#define malloc cachemalloc // Observe native-code ownership in the embedded allocator path.
#define mallocz cachemallocz // Observe sidecar allocation in both allocator paths.
#define free cachefree // Balance actual compiler cleanup against fixture allocations.
#include COMPILER_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#undef malloc
#undef mallocz
#undef free

extern void hostfree(void*) __asm__("free"); // Host release without the compiler's allocator substitution.
static int checks, live, allocations, flushes; // Active observations, including cached-refusal side effects.
static int failafter = -1; // Successful allocations permitted before a deliberate failure.
static int failflush; // Embedded cache-publication failure injection.


/* Keep address/UB checks without unavailable process-inspection leak scanning. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Keep every refusal and ownership observation active under NDEBUG. */
static void
check(int condition)
{
	if(!condition){
		fprintf(stderr, "RV cache predicate failed after %d checks\n", checks);
		abort();
	}
	checks++;
}


/* Allocate real writable storage while recording only compiler-owned blocks. */
void*
cachemalloc(size_t bytes)
{
	void *result;

	allocations++;
	if(failafter == 0)
		return nil;
	if(failafter > 0)
		failafter--;
	result = calloc(1, bytes);
	if(result != nil)
		live++;
	return result;
}


/* Match mallocz's public signature; calloc already initializes the complete block. */
void*
cachemallocz(ulong bytes, int clear)
{
	USED(clear);
	return cachemalloc(bytes);
}


/* Release exactly one successful allocation, never a borrowed table. */
void
cachefree(void *pointer)
{
	if(pointer != nil){
		check(live > 0);
		live--;
		hostfree(pointer);
	}
}


/* This gate has no VM error injection; unexpected errors are failures. */
void
error(char *message)
{
	fprintf(stderr, "unexpected RV compilation error: %s\n", message);
	abort();
}


/* Valid compiler paths use actual functions; only the platform error boundary is direct. */
int
kprotect(int (*function)(void*), void *argument)
{
	return function(argument);
}


/* Record embedded publication; Linux page publication uses the actual leaf-code helpers. */
int
segflush(void *code, ulong bytes)
{
	check(code != nil && bytes != 0);
	flushes++;
	return failflush ? -1 : 0;
}


/* Supply a distinguishable callback when testing stale cache identity. */
static void
wrongenter(void)
{
	flushes++;
}


/* Keep allocator-callback corruption type-correct without calling it. */
static void
wrongfree(uchar *code, int bytes)
{
	USED(code);
	USED(bytes);
}


/* Rejection must not allocate, flush, publish or replace any module field. */
static void
reject(Module *module)
{
	Module before;
	int oldallocations, oldlive, oldflushes;

	memmove(&before, module, sizeof(before));
	oldallocations = allocations;
	oldlive = live;
	oldflushes = flushes;
	check(!compile(module, module->nprog, nil));
	check(memcmp(&before, module, sizeof(before)) == 0);
	check(allocations == oldallocations && live == oldlive && flushes == oldflushes);
}


/* Alter only owned code, restoring Linux RX permissions before each verification. */
static void
codeword(Jit *jit, u32int word)
{
#ifdef RV_CODE_PAGES
	check(mprotect(jit->code, jit->size, PROT_READ|PROT_WRITE) == 0);
#endif
	memmove(jit->code, &word, sizeof(word));
#ifdef RV_CODE_PAGES
	check(leafcodepublish(jit->code, jit->size) == 0);
#endif
}


/* Reject malformed metadata and valid-address aliases before reusing a native sidecar. */
int
main(void)
{
	Module module;
	Inst program[256], *original;
	Jit *jit, saved;
	char *control;
	u32int first;
	int op, count, native, before, oldallocations, oldflushes;

	memset(&module, 0, sizeof(module));
	memset(program, 0, sizeof(program));
	count = 0;
	for(op = INOP+1; op < MAXDIS; op++){
		if(op == IECLR)
			continue;
		program[count++].op = op;
	}
	check(count == 173);
	module.prog = program;
	module.nprog = count;
	module.origmp = H;
	control = getenv("RISCV_CACHE_CASE");
	if(control != nil && strcmp(control, "opcode") == 0){
		program[count-1].op = INOP;
		reject(&module);
		check(live == 0);
		puts("PASS RV mixed reserved-opcode control");
		return 0;
	}
	if(control != nil && strcmp(control, "binding") == 0){
		check(compile(&module, count, nil));
		jit = module.jit;
		check(jit->entry[IADDW] != nil && jit->entry[IMOVW] != nil);
		jit->entry[IADDW] = jit->entry[IMOVW];
		reject(&module);
		rvcodefree(jit);
		cachefree(jit);
		check(live == 0);
		puts("PASS RV canonical opcode-binding control");
		return 0;
	}
	check(!compile(nil, count, nil) && !compile(H, count, nil));
	check(!compile((Module*)((uchar*)&module+1), count, nil));
	check(!compile((Module*)((uintptr)-1-sizeof(void*)+1), count, nil));
	original = module.prog;
	module.prog = (Inst*)((uintptr)-1-sizeof(Inst));
	reject(&module);
	module.prog = original;
	for(op = 0; op < 256; op++){
		if(op > INOP && op < MAXDIS && op != IECLR)
			continue;
		program[count-1].op = op;
		reject(&module);
	}
	program[count-1].op = MAXDIS-1;
	failafter = 0;
	check(!compile(&module, count, nil) && module.jit == nil && live == 0);
	failafter = -1;
#ifndef RV_CODE_PAGES
	failafter = 1;
	check(!compile(&module, count, nil) && module.jit == nil && live == 0);
	failafter = -1;
	failflush = 1;
	check(!compile(&module, count, nil) && module.jit == nil && live == 0);
	failflush = 0;
#endif
	check(compile(&module, count, nil));
	jit = module.jit;
	check(jit != nil && jit->nprog == count && jit->size > 0);
	before = live;
	oldallocations = allocations;
	oldflushes = flushes;
	check(compile(&module, count, nil));
	check(live == before && allocations == oldallocations && flushes == oldflushes);
	memmove(&saved, jit, sizeof(saved));
	module.jit = H;
	reject(&module);
	module.jit = (Jit*)((uchar*)jit+1);
	reject(&module);
	module.jit = (Jit*)((uintptr)-1-sizeof(void*)+1);
	reject(&module);
	module.jit = jit;
	jit->nprog--;
	reject(&module);
	*jit = saved;
	jit->enter = wrongenter;
	reject(&module);
	*jit = saved;
#ifdef RV_CODE_PAGES
	jit->freecode = nil;
#else
	jit->freecode = wrongfree;
#endif
	reject(&module);
	*jit = saved;
	jit->code = nil;
	reject(&module);
	*jit = saved;
	jit->code = H;
	reject(&module);
	*jit = saved;
	jit->code++;
	reject(&module);
	*jit = saved;
	jit->code = (uchar*)((uintptr)-1-3);
	reject(&module);
	*jit = saved;
	jit->size = 0;
	reject(&module);
	*jit = saved;
	jit->size--;
	reject(&module);
	*jit = saved;
	jit->size -= sizeof(u32int);
	reject(&module);
	*jit = saved;
	memmove(&first, jit->code, sizeof(first));
	codeword(jit, first^1);
	reject(&module);
	codeword(jit, first);
	native = 0;
	for(op = 0; op < 256; op++){
		jit->dispatch[op] = saved.entry[IADDW];
		reject(&module);
		*jit = saved;
		if(saved.entry[op] == nil){
			jit->entry[op] = saved.entry[IADDW];
			reject(&module);
			*jit = saved;
			continue;
		}
		native++;
		jit->entry[op] = nil;
		reject(&module);
		*jit = saved;
		jit->entry[op] = (int(*)(REG*))((uchar*)saved.entry[op]+sizeof(u32int));
		reject(&module);
		*jit = saved;
		jit->entry[op] = saved.entry[op == IADDW ? IMOVW : IADDW];
		reject(&module);
		*jit = saved;
	}
	check(native > 0);
	for(op = 0; op < 256; op++){
		if(op > INOP && op < MAXDIS && op != IECLR)
			continue;
		program[count-1].op = op;
		reject(&module);
	}
	program[count-1].op = MAXDIS-1;
	check(compile(&module, count, nil));
	rvcodefree(jit);
	cachefree(jit);
	check(live == 0);
	printf("PASS RV64 cache: %d active predicates, %d native entries, canonical code/bindings and atomic refusal\n", checks, native);
	return 0;
}
