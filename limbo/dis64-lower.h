#ifndef LIMBO_DIS64_LOWER_H
#define LIMBO_DIS64_LOWER_H // Private compiler translation; no shared wire definitions.
#include "dis64-pilot.h"

#define ScalarCopyBytes 64U // Bound pointer-free source block copies and their private snapshot tail.
#define ScalarObjectLimit (1024U*1024U) // Match the existing experimental runtime object bound.
#define ScalarCodeLimit 65536U // Match the existing runtime instruction bound.
#define ScalarLegacyPrefix 32U // This backend translates only the compiler's fixed legacy scalar prefix.
#define ScalarNativePrefix 64U // Existing Dis64 np0 entry-frame reservation.
#define ScalarScratchStart 16U // Align the legacy scratch span without admitting its return-address word.
#define ScalarScratchBytes 16U // Preserve byte aliases in legacy scratch cells20..31 in a private frame tail.

typedef struct Dis64Lower Dis64Lower;

struct Dis64Lower {
	Dis64Instruction *code; // Owned translated instruction array.
	Inst **origin; // Owned array of borrowed original IR instructions for diagnostic spans.
	unsigned int *pcs; // Owned old-PC to translated-PC map.
	Dis64Initializer *constants; // Owned scalar comparison bounds, never native jump-table pointers.
	unsigned int count; // Complete translated instruction count.
	unsigned int databytes; // Scalar data extent including appended bounds.
	unsigned int nconstants; // Number of initialized appended bounds.
};


/* Accept only bounded immediate Word-sized source block copies; no managed or indirect role is inferred. */
unsigned int dis64copywidth(Inst*);


/* Validate a complete legacy direct scalar span, excluding headers and the scratch/local discontinuity. */
int dis64copyspan(int, Addr*, unsigned int, unsigned int, unsigned int);


/* Validate one already remapped instruction in explicit frame/data geometry; borrow all inputs. */
int dis64scalarinst(Inst*, unsigned int, unsigned int, unsigned int, unsigned int, Dis64Instruction*);


/* Identify a private, compiler-created numeric case table; borrow all source declarations. */
int dis64scalartable(Decl*);


/* Validate a numeric case and borrow its table without changing source metadata. */
Case *dis64scalarcase(Inst*, unsigned int);


/* Translate immutable scalar IR and bounded numeric cases; caller zero-initializes output and frees partial output on refusal. */
int dis64lower(Inst*, unsigned int, unsigned int, unsigned int, unsigned int, Dis64Lower*);


/* Release only owned translation arrays, leaving every borrowed IR object unchanged. */
void dis64lowerfree(Dis64Lower*);
#endif
