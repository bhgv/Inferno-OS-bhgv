#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

typedef unsigned long ulong;
typedef uintptr_t uintptr;
#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
#define minimum(x,y) ((x) <= (y) ? (x) : (y))

static int depth, maximum, adaptive, gas, solid, candidate, checks;
static double keys[4096];
static long nativeorder[4096];
static WORD mathorder[4096];

NATIVE_IMPLEMENTATION

/* Observe recursive call boundaries without changing partition operations. */
static void
qsorts(char *a, long n, Sort *s)
{
	depth++;
	if(depth > maximum)
		maximum = depth;
	qsorts_body(a, n, s);
	depth--;
}

static int cmp(WORD*, WORD*, double*);
static void rqsort(WORD*, int, double*);
MATH_IMPLEMENTATION

/* Observe actual Math sort calls in both VM word models. */
static void
rqsort(WORD *a, int n, double *x)
{
	depth++;
	if(depth > maximum)
		maximum = depth;
	rqsort_body(a, n, x);
	depth--;
}

/* Freeze unresolved ranks to construct a median-pivot adversarial permutation. */
static int
rankcompare(int a, int b)
{
	if(a == b)
		return 0;
	if(adaptive && keys[a] == gas && keys[b] == gas){
		if(a == candidate)
			keys[a] = solid++;
		else
			keys[b] = solid++;
	}
	if(adaptive){
		if(keys[a] == gas)
			candidate = a;
		else if(keys[b] == gas)
			candidate = b;
	}
	return (keys[a] > keys[b])-(keys[a] < keys[b]);
}

/* Compare stable record IDs through their generated numeric keys. */
static int
nativecompare(void *a, void *b)
{
	return rankcompare(*(long*)a, *(long*)b);
}

/* Use adaptive comparisons only for generation, then the production comparator. */
static int
cmp(WORD *a, WORD *b, double *x)
{
	if(adaptive)
		return rankcompare(*a, *b);
	return mathcompare(a, b, x);
}

/* Bound recursion independently by halving the smaller side of each split. */
static int
stacklimit(int n)
{
	int bound;

	bound = 2;
	while(n > 1){
		n /= 2;
		bound++;
	}
	return bound;
}

/* Check sorted keys and exact ID membership without requiring sort stability. */
static void
validated(int n, int math)
{
	unsigned char seen[4096] = {0};
	int i, current, previous;

	for(i = 0; i < n; i++){
		current = math ? mathorder[i] : nativeorder[i];
		assert(current >= 0 && current < n && seen[current] == 0);
		seen[current] = 1;
		if(i){
			previous = math ? mathorder[i-1] : nativeorder[i-1];
			if(isnan(keys[previous]))
				assert(isnan(keys[current]));
			else if(!isnan(keys[current]))
				assert(keys[previous] <= keys[current]);
		}
	}
}

/* Generate ranks, freeze them, and rerun the unmodified numeric comparator. */
static void
adversary(int n, int math)
{
	int i, id, generated;

	gas = n-1;
	solid = candidate = depth = maximum = 0;
	for(i = 0; i < n; i++){
		nativeorder[i] = mathorder[i] = i;
		keys[i] = gas;
	}
	adaptive = 1;
	if(math)
		rqsort(mathorder, n, keys);
	else
		native_qsort(nativeorder, n, sizeof(long), nativecompare);
	generated = maximum;
	for(i = 0; i < n; i++){
		id = math ? mathorder[i] : nativeorder[i];
		if(keys[id] == gas)
			keys[id] = solid++;
	}
	for(i = 0; i < n; i++)
		nativeorder[i] = mathorder[i] = i;
	adaptive = depth = maximum = 0;
	if(math)
		rqsort(mathorder, n, keys);
	else
		native_qsort(nativeorder, n, sizeof(long), nativecompare);
	validated(n, math);
	printf("%s WORD%d n=%d: fixed depth=%d, generation depth=%d, bound=%d\n",
		math ? "Math" : "native", WORD_BITS, n, maximum, generated, stacklimit(n));
	fflush(stdout);
	assert(maximum <= stacklimit(n));
	checks++;
}

/* Check common, duplicate, reversed, random and IEEE-special Math inputs. */
static void
ordinary(void)
{
	uint32_t random = 0x1234abcd;
	int n, mode, i;

	for(n = 0; n <= 128; n++)
	for(mode = 0; mode < 6; mode++){
		for(i = 0; i < n; i++){
			random = random*1664525U+1013904223U;
			keys[i] = mode == 0 ? i : mode == 1 ? -i :
				mode == 2 ? 1 : mode == 3 ? i%3 : (int32_t)random;
			if(mode == 5){
				if(i%5 == 0)
					keys[i] = NAN;
				else if(i%5 == 1)
					keys[i] = INFINITY;
				else if(i%5 == 2)
					keys[i] = -INFINITY;
				else
					keys[i] = i%2 ? -0.0 : 0.0;
			}
			nativeorder[i] = mathorder[i] = i;
		}
		adaptive = depth = maximum = 0;
		rqsort(mathorder, n, keys);
		validated(n, 1);
		assert(maximum <= stacklimit(n));
		if(mode != 5){
			depth = maximum = 0;
			native_qsort(nativeorder, n, sizeof(long), nativecompare);
			validated(n, 0);
			assert(maximum <= stacklimit(n));
		}
		checks++;
	}
}

static int recordwidth;

/* Compare complete records independently of native element-swap alignment. */
static int
recordcompare(void *a, void *b)
{
	return memcmp(a, b, recordwidth);
}

/* Check byte/native-word swaps against insertion sort, including guard bytes. */
static void
records(void)
{
	unsigned char *data, wanted[1100], item[32];
	uint32_t random = 0x76543210;
	int n, width, offset, mode, i, j, k;

	data = malloc(sizeof(wanted));
	assert(data != NULL);
	for(n = 0; n <= 32; n++)
	for(width = 1; width <= 24; width++)
	for(offset = 0; offset < 8; offset++)
	for(mode = 0; mode < 3; mode++){
		memset(data, 0xa5, sizeof(wanted));
		for(i = 0; i < n; i++)
		for(j = 0; j < width; j++){
			random = random*1664525U+1013904223U;
			data[16+offset+i*width+j] = mode == 0 ? random>>24 :
				mode == 1 ? i%3 : n-i;
		}
		memcpy(wanted, data, sizeof(wanted));
		for(i = 1; i < n; i++){
			memcpy(item, wanted+16+offset+i*width, width);
			j = i;
			while(j > 0 && memcmp(wanted+16+offset+(j-1)*width, item, width) > 0){
				for(k = 0; k < width; k++)
					wanted[16+offset+j*width+k] = wanted[16+offset+(j-1)*width+k];
				j--;
			}
			memcpy(wanted+16+offset+j*width, item, width);
		}
		recordwidth = width;
		maximum = depth = 0;
		native_qsort(data+16+offset, n, width, recordcompare);
		assert(memcmp(data, wanted, sizeof(wanted)) == 0);
		assert(maximum <= stacklimit(n));
		checks++;
	}
	free(data);
}

/* A malformed or empty native span must not call comparators or touch storage. */
static int
unexpected(void *a, void *b)
{
	(void)a;
	(void)b;
	assert(0);
	return 0;
}

/* Cover signed count limits before multiplication and zero-width partitioning. */
static void
invalid(void)
{
	native_qsort(NULL, LONG_MIN, 8, unexpected);
	native_qsort(NULL, -1, 8, unexpected);
	native_qsort(NULL, 0, 8, unexpected);
	native_qsort(NULL, 1, 8, unexpected);
	native_qsort(NULL, 2, 0, unexpected);
	native_qsort(NULL, 2, -1, unexpected);
	native_qsort(NULL, LONG_MAX, 2, unexpected);
	native_qsort(NULL, 2, LONG_MAX, unexpected);
	checks += 8;
}

/* Run each algorithm's independent adversary and ordinary correctness checks. */
int
main(int argc, char **argv)
{
	int sizes[] = {127, 512, 2048, 4096}, i;

	if(argc == 1 || strcmp(argv[1], "native") == 0)
		for(i = 0; i < 4; i++)
			adversary(sizes[i], 0);
	if(argc == 1 || strcmp(argv[1], "math") == 0)
		for(i = 0; i < 4; i++)
			adversary(sizes[i], 1);
	if(argc == 1 || strcmp(argv[1], "ordinary") == 0)
		ordinary();
	if(argc == 1 || strcmp(argv[1], "records") == 0)
		records();
	if(argc == 1 || strcmp(argv[1], "invalid") == 0)
		invalid();
	printf("sort stack/correctness: %d scenarios passed, WORD%d\n", checks, WORD_BITS);
	return 0;
}
