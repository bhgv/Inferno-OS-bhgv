#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <stdio.h>
#include <setjmp.h>

REG R;
Type Tstring = {1};
char exNilref[] = "nil", exType[] = "type", exNomem[] = "memory", exHeap[] = "heap";
static Prog current;
static jmp_buf boundary;
static char *raised;
static int allocationfailure, attempts;


/* Capture the VM error or prepared message pointer after the nonlocal raise. */
void
error(char *text)
{
    raised = text;
    longjmp(boundary, 1);
}


/* Use stable process storage to observe exception-owner publication after errors. */
Prog*
currun(void)
{
    return &current;
}


/* Force cache refreshes so preparation failure and untouched-cache guards are visible. */
ulong
msize(void *pointer)
{
    (void)pointer;
    return 0;
}


/* Count every fallible cache attempt independently of fixture allocations. */
static void*
cachemalloc(ulong size)
{
    attempts++;
    return allocationfailure ? nil : malloc(size);
}


/* Process-based leak inspection is unavailable; ordinary stack/bounds checks stay enabled. */
int
__lsan_is_turned_off(void)
{
    return 1;
}

#define OP(name) void name(void) // Keep the production VM entry signatures.
#define malloc cachemalloc // Inject only production message cache allocations.
IMPLEMENTATION
#undef malloc // Fixture construction uses ordinary independent native allocation.


/* Construct a real heap-prefixed message with observable old cache and terminator bytes. */
static String*
textstring(int wide)
{
    Heap *heap;
    String *text;

    heap = calloc(1, sizeof(Heap)+sizeof(String)+64);
    if(heap == nil) abort();
    heap->ref = 1; heap->t = &Tstring; text = H2D(String*, heap);
    text->max = wide ? 64/sizeof(Rune) : 64;
    text->tmp = malloc(16);
    if(text->tmp == nil) abort();
    memset(text->tmp, 0xa5, 16);
    memmove(text->tmp, "old-cache", 10);
    if(wide){text->len = -2; text->Srune[0] = 'e'; text->Srune[1] = 'r';}
    else{text->len = 2; memmove(text->Sascii, "erx", 3);}
    return text;
}


/* Check source/message lifetime, counter limits and cache/publication ordering at all alignments. */
int
main(void)
{
    uchar slot[32], before[sizeof(String)+64];
    char oldcache[16];
    String *message, *text;
    Heap *tuple;
    Type type = {0}, wrong = {0};
    void *value;
    REG saved;
    ulong counters[] = {1, 0, (ulong)-2, (ulong)-1};
    ulong ownercount, messagecount;
    char *expected;
    int alignment, mode, fail, sourcecase, messagecase, accepted, wide;
    unsigned long checks = 0;

    for(alignment = 0; alignment < 8; alignment++) for(mode = 0; mode < 16; mode++)
    for(fail = 0; fail < 2; fail++) for(sourcecase = 0; sourcecase < 4; sourcecase++)
    for(messagecase = 0; messagecase < 4; messagecase++){
        if(getenv("RAISE_MODE") && mode != atoi(getenv("RAISE_MODE"))) continue;
        if(getenv("RAISE_COUNTER") && sourcecase != atoi(getenv("RAISE_COUNTER"))) continue;
        wide = mode == 1 || mode == 3;
        text = textstring(wide); message = text;
        tuple = calloc(1, sizeof(Heap)+sizeof(void*));
        if(tuple == nil) abort();
        tuple->ref = 1; type.size = sizeof(void*); tuple->t = &type;
        value = mode < 2 ? (void*)text : H2D(void*, tuple);
        D2H(text)->ref = counters[messagecase];
        if(mode == 4) value = nil;
        if(mode == 5) value = H;
        if(value != nil && value != H) D2H(value)->ref = counters[sourcecase];
        if(mode == 6) tuple->t = nil;
        if(mode == 7) tuple->t = H;
        if(mode == 8) type.size = sizeof(void*)-1;
        if(mode == 9) message = nil;
        if(mode == 10) message = H;
        if(mode == 11) text->len = (-0x7fffffff-1);
        if(mode == 12) D2H(text)->ref = 0;
        if(mode == 13) D2H(text)->t = &wrong;
        if(mode == 14){tuple->ref = 0; tuple->t = (Type*)(uintptr)1;}
        if(mode == 15){tuple->ref = (ulong)-1; tuple->t = (Type*)(uintptr)1;}
        memmove(H2D(void*, tuple), &message, sizeof(message));
        memset(&R, 0, sizeof(R)); R.s = slot+alignment; memmove(R.s, &value, sizeof(value)); saved = R;
        memmove(before, text, sizeof(before)); memmove(oldcache, text->tmp, sizeof(oldcache));
        ownercount = value == nil || value == H ? 0 : D2H(value)->ref;
        messagecount = D2H(text)->ref;
        expected = nil;
        if(value == nil || value == H) expected = exNilref;
        else if(ownercount == 0 || ownercount == (ulong)-1) expected = exHeap;
        else if(mode >= 6 && mode <= 8) expected = exType;
        else if(message == nil) expected = exNilref;
        else if(message != H){
            if(D2H(text)->t != &Tstring) expected = exType;
            else if(messagecount == 0) expected = exHeap;
            else if(mode == 11 || (fail && wide)) expected = exNomem;
        }
        accepted = expected == nil;
        current.exval = H; allocationfailure = fail; attempts = 0; raised = nil;
        if(setjmp(boundary) == 0){iraise(); abort();}
        if(memcmp(&R, &saved, sizeof(R))) abort();
        if(accepted){
            if(current.exval != value || D2H(value)->ref != ownercount+1 ||
               strcmp(raised, message == H ? "" : "er")) abort();
            if(value != text && D2H(text)->ref != messagecount) abort();
        }else{
            if(current.exval != H || raised != expected ||
               memcmp(text, before, sizeof(before)) || memcmp(text->tmp, oldcache, sizeof(oldcache))) abort();
            if(value != nil && value != H && D2H(value)->ref != ownercount) abort();
            if(expected != exNomem && attempts != 0) abort();
        }
        free(text->tmp); free(D2H(text)); free(tuple); checks++;
    }
    printf("PASS raise slots: %lu lifetime/counter/cache/publication cases WORD%zu\n", checks, sizeof(WORD)*8);
    return 0;
}
