#ifndef INFERNO_NATIVE64_MATH_CALL_H
#define INFERNO_NATIVE64_MATH_CALL_H // Private first fixed-call experiment; no loader or opcode admission.

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
#endif
