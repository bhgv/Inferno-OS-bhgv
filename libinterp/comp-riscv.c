#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"

#ifndef RVXLEN
#define RVXLEN 32
#endif
#include "riscv-emit.h"

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

int
compile(Module *m, int size, Modlink *ml)
{
	RvLayout abi;
	Jit *j;
	u32int *code;
	int (*fn[256])(REG*);
	int count[256], i, n, op, used;

	USED(ml);
	if(m == nil || m->compiled || size <= 0 || size != m->nprog ||
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
			if(used > 0)
				n+=used;
		}
	}
	if(n == 0)
		return 0;
	j=mallocz(sizeof(Jit), 1);
	if(j == nil)
		return 0;
	j->code=malloc(n*sizeof(u32int));
	if(j->code == nil) {
		free(j);
		return 0;
	}
	j->enter=rventer;
	j->nprog=size;
	j->size=n*sizeof(u32int);
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
	if(segflush(j->code, j->size) != 0)
		goto bad;
	/* Publish only fully generated, synchronized code. Failure is atomic. */
	m->jit=j;
	return 1;
bad:
	free(j->code);
	free(j);
	return 0;
}
