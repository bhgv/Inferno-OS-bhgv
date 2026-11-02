/* Experimental proposal preflight only: this header does not admit executable calls. */
#ifndef DIS64_LOCAL_CATALOG_H
#define DIS64_LOCAL_CATALOG_H // Private draft catalog checker, not a wire profile identifier.
#define Dis64LocalBytes 1048576U // Bound each draft metadata payload before any decode.
#define Dis64LocalFunctions 64U // Bounded local scratch/resource policy, not a format limit.
#define Dis64LocalParameters 64U // Initial scalar-parameter work bound.
#define Dis64LocalInstructions 65536U // Initial instruction-index work bound.

#include "dis64-bindings.h"

typedef struct Dis64LocalFunction Dis64LocalFunction;

struct Dis64LocalFunction {
	unsigned int symbol; // Existing export name ID, never a native address.
	unsigned int entry; // Inclusive proposed function interval start.
	unsigned int end; // Exclusive interval end, not proof of valid control flow.
	unsigned int frame; // Existing frame Type ordinal.
	unsigned int argumentend; // End of canonical scalar argument storage from byte64.
	unsigned int resultcell; // Proposed final aligned eight-byte caller result cell.
	Dis64Signature signature; // Borrowed exact accepted scalar signature bytes.
};


/* Check geometry of an already validated binding; ordinary frame semantics stay unchanged. */
static int
dis64localshape(const Dis64Binding *binding, const Dis64TypeTable *types,
	Dis64LocalFunction *result)
{
	Dis64LocalFunction function;
	Dis64Type frame;
	unsigned int i, at, width, tail;

	if(!dis64typeget(types, binding->frame, &frame) || frame.align != 8 || frame.bytes < 72)
		return 0;
	for(i=0; i<frame.mapbytes; i++)
		if(frame.map[i] != 0)
			return 0;
	tail = frame.bytes-8;
	at = 64;
	for(i=0; i<binding->signature.parameters; i++){
		width = dis64scalarwidth(dis64u32(binding->signature.data+12+4*i));
		if(width == 0 || at > tail || width-1 > tail-at)
			return 0;
		at = (at+width-1)&~(width-1);
		if(at > tail || width > tail-at)
			return 0;
		at += width;
	}
	function.symbol = binding->symbol;
	function.entry = binding->entry;
	function.end = 0;
	function.frame = binding->frame;
	function.argumentend = at;
	function.resultcell = tail;
	function.signature = binding->signature;
	*result = function;
	return 1;
}


/* Revalidate raw accepted codecs, then atomically publish a proposed interval catalog.
 * Inputs span accessible immutable bytes; output/written are disjoint caller storage.
 * This checks metadata/geometry only, not instructions, CFG, boolean values or CALL ABI.
 */
static int
dis64localcatalogcheck(const unsigned char *exports, unsigned int exportbytes,
	const unsigned char *strings, unsigned int stringbytes,
	const unsigned char *typedata, unsigned int typebytes, unsigned int objectlimit,
	unsigned int instructions, unsigned int entry, unsigned int entryframe,
	Dis64LocalFunction *output, unsigned int capacity, unsigned int *written)
{
	Dis64LocalFunction functions[Dis64LocalFunctions], moving;
	Dis64Bindings bindings;
	Dis64Binding binding;
	Dis64TypeTable types;
	DisStringPoolView pool;
	unsigned int i, j;

	if(output == 0 || written == 0 || exportbytes > Dis64LocalBytes ||
	   stringbytes > Dis64LocalBytes || typebytes > Dis64LocalBytes ||
	   entry != 0 || instructions == 0 ||
	   instructions > Dis64LocalInstructions ||
	   !dis64typescheck(typedata, typebytes, 65536, objectlimit, &types) ||
	   !disstringpoolcheck(strings, stringbytes, 65536, &pool) ||
	   !dis64bindingscheck(exports, exportbytes, 1, &pool, &types, instructions,
	   Dis64LocalFunctions, Dis64LocalParameters, &bindings) ||
	   bindings.count == 0 || bindings.count > capacity)
		return 0;
	for(i=0; i<bindings.count; i++){
		if(!dis64bindingget(&bindings, i, &binding) ||
		   !dis64localshape(&binding, &types, functions+i))
			return 0;
	}
	for(i=1; i<bindings.count; i++){
		moving = functions[i];
		j = i;
		while(j != 0 && functions[j-1].entry > moving.entry){
			functions[j] = functions[j-1];
			j--;
		}
		functions[j] = moving;
	}
	if(functions[0].entry != 0 || functions[0].frame != entryframe ||
	   functions[0].signature.result != Dis64Void || functions[0].signature.parameters != 0)
		return 0;
	for(i=0; i<bindings.count; i++){
		if(i != 0 && functions[i-1].entry == functions[i].entry)
			return 0;
		functions[i].end = i+1 < bindings.count ? functions[i+1].entry : instructions;
	}
	for(i=0; i<bindings.count; i++)
		output[i] = functions[i];
	*written = bindings.count;
	return 1;
}
#endif
