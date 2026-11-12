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


#define Dis64ClockInstructions (6U*Dis64ClockCalls+2U+Dis64ClockWords)

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
    return local >= 0 ? 1U<<local : offset == 72 ? 8U : offset == 96 ? 32U : 0;
}

static void
dis64clockjoin(unsigned int *target, unsigned int state)
{
    *target = *target ? 16U|((*target & state)&47U) : state;
}

/* Word effects are recomputed from checked operands, not caller-supplied facts. */
static int
dis64clockeffect(const Dis64Instruction *in, unsigned int *need, unsigned int *define)
{
    unsigned int sm = USRC(in->mode), mm = in->mode&ARM, bit;
    *need = *define = 0;
    if(dis64clockwordop(in->op) || (dis64clockbranch(in->op) && in->op != IJMP)) {
        if(sm == AFP) { bit = dis64clockbit(in->source); if(!bit) return 0; *need |= bit; }
        else if(sm != AIMM) return 0;
        if(in->op != IMOVW) {
            if(mm == AXINF || mm == AXNON) {
                bit = dis64clockbit(mm == AXNON ? in->destination : in->middle);
                if(!bit) return 0; *need |= bit;
            } else if(mm != AXIMM) return 0;
        }
        if(!dis64clockbranch(in->op) && UDST(in->mode) == AFP) {
            *define = dis64clockbit(in->destination); if(!*define) return 0;
        }
    } else if(in->op == IMCALL) *define = in->middle == -1 ? 32 : 8; // Typed projection: FD result defines no Word.
    else if(in->op != ILOAD && in->op != IMFRAME && in->op != ILEA && in->op != IEXIT && in->op != IJMP) return 0;
    return 1;
}

/* Entry has no initialized Word cells. A bounded descending intersection
 * solver handles loop-carried facts without inventing first-iteration values.
 * Branches can enter only call boundaries, never argument/result preparation. */
static int
dis64clockflow(const Dis64Instruction *code, unsigned int count)
{
    unsigned int incoming[Dis64ClockInstructions], outgoing[Dis64ClockInstructions];
    unsigned int queue[Dis64ClockInstructions];
    unsigned char boundary[Dis64ClockInstructions], reached[Dis64ClockInstructions] = {0}, queued[Dis64ClockInstructions] = {0};
    unsigned int i, p, phase = 0, calls = 0, state, need, define, target, fall, at;
    unsigned int head, tail, pending, budget;
    const Dis64Instruction *in;
    if(code == 0 || count == 0 || count > Dis64ClockInstructions) return 0;
    for(i=0; i<count; i++) {
        in = code+i; boundary[i] = phase == 0;
        if(in->op == IMFRAME) { if(phase) return 0; phase = 1; }
        else if(in->op == ILEA) { if(phase != 1) return 0; phase = 2; }
        else if(in->op == IMCALL) { if(phase != 2 || ++calls > Dis64ClockCalls) return 0; phase = 0; }
        else if(phase && !(phase == 1 && in->op == IMOVW)) return 0;
        if(!dis64clockeffect(in,&need,&define)) return 0;
        incoming[i] = outgoing[i] = 63; // Reachable plus five top facts, monotonically removed below.
    }
    if(phase || calls == 0 || code[count-1].op != IEXIT) return 0;
    for(i=0; i<count; i++) if(dis64clockbranch(code[i].op)) {
        if(!boundary[i] || code[i].destination <= 0 || code[i].destination >= count || !boundary[code[i].destination]) return 0;
    }
    head = 0; tail = 1; queue[0] = 0; reached[0] = 1;
    while(head < tail) {
        at = queue[head++]; in = code+at;
        target = dis64clockbranch(in->op) ? (unsigned int)in->destination : count;
        fall = in->op == IJMP || in->op == IEXIT ? count : at+1;
        if(target < count && !reached[target]) { reached[target] = 1; queue[tail++] = target; }
        if(fall < count && !reached[fall]) { reached[fall] = 1; queue[tail++] = fall; }
    }
    if(tail != count) return 0;
    head = 0; tail = 1%count; pending = 1; queue[0] = 0; queued[0] = 1;
    budget = 20*count+1; // Five disappearing bits, at most two outgoing edges per instruction.
    while(pending) {
        if(budget == 0) return 0; budget--;
        at = queue[head]; head = (head+1)%count; pending--; queued[at] = 0;
        state = at == 0 ? 16 : 0;
        for(p=0; p<count; p++) {
            in = code+p;
            target = dis64clockbranch(in->op) ? (unsigned int)in->destination : count;
            fall = in->op == IJMP || in->op == IEXIT ? count : p+1;
            if(target == at || fall == at) dis64clockjoin(&state,outgoing[p]);
        }
        if(!state || !dis64clockeffect(code+at,&need,&define)) return 0;
        incoming[at] = state; state |= define;
        if(state == outgoing[at]) continue;
        outgoing[at] = state; in = code+at;
        target = dis64clockbranch(in->op) ? (unsigned int)in->destination : count;
        fall = in->op == IJMP || in->op == IEXIT ? count : at+1;
        if(target < count && !queued[target]) {
            if(pending == count) return 0; queue[tail] = target; tail = (tail+1)%count; pending++; queued[target] = 1;
        }
        if(fall < count && !queued[fall]) {
            if(pending == count) return 0; queue[tail] = fall; tail = (tail+1)%count; pending++; queued[fall] = 1;
        }
    }
    for(i=0; i<count; i++) {
        if(!dis64clockeffect(code+i,&need,&define) || (need & ~incoming[i])) return 0;
    }
    return 1;
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
