#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include "isa.h"

#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef unsigned char uchar;
typedef double REAL;
FRAMES
#define __HI(x) (((int32_t*)&(x))[HI_INDEX])
#define __LO(x) (((int32_t*)&(x))[LO_INDEX])
#define __HIp(x) (((int32_t*)(x))[HI_INDEX])
#define __LOp(x) (((int32_t*)(x))[LO_INDEX])
static char exInval[] = "invalid numeric conversion";
static jmp_buf boundary;
static int failed, checks;

/* Observe numeric conversion exceptions without a running VM process. */
static void
error(char *message)
{
	assert(message == exInval);
	failed = 1;
	longjmp(boundary, 1);
}

/* LeakSanitizer process inspection is unavailable; this fixture has no heap allocations. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Compare exact numeric results and zero signs without requiring a stable NaN payload. */
static int
same(double actual, double expected)
{
	if(isnan(expected))
		return isnan(actual);
	if(actual != expected)
		return 0;
	return actual != 0 || !!signbit(actual) == !!signbit(expected);
}

/* Use the host libm for arithmetic and explicit deterministic signed-zero selection. */
static double
reference(int op, double x, double y)
{
	if(op == 0)
		return fdim(x, y);
	if(x == 0 && y == 0){
		if(op == 1)
			return signbit(x) || signbit(y) ? -0.0 : 0.0;
		return !signbit(x) || !signbit(y) ? 0.0 : -0.0;
	}
	return op == 1 ? fmin(x, y) : fmax(x, y);
}

/* Run both native functions and generated builtin frames, including aliased result slots. */
static void
pair(double x, double y, int selected)
{
	F_Math_fdim dim;
	F_Math_fmin min;
	F_Math_fmax max;
	double actual, expected;
	int op, alias;

	for(op = 0; op < 3; op++){
		if(selected >= 0 && op != selected)
			continue;
		expected = reference(op, x, y);
		actual = op == 0 ? tested_fdim(x, y) : op == 1 ? tested_fmin(x, y) : tested_fmax(x, y);
		if(!same(actual, expected))
			fprintf(stderr, "pair mismatch op=%d x=%a y=%a got=%a expected=%a\n", op, x, y, actual, expected);
		assert(same(actual, expected));
		checks++;
		for(alias = 0; alias < 3; alias++){
			memset(&dim, 0xa5, sizeof dim);
			memset(&min, 0xa5, sizeof min);
			memset(&max, 0xa5, sizeof max);
			if(op == 0){
				dim.x = x; dim.y = y;
				dim.ret = alias == 1 ? &dim.x : alias == 2 ? &dim.y : &actual;
				Math_fdim(&dim);
				actual = *dim.ret;
			}else if(op == 1){
				min.x = x; min.y = y;
				min.ret = alias == 1 ? &min.x : alias == 2 ? &min.y : &actual;
				Math_fmin(&min);
				actual = *min.ret;
			}else{
				max.x = x; max.y = y;
				max.ret = alias == 1 ? &max.x : alias == 2 ? &max.y : &actual;
				Math_fmax(&max);
				actual = *max.ret;
			}
			assert(same(actual, expected));
			checks++;
		}
	}
}

/* Keep exception-observed frame storage outside automatic setjmp-local variables. */
static union {
	F_Math_modf frame;
	double alignment;
	uchar bytes[256];
} modfframe;
static __typeof__(*((F_Math_modf*)0)->ret) modfresult;

/* Check native decomposition, VM integer truncation and atomic tuple publication. */
static void
split(double x, int nativeonly)
{
	double expected, integer, actual, nativeinteger, bound;
	uchar before[sizeof modfresult];
	uchar padding[sizeof modfresult];
	void *output;
	int alias, valid;

	expected = modf(x, &integer);
	actual = tested_modf(x, &nativeinteger);
	assert(same(actual, expected) && same(nativeinteger, integer));
	checks++;
	if(nativeonly)
		return;
	bound = ldexp(1.0, WORD_BITS-1);
	valid = isfinite(integer) && integer >= -bound && integer < bound;
	for(alias = 0; alias < 2; alias++){
		memset(&modfframe, 0xa5, sizeof modfframe);
		memset(&modfresult, 0xa5, sizeof modfresult);
		modfframe.frame.x = x;
		modfframe.frame.ret = alias ? (__typeof__(modfframe.frame.ret))&modfframe.frame.x : &modfresult;
		output = modfframe.frame.ret;
		memcpy(before, output, sizeof before);
		failed = 0;
		if(setjmp(boundary) == 0)
			Math_modf(&modfframe.frame);
		if(valid){
			assert(!failed && modfframe.frame.ret->t0 == (WORD)integer);
			assert(same(modfframe.frame.ret->t1, expected));
			/* Padding remains untouched even when the result aliases the input slot. */
			memcpy(padding, output, sizeof padding);
			memcpy(padding, before, sizeof(WORD));
			memcpy(padding+__builtin_offsetof(__typeof__(modfresult), t1),
				before+__builtin_offsetof(__typeof__(modfresult), t1), sizeof(double));
			assert(memcmp(padding, before, sizeof before) == 0);
		}else
			assert(failed && memcmp(output, before, sizeof before) == 0);
		checks++;
	}
}

/* Compare quiet NaNs; signaling-NaN exception policy depends on the host FP environment. */
static uint64_t
quietbits(uint64_t bits)
{
	if((bits & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000) &&
	   (bits & UINT64_C(0x000fffffffffffff)) != 0)
		bits |= UINT64_C(0x0008000000000000);
	return bits;
}

/* Exercise finite/subnormal limits, both zero signs, infinities and several NaN encodings. */
int
main(int argc, char **argv)
{
	union {uint64_t raw; double value;} x, y;
	uint64_t state = UINT64_C(0x6d61746873706563);
	double values[] = {0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 1.75, -1.75,
		0x1p-1074, -0x1p-1074, 0x1p-1022, -0x1p-1022,
		0x1.fffffffffffffp1023, -0x1.fffffffffffffp1023,
		INFINITY, -INFINITY, NAN, -NAN, 0x1p31, -0x1p31, 0x1p63, -0x1p63};
	int i, j, pairs, splits, selected, nativeonly;

	pairs = argc == 1 || strcmp(argv[1], "pairs") == 0 || strcmp(argv[1], "fdim") == 0 ||
		strcmp(argv[1], "fmin") == 0 || strcmp(argv[1], "fmax") == 0;
	splits = argc == 1 || strcmp(argv[1], "modf") == 0 || strcmp(argv[1], "native-modf") == 0;
	selected = argc > 1 && strcmp(argv[1], "fdim") == 0 ? 0 :
		argc > 1 && strcmp(argv[1], "fmin") == 0 ? 1 :
		argc > 1 && strcmp(argv[1], "fmax") == 0 ? 2 : -1;
	nativeonly = argc > 1 && strcmp(argv[1], "native-modf") == 0;
	for(i = 0; i < sizeof(values)/sizeof(values[0]); i++){
		if(pairs)
			for(j = 0; j < sizeof(values)/sizeof(values[0]); j++)
				pair(values[i], values[j], selected);
		if(splits){
			split(values[i], nativeonly);
			split(nextafter(values[i], -INFINITY), nativeonly);
			split(nextafter(values[i], INFINITY), nativeonly);
		}
	}
	for(i = 0; i < 10000; i++){
		state = state*UINT64_C(6364136223846793005)+1;
		x.raw = quietbits(state);
		state = state*UINT64_C(6364136223846793005)+1;
		y.raw = quietbits(state);
		if(pairs)
			pair(x.value, y.value, selected);
		if(splits)
			split(x.value, nativeonly);
	}
	assert(checks > 0);
	printf("Math special values: %d numeric/NaN/zero/tuple checks, WORD%d\n", checks, WORD_BITS);
	return 0;
}
