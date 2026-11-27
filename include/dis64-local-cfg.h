/* Private entry-seeded semantic CFG checker; not decoded Dis64 file admission. */
#ifndef DIS64_LOCAL_CFG_H
#define DIS64_LOCAL_CFG_H // Abstract CFG composition only; no wire/profile identifier.

#include "dis64-local-state.h"

typedef struct Dis64LocalCFGNode Dis64LocalCFGNode;

/* Private edge classes, never serialized as new ISA or format identifiers. */
enum {
	Dis64CFGNext, // One local fallthrough edge.
	Dis64CFGJump, // One explicit local target.
	Dis64CFGBranch, // Local fallthrough plus explicit local target.
	Dis64CFGTerminal // Return/failure with no continuation.
};

/* Edges are function-relative; preparation identities remain absolute PC+1. */
struct Dis64LocalCFGNode {
	Dis64LocalEvent event; // Trusted semantic event; zero action means a pure no-op.
	unsigned int edge; // Zero next, one jump, two conditional, three terminal.
	unsigned int target; // Local jump/conditional target, zero for next/terminal.
	unsigned int setup; // Owning preparation PC+1 for setup interiors, zero otherwise.
};

typedef struct Dis64LocalCFGWork Dis64LocalCFGWork;

/* Caller-owned scratch is reset on each analysis, never accepted as input proof. */
struct Dis64LocalCFGWork {
	Dis64LocalState state; // First entry-derived state reaching this node.
	unsigned int seen; // Node has a published scratch state and one queue entry.
};

typedef struct Dis64LocalCFG Dis64LocalCFG;

/* Only the completed summary is transactional; scratch may change on refusal. */
struct Dis64LocalCFG {
	unsigned int function; // Immutable caller-selected function identity, bounded64.
	unsigned int instructions; // All abstract nodes statically validated.
	unsigned int reached; // Nodes visited from zero entry state, each at most once.
	unsigned int edges; // Reachable edge comparisons, including back edges.
	unsigned int returns; // Reachable checked semantic returns.
	unsigned int failures; // Reachable terminal failure paths.
	unsigned int work; // Consumed node/edge work units, independent of execution time.
};


/* Check every abstract event and edge, including unreachable nodes and setup interiors. */
static int
dis64localcfgnode(const Dis64LocalPlan *plan, const Dis64LocalCFGNode *nodes,
	unsigned int count, unsigned int base, unsigned int pc)
{
	const Dis64LocalCFGNode *node;
	const Dis64LocalEvent *event, *prepare;
	const Dis64LocalCallee *function;
	unsigned int owner;

	node = nodes+pc;
	event = &node->event;
	function = 0;
	if(node->edge > Dis64CFGTerminal || ((node->edge == Dis64CFGNext || node->edge == Dis64CFGBranch) && pc+1 == count) ||
	   ((node->edge == Dis64CFGNext || node->edge == Dis64CFGTerminal) && node->target != 0))
		return 0;
	if(node->edge == Dis64CFGJump || node->edge == Dis64CFGBranch){
		if(node->target >= count || nodes[node->target].setup != 0 || node->setup != 0 ||
		   event->action != 0)
			return 0;
	}
	if(node->setup != 0){
		if(node->setup <= base || node->setup > base+pc)
			return 0;
		owner = node->setup-base-1;
		prepare = &nodes[owner].event;
		if(prepare->action != Dis64StatePrepare || prepare->site != node->setup ||
		   prepare->callee >= plan->callees)
			return 0;
		function = plan->functions+prepare->callee;
	}
	if(event->action == 0){
		Dis64LocalEvent zero = {0};
		if(event->site != zero.site || event->callee != zero.callee || event->frame != zero.frame ||
		   event->index != zero.index || event->kind != zero.kind || event->boolean != zero.boolean ||
		   event->source != zero.source || event->destination != zero.destination || event->width != zero.width)
			return 0;
		return node->edge != Dis64CFGTerminal;
	}
	if(!dis64localeventvalid(event) ||
	   ((event->action == Dis64StateReturn || event->action == Dis64StateFail) != (node->edge == Dis64CFGTerminal)))
		return 0;
	switch(event->action){
	case Dis64StatePrepare:
		return node->setup == 0 && event->site == base+pc+1 &&
			event->callee < plan->callees && plan->functions[event->callee].frame == event->frame;
	case Dis64StateArgument:
		return function != 0 && event->index < function->parameters &&
			event->kind == function->kinds[event->index] &&
			(event->kind == Dis64Bool ? event->boolean <= 1 : event->boolean == 0);
	case Dis64StateBind:
		return function != 0 && function->result != 0 && event->kind == function->result &&
			event->source == plan->resultcell && event->destination == 32 && event->width == 8;
	case Dis64StateCall:
		return function != 0 && event->callee == nodes[node->setup-base-1].event.callee &&
			event->frame == function->frame;
	case Dis64StateAbandon:
		return function != 0;
	case Dis64StateRead:
		return node->setup == 0 && dis64scalarwidth(event->kind) != 0;
	case Dis64StateStore:
		return node->setup == 0 && plan->result != 0 && event->kind == plan->result &&
			(event->kind == Dis64Bool ? event->boolean <= 1 : event->boolean == 0);
	case Dis64StateReturn:
		return node->setup == 0;
	case Dis64StateFail:
		return 1;
	}
	/* Unknown action values cannot acquire ownership through an unreachable block. */
	return 0;
}


/* Analyze one immutable function from zero state; exact joins need no widening/fixpoint.
 * All spans are accessible and disjoint. Scratch must cover count nodes/queue slots.
 * Summary is unchanged on any refusal; scratch is private disposable output, not proof input.
 * Callee return/source-value facts remain trusted until decoded whole-program composition.
 */
static int
dis64localcfgcheck(const Dis64LocalPlan *plan, const Dis64LocalCFGNode *nodes,
	unsigned int count, unsigned int base, unsigned int function, unsigned int limit,
	Dis64LocalCFGWork *scratch, unsigned int *queue, unsigned int capacity, Dis64LocalCFG *output)
{
	Dis64LocalCFG result = {0};
	Dis64LocalState next, merged;
	const Dis64LocalCFGNode *node;
	unsigned int pc, i, head, tail, targets[2], edges;

	if(output == 0 || nodes == 0 || scratch == 0 || queue == 0 ||
	   !dis64localplanvalid(plan) || count == 0 || count > Dis64LocalInstructions ||
	   base > Dis64LocalInstructions-count || function >= Dis64LocalFunctions ||
	   capacity < count || limit == 0 || limit > 3*Dis64LocalInstructions)
		return 0;
	for(pc=0; pc<count; pc++)
		if(!dis64localcfgnode(plan, nodes, count, base, pc))
			return 0;
	for(pc=0; pc<count; pc++)
		scratch[pc].seen = 0;
	result.function = function;
	result.instructions = count;
	next.phase = next.site = next.callee = next.bound = next.ready = 0;
	next.assigned = 0;
	next.arguments[0] = next.arguments[1] = 0;
	scratch[0].state = next;
	scratch[0].seen = 1;
	queue[0] = 0;
	head = 0;
	tail = 1;
	while(head != tail){
		if(result.work == limit)
			return 0;
		result.work++;
		pc = queue[head++];
		node = nodes+pc;
		if(scratch[pc].state.site != node->setup)
			return 0;
		if(node->event.action == 0)
			next = scratch[pc].state;
		else if(!dis64localstatestep(plan, &scratch[pc].state, &node->event, &next))
			return 0;
		result.reached++;
		if(node->edge == Dis64CFGTerminal){
			if(node->event.action == Dis64StateReturn)
				result.returns++;
			else
				result.failures++;
			continue;
		}
		edges = 1;
		targets[0] = node->edge == Dis64CFGJump ? node->target : pc+1;
		if(node->edge == Dis64CFGBranch){
			targets[1] = node->target;
			edges = 2;
		}
		for(i=0; i<edges; i++){
			if(result.work == limit)
				return 0;
			result.work++;
			result.edges++;
			if(scratch[targets[i]].seen){
				if(!dis64localstatemerge(plan, &scratch[targets[i]].state, &next, &merged))
					return 0;
			}else{
				scratch[targets[i]].seen = 1;
				scratch[targets[i]].state = next;
				queue[tail++] = targets[i];
			}
		}
	}
	*output = result;
	return 1;
}
#endif
