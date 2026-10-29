#include "lib9.h"
#include <limits.h>
#include <float.h>
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include "dis64-pilot.h"

#define Dis64RuntimeBytes (8U*1024U*1024U) // Maximum complete input image for this experimental loader.
#define Dis64RuntimeObject (1024U*1024U) // Maximum scalar data or entry-frame extent.
#define Dis64RuntimeCode 65536U // Maximum instructions admitted before allocation.
#define Dis64RuntimeName 255U // Maximum ASCII display-name length in the scalar runtime subset.

typedef struct Dis64Load Dis64Load;

struct Dis64Load {
	char *path; // Borrowed caller label, copied before successful return.
	Dis64Pilot pilot; // Complete checked immutable source, retained through construction.
	Module *module; // Partial unpublished owner, visible to protected failure cleanup.
	int jit; // Explicit request for the existing native leaf compiler.
};


/* Identify source operands whose native decoder must expose a complete LONG. */
static int
dis64longsource(unsigned int op)
{
	switch(op){
	case IMOVL:
	case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
	case IANDL: case IORL: case IXORL:
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		return 1;
	}
	return 0;
}


/* Bound legacy middle slots separately from the wider, unchanged wire operands. */
static int
dis64runtimeinstruction(const Dis64Instruction *instruction)
{
	unsigned int middle;

	middle = instruction->mode&ARM;
	if(middle == AXIMM){
		if(instruction->middle < -32768 || instruction->middle > 32767)
			return 0;
		/* R.t is WORD-sized; a LONG comparison/arithmetic source cannot borrow it. */
		if(dis64longsource(instruction->op) || instruction->op == ISHLL || instruction->op == ISHRL)
			return 0;
	}else if(middle != AXNON && (instruction->middle < 0 || instruction->middle > 65535))
		return 0;
	return 1;
}


/* Convert checked roles to existing decoder storage without truncating immediates. */
static void
dis64runtimeinst(Inst *target, const Dis64Instruction *source, Inst *program)
{
	LONG immediate;
	unsigned int mode;

	memset(target, 0, sizeof(*target));
	target->op = source->op;
	target->add = source->mode;
	if((source->mode&ARM) != AXNON)
		target->reg = source->middle < 0 ? source->middle+65536 : source->middle;
	mode = USRC(source->mode);
	if(mode == AIMM && dis64longsource(source->op)){
		immediate = source->source;
		memmove(&target->s, &immediate, sizeof(immediate));
	}else if(mode != AXXX)
		target->s.imm = source->source;
	mode = UDST(source->mode);
	if(mode == AIMM)
		target->d.ins = program+source->destination;
	else if(mode != AXXX)
		target->d.ind = source->destination;
}


/* Build actual descriptors, data and instructions while every resource stays owned. */
static int
dis64construct(void *argument)
{
	Dis64Load *load;
	Dis64Instruction instruction;
	DisRuneView name;
	Module *module;
	Heap *data;
	unsigned int i, rune, at, used;

	load = argument;
	load->module = module = mallocz(sizeof(*module), 1);
	if(module == nil)
		return 0;
	module->origmp = H;
	module->ref = 1;
	module->rt = DONTCOMPILE;
	module->ntype = 2;
	module->type = mallocz(2*sizeof(Type*), 1);
	if(module->type == nil)
		return 0;
	/* No type-index operations exist in this subset: runtime slot0 is always MP. */
	module->type[0] = dtype(freeheap, load->pilot.data.bytes, nil, 0);
	if(module->type[0] == nil)
		return 0;
	module->type[1] = dtype(freeheap, load->pilot.frame.bytes, nil, 0);
	if(module->type[1] == nil)
		return 0;
	module->entryt = module->type[1];
	module->nprog = load->pilot.code.count;
	module->prog = mallocz(module->nprog*sizeof(Inst), 1);
	if(module->prog == nil)
		return 0;
	at = 4;
	for(i=0; i<load->pilot.code.count; i++){
		if(!dis64instructionread(load->pilot.code.data+at, load->pilot.code.bytes-at,
		   &instruction, &used))
			return 0;
		dis64runtimeinst(module->prog+i, &instruction, module->prog);
		at += used;
	}
	module->entry = module->prog+load->pilot.module.entry;
	if(load->pilot.data.bytes != 0){
		data = heapz(module->type[0]);
		module->origmp = H2D(uchar*, data);
		if(!dis64initializersapply(&load->pilot.initializers, &load->pilot.data,
		   module->origmp, load->pilot.data.bytes, nil))
			return 0;
	}
	if(!disstringpoolget(&load->pilot.strings, load->pilot.module.name, &name))
		return 0;
	module->name = mallocz(name.count+1, 1);
	if(module->name == nil)
		return 0;
	for(i=0; i<name.count; i++){
		if(!disruneat(&name, i, &rune))
			return 0;
		module->name[i] = rune;
	}
	module->path = strdup(load->path);
	if(module->path == nil)
		return 0;
	if(load->jit && !compile(module, module->nprog, nil))
		return 0;
	return 1;
}


/* Return an unpublished scalar Module; the caller owns it and must unload or schedule it.
 * This explicit construction API does not route readmod, choose file policy or publish
 * a cache entry. Input bytes remain immutable and disjoint through this call. */
Module*
prepare64pilot(char *path, uchar *image, ulong bytes, int jit)
{
	Dis64Load load;
	Dis64Instruction instruction;
	DisRuneView name;
	unsigned int i, rune, at, used;

	memset(&load, 0, sizeof(load));
	if(sizeof(WORD) != 4 || sizeof(UWORD) != 4 || sizeof(void*) != 8 ||
	   sizeof(LONG) != 8 || sizeof(REAL) != 8 || sizeof(Adr) < 8 ||
	   sizeof(ushort) != 2 || CHAR_BIT != 8 || FLT_RADIX != 2 ||
	   DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024 || sizeof(Frame) != 32 ||
	   O(Frame, lr) != 0 || O(Frame, fp) != 8 || O(Frame, mr) != 16 || O(Frame, t) != 24 ||
	   path == nil || path == H || image == nil || image == H ||
	   bytes > Dis64RuntimeBytes || (uintptr)image > (uintptr)-1-bytes ||
	   (jit != 0 && jit != 1)){
		kwerrstr("unsupported Dis64 runtime model or input");
		return nil;
	}
	if(!dis64pilotcheck(image, bytes, Dis64RuntimeObject, Dis64RuntimeCode, &load.pilot) ||
	   !disstringpoolget(&load.pilot.strings, load.pilot.module.name, &name) ||
	   name.count == 0 || name.count > Dis64RuntimeName){
		kwerrstr("unsupported Dis64 scalar module");
		return nil;
	}
	for(i=0; i<name.count; i++)
		if(!disruneat(&name, i, &rune) || rune == 0 || rune > 127){
			kwerrstr("unsupported Dis64 runtime display name");
			return nil;
		}
	at = 4;
	for(i=0; i<load.pilot.code.count; i++){
		if(!dis64instructionread(load.pilot.code.data+at, load.pilot.code.bytes-at,
		   &instruction, &used) || !dis64runtimeinstruction(&instruction)){
			kwerrstr("unsupported Dis64 runtime operand");
			return nil;
		}
		at += used;
	}
	load.path = path;
	load.jit = jit;
	if(kprotect(dis64construct, &load))
		return load.module;
	if(load.module != nil){
		destroy(load.module->origmp);
		freemod(load.module);
	}
	kwerrstr("Dis64 module construction failed");
	return nil;
}
