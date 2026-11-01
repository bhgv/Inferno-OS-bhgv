#ifndef INFERNO_NATIVE64_MODULES_H
#define INFERNO_NATIVE64_MODULES_H // Trusted C builtin registration set, not a Dis64 import or wire signature.
#include "native64-entry.h"

typedef struct Native64ModuleSpec Native64ModuleSpec;

struct Native64ModuleSpec
{
	char *name; // Borrowed immutable module name through protected construction.
	const LimboNative64Entry *specs; // Complete readable immutable generated declarations.
	unsigned int count; // Number of declaration records, including validated unselected entries.
	const LimboNative64Entry *const *entries; // Selected immutable trusted C implementation records.
	unsigned int selected; // Number of fixed-frame exports to construct.
};

/* Outputs start nil and remain unchanged on failure; successful pointers borrow the published module owners. */
int builtinmods64(const Native64ModuleSpec*, unsigned int, Module**);
#endif
