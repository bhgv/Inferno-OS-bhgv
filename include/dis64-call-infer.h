/* Conservative local kinds from immutable Dis64 bytes; public file admission remains closed. */
#ifndef DIS64_CALL_INFER_H
#define DIS64_CALL_INFER_H // Private scalar inference, no wire or feature identifier.

#include "dis64-call-image.h"

typedef struct Dis64CallInference Dis64CallInference;

struct Dis64CallInference {
	Dis64LocalFunction functions[Dis64LocalFunctions]; // Raw catalog identities, in entry order.
	Dis64FunctionLocals locals[Dis64LocalFunctions]; // Derived declarations, never supplied by a manifest.
	Dis64LocalSlot slots[Dis64LocalFunctions][Dis64LocalSlots]; // Disposable bounded numeric span facts.
	unsigned int frames[Dis64LocalFunctions]; // Validated native-independent byte geometry.
	unsigned int count; // Disposable catalog population, not successful proof publication.
};


/* Insert one exact numeric span; kinds cannot change or overlap even across unreachable code. */
static int
dis64inferslot(Dis64CallInference *work, unsigned int owner, long offset, unsigned int kind)
{
	Dis64FunctionLocals *locals;
	Dis64LocalSlot *slots, moving;
	unsigned int width, tail, at, i, j, other;

	width = dis64scalarwidth(kind);
	if(kind == Dis64Bool || width == 0 || offset < 64 || (unsigned long)offset > 0xffffffffUL)
		return 0;
	at = (unsigned int)offset;
	tail = work->frames[owner]-8;
	if(at%width != 0 || at > tail || width > tail-at)
		return 0;
	locals = work->locals+owner;
	slots = work->slots[owner];
	for(i=0; i<locals->count; i++){
		other = dis64scalarwidth(slots[i].kind);
		if(at == slots[i].offset)
			return kind == slots[i].kind;
		if(at < slots[i].offset+other && slots[i].offset < at+width)
			return 0;
	}
	if(locals->count == Dis64LocalSlots)
		return 0;
	moving.offset = at;
	moving.kind = kind;
	j = locals->count;
	while(j != 0 && slots[j-1].offset > at){
		slots[j] = slots[j-1];
		j--;
	}
	slots[j] = moving;
	locals->count++;
	return 1;
}


/* Only direct numeric FP spans contribute facts; immediates never invent a local declaration. */
static int
dis64inferoperand(Dis64CallInference *work, unsigned int owner, unsigned int mode,
	long value, unsigned int kind)
{
	if(mode == AIMM)
		return kind != Dis64Real;
	return mode == AFP && dis64inferslot(work, owner, value, kind);
}


/* Infer a conservative numeric subset, then leave modes/control/assignment to the full verifier.
 * Bool is deliberately excluded: equal W width is not sufficient evidence for a Bool value. */
static int
dis64inferinstruction(Dis64CallInference *work, unsigned int owner, const Dis64CallDecoded *decoded)
{
	const Dis64Instruction *instruction;
	unsigned int op, sm, dm, mm, lm, kind, sourcekind;
	long lhs;
	int move, branch;

	instruction = &decoded->instruction;
	op = instruction->op;
	sm = USRC(instruction->mode);
	dm = UDST(instruction->mode);
	mm = instruction->mode&ARM;
	if(op == IFRAME || op == ICALL || op == ILEA || op == IRET || op == IEXIT || op == IJMP)
		return 1;
	kind = Dis64Int;
	switch(op){
	case IMOVW: case IADDW: case ISUBW: case IMULW: case IDIVW: case IMODW:
	case IANDW: case IORW: case IXORW: case ISHLW: case ISHRW:
	case IBEQW: case IBNEW: case IBLTW: case IBLEW: case IBGTW: case IBGEW:
		break;
	case IMOVL: case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
	case IANDL: case IORL: case IXORL: case ISHLL: case ISHRL:
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		kind = Dis64Big;
		break;
	case IMOVF: case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		kind = Dis64Real;
		break;
	case ICVTWL:case ICVTWF:
		return dis64inferoperand(work, owner, sm, instruction->source, Dis64Int) &&
			dm == AFP && dis64inferslot(work, owner, instruction->destination, op == ICVTWL ? Dis64Big : Dis64Real);
	default:
		return 0;
	}
	move = op == IMOVW || op == IMOVL || op == IMOVF;
	branch = dis64localflowkind(op) == Dis64FlowBranch;
	/* Outgoing result tails and prepared frames are pointer-bearing special roles.
	 * They are never inserted into the ordinary numeric local namespace. */
	if(move && owner == 0 && sm == AFP && instruction->source >= 0 &&
	   (unsigned long)instruction->source == work->frames[0]-8){
		return dm == AFP && dis64inferslot(work, owner, instruction->destination, kind);
	}
	sourcekind = op == ISHLL || op == ISHRL ? Dis64Int : kind;
	if(!dis64inferoperand(work, owner, sm, instruction->source, sourcekind))
		return 0;
	if(dm == (AFP|AIND))
		return move;
	if(!branch && (dm != AFP || !dis64inferslot(work, owner, instruction->destination, kind)))
		return 0;
	if(move)
		return 1;
	lm = mm == AXNON ? dm : mm == AXIMM ? AIMM : mm == AXINF ? AFP : AMP;
	lhs = mm == AXNON ? instruction->destination : instruction->middle;
	return dis64inferoperand(work, owner, lm, lhs, kind);
}


/* A chained caller's result tail is special storage at every depth. */
static int
dis64chaininferinstruction(Dis64CallInference *work,unsigned int owner,const Dis64CallDecoded *decoded)
{
    const Dis64Instruction *in=&decoded->instruction;unsigned int kind;
    if((in->op==IMOVW || in->op==IMOVL || in->op==IMOVF) && USRC(in->mode)==AFP &&
       in->source>=0 && (unsigned long)in->source==work->frames[owner]-8){
        kind=in->op==IMOVW?Dis64Int:in->op==IMOVL?Dis64Big:Dis64Real;
        return in->mode==(SRC(AFP)|DST(AFP)) && dis64inferslot(work,owner,in->destination,kind);
    }
    return dis64inferinstruction(work,owner,decoded);
}

/* Derive declarations solely from raw catalog signatures and complete decoded instruction roles.
 * Work/decoded are disposable, caller-owned, disjoint from immutable bytes and final outputs.
 * This does not prove definite initialization, result readiness or reachable control flow. */
static int
dis64callinferlocalsmode(const Dis64CallImage *image, Dis64CallInference *work,
	Dis64CallDecoded *decoded, unsigned int capacity, unsigned int objectlimit,int chain)
{
	Dis64Section exports = {0}, strings = {0}, types = {0}, section;
	Dis64Type frame;
	Dis64FunctionLocals *locals;
	const unsigned char *data;
	unsigned int i, j, at, used, owner, kind, width, count, codebytes;

	if(image == 0 || work == 0 || decoded == 0 || image->instructions > capacity)
		return 0;
	data = image->envelope.data;
	codebytes = 0;
	for(i=0; i<image->envelope.sections; i++){
		if(!dis64section(&image->envelope, i, &section))
			return 0;
		switch(section.kind){
		case Dis64Code: codebytes = section.bytes; break;
		case Dis64Exports: exports = section; break;
		case Dis64Strings: strings = section; break;
		case Dis64Types: types = section; break;
		}
	}
	if(!dis64localcatalogcheck(data+exports.offset, exports.bytes, data+strings.offset,
	   strings.bytes, data+types.offset, types.bytes, objectlimit, image->instructions,
	   image->module.entry, image->module.frametype, work->functions, Dis64LocalFunctions, &count))
		return 0;
	work->count = count;
	for(i=0; i<count; i++){
		if(work->functions[i].signature.result == Dis64Bool ||
		   !dis64typeget(&image->types, work->functions[i].frame, &frame))
			return 0;
		work->frames[i] = frame.bytes;
		locals = work->locals+i;
		locals->symbol = work->functions[i].symbol;
		locals->entry = work->functions[i].entry;
		locals->frame = work->functions[i].frame;
		locals->count = 0;
		locals->slots = work->slots[i];
		at = 64;
		for(j=0; j<work->functions[i].signature.parameters; j++){
			kind = dis64u32(work->functions[i].signature.data+12+4*j);
			width = dis64scalarwidth(kind);
			if(width == 0 || kind == Dis64Bool)
				return 0;
			at = (at+width-1)&~(width-1);
			if(!dis64inferslot(work, i, at, kind))
				return 0;
			at += width;
		}
	}
	at = 4;
	owner = 0;
	for(i=0; i<image->instructions; i++){
		while(i >= work->functions[owner].end)
			owner++;
		if(!dis64callread(image->code+at, codebytes-at, decoded+i, &used) ||
		   !(chain?dis64chaininferinstruction(work,owner,decoded+i):dis64inferinstruction(work, owner, decoded+i)))
			return 0;
		at += used;
	}
	return at == codebytes;
}


static int
dis64callinferlocals(const Dis64CallImage *image,Dis64CallInference *work,Dis64CallDecoded *decoded,unsigned int capacity,unsigned int objectlimit)
{
    return dis64callinferlocalsmode(image,work,decoded,capacity,objectlimit,0);
}

/* Raw-image authority is derived locally; only the complete existing semantic proof publishes.
 * No external local declarations, runtime Module, C-step selection or file permission are accepted. */
static int
dis64callinfercheck(const unsigned char *data, unsigned int bytes, unsigned int objectlimit,
	Dis64CallInference *work, Dis64CallDecoded *decoded, Dis64InitWork *scratch, unsigned int *queue,
	unsigned int workcapacity, unsigned int limit, Dis64BoundCall *output, unsigned int capacity,
	unsigned int *written, Dis64CallImage *result)
{
	Dis64CallImage image;

	if(result == 0 || written == 0 || !dis64callimageopen(data, bytes, objectlimit, &image) ||
	   !dis64callinferlocals(&image, work, decoded, workcapacity, objectlimit))
		return 0;
	return dis64callimagecheck(data, bytes, objectlimit, work->locals, work->count,
		decoded, scratch, queue, workcapacity, limit, output, capacity, written, result);
}
#endif
