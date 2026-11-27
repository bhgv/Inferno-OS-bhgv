/* Private scalar-call state model; no wire operands or executable admission. */
#ifndef DIS64_LOCAL_STATE_H
#define DIS64_LOCAL_STATE_H // Abstract verifier-state API, never a format/profile ID.

#include "dis64-local-catalog.h"

typedef struct Dis64LocalCallee Dis64LocalCallee;

/* Immutable abstract signature facts; decoded catalog/operand composition comes later. */
struct Dis64LocalCallee {
	unsigned int frame; // Exact frame Type ordinal expected by preparation and call.
	unsigned int result; // Declared scalar result kind, including void.
	unsigned int parameters; // Number of canonical scalar argument positions.
	unsigned int kinds[Dis64LocalParameters]; // Exact argument kinds; unused entries zero.
};

typedef struct Dis64LocalPlan Dis64LocalPlan;

/* Caller geometry and borrowed immutable callee facts, not a serialized record. */
struct Dis64LocalPlan {
	unsigned int framebytes; // Caller frame size, aligned to eight bytes.
	unsigned int argumentend; // End of caller argument span, at least byte64.
	unsigned int resultcell; // Dedicated final eight-byte caller-owned cell.
	unsigned int result; // Current function's declared incoming result kind.
	unsigned int callees; // Number of accessible callee descriptors, bounded64.
	const Dis64LocalCallee *functions; // Disjoint accessible immutable descriptor array.
};

typedef struct Dis64LocalState Dis64LocalState;

/* Zero initialization denotes entry into a live function with no assigned result. */
struct Dis64LocalState {
	unsigned int phase; // Zero live, one returned, two failed; terminal states cannot resume.
	unsigned int site; // Nonzero preparation identity (abstract PC+1), or zero when idle.
	unsigned int callee; // Pending callee ordinal+1, or zero when idle.
	unsigned int bound; // Whether the pending nonvoid call owns a result-address binding.
	unsigned int ready; // Exact completed outgoing result kind; zero means invalid.
	unsigned int assigned; // Whether this function established its own incoming result.
	unsigned int arguments[2]; // Initialized outgoing arguments, bits0..63 independent of WORD.
};

typedef struct Dis64LocalEvent Dis64LocalEvent;

/* Abstract semantic facts; a future decoder must prove offsets, values and provenance. */
struct Dis64LocalEvent {
	unsigned int action; // Internal transition selector below, never a Dis opcode.
	unsigned int site; // Preparation identity for Prepare only.
	unsigned int callee; // Exact zero-based callee ordinal for Prepare/Call.
	unsigned int frame; // Exact frame Type ordinal for Prepare/Call.
	unsigned int index; // Canonical argument position for Argument only.
	unsigned int kind; // Exact argument/result kind for typed operations.
	unsigned int boolean; // Concrete0/1 for bool operations; zero for all other kinds.
	unsigned int source; // Caller result-cell byte offset for Bind only.
	unsigned int destination; // Prepared callee result-address byte32 for Bind only.
	unsigned int width; // Eight-byte runtime address write for Bind only.
};

/* Internal actions only; no registry, opcode or feature allocation is implied. */
enum {
	Dis64StatePrepare = 1, // Acquire exactly one outgoing frame and invalidate the old result.
	Dis64StateArgument, // Establish one argument with its exact signature kind.
	Dis64StateBind, // Bind the dedicated caller result cell to callee byte32 width8.
	Dis64StateCall, // Require complete setup, retire it and publish a typed successful result.
	Dis64StateRead, // Read an idle completed result with the exact kind.
	Dis64StateStore, // Establish the current function's declared incoming result.
	Dis64StateAbandon, // Retire one pending preparation without publishing a result.
	Dis64StateReturn, // Require no preparation and a definitely established nonvoid result.
	Dis64StateFail // Retire all preparation/results and terminate the abstract path.
};


/* Check accessible abstract facts before using counts, kinds or caller cell offsets. */
static int
dis64localplanvalid(const Dis64LocalPlan *plan)
{
	const Dis64LocalCallee *function;
	unsigned int i, j;

	if((unsigned int)-1 != 0xffffffffU || plan == 0 || plan->framebytes < 72 ||
	   (plan->framebytes&7) != 0 ||
	   plan->resultcell != plan->framebytes-8 || plan->argumentend < 64 ||
	   plan->argumentend > plan->resultcell || plan->callees > Dis64LocalFunctions ||
	   (plan->callees != 0 && plan->functions == 0) ||
	   (plan->result != Dis64Void && dis64scalarwidth(plan->result) == 0))
		return 0;
	for(i=0; i<plan->callees; i++){
		function = plan->functions+i;
		if(function->parameters > Dis64LocalParameters ||
		   (function->result != Dis64Void && dis64scalarwidth(function->result) == 0))
			return 0;
		for(j=0; j<Dis64LocalParameters; j++){
			if(j < function->parameters ? dis64scalarwidth(function->kinds[j]) == 0 :
			   function->kinds[j] != 0)
				return 0;
		}
	}
	return 1;
}


/* Check canonical state shape after plan validation; entry-seeded transitions prove provenance. */
static int
dis64localstatevalid(const Dis64LocalPlan *plan, const Dis64LocalState *state)
{
	const Dis64LocalCallee *function;
	unsigned int i;

	if(state == 0 || state->phase > 2 || state->assigned > 1 ||
	   (plan->result == Dis64Void && state->assigned != 0) ||
	   (state->ready != Dis64Void && dis64scalarwidth(state->ready) == 0))
		return 0;
	if(state->phase != 0 && (state->site != 0 || state->callee != 0 ||
	   state->bound != 0 || state->ready != 0 || state->assigned != 0 ||
	   state->arguments[0] != 0 || state->arguments[1] != 0))
		return 0;
	if(state->site == 0)
		return state->callee == 0 && state->bound == 0 &&
			state->arguments[0] == 0 && state->arguments[1] == 0;
	if(state->site > Dis64LocalInstructions || state->callee == 0 ||
	   state->callee > plan->callees || state->bound > 1 || state->ready != 0)
		return 0;
	function = plan->functions+state->callee-1;
	if(function->result == Dis64Void && state->bound != 0)
		return 0;
	for(i=function->parameters; i<Dis64LocalParameters; i++)
		if((state->arguments[i/32]&(1U<<(i%32))) != 0)
			return 0;
	return 1;
}


/* Unused abstract fields must be zero so hidden state cannot survive a transition. */
static int
dis64localeventvalid(const Dis64LocalEvent *event)
{
	unsigned int allowed, i, fields[9];

	if(event == 0)
		return 0;
	fields[0] = event->site;
	fields[1] = event->callee;
	fields[2] = event->frame;
	fields[3] = event->index;
	fields[4] = event->kind;
	fields[5] = event->boolean;
	fields[6] = event->source;
	fields[7] = event->destination;
	fields[8] = event->width;
	switch(event->action){
	case Dis64StatePrepare: allowed = 7; break;
	case Dis64StateArgument: allowed = 56; break;
	case Dis64StateBind: allowed = 464; break;
	case Dis64StateCall: allowed = 6; break;
	case Dis64StateRead: allowed = 16; break;
	case Dis64StateStore: allowed = 48; break;
	case Dis64StateAbandon: case Dis64StateReturn: case Dis64StateFail:
		allowed = 0;
		break;
	default:
		return 0;
	}
	for(i=0; i<9; i++)
		if((allowed&(1U<<i)) == 0 && fields[i] != 0)
			return 0;
	return 1;
}


/* Retire a prepared owner exactly once; keep incoming-result assignment separate. */
static void
dis64localstateclear(Dis64LocalState *state)
{
	state->site = state->callee = state->bound = state->ready = 0;
	state->arguments[0] = state->arguments[1] = 0;
}


/* Apply one trusted abstract semantic event transactionally, including in-place use.
 * This does not decode operands, prove local spans, execute CALL or establish a runtime ABI.
 * Input spans must be accessible; output may equal input but must not overlap plan/events.
 */
static int
dis64localstatestep(const Dis64LocalPlan *plan, const Dis64LocalState *input,
	const Dis64LocalEvent *event, Dis64LocalState *output)
{
	const Dis64LocalCallee *function;
	Dis64LocalState next;
	unsigned int i;

	if(output == 0 || !dis64localplanvalid(plan) ||
	   !dis64localstatevalid(plan, input) || !dis64localeventvalid(event) || input->phase != 0)
		return 0;
	next = *input;
	function = next.callee == 0 ? 0 : plan->functions+next.callee-1;
	switch(event->action){
	case Dis64StatePrepare:
		if(next.site != 0 || event->site == 0 || event->site > Dis64LocalInstructions ||
		   event->callee >= plan->callees || plan->functions[event->callee].frame != event->frame)
			return 0;
		dis64localstateclear(&next);
		next.site = event->site;
		next.callee = event->callee+1;
		break;
	case Dis64StateArgument:
		if(function == 0 || event->index >= function->parameters ||
		   event->kind != function->kinds[event->index] ||
		   (event->kind == Dis64Bool ? event->boolean > 1 : event->boolean != 0))
			return 0;
		next.arguments[event->index/32] |= 1U<<(event->index%32);
		break;
	case Dis64StateBind:
		if(function == 0 || function->result == Dis64Void || next.bound != 0 ||
		   event->kind != function->result || event->source != plan->resultcell ||
		   event->destination != 32 || event->width != 8)
			return 0;
		next.bound = 1;
		break;
	case Dis64StateCall:
		if(function == 0 || event->callee+1 != next.callee || event->frame != function->frame ||
		   (function->result != Dis64Void && next.bound == 0))
			return 0;
		for(i=0; i<function->parameters; i++)
			if((next.arguments[i/32]&(1U<<(i%32))) == 0)
				return 0;
		dis64localstateclear(&next);
		next.ready = function->result;
		break;
	case Dis64StateRead:
		if(next.site != 0 || next.ready == 0 || event->kind != next.ready)
			return 0;
		break;
	case Dis64StateStore:
		if(next.site != 0 || plan->result == Dis64Void || event->kind != plan->result ||
		   (event->kind == Dis64Bool ? event->boolean > 1 : event->boolean != 0))
			return 0;
		next.assigned = 1;
		break;
	case Dis64StateAbandon:
		if(next.site == 0)
			return 0;
		dis64localstateclear(&next);
		break;
	case Dis64StateReturn:
		if(next.site != 0 || (plan->result != Dis64Void && next.assigned == 0))
			return 0;
		dis64localstateclear(&next);
		next.assigned = 0;
		next.phase = 1;
		break;
	case Dis64StateFail:
		dis64localstateclear(&next);
		next.assigned = 0;
		next.phase = 2;
		break;
	default:
		return 0;
	}
	*output = next;
	return 1;
}


/* Join only identical live states, comparing fields rather than structure padding. */
static int
dis64localstatemerge(const Dis64LocalPlan *plan, const Dis64LocalState *left,
	const Dis64LocalState *right, Dis64LocalState *output)
{
	if(output == 0 || !dis64localplanvalid(plan) ||
	   !dis64localstatevalid(plan, left) || !dis64localstatevalid(plan, right) ||
	   left->phase != 0 || right->phase != 0 || left->site != right->site ||
	   left->callee != right->callee || left->bound != right->bound || left->ready != right->ready ||
	   left->assigned != right->assigned || left->arguments[0] != right->arguments[0] ||
	   left->arguments[1] != right->arguments[1])
		return 0;
	*output = *left;
	return 1;
}
#endif
