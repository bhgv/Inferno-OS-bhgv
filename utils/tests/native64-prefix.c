#include "lib9.h"
#include "isa.h"
#include "interp.h"
#define LIMBO_NATIVE64_PREFIXES
#include "sys-native64.h"
#include "prefix-native64.h"
#include "only-native64.h"
#include "native64-tail.h"
#include <stdio.h>

static int checks;
static void check(int ok) { if(!ok) abort(); checks++; }
static void callback(void *frame) { USED(frame); abort(); }

/* No Sys variadic implementation is linked: these generated records must remain schemas. */
static void prefix(const LimboNative64Prefix *schema, const char *name, UWORD hash,
    unsigned int arguments, unsigned int bytes, const uchar *map, unsigned int mapbytes)
{
    LimboNative64Prefix changed;
    const LimboNative64Entry *selected[1];
    uchar corrupt[20];
    unsigned int i;

    check(native64tailprefix(schema, name, hash, arguments, bytes, map, mapbytes) == &schema->fixed);
    check(schema->fixed.function == nil && !native64bindrecord(&schema->fixed, 1));
    selected[0] = &schema->fixed;
    check(!native64bindcheck(&schema->fixed, 1, selected, 1));
    for(i=0; i<12; i++) {
        changed = *schema;
        memmove(corrupt, schema->fixed.map, mapbytes);
        changed.fixed.map = corrupt;
        switch(i) {
        case 0: changed.arguments++; break;
        case 1: changed.fixed.name = "other"; break;
        case 2: changed.fixed.legacy_signature ^= 1; break;
        case 3: changed.fixed.function = callback; break;
        case 4: changed.fixed.frame_size += 8; break;
        case 5: changed.fixed.map_bytes--; break;
        case 6: changed.fixed.map = nil; break;
        case 7: corrupt[0] = 0x80; break;
        case 8: corrupt[mapbytes-1] ^= 0x80; break;
        case 9: changed.fixed.frame_size = 63; break;
        case 10: changed.fixed.name = ""; break;
        case 11: changed.arguments = Native64BindEntries+1; break;
        }
        check(native64tailprefix(&changed, name, hash, arguments, bytes, map, mapbytes) == nil);
    }
}

int main(void)
{
    static const uchar one[] = {0, 0x80}, two[] = {0, 0xc0}, mixed[] = {0, 0x10}, record[] = {0, 0x60};
    uchar wide[20], complete[21];
    Native64TailPlan plan = {0};
    Type type = {.ref = 1};
    Native64TailSlot slots[] = {{Native64TailRef, 0, 8, &type}, {Native64TailInt, 8, 8, nil}};
    unsigned int i;

    prefix(&limbo_native64_Sys_sprint_prefix, "sprint", LIMBO_NATIVE64_SIGNATURE_Sys_sprint, 1, 72, one, sizeof(one));
    prefix(&limbo_native64_Sys_aprint_prefix, "aprint", LIMBO_NATIVE64_SIGNATURE_Sys_aprint, 1, 72, one, sizeof(one));
    prefix(&limbo_native64_Sys_print_prefix, "print", LIMBO_NATIVE64_SIGNATURE_Sys_print, 1, 72, one, sizeof(one));
    prefix(&limbo_native64_Sys_fprint_prefix, "fprint", LIMBO_NATIVE64_SIGNATURE_Sys_fprint, 2, 80, two, sizeof(two));
    prefix(&limbo_native64_Prefix_mixed_prefix, "mixed", LIMBO_NATIVE64_SIGNATURE_Prefix_mixed, 5, 96, mixed, sizeof(mixed));
    prefix(&limbo_native64_Prefix_record_prefix, "record", LIMBO_NATIVE64_SIGNATURE_Prefix_record, 2, 88, record, sizeof(record));
    prefix(&limbo_native64_Prefix_Node_format_prefix, "Node.format", LIMBO_NATIVE64_SIGNATURE_Prefix_Node_format, 2, 80, two, sizeof(two));
    prefix(&limbo_native64_Only_f_prefix, "f", LIMBO_NATIVE64_SIGNATURE_Only_f, 1, 72, one, sizeof(one));
    memset(wide, 0xff, sizeof(wide)); wide[0] = 0; wide[19] = 0xfc;
    prefix(&limbo_native64_Prefix_many_prefix, "many", LIMBO_NATIVE64_SIGNATURE_Prefix_many, 150, 1264, wide, sizeof(wide));
    check(native64tailplan(&limbo_native64_Prefix_many_prefix.fixed, slots, 2, &plan));
    check(plan.tail == 1264 && plan.size == 1280 && plan.map_bytes == 20);
    memset(complete, 0xa5, sizeof(complete));
    check(native64tailmap(&limbo_native64_Prefix_many_prefix.fixed, slots, 2, complete, 20));
    for(i=0; i<19; i++) check(complete[i] == wide[i]);
    check(complete[19] == 0xfe && complete[20] == 0xa5);
    check(offsetof(F_Prefix_mixed, b) == 64 && offsetof(F_Prefix_mixed, n) == 68 &&
          offsetof(F_Prefix_mixed, wide) == 72 && offsetof(F_Prefix_mixed, r) == 80 && offsetof(F_Prefix_mixed, fmt) == 88);
    printf("PASS generated non-callable variadic prefixes: %d predicates; Sys, method, mixed/ADT/150-root frames and complete typed tail map\n", checks);
    return 0;
}
