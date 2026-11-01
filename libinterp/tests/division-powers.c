/* Owning production scalar handlers with independent Python integer vectors. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#if WORD_BITS == 32
typedef int32_t WORD; /* Actual scalar model selected by this test. */
typedef uint32_t UWORD; /* Same-width modular companion. */
#else
typedef int64_t WORD; /* Actual scalar model selected by this test. */
typedef uint64_t UWORD; /* Same-width modular companion. */
#endif
typedef int64_t LONG; /* Dis big remains64 bits in either WORD model. */
typedef uint64_t ULONG; /* Exact unsigned big representation. */
typedef uintptr_t uintptr; /* Host address bounds used by the unchanged bodies. */
#define nil NULL /* Existing nil spelling in owning handlers. */
#define H ((void *)(uintptr)-1) /* Managed empty value sentinel. */
#define OP(name) static void name(void) /* No instruction decoder is modeled. */
#define exBounds 1 /* Bounded address refusal. */
#define exZdiv 2 /* Zero divisor has priority over other operand access. */

static struct {
    void *s; /* Borrowed divisor storage. */
    void *m; /* Borrowed dividend storage. */
    void *d; /* Borrowed destination, possibly aliased/unaligned. */
} R;
static jmp_buf failure; /* Test-only synchronous error transport. */
static int diagnostic; /* Last owning error identifier, reset for every call. */


/* Preserve the production error boundary without invoking any OS or scheduler.
 * code is the owning error identifier. This function does not return normally.
 */
static void error(int code)
{
    diagnostic = code;
    longjmp(failure, 1);
}

IMPLEMENTATION
VECTORS


/* Exercise real div/mod over every copied vector and all three destination
 * placements. Source and target byte buffers are owned here; misalignment and
 * aliases are intentional. Check exact results plus every non-destination byte.
 * Return0 with a diagnostic on any mismatch, otherwise1. No heap or GC claim.
 */
static int exercise(const int64_t (*vectors)[4], unsigned count, unsigned bytes,
                    void (*divide)(void), void (*modulo)(void))
{
    unsigned row; /* Current independent arithmetic vector. */
    unsigned alias; /* Separate target, divisor alias or dividend alias. */
    unsigned operation; /* Quotient or remainder. */
    for (row = 0; row < count; row++)
    {
        for (alias = 0; alias < 3; alias++)
        {
            for (operation = 0; operation < 2; operation++)
            {
                unsigned char arena[96]; /* Three disjoint storage regions. */
                unsigned char before[96]; /* Non-destination byte oracle. */
                unsigned char expected[8]; /* Native-order expected scalar. */
                unsigned offset = alias == 1 ? 1 : alias == 2 ? 35 : 69;
                unsigned index; /* Byte outside/inside the destination. */
                WORD word; /* Correct native scalar encoding, including big-endian hosts. */
                LONG big; /* Correct native big encoding. */
                memset(arena, 0xa7, sizeof(arena));
                if (bytes == sizeof(WORD) && divide == divw)
                {
                    word = (WORD)vectors[row][0];
                    memcpy(arena + 35, &word, bytes);
                    word = (WORD)vectors[row][1];
                    memcpy(arena + 1, &word, bytes);
                    word = (WORD)vectors[row][operation + 2];
                    memcpy(expected, &word, bytes);
                }
                else
                {
                    big = (LONG)vectors[row][0];
                    memcpy(arena + 35, &big, bytes);
                    big = (LONG)vectors[row][1];
                    memcpy(arena + 1, &big, bytes);
                    big = (LONG)vectors[row][operation + 2];
                    memcpy(expected, &big, bytes);
                }
                memcpy(before, arena, sizeof(arena));
                R.s = arena + 1;
                R.m = arena + 35;
                R.d = arena + offset;
                diagnostic = 0;
                if (setjmp(failure))
                {
                    fprintf(stderr, "Unexpected division error%d row%u\n", diagnostic, row);
                    return 0;
                }
                if (operation)
                {
                    modulo();
                }
                else
                {
                    divide();
                }
                for (index = 0; index < sizeof(arena); index++)
                {
                    unsigned char wanted = index >= offset && index < offset + bytes
                        ? expected[index-offset] : before[index];
                    if (arena[index] != wanted)
                    {
                        fprintf(stderr, "Division byte mismatch row%u alias%u op%u byte%u\n",
                                row, alias, operation, index);
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}


/* A zero divisor must be refused before touching an invalid dividend/target.
 * Handler is borrowed actual production code; zero points to sufficient native
 * zero bytes for either width. Return1 only for the exact expected exception.
 */
static int refuse(void (*handler)(void), void *zero)
{
    R.s = zero;
    R.m = H;
    R.d = nil;
    diagnostic = 0;
    if (!setjmp(failure))
    {
        handler();
        fputs("Zero divisor did not fail\n", stderr);
        return 0;
    }
    if (diagnostic != exZdiv)
    {
        fputs("Zero divisor lost error precedence\n", stderr);
        return 0;
    }
    return 1;
}


/* Run both actual scalar kinds. The WORD64 build also retests distinct big
 * handlers; function identity never substitutes for arithmetic evidence.
 */
int main(void)
{
    WORD word = 0; /* Native zero divisor for divw/modw. */
    LONG big = 0; /* Native zero divisor for divl/modl. */
    if (!exercise(word_vectors, sizeof(word_vectors)/sizeof(word_vectors[0]), sizeof(WORD), divw, modw) ||
        !exercise(big_vectors, sizeof(big_vectors)/sizeof(big_vectors[0]), sizeof(LONG), divl, modl) ||
        !refuse(divw, &word) || !refuse(modw, &word) ||
        !refuse(divl, &big) || !refuse(modl, &big))
    {
        return 1;
    }
    printf("PASS actual signed division WORD%d: %lu independent vectors, 3 alias placements, unaligned bytes and zero-first errors\n",
           WORD_BITS, (unsigned long)(sizeof(word_vectors)/sizeof(word_vectors[0]) + sizeof(big_vectors)/sizeof(big_vectors[0])));
    return 0;
}
