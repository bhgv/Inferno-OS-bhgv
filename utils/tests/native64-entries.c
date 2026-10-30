#include "lib9.h"
#include "isa.h"
#include "interp.h"
#define LIMBO_NATIVE64_TABLES // Inspect opt-in records without enabling runtime registration.
#include "native64.h"
#include "native64-bind.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Metadata fixtures provide symbols only; actual builtin calls are checked separately. */
void
Probe_value(void *frame)
{
	USED(frame);
}


/* Keep a void-return entry available to the generated record. */
void
Probe_noop(void *frame)
{
	USED(frame);
}


/* A large fixed frame must retain all twenty map bytes in its record. */
void
Probe_many(void *frame)
{
	USED(frame);
}


/* Refused late/early bindings must leave every caller output slot untouched. */
static void
rejected(const LimboNative64Entry *specs, unsigned int count,
	const LimboNative64Entry *const *selected, unsigned int number, unsigned int capacity)
{
	const LimboNative64Entry *output[2];

	output[0] = &limbo_native64_Probe_noop_entry;
	output[1] = &limbo_native64_Probe_many_entry;
	if(native64bind(specs, count, selected, number, output, capacity) ||
	   output[0] != &limbo_native64_Probe_noop_entry ||
	   output[1] != &limbo_native64_Probe_many_entry)
		abort();
}


/* Exercise map/schema/name mismatches after a valid first binding, plus empty/capacity cases. */
static void
binding(void)
{
	LimboNative64Entry changed, specs[limbo_native64_Probe_spec_count];
	const LimboNative64Entry *selected[2], *output[2];
	uchar map[20];
	char longname[257];
	unsigned int i;

	selected[0] = &limbo_native64_Probe_noop_entry;
	selected[1] = &limbo_native64_Probe_many_entry;
	if(!native64bind(limbo_native64_Probe_specs, limbo_native64_Probe_spec_count,
	   selected, 2, output, 2) || output[0] != selected[0] || output[1] != selected[1])
		abort();
	if(!native64bind(0, 0, 0, 0, 0, 0))
		abort();
	rejected(limbo_native64_Probe_specs, limbo_native64_Probe_spec_count, selected, 2, 1);
	rejected(0, limbo_native64_Probe_spec_count, selected, 2, 2);
	rejected(0, 0, selected, 2, 2);
	rejected(limbo_native64_Probe_specs, Native64BindEntries+1, selected, 2, 2);
	rejected(limbo_native64_Probe_specs, limbo_native64_Probe_spec_count, selected, Native64BindEntries+1, Native64BindEntries+1);
	rejected(limbo_native64_Probe_specs, limbo_native64_Probe_spec_count, 0, 1, 2);
	for(i=0; i<12; i++){
		changed = limbo_native64_Probe_many_entry;
		memmove(map, changed.map, sizeof(map));
		changed.map = map;
		switch(i){
		case 0: changed.name = "unknown"; break;
		case 1: changed.legacy_signature ^= 1; break;
		case 2: changed.frame_size += 8; break;
		case 3: changed.map_bytes = 16; break;
		case 4: changed.function = 0; break;
		case 5: changed.map = 0; break;
		case 6: map[19] |= 1; break;
		case 7: map[0] = 0x80; break;
		case 8: map[19] ^= 0x80; break;
		case 9: changed.name = 0; break;
		case 10: changed.name = ""; break;
		case 11:
			memset(longname, 'x', sizeof(longname));
			longname[sizeof(longname)-1] = 0;
			changed.name = longname;
			break;
		}
		selected[1] = &changed;
		rejected(limbo_native64_Probe_specs, limbo_native64_Probe_spec_count, selected, 2, 2);
	}
	selected[1] = selected[0];
	rejected(limbo_native64_Probe_specs, limbo_native64_Probe_spec_count, selected, 2, 2);
	selected[1] = 0;
	rejected(limbo_native64_Probe_specs, limbo_native64_Probe_spec_count, selected, 2, 2);
	selected[1] = &limbo_native64_Probe_many_entry;
	memmove(specs, limbo_native64_Probe_specs, sizeof(specs));
	specs[1].name = specs[0].name;
	rejected(specs, nelem(specs), selected, 2, 2);
	memmove(specs, limbo_native64_Probe_specs, sizeof(specs));
	specs[0].function = Probe_noop;
	rejected(specs, nelem(specs), selected, 2, 2);
	memmove(specs, limbo_native64_Probe_specs, sizeof(specs));
	specs[0].map_bytes = 0;
	rejected(specs, nelem(specs), selected, 2, 2);
	if(native64bind(limbo_native64_Probe_specs, limbo_native64_Probe_spec_count,
	   selected, 2, 0, 2))
		abort();
	printf("PASS native64 binding: full20-byte map, 24 refusal cases, unchanged output on failure\n");
}


/* Inspect complete generated records; no truncating bridge to legacy Runtab exists. */
int
main(void)
{
	uchar expected[] = F_Probe_many_map;
	uchar legacy_map[16];
	const LimboNative64Entry *record;

#ifdef NATIVE64_UNSUPPORTED
	if(native64bind(0, 0, 0, 0, 0, 0))
		abort();
	return 0;
#endif
	binding();
	record = &limbo_native64_Probe_many_entry;
	if(!limbo_native64_Probe_layout() || record->frame_size != sizeof(F_Probe_many) ||
	   record->map_bytes != 20 || record->map_bytes <= sizeof(legacy_map) ||
	   memcmp(record->map, expected, sizeof(expected)) != 0 ||
	   record->function != Probe_many || strcmp(record->name, "many") != 0)
		abort();
	if(limbo_native64_Probe_noop_entry.frame_size != 64 ||
	   limbo_native64_Probe_noop_entry.map_bytes != 1 ||
	   limbo_native64_Probe_noop_entry.map[0] != 0)
		abort();
	return 0;
}
