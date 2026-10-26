#include "dat.h"
#include "fns.h"
#include "interp.h"
#include "isa.h"
#include "raise.h"
#include <setjmp.h>
#include <stdio.h>

REG R; /* Extracted operator's real register context; never published on a
          failure. */
char exNilref[] = "nil";    /* Required nil String error. */
char exHeap[] = "heap";     /* Invalid slot/header error. */
char exBounds[] = "bounds"; /* Valid String with an invalid character index. */
static jmp_buf
    boundary; /* Test-owned recovery frame for the VM error callback. */
static char
    *raised; /* Exact raised error identity, cleared before each call. */

/* Capture the VM error without resuming the failed operation or changing its
 * registers. */
void error(char *message)
{
    raised = message;
    longjmp(boundary, 1);
}

/* Disable process-based leak inspection; main frees its only owned allocation.
 */
int __lsan_is_turned_off(void) { return 1; }

#define OP(name) void name(void)
IMPLEMENTATION

/* Validate real decoded slots/headers and byte-aligned operands for both VM
 * WORD widths. No arguments; failed checks print a diagnostic and return
 * nonzero. Owner storage is private. */
int main(void)
{
    union
    {
        uvlong align;     /* Natural alignment of the underlying storage. */
        uchar bytes[256]; /* Unaligned operand slots and unchanged destination
                             canaries. */
    } slots;
    uchar before[256];
    String *owner, *source;
    REG saved;
    WORD index, output;
    Rune rune;
    char *expected;
    int alignment, kind, field, delta, length, checks = 0;

    owner = malloc(sizeof(String) + 32 * sizeof(Rune));
    if (owner == nil)
    {
        return 1;
    }
    for (alignment = 0; alignment < 8; alignment++)
    {
        for (kind = 0; kind < 10; kind++)
        {
            for (field = 0; field < 3; field++)
            {
                for (delta = 0; delta < (int)sizeof(void *); delta++)
                {
                    if (getenv("INDC_CASE") != nil &&
                        kind != atoi(getenv("INDC_CASE")))
                    {
                        continue;
                    }
                    memset(owner, 0, sizeof(String) + 32 * sizeof(Rune));
                    memset(&slots, 0xa5, sizeof(slots));
                    owner->len = kind == 1 ? -2 : 2;
                    owner->max = 2;
                    owner->Sascii[0] = 'A';
                    owner->Sascii[1] = 'B';
                    if (kind == 1)
                    {
                        rune = 0x20ac;
                        memmove(owner->Srune, &rune, sizeof(rune));
                    }
                    source = owner;
                    index = 0;
                    expected = nil;
                    if (kind == 2)
                    {
                        source = nil;
                        expected = exNilref;
                    }
                    if (kind == 3)
                    {
                        source = H;
                        expected = exNilref;
                    }
                    if (kind == 4)
                    {
                        source = (String *)((uintptr)-1 - delta);
                        expected = delta == 0 ? exNilref : exHeap;
                    }
                    if (kind == 5)
                    {
                        owner->len = (-0x7fffffff - 1);
                        expected = exHeap;
                    }
                    if (kind == 6)
                    {
                        owner->max = 1;
                        expected = exHeap;
                    }
                    if (kind == 7)
                    {
                        index = -1;
                        expected = exBounds;
                    }
                    if (kind == 8)
                    {
                        index = 2;
                        expected = exBounds;
                    }
                    memmove(slots.bytes + alignment, &source, sizeof(source));
                    memmove(slots.bytes + 32 + alignment, &index,
                            sizeof(index));
                    memset(&R, 0, sizeof(R));
                    R.s = slots.bytes + alignment;
                    R.m = slots.bytes + 32 + alignment;
                    R.d = slots.bytes + 64 + alignment;
                    if (kind == 9)
                    {
                        void *invalid =
                            delta == 0 ? nil
                            : delta == 1
                                ? H
                                : (void *)((uintptr)-1 -
                                           (delta - 2) % (field == 0
                                                              ? sizeof(void *)
                                                              : sizeof(WORD)));
                        if (field == 0)
                        {
                            R.s = invalid;
                        }
                        if (field == 1)
                        {
                            R.m = invalid;
                        }
                        if (field == 2)
                        {
                            R.d = invalid;
                        }
                        expected = exHeap;
                    }
                    saved = R;
                    memmove(before, slots.bytes, sizeof(before));
                    raised = nil;
                    if (setjmp(boundary) == 0)
                    {
                        indc();
                    }
                    if (raised != expected || memcmp(&saved, &R, sizeof(R)))
                    {
                        fprintf(stderr,
                                "INDC preflight mismatch kind=%d field=%d "
                                "delta=%d alignment=%d\n",
                                kind, field, delta, alignment);
                        free(owner);
                        return 2;
                    }
                    if (expected == nil)
                    {
                        output = kind == 1 ? 0x20ac : 'A';
                        memmove(before + 64 + alignment, &output,
                                sizeof(output));
                    }
                    if (memcmp(before, slots.bytes, sizeof(before)))
                    {
                        fputs("INDC changed bytes outside successful "
                              "destination\n",
                              stderr);
                        free(owner);
                        return 3;
                    }
                    checks++;
                }
            }
        }
    }
    length = sizeof(WORD) * 8;
    printf("PASS INDC decoded slots/header preflight: %d checks WORD%d\n",
           checks, length);
    free(owner);
    return 0;
}
