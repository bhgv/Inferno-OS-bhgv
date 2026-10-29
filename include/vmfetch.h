#ifndef INFERNO_VMFETCH_H
#define INFERNO_VMFETCH_H // Shared checked bytecode fetch for normal and instrumented VM loops.
#include "raise.h"

#if defined(__GNUC__)
#define VmInline static __inline__ // Share checks without forcing calls in the instruction loop.
#else
#define VmInline static // Preserve compilers without GNU inline support.
#endif

typedef struct VmFetch VmFetch;

/* Stable source identity retained while decoding and handlers publish REG fields. */
struct VmFetch
{
	Module *module; // Current bytecode module; nil denotes native-PC mode.
	Inst *pc; // Original Dis instruction address, including packed images.
	int index; // Relative instruction index within the complete program span.
	int op, add; // Captured opcode and addressing byte before decoder writes.
};


/* Validate scheduler storage before any owner or debugger field is read. */
VmInline void
vmprocess(Prog *p)
{
	if(p == nil || p == H || (uintptr)p%sizeof(void*) != 0 ||
	   (uintptr)p > (uintptr)-1-sizeof(*p))
		error(exHeap);
}


/* Restore a valid process and consume pending kills before touching VM owners. */
VmInline void
vmstart(Prog *p)
{
	Modlink *link;

	vmprocess(p);
	if(p->quanta <= 0)
		error(exInval);
	R = p->R;
	R.IC = p->quanta;
	if(p->kill != nil){
		char *message;

		message = p->kill;
		p->kill = nil;
		error(message);
	}
	link = R.M;
	if(link == nil || link == H ||
	   (uintptr)link > (uintptr)-1-(O(Modlink, compiled)+sizeof(link->compiled)))
		error(exModule);
	memmove(&R.MP, (uchar*)link+O(Modlink, MP), sizeof(R.MP));
}


/* Locate the next Dis instruction without reading it; native/outside PCs return -1.
 * Diagnostic callers can inspect a yielded one-past PC without fetching bytes. */
VmInline int
vmlocation(Module **owner)
{
	Modlink *link;
	Module *module;
	Inst *program;
	uintptr pc, base, bytes;
	int compiled, count;

	link = R.M;
	if(link == nil || link == H ||
	   (uintptr)link > (uintptr)-1-(O(Modlink, compiled)+sizeof(compiled)))
		error(exModule);
	memmove(&compiled, (uchar*)link+O(Modlink, compiled), sizeof(compiled));
	if(compiled){
		*owner = nil;
		return -1;
	}
	memmove(&module, (uchar*)link+O(Modlink, m), sizeof(module));
	if(module == nil || module == H ||
	   (uintptr)module > (uintptr)-1-sizeof(*module))
		error(exModule);
	memmove(&program, (uchar*)module+O(Module, prog), sizeof(program));
	memmove(&count, (uchar*)module+O(Module, nprog), sizeof(count));
	if(count <= 0 || program == nil || program == H ||
	   (uintptr)count > (uintptr)-1/sizeof(Inst))
		error(exBounds);
	base = (uintptr)program;
	bytes = (uintptr)count*sizeof(Inst);
	if(base > (uintptr)-1-bytes)
		error(exBounds);
	*owner = module;
	pc = (uintptr)R.PC;
	if(pc < base || pc-base >= bytes || (pc-base)%sizeof(Inst) != 0)
		return -1;
	return (pc-base)/sizeof(Inst);
}


/* Capture a complete valid bytecode instruction before callbacks or REG publication. */
VmInline int
vmfetch(VmFetch *f)
{
	f->pc = R.PC;
	f->index = vmlocation(&f->module);
	if(f->module == nil)
		return 0;
	if(f->index < 0)
		error(exBounds);
	f->op = ((uchar*)f->pc)[O(Inst, op)];
	f->add = ((uchar*)f->pc)[O(Inst, add)];
	if(f->op <= INOP || f->op >= MAXDIS || f->op == IECLR)
		error(exOp);
	return 1;
}


/* Decode using captured mode/PC identity and publish exactly the next Dis address. */
VmInline void
vmdecode(VmFetch *f)
{
	void (*decode)(void);

	decode = dec[f->add];
	if(decode == nil || decode == (void(*)(void))H)
		error(exOp);
	decode();
	R.PC = (Inst*)((uintptr)f->pc+sizeof(Inst));
}


/* Instrumented loops intentionally execute the authoritative C opcode handler. */
VmInline void
vmrun(VmFetch *f)
{
	void (*handler)(void);

	handler = optab[f->op];
	if(handler == nil || handler == (void(*)(void))H)
		error(exOp);
	handler();
}


/* Honor callback yields without decrementing zero, negative values or INT_MIN. */
VmInline int
vmyield(void)
{
	if(R.IC <= 1){
		R.IC = 0;
		return 0;
	}
	R.IC--;
	return 1;
}


/* Preserve relocated native-PC tokens and let the native backend own its quantum. */
VmInline void
vmnative(void)
{
	if(comvec == nil || comvec == (void(*)(void))H)
		error(exCompile);
	comvec();
}
#undef VmInline
#endif
