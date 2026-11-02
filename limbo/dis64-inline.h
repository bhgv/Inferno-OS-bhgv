#ifndef LIMBO_DIS64_INLINE_H
#define LIMBO_DIS64_INLINE_H // Private bounded source specialization; no new VM calling ABI.

typedef struct Dis64Prepared Dis64Prepared;

struct Dis64Prepared {
	Inst *instructions; // Borrowed original list or owned flattened entry instructions.
	Inst *storage; // Owned flattened instruction storage, released after diagnostic emission.
	void *tables; // Owned case declarations and target clones for the flattened entry.
	unsigned int count; // Number of instructions supplied to scalar lowering.
	unsigned int localbytes; // Effective legacy local extent, including isolated callee scratch.
};


/* Validate private source scalar/flat-tuple geometry; no shared wire type or calling signature is added. */
unsigned int dis64inlineshape(Type*);


/* Prepare a bounded entry and acyclic scalar helpers; zero output first and free partial state on refusal. */
int dis64prepare(Decl*, Inst*, unsigned int, unsigned int, Dis64Prepared*);


/* Release only owned preparation state; original compiler IR and declarations remain borrowed. */
void dis64preparefree(Dis64Prepared*);
#endif
