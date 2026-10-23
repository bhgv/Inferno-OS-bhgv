#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#if WORD_BITS == 32
typedef int32_t WORD;
typedef uint32_t UWORD;
#else
typedef int64_t WORD;
typedef uint64_t UWORD;
#endif
typedef int64_t LONG;
typedef uint64_t ULONG;
static struct {
	void *s, *m, *d;
} R;
#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define UW(r) (*(UWORD*)R.r)
#define V(r) (*(LONG*)R.r)
#define UV(r) (*(ULONG*)R.r)
static char exInval[] = "invalid";

/* Valid-count arithmetic tests must never enter the VM error path. */
static void
error(char *message)
{
	(void)message;
	abort();
}

IMPLEMENTATION

static unsigned int checks;
static uint64_t seed = 0x6c696d626f;

/* Deterministic random bits supplement the signed and unsigned boundaries. */
static uint64_t
nextvalue(void)
{
	seed ^= seed << 13;
	seed ^= seed >> 7;
	seed ^= seed << 17;
	return seed;
}

/* Every operation must capture sources correctly when either is the destination. */
static void
checkpair(uint64_t x, uint64_t y)
{
	void (*words[])(void) = {addw, subw, mulw};
	void (*bigs[])(void) = {addl, subl, mull};
	uint64_t expected[] = {y+x, y-x, y*x};
	WORD a, b, result;
	LONG c, d, bigresult;
	int op, alias;

	for(op = 0; op < 3; op++)
		for(alias = 0; alias < 3; alias++) {
			a = x;
			b = y;
			result = 123;
			R.s = &a;
			R.m = &b;
			R.d = alias == 0 ? &result : alias == 1 ? &a : &b;
			words[op]();
			assert(*(UWORD*)R.d == (UWORD)expected[op]);
			c = x;
			d = y;
			bigresult = 456;
			R.s = &c;
			R.m = &d;
			R.d = alias == 0 ? &bigresult : alias == 1 ? &c : &d;
			bigs[op]();
			assert(*(ULONG*)R.d == expected[op]);
			checks += 2;
		}
}

/* Only language-valid counts are tested; undefined counts remain outside scope. */
static void
checkshift(uint64_t value)
{
	WORD count, word;
	LONG big;

	for(count = 0; count < WORD_BITS; count++) {
		word = value;
		R.s = &count;
		R.m = &word;
		R.d = &word;
		shlw();
		assert((UWORD)word == (UWORD)((UWORD)value << count));
		checks++;
	}
	for(count = 0; count < 64; count++) {
		big = value;
		R.s = &count;
		R.m = &big;
		R.d = &big;
		shll();
		assert((ULONG)big == value << count);
		checks++;
	}
}

/* Check wraparound and sign-bit shifts in both supported VM scalar models. */
int
main(void)
{
	uint64_t edges[] = {0, 1, UINT64_MAX, INT64_MAX, (uint64_t)INT64_MIN,
		INT32_MAX, (uint64_t)(int64_t)INT32_MIN, UINT32_MAX};
	unsigned int a, b;

	for(a = 0; a < sizeof(edges)/sizeof(edges[0]); a++) {
		for(b = 0; b < sizeof(edges)/sizeof(edges[0]); b++)
			checkpair(edges[a], edges[b]);
		checkshift(edges[a]);
	}
	for(a = 0; a < 256; a++) {
		uint64_t x, y;

		x = nextvalue();
		y = nextvalue();
		checkpair(x, y);
		checkshift(x);
	}
	printf("PASS VM integer operations: %u arithmetic/alias/shift checks, WORD%d\n",
		checks, WORD_BITS);
	return 0;
}
