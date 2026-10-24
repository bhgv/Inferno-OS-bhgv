#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"

#ifndef RVXLEN
#define RVXLEN 32
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

void
typecom(Type *t)
{
	USED(t); /* Existing initmem/freeptrs remain authoritative. */
}

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
	Module *m;
	int size;
	Jit *j;
};

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

	c=arg; m=c->m; size=c->size;
	if(m == nil || m == H || m->compiled || size <= 0 || size != m->nprog ||
	   m->prog == nil || m->prog == H || size > 0x7fffffff/sizeof(Inst) ||
	   sizeof(u32int) != 4 || sizeof(void*)*8 != RVXLEN ||
	   sizeof(WORD)*8 > RVXLEN)
		return 0;
	if(m->jit != nil)
		return 1;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	return 0;
#endif
	abi.xlen=RVXLEN;
	abi.wordbytes=sizeof(WORD);
	abi.arraylen=O(Array, len); abi.arraydata=O(Array, data);
	abi.stringlen=O(String, len);
	abi.stringdata=O(String, Sascii);
	abi.listdata=O(List, data);
	abi.listtail=O(List, tail);
	abi.fp=O(REG, FP);
	abi.fixedtmp=(NREG+2)*IBY2WD;
#ifdef __riscv_mul
	abi.hasm=1;
#else
	abi.hasm=0;
#endif
	abi.pc=O(REG, PC); abi.s=O(REG, s); abi.d=O(REG, d); abi.m=O(REG, m);
	memset(count, 0, sizeof(count));
	memset(fn, 0, sizeof(fn));
	n=0;
	for(i=0; i<size; i++) {
		op=m->prog[i].op;
		if(count[op] == 0) {
			used=rvemit(nil, RvMax, &abi, op);
			count[op]=used == 0 ? -1 : used;
			if(used > 0) {
				/* Keep both the instruction count and allocation size representable. */
				if(used > 0x7fffffff/(int)sizeof(u32int)-n)
					return 0;
				n+=used;
			}
		}
	}
	if(n == 0)
		return 0;
	c->j=j=mallocz(sizeof(Jit), 1);
	if(j == nil)
		return 0;
	j->size=n*sizeof(u32int);
#ifdef RV_CODE_PAGES
	j->freecode=leafcodefree;
	j->code=leafcodealloc(j->size);
#else
	j->code=malloc(j->size);
#endif
	if(j->code == nil)
		goto bad;
	j->enter=rventer;
	j->nprog=size;
	code=(u32int*)j->code;
	for(op=0; op<256; op++) {
		if(count[op] <= 0)
			continue;
		fn[op]=(int(*)(REG*))code;
		used=rvemit(code, count[op], &abi, op);
		if(used != count[op])
			goto bad;
		code+=used;
	}
	for(op=0; op<256; op++)
		j->entry[op]=fn[op];
#ifdef RV_CODE_PAGES
	if(leafcodepublish(j->code, j->size) != 0)
#else
	if(segflush(j->code, j->size) != 0)
#endif
		goto bad;
	/* Publish only fully generated, synchronized code. Failure is atomic. */
	m->jit=j;
	return 1;
bad:
	/* A failed sizing, emission or instruction-cache flush must not leak code. */
	if(j != nil) {
		rvcodefree(j);
		free(j);
		c->j=nil;
	}
	return 0;
}

int
compile(Module *m, int size, Modlink *ml)
{
	RvCompile c;
	int ok;
	USED(ml);
	c.m=m; c.size=size; c.j=nil;
	ok=kprotect(rvcompile,&c);
	if(!ok && c.j != nil) {
		rvcodefree(c.j);
		free(c.j);
	}
	return ok;
}
