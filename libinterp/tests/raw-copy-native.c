#include "comp-amd64.c"
#include <stdio.h>
#include <stddef.h>
#include <setjmp.h>
#include <fenv.h>

REG R;
int mutator, nprop;
char exNilref[] = "nil", exBounds[] = "bounds";
static jmp_buf boundary;
static char *raised;


/* Allocate compiler metadata; raw copies themselves allocate no VM objects. */
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

#define OP(name) void name(void) // Preserve the production interpreter signatures.
IMPLEMENTATION


/* Compare byte images, aliases, guard order and FP state for actual C/RX copies. */
int
main(void)
{
    Module module = {0};
    Inst code[2] = {{0}, {0}};
    WORD lengths[] = {0, 1, 2, 7, 8, 15, 31, 64, 127, 128, 129, 255, 256, 257, -1};
    int deltas[] = {-17, -1, 0, 1, 4, 17, 257};

    union {
        uvlong alignment; // Stable native alignment before individual byte offsets are applied.
        uchar bytes[2048]; // Borrowed node/payload storage with large surrounding canaries.
    } data;

    union {
        uvlong alignment; // Stable native alignment for the independent operand image.
        uchar bytes[1024]; // Unaligned scalar, owner and result slots.
    } slots;
    uchar databefore[2048], slotsbefore[1024], dataexpected[2048], slotsexpected[1024];
    uchar *source, *destination, *middle, *cell;
    List *owner;
    REG saved;
    WORD count;
    uintptr start, finish;
    int head, size, profile, sa, ma, delta, pass, valid, native, i, flags;
    char *why;
    unsigned long total = 0;
    char *selection = getenv("RAW_COPY_PROFILE");

    code[0].op = IMOVM; code[1].op = IHEADM;
    module.prog = code; module.nprog = 2;
    if(!compile(&module, 2, nil) || !module.jit ||
       !module.jit->entry[IMOVM] || !module.jit->entry[IHEADM]) abort();
    for(head = 0; head < 2; head++) for(size = 0; size < sizeof(lengths)/sizeof(lengths[0]); size++)
    for(profile = 0; profile < 12; profile++) for(sa = 0; sa < 8; sa++)
    for(ma = 0; ma < 8; ma++) for(delta = 0; delta < sizeof(deltas)/sizeof(deltas[0]); delta++){
        if(selection && profile != atoi(selection)) continue;
        if(getenv("RAW_COPY_HEAD") && head != atoi(getenv("RAW_COPY_HEAD"))) continue;
        for(i = 0; i < sizeof(data.bytes); i++) data.bytes[i] = i*37+11;
        memset(slots.bytes, 0xa5, sizeof(slots.bytes));
        owner = (List*)(data.bytes+512+sa);
        start = (uintptr)owner+(head ? offsetof(List, data) : 0);
        finish = start+deltas[delta]; count = lengths[size];
        cell = slots.bytes+16+sa; middle = slots.bytes+128+ma;
        if(profile == 1){owner = nil; start = head ? offsetof(List, data) : 0;}
        if(profile == 2){owner = H; start = head ? offsetof(List, data)-1 : (uintptr)-1;}
        if(profile == 3 || profile == 4){
            owner = head ? (List*)((uintptr)-1 - offsetof(List, data) + (profile == 3 ? -3 : 1)) : (List*)(uintptr)-4;
            start = (uintptr)owner+(head ? offsetof(List, data) : 0);
            if(count > 0) count = 8;
        }
        if(profile == 5) finish = 0;
        if(profile == 6) finish = (uintptr)-1;
        if(profile == 7){finish = (uintptr)-4; if(count > 0) count = 8;}
        if(profile == 8) middle = data.bytes+512+sa+32;
        if(profile == 9){middle = data.bytes+512+sa+32; finish = (uintptr)middle;}
        if(profile == 10) cell = data.bytes+512+sa+32;
        if(profile == 11) finish = (uintptr)(head ? cell : middle);
        source = head ? cell : (uchar*)start;
        destination = (uchar*)finish;
        if(head) memmove(cell, &owner, sizeof(owner));
        memmove(middle, &count, sizeof(count));
        memset(&R, 0, sizeof(R)); R.s = source; R.d = destination; R.m = middle;
        saved = R;
        memmove(databefore, data.bytes, sizeof(databefore));
        memmove(slotsbefore, slots.bytes, sizeof(slotsbefore));
        valid = 1; why = exBounds;
        if(head && (owner == nil || owner == H)){valid = 0; why = exNilref;}
        else if(count < 0) valid = 0;
        else if(count){
            if(!finish || finish == (uintptr)-1 || (!head && (!start || start == (uintptr)-1))){valid = 0; why = exNilref;}
            else if(head && (uintptr)owner > (uintptr)-1 - offsetof(List, data)) valid = 0;
            else if(start > (uintptr)-1-(uintptr)count || finish > (uintptr)-1-(uintptr)count) valid = 0;
        }
        native = valid && count <= 256;
        for(pass = 0; pass < 2; pass++){
            R = saved;
            memmove(data.bytes, databefore, sizeof(databefore)); memmove(slots.bytes, slotsbefore, sizeof(slotsbefore));
            memmove(dataexpected, databefore, sizeof(dataexpected)); memmove(slotsexpected, slotsbefore, sizeof(slotsexpected));
            if((pass ? valid : native) && count){
                if(finish >= (uintptr)data.bytes && finish < (uintptr)data.bytes+sizeof(data.bytes))
                    memmove(dataexpected+(finish-(uintptr)data.bytes), databefore+(start-(uintptr)data.bytes), count);
                else memmove(slotsexpected+(finish-(uintptr)slots.bytes), databefore+(start-(uintptr)data.bytes), count);
            }
            if(fesetround(FE_DOWNWARD)) abort();
            feclearexcept(FE_ALL_EXCEPT); feraiseexcept(FE_INVALID|FE_INEXACT);
            flags = fetestexcept(FE_ALL_EXCEPT); raised = nil;
            if(pass == 0){
                if(module.jit->entry[code[head].op](&R) != native) abort();
            }else if(setjmp(boundary) == 0){
                if(head) headm(); else movm();
                if(!valid) abort();
            }else if(valid || raised != why) abort();
            if(memcmp(&R, &saved, sizeof(R)) || memcmp(data.bytes, dataexpected, sizeof(dataexpected)) ||
               memcmp(slots.bytes, slotsexpected, sizeof(slotsexpected)) || fegetround() != FE_DOWNWARD ||
               fetestexcept(FE_ALL_EXCEPT) != flags) abort();
        }
        total++;
    }
    module.jit->freecode(module.jit->code, module.jit->size); free(module.jit);
    printf("PASS raw copies C/RX: %lu image/alias/span/error/FP cases WORD%zu\n", total, sizeof(WORD)*8);
    return 0;
}
