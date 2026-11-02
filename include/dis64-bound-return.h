/* Private conditional return-store proof; executable file-call admission stays closed. */
#ifndef DIS64_BOUND_RETURN_H
#define DIS64_BOUND_RETURN_H // Canonical return-body analysis, not a wire/profile identifier.
#define Dis64ReturnLimit (134U*Dis64LocalInstructions) // Numeric work plus at most three cut-graph units/node.

#include "dis64-bound-numeric.h"

typedef struct Dis64BoundReturn Dis64BoundReturn;

/* Accepted nonvoid functions have a reachable RET and a typed store before EVERY reachable RET. */
struct Dis64BoundReturn {
	Dis64BoundNumeric function; // Canonical identity and conditional numeric summary.
	unsigned int kind; // Canonical scalar result kind; zero means void.
	unsigned int returns; // Entry-reachable RET count, not a termination guarantee.
	unsigned int stores; // Entry-reachable exact typed result stores, excluding numeric writes.
	unsigned int work; // Additional nodes/edge roles reached while cutting after result stores.
};


/* Decode only the agreed destination 0(32(fp)) extension; direct production reader is unchanged.
 * Incoming result-address provenance must be established by the future checked caller binding.
 * Existing classic indirect framing is base operand THEN secondary displacement.
 */
static int
dis64returnread(const unsigned char *data, unsigned int bytes,
	Dis64Instruction *output, unsigned int *used, unsigned int *store)
{
	Dis64Instruction instruction = {0};
	unsigned int at, length, sm;
	long base, secondary;

	if(bytes < 2 || data == 0 || output == 0 || used == 0 || store == 0)
		return 0;
	if(UDST(data[1]) != (AFP|AIND)){
		if(!dis64instructionread(data, bytes, output, used))
			return 0;
		*store = 0;
		return 1;
	}
	instruction.op = data[0];
	instruction.mode = data[1];
	sm = USRC(instruction.mode);
	if((instruction.op != IMOVW && instruction.op != IMOVL && instruction.op != IMOVF) ||
	   (instruction.mode&ARM) != AXNON || (sm != AIMM && sm != AFP))
		return 0;
	at = 2;
	if(!dis64operandread(data+at, bytes-at, &instruction.source, &length))
		return 0;
	at += length;
	if(!dis64operandread(data+at, bytes-at, &base, &length))
		return 0;
	at += length;
	if(!dis64operandread(data+at, bytes-at, &secondary, &length) || base != 32 || secondary != 0)
		return 0;
	at += length;
	instruction.destination = base;
	*output = instruction;
	*used = at;
	*store = 1;
	return 1;
}


/* Derive the exact result kind and read role; no numeric local or pointer field is written. */
static int
dis64returnfact(const Dis64Instruction *instruction, unsigned int kind,
	const Dis64LocalSlot *slots, unsigned int count, Dis64LocalOperands *output)
{
	Dis64LocalOperands fact = {0};
	unsigned int op;

	op = kind == Dis64Big ? IMOVL : kind == Dis64Real ? IMOVF : IMOVW;
	if(kind == 0 || kind > Dis64Bool || instruction->op != op ||
	   !dis64localreadslot(slots, count, USRC(instruction->mode), instruction->source, kind, &fact.source))
		return 0;
	fact.op = instruction->op;
	fact.kind = kind;
	fact.reads = fact.source != 0;
	fact.edge = Dis64CFGNext;
	*output = fact;
	return 1;
}


/* Only internally decoded MOV facts without numeric destinations represent result stores. */
static int
dis64isreturnstore(const Dis64LocalOperands *fact)
{
	return fact->write == 0 && (fact->op == IMOVW || fact->op == IMOVL || fact->op == IMOVF);
}


/* Prove the conditional return postcondition by cutting traversal AFTER every valid store.
 * Numeric analysis has succeeded; facts/seen and edge spans are owned validated inputs.
 * A RET reachable without crossing a store refuses. No result-invalidating calls are admitted.
 * Require a reachable nonvoid RET conservatively; other paths may still loop forever.
 * Candidate result is disposable internal storage; final publication belongs to the constructor.
 */
static int
dis64returncover(Dis64InitWork *scratch, unsigned int count, unsigned int kind,
	unsigned int *queue, unsigned int limit, Dis64BoundReturn *result)
{
	Dis64LocalOperands *fact;
	unsigned int pc, head, tail, edges, targets[2], i, next, units;

	result->kind = kind;
	result->returns = result->stores = result->work = 0;
	for(pc=0; pc<count; pc++){
		scratch[pc].queued = 0;
		if(scratch[pc].seen){
			result->returns += scratch[pc].fact.op == IRET;
			result->stores += dis64isreturnstore(&scratch[pc].fact);
		}
	}
	if(kind == 0)
		return 1;
	if(result->returns == 0)
		return 0;
	head = 0;
	tail = 1;
	queue[0] = 0;
	scratch[0].queued = 1;
	while(head < tail){
		pc = queue[head++];
		fact = &scratch[pc].fact;
		if(fact->op == IRET)
			return 0;
		edges = 0;
		if(!dis64isreturnstore(fact) && fact->edge != Dis64CFGTerminal){
			edges = fact->edge == Dis64CFGBranch ? 2 : 1;
			targets[0] = fact->edge == Dis64CFGJump ? fact->target : pc+1;
			targets[1] = fact->target;
		}
		units = 1+edges;
		if(result->work > limit || units > limit-result->work)
			return 0;
		result->work += units;
		for(i=0; i<edges; i++){
			next = targets[i];
			if(!scratch[next].queued){
				if(tail == count)
					return 0;
				scratch[next].queued = 1;
				queue[tail++] = next;
			}
		}
	}
	return 1;
}


/* Bind actual return stores to canonical metadata, numeric reads and every returning path.
 * Context kinds remain trusted compiler facts; container provenance is a caller obligation.
 * Code/metadata/declarations/scratch/queue/output/written spans are accessible and disjoint.
 * Scratch may change on refusal; final output/written remain unchanged until total success.
 */
static int
dis64boundreturncheck(const unsigned char *code, unsigned int codebytes,
	const unsigned char *exports, unsigned int exportbytes,
	const unsigned char *strings, unsigned int stringbytes,
	const unsigned char *typedata, unsigned int typebytes, unsigned int objectlimit,
	unsigned int entry, unsigned int entryframe,
	const Dis64FunctionLocals *locals, unsigned int localcount,
	Dis64InitWork *scratch, unsigned int *queue, unsigned int workcapacity,
	unsigned int limit, Dis64BoundReturn *output, unsigned int capacity, unsigned int *written)
{
	Dis64LocalFunction functions[Dis64LocalFunctions];
	Dis64BoundReturn results[Dis64LocalFunctions];
	Dis64TypeTable types;
	Dis64Type type;
	Dis64Instruction instruction;
	Dis64LocalOperands terminal = {0};
	const Dis64FunctionLocals *declaration;
	unsigned int mapping[Dis64LocalFunctions], frames[Dis64LocalFunctions];
	unsigned int seeds[Dis64LocalFunctions][4], parameters[Dis64LocalFunctions];
	unsigned int count, functionsused, i, j, found, owner, pc, at, used, spent, store, kind;

	if(code == 0 || codebytes < 4 || codebytes > Dis64LocalBytes || locals == 0 ||
	   localcount == 0 || localcount > Dis64LocalFunctions || scratch == 0 || queue == 0 ||
	   output == 0 || written == 0 || limit == 0 || limit > Dis64ReturnLimit)
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
		if(!dis64returnread(code+at, codebytes-at, &instruction, &used, &store))
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
	if(at != codebytes)
		return 0;
	spent = 0;
	for(i=0; i<functionsused; i++){
		declaration = locals+mapping[i];
		if(spent == limit || !dis64initializedsolve(functions[i].end-functions[i].entry,
		   declaration->count, parameters[i], seeds[i], scratch+functions[i].entry, queue,
		   limit-spent, &results[i].function.numeric))
			return 0;
		spent += results[i].function.numeric.work;
		if(!dis64returncover(scratch+functions[i].entry, functions[i].end-functions[i].entry,
		   functions[i].signature.result, queue, limit-spent, results+i))
			return 0;
		spent += results[i].work;
		results[i].function.symbol = functions[i].symbol;
		results[i].function.entry = functions[i].entry;
		results[i].function.end = functions[i].end;
		results[i].function.frame = functions[i].frame;
	}
	for(i=0; i<functionsused; i++)
		output[i] = results[i];
	*written = functionsused;
	return 1;
}
#endif
