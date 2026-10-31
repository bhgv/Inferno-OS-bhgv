#ifndef LIMBO_NATIVE64_ENTRY_TYPE
#define LIMBO_NATIVE64_ENTRY_TYPE // Trusted generated-C metadata; never a wire record or legacy Runtab.

typedef struct LimboNative64Entry LimboNative64Entry;

struct LimboNative64Entry
{
	const char *name; // Borrowed module-relative source symbol.
	void (*function)(void*); // Trusted C implementation, or nil for a specification.
	UWORD legacy_signature; // Consistency hash, not a structural Dis64 call signature.
	unsigned int frame_size; // Complete fixed-frame byte extent.
	unsigned int map_bytes; // Complete map8 extent, including scalar suffixes.
	const uchar *map; // Borrowed immutable map storage.
};
#endif
