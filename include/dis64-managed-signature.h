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

/* Read one private record without routing managed tags through the scalar accessor. */
static int
dis64managedget(const Dis64Bindings *table, unsigned int index, const Dis64ManagedSchema *schema,
	Dis64Binding *binding, Dis64ManagedSignature *signature)
{
	Dis64Binding record;
	unsigned int i, at, used;

	if(table == 0 || index >= table->count)
		return 0;
	at = 4;
	for(i=0; i<=index; i++){
		if(at > table->bytes || !dis64bindingrecord(table->data+at, table->bytes-at, table->exports, &record, &used))
			return 0;
		if(i == index)
			return dis64managedbindingread(table->data+at, used, table->exports, schema, binding, signature);
		at += used;
	}
	return 0;
}

/* A trial table has one or two unique managed records. */
static int
dis64managedtable(const unsigned char *data, unsigned int bytes, unsigned int exports,
	const DisStringPoolView *pool, const Dis64TypeTable *types, unsigned int instructions,
	unsigned int limit, unsigned int parameters, Dis64Bindings *result, const void *context)
{
	Dis64Binding binding, previous;
	Dis64ManagedSignature signature;
	DisRuneView name;
	Dis64Type frame;
	unsigned int count, at, i, j, used;
	int order;

	if(bytes < 4 || parameters == 0 || exports > 1)
		return 0;
	count = dis64u32(data);
	if(count == 0 || count > 2 || count > limit)
		return 0;
	at = 4;
	for(i=0; i<count; i++){
		if(at > bytes || !dis64bindingrecord(data+at, bytes-at, exports, &binding, &used) ||
		   !dis64managedbindingread(data+at, used, exports, context, &binding, &signature) ||
		   !disstringpoolget(pool, binding.symbol, &name))
			return 0;
		if(exports){
			if(binding.entry >= instructions || !dis64typeget(types, binding.frame, &frame) || frame.bytes < 64 || frame.align != 8)
				return 0;
		}else if(!disstringpoolget(pool, binding.module, &name))
			return 0;
		for(j=0; j<i; j++){
			Dis64Bindings prefix = {data, at, i, exports, parameters};
			if(!dis64managedget(&prefix, j, context, &previous, &signature) ||
			   !disstringpoolcompare(pool, binding.symbol, previous.symbol, &order))
				return 0;
			if(order == 0){
				if(exports || !disstringpoolcompare(pool, binding.module, previous.module, &order) || order == 0)
					return 0;
			}
		}
		at += used;
	}
	if(at != bytes)
		return 0;
	*result = (Dis64Bindings){data, bytes, count, exports, parameters};
	return 1;
}

/* Check one or both one-token provider bodies, including the unentered prefix. */
static int
dis64managedprovidercode(const unsigned char *data, unsigned int bytes, unsigned int limit,
	Dis64CodeView *result)
{
	Dis64CallDecoded decoded;
	unsigned int used, at, count, i, seen, bit;

	if(bytes < 8 || data[4] != IEXIT || data[5] != (SRC(AXXX)|DST(AXXX)))
		return 0;
	count = dis64u32(data);
	if((count != 3 && count != 5) || count > limit)
		return 0;
	at = 6;
	seen = 0;
	for(i=1; i<count; i+=2){
		if(at > bytes || !dis64callread(data+at, bytes-at, &decoded, &used) ||
		   (decoded.instruction.op != ICONSP && decoded.instruction.op != IHEADP) ||
		   decoded.instruction.mode != (SRC(AFP)|DST(AFP|AIND)) || decoded.instruction.source != 64 ||
		   decoded.instruction.destination != 32 || decoded.secondary != 0)
			return 0;
		bit = decoded.instruction.op == ICONSP ? 1 : 2;
		if(seen & bit)
			return 0;
		seen |= bit;
		at += used;
		if(bytes-at < 2 || data[at] != IRET || data[at+1] != (SRC(AXXX)|DST(AXXX)))
			return 0;
		at += 2;
	}
	if(at != bytes)
		return 0;
	*result = (Dis64CodeView){data, bytes, count};
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
	Dis64Binding selected; // Compatibility view of matches[0].
	Dis64Binding matches[2];
	unsigned int shapes[2], count, initial;
};

/* Consume a complete fixed instruction island without reading beyond the code extent. */
static int
dis64managedbytes(const unsigned char *data, unsigned int bytes, unsigned int *at,
	const unsigned char *expected, unsigned int count)
{
	unsigned int i;

	if(*at > bytes || count > bytes-*at)
		return 0;
	for(i=0; i<count; i++)
		if(data[*at+i] != expected[i])
			return 0;
	*at += count;
	return 1;
}

/* This is layout classification only; callers still require complete image/code proof. */
static int
dis64managedfilecode(const Dis64CodeView *code)
{
	return code != 0 && code->data != 0 && code->bytes >= 5 && code->data[4] == ILOAD;
}

/* Repeat checked calls and optional file reloads; clear the old result before reuse. */
static int
dis64managedcallercode(const unsigned char *data, unsigned int bytes, unsigned int limit,
	Dis64CodeView *result)
{
	static const unsigned char pattern[] = {
		IMFRAME, SRC(AMP)|AXIMM|DST(AFP), 0, 0, 40,
		IMOVP, SRC(AFP)|DST(AFP|AIND), 0x80,64, 40, 0x80,64,
		ILEA, SRC(AFP)|DST(AFP|AIND), 0x80,72, 40,32,
		IMCALL, SRC(AFP)|AXIMM|DST(AMP), 0, 40,0,
	};
	static const unsigned char transfer[] = {IMOVP,SRC(AFP)|DST(AFP),0x80,72,0x80,64};
	static const unsigned char clear[] = {IMOVP, SRC(AMP)|DST(AFP), 8, 0x80,72};
	static const unsigned char empty[] = {IMOVP, SRC(AMP)|DST(AFP), 8, 0x80,64};
	static const unsigned char load[] = {ILOAD,SRC(AMP)|AXIMM|DST(AMP),0,16,0};
	static const unsigned char exit[] = {IEXIT,SRC(AXXX)|DST(AXXX)};
	unsigned char call[sizeof(pattern)];
	unsigned int count, calls, at, instructions, fileload, i, binding, used;
	Dis64CallDecoded literal;

	if(bytes < 5)
		return 0;
	count = dis64u32(data);
	if(count < 5 || count > limit || count > 7*Dis64ManagedCalls)
		return 0;
	at = 4;
	calls = instructions = 0;
	fileload = data[at] == ILOAD;
	if(fileload){
		if(!dis64managedbytes(data, bytes, &at, load, sizeof(load)))
			return 0;
		instructions++;
	}
	for(;;){
		if(at < bytes && data[at] == IMOVP){
			if(fileload && dis64callread(data+at,bytes-at,&literal,&used) &&
			   literal.instruction.mode == (SRC(AMP)|DST(AFP)) && literal.instruction.destination == 64 &&
			   literal.instruction.source >= 24 && literal.instruction.source%8 == 0 &&
			   literal.instruction.source < 24+8*Dis64ManagedLiterals) at += used;
			else if(!dis64managedbytes(data, bytes, &at, empty, sizeof(empty))) return 0;
			instructions++;
		}
		if(calls == Dis64ManagedCalls || bytes-at < 3 || data[at+2] > 1)
			return 0;
		binding = data[at+2];
		for(i=0; i<sizeof(pattern); i++)
			call[i] = pattern[i];
		call[2] = call[sizeof(call)-3] = binding;
		if(!dis64managedbytes(data, bytes, &at, call, sizeof(call)))
			return 0;
		calls++;
		instructions += 4;
		if(at < bytes && data[at] == IEXIT)
			break;
		if(at < bytes && data[at] == IMOVP && bytes-at >= 2 && data[at+1] == (SRC(AFP)|DST(AFP))){
			if(!dis64managedbytes(data, bytes, &at, transfer, sizeof(transfer)))
				return 0;
			instructions++;
		}
		if(!dis64managedbytes(data, bytes, &at, clear, sizeof(clear)))
			return 0;
		instructions++;
		if(fileload && at < bytes && data[at] == ILOAD){
			if(!dis64managedbytes(data, bytes, &at, load, sizeof(load)))
				return 0;
			instructions++;
		}
	}
	if(!dis64managedbytes(data, bytes, &at, exit, sizeof(exit)) || at != bytes || instructions+1 != count)
		return 0;
	*result = (Dis64CodeView){data, bytes, count};
	return 1;
}

/* Validate every exported body against a registered generated managed shape. */
static int
dis64managedexports(const Dis64StorageImage *provider, const Dis64ManagedSchema *schema,
	const LimboNative64Managed *shapes, unsigned int count, Dis64Binding *exports, unsigned int *ids)
{
	Dis64ManagedSignature signature;
	DisRuneView name;
	unsigned char imported[12+Dis64ManagedSignatureBytes];
	unsigned int i, j, at, rune, seen = 0;

	if(shapes == 0 || exports == 0 || ids == 0 || count == 0 || count > 2 || provider->module.profile != 0 || provider->module.databytes != 0 ||
	   (provider->present&(1U<<(Dis64Data-1))) != 0 || provider->imports.count != 0 ||
	   provider->exports.count == 0 || provider->exports.count > count ||
	   provider->exports.count != (provider->code.count-1)/2 ||
	   !disstringpoolget(&provider->strings, provider->module.name, &name) || name.count == 0 || name.count > 255)
		return 0;
	for(i=0; i<name.count; i++)
		if(!disruneat(&name, i, &rune) || rune == 0 || rune > 127)
			return 0;
	for(i=0; i<provider->exports.count; i++){
		if(!dis64managedget(&provider->exports, i, schema, &exports[i], &signature))
			return 0;
		dis64put32(imported, provider->module.name);
		dis64put32(imported+4, exports[i].symbol);
		dis64put32(imported+8, Dis64ManagedSignatureBytes);
		for(j=0; j<Dis64ManagedSignatureBytes; j++)
			imported[12+j] = exports[i].signature.data[j];
		for(j=0; j<count; j++)
			if(dis64managedpair(imported, sizeof(imported), exports[i].signature.data-16,
			   16+exports[i].signature.bytes, schema, &provider->strings, &provider->strings,
			   provider->module.name, &shapes[j], &provider->types, provider->code.count, &exports[i]))
				break;
		if(j == count || exports[i].entry < 1 || (exports[i].entry&1) == 0 ||
		   exports[i].entry >= provider->code.count || (seen & (1U<<j)))
			return 0;
		at = 6+4*(exports[i].entry-1);
		if(at >= provider->code.bytes || provider->code.data[at] !=
		   (shapes[j].result == LimboNative64ListString ? ICONSP : IHEADP))
			return 0;
		seen |= 1U<<j;
		ids[i] = j;
	}
	return 1;
}

/* Prove type flow through the two mapped caller cells, including result-to-argument transfer. */
static int
dis64managedflow(const Dis64CodeView *code, const LimboNative64Managed *shapes,
	const unsigned int *ids, unsigned int bindings, unsigned int *initial)
{
	Dis64CallDecoded decoded;
	Dis64Instruction *inst;
	unsigned int at = 4, used, i, current = 0, latest = 0, empty = 0;

	*initial = 0;
	for(i=0; i<code->count; i++){
		if(at > code->bytes || !dis64callread(code->data+at, code->bytes-at, &decoded, &used))
			return 0;
		at += used;
		inst = &decoded.instruction;
		if(inst->op == IMFRAME || inst->op == IMCALL){
			if(inst->middle < 0 || inst->middle >= bindings)
				return 0;
			if(inst->op == IMFRAME){
				if(*initial == 0) *initial = shapes[ids[inst->middle]].argument;
				if(current == 0)
					*initial = current = shapes[ids[inst->middle]].argument;
				if(empty)
					current = shapes[ids[inst->middle]].argument;
				empty = 0;
				if(current != shapes[ids[inst->middle]].argument)
					return 0;
			}else
				latest = shapes[ids[inst->middle]].result;
		}else if(inst->op == IMOVP && inst->mode == (SRC(AFP)|DST(AFP))){
			if(latest == 0)
				return 0;
			current = latest;
		}else if(inst->op == IMOVP && inst->mode == (SRC(AMP)|DST(AFP))){
			if(inst->destination == 64){
				if(inst->source >= 24){ current = LimboNative64String; empty = 0; }
				else empty = 1;
			}
			else
				latest = 0;
		}
	}
	return at == code->bytes && current != 0;
}

typedef struct Dis64ManagedLiteralsView Dis64ManagedLiteralsView;
struct Dis64ManagedLiteralsView {
    unsigned int count; // Number of independently mapped literal roots.
    DisRuneView values[Dis64ManagedLiterals]; // Borrowed only during construction.
};

/* Validate every MP/pool root and every literal use before publishing a view.
 * All roots must be referenced, and aggregate/individual Rune limits are independent. */
static int
dis64managedliteralview(const Dis64StorageImage *image, Dis64ManagedLiteralsView *result)
{
    Dis64ManagedLiteralsView view = {0};
    Dis64CallDecoded decoded;
    unsigned int at = 4, used, i, j, runes = 0, map, found = 0, expected;
    long source;
    if(dis64managedfilecode(&image->code)) {
        if(image->data.bytes < 24 || image->data.bytes%8 ||
           image->data.bytes > 24+8*Dis64ManagedLiterals) return 0;
        view.count = (image->data.bytes-24)/8;
        if(image->data.mapbytes != (3+view.count+7)/8) return 0;
        for(i=0; i<image->data.mapbytes; i++) {
            map = 0;
            for(j=0; j<8 && i*8+j<3+view.count; j++) map |= 0x80U>>j;
            if(image->data.map[i] != map) return 0;
        }
        for(i=0; i<view.count; i++) {
            if(!disstringpoolget(&image->strings,3+image->imports.count+i,&view.values[i]) ||
               view.values[i].count > 255) return 0;
            runes += view.values[i].count;
        }
        if(runes > Dis64ManagedLiteralRunes) return 0;
    }
    for(i=0; i<image->code.count; i++) {
        if(at > image->code.bytes || !dis64callread(image->code.data+at,image->code.bytes-at,&decoded,&used)) return 0;
        at += used;
        if(decoded.instruction.op == IMOVP && decoded.instruction.mode == (SRC(AMP)|DST(AFP)) &&
           (decoded.instruction.destination == 64 || decoded.instruction.destination == 72) && decoded.instruction.source >= 24) {
            source = decoded.instruction.source;
            if(source%8 || (source-24)/8 >= view.count) return 0;
            found |= 1U<<((source-24)/8);
        }
    }
    expected = view.count == 32 ? 0xffffffffU : (1U<<view.count)-1;
    if(found != expected || at != image->code.bytes) return 0;
    *result = view;
    return 1;
}

static int
dis64managedimagegroup(const unsigned char *consumer, unsigned int consumerbytes,
	const unsigned char *provider, unsigned int providerbytes, const Dis64StorageLimits *limits,
	const Dis64ManagedSchema *schema, const LimboNative64Managed *shapes, unsigned int count,
	Dis64ManagedPlan *result)
{
	Dis64ManagedPlan plan = {0};
	Dis64StorageImage *in = &plan.consumer, *out = &plan.provider;
	Dis64Type callerframe;
	Dis64Binding exported[2], imported;
	Dis64ManagedSignature signature;
	Dis64ManagedLiteralsView literals;
	unsigned int ids[2], i, j, found;

	if(result == 0 || shapes == 0 || count == 0 || count > 2 || !dis64storageimagecheckrecords(consumer, consumerbytes, limits, in,
	   dis64managedcallercode, dis64managedtable, schema) ||
	   !dis64storageimagecheckrecords(provider, providerbytes, limits, out,
	   dis64managedprovidercode, dis64managedtable, schema) ||
	   in->module.profile != 1 || in->module.entry != 0 || in->imports.count == 0 || in->imports.count > count ||
	   (!dis64managedfilecode(&in->code) && in->data.bytes != (in->code.count == 5 ? 8 : 16)) || in->data.align != 8 ||
	   (!dis64managedfilecode(&in->code) && (in->data.mapbytes != 1 || in->data.map[0] != (in->code.count == 5 ? 0x80 : 0xc0))) ||
	   (in->present&(1U<<(Dis64Data-1))) != 0 ||
	   !dis64typeget(&in->types, in->module.frametype, &callerframe) || callerframe.bytes != 80 ||
	   callerframe.mapbytes != 2 || callerframe.map[0] != 0 || callerframe.map[1] != 0xc0 || in->exports.count != 0 ||
	   !dis64managedexports(out, schema, shapes, count, exported, ids) || !dis64managedliteralview(in,&literals))
		return 0;
	plan.count = in->imports.count;
	for(i=0; i<plan.count; i++){
		if(!dis64managedget(&in->imports, i, schema, &imported, &signature))
			return 0;
		found = 0;
		for(j=0; j<out->exports.count; j++)
			if(dis64managedpair(imported.signature.data-12, 12+imported.signature.bytes,
			   exported[j].signature.data-16, 16+exported[j].signature.bytes, schema,
			   &in->strings, &out->strings, out->module.name, &shapes[ids[j]], &out->types,
			   out->code.count, &plan.matches[i])){
				plan.shapes[i] = ids[j];
				found++;
			}
		if(found != 1)
			return 0;
	}
	if(!dis64managedflow(&in->code, shapes, plan.shapes, plan.count, &plan.initial))
		return 0;
	plan.selected = plan.matches[0];
	*result = plan;
	return 1;
}

/* Compatibility entry point for an explicitly selected single signature. */
static int
dis64managedimagepair(const unsigned char *consumer, unsigned int consumerbytes,
	const unsigned char *provider, unsigned int providerbytes, const Dis64StorageLimits *limits,
	const Dis64ManagedSchema *schema, const LimboNative64Managed *shape, Dis64ManagedPlan *result)
{
	return dis64managedimagegroup(consumer, consumerbytes, provider, providerbytes, limits, schema, shape, 1, result);
}

#endif
