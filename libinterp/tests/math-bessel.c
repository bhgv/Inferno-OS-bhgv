#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <limits.h>

typedef WORD_TYPE WORD;
typedef double REAL;
typedef unsigned char uchar;
typedef int64_t vlong;
#define NREG 4
#define __HI(x) (((int32_t*)&(x))[HI_INDEX])
#define __LO(x) (((int32_t*)&(x))[LO_INDEX])
FRAMES
static jmp_buf boundary;
static int expectederror, logprobe, checks;
static int currentorder;
static double currentx;
static uint64_t seed = 0x6c696d626f;
static char exInval[] = "invalid";

/* Invalid VM orders must fail before modifying the caller's result. */
static void
error(char *message)
{
	assert(expectederror && message == exInval);
	longjmp(boundary, 1);
}

/* No test-owned allocation requires LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Host special functions provide base cases; the tested order recurrence is production. */
static double __ieee754_j0(double x) { return j0(x); }
static double __ieee754_j1(double x) { return j1(x); }
static double __ieee754_y0(double x) { return y0(x); }
static double __ieee754_y1(double x) { return y1(x); }

/* Stop extreme backward recurrences after their real doubled-index setup. */
static double
__ieee754_log(double x)
{
	if(logprobe){
		assert(x > 1e9);
		longjmp(boundary, 1);
	}
	return log(x);
}

IMPLEMENTATION

/* Compare classes and zero signs explicitly; allow recurrence rounding differences. */
static void
same(double actual, double expected)
{
	if(isnan(expected))
		assert(isnan(actual));
	else if(isinf(expected))
		assert(actual == expected);
	else if(expected == 0){
		if(!(actual == 0 && signbit(actual) == signbit(expected)))
			fprintf(stderr, "zero mismatch: order=%d x=%a actual=%a expected=%a check=%d\n", currentorder, currentx, actual, expected, checks);
		assert(actual == 0 && signbit(actual) == signbit(expected));
	}
	else
		assert(fabs(actual-expected) <= 1e-12*fabs(expected)+1e-300);
	checks++;
}

/* Enter both production VM bridges with actual generated frame fields. */
static void
pair(int n, double x)
{
	F_Math_jn jf = {0};
	F_Math_yn yf = {0};
	double out, expectedy;

	expectedy = yn(n, x);
	if(x == 0)
		expectedy = n < 0 && (n&1) ? INFINITY : -INFINITY;
	if(isinf(x) && x > 0)
		expectedy = n < 0 && (n&1) ? -0.0 : 0.0;
	currentorder = n; currentx = x;
	jf.ret = yf.ret = &out;
	jf.n = yf.n = n;
	jf.x = yf.x = x;
	Math_jn(&jf); same(out, jn(n, x));
	Math_yn(&yf); same(out, expectedy);
	jf.ret = &jf.x;
	yf.ret = &yf.x;
	Math_jn(&jf); same(jf.x, jn(n, x));
	Math_yn(&yf); same(yf.x, expectedy);
}

/* Cover normal recurrence paths, parity, special values and arbitrary low-word bits. */
static void
ordinary(void)
{
	static const uint64_t bits[] = {0, 0x8000000000000000ULL,
		0x3ff0000080000000ULL, 0x4000000080000000ULL, 0x0000000080000000ULL,
		0x7ff0000000000000ULL, 0xfff0000000000000ULL,
		0x7ff8000080000000ULL, 0xfff8000080000000ULL};
	double x;
	int n, i;

	for(n = -64; n <= 64; n++){
		for(i = 0; i < (int)(sizeof(bits)/sizeof(bits[0])); i++){
			memcpy(&x, &bits[i], sizeof(x));
			pair(n, x);
		}
		for(i = 1; i <= 64; i++)
			pair(n, i*0.125);
	}
	for(i = 0; i < 4096; i++){
		seed ^= seed << 13;
		seed ^= seed >> 7;
		seed ^= seed << 17;
		n = (int)(seed%257)-128;
		x = (double)((seed>>16)%1000001)/1000.0+0.001;
		pair(n, x);
	}
}

/* INT_MIN is even: analytic fast paths avoid billions of recurrence steps. */
static void
extreme(void)
{
	double x;
	int n, i;
	const int orders[] = {INT_MIN, INT_MIN+1, INT_MAX};

	for(i = 0; i < 3; i++){
		n = orders[i];
		same(tested_jn(n, 0.0), n < 0 && (n&1) ? -0.0 : 0.0);
		same(tested_jn(n, 0x1p-40), n < 0 && (n&1) ? -0.0 : 0.0);
		same(tested_jn(n, INFINITY), n < 0 && (n&1) ? -0.0 : 0.0);
		same(tested_jn(n, NAN), NAN);
		same(tested_yn(n, 0.0), n < 0 && (n&1) ? INFINITY : -INFINITY);
		same(tested_yn(n, INFINITY), n < 0 && (n&1) ? -0.0 : 0.0);
		same(tested_yn(n, NAN), NAN);
		same(tested_yn(n, -1.0), NAN);
		/* At huge x the leading asymptote depends only on order modulo four. */
		x = 0x1p400;
		same(tested_jn(n, x), 0.564189583547756279280*
			(n%4 == 0 ? cos(x)+sin(x) : n%4 == 1 || n%4 == -3 ? -cos(x)+sin(x) :
			 n%4 == 2 || n%4 == -2 ? -cos(x)-sin(x) : cos(x)-sin(x))/sqrt(x));
	}
}

/* Reject native-unrepresentable orders rather than passing their low bits to libm. */
static void
wideorders(void)
{
	int64_t orders[] = {INT64_MIN, INT64_MIN+1, -4294967297LL, -4294967296LL,
		-2147483649LL, 2147483648LL, 4294967296LL, 4294967297LL, INT64_MAX};
	F_Math_jn jf = {0};
	F_Math_yn yf = {0};
	double out;
	unsigned int i;

	if(sizeof(WORD) == 4)
		return;
	for(i = 0; i < sizeof(orders)/sizeof(orders[0]); i++){
		jf.n = yf.n = orders[i];
		jf.x = yf.x = 1.0;
		jf.ret = yf.ret = &out;
		out = 123.5;
		expectederror = 1;
		if(setjmp(boundary) == 0){ Math_jn(&jf); assert(0); }
		assert(out == 123.5);
		if(setjmp(boundary) == 0){ Math_yn(&yf); assert(0); }
		assert(out == 123.5);
		expectederror = 0;
		checks += 2;
	}
}

/* Exercise the real continued-fraction index setup before bounding expensive work. */
static void
indices(void)
{
	int orders[] = {INT_MAX/2, INT_MAX/2+1, INT_MAX-1, INT_MAX, INT_MIN};
	unsigned int i;

	for(i = 0; i < sizeof(orders)/sizeof(orders[0]); i++){
		logprobe = 1;
		if(setjmp(boundary) == 0){ tested_jn(orders[i], 1.0); assert(0); }
		logprobe = 0;
		checks++;
	}
}

/* Keep native magnitude, NaN low words, recurrence indices and VM validation selectable. */
int
main(int argc, char **argv)
{
	if(argc == 1 || strcmp(argv[1], "ordinary") == 0) ordinary();
	if(argc == 1 || strcmp(argv[1], "extreme") == 0) extreme();
	if(argc == 1 || strcmp(argv[1], "wide") == 0) wideorders();
	if(argc == 1 || strcmp(argv[1], "indices") == 0) indices();
	printf("PASS Bessel orders: %d numeric/boundary/oracle checks, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
