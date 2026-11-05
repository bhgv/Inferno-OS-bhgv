/* Candidate String/List codec. Only an explicit trial schema can select it. */
#ifndef DIS64_MANAGED_SIGNATURE_H
#define DIS64_MANAGED_SIGNATURE_H

#include "dis64.h"
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

#endif
