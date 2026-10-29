/* Exercise actual Math count guards with module-local primitive descriptors. */
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <limits.h>

#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef uint64_t uvlong;
typedef unsigned char uchar;


/* Only production fields read by the extracted primitive layout guards. */
typedef struct
{
    int size; /* Element bytes. */
    int np; /* Pointer-map bytes; zero identifies a primitive layout. */
} Type;


/* Count guard input, with payload deliberately poisoned to prove preflight. */
typedef struct Array
{
    WORD len; /* Complete VM length, never narrowed before validation. */
    Type *t; /* Borrowed canonical or module-local primitive layout. */
    struct Array *root; /* Unused owner link, matching the production array view. */
    uchar *data; /* Never read by either count guard. */
} Array;

static Type Tbyte = {1, 0}; /* Canonical byte descriptor. */
static Type Tword = {sizeof(WORD), 0}; /* Canonical target-word descriptor. */
static Type Treal = {8, 0}; /* Canonical binary64 descriptor. */
static Type Tlong = {8, 0}; /* Canonical big, distinct even when widths coincide. */
#define H ((void *)1) /* VM nil owner sentinel. */
#define nil NULL /* C metadata nil sentinel. */
enum
{
    exNilref = 1, /* Nil array owner precedes descriptor checks. */
    exType, /* Incompatible primitive layout precedes length checks. */
    exMathia /* Invalid array extent after successful type validation. */
};
static jmp_buf escape; /* Private error boundary; no heap or foreign callbacks. */
static int last_error; /* Copied error from the production guard. */
static unsigned checks; /* Explicit predicates, active under NDEBUG. */


/* Disable unavailable /proc leak inspection; this fixture allocates no heap. */
int __lsan_is_turned_off(void)
{
    return 1;
}


/* Capture one production error without touching the poisoned operand payload. */
static void error(int code)
{
    last_error = code;
    longjmp(escape, 1);
}

IMPLEMENTATION


/* Invoke a real codec/vector preflight with borrowed descriptors. vector selects
 * the vector guard; wanted is zero for a three-element success or an exact error.
 * Return zero with a concrete diagnostic on mismatch. No VM or heap is created. */
static int probe(Array *bytes, Array *numbers, int width, Type *type, int vector, int wanted)
{
    last_error = 0;
    if (setjmp(escape) == 0)
    {
        int count = vector ? mathvectorcount(numbers, type) : matharraycount(bytes, numbers, width, type);
        if (wanted || count != 3)
        {
            fprintf(stderr, "Math local type: wanted error %d, observed count %d\n", wanted, count);
            return 0;
        }
    }
    else if (!wanted || last_error != wanted)
    {
        fprintf(stderr, "Math local type: wanted error %d, observed error %d\n", wanted, last_error);
        return 0;
    }
    ++checks;
    return 1;
}


/* Exercise local/canonical layouts and exact prior error order for WORD32/64.
 * No arguments. Return zero only after all guard paths reject poisoned payloads
 * before data access and admit legitimate typed module-local array layouts. */
int main(void)
{
    Type byte = {1, 0};
    Type word = {sizeof(WORD), 0};
    Type real = {8, 0};
    Type pointers = {8, 1};
    Type wrong = {3, 0};
    int kind;
    for (kind = 0; kind < 2; ++kind)
    {
        Type *expected = kind ? &Treal : &Tword;
        Type *local = kind ? &real : &word;
        int width = kind ? 8 : 4;
        int canonical;
        for (canonical = 0; canonical < 4; ++canonical)
        {
            Array bytes = {3 * width, canonical & 1 ? &Tbyte : &byte, nil, (uchar *)1};
            Array numbers = {3, canonical & 2 ? expected : local, nil, (uchar *)1};
            if (!probe(&bytes, &numbers, width, expected, 0, 0) ||
                !probe(&bytes, &numbers, width, expected, 1, 0))
            {
                return 1;
            }
        }
        {
            Type *invalid[] = {nil, H, &wrong, &pointers, &Tbyte, &Tlong, kind ? &Tword : &Treal};
            unsigned i;
            for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
            {
                Array bytes = {3 * width, &byte, nil, (uchar *)1};
                Array numbers = {3, invalid[i], nil, (uchar *)1};
                if (!probe(&bytes, &numbers, width, expected, 0, exType) ||
                    !probe(&bytes, &numbers, width, expected, 1, exType))
                {
                    return 1;
                }
            }
        }
        {
            Array bytes = {3 * width, &byte, nil, (uchar *)1};
            Array numbers = {-1, local, nil, (uchar *)1};
            if (!probe(&bytes, &numbers, width, expected, 0, exMathia) ||
                !probe(&bytes, &numbers, width, expected, 1, exMathia) ||
                !probe(&bytes, nil, width, expected, 0, exNilref) ||
                !probe(&bytes, H, width, expected, 1, exNilref))
            {
                return 1;
            }
        }
    }
    printf("PASS Math local primitive descriptors: %u predicates WORD%d\n", checks, WORD_BITS);
    return 0;
}
