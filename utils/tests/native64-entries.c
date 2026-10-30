#include "lib9.h"
#include "isa.h"
#include "interp.h"
#define LIMBO_NATIVE64_ENTRIES // Inspect opt-in records without enabling runtime registration.
#include "native64.h"
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


/* Inspect complete generated records; no truncating bridge to legacy Runtab exists. */
int
main(void)
{
	uchar expected[] = F_Probe_many_map;
	uchar legacy_map[16];
	const LimboNative64Entry *record;

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
