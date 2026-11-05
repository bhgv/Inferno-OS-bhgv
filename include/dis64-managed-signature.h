/* Candidate String/List codec. Only an explicit trial schema can select it. */
#ifndef DIS64_MANAGED_SIGNATURE_H
#define DIS64_MANAGED_SIGNATURE_H

#include "dis64-bindings.h"
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

#endif
