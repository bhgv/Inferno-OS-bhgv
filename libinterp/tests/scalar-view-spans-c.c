#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"

Type Tstring = {1}, Tarray = {1}; // Exact builtin identity required by native leaf publication.
#include <stdio.h>
#include <stddef.h>
#include <setjmp.h>

typedef unsigned short ViewAddress; // Reduced address domain for span guards only.
REG R;
int mutator, nprop;
char exNilref[] = "nil", exBounds[] = "bounds", exHeap[] = "heap";
static jmp_buf boundary;
static char *raised;


/* Supply allocation support for the included production compiler adapter. */
void*
mallocz(ulong size, int clear)
{
    void *value;

    value = malloc(size);
    if(value != nil && clear)
        memset(value, 0, size);
    return value;
}


/* Run compiler publication callbacks in the fixture's ordinary host context. */
int
kprotect(int (*fn)(void*), void *arg)
{
    return fn(arg);
}


/* Capture exact interpreter error identity before any output can be published. */
void
error(char *message)
{
    raised = message;
    longjmp(boundary, 1);
}


/* Process-based leak inspection is unavailable; the sole fixture allocation is freed. */
int
__lsan_is_turned_off(void)
{
    return 1;
}

#define OP(name) void name(void) // Preserve actual interpreter entry signatures.
IMPLEMENTATION


/* Compare complete owner and operand images at both sides of the reduced boundary. */
int
main(void)
{
    void (*ops[])(void) = {headb, headw, headl, headf, lenl, lenc};
    int widths[] = {sizeof(BYTE), sizeof(WORD), sizeof(LONG), sizeof(REAL), sizeof(void*), sizeof(int)};
    int lengths[] = {0, 1, -7, 0x7fffffff, -0x7fffffff-1};
    uchar *raw, *base, *owner, *expected, *before;
    uchar slots[128], slotsexpected[128];
    uintptr first, offset, extent;
    REG saved;
    void *source, *next;
    WORD result;
    int kind, delta, profile, sa, alias, chain, valid;
    unsigned long checks;

    raw = malloc(0x50000);
    expected = malloc(0x20000);
    before = malloc(0x20000);
    if(raw == nil || expected == nil || before == nil)
        abort();
    first = ((uintptr)raw+0xffff)&~(uintptr)0xffff;
    base = (uchar*)first;
    checks = 0;
    for(kind = 0; kind < 6; kind++)
    for(delta = -16; delta <= 16; delta++)
    for(profile = 0; profile < 7; profile++)
    for(sa = 0; sa < 8; sa++)
    for(alias = 0; alias < 3; alias++)
    for(chain = 0; chain <= (kind == 4); chain++){
        memset(base, 0x5a, 0x20000);
        memset(slots, 0xa5, sizeof(slots));
        offset = kind < 4 ? offsetof(List, data) : kind == 4 ? offsetof(List, tail) : offsetof(String, len);
        extent = offset+widths[kind];
        owner = base+0xffff-extent+delta;
        source = profile == 0 ? nil : profile == 1 ? H : owner;
        if(kind == 4){
            next = H;
            memmove(owner+offset, &next, sizeof(next));
            if(chain){
                memmove(base+0x800+offset, &source, sizeof(source));
                source = base+0x800;
            }
        }else if(kind == 5)
            memmove(owner+offset, &lengths[profile < 2 ? 0 : profile-2], sizeof(int));
        memmove(slots+sa, &source, sizeof(source));
        R.s = slots+sa;
        R.d = alias == 0 ? slots+80+sa : alias == 1 ? slots+sa : owner+offset;
        saved = R;
        memmove(before, base, 0x20000);
        memmove(expected, base, 0x20000);
        memmove(slotsexpected, slots, sizeof(slots));
        raised = nil;
        valid = (profile == 1 && kind >= 4) || (profile >= 2 && ((uintptr)owner&0xffff)+extent <= 0xffff);
        if(kind == 5 && profile == 6)
            valid = 0;
        if(valid){
            if(kind < 4){
                if(alias == 2)
                    memmove(expected+(owner-base)+offset, owner+offset, widths[kind]);
                else
                    memmove(slotsexpected+(alias == 0 ? 80+sa : sa), owner+offset, widths[kind]);
            }else{
                result = kind == 4 ? (profile == 1 ? chain : chain+1) : profile == 1 ? 0 : lengths[profile-2] < 0 ? -lengths[profile-2] : lengths[profile-2];
                if(alias == 2)
                    memmove(expected+(owner-base)+offset, &result, sizeof(result));
                else
                    memmove(slotsexpected+(alias == 0 ? 80+sa : sa), &result, sizeof(result));
            }
        }
        if(setjmp(boundary) == 0)
            ops[kind]();
        if(valid ? raised != nil : raised == nil){
            fprintf(stderr, "error oracle kind=%d delta=%d profile=%d sa=%d alias=%d chain=%d valid=%d raised=%s\n", kind, delta, profile, sa, alias, chain, valid, raised ? raised : "none");
            abort();
        }
        if(!valid && raised != (profile == 0 || (profile == 1 && kind < 4) ? exNilref : kind == 5 ? exHeap : exBounds))
            abort();
        if(memcmp(&saved, &R, sizeof(R)) || memcmp(base, expected, 0x20000) ||
           memcmp(slots, slotsexpected, sizeof(slots)))
            abort();
        checks++;
    }
    free(raw);
    free(expected);
    free(before);
    printf("PASS C scalar view spans: %lu checks WORD%lu\n", checks, (ulong)(sizeof(WORD)*8));
    return 0;
}
