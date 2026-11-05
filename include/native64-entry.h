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

typedef struct LimboNative64Field LimboNative64Field;

struct LimboNative64Field
{
	unsigned int ordinal; // Source argument ordinal, including unsupported reference/aggregate fields.
	unsigned int kind; // Existing Dis64Big/Dis64Real; zero means unsupported in this private scalar profile.
	unsigned int offset; // Compiler-computed native Frame64 offset, never inferred from a GC map.
	unsigned int bytes; // Complete declared field extent, excluding adjacent padding.
};

typedef struct LimboNative64Fields LimboNative64Fields;

struct LimboNative64Fields
{
	const LimboNative64Entry *entry; // Exact generated callable identity, not its legacy hash.
	const LimboNative64Field *fields; // Immutable compiler-owned source argument descriptions.
	unsigned int count; // Number of declared arguments, not the number of eligible scalar fields.
};

/* Private compiler symbols. These values are never written into Dis64. */
typedef enum {
	LimboNative64String = 1,
	LimboNative64ListString,
} LimboNative64ManagedKind;

typedef struct LimboNative64Managed LimboNative64Managed;
struct LimboNative64Managed
{
	const LimboNative64Entry *entry;
	LimboNative64ManagedKind argument;
	LimboNative64ManagedKind result;
};
#endif
