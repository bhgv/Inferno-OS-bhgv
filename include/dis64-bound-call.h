/* Private linear entry/leaf-call composition; executable file-call admission remains closed. */
#ifndef DIS64_BOUND_CALL_H
#define DIS64_BOUND_CALL_H // Canonical decoded call setup, never a wire/profile identifier.
#define Dis64CallLimit (135U*Dis64LocalInstructions) // Numeric/return work plus one linear caller unit/node.

#include "dis64-bound-return.h"

typedef struct Dis64CallDecoded Dis64CallDecoded;

/* Disposable owned decode scratch, overwritten from the complete raw payload before use. */
struct Dis64CallDecoded {
	Dis64Instruction instruction; // Existing opcode/modes and primary signed30 operands.
	long secondary; // Indirect destination displacement, zero for direct destinations.
};

typedef struct Dis64BoundCall Dis64BoundCall;

/* Conditional static composition, not a runtime capability or proof of call completion. */
struct Dis64BoundCall {
	Dis64BoundReturn body; // Canonical identity, numeric facts and leaf return coverage.
	unsigned int calls; // Checked normal-return CALL continuations in the linear entry.
	unsigned int arguments; // Exact typed argument stores, including legal repeated stores.
	unsigned int bindings; // Dedicated caller-tail to prepared callee byte32 bindings.
	unsigned int results; // Exact typed copies of completed outgoing results into locals.
	unsigned int steps; // Charged linear caller instructions; leaf records contain zero.
};


/* Decode existing indirect destination framing; interpretation belongs to the owned adapters. */
static int
dis64callread(const unsigned char *data, unsigned int bytes, Dis64CallDecoded *output,
	unsigned int *used)
{
	Dis64CallDecoded decoded = {0};
	unsigned int at, length, sm;

	if(data == 0 || output == 0 || used == 0 || bytes < 2)
		return 0;
	if(UDST(data[1]) != (AFP|AIND)){
		if(!dis64instructionread(data, bytes, &decoded.instruction, used))
			return 0;
		*output = decoded;
		return 1;
	}
	decoded.instruction.op = data[0];
	decoded.instruction.mode = data[1];
	sm = USRC(data[1]);
	if((data[1]&ARM) != AXNON || (sm != AFP && sm != AIMM))
		return 0;
	at = 2;
	if(!dis64operandread(data+at, bytes-at, &decoded.instruction.source, &length))
		return 0;
	at += length;
	if(!dis64operandread(data+at, bytes-at, &decoded.instruction.destination, &length))
		return 0;
	at += length;
	if(!dis64operandread(data+at, bytes-at, &decoded.secondary, &length))
		return 0;
	at += length;
	*output = decoded;
	*used = at;
	return 1;
}


/* Apply only internally derived setup facts; input state is never supplied by a file client. */
static int
dis64callstep(const Dis64LocalPlan *plan, Dis64LocalState *state, Dis64LocalEvent *event)
{
	Dis64LocalState next;

	if(!dis64localstatestep(plan, state, event, &next))
		return 0;
	*state = next;
	return 1;
}


/* Prove both values of a typed Bool source instead of inventing its concrete runtime value. */
static int
dis64callargument(const Dis64LocalPlan *plan, Dis64LocalState *state,
	Dis64LocalEvent *event, unsigned int mode, long value)
{
	Dis64LocalState zero, one, merged;

	if(event->kind != Dis64Bool || mode == AIMM){
		event->boolean = event->kind == Dis64Bool ? (unsigned int)value : 0;
		return dis64callstep(plan, state, event);
	}
	event->boolean = 0;
	if(!dis64localstatestep(plan, state, event, &zero))
		return 0;
	event->boolean = 1;
	if(!dis64localstatestep(plan, state, event, &one) ||
	   !dis64localstatemerge(plan, &zero, &one, &merged))
		return 0;
	*state = merged;
	return 1;
}


/* Resolve an exact declared argument start; padding, interiors and result/header cells refuse. */
static int
dis64callargumentindex(const Dis64LocalCallee *callee, long offset, unsigned int *index)
{
	unsigned int i, at, width;

	if(offset < 64 || (unsigned long)offset > 0xffffffffUL)
		return 0;
	at = 64;
	for(i=0; i<callee->parameters; i++){
		width = dis64scalarwidth(callee->kinds[i]);
		at = (at+width-1)&~(width-1);
		if((unsigned long)offset == at){
			*index = i;
			return 1;
		}
		at += width;
	}
	return 0;
}


/* Check a linear root with complete setup islands and only root-to-leaf edges.
 * Canonical catalog/declarations and decoded bytes are owned checked inputs.
 * Numeric initialization and leaf guarantees are discharged before final publication.
 */
static int
dis64callroot(const Dis64LocalFunction *functions, const Dis64LocalCallee *callees,
	unsigned int functionsused, unsigned int framebytes, const Dis64FunctionLocals *locals,
	const Dis64CallDecoded *decoded, Dis64InitWork *scratch, Dis64BoundCall *result)
{
	Dis64LocalPlan plan;
	Dis64LocalState state = {0};
	Dis64LocalEvent event, zeroevent = {0};
	Dis64LocalOperands fact, zerofact = {0};
	const Dis64Instruction *instruction, *call;
	const Dis64LocalCallee *callee;
	unsigned int pc, j, chosen, pointer, sm, dm, kind, op, index, id, end;

	plan.framebytes = framebytes;
	plan.argumentend = functions[0].argumentend;
	plan.resultcell = functions[0].resultcell;
	plan.result = 0;
	plan.callees = functionsused;
	plan.functions = callees;
	if(!dis64localplanvalid(&plan))
		return 0;
	end = functions[0].end;
	pointer = 0;
	for(pc=0; pc<end; pc++){
		instruction = &decoded[pc].instruction;
		sm = USRC(instruction->mode);
		dm = UDST(instruction->mode);
		event = zeroevent;
		fact = zerofact;
		fact.op = instruction->op;
		fact.edge = Dis64CFGNext;
		if(instruction->op == IFRAME){
			if(state.site != 0 || instruction->mode != (SRC(AIMM)|DST(AFP)) ||
			   (instruction->destination != 40 && instruction->destination != 48 && instruction->destination != 56))
				return 0;
			/* Disjoint islands are scanned once; nested FRAME or an early exit refuses. */
			for(j=pc+1; j<end; j++){
				op = decoded[j].instruction.op;
				if(op == ICALL || op == IFRAME || op == IEXIT)
					break;
			}
			if(j == end || decoded[j].instruction.op != ICALL)
				return 0;
			call = &decoded[j].instruction;
			chosen = functionsused;
			for(j=1; j<functionsused; j++)
				if(call->destination >= 0 && (unsigned long)call->destination == functions[j].entry)
					chosen = j;
			if(chosen == functionsused || instruction->source < 0 ||
			   (unsigned long)instruction->source != functions[chosen].frame)
				return 0;
			event.action = Dis64StatePrepare;
			event.site = pc+1;
			event.callee = chosen;
			event.frame = functions[chosen].frame;
			if(!dis64callstep(&plan, &state, &event))
				return 0;
			pointer = (unsigned int)instruction->destination;
		}else if(instruction->op == ICALL){
			if(state.site == 0 || instruction->mode != (SRC(AFP)|DST(AIMM)) ||
			   (unsigned long)instruction->source != pointer || instruction->destination < 0 ||
			   (unsigned long)instruction->destination != functions[state.callee-1].entry)
				return 0;
			event.action = Dis64StateCall;
			event.callee = state.callee-1;
			event.frame = functions[event.callee].frame;
			if(!dis64callstep(&plan, &state, &event))
				return 0;
			pointer = 0;
			result->calls++;
		}else if(state.site != 0 && dm == (AFP|AIND)){
			callee = callees+state.callee-1;
			if((unsigned long)instruction->destination != pointer)
				return 0;
			if(instruction->op == ILEA){
				if(instruction->mode != (SRC(AFP)|DST(AFP|AIND)) ||
				   instruction->source < 0 || (unsigned long)instruction->source != plan.resultcell ||
				   decoded[pc].secondary != 32)
					return 0;
				event.action = Dis64StateBind;
				event.kind = callee->result;
				event.source = plan.resultcell;
				event.destination = 32;
				event.width = 8;
				if(!dis64callstep(&plan, &state, &event))
					return 0;
				result->bindings++;
			}else{
				if(!dis64callargumentindex(callee, decoded[pc].secondary, &index))
					return 0;
				kind = callee->kinds[index];
				op = kind == Dis64Big ? IMOVL : kind == Dis64Real ? IMOVF : IMOVW;
				if(instruction->op != op ||
				   !dis64localreadslot(locals->slots, locals->count, sm, instruction->source, kind, &fact.source))
					return 0;
				fact.kind = kind;
				fact.reads = fact.source != 0;
				event.action = Dis64StateArgument;
				event.index = index;
				event.kind = kind;
				if(!dis64callargument(&plan, &state, &event, sm, instruction->source))
					return 0;
				result->arguments++;
			}
		}else if(state.site != 0){
			return 0;
		}else if((instruction->op == IMOVW || instruction->op == IMOVL || instruction->op == IMOVF) &&
		   sm == AFP && instruction->source >= 0 && (unsigned long)instruction->source == plan.resultcell){
			kind = state.ready;
			op = kind == Dis64Big ? IMOVL : kind == Dis64Real ? IMOVF : IMOVW;
			if(kind == 0 || instruction->op != op || instruction->mode != (SRC(AFP)|DST(AFP)) ||
			   !dis64localreadslot(locals->slots, locals->count, AFP, instruction->destination, kind, &id))
				return 0;
			event.action = Dis64StateRead;
			event.kind = kind;
			if(!dis64callstep(&plan, &state, &event))
				return 0;
			fact.write = id;
			fact.kind = kind;
			result->results++;
		}else if(instruction->op == ICVTWF){
			/* Existing int32-to-real conversion supplies a typed Real without inventing a seed. */
			if((instruction->mode != (SRC(AIMM)|DST(AFP)) && instruction->mode != (SRC(AFP)|DST(AFP))) ||
			   !dis64localreadslot(locals->slots, locals->count, sm, instruction->source, Dis64Int, &fact.source) ||
			   !dis64localreadslot(locals->slots, locals->count, AFP, instruction->destination, Dis64Real, &id))
				return 0;
			fact.kind = Dis64Real;
			fact.write = id;
			fact.reads = fact.source != 0;
		}else if(instruction->op == IEXIT){
			if(instruction->mode != (SRC(AXXX)|DST(AXXX)) || pc+1 != end)
				return 0;
			fact.edge = Dis64CFGTerminal;
		}else{
			if(!dis64localoperandscheck(instruction, end, 0, end,
			   locals->slots, locals->count, framebytes, &fact) || fact.edge != Dis64CFGNext)
				return 0;
		}
		if(fact.edge == Dis64CFGNext && pc+1 == end)
			return 0;
		scratch[pc].fact = fact;
	}
	result->steps = end;
	return 1;
}

/* Compose actual linear caller setup with canonical leaf bodies before publishing any result.
 * Context kinds remain trusted compiler facts; container provenance is a caller obligation.
 * Code/metadata/declarations/scratch/queue/output/written spans are accessible and disjoint.
 * Scratch may change on refusal; final output/written remain unchanged until total success.
 */
static int
dis64boundcallcheck(const unsigned char *code, unsigned int codebytes,
	const unsigned char *exports, unsigned int exportbytes,
	const unsigned char *strings, unsigned int stringbytes,
	const unsigned char *typedata, unsigned int typebytes, unsigned int objectlimit,
	unsigned int entry, unsigned int entryframe,
	const Dis64FunctionLocals *locals, unsigned int localcount,
	Dis64CallDecoded *decoded, Dis64InitWork *scratch, unsigned int *queue, unsigned int workcapacity,
	unsigned int limit, Dis64BoundCall *output, unsigned int capacity, unsigned int *written)
{
	Dis64LocalFunction functions[Dis64LocalFunctions];
	Dis64BoundCall results[Dis64LocalFunctions] = {0};
	Dis64TypeTable types;
	Dis64Type type;
	Dis64Instruction instruction;
	Dis64LocalCallee callees[Dis64LocalFunctions];
	Dis64LocalOperands terminal = {0};
	const Dis64FunctionLocals *declaration;
	unsigned int mapping[Dis64LocalFunctions], frames[Dis64LocalFunctions];
	unsigned int seeds[Dis64LocalFunctions][4], parameters[Dis64LocalFunctions];
	unsigned int count, functionsused, i, j, found, owner, pc, at, used, spent, store, kind, ordinal;

	if(code == 0 || codebytes < 4 || codebytes > Dis64LocalBytes || locals == 0 || decoded == 0 ||
	   localcount == 0 || localcount > Dis64LocalFunctions || scratch == 0 || queue == 0 ||
	   output == 0 || written == 0 || limit == 0 || limit > Dis64CallLimit)
		return 0;
	count = dis64u32(code);
	if(count == 0 || count > Dis64LocalInstructions || count > workcapacity || count > (codebytes-4)/2 ||
	   !dis64localcatalogcheck(exports, exportbytes, strings, stringbytes, typedata, typebytes,
	   objectlimit, count, entry, entryframe, functions, Dis64LocalFunctions, &functionsused) ||
	   functionsused != localcount || functionsused > capacity ||
	   !dis64typescheck(typedata, typebytes, 65536, objectlimit, &types))
		return 0;
	for(i=0; i<functionsused; i++){
		found = 0;
		for(j=0; j<localcount; j++){
			if(locals[j].symbol == functions[i].symbol && locals[j].entry == functions[i].entry &&
			   locals[j].frame == functions[i].frame){
				mapping[i] = j;
				found++;
			}
		}
		if(found != 1 || !dis64typeget(&types, functions[i].frame, &type))
			return 0;
		declaration = locals+mapping[i];
		frames[i] = type.bytes;
		callees[i].frame = functions[i].frame;
		callees[i].result = functions[i].signature.result;
		callees[i].parameters = functions[i].signature.parameters;
		for(j=0; j<Dis64LocalParameters; j++)
			callees[i].kinds[j] = j < callees[i].parameters ? dis64u32(functions[i].signature.data+12+4*j) : 0;
		if(!dis64localslotscheck(declaration->slots, declaration->count, frames[i]) ||
		   !dis64initseed(functions[i].signature.data, functions[i].signature.bytes,
		   declaration->slots, declaration->count, seeds[i], parameters+i))
			return 0;
	}
	at = 4;
	owner = 0;
	for(pc=0; pc<count; pc++){
		while(pc >= functions[owner].end)
			owner++;
		declaration = locals+mapping[owner];
		if(!dis64callread(code+at, codebytes-at, decoded+pc, &used))
			return 0;
		instruction = decoded[pc].instruction;
		if(owner == 0){
			at += used;
			continue;
		}
		store = UDST(instruction.mode) == (AFP|AIND);
		if(store && (instruction.destination != 32 || decoded[pc].secondary != 0 ||
		   (instruction.mode&ARM) != AXNON || (USRC(instruction.mode) != AFP && USRC(instruction.mode) != AIMM)))
			return 0;
		kind = functions[owner].signature.result;
		if(store){
			if(owner == 0 || !dis64returnfact(&instruction, kind, declaration->slots,
			   declaration->count, &scratch[pc].fact))
				return 0;
		}else if(instruction.op == IRET){
			if(owner == 0 || instruction.mode != (SRC(AXXX)|DST(AXXX)|AXNON) ||
			   instruction.source != 0 || instruction.middle != 0 || instruction.destination != 0)
				return 0;
			scratch[pc].fact = terminal;
			scratch[pc].fact.op = IRET;
			scratch[pc].fact.edge = Dis64CFGTerminal;
		}else{
			if(instruction.op == IEXIT && owner != 0)
				return 0;
			if(!dis64localoperandscheck(&instruction, count, functions[owner].entry, functions[owner].end,
			   declaration->slots, declaration->count, frames[owner], &scratch[pc].fact))
				return 0;
		}
		if((scratch[pc].fact.edge == Dis64CFGNext || scratch[pc].fact.edge == Dis64CFGBranch) &&
		   pc+1 == functions[owner].end)
			return 0;
		if(scratch[pc].fact.edge == Dis64CFGJump || scratch[pc].fact.edge == Dis64CFGBranch)
			scratch[pc].fact.target -= functions[owner].entry;
		at += used;
	}
	if(at != codebytes || !dis64callroot(functions, callees, functionsused, frames[0],
	   locals+mapping[0], decoded, scratch, results))
		return 0;
	spent = results[0].steps;
	if(spent > limit)
		return 0;
	/* Prove every leaf before validating the caller's conditional normal continuations. */
	for(ordinal=0; ordinal<functionsused; ordinal++){
		i = ordinal+1 == functionsused ? 0 : ordinal+1;
		declaration = locals+mapping[i];
		if(spent == limit || !dis64initializedsolve(functions[i].end-functions[i].entry,
		   declaration->count, parameters[i], seeds[i], scratch+functions[i].entry, queue,
		   limit-spent, &results[i].body.function.numeric))
			return 0;
		spent += results[i].body.function.numeric.work;
		if(i != 0 && !dis64returncover(scratch+functions[i].entry, functions[i].end-functions[i].entry,
		   functions[i].signature.result, queue, limit-spent, &results[i].body))
			return 0;
		spent += results[i].body.work;
		results[i].body.function.symbol = functions[i].symbol;
		results[i].body.function.entry = functions[i].entry;
		results[i].body.function.end = functions[i].end;
		results[i].body.function.frame = functions[i].frame;
	}
	for(i=0; i<functionsused; i++)
		output[i] = results[i];
	*written = functionsused;
	return 1;
}
#endif
