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
 * payloads are envelope-checked only. Success grants no code or ownership capability. */
static int
dis64storageimagecheck(const unsigned char *data, unsigned int bytes,
	const Dis64StorageLimits *limits, Dis64StorageImage *result)
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
	if(!dis64storagecode(data+section.offset, section.bytes, limits->instructions, &image.code))
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
		if((image.present&(1U<<(i-1))) == 0)
			continue;
		section = sections[i-1];
		if(!dis64bindingscheck(data+section.offset, section.bytes, i == Dis64Exports,
		   &image.strings, &image.types, image.code.count, limits->bindings,
		   limits->parameters, i == Dis64Imports ? &image.imports : &image.exports))
			return 0;
	}
	*result = image;
	return 1;
}
#endif
