#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"

#include LEAF_EMITTER
#include "leaf-code.h"


/* Leaf JIT keeps original Inst PCs and uses the existing decoder/optab for
 * unsupported operations. No patching of frames, GC maps or case tables.
 * compiled remains 0: its historical meaning is a relocated native-PC VM.
 */
void (*comvec)(void);


/* Leaf execution retains the existing native pointer maps and C heap helpers. */
void
typecom(Type *t)
{
	USED(t); /* Existing initmem/freeptrs remain authoritative. */
}


/* Enter only code whose cache and executable permissions were published together. */
static void
leafenter(void)
{
	/* Cache synchronization is performed by the target segflush implementation. */
}

typedef struct LeafCompile LeafCompile;

struct LeafCompile {
	Module *m; // Module whose original Dis program remains authoritative.
	int size; // Instruction count validated before staging native entries.
	Jit *j; // Partial code-table owner retained across protected compilation.
};


/* Reject invalid module storage before sizing or publishing native leaf code. */
static int
leafcompile(void *arg)
{
	LeafCompile *c;
	Module *m;
	int size;
	LeafLayout abi;
	Jit *j;
	u32int *code;
	int (*fn[256])(REG*);
	int count[256], i, n, op, used;

	c=arg; m=c->m; size=c->size;
	if(m == nil || m == H || m->compiled || size <= 0 || size != m->nprog ||
	   m->prog == nil || m->prog == H || size > 0x7fffffff/sizeof(Inst) ||
	   sizeof(u32int) != 4 || sizeof(void*) != 8 ||
	   (sizeof(WORD) != 4 && sizeof(WORD) != 8))
		return 0;
	if(m->jit != nil)
		return 1;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	return 0;
#endif
	abi.wordbytes=sizeof(WORD);
	abi.arraylen=O(Array, len); abi.arraydata=O(Array, data);
	abi.stringlen=O(String, len); abi.stringmax=O(String, max);
	abi.stringdata=O(String, Sascii);
	abi.listdata=O(List, data);
	abi.listtail=O(List, tail);
	abi.fp=O(REG, FP);
	abi.fixedtmp=(NREG+2)*IBY2WD;
	abi.fixedcoef=NREG*IBY2WD;
	abi.module=O(REG, M); abi.linkmodule=O(Modlink, m);
	abi.linkcompiled=O(Modlink, compiled);
	abi.moduleprog=O(Module, prog); abi.modulenprog=O(Module, nprog);
	abi.instbytes=sizeof(Inst);
	abi.heaptype=sizeof(Heap)-O(Heap, t);
	abi.stringtype=&Tstring;
	abi.arrayidentity=&Tarray;
	abi.heapref=sizeof(Heap)-O(Heap, ref);
	abi.heapcolor=sizeof(Heap)-O(Heap, color);
	abi.refbytes=sizeof(((Heap*)0)->ref);
	abi.mutator=&mutator;
	abi.nprop=&nprop; abi.markcolor=propagator;
	abi.moduletypes=O(Module, type); abi.modulenty=O(Module, ntype);
	abi.typenp=O(Type, np);
	abi.linknlinks=O(Modlink, nlinks); abi.linklinks=O(Modlink, links);
	abi.modlbytes=sizeof(Modl); abi.modlframe=O(Modl, frame);
	abi.regmp=O(REG, MP); abi.regtemp=O(REG, t);
	abi.linkprog=O(Modlink, prog); abi.linkmp=O(Modlink, MP); abi.modlpc=O(Modl, u);
	abi.sp=O(REG, SP); abi.ts=O(REG, TS);
	abi.frametype=O(Frame, t); abi.framemr=O(Frame, mr); abi.framesize=sizeof(Frame);
	abi.framelr=O(Frame, lr); abi.framefp=O(Frame, fp);
	abi.arraytype=O(Array, t); abi.typesize=O(Type, size);
	abi.arrayroot=O(Array, root); abi.typeref=O(Type, ref);
	abi.pc=O(REG, PC); abi.s=O(REG, s); abi.d=O(REG, d); abi.m=O(REG, m);
	memset(count, 0, sizeof(count));
	memset(fn, 0, sizeof(fn));
	n=0;
	for(i=0; i<size; i++) {
		op=m->prog[i].op;
		if(count[op] == 0) {
			used=leafemit(nil, LeafMax, &abi, op);
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
	j->freecode=leafcodefree;
	j->code=leafcodealloc(j->size);
	if(j->code == nil)
		goto bad;
	j->enter=leafenter;
	j->nprog=size;
	code=(u32int*)j->code;
	for(op=0; op<256; op++) {
		if(count[op] <= 0)
			continue;
		fn[op]=(int(*)(REG*))code;
		used=leafemit(code, count[op], &abi, op);
		if(used != count[op])
			goto bad;
		code+=used;
	}
	for(op=0; op<256; op++)
		j->entry[op]=fn[op];
	if(leafcodepublish(j->code, j->size) != 0)
		goto bad;
	/* Publish only fully generated, synchronized code. Failure is atomic. */
	m->jit=j;
	return 1;
bad:
	/* A failed sizing, emission or instruction-cache flush must not leak code. */
	if(j != nil) {
		leafcodefree(j->code, j->size);
		free(j);
		c->j=nil;
	}
	return 0;
}


/* Own temporary code across VM errors and publish only a completed leaf table. */
int
compile(Module *m, int size, Modlink *ml)
{
	LeafCompile c;
	int ok;
	USED(ml);
	c.m=m; c.size=size; c.j=nil;
	ok=kprotect(leafcompile,&c);
	if(!ok && c.j != nil) {
		leafcodefree(c.j->code, c.j->size);
		free(c.j);
	}
	return ok;
}
