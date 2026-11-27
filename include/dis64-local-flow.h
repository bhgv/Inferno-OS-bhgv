/* Experimental control-flow preflight, not an executable Dis64 call profile. */
#ifndef DIS64_LOCAL_FLOW_H
#define DIS64_LOCAL_FLOW_H // Draft flow checker; include isa.h once before this header.

#include "dis64-local-catalog.h"

typedef struct Dis64LocalNode Dis64LocalNode;

struct Dis64LocalNode {
	unsigned int op; // Existing ISA number, not a new wire opcode.
	unsigned int target; // Explicit abstract Dis-PC for branches/calls; zero otherwise.
};

typedef struct Dis64LocalFlow Dis64LocalFlow;

struct Dis64LocalFlow {
	unsigned int functions; // Number of interval owners checked.
	unsigned int instructions; // All nodes checked, including unreachable code.
	unsigned int branches; // Conditional and unconditional branch sites.
	unsigned int calls; // Call sites, including repeated edges.
	unsigned int depth; // Longest path over all declared functions, including disconnected ones.
	unsigned int terminals; // RET/EXIT sites; not proof of result assignment or termination.
};

/* Internal classification only; these constants are never serialized. */
enum {
	Dis64FlowReject, // Opcode outside the proposed initial control vocabulary.
	Dis64FlowNext, // Requires a same-function fallthrough.
	Dis64FlowBranch, // Requires a local target and local fallthrough.
	Dis64FlowJump, // Requires a local target, without implicit fallthrough.
	Dis64FlowCall, // Requires an exact function entry and local continuation.
	Dis64FlowTerminal // RET/EXIT, without an outgoing control edge.
};


/* Classify existing opcodes without pretending to validate their operand roles. */
static int
dis64localflowkind(unsigned int op)
{
	switch(op){
	case IEXIT: case IRET: return Dis64FlowTerminal;
	case IJMP: return Dis64FlowJump;
	case ICALL: return Dis64FlowCall;
	case IBEQW: case IBNEW: case IBLTW: case IBLEW: case IBGTW: case IBGEW:
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		return Dis64FlowBranch;
	case IMOVW: case IMOVL: case IMOVF:
	case IADDW: case ISUBW: case IMULW: case IDIVW: case IMODW:
	case IANDW: case IORW: case IXORW: case ISHLW: case ISHRW:
	case IADDL: case ISUBL: case IMULL: case IDIVL: case IMODL:
	case IANDL: case IORL: case IXORL: case ISHLL: case ISHRL:
	case IFRAME: case ILEA:
		return Dis64FlowNext;
	}
	return Dis64FlowReject;
}


/* Check interval ownership and an acyclic bounded call graph before publishing counts.
 * Only entry/end fields are consumed here; metadata/signatures require the separate
 * catalog checker. Nodes are abstract control IR, NOT decoded wire instructions.
 * Inputs/output are disjoint accessible storage. No operands/setup/lifetimes are checked.
 */
static int
dis64localflowcheck(const Dis64LocalFunction *functions, unsigned int count,
	const Dis64LocalNode *nodes, unsigned int instructions, unsigned int depthlimit,
	Dis64LocalFlow *result)
{
	unsigned char graph[Dis64LocalFunctions][Dis64LocalFunctions/8] = {{0}};
	unsigned int incoming[Dis64LocalFunctions] = {0};
	unsigned int depths[Dis64LocalFunctions] = {0};
	unsigned int queue[Dis64LocalFunctions];
	Dis64LocalFlow flow;
	unsigned int i, j, pc, target, first, end, head, tail, seen, owner, longest;
	unsigned char bit;
	int kind;

	if(functions == 0 || nodes == 0 || result == 0 || count == 0 ||
	   count > Dis64LocalFunctions || instructions == 0 ||
	   instructions > Dis64LocalInstructions || depthlimit == 0 || depthlimit > Dis64LocalFunctions)
		return 0;
	for(i=0; i<count; i++){
		if(functions[i].entry >= functions[i].end || functions[i].end > instructions ||
		   (i == 0 ? functions[i].entry != 0 : functions[i].entry != functions[i-1].end))
			return 0;
	}
	if(functions[count-1].end != instructions)
		return 0;
	flow.functions = count;
	flow.instructions = instructions;
	flow.branches = flow.calls = flow.terminals = 0;
	flow.depth = 0;
	for(i=0; i<count; i++){
		first = functions[i].entry;
		end = functions[i].end;
		for(pc=first; pc<end; pc++){
			kind = dis64localflowkind(nodes[pc].op);
			target = nodes[pc].target;
			if(kind == Dis64FlowReject)
				return 0;
			if(kind == Dis64FlowTerminal || kind == Dis64FlowNext){
				if(target != 0)
					return 0;
			}
			if(kind == Dis64FlowTerminal){
				flow.terminals++;
				continue;
			}
			if(kind != Dis64FlowJump && pc+1 == end)
				return 0;
			if(kind == Dis64FlowBranch || kind == Dis64FlowJump){
				if(target < first || target >= end)
					return 0;
				flow.branches++;
			}else if(kind == Dis64FlowCall){
				for(j=0; j<count && functions[j].entry != target; j++)
					;
				if(j == count)
					return 0;
				bit = (unsigned char)(1U<<(j%8));
				if((graph[i][j/8]&bit) == 0){
					graph[i][j/8] |= bit;
					incoming[j]++;
				}
				flow.calls++;
			}
		}
	}
	head = tail = seen = longest = 0;
	for(i=0; i<count; i++){
		depths[i] = 1;
		if(incoming[i] == 0)
			queue[tail++] = i;
	}
	while(head != tail){
		owner = queue[head++];
		seen++;
		if(depths[owner] > longest)
			longest = depths[owner];
		for(j=0; j<count; j++){
			if((graph[owner][j/8]&(1U<<(j%8))) == 0)
				continue;
			if(depths[j] < depths[owner]+1)
				depths[j] = depths[owner]+1;
			if(--incoming[j] == 0)
				queue[tail++] = j;
		}
	}
	if(seen != count || longest > depthlimit)
		return 0;
	flow.depth = longest;
	*result = flow;
	return 1;
}
#endif
