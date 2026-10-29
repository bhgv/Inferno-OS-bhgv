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
typedef uint64_t uvlong;
typedef double REAL;
typedef unsigned char uchar;
typedef struct Type Type;
struct Type
{
    int size; /* Primitive element bytes, matching the production metadata. */
    int np; /* Pointer-map bytes; primitive module-local layouts use zero. */
};
typedef struct Array Array;
struct Array { WORD len; Type *t; Array *root; uchar *data; };
static Type Tword = {sizeof(WORD), 0}; /* Canonical target word. */
static Type Treal = {8, 0}; /* Canonical real. */
static Type Tlong = {8, 0}; /* Canonical big, distinct even when widths coincide. */
static Type Tbyte = {1, 0}; /* Canonical byte. */
#define H ((void*)1)
#define nil NULL
#define isNaN(x) isnan(x)
#define isInf(x, sign) (isinf(x) && ((sign) > 0 ? (x) > 0 : (x) < 0))
enum {exNilref = 1, exType, exMathia, exBounds};
static jmp_buf escape;
static int last_error, checks;

/* The sandbox cannot provide LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Stop malformed descriptors before accessing poisoned backing pointers. */
static void
error(int code)
{
	last_error = code;
	longjmp(escape, 1);
}

typedef struct { Array *x, *pi; } F_Math_sort;
typedef struct { Array *x, *y; double *ret; } F_Math_dot;
typedef struct { Array *x; WORD *ret; } F_Math_iamax;
typedef struct { Array *x; double *ret; } F_Math_norm1;
typedef F_Math_norm1 F_Math_norm2;
typedef struct {
	WORD transa, transb, m, n, k;
	double alpha;
	Array *a;
	WORD lda;
	Array *b;
	WORD ldb;
	double beta;
	Array *c;
	WORD ldc;
} F_Math_gemm;
static double dot(int, double*, double*);
static int iamax(int, double*);
static double norm1(int, double*), norm2(int, double*);
static void gemm(int, int, int, int, int, double, double*, int, double*, int, double, double*, int);
IMPLEMENTATION

/* Bound fixture pseudo-randomness; no production entropy behavior is modelled. */
static unsigned int
randomword(void)
{
	static unsigned int state = 0x736f7274;

	state = state*1664525U+1013904223U;
	return state;
}

/* Check full index slots, permutation preservation, NaN order and data guards. */
static void
sorts(void)
{
	WORD indices[259], original[257];
	double values[257], copy[257];
	int seen[257], n, mode, i, index, previous;
	Array x = {0, &Treal, nil, (uchar*)values};
	Array p = {0, &Tword, nil, (uchar*)(indices+1)};
	F_Math_sort frame = {&x, &p};

	for(n = 0; n <= 257; n++)
		for(mode = 0; mode < 5; mode++) {
			memset(indices, 0xa5, sizeof(indices));
			memset(seen, 0, sizeof(seen));
			for(i = 0; i < n; i++) {
				values[i] = mode == 0 ? i : mode == 1 ? n-i : mode == 2 ? 1 : (int)(randomword()%31)-15;
				if(mode == 4 && i%5 == 0)
					values[i] = NAN;
				indices[i+1] = i;
			}
			memcpy(copy, values, sizeof(values));
			x.len = p.len = n;
			Math_sort(&frame);
			previous = -1;
			for(i = 0; i < n; i++) {
				index = indices[i+1];
				assert(index >= 0 && index < n && !seen[index]);
				seen[index] = 1;
				if(previous >= 0) {
					if(isnan(values[previous]))
						assert(isnan(values[index]));
					else if(!isnan(values[index]))
						assert(values[previous] <= values[index]);
				}
				previous = index;
			}
			assert(memcmp(values, copy, sizeof(values)) == 0);
			for(i = 0; i < sizeof(WORD); i++)
				assert(((uchar*)indices)[i] == 0xa5 && ((uchar*)(indices+n+1))[i] == 0xa5);
			checks++;
		}
	for(i = 0; i < 257; i++)
		original[i] = indices[i+1] = 256-i;
	indices[257] = -1;
	x.len = p.len = 257;
	if(setjmp(escape) == 0) {
		Math_sort(&frame);
		assert(0);
	}
	assert(last_error == exMathia);
	assert(memcmp(indices+1, original, 256*sizeof(WORD)) == 0 && indices[257] == -1);
	checks++;
}

/* Compare scaled norms with hypot, including representable huge/tiny results. */
static void
vectors(void)
{
	double values[][4] = {{3, -4, 0, 0}, {1e308, 1e308, 0, 0},
		{1e-300, 1e-300, 0, 0}, {0, -0.0, 0, 0}, {INFINITY, INFINITY, 0, 0},
		{INFINITY, NAN, 0, 0}, {NAN, INFINITY, 0, 0}};
	double answer, expected;
	WORD maximum;
	Array x = {4, &Treal, nil, nil};
	F_Math_norm2 frame = {&x, &answer};
	F_Math_dot df = {&x, &x, &answer};
	F_Math_iamax af = {&x, &maximum};
	F_Math_norm1 nf = {&x, &answer};
	unsigned int i;
	int j;

	for(i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
		x.data = (uchar*)values[i];
		Math_norm2(&frame);
		expected = 0;
		for(j = 0; j < 4; j++)
			expected = hypot(expected, values[i][j]);
		if(i >= 5)
			assert(isnan(answer));
		else if(isinf(expected))
			assert(answer == expected);
		else
			assert(fabs(answer-expected) <= fabs(expected)*1e-15);
		checks++;
	}
	x.data = (uchar*)values[0];
	Math_dot(&df); assert(answer == 25);
	Math_iamax(&af); assert(maximum == 1);
	Math_norm1(&nf); assert(answer == 7);
	checks += 3;
	x.len = 0; x.data = nil;
	Math_norm2(&frame); assert(answer == 0);
	Math_dot(&df); assert(answer == 0);
	Math_iamax(&af); assert(maximum == 0);
	Math_norm1(&nf); assert(answer == 0);
	checks += 4;
}

/* Copy scalar vector results into every byte alignment without changing guard bytes. */
static void
vectorreturns(void)
{
	double values[]={3,-4,0,0}, real;
	WORD integer;
	Array x={4,&Treal,nil,(uchar*)values};
	F_Math_dot df={&x,&x,nil};
	F_Math_iamax af={&x,nil};
	F_Math_norm1 nf={&x,nil};
	F_Math_norm2 tf={&x,nil};
	union { double align; uchar bytes[48]; } memory;
	uchar expected[48];
	int op, offset, alias, width;
	void *destination;

	for(op=0;op<4;op++)for(offset=0;offset<8;offset++)for(alias=0;alias<2;alias++){
		values[0]=3;values[1]=-4;values[2]=values[3]=0;
		memset(memory.bytes,0xa5,sizeof(memory.bytes));memmove(expected,memory.bytes,sizeof(expected));
		destination=alias ? (void*)values : memory.bytes+16+offset;
		df.ret=destination;af.ret=destination;nf.ret=destination;tf.ret=destination;
		width=op==1 ? sizeof(WORD) : sizeof(double);
		if(op==0){real=25;Math_dot(&df);}
		else if(op==1){integer=1;Math_iamax(&af);}
		else if(op==2){real=7;Math_norm1(&nf);}
		else{real=5;Math_norm2(&tf);}
		assert(!memcmp(destination,op==1 ? (void*)&integer : (void*)&real,width));
		if(!alias)memmove(expected+16+offset,destination,width);
		assert(!memcmp(memory.bytes,expected,sizeof(expected)));
		checks++;
	}
}

/* Validate descriptors through each wrapper without reading their poisoned data. */
static void
vectorbounds(void)
{
	Array x, y, p;
	double result;
	WORD index;
	F_Math_norm1 nf = {&x, &result};
	F_Math_norm2 tf = {&x, &result};
	F_Math_dot df = {&x, &y, &result};
	F_Math_iamax af = {&x, &index};
	F_Math_sort sf = {&x, &p};
	int mode, op, wanted;

	for(op = 0; op < 5; op++)
		for(mode = 0; mode < 6; mode++) {
			x = (Array){1, &Treal, nil, (uchar*)1};
			y = x;
			p = (Array){0, &Tword, nil, nil};
			nf.x = tf.x = df.x = af.x = sf.x = &x;
			wanted = exMathia;
			if(mode == 0) { nf.x = tf.x = df.x = af.x = sf.x = H; wanted = exNilref; }
			if(mode == 1) { nf.x = tf.x = df.x = af.x = sf.x = nil; wanted = exNilref; }
			if(mode == 2) { x.t = &Tbyte; wanted = exType; }
			if(mode == 3) x.len = -1;
			if(mode == 4) x.len = INT_MAX;
			if(mode == 5) x.len = sizeof(WORD) > 4 ? (WORD)INT64_C(4294967297) : INT_MAX/8+1;
			result = 99; index = 99;
			if(setjmp(escape) == 0) {
				switch(op) {
				case 0: Math_norm1(&nf); break;
				case 1: Math_norm2(&tf); break;
				case 2: Math_dot(&df); break;
				case 3: Math_iamax(&af); break;
				case 4: Math_sort(&sf); break;
				}
				assert(0);
			}
			assert(last_error == wanted && result == 99 && index == 99);
			checks++;
		}
}

/* Validate the second dot operand and the permutation descriptor independently. */
static void
secondarybounds(void)
{
	double values[] = {1, 2}, answer;
	WORD indices[] = {1, 0}, saved[2];
	Array x = {2, &Treal, nil, (uchar*)values}, y, p;
	F_Math_dot df = {&x, &y, &answer};
	F_Math_sort sf = {&x, &p};
	int op, mode, wanted;

	for(op = 0; op < 2; op++)
		for(mode = 0; mode < 7; mode++) {
			y = (Array){2, &Treal, nil, (uchar*)1};
			p = (Array){2, &Tword, nil, (uchar*)indices};
			df.y = &y; sf.pi = &p;
			indices[0] = 1; indices[1] = 0;
			wanted = exMathia;
			if(mode == 0) { df.y = H; sf.pi = H; wanted = exNilref; }
			if(mode == 1) { df.y = nil; sf.pi = nil; wanted = exNilref; }
			if(mode == 2) { y.t = p.t = &Tbyte; wanted = exType; }
			if(mode == 3) y.len = p.len = -1;
			if(mode == 4) y.len = p.len = INT_MAX;
			if(mode == 5) { y.len = 1; indices[1] = 2; }
			if(mode == 6) { y.len = 0; indices[1] = sizeof(WORD) > 4 ? (WORD)INT64_C(4294967296) : INT_MAX; }
			memcpy(saved, indices, sizeof(saved));
			answer = 99;
			if(setjmp(escape) == 0) {
				if(op == 0) Math_dot(&df); else Math_sort(&sf);
				assert(0);
			}
			assert(last_error == wanted && answer == 99);
			assert(memcmp(saved, indices, sizeof(saved)) == 0);
			checks++;
		}
}

/* Compare all transpose combinations and padding with a direct triple-loop oracle. */
static void
matrices(void)
{
	double a[64], b[64], c[64], expected[64], sum;
	Array aa = {64, &Treal, nil, (uchar*)a}, bb = {64, &Treal, nil, (uchar*)b}, cc = {64, &Treal, nil, (uchar*)c};
	F_Math_gemm f;
	int m, n, k, ta, tb, i, j, l, mode;
	char trans[] = {'N', 'T', 'C'};

	for(m = 1; m <= 3; m++)
	for(n = 1; n <= 3; n++)
	for(k = 0; k <= 3; k++)
	for(ta = 0; ta < 3; ta++)
	for(tb = 0; tb < 3; tb++)
	for(mode = 0; mode < 3; mode++) {
		f = (F_Math_gemm){trans[ta], trans[tb], m, n, k, mode == 0 ? 0 : 2, &aa, 5, &bb, 5, mode == 1 ? 0 : -1, &cc, 5};
		for(i = 0; i < 64; i++) {
			a[i] = (int)(randomword()%7)-3;
			b[i] = (int)(randomword()%7)-3;
			c[i] = expected[i] = mode == 1 ? NAN : (int)(randomword()%7)-3;
		}
		for(j = 0; j < n; j++)
			for(i = 0; i < m; i++) {
				sum = 0;
				for(l = 0; l < k; l++)
					sum += a[ta == 0 ? i+l*5 : l+i*5]*b[tb == 0 ? l+j*5 : j+l*5];
				expected[i+j*5] = f.alpha*sum+(f.beta == 0 ? 0 : f.beta*c[i+j*5]);
			}
		Math_gemm(&f);
		for(i = 0; i < 64; i++)
			assert((isnan(expected[i]) && isnan(c[i])) || c[i] == expected[i]);
		checks++;
	}
	/* A nil left operand is the existing square-identity extension. */
	for(tb = 0; tb < 3; tb++) {
		for(i = 0; i < 64; i++) { b[i] = i+1; c[i] = NAN; }
		f = (F_Math_gemm){'N', trans[tb], 2, 3, 2, 2, H, 0, &bb, 5, 0, &cc, 5};
		Math_gemm(&f);
		for(j = 0; j < 3; j++)
			for(i = 0; i < 2; i++)
				assert(c[i+j*5] == 2*b[tb == 0 ? i+j*5 : j+i*5]);
		checks++;
	}
	for(i = 0; i < 64; i++) c[i] = 3;
	f = (F_Math_gemm){'N', 'N', 2, 3, 0, 2, H, 0, H, 0, 2, &cc, 5};
	Math_gemm(&f);
	for(j = 0; j < 3; j++)
		for(i = 0; i < 2; i++) assert(c[i+j*5] == 6);
	checks++;
	/* Neither empty matrices nor an absent product may read operand data. */
	cc.len = 0; cc.data = nil;
	f = (F_Math_gemm){'N', 'N', 0, 3, 2, 1, H, 0, H, 0, 0, &cc, 0};
	Math_gemm(&f);
	f.m = 2; f.n = 0; f.ldc = 2;
	Math_gemm(&f);
	checks += 2;
	cc.len = 64; cc.data = (uchar*)c;
	for(i = 0; i < 64; i++) c[i] = 3;
	f = (F_Math_gemm){'N', 'N', 2, 3, 2, 0, H, 0, H, 0, 2, &cc, 5};
	Math_gemm(&f);
	for(j = 0; j < 3; j++)
		for(i = 0; i < 2; i++) assert(c[i+j*5] == 6);
	f.alpha = 1; f.k = 0; f.beta = 1;
	Math_gemm(&f);
	for(j = 0; j < 3; j++)
		for(i = 0; i < 2; i++) assert(c[i+j*5] == 6);
	checks += 2;

}

/* Catch truncated final columns, int products and invalid full-word parameters. */
static void
matrixbounds(void)
{
	double data[16], before[16];
	Array a, b, c;
	F_Math_gemm f;
	int mode, wanted;

	for(mode = 0; mode < 15; mode++) {
		a = b = c = (Array){16, &Treal, nil, (uchar*)data};
		f = (F_Math_gemm){'N', 'N', 2, 2, 2, 1, &a, 2, &b, 2, 0, &c, 2};
		wanted = exBounds;
		memset(data, 0xa5, sizeof(data)); memcpy(before, data, sizeof(data));
		switch(mode) {
		case 0: a.len = 3; break;
		case 1: b.len = 3; break;
		case 2: c.len = 3; break;
		case 3: f.lda = INT_MAX; break;
		case 4: f.ldb = INT_MAX; break;
		case 5: f.ldc = INT_MAX; break;
		case 6: f.c = H; wanted = exNilref; break;
		case 7: f.b = H; wanted = exNilref; break;
		case 8: f.a = H; f.k = 3; wanted = exMathia; break;
		case 9: f.transa = 'X'; wanted = exMathia; break;
		case 10: f.m = -1; wanted = exMathia; break;
		case 11: a.t = &Tbyte; wanted = exType; break;
		case 12: f.n = sizeof(WORD) > 4 ? (WORD)INT64_C(4294967296) : -1; wanted = exMathia; break;
		case 13: f.lda = sizeof(WORD) > 4 ? (WORD)INT64_C(4294967298) : -1; break;
		case 14: f.transa = sizeof(WORD) > 4 ? (WORD)(INT64_C(4294967296)+'N') : 'x'; wanted = exMathia; break;
		}
		if(setjmp(escape) == 0) { Math_gemm(&f); assert(0); }
		assert(last_error == wanted && memcmp(data, before, sizeof(data)) == 0);
		checks++;
	}
}

/* Run valid work before poisoned descriptors so failures identify arithmetic regressions. */
int
main(void)
{
	sorts();
	vectors();
	vectorreturns();
	vectorbounds();
	secondarybounds();
	matrices();
	matrixbounds();
	printf("PASS Math vectors: %d sort/norm/matrix/bounds cases, WORD%d\n", checks, WORD_BITS);
	return 0;
}
