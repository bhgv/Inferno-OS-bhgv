#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef unsigned char uchar;
typedef unsigned int uint;
typedef int64_t vlong;
typedef uint64_t uvlong;
#if DIGIT_BITS == 32
typedef uint32_t mpdigit;
#else
typedef uint64_t mpdigit;
#endif
#define nil NULL
typedef struct Fmt Fmt;
#include "mp.h"
#define MAXUINT UINT_MAX
#define MAXINT (UINT_MAX>>1)
#define MININT ((uint)INT_MAX+1)
#define MAXUVLONG UINT64_MAX
#define MAXVLONG INT64_MAX
#define MINVLONG ((uvlong)1<<63)
#define DIGITS(n) (((n)+Dbits-1)/Dbits)

static mpdigit zero_data[1];
static mpint zero = {1, 1, 0, zero_data, 0};
mpint *mpzero = &zero;
static unsigned int checks;

/* Use exact capacities; the production default allocation can hide undersizing. */
mpint*
mpnew(int bits)
{
	mpint *b;
	int size;

	assert(bits >= 0 && bits < 100000);
	size = DIGITS(bits);
	if(size == 0)
		size = 1;
	b = calloc(1, sizeof(*b));
	assert(b);
	b->p = calloc(size, Dbytes);
	assert(b->p);
	b->size = size;
	b->sign = 1;
	return b;
}

/* Grow storage and significant digits according to the actual libmp contract. */
void
mpbits(mpint *b, int bits)
{
	int count;

	assert(bits >= 0 && bits < 100000);
	count = DIGITS(bits);
	if(count > b->size) {
		b->p = realloc(b->p, count*Dbytes);
		assert(b->p);
		b->size = count;
	}
	if(count > b->top) {
		memset(b->p+b->top, 0, (count-b->top)*Dbytes);
		b->top = count;
	}
}

/* Preserve unused digit contents to exercise zero conversions after reuse. */
void
mpassign(mpint *from, mpint *to)
{
	mpbits(to, from->top*Dbits);
	memmove(to->p, from->p, from->top*Dbytes);
	to->top = from->top;
	to->sign = from->sign;
}

/* Canonicalize zero and remove redundant high zero digits. */
void
mpnorm(mpint *b)
{
	while(b->top && b->p[b->top-1] == 0)
		b->top--;
	if(b->top == 0)
		b->sign = 1;
}

/* Release the exact-capacity fixture allocations. */
void
mpfree(mpint *b)
{
	if(b) {
		free(b->p);
		free(b);
	}
}

/* The sandbox does not expose the process inspection used by LeakSanitizer. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Check all result bits, signed saturation, and conversion into reused storage. */
static void
checkscalar(uint64_t value)
{
	mpint *b;
	int64_t signed_value;
	int expected;

	b = uvtomp(value, nil);
	assert(mptouv(b) == value);
	assert(mptov(b) == (value > INT64_MAX ? INT64_MAX : (int64_t)value));
	assert(mptoui(b) == (value > UINT_MAX ? UINT_MAX : (uint)value));
	signed_value = value;
	vtomp(signed_value, b);
	assert(mptov(b) == signed_value);
	assert(mptouv(b) == (signed_value < 0 ? (uint64_t)0-value : value));
	if(signed_value > INT_MAX)
		expected = INT_MAX;
	else if(signed_value < INT_MIN)
		expected = INT_MIN;
	else
		expected = signed_value;
	assert(mptoi(b) == expected);
	itomp((int)value, b);
	assert(mptoi(b) == (int)value);
	uvtomp(0, b);
	assert(b->top == 0 && mptoui(b) == 0 && mptouv(b) == 0);
	mpfree(b);
	checks++;
}

/* Imports must normalize unsigned magnitudes and accept empty NULL input. */
static void
checkbytes(uint64_t value)
{
	uchar be[8], le[8], encoded[16];
	mpint *b;
	int i, n;

	for(i = 0; i < 8; i++) {
		le[i] = value>>(8*i);
		be[7-i] = le[i];
	}
	b = vtomp(-1, nil);
	betomp(be, 8, b);
	assert(b->sign == 1 && mptouv(b) == value);
	n = mptobe(b, encoded, sizeof(encoded), nil);
	assert(n >= 1 && n <= 8 && memcmp(encoded, be+8-n, n) == 0);
	b->sign = -1;
	letomp(le, 8, b);
	assert(b->sign == 1 && mptouv(b) == value);
	n = mptole(b, encoded, sizeof(encoded), nil);
	assert(n >= 0 && n <= 8 && memcmp(encoded, le, n) == 0);
	betomp(nil, 0, b);
	assert(b->top == 0 && b->sign == 1);
	b->sign = -1;
	letomp(nil, 0, b);
	assert(b->top == 0 && b->sign == 1);
	mpfree(b);
	checks++;
}

/* Include full-width limbs, saturating magnitudes and deterministic random bits. */
int
main(void)
{
	uint64_t edges[] = {0, 1, UINT_MAX, INT_MAX, (uint64_t)(int64_t)INT_MIN,
		INT64_MAX, (uint64_t)1<<63, UINT64_MAX, 0x1234567887654321};
	uint64_t random;
	unsigned int i;
	mpint *b;

	for(i = 0; i < sizeof(edges)/sizeof(edges[0]); i++) {
		checkscalar(edges[i]);
		checkbytes(edges[i]);
	}
	random = 0x6d70696e74;
	for(i = 0; i < 2000; i++) {
		random ^= random<<13;
		random ^= random>>7;
		random ^= random<<17;
		checkscalar(random);
		checkbytes(random);
	}
	b = mpnew(128);
	b->top = b->size;
	memset(b->p, 0xff, b->size*Dbytes);
	assert(mptouv(b) == UINT64_MAX);
	assert(mptov(b) == INT64_MAX);
	b->sign = -1;
	assert(mptov(b) == INT64_MIN);
	mpfree(b);
	printf("PASS mp scalar/byte conversions: %u cases, %d-bit digits and exact capacities\n",
		checks, DIGIT_BITS);
	return 0;
}
