#ifndef DIS64_IMPORT_OWNER_H
#define DIS64_IMPORT_OWNER_H
/* Private native allocation, shared by checked file and trusted C constructors.
 * Include interp.h first. No pointer/owner/callback is serialized. */
#ifndef Dis64ScalarClient
#define Dis64ScalarClient (1U<<29) /* Native-only checked file identity. */
#endif
typedef struct Dis64ImportOwner Dis64ImportOwner;
struct Dis64ImportOwner {
 uint count;
 int jit;
 uvlong managed; // Native-owned table selectors; no bit comes from serialized bytes.
 Native64ImportRequest tables[64];
};
#endif
