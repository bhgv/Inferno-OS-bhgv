/* Private catalog-bound numeric proof; no executable callable Module admission. */
#ifndef DIS64_BOUND_NUMERIC_H
#define DIS64_BOUND_NUMERIC_H // Metadata-bound function analysis, not a wire/profile identifier.

#include "dis64-local-initialized.h"

typedef struct Dis64FunctionLocals Dis64FunctionLocals;

/* Immutable compiler declarations; kinds/provenance are not inferred from client widths. */
struct Dis64FunctionLocals {
	unsigned int symbol; // Exact export symbol ID, not a frame-type surrogate.
	unsigned int entry; // Exact function entry Dis-PC.
	unsigned int frame; // Exact exported Type ordinal, even when shapes are equal.
	const Dis64LocalSlot *slots; // Accessible immutable numeric declarations.
	unsigned int count; // At most128 exact scalar spans in this function.
};

typedef struct Dis64BoundNumeric Dis64BoundNumeric;

/* A numeric-only result, never a callee return guarantee or execution capability. */
struct Dis64BoundNumeric {
	unsigned int symbol; // Canonical export symbol, sorted by entry PC.
	unsigned int entry; // Inclusive global instruction interval start.
	unsigned int end; // Exclusive global instruction interval end.
	unsigned int frame; // Canonical exact Type ordinal.
	Dis64Initialized numeric; // Conditional numeric proof under the bound local declarations.
};


/* Bind declarations to raw canonical metadata, check ALL bytes, then analyze every interval.
 * Context kinds remain trusted compiler facts; container provenance is a caller obligation.
 * Code/metadata/declarations/scratch/queue/output/written spans are accessible and disjoint.
 * Scratch may change on refusal; final output/written remain unchanged until total success.
 */
static int
dis64boundnumericcheck(const unsigned char *code, unsigned int codebytes,
	const unsigned char *exports, unsigned int exportbytes,
	const unsigned char *strings, unsigned int stringbytes,
	const unsigned char *typedata, unsigned int typebytes, unsigned int objectlimit,
	unsigned int entry, unsigned int entryframe,
	const Dis64FunctionLocals *locals, unsigned int localcount,
	Dis64InitWork *scratch, unsigned int *queue, unsigned int workcapacity,
	unsigned int limit, Dis64BoundNumeric *output, unsigned int capacity, unsigned int *written)
{
	Dis64LocalFunction functions[Dis64LocalFunctions];
	Dis64BoundNumeric results[Dis64LocalFunctions];
	Dis64TypeTable types;
	Dis64Type type;
	Dis64Instruction instruction;
	Dis64LocalOperands terminal = {0};
	const Dis64FunctionLocals *declaration;
	unsigned int mapping[Dis64LocalFunctions], frames[Dis64LocalFunctions];
	unsigned int seeds[Dis64LocalFunctions][4], parameters[Dis64LocalFunctions];
	unsigned int count, functionsused, i, j, found, owner, pc, at, used, spent;

	if(code == 0 || codebytes < 4 || codebytes > Dis64LocalBytes || locals == 0 ||
	   localcount == 0 || localcount > Dis64LocalFunctions || scratch == 0 || queue == 0 ||
	   output == 0 || written == 0 || limit == 0 || limit > Dis64InitLimit)
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
		if(!dis64instructionread(code+at, codebytes-at, &instruction, &used))
			return 0;
		if(instruction.op == IRET){
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
		   limit-spent, &results[i].numeric))
			return 0;
		spent += results[i].numeric.work;
		results[i].symbol = functions[i].symbol;
		results[i].entry = functions[i].entry;
		results[i].end = functions[i].end;
		results[i].frame = functions[i].frame;
	}
	for(i=0; i<functionsused; i++)
		output[i] = results[i];
	*written = functionsused;
	return 1;
}
#endif
