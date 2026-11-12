/* Allocation-free semantic gate for the first scalar executable Dis64 profile. */
#ifndef DIS64_PILOT_H
#define DIS64_PILOT_H // First scalar module semantic validation guard.
#define Dis64PilotInitializers 1024U // Explicit resource cap for pairwise initializer validation.
#define Dis64ClockWords 64U // Scalar steps, including saved return values, in a Sys caller.
#define Dis64ClockCalls 32U // Resource cap for the explicit Sys clock pilot.

#include "dis64-module.h"
#include "dis64-types.h"
#include "dis64-data.h"
#include "dis64-code.h"
#include "dis-string-pool.h"

#define Dis64ClockLocals 3U

/* Three persistent Word cells share the existing 80-byte frame with result 72. */
static int
dis64clocklocal(long offset)
{
	return offset == 64 ? 0 : offset == 68 ? 1 : offset == 76 ? 2 : -1;
}

/* Limit the Sys caller to existing direct Word operations with no control transfer. */
static int
dis64clockwordop(unsigned int op)
{
	switch(op){
	case IMOVW:
	case IADDW: case ISUBW: case IMULW: case IDIVW: case IMODW:
	case IANDW: case IORW: case IXORW: case ISHLW: case ISHRW:
		return 1;
	default:
		return 0;
	}
}


#define Dis64ClockInstructions (5U*Dis64ClockCalls+2U+Dis64ClockWords)

static int
dis64clockbranch(unsigned int op)
{
    return op == IJMP || op == IBEQW || op == IBNEW || op == IBLTW ||
        op == IBLEW || op == IBGTW || op == IBGEW;
}

static unsigned int
dis64clockbit(long offset)
{
    int local = dis64clocklocal(offset);
    return local >= 0 ? 1U<<local : offset == 72 ? 8U : 0;
}

static void
dis64clockjoin(unsigned int *target, unsigned int state)
{
    *target = *target ? 16U|((*target & state)&15U) : state;
}

/* Forward-only scalar diamonds cannot skip, repeat or enter a prepared Sys call.
 * Merge by intersection: a Word must be initialized on every reaching path. */
static int
dis64clockflow(const Dis64Instruction *code, unsigned int count)
{
    unsigned int incoming[Dis64ClockInstructions] = {0};
    unsigned int group[Dis64ClockInstructions], boundary[Dis64ClockInstructions];
    unsigned int i, phase = 0, calls = 0, state, need, define, sm, mm, bit, target;
    const Dis64Instruction *in;
    if(code == 0 || count == 0 || count > Dis64ClockInstructions) return 0;
    for(i=0; i<count; i++) {
        in = code+i; group[i] = calls; boundary[i] = phase == 0;
        if(in->op == IMFRAME) { if(phase) return 0; phase = 1; }
        else if(in->op == ILEA) { if(phase != 1) return 0; phase = 2; }
        else if(in->op == IMCALL) { if(phase != 2 || ++calls > Dis64ClockCalls) return 0; phase = 0; }
        else if(phase && !(phase == 1 && in->op == IMOVW)) return 0;
    }
    if(phase || calls == 0) return 0;
    incoming[0] = 16;
    for(i=0; i<count; i++) {
        in = code+i; state = incoming[i]; need = define = 0;
        if(!state) return 0;
        sm = USRC(in->mode); mm = in->mode&ARM;
        if(dis64clockwordop(in->op) || (dis64clockbranch(in->op) && in->op != IJMP)) {
            if(sm == AFP) { bit = dis64clockbit(in->source); if(!bit) return 0; need |= bit; }
            else if(sm != AIMM) return 0;
            if(in->op != IMOVW) {
                if(mm == AXINF || mm == AXNON) {
                    bit = dis64clockbit(mm == AXNON ? in->destination : in->middle);
                    if(!bit) return 0; need |= bit;
                } else if(mm != AXIMM) return 0;
            }
            if(!dis64clockbranch(in->op) && UDST(in->mode) == AFP) {
                define = dis64clockbit(in->destination); if(!define) return 0;
            }
        } else if(in->op == IMCALL) define = 8;
        else if(in->op != ILOAD && in->op != IMFRAME && in->op != ILEA &&
                in->op != IEXIT && in->op != IJMP) return 0;
        if(need & ~state) return 0;
        state |= define;
        if(dis64clockbranch(in->op)) {
            if(!boundary[i] || in->destination <= i || in->destination >= count) return 0;
            target = in->destination;
            if(!boundary[target] || group[target] != group[i]) return 0;
            dis64clockjoin(incoming+target,state);
        }
        if(i+1 < count && in->op != IJMP && in->op != IEXIT) dis64clockjoin(incoming+i+1,state);
    }
    return code[count-1].op == IEXIT;
}


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
