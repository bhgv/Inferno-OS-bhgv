/* Shared Android C declarations; machine emu/FPU contexts are separate port work. */
#include "../../arm/include/lib9.h"

#include "host64-fp.h"

/* Keep Inferno conversion semantics private instead of interposing on Android/Bionic symbols. */
#define strtoll inferno_strtoll // Both implementation and Inferno callers use this private name.
#define strtoull inferno_strtoull // Bionic retains its own unsigned conversion entry.


/* Preserve the existing signed converter's exact C prototype after the private rename. */
extern vlong inferno_strtoll(const char*, char**, int);


/* Preserve the existing unsigned converter's exact C prototype after the private rename. */
extern uvlong inferno_strtoull(const char*, char**, int);
