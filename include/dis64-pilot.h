/* Allocation-free semantic gate for the first scalar executable Dis64 profile. */
#ifndef DIS64_PILOT_H
#define DIS64_PILOT_H // First scalar module semantic validation guard.
#define Dis64PilotInitializers 1024U // Explicit resource cap for pairwise initializer validation.

#include "dis64-module.h"
#include "dis64-types.h"
#include "dis64-data.h"
#include "dis64-code.h"
#include "dis-string-pool.h"

typedef struct Dis64Pilot Dis64Pilot;
struct Dis64Pilot {
	Dis64View envelope; // Borrowed accepted container, not a runtime Module.
	Dis64ModuleInfo module; // Linked ordinal metadata.
	Dis64TypeTable types; // Structural descriptor view.
	DisStringPoolView strings; // Actual agreed pool, including duplicate contents.
	Dis64CodeView code; // Validated initial scalar subset.
	Dis64Type data; // Descriptor for scalar module data.
	Dis64Initializers initializers; // Validated scalar initialization payload, if present.
	Dis64Type frame; // Entry frame, with no managed references in this pilot.
};


/* Find a singleton known section without exposing an unchecked host pointer. */
static int
dis64pilotsection(const Dis64View *view, unsigned int kind, Dis64Section *result)
{
	Dis64Section section;
	unsigned int i;

	for(i=0; i<view->sections; i++){
		if(!dis64section(view, i, &section))
			return 0;
		if(section.kind == kind){
			*result = section;
			return 1;
		}
	}
	return 0;
}


/* Require scalar-only storage until real GC/result-slot acceptance is implemented. */
static int
dis64pilotscalar(const Dis64Type *type)
{
	unsigned int i;

	for(i=0; i<type->mapbytes; i++)
		if(type->map[i] != 0)
			return 0;
	return 1;
}


/* Link metadata, descriptors, strings and scalar code without allocating a VM.
 * Import/export/control execution semantics are deliberately outside this pilot and rejected explicitly, never sent to the legacy loader. */
static int
dis64pilotcheck(const unsigned char *data, unsigned int bytes,
	unsigned int objectlimit, unsigned int instructionlimit, Dis64Pilot *result)
{
	Dis64Pilot pilot;
	Dis64Section section;
	DisRuneView name;
	unsigned int i;

	if(result == 0 || !dis64check(data, bytes, 0, 64, 0, 0, &pilot.envelope))
		return 0;
	if(!dis64pilotsection(&pilot.envelope, Dis64Module, &section) ||
	   !dis64modulecheck(data+section.offset, section.bytes, &pilot.module) ||
	   pilot.module.profile != 1 || pilot.module.databytes > objectlimit)
		return 0;
	if(!dis64pilotsection(&pilot.envelope, Dis64Types, &section) ||
	   !dis64typescheck(data+section.offset, section.bytes, 65536, objectlimit, &pilot.types))
		return 0;
	if(!dis64pilotsection(&pilot.envelope, Dis64Strings, &section) ||
	   !disstringpoolcheck(data+section.offset, section.bytes, 65536, &pilot.strings) ||
	   !disstringpoolget(&pilot.strings, pilot.module.name, &name))
		return 0;
	if(!dis64typeget(&pilot.types, pilot.module.frametype, &pilot.frame) ||
	   pilot.frame.bytes < 64 || pilot.frame.align != 8 || !dis64pilotscalar(&pilot.frame))
		return 0;
	pilot.data.bytes = 0;
	pilot.data.align = 1;
	pilot.data.map = 0;
	pilot.data.mapbytes = 0;
	if(pilot.module.databytes != 0 &&
	   (!dis64typeget(&pilot.types, pilot.module.datatype, &pilot.data) ||
	   pilot.data.bytes != pilot.module.databytes || !dis64pilotscalar(&pilot.data)))
		return 0;
	if(dis64pilotsection(&pilot.envelope, Dis64Data, &section)){
		if(!dis64initializerscheck(data+section.offset, section.bytes, &pilot.data,
		   Dis64PilotInitializers, &pilot.initializers))
			return 0;
	}else{
		if(pilot.module.databytes != 0)
			return 0;
		pilot.initializers.data = 0;
		pilot.initializers.bytes = 0;
		pilot.initializers.count = 0;
	}
	for(i=Dis64Imports; i<=Dis64Control; i++){
		if(i == Dis64Strings)
			continue;
		if(dis64pilotsection(&pilot.envelope, i, &section))
			return 0;
	}
	if(!dis64pilotsection(&pilot.envelope, Dis64Code, &section) ||
	   !dis64codecheck(data+section.offset, section.bytes, instructionlimit,
		pilot.data.bytes, pilot.frame.bytes, &pilot.code) || pilot.module.entry >= pilot.code.count)
		return 0;
	*result = pilot;
	return 1;
}
#endif
