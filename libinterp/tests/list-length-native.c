#include "comp-amd64.c"
#include <stdio.h>
#include <stddef.h>
#include <setjmp.h>
#include <fenv.h>

REG R;
int mutator, nprop;
char exNilref[] = "nil", exBounds[] = "bounds", exHeap[] = "heap";
static jmp_buf boundary;
static char *raised;


/* Allocate compiler metadata; list scans themselves allocate no VM objects. */
void*
mallocz(ulong size, int clear)
{
    void *value = malloc(size);
    if(value && clear) memset(value, 0, size);
    return value;
}


/* Compile ordinary leaves with the production publication ownership path. */
int
kprotect(int (*fn)(void*), void *arg)
{
    return fn(arg);
}


/* Observe exact VM error identity without allowing rejected copies to terminate the host. */
void
error(char *message)
{
    raised = message;
    longjmp(boundary, 1);
}


/* No heap allocations leak; the sandbox cannot perform process-based leak inspection. */
int
__lsan_is_turned_off(void)
{
    return 1;
}

#define OP(name) void name(void) // Preserve production interpreter entry signatures.
IMPLEMENTATION


/* Compare finite, nil-terminated and cyclic graphs before any output publication. */
int
main(void)
{
    Module module = {0};
    Inst code = {0};
    List nodes[514], before[514], expected[514];
    int lengths[] = {0, 1, 2, 3, 7, 15, 31, 63, 127, 128, 129, 255, 256, 257, 513};

    union {
        uvlong alignment; // Stable base for every unaligned owner/result operand.
        uchar bytes[128]; // Slots with independently observed surrounding canaries.
    } slots;
    uchar slotsbefore[128], slotsexpected[128];
    List *owner, *terminal;
    REG saved;
    WORD result;
    int size, profile, sa, da, alias, i, pass, valid, native, flags;
    unsigned long total = 0;

    code.op = ILENL; module.prog = &code; module.nprog = 1;
    if(!compile(&module, 1, nil) || !module.jit || !module.jit->entry[ILENL]) abort();
    for(size = 0; size < sizeof(lengths)/sizeof(lengths[0]); size++) for(profile = 0; profile < 6; profile++)
    for(sa = 0; sa < 8; sa++) for(da = 0; da < 8; da++) for(alias = 0; alias < 3; alias++){
        if(getenv("LIST_LENGTH_CYCLE") && !(size == 1 && profile == 2 && sa == 0 && da == 0 && alias == 0)) continue;
        memset(nodes, 0xa5, sizeof(nodes));
        terminal = profile == 0 ? H : profile == 1 ? nil : nodes+(profile == 2 ? 0 : profile == 3 ? 1% (lengths[size] ? lengths[size] : 1) : profile == 4 ? lengths[size]/2 : lengths[size] ? lengths[size]-1 : 0);
        for(i = 0; i < lengths[size]; i++) nodes[i].tail = i+1 < lengths[size] ? nodes+i+1 : terminal;
        if(lengths[size] == 0 && profile >= 2) nodes[0].tail = nodes;
        owner = lengths[size] ? nodes : terminal;
        memset(slots.bytes, 0xa5, sizeof(slots.bytes));
        R.s = slots.bytes+sa;
        R.d = alias == 0 ? slots.bytes+80+da : alias == 1 ? R.s : (void*)((uchar*)&nodes[0].tail+da);
        memmove(R.s, &owner, sizeof(owner)); saved = R;
        memmove(before, nodes, sizeof(before)); memmove(slotsbefore, slots.bytes, sizeof(slotsbefore));
        valid = profile == 0; native = valid && lengths[size] <= 256; result = lengths[size];
        for(pass = 0; pass < 2; pass++){
            R = saved; memmove(nodes, before, sizeof(before)); memmove(slots.bytes, slotsbefore, sizeof(slotsbefore));
            memmove(expected, before, sizeof(expected)); memmove(slotsexpected, slotsbefore, sizeof(slotsexpected));
            if(pass ? valid : native){
                if(alias == 2) memmove((uchar*)&expected[0].tail+da, &result, sizeof(result));
                else memmove(slotsexpected+(alias ? sa : 80+da), &result, sizeof(result));
            }
            if(fesetround(FE_DOWNWARD)) abort();
            feclearexcept(FE_ALL_EXCEPT); feraiseexcept(FE_INVALID|FE_INEXACT); flags = fetestexcept(FE_ALL_EXCEPT);
            raised = nil;
            if(pass == 0){
                if(module.jit->entry[ILENL](&R) != native) abort();
            }else if(setjmp(boundary) == 0){
                lenl(); if(!valid) abort();
            }else if(valid || raised != (profile == 1 ? exNilref : exHeap)) abort();
            if(memcmp(&R, &saved, sizeof(R)) || memcmp(nodes, expected, sizeof(expected)) ||
               memcmp(slots.bytes, slotsexpected, sizeof(slotsexpected)) || fegetround() != FE_DOWNWARD ||
               fetestexcept(FE_ALL_EXCEPT) != flags) abort();
        }
        total++;
    }
    module.jit->freecode(module.jit->code, module.jit->size); free(module.jit);
    printf("PASS list lengths C/RX: %lu graph/cycle/alias/FP cases WORD%zu\n", total, sizeof(WORD)*8);
    return 0;
}
