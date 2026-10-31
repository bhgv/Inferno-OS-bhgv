#ifndef INFERNO_RUNTAB_LEGACY_H
#define INFERNO_RUNTAB_LEGACY_H // Legacy constructor storage only; never convert native64 maps into this record.

typedef struct LegacyRuntab Runtab;

struct LegacyRuntab
{
	char *name; // Borrowed legacy export name; nil terminates a table.
	long sig; // Existing legacy signature representation.
	void (*fn)(void*); // Trusted legacy C callback.
	int size; // Legacy frame byte extent.
	int np; // Used bytes in the fixed map.
	uchar map[16]; // Fixed legacy map capacity, not a native64 adapter.
};
#endif
