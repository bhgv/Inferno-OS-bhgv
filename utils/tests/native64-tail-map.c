/* Independent map8 boundary oracle for the trusted internal typed-tail helpers. */
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "native64-tail.h"
#include <stdio.h>
#include <string.h>

static Type expected; /* Trusted identity only; this metadata test dereferences no heap. */


/* Compare every map slot against semantic root positions, not the helper's byte
 * construction. fixedslots describes the validated prefix; count/pattern describe
 * borrowed test descriptors. Padding and Frame64 reserved slots must stay zero. */
static int inspect(const uchar *map, unsigned int bytes, unsigned int fixedslots,
                   unsigned int count, const Native64TailSlot *slots)
{
    unsigned int bit;
    for (bit = 0; bit < bytes * 8; bit++)
    {
        int wanted = fixedslots > 8 && bit == fixedslots - 1;
        int actual = (map[bit / 8] >> (7 - bit % 8)) & 1;
        if (bit >= fixedslots && bit < fixedslots + count)
        {
            wanted = slots[bit - fixedslots].kind == Native64TailRef;
        }
        if (actual != wanted)
        {
            fputs("FAIL typed-tail boundary oracle: root/padding bit mismatch\n", stderr);
            return 0;
        }
    }
    return 1;
}


/* Exercise the admitted private resource limit, with a complete1MiB frame and
 *4096 references. Return1 only for exact extent, full roots and a preserved
 * trailing canary. This is trusted metadata validation, not file admission. */
static int maximum(void)
{
    Native64TailSlot slots[Native64BindEntries];
    uvlong values[Native64BindEntries];
    uchar fixedmap[Native64BindFrame / 64];
    uchar output[Native64BindFrame / 64 + 1];
    Native64TailPlan plan;
    LimboNative64Entry fixed = {"maximum", nil, 0, 0, 0, fixedmap};
    unsigned int prefix;
    unsigned int index;
    fixed.frame_size = Native64BindFrame - Native64BindEntries * 8;
    fixed.map_bytes = (fixed.frame_size + 63) / 64;
    prefix = fixed.frame_size / 8;
    memset(fixedmap, 0, sizeof(fixedmap));
    fixedmap[(prefix - 1) / 8] = 0x80 >> ((prefix - 1) % 8);
    for (index = 0; index < Native64BindEntries; index++)
    {
        slots[index].kind = Native64TailRef;
        slots[index].offset = index * 8;
        slots[index].bytes = 8;
        slots[index].type = &expected;
        values[index] = (uvlong)(uintptr)H;
    }
    memset(output, 0x5a, sizeof(output));
    if (!native64tailplan(&fixed, slots, Native64BindEntries, &plan) ||
        plan.size != Native64BindFrame || plan.map_bytes != Native64BindFrame / 64 ||
        !native64tailvalues(&fixed, slots, Native64BindEntries, values, sizeof(values), nil, nil) ||
        !native64tailmap(&fixed, slots, Native64BindEntries, output, sizeof(output)) ||
        !inspect(output, plan.map_bytes, prefix, Native64BindEntries, slots) ||
        output[sizeof(output) - 1] != 0x5a)
    {
        fputs("FAIL typed-tail boundary oracle: private maximum extent/map\n", stderr);
        return 0;
    }
    return 1;
}


/* Enumerate every prefix map-byte residue, tail count0..257 and three root
 * distributions. Scalar H-like bits are deliberately unmarked. Capacity refusal
 * must preserve the whole destination, including canaries. Optional 'damage'
 * corrupts test-owned output to prove this oracle is active; it never changes VM code. */
int main(int argc, char **argv)
{
    unsigned int prefix;
    unsigned int count;
    unsigned int pattern;
    unsigned int cases = 0;
    int damage = argc == 2 && !strcmp(argv[1], "damage");
    for (prefix = 8; prefix < 16; prefix++)
    {
        uchar fixedmap[2] = {0, 0};
        LimboNative64Entry fixed = {"boundary", nil, 0, 0, 0, fixedmap};
        fixed.frame_size = prefix * 8;
        fixed.map_bytes = (fixed.frame_size + 63) / 64;
        if (prefix > 8)
        {
            fixedmap[(prefix - 1) / 8] = 0x80 >> ((prefix - 1) % 8);
        }
        for (count = 0; count <= 257; count++)
        {
            for (pattern = 0; pattern < 3; pattern++)
            {
                Native64TailSlot slots[257];
                Native64TailPlan plan;
                uvlong values[257];
                uchar output[64];
                uchar saved[64];
                unsigned int index;
                for (index = 0; index < count; index++)
                {
                    int rooted = pattern == 2 || index % (pattern + 2) == pattern;
                    slots[index].kind = rooted ? Native64TailRef : Native64TailBig;
                    slots[index].offset = index * 8;
                    slots[index].bytes = 8;
                    slots[index].type = rooted ? &expected : nil;
                    values[index] = ~(uvlong)0;
                }
                memset(output, 0x5a, sizeof(output));
                memmove(saved, output, sizeof(output));
                if (!native64tailplan(&fixed, slots, count, &plan) ||
                    plan.tail != prefix * 8 || plan.size != (prefix + count) * 8 ||
                    plan.count != count || plan.map_bytes != (prefix + count + 7) / 8 ||
                    native64tailmap(&fixed, slots, count, output, plan.map_bytes - 1) ||
                    memcmp(output, saved, sizeof(output)) ||
                    !native64tailvalues(&fixed, slots, count, values, count * 8, nil, nil) ||
                    !native64tailmap(&fixed, slots, count, output, sizeof(output)))
                {
                    fputs("FAIL typed-tail boundary oracle: plan/value/capacity contract\n", stderr);
                    return 1;
                }
                if (damage && count == 129)
                {
                    output[plan.map_bytes - 1] ^= 1;
                }
                if (!inspect(output, plan.map_bytes, prefix, count, slots))
                {
                    return 1;
                }
                for (index = plan.map_bytes; index < sizeof(output); index++)
                {
                    if (output[index] != saved[index])
                    {
                        fputs("FAIL typed-tail boundary oracle: output canary modified\n", stderr);
                        return 1;
                    }
                }
                cases++;
            }
        }
    }
    if (!maximum())
    {
        return 1;
    }
    cases++;
    printf("PASS typed-tail map boundaries: %u cases, every residue, counts0..257 plus private4096-slot/1MiB limit, padding and refusal canaries\n", cases);
    return 0;
}
