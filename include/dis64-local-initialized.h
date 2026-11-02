/* Private definite-initialization analysis of real scalar code; no call admission. */
#ifndef DIS64_LOCAL_INITIALIZED_H
#define DIS64_LOCAL_INITIALIZED_H // Numeric initialization solver, not a wire/profile ID.
#define Dis64InitLimit (131U*Dis64LocalInstructions) // New node/edge plus removed-bit work cap.

#include "dis64-local-operands.h"

typedef struct Dis64InitWork Dis64InitWork;

/* Disposable caller storage; no incoming state supplied here is ever trusted. */
struct Dis64InitWork {
	Dis64LocalOperands fact; // Derived from actual bytes before traversal.
	unsigned int incoming[4]; // Definitely initialized local IDs at this instruction.
	unsigned int seen; // An entry-derived incoming state has been installed.
	unsigned int queued; // One pending ring entry exists for this node.
	unsigned int processed; // Unique-node reads/writes/edges have been counted.
};

typedef struct Dis64Initialized Dis64Initialized;

/* Deterministic summary; traversal-order-dependent visits are only an internal hard bound. */
struct Dis64Initialized {
	unsigned int instructions; // All decoded/typed instructions, including unreachable ones.
	unsigned int slots; // Exact typed declarations under the immutable context.
	unsigned int parameters; // Canonical incoming arguments seeded from the checked signature.
	unsigned int reached; // Distinct nodes reached from entry.
	unsigned int edges; // Distinct outgoing roles from reached nodes, including duplicate targets.
	unsigned int missing; // Total absent incoming slot facts over reached nodes at convergence.
	unsigned int work; // reached+edges+missing; monotone and independent of queue order.
	unsigned int reads; // Slot read roles at reached nodes, including aliased repeated reads.
	unsigned int writes; // Slot write roles at reached nodes.
};


/* Count bits using portable unsigned operations, independent of host ISA features. */
static unsigned int
dis64initbits(unsigned int bits)
{
	unsigned int count;

	count = 0;
	while(bits != 0){
		bits &= bits-1;
		count++;
	}
	return count;
}


/* Charge monotone information changes before overflow or publication. */
static int
dis64initcharge(Dis64Initialized *summary, unsigned int units, unsigned int limit)
{
	if(summary->work > limit || units > limit-summary->work)
		return 0;
	summary->work += units;
	return 1;
}


/* Zero ID is an immediate/unused role; nonzero IDs identify complete typed spans. */
static int
dis64initread(const unsigned int *bits, unsigned int id)
{
	return id == 0 || (id <= Dis64LocalSlots && (bits[(id-1)/32]&(1U<<((id-1)%32))) != 0);
}


/* Derive entry argument bits from canonical signature layout, never declared locals. */
static int
dis64initseed(const unsigned char *data, unsigned int bytes, const Dis64LocalSlot *slots,
	unsigned int count, unsigned int *bits, unsigned int *parameters)
{
	Dis64Signature signature;
	unsigned int i, at, kind, width;

	if(!dis64signaturecheck(data, bytes, Dis64LocalParameters, &signature) || signature.parameters > count)
		return 0;
	for(i=0; i<4; i++)
		bits[i] = 0;
	at = 64;
	for(i=0; i<signature.parameters; i++){
		kind = dis64u32(signature.data+12+4*i);
		width = dis64scalarwidth(kind);
		at = (at+width-1)&~(width-1);
		if(slots[i].offset != at || slots[i].kind != kind)
			return 0;
		bits[i/32] |= 1U<<(i%32);
		at += width;
	}
	*parameters = signature.parameters;
	return 1;
}


/* Decode every actual instruction, then solve reachable definite initialization by intersection.
 * Signature/local declarations are trusted immutable context, to be canonically bound later.
 * Accessible input/scratch/queue/output spans are disjoint. Scratch is disposable on refusal.
 * Result-tail/callee guarantees and callable role validation remain outside this scalar pass.
 */
static int
dis64initializedcheck(const unsigned char *data, unsigned int bytes,
	const unsigned char *signature, unsigned int signaturebytes,
	const Dis64LocalSlot *slots, unsigned int slotcount, unsigned int framebytes,
	Dis64InitWork *scratch, unsigned int *queue, unsigned int capacity,
	unsigned int limit, Dis64Initialized *output)
{
	Dis64Initialized summary = {0};
	Dis64Instruction instruction;
	Dis64InitWork *node, *target;
	unsigned int count, pc, at, used, i, j, seed[4], outgoing[4], merged[4], targets[2];
	unsigned int head, tail, pending, visits, edges, missing, lost, first, next;

	if(data == 0 || bytes < 4 || scratch == 0 || queue == 0 || output == 0 ||
	   !dis64localslotscheck(slots, slotcount, framebytes) || limit == 0 || limit > Dis64InitLimit ||
	   !dis64initseed(signature, signaturebytes, slots, slotcount, seed, &summary.parameters))
		return 0;
	count = dis64u32(data);
	if(count == 0 || count > Dis64LocalInstructions || count > capacity || count > (bytes-4)/2)
		return 0;
	at = 4;
	for(pc=0; pc<count; pc++){
		if(!dis64instructionread(data+at, bytes-at, &instruction, &used) ||
		   !dis64localoperandscheck(&instruction, count, 0, count, slots, slotcount, framebytes, &scratch[pc].fact))
			return 0;
		if((scratch[pc].fact.edge == Dis64CFGNext || scratch[pc].fact.edge == Dis64CFGBranch) && pc+1 == count)
			return 0;
		scratch[pc].seen = scratch[pc].queued = scratch[pc].processed = 0;
		at += used;
	}
	if(at != bytes)
		return 0;
	summary.instructions = count;
	summary.slots = slotcount;
	summary.reached = 1;
	summary.missing = slotcount-summary.parameters;
	if(!dis64initcharge(&summary, 1+summary.missing, limit))
		return 0;
	for(i=0; i<4; i++)
		scratch[0].incoming[i] = seed[i];
	scratch[0].seen = scratch[0].queued = 1;
	queue[0] = 0;
	head = 0;
	tail = count == 1 ? 0 : 1;
	pending = 1;
	visits = 0;
	while(pending != 0){
		if(visits == count*(slotcount+1))
			return 0;
		visits++;
		pc = queue[head];
		head = head+1 == count ? 0 : head+1;
		pending--;
		node = scratch+pc;
		node->queued = 0;
		first = !node->processed;
		if(!dis64initread(node->incoming, node->fact.source) || !dis64initread(node->incoming, node->fact.middle))
			return 0;
		for(i=0; i<4; i++)
			outgoing[i] = node->incoming[i];
		if(node->fact.write != 0){
			j = node->fact.write-1;
			outgoing[j/32] |= 1U<<(j%32);
		}
		if(first){
			summary.reads += node->fact.reads;
			summary.writes += node->fact.write != 0;
			node->processed = 1;
		}
		if(node->fact.edge == Dis64CFGTerminal)
			continue;
		edges = 1;
		targets[0] = node->fact.edge == Dis64CFGJump ? node->fact.target : pc+1;
		if(node->fact.edge == Dis64CFGBranch){
			targets[1] = node->fact.target;
			edges = 2;
		}
		for(i=0; i<edges; i++){
			if(first){
				if(!dis64initcharge(&summary, 1, limit))
					return 0;
				summary.edges++;
			}
			next = targets[i];
			target = scratch+next;
			lost = 0;
			if(!target->seen){
				missing = slotcount;
				for(j=0; j<4; j++){
					merged[j] = outgoing[j];
					missing -= dis64initbits(outgoing[j]);
				}
				if(!dis64initcharge(&summary, 1+missing, limit))
					return 0;
				summary.reached++;
				summary.missing += missing;
				target->seen = 1;
			}else{
				for(j=0; j<4; j++){
					merged[j] = target->incoming[j]&outgoing[j];
					lost += dis64initbits(target->incoming[j]^merged[j]);
				}
				if(lost == 0)
					continue;
				if(!dis64initcharge(&summary, lost, limit))
					return 0;
				summary.missing += lost;
			}
			for(j=0; j<4; j++)
				target->incoming[j] = merged[j];
			if(!target->queued){
				if(pending == count)
					return 0;
				target->queued = 1;
				queue[tail] = next;
				tail = tail+1 == count ? 0 : tail+1;
				pending++;
			}
		}
	}
	*output = summary;
	return 1;
}
#endif
