/* Accepted scalar imports/exports framing; this does not enable CALL/MCALL. */
#ifndef DIS64_BINDINGS_H
#define DIS64_BINDINGS_H // Structural scalar binding-table codec guard.

#include "dis64-signature.h"
#include "dis64-types.h"
#include "dis-string-pool.h"

typedef struct Dis64Binding Dis64Binding;
struct Dis64Binding {
	unsigned int module; // Import-only module-name pool ID; absent for exports.
	unsigned int symbol; // Non-nil symbol-name pool ID.
	unsigned int entry; // Export-only checked Dis-PC.
	unsigned int frame; // Export-only frame descriptor ID.
	Dis64Signature signature; // Complete versioned signature, including its version.
};

typedef struct Dis64Bindings Dis64Bindings;
struct Dis64Bindings {
	const unsigned char *data; // Complete immutable count/records payload.
	unsigned int bytes; // Exact byte extent.
	unsigned int count; // Ordinal record IDs; not resolved native call addresses.
	unsigned int exports; // Internal API selector: zero imports, one exports.
	unsigned int parameters; // Resource limit applied to each signature.
};


/* Decode one extent-bounded record without assigning absent fields wire sentinels. */
static int
dis64bindingread(const unsigned char *data, unsigned int bytes, unsigned int exports,
	unsigned int parameters, Dis64Binding *result, unsigned int *used)
{
	Dis64Binding binding;
	unsigned int header, signaturebytes;

	if(data == 0 || result == 0 || used == 0 || exports > 1 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	header = exports ? 16 : 12;
	if(bytes < header)
		return 0;
	binding.module = binding.entry = binding.frame = 0;
	if(exports){
		binding.symbol = dis64u32(data);
		binding.entry = dis64u32(data+4);
		binding.frame = dis64u32(data+8);
	}else{
		binding.module = dis64u32(data);
		binding.symbol = dis64u32(data+4);
	}
	signaturebytes = dis64u32(data+header-4);
	if(signaturebytes > bytes-header ||
	   !dis64signaturecheck(data+header, signaturebytes, parameters, &binding.signature))
		return 0;
	*result = binding;
	*used = header+signaturebytes;
	return 1;
}


/* Check IDs and export frame shape; runtime roots/call offsets are separate gates. */
static int
dis64bindingvalid(const Dis64Binding *binding, unsigned int exports,
	const DisStringPoolView *pool, const Dis64TypeTable *types,
	unsigned int instructions, unsigned int parameters)
{
	DisRuneView name;
	Dis64Type frame;
	Dis64Signature signature;

	if(binding == 0 || exports > 1 ||
	   !dis64signaturecheck(binding->signature.data, binding->signature.bytes, parameters, &signature) ||
	   !disstringpoolget(pool, binding->symbol, &name))
		return 0;
	if(!exports)
		return disstringpoolget(pool, binding->module, &name);
	return binding->entry < instructions && dis64typeget(types, binding->frame, &frame) &&
		frame.bytes >= 64 && frame.align == 8;
}


/* Reject duplicate exports and conflicting same-name imports by exact signatures. */
static int
dis64bindingpair(const Dis64Binding *left, const Dis64Binding *right,
	unsigned int exports, const DisStringPoolView *pool)
{
	int order, equal;

	if(!disstringpoolcompare(pool, left->symbol, right->symbol, &order))
		return 0;
	if(order != 0)
		return 1;
	if(exports)
		return 0;
	if(!disstringpoolcompare(pool, left->module, right->module, &order))
		return 0;
	if(order != 0)
		return 1;
	return dis64signatureequal(&left->signature, &right->signature, &equal) && equal;
}


/* Validate all scalar records and ambiguity before publishing a borrowed table. */
static int
dis64bindingscheck(const unsigned char *data, unsigned int bytes, unsigned int exports,
	const DisStringPoolView *pool, const Dis64TypeTable *types, unsigned int instructions,
	unsigned int limit, unsigned int parameters, Dis64Bindings *result)
{
	Dis64Bindings view;
	Dis64Binding current, previous;
	unsigned int i, j, at, before, used, earlier, minimum;

	if(data == 0 || result == 0 || bytes < 4 || exports > 1 || (unsigned int)-1 != 0xffffffffU)
		return 0;
	view.count = dis64u32(data);
	minimum = exports ? 28 : 24;
	if(view.count > limit || view.count > (bytes-4)/minimum)
		return 0;
	at = 4;
	for(i=0; i<view.count; i++){
		if(!dis64bindingread(data+at, bytes-at, exports, parameters, &current, &used) ||
		   !dis64bindingvalid(&current, exports, pool, types, instructions, parameters))
			return 0;
		before = 4;
		for(j=0; j<i; j++){
			if(!dis64bindingread(data+before, bytes-before, exports, parameters, &previous, &earlier) ||
			   !dis64bindingpair(&current, &previous, exports, pool))
				return 0;
			before += earlier;
		}
		at += used;
	}
	if(at != bytes)
		return 0;
	view.data = data;
	view.bytes = bytes;
	view.exports = exports;
	view.parameters = parameters;
	*result = view;
	return 1;
}


/* Resolve a previously checked ordinal without performing dependency resolution. */
static int
dis64bindingget(const Dis64Bindings *view, unsigned int index, Dis64Binding *result)
{
	Dis64Binding binding;
	unsigned int i, at, used;

	if(view == 0 || result == 0 || view->data == 0 || index >= view->count)
		return 0;
	at = 4;
	for(i=0; i<=index; i++){
		if(at > view->bytes || !dis64bindingread(view->data+at, view->bytes-at,
		   view->exports, view->parameters, &binding, &used))
			return 0;
		if(i == index){
			*result = binding;
			return 1;
		}
		at += used;
	}
	return 0;
}


/* Serialize canonical scalar binding records after full contextual preflight.
 * Inputs, maps, names and signatures remain borrowed and disjoint from output. */
static int
dis64bindingswrite(unsigned char *output, unsigned int capacity, unsigned int exports,
	const Dis64Binding *records, unsigned int count, const DisStringPoolView *pool,
	const Dis64TypeTable *types, unsigned int instructions, unsigned int parameters,
	unsigned int *written)
{
	unsigned int i, j, total, at, header, bytes;

	if(output == 0 || written == 0 || capacity < 4 || exports > 1 ||
	   (count != 0 && records == 0) || (unsigned int)-1 != 0xffffffffU)
		return 0;
	header = exports ? 16 : 12;
	if(count > (capacity-4)/(header+12))
		return 0;
	total = 4;
	for(i=0; i<count; i++){
		bytes = records[i].signature.bytes;
		if(capacity-total < header || bytes > capacity-total-header ||
		   !dis64bindingvalid(&records[i], exports, pool, types, instructions, parameters))
			return 0;
		for(j=0; j<i; j++)
			if(!dis64bindingpair(&records[i], &records[j], exports, pool))
				return 0;
		total += header+bytes;
	}
	dis64put32(output, count);
	at = 4;
	for(i=0; i<count; i++){
		if(exports){
			dis64put32(output+at, records[i].symbol);
			dis64put32(output+at+4, records[i].entry);
			dis64put32(output+at+8, records[i].frame);
		}else{
			dis64put32(output+at, records[i].module);
			dis64put32(output+at+4, records[i].symbol);
		}
		dis64put32(output+at+header-4, records[i].signature.bytes);
		at += header;
		for(j=0; j<records[i].signature.bytes; j++)
			output[at++] = records[i].signature.data[j];
	}
	*written = total;
	return 1;
}
#endif
