#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"

#include LEAF_EMITTER
#include "leaf-code.h"


/* Leaf JIT keeps original Inst PCs and uses the existing decoder/optab for
 * complete decoded dispatch with guarded fast paths and C handlers. No patching of frames, GC maps or case tables.
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


/* Reuse only complete cached tables whose entries fit the owned native mapping. */
static int
leafcachevalid(Jit *j, Module *m, int size, LeafLayout *abi)
{
	LeafOp operation;
	uintptr first, last, target;
	int op, n, i;

	first = (uintptr)j->code;
	last = first+(uintptr)j->size;
	for(op = 0; op < 256; op++){
		if(j->entry[op] != nil){
			target = (uintptr)j->entry[op];
			n = leafemit(nil, LeafMax, abi, op);
			if(n <= 0 || target < first || target >= last || target%sizeof(u32int) != 0 ||
			   (uintptr)n*sizeof(u32int) > last-target || !leafop(&operation, op, sizeof(WORD)))
				return 0;
		}
		if(j->dispatch[op] == nil)
			continue;
		target = (uintptr)j->dispatch[op];
		n = leafbridgeemit(nil, LeafMax, (void*)cjitexec, op, (void*)j->entry[op]);
		if(n <= 0 || target < first || target >= last || target%sizeof(u32int) != 0 ||
		   (uintptr)n*sizeof(u32int) > last-target)
			return 0;
	}
	for(i = 0; i < size; i++){
		op = ((uchar*)m->prog)[(uintptr)i*sizeof(Inst)+O(Inst, op)];
		if(j->dispatch[op] == nil)
			return 0;
	}
	return 1;
}


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
	int count[256], bridge[256], i, n, op, used, complete;

	c=arg; m=c->m; size=c->size;
	/* Public Module/header failures must reject before the first typed read. */
	if(m == nil || m == H || (uintptr)m > (uintptr)-1-sizeof(*m) ||
	   (uintptr)m%sizeof(void*) != 0)
		return 0;
	if(m->compiled || size <= 0 || size != m->nprog ||
	   m->prog == nil || m->prog == H || size > 0x7fffffff/sizeof(Inst) ||
	   sizeof(u32int) != 4 || sizeof(void*) != 8 ||
	   (sizeof(WORD) != 4 && sizeof(WORD) != 8))
		return 0;
	if((uintptr)m->prog > (uintptr)-1-(uintptr)size*sizeof(Inst))
		return 0;
	/* Check every byte opcode, including cached and mixed active/reserved programs. */
	for(i = 0; i < size; i++){
		op = ((uchar*)m->prog)[(uintptr)i*sizeof(Inst)+O(Inst, op)];
		if(op <= INOP || op >= MAXDIS || op == IECLR)
			return 0;
	}

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	return 0;
#endif
	abi.wordbytes=sizeof(WORD);
	abi.arraylen=O(Array, len); abi.arraydata=O(Array, data);
	abi.stringlen=O(String, len); abi.stringmax=O(String, max);
	abi.stringdata=O(String, Sascii);
	abi.listdata=O(List, data);
	abi.consentry=(void*)cons;
	abi.constypeentry=(void*)cconsmappedtype;
	abi.channelentry=(void*)cnewcscalar;
	abi.channeltransferentry=(void*)cchanleaf;
	abi.rawchannelentry=(void*)cnewcraw;
	abi.mappedchannelentry=(void*)cnewcmapped;
	abi.channelmid=O(Channel, mid);
	abi.listtype=O(List, t); abi.ptrtype=&Tptr;
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
	abi.typefree=O(Type, free); abi.typemark=O(Type, mark);
	abi.plainfree=(void*)freeheap; abi.plainmark=(void*)markheap;
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
	/* Cache span sizing uses exactly the same complete native layout as emission. */
	if(m->jit != nil){
		j=m->jit;
		if(j == H || (uintptr)j > (uintptr)-1-sizeof(*j) ||
		   (uintptr)j%sizeof(void*) != 0 || j->nprog != size ||
		   j->code == nil || j->code == H || j->size <= 0 || j->size%sizeof(u32int) != 0 ||
		   (uintptr)j->code%sizeof(u32int) != 0 ||
		   (uintptr)j->code > (uintptr)-1-(uintptr)j->size)
			return 0;
		return leafcachevalid(j, m, size, &abi);
	}
	memset(count, 0, sizeof(count));
	memset(bridge, 0, sizeof(bridge));
	memset(fn, 0, sizeof(fn));
	n=0;
	for(i=0; i<size; i++) {
		op=((uchar*)m->prog)[(uintptr)i*sizeof(Inst)+O(Inst, op)];
		if(count[op] == 0) {
			used=leafemit(nil, LeafMax, &abi, op);
			count[op]=used == 0 ? -1 : used;
			complete=leafbridgeemit(nil, LeafMax, (void*)cjitexec, op, nil);
			bridge[op]=complete;
			if(op > INOP && op < MAXDIS && op != IECLR && complete == 0)
				return 0;
			/* Both fixed bridge size and fast code must fit the same owned mapping. */
			if(used < 0 || complete < 0 || used > 0x7fffffff/(int)sizeof(u32int)-n)
				return 0;
			n+=used;
			if(complete > 0x7fffffff/(int)sizeof(u32int)-n)
				return 0;
			n+=complete;
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
	for(op=0; op<256; op++){
		j->entry[op]=fn[op];
		if(bridge[op] <= 0)
			continue;
		j->dispatch[op]=(int(*)(REG*))code;
		used=leafbridgeemit(code, bridge[op], (void*)cjitexec, op, (void*)fn[op]);
		if(used != bridge[op])
			goto bad;
		code+=used;
	}
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
