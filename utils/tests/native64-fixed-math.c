#include "native64-math-call.h"
#include "fixed-math-golden.h"

static Native64MathBitsCall request; // Mutable original request used to test the owned snapshot.
static Module *fixedmodule; // Registry owner observed during the protected call.
static Type *fixedtype; // Exact registered scalar frame descriptor.
static int modulerefs, typerefs, fixedlive, mode, resolutions, mutate; // Independent lifetime/failure controls.
static uchar cell[24], movedcell[24]; // Caller-owned scalar cells with surrounding canaries.
static Prog *mappedprog; // Optional live caller whose current frame changes during producer allocation.
static uchar *mappedtarget; // A distinct already rooted frame or an invalid interior boundary.
static int offset; // Exercise every unaligned scalar result position.


/* Inject the existing allocation refusal and optionally invalidate the original request after preflight. */
void*
native64testalloc(ulong bytes, int clear)
{
	void *memory;

	memory = zeroallocate(bytes, clear);
	if(mappedprog != nil)
		mappedprog->R.FP = mappedtarget;
	if(mutate){
		request.input = 0;
		request.resolve = nil;
		golden_imports[4] = 0xff;
	}
	return memory;
}


/* Observe every raw temporary frame release without substituting descriptor/module cleanup. */
void
native64testfree(void *pointer)
{
	if(pointer != nil)
		releaseowned(pointer);
}


/* Relocate or refuse publication while checking retained module/type/frame ownership. */
static void*
fixedresolve(void *argument)
{
	check(argument == &request);
	check(fixedmodule->ref == modulerefs+1 && fixedtype->ref == typerefs+1);
	check(live == fixedlive+1);
	resolutions++;
	if(mode == 1)
		return nil;
	if(mode == 2)
		error("fixed scalar resolver failed");
	if(mode == 3){
		unload(fixedmodule);
		check(fixedmodule->ref == modulerefs);
	}
	if(mode == 4)
		return H;
	if(mode == 5)
		return (void*)((uintptr)-1-3);
	return mode == 6 ? movedcell+offset : cell+offset;
}


/* Inject an error after the actual owning callee has produced its private scalar. */
void
native64testcallee(void)
{
	if(mode == 7){
		check(fixedmodule->ref == modulerefs+1 && fixedtype->ref == typerefs+1);
		error("fixed scalar callee failed");
	}
}


/* Reset the complete immutable input, and preserve the independent result bit oracle. */
static void
fixedrequest(uvlong bits)
{
	memset(cell, 0xa5, sizeof(cell));
	memset(movedcell, 0xa5, sizeof(movedcell));
	request.strings = golden_strings;
	request.string_bytes = sizeof(golden_strings);
	request.imports = golden_imports;
	request.import_bytes = sizeof(golden_imports);
	request.ordinal = 0;
	request.input = bits;
	request.resolve = fixedresolve;
	request.argument = &request;
	resolutions = 0;
	fixedlive = live;
	modulerefs = fixedmodule->ref;
	typerefs = fixedtype->ref;
}


/* Check scalar result bytes and all guard bytes without comparing floating NaNs arithmetically. */
static void
fixedcells(uvlong bits, int success)
{
	uchar expected[24], untouched[24];

	memset(expected, 0xa5, sizeof(expected));
	memset(untouched, 0xa5, sizeof(untouched));
	if(success)
		memmove(expected+offset, &bits, sizeof(bits));
	check(memcmp(mode == 6 ? movedcell : cell, expected, sizeof(expected)) == 0);
	check(memcmp(mode == 6 ? cell : movedcell, untouched, sizeof(untouched)) == 0);
	check(live == fixedlive);
}


/* Prove the adapter selects the current typed frame rather than a stale physical cell. */
static void
fixedframes(void)
{
	union {
		void *alignment; // Natural Frame64 alignment.
		uchar bytes[OA(Stkext, reg.tos.fu)+176]; // Two complete generated-shape caller frames.
	} stack;
	Prog prog;
	Type *type;
	Stkext *extent;
	Frame *first, *second;
	Native64MathFrameCall call;
	uchar map[] = {0, 0x20}; // Scalar result at72; unrelated managed field at80.
	uvlong bits, before, observed;
	int phase, status, allocations_before;

	mode = 0;
	type = dtype(freeheap, 88, map, sizeof(map));
	check(type != nil);
	for(phase=0; phase<5; phase++){
		memset(&stack, 0, sizeof(stack));
		memset(&prog, 0, sizeof(prog));
		extent = (Stkext*)stack.bytes;
		extent->reg.TR = type;
		first = (Frame*)extent->reg.tos.fu;
		second = (Frame*)((uchar*)first+88);
		first->t = second->t = type;
		prog.R.EX = stack.bytes;
		prog.R.FP = (uchar*)first;
		prog.R.SP = prog.R.TS = stack.bytes+sizeof(stack.bytes);
		bits = 0x3ff0000000000000ULL;
		before = 0x1122334455667788ULL;
		memmove((uchar*)first+72, &before, 8);
		memmove((uchar*)second+72, &before, 8);
		fixedrequest(bits);
		call.call = request; call.caller = &prog; call.type = type; call.offset = 72;
		mappedprog = phase ? &prog : nil;
		mappedtarget = phase == 1 ? (uchar*)second : phase == 2 ? (uchar*)second+8 : nil;
		if(phase == 3) call.offset = 80;
		if(phase == 4) call.type = fixedtype;
		allocations_before = attempts;
		status = Math_bits64realframe64(&call);
		mappedprog = nil;
		check(status == (phase < 2) && live == fixedlive && resolutions == 0);
		memmove(&observed, (uchar*)first+72, 8);
		check(observed == (phase == 0 ? bits : before));
		memmove(&observed, (uchar*)second+72, 8);
		check(observed == (phase == 1 ? bits : before));
		if(phase >= 3) check(attempts == allocations_before);
	}
	freetype(type);
	puts("PASS typed VM frame adapter: active/relocated cells, stale boundary, managed-slot and Type refusals");
}


/* Execute the proposed shared fixture, malformed records and post-call error/retirement paths. */
int
main(void)
{
	uvlong bits, seed;
	int i, j, status, before, entryindex, old, total = 0;
	void (*oldfunction)(void*);
	char *oldpath;
	uchar saved;
	uchar aliases[2*sizeof(golden_imports)-4]; // Two equal import records preserve their ordinals.
	static const uvlong edges[] = {0, 1, 0x8000000000000000ULL, 0x3ff0000000000000ULL,
		0x7ff0000000000000ULL, 0xfff0000000000000ULL, 0x7ff8000000000123ULL, 0x7ff0000000000001ULL}; // Independent binary64 patterns.

	check(sizeof(F_Math_bits64real) == 72 && offsetof(F_Math_bits64real, ret) == 32 &&
	   offsetof(F_Math_bits64real, b) == 64 && offsetof(Frame, t) == 24);
	check(native64_full_math_main() == 0);
	check(kprotect(startmath, nil));
	fixedmodule = modules;
	for(i=0; i<fixedmodule->ntype; i++)
		if(strcmp(fixedmodule->ext[i].name, "bits64real") == 0)
			break;
	check(i < fixedmodule->ntype);
	entryindex = i;
	fixedtype = fixedmodule->ext[i].frame;
	seed = 0x123456789abcdef0ULL;
	for(j=0; j<264; j++){
		seed ^= seed<<13; seed ^= seed>>7; seed ^= seed<<17;
		bits = j<8 ? edges[j] : seed;
		for(mode=0; mode<7; mode++){
			if(mode == 3)
				continue;
			for(offset=0; offset<8; offset++){
				fixedrequest(bits);
				status = Math_bits64real64(&request);
				check(status == (mode == 0 || mode == 6));
				check(resolutions == 1 && worker.nerr == 0);
				check(fixedmodule->ref == modulerefs && fixedtype->ref == typerefs);
				fixedcells(bits, status);
				total++;
			}
		}
	}
	mode = offset = 0;
	/* Mutate every structural byte separately; no malformed import may allocate. */
	for(i=0; i<sizeof(golden_imports); i++){
		fixedrequest(0x3ff0000000000000ULL);
		saved = golden_imports[i]; golden_imports[i] ^= 0x80;
		before = attempts;
		check(!Math_bits64real64(&request) && attempts == before && resolutions == 0);
		golden_imports[i] = saved;
		fixedcells(0, 0);
	}
	for(i=0; i<sizeof(golden_imports); i++){
		fixedrequest(0); request.import_bytes = i;
		before = attempts;
		check(!Math_bits64real64(&request) && attempts == before);
		fixedcells(0, 0);
	}
	/* Structurally valid wrong types must not pass even with the same registered legacy hash. */
	for(i=0; i<2; i++){
		fixedrequest(0);
		j = i == 0 ? 23 : 31;
		saved = golden_imports[j]; golden_imports[j] = i == 0 ? 2 : 3;
		before = attempts;
		check(!Math_bits64real64(&request) && attempts == before && resolutions == 0);
		golden_imports[j] = saved;
	}
	for(i=0; i<sizeof(golden_strings); i++){
		fixedrequest(0);
		saved = golden_strings[i]; golden_strings[i] ^= 0x80;
		before = attempts;
		check(!Math_bits64real64(&request) && attempts == before);
		golden_strings[i] = saved;
		fixedcells(0, 0);
	}
	for(i=0; i<sizeof(golden_strings); i++){
		fixedrequest(0); request.string_bytes = i;
		before = attempts;
		check(!Math_bits64real64(&request) && attempts == before);
	}
	memmove(aliases, golden_imports, sizeof(golden_imports));
	aliases[3] = 2;
	memmove(aliases+sizeof(golden_imports), golden_imports+4, sizeof(golden_imports)-4);
	fixedrequest(0x3ff0000000000000ULL);
	request.imports = aliases; request.import_bytes = sizeof(aliases); request.ordinal = 1;
	check(Math_bits64real64(&request));
	fixedcells(0x3ff0000000000000ULL, 1);
	fixedrequest(0); request.ordinal = 1;
	before = attempts;
	check(!Math_bits64real64(&request) && attempts == before);
	/* Registry metadata must still match the exact trusted implementation and generated frame. */
	for(i=0; i<8; i++){
		fixedrequest(0); before = attempts;
		old = 0; oldfunction = fixedmodule->ext[entryindex].u.runt; oldpath = fixedmodule->path;
		switch(i){
		case 0: old = fixedmodule->ext[entryindex].sig; fixedmodule->ext[entryindex].sig ^= 1; break;
		case 1: fixedmodule->ext[entryindex].u.runt = nil; break;
		case 2: old = fixedtype->size; fixedtype->size += 8; break;
		case 3: old = fixedtype->np; fixedtype->np = 0; break;
		case 4: fixedtype->map[0] = 1; break;
		case 5: old = fixedtype->ref; fixedtype->ref = 0x7fffffff; break;
		case 6: old = fixedmodule->ref; fixedmodule->ref = 0x7fffffff; break;
		case 7: fixedmodule->path = "$Other"; break;
		}
		check(!Math_bits64real64(&request) && attempts == before && resolutions == 0);
		switch(i){
		case 0: fixedmodule->ext[entryindex].sig = old; break;
		case 1: fixedmodule->ext[entryindex].u.runt = oldfunction; break;
		case 2: fixedtype->size = old; break;
		case 3: fixedtype->np = old; break;
		case 4: fixedtype->map[0] = 0; break;
		case 5: fixedtype->ref = old; break;
		case 6: fixedmodule->ref = old; break;
		case 7: fixedmodule->path = oldpath; break;
		}
		fixedcells(0, 0);
	}
	fixedrequest(0); fail_after = 0;
	check(!Math_bits64real64(&request) && resolutions == 0);
	fail_after = -1;
	fixedcells(0, 0);
	check(fixedmodule->ref == modulerefs && fixedtype->ref == typerefs);
	fixedrequest(0x3ff0000000000000ULL); mutate = 1;
	check(Math_bits64real64(&request));
	mutate = 0; golden_imports[4] = 0;
	fixedcells(0x3ff0000000000000ULL, 1);
#ifdef FIXED_CALLEE_INJECTED
	fixedrequest(0x3ff0000000000000ULL); mode = 7;
	check(!Math_bits64real64(&request) && resolutions == 0);
	check(strcmp(diagnostic, "fixed scalar callee failed") == 0);
	check(fixedmodule->ref == modulerefs && fixedtype->ref == typerefs && worker.nerr == 0);
	fixedcells(0, 0);
#endif
	fixedframes();
	fixedrequest(0x3ff0000000000000ULL); mode = 3;
	check(Math_bits64real64(&request));
	check(modules == &baseline && live == 0 && worker.nerr == 0);
	puts("PASS fixed scalar import: 12672 calls, malformed/truncated signatures, unaligned/late results, allocation/unwind, snapshot and module retirement");
	check(total == 12672);
	return 0;
}
