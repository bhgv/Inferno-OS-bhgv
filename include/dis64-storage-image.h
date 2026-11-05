/* Joined structural storage validation; include isa.h once before this header. */
#ifndef DIS64_STORAGE_IMAGE_H
#define DIS64_STORAGE_IMAGE_H // Immutable module storage inspection, never runtime admission.

#include "dis64-module.h"
#include "dis64-bindings.h"
#include "dis64-data.h"
#include "dis64-code.h"

typedef struct Dis64StorageLimits Dis64StorageLimits;

struct Dis64StorageLimits {
	unsigned int sections; // Caller resource policy for directory population.
	unsigned int types; // Caller resource policy, including every unused descriptor.
	unsigned int objectbytes; // Largest allowed declared object/frame/data extent.
	unsigned int strings; // Caller resource policy for actual Rune pool records.
	unsigned int instructions; // Caller resource policy for direct-mode framing only.
	unsigned int bindings; // Caller resource policy for each quadratic ambiguity check.
	unsigned int parameters; // Caller resource policy for each scalar signature.
	unsigned int initializers; // Caller resource policy for quadratic DATA overlap checks.
};

typedef struct Dis64StorageImage Dis64StorageImage;

struct Dis64StorageImage {
	Dis64View envelope; // Borrowed complete immutable source identity.
	Dis64ModuleInfo module; // Metadata linked to the same image's names/types/code extent.
	Dis64TypeTable types; // Every descriptor checked, including unused managed descriptors.
	DisStringPoolView strings; // Exact ordinal names, without content normalization.
	Dis64CodeView code; // Direct-mode instruction framing, not opcode execution proof.
	Dis64Type data; // Actual declared module-data type, or canonical empty descriptor.
	Dis64Initializers initializers; // Present DATA records checked against that exact type.
	Dis64Bindings imports; // Local structural interface records, not resolved dependencies.
	Dis64Bindings exports; // Local structural export names, PCs, frame shapes and signatures.
	unsigned int present; // Local bitset of known sections; not a shared wire feature mask.
};


/* Frame every instruction before trusting the count used by metadata and exports.
 * Direct modes only; role/type/init/control-flow/call safety is a separate gate. */
static int
dis64storagecode(const unsigned char *data, unsigned int bytes, unsigned int limit,
	Dis64CodeView *result)
{
	Dis64CodeView view;
	Dis64Instruction instruction;
	unsigned int i, at, used;

	if(data == 0 || result == 0 || bytes < 4)
		return 0;
	view.count = dis64u32(data);
	if(view.count > limit || view.count > (bytes-4)/2)
		return 0;
	at = 4;
	for(i=0; i<view.count; i++){
		if(!dis64instructionread(data+at, bytes-at, &instruction, &used) || instruction.op >= MAXDIS)
			return 0;
		at += used;
	}
	if(at != bytes)
		return 0;
	view.data = data;
	view.bytes = bytes;
	*result = view;
	return 1;
}


/* Inspect one immutable image before any Type/Module/root/cache/native allocation.
 * Missing optional DATA/import/export tables remain explicitly absent, not synthesized.
 * CONTROL semantics and mandatory extensions remain unsupported. Unknown optional
 * payloads are envelope-checked only. Success grants no code or ownership capability.
 * Source/policy/result occupy independent disjoint live mapped storage. Every
 * borrowed source span remains immutable and mapped for the complete view lifetime;
 * aggregate publication does not provide alias handling or acquire source lifetime. */
typedef int (*Dis64StorageBindingCheck)(const unsigned char*, unsigned int, unsigned int,
	const DisStringPoolView*, const Dis64TypeTable*, unsigned int, unsigned int,
	unsigned int, Dis64Bindings*, const void*);

static int
dis64storageimagecheckrecords(const unsigned char *data, unsigned int bytes,
	const Dis64StorageLimits *limits, Dis64StorageImage *result,
	int (*checkcode)(const unsigned char*, unsigned int, unsigned int, Dis64CodeView*),
	Dis64StorageBindingCheck checkbindings, const void *context)
{
	Dis64StorageImage image = {0};
	Dis64Section sections[8] = {{0}}, section;
	Dis64Type frame;
	DisRuneView name;
	unsigned int i;

	if(limits == 0 || result == 0 ||
	   !dis64check(data, bytes, 0, limits->sections, 0, 0, &image.envelope))
		return 0;
	for(i=0; i<image.envelope.sections; i++){
		if(!dis64section(&image.envelope, i, &section))
			return 0;
		if(section.kind >= Dis64Module && section.kind <= Dis64Control){
			sections[section.kind-1] = section;
			image.present |= 1U<<(section.kind-1);
		}
	}
	if((image.present&(1U<<(Dis64Control-1))) != 0 ||
	   (image.present&(1U<<(Dis64Strings-1))) == 0)
		return 0;
	section = sections[Dis64Module-1];
	if(!dis64modulecheck(data+section.offset, section.bytes, &image.module) ||
	   image.module.databytes > limits->objectbytes)
		return 0;
	section = sections[Dis64Types-1];
	if(!dis64typescheck(data+section.offset, section.bytes, limits->types,
	   limits->objectbytes, &image.types))
		return 0;
	section = sections[Dis64Strings-1];
	if(!disstringpoolcheck(data+section.offset, section.bytes, limits->strings, &image.strings) ||
	   !disstringpoolget(&image.strings, image.module.name, &name))
		return 0;
	section = sections[Dis64Code-1];
	if(!checkcode(data+section.offset, section.bytes, limits->instructions, &image.code))
		return 0;
	if(image.module.profile == 1 &&
	   (image.module.entry >= image.code.count ||
	   !dis64typeget(&image.types, image.module.frametype, &frame) ||
	   frame.bytes < 64 || frame.align != 8))
		return 0;
	image.data.align = 1;
	if(image.module.databytes != 0 &&
	   (!dis64typeget(&image.types, image.module.datatype, &image.data) ||
	   image.data.bytes != image.module.databytes))
		return 0;
	if((image.present&(1U<<(Dis64Data-1))) != 0){
		section = sections[Dis64Data-1];
		if(!dis64initializerscheck(data+section.offset, section.bytes, &image.data,
		   limits->initializers, &image.initializers))
			return 0;
	}
	for(i=Dis64Imports; i<=Dis64Exports; i++){
		Dis64Bindings *bindings;

		if((image.present&(1U<<(i-1))) == 0)
			continue;
		section = sections[i-1];
		bindings = i == Dis64Imports ? &image.imports : &image.exports;
		if(checkbindings != 0){
			if(!checkbindings(data+section.offset, section.bytes, i == Dis64Exports,
			   &image.strings, &image.types, image.code.count, limits->bindings,
			   limits->parameters, bindings, context))
				return 0;
		}else if(!dis64bindingscheck(data+section.offset, section.bytes, i == Dis64Exports,
		   &image.strings, &image.types, image.code.count, limits->bindings,
		   limits->parameters, bindings))
			return 0;
	}
	*result = image;
	return 1;
}

static int
dis64storageimagecheckcode(const unsigned char *data, unsigned int bytes,
	const Dis64StorageLimits *limits, Dis64StorageImage *result,
	int (*checkcode)(const unsigned char*, unsigned int, unsigned int, Dis64CodeView*))
{
	return dis64storageimagecheckrecords(data, bytes, limits, result, checkcode, 0, 0);
}


/* Keep ordinary storage admission restricted to the direct decoder. */
static int
dis64storageimagecheck(const unsigned char *data, unsigned int bytes,
	const Dis64StorageLimits *limits, Dis64StorageImage *result)
{
	return dis64storageimagecheckcode(data, bytes, limits, result, dis64storagecode);
}


typedef struct Dis64ImportMatch Dis64ImportMatch;

struct Dis64ImportMatch {
	unsigned int ordinal; // Export ordinal in the selected provider.
	unsigned int entry; // Provider Dis-PC.
	unsigned int frame; // Provider frame descriptor ordinal.
};


/* Match one import against an explicitly selected provider by exact Rune contents.
 * Both images must be immutable results of dis64storageimagecheck. Output is
 * independent of both images and is written only on success. The result gives
 * metadata coordinates for later linking and carries no native pointer or retain. */
static int
dis64importmatch(const Dis64StorageImage *consumer, unsigned int ordinal,
	const Dis64StorageImage *provider, Dis64ImportMatch *result)
{
	Dis64Binding imported, exported;
	DisRuneView dependency, module, symbol, candidate;
	const Dis64Bindings *exports;
	unsigned int i, at, used;
	int order, equal;

	if(consumer == 0 || provider == 0 || result == 0 ||
	   !dis64bindingget(&consumer->imports, ordinal, &imported) ||
	   !disstringpoolget(&consumer->strings, imported.module, &dependency) ||
	   !disstringpoolget(&provider->strings, provider->module.name, &module) ||
	   !disrunecompare(&dependency, &module, &order) || order != 0 ||
	   !disstringpoolget(&consumer->strings, imported.symbol, &symbol))
		return 0;
	exports = &provider->exports;
	at = 4;
	for(i=0; i<exports->count; i++){
		if(!dis64bindingread(exports->data+at, exports->bytes-at, 1,
		   exports->parameters, &exported, &used) ||
		   !disstringpoolget(&provider->strings, exported.symbol, &candidate) ||
		   !disrunecompare(&symbol, &candidate, &order))
			return 0;
		at += used;
		if(order != 0)
			continue;
		if(!dis64signatureequal(&imported.signature, &exported.signature, &equal) || !equal)
			return 0;
		result->ordinal = i;
		result->entry = exported.entry;
		result->frame = exported.frame;
		return 1;
	}
	return 0;
}
#endif
