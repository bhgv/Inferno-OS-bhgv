#ifndef INFERNO_NATIVE64_MATH_CALL_H
#define INFERNO_NATIVE64_MATH_CALL_H // Private first fixed-call experiment; no loader or opcode admission.
#include "native64-entry.h"

typedef struct Native64MathBitsCall Native64MathBitsCall;

struct Native64MathBitsCall
{
	const uchar *strings; // Borrowed complete immutable Rune pool during nonallocating preflight.
	unsigned int string_bytes; // Exact readable serialized pool extent.
	const uchar *imports; // Borrowed complete immutable canonical scalar import table during preflight.
	unsigned int import_bytes; // Exact readable serialized import extent.
	unsigned int ordinal; // Existing import ordinal, including permitted aliases.
	uvlong input; // Big bit pattern copied before allocation or callbacks.
	void *(*resolve)(void*); // Trusted current live eight-byte scalar result cell; nil cancels publication.
	void *argument; // Caller-owned resolver context, live throughout retirement.
};

/* Trusted caller serializes the registry/result and roots its logical destination.
 * Resolver may allocate or throw but must preserve the prior logical scalar value.
 * On success exactly eight bytes are published with no intervening callback.
 * Registry/type retirement must not throw. No VM destination provenance is inferred.
 * This single Big->Real call owns no managed arguments/results; it is not generic CALL. */
int Math_bits64real64(const Native64MathBitsCall*);

typedef struct Native64MathFrameCall Native64MathFrameCall;

struct Native64MathFrameCall
{
	Native64MathBitsCall call; // Immutable input snapshot; resolve/argument fields are ignored by this adapter.
	Prog *caller; // Live rooted process with current REG published throughout the synchronous call.
	Type *type; // Exact retained generated descriptor for the current caller frame.
	unsigned int offset; // Naturally aligned scalar result field after the reserved64 bytes.
};

/* Resolve a current VM stack cell at preflight and again after production.
 * Caller must keep Prog/extent/Type alive and GC-visible; older extents are refused. */
int Math_bits64realframe64(const Native64MathFrameCall*);
/* Opt-in noncopyable caller lease; ordinary frame adapter semantics remain unchanged. */
int Math_bits64realguard64(const Native64MathFrameCall*);


/* Opt-in live interpreter owner only; publishes validated R before acquiring the caller lease. */
int Math_bits64realvm64(const Native64MathFrameCall*);


typedef struct Native64MathFieldCall Native64MathFieldCall;

struct Native64MathFieldCall
{
	Native64MathBitsCall call; // Canonical import/input snapshot; resolver fields are ignored by this adapter.
	Prog *caller; // Current serialized scheduler owner.
	Module *module; // Exact owning caller module, matching the current Modlink.
	Type *type; // Exact retained generated caller frame descriptor.
	const LimboNative64Fields *fields; // Immutable compiler-produced callable and argument metadata.
	unsigned int ordinal; // Declared Real argument field used as the logical result destination.
	int live; // Exactly zero for published REG or one for the explicit live-R profile.
};


/* Retain a compiler-declared Real destination across protected Big->Real production and late resolution. */
int Math_bits64realfield64(const Native64MathFieldCall*);
#endif
