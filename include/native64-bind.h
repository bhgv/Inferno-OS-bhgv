/* Private generated-C binding checks; no runtime registration or wire admission. */
#ifndef NATIVE64_BIND_H
#define NATIVE64_BIND_H // Header-only opt-in record matcher.

#ifndef LIMBO_NATIVE64_ENTRY_TYPE
#error include an explicit native64 header with entries before native64-bind.h
#endif

#include "dis-layout.h"

/* Private resource bounds match the declaration emitter, not a wire registry. */
enum {
	Native64BindEntries = 4096, // Maximum records in either borrowed table.
	Native64BindName = 255, // Maximum module-relative symbol bytes.
	Native64BindFrame = 1024*1024, // Maximum generated fixed-frame extent.
};


/* Bound a readable caller-owned C symbol before any table comparison. */
static int
native64bindname(const char *name)
{
	unsigned int i;

	if(name == 0 || name[0] == 0)
		return 0;
	for(i=0; i<=Native64BindName; i++)
		if(name[i] == 0)
			return 1;
	return 0;
}


/* Compare already validated immutable source symbols without locale or normalization. */
static int
native64bindsame(const char *left, const char *right)
{
	unsigned int i;

	for(i=0; left[i] != 0; i++)
		if(left[i] != right[i])
			return 0;
	return right[i] == 0;
}


/* Validate complete map8 and protect Frame64 links/result/temp bytes from argument roots. */
static int
native64bindrecord(const LimboNative64Entry *record, int implementation)
{
	DisAbi abi;

	if(record == 0 || !native64bindname(record->name) ||
	   (implementation ? record->function == 0 : record->function != 0) ||
	   record->frame_size < 64 || record->frame_size > Native64BindFrame ||
	   record->frame_size%8 != 0 || record->map == 0 ||
	   record->map_bytes != (record->frame_size+63)/64 || record->map[0] != 0)
		return 0;
	return disabinit(&abi, 8) && disabmap(&abi, record->frame_size, record->map, record->map_bytes);
}


/* Match trusted generated specifications to selected implementations before publishing any output.
 * Names, maps and records must remain readable/immutable for this call and the borrowed result.
 * output storage must be disjoint from the input tables; this does not authenticate wire data.
 * A legacy hash match is consistency evidence, not structural callable-signature acceptance. */
static int
native64bind(const LimboNative64Entry *specs, unsigned int count,
	const LimboNative64Entry *const *implementations, unsigned int selected,
	const LimboNative64Entry **output, unsigned int capacity)
{
	const LimboNative64Entry *entry, *spec;
	unsigned int i, j, byte;

	if(sizeof(UWORD) != 4 || sizeof(void*) != 8 || count > Native64BindEntries ||
	   selected > Native64BindEntries || selected > capacity ||
	   (count != 0 && specs == 0) ||
	   (selected != 0 && (implementations == 0 || output == 0)))
		return 0;
	for(i=0; i<count; i++){
		if(!native64bindrecord(specs+i, 0))
			return 0;
		for(j=0; j<i; j++)
			if(native64bindsame(specs[i].name, specs[j].name))
				return 0;
	}
	for(i=0; i<selected; i++){
		entry = implementations[i];
		if(!native64bindrecord(entry, 1))
			return 0;
		for(j=0; j<i; j++)
			if(native64bindsame(entry->name, implementations[j]->name))
				return 0;
		for(j=0; j<count; j++)
			if(native64bindsame(entry->name, specs[j].name))
				break;
		if(j == count)
			return 0;
		spec = specs+j;
		if(entry->legacy_signature != spec->legacy_signature ||
		   entry->frame_size != spec->frame_size || entry->map_bytes != spec->map_bytes)
			return 0;
		for(byte=0; byte<spec->map_bytes; byte++)
			if(entry->map[byte] != spec->map[byte])
				return 0;
	}
	for(i=0; i<selected; i++)
		output[i] = implementations[i];
	return 1;
}

#endif
