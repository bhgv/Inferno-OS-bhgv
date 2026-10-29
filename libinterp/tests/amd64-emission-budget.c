/* Exercise private staging with prefixes and public zero-publication declines. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32int; /* Native instruction staging unit. */
typedef unsigned char uchar; /* Instruction-byte storage. */
#define nil 0 /* Inferno's native null sentinel. */
#include "isa.h"
#include "amd64-emit.h"


/* Return copied metadata: width is Dis WORD bytes (4/8); shifted string fields force
 * the long-displacement paths without changing the production capacity. */
static LeafLayout layout(int width, int shifted)
{
    LeafLayout a;
    memset(&a, 0, sizeof(a));
    a.wordbytes = width;
    a.s = 8;
    a.d = 16;
    a.m = 24;
    a.module = 40;
    a.linkmodule = 8;
    a.moduletypes = 32;
    a.modulenty = 40;
    a.typeref = 0;
    a.typesize = 24;
    a.typenp = 28;
    a.typefree = 8;
    a.typemark = 16;
    a.heapref = 24;
    a.heaptype = 16;
    a.heapcolor = 32;
    a.refbytes = 8;
    a.arraylen = 0;
    a.arraytype = 8;
    a.arrayroot = 16;
    a.arraydata = 24;
    a.arrayidentity = (void *)(uintptr_t)0x100004500ULL;
    a.stringlen = shifted ? 1024 : 0;
    a.stringmax = shifted ? 1028 : 4;
    a.stringdata = shifted ? 1032 : 8;
    return a;
}


/* Run actual helper fixups after every near-capacity prefix. Canary checks and
 * sanitizer array-subobject checks are separate oracles; no emitted code runs. */
int main(void)
{
    struct {
        uchar before[64]; /* Must remain untouched by all staging operations. */
        X64Buf buffer; /* Production private byte storage and requested count. */
        uchar after[1024]; /* Detect writes bypassing private staging capacity. */
    } state;
    const int operations[] = {INEWA, INEWAZ, IINSC, IADDC, IMOVW, IADDW};
    int width;
    int shifted;
    int prefix;
    int helper;
    int checks = 0;
    uint64_t hash = 1469598103934665603ULL; /* Independent publication fingerprint. */
    size_t byte;
    for (width = 4; width <= 8; width += 4) {
        for (shifted = 0; shifted <= 1; ++shifted) {
            LeafLayout a = layout(width, shifted);
#ifndef BUDGET_PUBLICATION_ONLY
            for (prefix = 0; prefix <= (int)sizeof(state.buffer.bytes) + 64; ++prefix) {
                for (helper = 0; helper < 2; ++helper) {
                    memset(&state, 0xa7, sizeof(state));
                    state.buffer.n = prefix;
                    if (helper) {
                        x64arraycreate(&state.buffer, &a, 1);
                    } else {
                        x64stringstore(&state.buffer, &a);
                    }
                    for (byte = 0; byte < sizeof(state.before); ++byte) {
                        if (state.before[byte] != 0xa7) {
                            fputs("emitter overwrote its leading canary\n", stderr);
                            return 1;
                        }
                    }
                    for (byte = 0; byte < sizeof(state.after); ++byte) {
                        if (state.after[byte] != 0xa7) {
                            fputs("emitter overwrote its trailing canary\n", stderr);
                            return 1;
                        }
                    }
                    for (byte = 0; byte < (size_t)prefix && byte < sizeof(state.buffer.bytes); ++byte) {
                        if (state.buffer.bytes[byte] != 0xa7) {
                            fputs("fixup overwrote an earlier instruction prefix\n", stderr);
                            return 1;
                        }
                    }
                    ++checks;
                }
            }
#endif
            for (helper = 0; helper < (int)(sizeof(operations) / sizeof(operations[0])); ++helper) {
                int capacity;
                for (capacity = 0; capacity <= LeafMax; ++capacity) {
                    u32int output[LeafMax + 8];
                    int produced;
                    memset(output, 0xa7, sizeof(output));
                    produced = leafemit(output, capacity, &a, operations[helper]);
                    if (produced < 0 || produced > capacity) {
                        fputs("emitter returned an invalid publication extent\n", stderr);
                        return 1;
                    }
                    for (byte = (size_t)produced * sizeof(output[0]); byte < sizeof(output); ++byte) {
                        if (((uchar *)output)[byte] != 0xa7) {
                            fputs("declined/truncated emission published destination bytes\n", stderr);
                            return 1;
                        }
                    }
                    hash = (hash ^ (unsigned int)produced) * 1099511628211ULL;
                    for (byte = 0; byte < sizeof(output); ++byte) {
                        hash = (hash ^ ((uchar *)output)[byte]) * 1099511628211ULL;
                    }
                    ++checks;
                }
            }
        }
    }
    printf("PASS AMD64 emission budget: %d prefixes/capacities, classic/NC64, shifted fields, zero publication; hash=%016llx\n", checks, (unsigned long long)hash);
    return 0;
}
