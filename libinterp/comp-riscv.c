#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"

#ifndef RVXLEN
#define RVXLEN 32 // Default register width for the original embedded backend.
#endif
#include "riscv-emit.h"
#ifdef RV_CODE_PAGES
#include "leaf-code.h"
#endif

/* Leaf JIT keeps original Inst PCs and uses the existing decoder/optab for
 * unsupported operations. No patching of frames, GC maps or case tables.
 * compiled remains 0: its historical meaning is a relocated native-PC VM.
 */
void (*comvec)(void);


/* Preserve the ordinary C descriptor initialization and destruction paths. */
void
typecom(Type *t)
{
	USED(t); /* Existing initmem/freeptrs remain authoritative. */
}


/* Synchronize instruction fetch when a task resumes on another hart. */
static void
rventer(void)
{
#ifdef __riscv
	/* A task can resume on a different hart after publication. A local
	 * publication fence alone is insufficient. Start conservatively;
	 * a per-hart code-generation epoch can amortize this later. */
	__asm__ volatile(".word 0x0000100f" ::: "memory"); /* fence.i */
#endif
}


/* Match executable mapping ownership while preserving existing embedded allocators. */
static void
rvcodefree(Jit *j)
{
	if(j->freecode != nil)
		j->freecode(j->code, j->size);
	else
		free(j->code);
}

typedef struct RvCompile RvCompile;

struct RvCompile {
	Module *m; // Original module, whose Dis instructions remain authoritative.
	int size; // Validated instruction count for this compilation request.
	Jit *j; // Partial allocation retained across protected VM errors.
};


/* Accept only canonical opcode entries and exact code for this target's features.
 * An entry inside the mapping can still name a different operation. */
static int
rvcachevalid(Jit *j, Module *m, int size, RvLayout *abi)
{
	u32int expected[RvMax];
	uintptr cursor, last, bytes;
	int op, n, i;

	if(j->enter != rventer)
		return 0;
#ifdef RV_CODE_PAGES
	if(j->freecode != leafcodefree)
#else
	if(j->freecode != nil)
#endif
		return 0;
	cursor = (uintptr)j->code;
	last = cursor+(uintptr)j->size;
	for(op = 0; op < 256; op++){
		if(j->dispatch[op] != nil)
			return 0;
		if(j->entry[op] == nil)
			continue;
		if((uintptr)j->entry[op] != cursor)
			return 0;
		n = rvemit(expected, RvMax, abi, op);
		if(n <= 0)
			return 0;
		bytes = (uintptr)n*sizeof(u32int);
		if(bytes > last-cursor || memcmp((void*)cursor, expected, bytes) != 0)
			return 0;
		cursor += bytes;
	}
	if(cursor != last)
		return 0;
	for(i = 0; i < size; i++){
		op = ((uchar*)m->prog)[(uintptr)i*sizeof(Inst)+O(Inst, op)];
		if(j->entry[op] == nil && rvemit(nil, RvMax, abi, op) > 0)
			return 0;
	}
	return 1;
}


/* Reject invalid module storage before sizing or publishing native leaf code. */
static int
rvcompile(void *arg)
{
	RvCompile *c;
	Module *m;
	int size;
	RvLayout abi;
	Jit *j;
	u32int *code;
	int (*fn[256])(REG*);
	int count[256], i, n, op, used;

	c = arg;
	m = c->m;
	size = c->size;
	if(m == nil || m == H || (uintptr)m > (uintptr)-1-sizeof(*m) ||
	   (uintptr)m%sizeof(void*) != 0)
		return 0;
	if(m->compiled || size <= 0 || size != m->nprog ||
	   m->prog == nil || m->prog == H || size > 0x7fffffff/sizeof(Inst) ||
	   sizeof(u32int) != 4 || sizeof(void*)*8 != RVXLEN ||
	   sizeof(WORD)*8 > RVXLEN)
		return 0;
	if((uintptr)m->prog > (uintptr)-1-(uintptr)size*sizeof(Inst))
		return 0;
	for(i = 0; i < size; i++){
		op = ((uchar*)m->prog)[(uintptr)i*sizeof(Inst)+O(Inst, op)];
		if(op <= INOP || op >= MAXDIS || op == IECLR)
			return 0;
	}
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	return 0;
#endif
	abi.xlen = RVXLEN;
	abi.wordbytes = sizeof(WORD);
	abi.arraylen = O(Array, len);
	abi.arraydata = O(Array, data);
	abi.stringlen = O(String, len);
	abi.stringdata = O(String, Sascii);
	abi.listdata = O(List, data);
	abi.listtail = O(List, tail);
	abi.fp = O(REG, FP);
	abi.fixedtmp = (NREG+2)*IBY2WD;
	abi.fixedcoef = NREG*IBY2WD;
	abi.module = O(REG, M);
	abi.linkmodule = O(Modlink, m);
	abi.moduleprog = O(Module, prog);
	abi.modulenprog = O(Module, nprog);
	abi.instbytes = sizeof(Inst);
	abi.heaptype = sizeof(Heap)-O(Heap, t);
	abi.framelr = O(Frame, lr);
	abi.framefp = O(Frame, fp);
	abi.arraytype = O(Array, t);
	abi.typesize = O(Type, size);
#ifdef __riscv_mul
	abi.hasm = 1;
#else
	abi.hasm = 0;
#endif
#ifdef __riscv_div
	abi.hasdiv = 1;
#else
	abi.hasdiv = 0;
#endif
	abi.pc = O(REG, PC);
	abi.s = O(REG, s);
	abi.d = O(REG, d);
	abi.m = O(REG, m);
	if(m->jit != nil){
		j = m->jit;
		if(j == H || (uintptr)j > (uintptr)-1-sizeof(*j) ||
		   (uintptr)j%sizeof(void*) != 0 || j->nprog != size ||
		   j->code == nil || j->code == H || j->size <= 0 ||
		   j->size%sizeof(u32int) != 0 || (uintptr)j->code%sizeof(u32int) != 0 ||
		   (uintptr)j->code > (uintptr)-1-(uintptr)j->size)
			return 0;
		return rvcachevalid(j, m, size, &abi);
	}
	memset(count, 0, sizeof(count));
	memset(fn, 0, sizeof(fn));
	n = 0;
	for(i = 0; i < size; i++){
		op = ((uchar*)m->prog)[(uintptr)i*sizeof(Inst)+O(Inst, op)];
		if(count[op] == 0) {
			used = rvemit(nil, RvMax, &abi, op);
			if(used < 0)
				return 0;
			count[op] = used == 0 ? -1 : used;
			if(used > 0) {
				/* Keep both the instruction count and allocation size representable. */
				if(used > 0x7fffffff/(int)sizeof(u32int)-n)
					return 0;
				n += used;
			}
		}
	}
	if(n == 0)
		return 0;
	c->j = j = mallocz(sizeof(Jit), 1);
	if(j == nil)
		return 0;
	j->size = n*sizeof(u32int);
#ifdef RV_CODE_PAGES
	j->freecode = leafcodefree;
	j->code = leafcodealloc(j->size);
#else
	j->code = malloc(j->size);
#endif
	if(j->code == nil)
		goto bad;
	j->enter = rventer;
	j->nprog = size;
	code = (u32int*)j->code;
	for(op = 0; op < 256; op++){
		if(count[op] <= 0)
			continue;
		fn[op] = (int(*)(REG*))code;
		used = rvemit(code, count[op], &abi, op);
		if(used != count[op])
			goto bad;
		code += used;
	}
	for(op = 0; op < 256; op++)
		j->entry[op] = fn[op];
#ifdef RV_CODE_PAGES
	if(leafcodepublish(j->code, j->size) != 0)
#else
	if(segflush(j->code, j->size) != 0)
#endif
		goto bad;
	/* Publish only fully generated, synchronized code. Failure is atomic. */
	m->jit = j;
	return 1;
bad:
	/* A failed sizing, emission or instruction-cache flush must not leak code. */
	if(j != nil) {
		rvcodefree(j);
		free(j);
		c->j = nil;
	}
	return 0;
}


/* Own partial native code until protected compilation publishes the complete table. */
int
compile(Module *m, int size, Modlink *ml)
{
	RvCompile c;
	int ok;
	USED(ml);
	c.m = m;
	c.size = size;
	c.j = nil;
	ok = kprotect(rvcompile, &c);
	if(!ok && c.j != nil) {
		rvcodefree(c.j);
		free(c.j);
	}
	return ok;
}
