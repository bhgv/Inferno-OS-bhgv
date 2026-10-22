#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include <limits.h>

#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef int64_t BIG;
typedef uint64_t uvlong;
typedef unsigned char uchar;
typedef struct Type Type;
struct Type { int size; };
typedef struct Array Array;
struct Array { WORD len; Type *t; Array *root; uchar *data; };
static Type Tbyte = {1}, Tword = {sizeof(WORD)}, Treal = {8};
#define H ((void*)1)
#define nil NULL
enum {exNilref = 1, exType, exMathia};
static jmp_buf escape;
static int last_error, checks;

/* LeakSanitizer cannot inspect processes in this sandbox; no heap is used here. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Catch errors before a malformed descriptor can reach its poisoned data. */
static void
error(int code)
{
	last_error = code;
	longjmp(escape, 1);
}

typedef struct Frame Frame;
struct Frame { Array *b, *x; };
#define ARRAY_FRAME(name) typedef Frame F_Math_##name
ARRAY_FRAME(import_int); ARRAY_FRAME(export_int);
ARRAY_FRAME(import_real32); ARRAY_FRAME(export_real32);
ARRAY_FRAME(import_real); ARRAY_FRAME(export_real);
typedef struct { WORD b; double *ret; } F_Math_bits32real;
typedef struct { BIG b; double *ret; } F_Math_bits64real;
typedef struct { double x; WORD *ret; } F_Math_realbits32;
typedef struct { double x; BIG *ret; } F_Math_realbits64;
typedef struct { WORD t0; double t1; } Gamma;
typedef struct { double x; Gamma *ret; } F_Math_lgamma;

/* Model only the native int output contract; gamma arithmetic is outside scope. */
static double
__ieee754_lgamma_r(double x, int *sign)
{
	*sign = x < 0 ? -1 : 1;
	return 1.25;
}

IMPLEMENTATION

/* Form the external representation independently of the production loops. */
static void
network(uchar *bytes, uint64_t value, int width)
{
	int i;

	for(i = width-1; i >= 0; i--) {
		bytes[i] = value & 255;
		value >>= 8;
	}
}

/* Use widened arithmetic as the signed-32 oracle in either VM model. */
static WORD
signed32(uint32_t value)
{
	return value & 0x80000000U ? (WORD)((int64_t)value-INT64_C(4294967296)) : (WORD)value;
}

/* Check exact array slots, signed widening, external bytes and untouched guards. */
static void
integers(void)
{
	uint32_t values[] = {0, 1, 0x7fffffff, 0x80000000U, 0xffffffffU, 0x12345678, 0x87654321U};
	WORD words[9];
	uchar bytes[36], encoded[36];
	Array b = {28, &Tbyte, nil, bytes+4}, x = {7, &Tword, nil, (uchar*)(words+1)};
	Frame frame = {&b, &x};
	unsigned int i;

	memset(bytes, 0xa5, sizeof(bytes));
	memset(encoded, 0xa5, sizeof(encoded));
	memset(words, 0xa5, sizeof(words));
	for(i = 0; i < 7; i++)
		network(bytes+4+4*i, values[i], 4);
	Math_import_int(&frame);
	for(i = 0; i < 7; i++)
		assert(words[i+1] == signed32(values[i]));
	for(i = 0; i < sizeof(WORD); i++)
		assert(((uchar*)words)[i] == 0xa5 && ((uchar*)(words+8))[i] == 0xa5);
	b.data = encoded+4;
	Math_export_int(&frame);
	assert(memcmp(encoded, bytes, sizeof(bytes)) == 0);
	if(sizeof(WORD) > 4) {
		words[1] = (WORD)INT64_C(4294967297);
		Math_export_int(&frame);
		assert(memcmp(encoded+4, "\0\0\0\1", 4) == 0);
	}
	checks += 16;
}

/* Check scalar and vector IEEE encodings including signs, infinities and NaNs. */
static void
reals(void)
{
	uint32_t single[] = {0, 0x80000000U, 0x3f800000, 1, 0x7f7fffff, 0x7f800000, 0xff800000U, 0x7fc12345};
	uint64_t wide[] = {0, UINT64_C(0x8000000000000000), UINT64_C(0x3ff0000000000000), 1,
		UINT64_C(0x7fefffffffffffff), UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000), UINT64_C(0x7ff8123456789abc)};
	uchar bytes[80], encoded[80];
	double values[10], actual, expected;
	float f;
	WORD word;
	BIG big;
	Array b = {32, &Tbyte, nil, bytes+8}, x = {8, &Treal, nil, (uchar*)(values+1)};
	Frame frame = {&b, &x};
	unsigned int i;
	F_Math_bits32real from32 = {0, &actual};
	F_Math_realbits32 to32 = {0, &word};
	F_Math_bits64real from64 = {0, &actual};
	F_Math_realbits64 to64 = {0, &big};

	memset(bytes, 0xa5, sizeof(bytes));
	memset(encoded, 0xa5, sizeof(encoded));
	memset(values, 0xa5, sizeof(values));
	for(i = 0; i < 8; i++)
		network(bytes+8+4*i, single[i], 4);
	Math_import_real32(&frame);
	for(i = 0; i < 8; i++) {
		memcpy(&f, single+i, 4);
		expected = f;
		if(isnan(expected))
			assert(isnan(values[i+1]));
		else
			assert(memcmp(&expected, values+i+1, 8) == 0);
		from32.b = signed32(single[i]);
		Math_bits32real(&from32);
		if(isnan(expected))
			assert(isnan(actual));
		else
			assert(memcmp(&actual, &expected, 8) == 0);
		to32.x = expected;
		Math_realbits32(&to32);
		assert(word == signed32(single[i]));
	}
	b.data = encoded+8;
	Math_export_real32(&frame);
	assert(memcmp(bytes, encoded, sizeof(bytes)) == 0);
	b.data = bytes+8;
	b.len = 64;
	for(i = 0; i < 8; i++)
		network(bytes+8+8*i, wide[i], 8);
	Math_import_real(&frame);
	for(i = 0; i < 8; i++) {
		assert(memcmp(values+i+1, wide+i, 8) == 0);
		memcpy(&from64.b, wide+i, 8);
		Math_bits64real(&from64);
		assert(memcmp(&actual, wide+i, 8) == 0);
		to64.x = actual;
		Math_realbits64(&to64);
		assert(memcmp(&big, wide+i, 8) == 0);
	}
	b.data = encoded+8;
	Math_export_real(&frame);
	assert(memcmp(bytes, encoded, sizeof(bytes)) == 0);
	for(i = 0; i < 8; i++)
		assert(((uchar*)values)[i] == 0xa5 && ((uchar*)(values+9))[i] == 0xa5);
	checks += 66;
}

/* Reject nil/type/size errors before reading or writing any poisoned storage. */
static void
bounds(void)
{
	void (*functions[])(void*) = {Math_import_int, Math_export_int, Math_import_real32,
		Math_export_real32, Math_import_real, Math_export_real};
	Array b, x;
	Frame frame;
	int op, mode, wanted, width;

	for(op = 0; op < 6; op++)
		for(mode = 0; mode < 11; mode++) {
			width = op >= 4 ? 8 : 4;
			b = (Array){width, &Tbyte, nil, (uchar*)1};
			x = (Array){1, op < 2 ? &Tword : &Treal, nil, (uchar*)1};
			frame = (Frame){&b, &x};
			wanted = exMathia;
			switch(mode) {
			case 0: frame.b = H; wanted = exNilref; break;
			case 1: frame.x = H; wanted = exNilref; break;
			case 2: frame.b = nil; wanted = exNilref; break;
			case 3: frame.x = nil; wanted = exNilref; break;
			case 4: b.t = &Treal; wanted = exType; break;
			case 5: x.t = &Tbyte; wanted = exType; break;
			case 6: b.len--; break;
			case 7: x.len = -1; break;
			case 8: b.len = -1; break;
			case 9: x.len = INT_MAX/width+1; b.len = 0; break;
			case 10: x.len = sizeof(WORD) > 4 ? (WORD)INT64_C(4294967297) : INT_MAX; break;
			}
			last_error = 0;
			if(setjmp(escape) == 0) {
				functions[op](&frame);
				assert(0);
			}
			assert(last_error == wanted);
			checks++;
		}
	/* Empty arrays may have no data buffer and must never be dereferenced. */
	for(op = 0; op < 6; op++) {
		b = (Array){0, &Tbyte, nil, nil};
		x = (Array){0, op < 2 ? &Tword : &Treal, nil, nil};
		frame = (Frame){&b, &x};
		functions[op](&frame);
		checks++;
	}
}

/* Native lgamma writes an int, while the VM's sign slot can be wider. */
int
main(void)
{
	Gamma output;
	F_Math_lgamma frame = {-0.5, &output};

	integers();
	reals();
	bounds();
	memset(&output, 0xa5, sizeof(output));
	Math_lgamma(&frame);
	assert(output.t0 == -1 && output.t1 == 1.25);
	frame.x = 0.5;
	Math_lgamma(&frame);
	assert(output.t0 == 1 && output.t1 == 1.25);
	checks += 2;
	printf("PASS Math codecs: %d bounds/stride/IEEE/sign cases, WORD%d\n", checks, WORD_BITS);
	return 0;
}
