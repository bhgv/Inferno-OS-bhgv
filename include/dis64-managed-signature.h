/* Candidate String/List codec. Only an explicit trial schema can select it. */
#ifndef DIS64_MANAGED_SIGNATURE_H
#define DIS64_MANAGED_SIGNATURE_H

#include "dis64-storage-image.h"
#include "dis64-bound-call.h"
#include "dis64-managed-trial.h"
#include "native64-entry.h"

typedef struct Dis64ManagedSchema Dis64ManagedSchema;
struct Dis64ManagedSchema {
	unsigned int version;
	unsigned int stringtag;
	unsigned int listtag;
};

typedef struct Dis64ManagedSignature Dis64ManagedSignature;
struct Dis64ManagedSignature {
	LimboNative64ManagedKind argument;
	LimboNative64ManagedKind result;
};

enum { Dis64ManagedSignatureBytes = 20 };

/* Keep the candidate independent of any assigned version or type tags. */
static int
dis64managedschema(const Dis64ManagedSchema *schema)
{
	return schema != 0 && schema->version > 1 && schema->stringtag != 0 &&
		schema->listtag != 0 && schema->stringtag != schema->listtag;
}

/* One child is enough for this pilot and bounds the type depth. */
static LimboNative64ManagedKind
dis64managedtype(const unsigned char *data, unsigned int bytes,
	const Dis64ManagedSchema *schema, unsigned int *used)
{
	unsigned int tag;

	if(bytes < 4)
		return 0;
	tag = dis64u32(data);
	if(tag == schema->stringtag){
		*used = 4;
		return LimboNative64String;
	}
	if(tag != schema->listtag || bytes < 8 || dis64u32(data+4) != schema->stringtag)
		return 0;
	*used = 8;
	return LimboNative64ListString;
}

/* Validate the entire record before publishing a symbolic signature. */
static int
dis64managedread(const unsigned char *data, unsigned int bytes,
	const Dis64ManagedSchema *schema, Dis64ManagedSignature *output)
{
	Dis64ManagedSignature signature;
	unsigned int resultbytes, argumentbytes;

	if(data == 0 || output == 0 || bytes != Dis64ManagedSignatureBytes ||
	   !dis64managedschema(schema) || dis64u32(data) != schema->version ||
	   dis64u32(data+4) != 1)
		return 0;
	signature.result = dis64managedtype(data+8, bytes-8, schema, &resultbytes);
	if(signature.result == 0)
		return 0;
	signature.argument = dis64managedtype(data+8+resultbytes, bytes-8-resultbytes,
		schema, &argumentbytes);
	if(signature.argument == 0 || signature.argument == signature.result ||
	   resultbytes+argumentbytes != bytes-8)
		return 0;
	*output = signature;
	return 1;
}

/* Compare the whole shape and the selected generated export before frame setup. */
static int
dis64managedmatch(const Dis64ManagedSignature *signature, const LimboNative64Managed *shape,
	const LimboNative64Entry *entry)
{
	return shape->entry == entry && signature->argument == shape->argument &&
		signature->result == shape->result;
}

/* Trial binding records use the same import/export framing as scalar records. */
static int
dis64managedbindingread(const unsigned char *data, unsigned int bytes, unsigned int exports,
	const Dis64ManagedSchema *schema, Dis64Binding *record, Dis64ManagedSignature *signature)
{
	Dis64Binding binding;
	Dis64ManagedSignature decoded;
	unsigned int used;

	if(record == 0 || signature == 0 ||
	   !dis64bindingrecord(data, bytes, exports, &binding, &used) || used != bytes ||
	   !dis64managedread(binding.signature.data, binding.signature.bytes, schema, &decoded))
		return 0;
	*record = binding;
	*signature = decoded;
	return 1;
}

/* Match a candidate import and selected export without retaining either module. */
static int
dis64managedpair(const unsigned char *imported, unsigned int importbytes,
	const unsigned char *exported, unsigned int exportbytes, const Dis64ManagedSchema *schema,
	const DisStringPoolView *consumer, const DisStringPoolView *provider, unsigned int providername,
	const LimboNative64Managed *shape, const Dis64TypeTable *types, unsigned int instructions,
	Dis64Binding *result)
{
	Dis64Binding in, out;
	Dis64ManagedSignature insig, outsig;
	DisRuneView dependency, module, symbol, candidate;
	Dis64Type frame;
	unsigned int i, rune;
	int order;

	if(result == 0 || shape == 0 || shape->entry == 0 || shape->entry->name == 0 ||
	   !dis64managedbindingread(imported, importbytes, 0, schema, &in, &insig) ||
	   !dis64managedbindingread(exported, exportbytes, 1, schema, &out, &outsig) ||
	   !dis64managedmatch(&insig, shape, shape->entry) ||
	   !dis64managedmatch(&outsig, shape, shape->entry) ||
	   !disstringpoolget(consumer, in.module, &dependency) ||
	   !disstringpoolget(provider, providername, &module) ||
	   !disrunecompare(&dependency, &module, &order) || order != 0 ||
	   !disstringpoolget(consumer, in.symbol, &symbol) ||
	   !disstringpoolget(provider, out.symbol, &candidate) ||
	   !disrunecompare(&symbol, &candidate, &order) || order != 0 ||
	   out.entry >= instructions || !dis64typeget(types, out.frame, &frame) ||
	   frame.align != 8 || frame.bytes != shape->entry->frame_size ||
	   frame.mapbytes != shape->entry->map_bytes || shape->entry->map == 0)
		return 0;
	for(i=0; i<candidate.count; i++)
		if(shape->entry->name[i] == 0 || !disruneat(&candidate, i, &rune) ||
		   rune != (unsigned char)shape->entry->name[i])
			return 0;
	if(shape->entry->name[i] != 0)
		return 0;
	for(i=0; i<frame.mapbytes; i++)
		if(frame.map[i] != shape->entry->map[i])
			return 0;
	*result = out;
	return 1;
}

/* A trial table has one managed record. The scalar accessor cannot read it. */
static int
dis64managedtable(const unsigned char *data, unsigned int bytes, unsigned int exports,
	const DisStringPoolView *pool, const Dis64TypeTable *types, unsigned int instructions,
	unsigned int limit, unsigned int parameters, Dis64Bindings *result, const void *context)
{
	Dis64Binding binding;
	Dis64ManagedSignature signature;
	DisRuneView name;
	Dis64Type frame;

	if(bytes < 4 || dis64u32(data) != 1 || limit == 0 || parameters == 0 ||
	   !dis64managedbindingread(data+4, bytes-4, exports, context, &binding, &signature) ||
	   !disstringpoolget(pool, binding.symbol, &name))
		return 0;
	if(exports){
		if(binding.entry >= instructions || !dis64typeget(types, binding.frame, &frame) ||
		   frame.bytes < 64 || frame.align != 8)
			return 0;
	}else if(!disstringpoolget(pool, binding.module, &name))
		return 0;
	*result = (Dis64Bindings){data, bytes, 1, exports, parameters};
	return 1;
}

/* Check the entire trial provider body, including its unentered first instruction. */
static int
dis64managedprovidercode(const unsigned char *data, unsigned int bytes, unsigned int limit,
	Dis64CodeView *result)
{
	Dis64CallDecoded decoded;
	unsigned int used, at;

	if(bytes < 8 || limit < 3 || dis64u32(data) != 3 ||
	   data[4] != IEXIT || data[5] != (SRC(AXXX)|DST(AXXX)) ||
	   !dis64callread(data+6, bytes-6, &decoded, &used) ||
	   (decoded.instruction.op != ICONSP && decoded.instruction.op != IHEADP) ||
	   decoded.instruction.mode != (SRC(AFP)|DST(AFP|AIND)) ||
	   decoded.instruction.source != 64 || decoded.instruction.destination != 32 ||
	   decoded.secondary != 0)
		return 0;
	at = 6+used;
	if(bytes-at != 2 || data[at] != IRET || data[at+1] != (SRC(AXXX)|DST(AXXX)))
		return 0;
	*result = (Dis64CodeView){data, bytes, 3};
	return 1;
}

/* Join complete trial images and return only the bounded one-token body. */
typedef struct Dis64ManagedBody Dis64ManagedBody;
struct Dis64ManagedBody {
	const unsigned char *data; // Borrowed instruction bytes without a count prefix.
	unsigned int bytes;
};

typedef struct Dis64ManagedPlan Dis64ManagedPlan;
struct Dis64ManagedPlan {
	Dis64StorageImage consumer;
	Dis64StorageImage provider;
	Dis64Binding selected;
};

/* Repeat exact managed call islands, clearing the old mapped result before reuse. */
static int
dis64managedcallercode(const unsigned char *data, unsigned int bytes, unsigned int limit,
	Dis64CodeView *result)
{
	static const unsigned char call[] = {
		IMFRAME, SRC(AMP)|AXIMM|DST(AFP), 0, 0, 40,
		IMOVP, SRC(AFP)|DST(AFP|AIND), 0x80,64, 40, 0x80,64,
		ILEA, SRC(AFP)|DST(AFP|AIND), 0x80,72, 40,32,
		IMCALL, SRC(AFP)|AXIMM|DST(AMP), 0, 40,0,
	};
	static const unsigned char clear[] = {IMOVP, SRC(AMP)|DST(AFP), 8, 0x80,72};
	static const unsigned char exit[] = {IEXIT, SRC(AXXX)|DST(AXXX)};
	unsigned int count, calls, at, i, j;

	if(bytes < 4)
		return 0;
	count = dis64u32(data);
	if(count < 5 || count%5 != 0 || count > limit)
		return 0;
	calls = count/5;
	if(calls > Dis64ManagedCalls || bytes != 4+calls*sizeof(call)+(calls-1)*sizeof(clear)+sizeof(exit))
		return 0;
	at = 4;
	for(i=0; i<calls; i++){
		if(i != 0)
			for(j=0; j<sizeof(clear); j++)
				if(data[at++] != clear[j])
					return 0;
		for(j=0; j<sizeof(call); j++)
			if(data[at++] != call[j])
				return 0;
	}
	for(j=0; j<sizeof(exit); j++)
		if(data[at++] != exit[j])
			return 0;
	*result = (Dis64CodeView){data, bytes, count};
	return 1;
}

static int
dis64managedimagepair(const unsigned char *consumer, unsigned int consumerbytes,
	const unsigned char *provider, unsigned int providerbytes, const Dis64StorageLimits *limits,
	const Dis64ManagedSchema *schema, const LimboNative64Managed *shape,
	Dis64ManagedPlan *result)
{
	Dis64ManagedPlan plan;
	Dis64StorageImage *in = &plan.consumer, *out = &plan.provider;
	Dis64Type callerframe;

	if(result == 0 || !dis64storageimagecheckrecords(consumer, consumerbytes, limits, in,
	   dis64managedcallercode, dis64managedtable, schema) ||
	   !dis64storageimagecheckrecords(provider, providerbytes, limits, out,
	   dis64managedprovidercode, dis64managedtable, schema) ||
	   in->module.profile != 1 || in->module.entry != 0 || out->module.profile != 0 ||
	   in->data.bytes != (in->code.count == 5 ? 8 : 16) || in->data.align != 8 ||
	   in->data.mapbytes != 1 || in->data.map[0] != (in->code.count == 5 ? 0x80 : 0xc0) ||
	   (in->present&(1U<<(Dis64Data-1))) != 0 ||
	   !dis64typeget(&in->types, in->module.frametype, &callerframe) ||
	   callerframe.bytes != 80 || callerframe.mapbytes != 2 ||
	   callerframe.map[0] != 0 || callerframe.map[1] != 0xc0 ||
	   out->module.databytes != 0 || (out->present&(1U<<(Dis64Data-1))) != 0 ||
	   in->imports.count != 1 || in->exports.count != 0 ||
	   out->imports.count != 0 || out->exports.count != 1)
		return 0;
	if(!dis64managedpair(in->imports.data+4, in->imports.bytes-4,
		out->exports.data+4, out->exports.bytes-4, schema,
		&in->strings, &out->strings, out->module.name, shape, &out->types, out->code.count, &plan.selected) ||
	   plan.selected.entry != 1 ||
	   out->code.data[6] != (shape->result == LimboNative64ListString ? ICONSP : IHEADP))
		return 0;
	*result = plan;
	return 1;
}

#endif
