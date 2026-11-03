/* Private whole-image composition gate, not executable file-loader admission. */
#ifndef DIS64_CALL_IMAGE_H
#define DIS64_CALL_IMAGE_H // Bind sections to one immutable container before private call proof.

#include "dis64-bound-call.h"
#include "dis64-module.h"

#define Dis64CallImageBytes (4U*Dis64LocalBytes+4096U) // Explicit bounded image policy, not a new wire limit.

typedef struct Dis64CallImage Dis64CallImage;

struct Dis64CallImage {
	Dis64View envelope; // Borrowed complete immutable container identity.
	Dis64ModuleInfo module; // Executable, zero-data metadata from this same image.
	Dis64TypeTable types; // Borrowed checked descriptors inside this image.
	const unsigned char *code; // Borrowed complete code payload, never native pointer bytes.
	unsigned int instructions; // Exact decoded instruction population.
	unsigned int functions; // Atomic count of complete caller/leaf proof records.
};


/* Bind all structural sections to one immutable image before private semantic analysis.
 * The returned borrowed view is not an execution capability or a Module owner. */
static int
dis64callimageopen(const unsigned char *data, unsigned int bytes, unsigned int objectlimit,
	Dis64CallImage *result)
{
	Dis64CallImage image = {0};
	Dis64Section module = {0}, code = {0}, types = {0}, exports = {0}, strings = {0}, section;
	Dis64Type type;
	DisRuneView name;
	DisStringPoolView pool;
	unsigned int i, j, seen, bit;

	if(result == 0 || bytes > Dis64CallImageBytes ||
	   !dis64check(data, bytes, 0, 5, 0, 0, &image.envelope) || image.envelope.sections != 5)
		return 0;
	seen = 0;
	for(i=0; i<image.envelope.sections; i++){
		if(!dis64section(&image.envelope, i, &section))
			return 0;
		bit = 0;
		switch(section.kind){
		case Dis64Module: module = section; bit = 1; break;
		case Dis64Code: code = section; bit = 2; break;
		case Dis64Types: types = section; bit = 4; break;
		case Dis64Exports: exports = section; bit = 8; break;
		case Dis64Strings: strings = section; bit = 16; break;
		default: return 0;
		}
		if(seen&bit)
			return 0;
		seen |= bit;
	}
	if(seen != 31 || !dis64modulecheck(data+module.offset, module.bytes, &image.module) ||
	   image.module.profile != 1 || image.module.databytes != 0 ||
	   !dis64typescheck(data+types.offset, types.bytes, Dis64LocalFunctions, objectlimit, &image.types) ||
	   !disstringpoolcheck(data+strings.offset, strings.bytes, 65536, &pool) ||
	   !disstringpoolget(&pool, image.module.name, &name) || code.bytes < 4)
		return 0;
	/* Even unused descriptors must remain scalar: the private constructor supplies np0 maps. */
	for(i=0; i<image.types.count; i++){
		if(!dis64typeget(&image.types, i, &type))
			return 0;
		for(j=0; j<type.mapbytes; j++)
			if(type.map[j] != 0)
				return 0;
	}
	image.code = data+code.offset;
	image.instructions = dis64u32(image.code);
	*result = image;
	return 1;
}


/* Caller supplies independent workspace/output and trusted immutable compiler declarations.
 * Declarations are not authenticated or inferred from arbitrary file metadata here.
 * No runtime Module, C-step permission, cache capability or file admission is published. */
static int
dis64callimagecheck(const unsigned char *data, unsigned int bytes, unsigned int objectlimit,
	const Dis64FunctionLocals *locals, unsigned int localcount,
	Dis64CallDecoded *decoded, Dis64InitWork *scratch, unsigned int *queue,
	unsigned int workcapacity, unsigned int limit, Dis64BoundCall *output,
	unsigned int capacity, unsigned int *written, Dis64CallImage *result)
{
	Dis64CallImage image;
	Dis64Section code = {0}, exports = {0}, strings = {0}, types = {0}, section;
	unsigned int i;

	if(result == 0 || written == 0 || !dis64callimageopen(data, bytes, objectlimit, &image))
		return 0;
	for(i=0; i<image.envelope.sections; i++){
		if(!dis64section(&image.envelope, i, &section))
			return 0;
		switch(section.kind){
		case Dis64Code: code = section; break;
		case Dis64Types: types = section; break;
		case Dis64Exports: exports = section; break;
		case Dis64Strings: strings = section; break;
		}
	}
	/* The underlying gate atomically publishes records/count only on complete proof. */
	if(!dis64boundcallcheck(image.code, code.bytes, data+exports.offset, exports.bytes,
	   data+strings.offset, strings.bytes, data+types.offset, types.bytes, objectlimit,
	   image.module.entry, image.module.frametype, locals, localcount,
	   decoded, scratch, queue, workcapacity, limit, output, capacity, written))
		return 0;
	image.functions = *written;
	*result = image;
	return 1;
}
#endif
