#ifndef INFERNO_NATIVE64_MODEL_H
#define INFERNO_NATIVE64_MODEL_H // Shared trusted-C geometry gate, not a file ABI selector.
#include <float.h>
#include <limits.h>
#include <stddef.h>

typedef struct Native64ModelAlign Native64ModelAlign;

struct Native64ModelAlign
{
	uchar prefix; // Reveal natural word alignment.
	WORD word; // Expected offset4.
	uchar reference_prefix; // Reveal independent pointer alignment.
	void *reference; // Expected offset16.
	uchar big_prefix; // Reveal independent big alignment.
	LONG big; // Expected offset32.
	uchar real_prefix; // Reveal independent real alignment.
	REAL real; // Expected offset48.
};


/* Reject wrong scalar widths, packed arguments and incompatible Frame64 links before touching metadata. */
static int
native64model(void)
{
	return CHAR_BIT == 8 && FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
		sizeof(WORD) == 4 && sizeof(UWORD) == 4 && sizeof(int) == 4 &&
		sizeof(void*) == 8 && sizeof(uintptr) == 8 && sizeof(uvlong) == 8 &&
		sizeof(LONG) == 8 && sizeof(REAL) == 8 &&
		offsetof(Native64ModelAlign, word) == 4 && offsetof(Native64ModelAlign, reference) == 16 &&
		offsetof(Native64ModelAlign, big) == 32 && offsetof(Native64ModelAlign, real) == 48 &&
		sizeof(Frame) == 32 && offsetof(Frame, lr) == 0 && offsetof(Frame, fp) == 8 &&
		offsetof(Frame, mr) == 16 && offsetof(Frame, t) == 24;
}
#endif
