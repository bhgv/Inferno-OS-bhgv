/* Independent small-state oracle for the actual private caller header.
 * Scheduler identity and descriptor retirement are bounded fixtures here;
 * owning VM/GC/clean-boot tests provide separate integration evidence. */
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "native64-caller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STEPS 5 /* Every word over eight operations, including invalid orders. */


/* Fixture operations are not VM opcodes or serialized feature identifiers. */
enum SequenceOperation
{
    SeqAcquire, /* Try to acquire the original empty lease. */
    SeqInvalidate, /* Advance without wrapping the process generation. */
    SeqResolve, /* Check the exact current scalar result address. */
    SeqCopy, /* A copied lease must neither resolve nor retire its original. */
    SeqRebind, /* Transfer between two current valid frame boundaries. */
    SeqClear, /* Retire the original hold and Type once. */
    SeqExhaust, /* Probe the final three generation values. */
    SeqDesync, /* Change live FP independently of its saved publication. */
    SeqOperationCount /* Bound the exhaustive alphabet. */
};

REG R; /* Actual live-register symbol required by the production header. */
static Prog subject; /* Bounded current scheduler root; never a detached lifetime claim. */


/* Report an active semantic predicate, independent of sanitizer diagnostics. */
static void check(int valid, const char *message)
{
    if (!valid)
    {
        fprintf(stderr, "FAIL caller sequence: %s\n", message);
        exit(1);
    }
}


/* Bounded scheduler identity; actual port lifecycle is tested separately. */
Prog *currun(void)
{
    return &subject;
}


/* Model a coherently linked owning runtime, not its link-time guard proof. */
int native64callerruntime(void)
{
    return 1;
}


/* Retire a descriptor reference without freeing the fixture's initial owner. */
void freetype(Type *type)
{
    check(type != nil && type->ref > 1, "descriptor retirement balanced");
    type->ref--;
}


/* Unused enter helper's failure is still active if a compiler retains its body. */
void error(char *message)
{
    check(0, message);
}


/* Execute one sequence against independent scalar state. frame/type storage is
 * owned by this call and survives through final clear. live selects the exact
 * global-R profile; otherwise the saved-register trusted-C profile is checked. */
static void sequence(unsigned int word, int live)
{
    union {
        uvlong align;
        uchar bytes[sizeof(Stkext) + 256];
    } stack;
    union {
        uvlong align;
        uchar bytes[sizeof(Type) + 8];
    } descriptor;
    Native64Caller caller;
    Type *type = (Type *)descriptor.bytes;
    uchar *base = stack.bytes + OA(Stkext, reg.tos.fu);
    uvlong epoch = 1;
    uvlong captured = 0;
    int held = 0;
    int saved = 0;
    int running = 0;
    int frame = 0;
    unsigned int step;
    memset(&subject, 0, sizeof(subject));
    memset(&stack, 0, sizeof(stack));
    memset(&descriptor, 0, sizeof(descriptor));
    memset(&caller, 0, sizeof(caller));
    type->ref = 1;
    type->size = 80;
    type->np = 2;
    ((Frame *)base)->t = type;
    ((Frame *)(base + 80))->t = type;
    subject.pid = 17;
    subject.state = Pready;
    subject.native64epoch = epoch;
    subject.R.EX = stack.bytes;
    subject.R.FP = base;
    subject.R.SP = base + 160;
    subject.R.TS = base + 160;
    R = subject.R;
    subject.native64live = live ? &R : nil;
    for (step = 0; step < STEPS; step++)
    {
        unsigned int operation = word % SeqOperationCount;
        int ready = held && epoch == captured && epoch != ~(uvlong)0 &&
                    (!live || saved == running);
        int accepted;
        word /= SeqOperationCount;
        switch (operation)
        {
        case SeqAcquire:
            accepted = !held && epoch < ~(uvlong)0 - 1;
            check((live ? native64callerbeginlive(&caller, &subject, type, 72)
                        : native64callerbegin(&caller, &subject, type, 72)) == accepted,
                  "acquisition predicate");
            if (accepted)
            {
                held = 1;
                if (live)
                {
                    saved = running;
                }
                frame = saved;
                captured = ++epoch;
            }
            break;
        case SeqInvalidate:
            native64calleradvance(&subject);
            if (epoch != ~(uvlong)0)
            {
                epoch++;
            }
            break;
        case SeqResolve:
            check(native64callerresolve(&caller) == (ready ? base + frame * 80 + 72 : nil),
                  "generation/coherence-gated result");
            break;
        case SeqCopy:
            {
                Native64Caller copy = caller;
                check(native64callerresolve(&copy) == nil, "copied lease cannot resolve");
                native64callerclear(&copy);
            }
            break;
        case SeqRebind:
            accepted = ready && epoch < ~(uvlong)0 - 1;
            check(native64callerrebind(&caller, base + (1 - frame) * 80) == accepted,
                  "checked frame transfer predicate");
            if (accepted)
            {
                frame = 1 - frame;
                saved = frame;
                if (live)
                {
                    running = frame;
                }
                captured = ++epoch;
            }
            break;
        case SeqClear:
            native64callerclear(&caller);
            held = 0;
            break;
        case SeqExhaust:
            epoch = ~(uvlong)0 - word % 3;
            subject.native64epoch = epoch;
            break;
        case SeqDesync:
            running = 1 - running;
            R.FP = base + running * 80;
            break;
        }
        check(subject.native64epoch == epoch, "epoch never wraps or resets");
        check(subject.native64holds == (unsigned int)held, "exactly one scheduler hold");
        check(type->ref == 1 + held, "exact descriptor retention");
        check(subject.R.FP == base + saved * 80, "saved frame publication");
        check(R.FP == base + running * 80, "live frame publication");
    }
    native64callerclear(&caller);
    native64callerclear(&caller);
    check(subject.native64holds == 0 && type->ref == 1, "idempotent final retirement");
}


/* Enumerate every length-five operation word in both header profiles. */
int main(void)
{
    unsigned int words = 1;
    unsigned int index;
    unsigned int word;
    int live;
    for (index = 0; index < STEPS; index++)
    {
        words *= SeqOperationCount;
    }
    for (live = 0; live < 2; live++)
    {
        for (word = 0; word < words; word++)
        {
            sequence(word, live);
        }
    }
    printf("PASS independent caller sequences: %u traces, published/live, holds/Type/epoch/transfer\n", words * 2);
    return 0;
}
