/* Private typed facts from actual scalar Dis64 operands; not file-call admission. */
#ifndef DIS64_LOCAL_OPERANDS_H
#define DIS64_LOCAL_OPERANDS_H // Typed numeric operand adapter; include isa.h once first.
#define Dis64LocalSlots 128U // Bounded incoming-argument/numeric-local span facts.

#include "dis64-code.h"
#include "dis64-local-flow.h"
#include "dis64-local-cfg.h"

typedef struct Dis64LocalSlot Dis64LocalSlot;

/* Immutable typed declarations, to be derived from compiler/catalog local facts later. */
struct Dis64LocalSlot {
	unsigned int offset; // Exact AFP byte start, never an interior or native pointer.
	unsigned int kind; // Int/bool4 or big/real8, not just a byte width.
};

typedef struct Dis64LocalOperands Dis64LocalOperands;

/* Derived actual operand roles; zero IDs mean immediate/unused, otherwise slot ordinal+1. */
struct Dis64LocalOperands {
	unsigned int op; // Existing checked scalar opcode.
	unsigned int reads; // Number of slot read roles, including repeated aliased reads.
	unsigned int source; // Source read ID, or zero for an immediate/unused role.
	unsigned int middle; // LHS read ID, including old destination for AXNON arithmetic.
	unsigned int write; // Destination slot ID, or zero for control-only instructions.
	unsigned int kind; // Exact value/comparison kind; shift-count source is separately int4.
	unsigned int target; // Absolute Dis-PC for jump/branch, zero otherwise.
	unsigned int edge; // Existing private CFG edge class, never a new wire field.
};


/* Check sorted non-overlapping complete scalar spans, excluding header and result tail. */
static int
dis64localslotscheck(const Dis64LocalSlot *slots, unsigned int count, unsigned int framebytes)
{
	unsigned int i, end, at, width, tail;

	if((unsigned int)-1 != 0xffffffffU || count > Dis64LocalSlots ||
	   (count != 0 && slots == 0) || framebytes < 72 || (framebytes&7) != 0)
		return 0;
	tail = framebytes-8;
	end = 64;
	for(i=0; i<count; i++){
		at = slots[i].offset;
		width = dis64scalarwidth(slots[i].kind);
		if(width == 0 || at < end || at%width != 0 || at > tail || width > tail-at)
			return 0;
		end = at+width;
	}
	return 1;
}


/* Resolve an exact typed FP span; MP, header/tail, holes and width-equivalent kinds refuse. */
static int
dis64localreadslot(const Dis64LocalSlot *slots, unsigned int count, unsigned int mode,
	long offset, unsigned int kind, unsigned int *id)
{
	unsigned int i;

	if(mode == AIMM){
		if(kind == Dis64Real || (kind == Dis64Bool && offset != 0 && offset != 1))
			return 0;
		*id = 0;
		return 1;
	}
	if(mode != AFP || offset < 0 || (unsigned long)offset > 0xffffffffUL)
		return 0;
	for(i=0; i<count; i++){
		if(slots[i].offset == (unsigned int)offset && slots[i].kind == kind){
			*id = i+1;
			return 1;
		}
	}
	return 0;
}


/* Infer only a W move/comparison's declared kind; no numeric-width coercion is implied. */
static unsigned int
dis64localwordkind(const Dis64LocalSlot *slots, unsigned int count, unsigned int mode, long offset)
{
	unsigned int i;

	if(mode != AFP || offset < 0 || (unsigned long)offset > 0xffffffffUL)
		return Dis64Int;
	for(i=0; i<count; i++)
		if(slots[i].offset == (unsigned int)offset)
			return slots[i].kind;
	return Dis64Int;
}


/* Derive exact numeric read/write/control roles from one actual decoded scalar instruction.
 * This validates spans/kinds, not source initialization, value execution or CALL semantics.
 * Accessible immutable input spans and output are disjoint; failure never publishes facts.
 */
static int
dis64localoperandscheck(const Dis64Instruction *instruction, unsigned int instructions,
	unsigned int first, unsigned int end, const Dis64LocalSlot *slots, unsigned int count,
	unsigned int framebytes, Dis64LocalOperands *output)
{
	Dis64LocalOperands fact = {0};
	unsigned int sm, dm, mm, lm, kind, sourcekind, id;
	long lhs;
	int control, branch, move;

	if(output == 0 || instruction == 0 || instructions == 0 ||
	   instructions > Dis64LocalInstructions || first >= end || end > instructions ||
	   !dis64localslotscheck(slots, count, framebytes) ||
	   instruction->source < -536870912L || instruction->source > 536870911L ||
	   instruction->middle < -536870912L || instruction->middle > 536870911L ||
	   instruction->destination < -536870912L || instruction->destination > 536870911L ||
	   !dis64instructioncheck(instruction, instructions, 0, framebytes))
		return 0;
	sm = USRC(instruction->mode);
	dm = UDST(instruction->mode);
	mm = instruction->mode&ARM;
	if((sm == AXXX && instruction->source != 0) ||
	   (dm == AXXX && instruction->destination != 0) || (mm == AXNON && instruction->middle != 0))
		return 0;
	fact.op = instruction->op;
	control = dis64localflowkind(instruction->op);
	fact.edge = Dis64CFGNext;
	if(instruction->op == IEXIT){
		fact.edge = Dis64CFGTerminal;
		*output = fact;
		return 1;
	}
	branch = control == Dis64FlowBranch;
	if(branch || instruction->op == IJMP){
		if((unsigned long)instruction->destination < first || (unsigned long)instruction->destination >= end)
			return 0;
		fact.target = (unsigned int)instruction->destination;
		fact.edge = branch ? Dis64CFGBranch : Dis64CFGJump;
		if(!branch){
			*output = fact;
			return 1;
		}
	}
	kind = Dis64Int;
	switch(instruction->op){
	case IMOVL: case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
	case IANDL: case IORL: case IXORL: case ISHLL: case ISHRL:
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		kind = Dis64Big;
		break;
	case IMOVF: case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		kind = Dis64Real;
		break;
	}
	move = instruction->op == IMOVW || instruction->op == IMOVL || instruction->op == IMOVF;
	lm = mm == AXNON ? dm : mm == AXIMM ? AIMM : mm == AXINF ? AFP : AMP;
	lhs = mm == AXNON ? instruction->destination : instruction->middle;
	if(instruction->op == IMOVW){
		kind = dis64localwordkind(slots, count, dm, instruction->destination);
		if(kind != Dis64Int && kind != Dis64Bool)
			return 0;
	}else if(branch && kind == Dis64Int){
		if(dis64localwordkind(slots, count, sm, instruction->source) == Dis64Bool ||
		   dis64localwordkind(slots, count, lm, lhs) == Dis64Bool)
			kind = Dis64Bool;
	}
	fact.kind = kind;
	sourcekind = instruction->op == ISHLL || instruction->op == ISHRL ? Dis64Int : kind;
	if(!dis64localreadslot(slots, count, sm, instruction->source, sourcekind, &fact.source))
		return 0;
	if(!branch){
		if(dm != AFP || !dis64localreadslot(slots, count, dm, instruction->destination, kind, &id))
			return 0;
		fact.write = id;
	}
	if(!move && !dis64localreadslot(slots, count, lm, lhs, kind, &fact.middle))
		return 0;
	fact.reads = (fact.source != 0)+(fact.middle != 0);
	*output = fact;
	return 1;
}


/* Revalidate raw complete code and derive every numeric fact before publishing any output.
 * Scratch is disposable and may change on refusal. All spans/output/written are disjoint.
 * The current adapter covers one whole scalar code payload, not mixed callable functions.
 */
static int
dis64localoperandsdecode(const unsigned char *data, unsigned int bytes,
	const Dis64LocalSlot *slots, unsigned int slotcount, unsigned int framebytes,
	Dis64LocalOperands *scratch, Dis64LocalOperands *output, unsigned int capacity, unsigned int *written)
{
	Dis64Instruction instruction;
	unsigned int count, pc, at, used;

	if(data == 0 || bytes < 4 || scratch == 0 || output == 0 || written == 0 ||
	   !dis64localslotscheck(slots, slotcount, framebytes))
		return 0;
	count = dis64u32(data);
	if(count == 0 || count > Dis64LocalInstructions || count > capacity || count > (bytes-4)/2)
		return 0;
	at = 4;
	for(pc=0; pc<count; pc++){
		if(!dis64instructionread(data+at, bytes-at, &instruction, &used) ||
		   !dis64localoperandscheck(&instruction, count, 0, count, slots, slotcount, framebytes, scratch+pc))
			return 0;
		at += used;
	}
	if(at != bytes)
		return 0;
	for(pc=0; pc<count; pc++)
		output[pc] = scratch[pc];
	*written = count;
	return 1;
}
#endif
