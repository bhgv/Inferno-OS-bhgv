#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <limits.h>
#include "mathi.h"
#include "bench-fp-adapter.h"

typedef long long vlong;
typedef unsigned long long uvlong;
typedef uint32_t u32int;
typedef uintptr_t uintptr;
#define nil NULL // Inferno null pointer spelling used by the actual bodies.
#define print printf // Parent replay contains a disabled print block.
#define sprint sprintf // Parent formatter uses the legacy unbounded spelling.
#define snprint snprintf // The numeric formatter uses standard-compatible conversions.
#define log benchlog // Avoid the host logarithm namespace.
#define sleep benchsleep // Controlled scheduler hook for the operation sampler.

enum { PriRealtime, PriNormal }; // Priority identities accepted by the scheduler adapter.
typedef struct Psync Psync;

struct Psync {
	int r, flag; // Unused wait object and ready predicate storage.
};

static struct {
	char *buf, *wpos; // Logical report allocation and initialized output cursor.
	int test; // Selected report channel callback.
	int bufsz; // Available report allocation size.
	vlong tickstart; // Last origin selected by the actual bus function.
	void (*op)(void); // Measured operation selected by timeop0.
} bench;
static struct { int pid; } process = { 17 }, *up = &process; // Sampler process identity.
static int nactive, sync; // Scheduler objects observed by the sampler.
static vlong MS2TS, US2TS; // Selected source's calibration variables.
static uvlong ticks, stride; // Deterministic counter bit pattern and low-word increment.
static unsigned long checks, calls; // Explicit predicate count and measured operation calls.
static long reported; // Last logged calibration/sample result count.
static char reportline[256]; // Output emitted through the actual sampler formatting call.


/* Fail with an executable source location rather than compiling checks out. */
static void
checkat(int condition, int line)
{
	checks++;
	if(!condition){
		fprintf(stderr, "benchmark numeric failure at %d\n", line);
		abort();
	}
}
#define check(x) checkat((x), __LINE__) // Keep validation active with NDEBUG.


/* Translate Inferno unsigned verbs only at the host formatting adapter boundary. */
static void
benchlog(char *format, ...)
{
	char converted[256], *out;
	va_list ap;

	out = converted;
	while(*format){
		if(!strncmp(format, "%llud", 5)){
			memcpy(out, "%llu", 4); out += 4; format += 5;
		}else if(!strncmp(format, "%lud", 4)){
			memcpy(out, "%lu", 3); out += 3; format += 4;
		}else if(!strncmp(format, "%ud", 3)){
			memcpy(out, "%u", 2); out += 2; format += 3;
		}else
			*out++ = *format++;
	}
	*out = 0;
	va_start(ap, format);
	vsnprintf(reportline, sizeof(reportline), converted, ap);
	va_end(ap);
	reported++;
}


/* Preserve the underlying timer bits when the signed timestamp crosses its midpoint. */
static vlong
archrdtsc(void)
{
	vlong result;

	memcpy(&result, &ticks, sizeof(result));
	return result;
}


/* Hardware low-word reads may wrap between adjacent calls. */
static unsigned long
archrdtsc32(void)
{
	u32int result;

	result = ticks;
	ticks += stride;
	return result;
}


/* Controlled platform hooks preserve the actual sampler's looping and arithmetic. */
static void clear(void) { bench.wpos = bench.buf; }


/* Priority changes are irrelevant to a deterministic counter. */
static void setpri(int priority) { (void)priority; }


/* Interrupt-state restoration retains a visible token. */
static int splhi(void) { return 7; }


/* Accept the token returned by the paired interrupt hook. */
static void splx(int priority) { check(priority == 7); }


/* Counter reads advance independently of the mock delay. */
static void microdelay(int duration) { check(duration == 1000); }


/* Observe one timed operation on each real sampler iteration. */
static void operation(void) { calls++; }


/* The wait predicate reads the actual selected process's flag. */
static int timev(void *p) { return *(int*)p; }


/* The synchronization adapters retain a real ready predicate call. */
static void benchsleep(int *wait, int (*ready)(void*), void *arg) { (void)wait; check(ready(arg)); }


/* Count entry into the active sampler set. */
static void incref(int *ref) { (*ref)++; }


/* A host fixture has no concurrent sampler lock owner. */
static void rlock(int *lock) { (void)lock; }


/* Pair the lock adapter without changing samples. */
static void runlock(int *lock) { (void)lock; }


/* Return to the test after the actual sampler finishes its report. */
static void pexit(char *message, int code) { check(message[0] == 0 && code == 0); }

/* No heap allocations need leak scanning in this restricted host fixture. */
int __lsan_is_turned_off(void) { return 1; }

typedef struct Chan Chan;

struct Chan {
	struct { uvlong path; } qid; // Device endpoint identity consumed by the actual reader.
};

enum { Benchdirqid, Benchdataqid, Benchusqid }; // Reader endpoint selectors.
static int benchtab[1]; // Placeholder directory table for the unused directory route.
#define nelem(a) (sizeof(a)/sizeof((a)[0])) // Inferno fixed-array length helper.


/* The numeric reader never routes into directory generation. */
static void devgen(void) { abort(); }


/* Reject an unexpected directory branch in this focused fixture. */
static long devdirread(Chan *c, void *p, long n, int *table, int count, void (*gen)(void))
{
	(void)c; (void)p; (void)n; (void)table; (void)count; (void)gen; abort();
}


/* Read the actual generated string through the caller's logical file offset. */
static long
readstr(vlong offset, void *out, long count, char *text)
{
	size_t length;

	length = strlen(text);
	if(offset < 0 || (uvlong)offset >= length || count <= 0)
		return 0;
	if((uvlong)count > length-(uvlong)offset)
		count = length-(uvlong)offset;
	memcpy(out, text+offset, count);
	return count;
}


/* Reject an unexpected report-channel branch in the clock reader tests. */
static long unexpected(int report, void *p, long n, unsigned long offset)
{
	(void)report; (void)p; (void)n; (void)offset; abort();
}
static long (*Test[])(int, void*, long, unsigned long) = { unexpected }; // Unused data-channel adapter.

STATISTICS

IMPLEMENTATION


/* Use a centered long-double two-pass oracle independent of Welford/merge updates. */
static void
population(vlong *values, int count)
{
	Stat direct, merged, part, empty;
	long double sum, mean, variance, delta;
	int i, split;
	vlong deviation, observed;

	stat(Reset, &direct, 0);
	stat(Reset, &empty, 0);
	sum = 0.0L;
	for(i = 0; i < count; i++){
		stat(Inc, &direct, values[i]);
		sum += (long double)values[i]-(long double)values[0];
	}
	mean = count ? sum/count : 0.0L;
	variance = 0.0L;
	for(i = 0; i < count; i++){
		delta = ((long double)values[i]-(long double)values[0])-mean;
		variance += delta*delta;
	}
	variance = count ? sqrtl(variance/count) : 0.0L;
	deviation = variance >= 9223372036854775807.0L ? LLONG_MAX : (vlong)variance;
	observed = sdev(&direct);
	check(direct.n == count);
	check(fabsl((long double)observed-deviation) <= 1.0L+variance*1e-12L);
	if(count){
		mean += values[0];
		check(fabsl((long double)statmean(&direct)-mean) <= 2048.0L);
		for(i = 0; i < count; i++)
			check(direct.min <= values[i] && direct.max >= values[i]);
	}
	for(split = 0; split <= count; split++){
		stat(Reset, &merged, 0);
		stat(Reset, &part, 0);
		for(i = 0; i < split; i++)
			stat(Inc, &merged, values[i]);
		for(i = split; i < count; i++)
			stat(Inc, &part, values[i]);
		statinc(&merged, &part);
		statinc(&merged, &empty);
		check(merged.n == direct.n && merged.min == direct.min && merged.max == direct.max);
		check(fabsl((long double)sdev(&merged)-deviation) <= 1.0L+variance*1e-12L);
		check(fabsl((long double)statmean(&merged)-statmean(&direct)) <= 2048.0L);
	}
}


/* Exercise all split points, huge offsets, small spreads, opposite signs and random sets. */
static void
statistics(void)
{
	vlong values[33], bases[] = { 0, 10000000000LL, LLONG_MAX-1024, LLONG_MIN+1024 };
	uvlong random;
	int b, n, i;
	Stat s, before;

	population(values, 0);
	for(b = 0; b < 4; b++)
	for(n = 1; n <= 33; n++){
		for(i = 0; i < n; i++)
			values[i] = bases[b]+i%7;
		population(values, n);
	}
	values[0] = LLONG_MIN; values[1] = LLONG_MAX;
	population(values, 2);
	random = 314159;
	for(n = 1; n <= 33; n++){
		for(i = 0; i < n; i++){
			random = random*6364136223846793005ULL+1;
			values[i] = (vlong)(random%20000000001ULL)-10000000000LL;
		}
		population(values, n);
	}
	stat(Reset, &s, 0);
	stat(Inc, &s, 3000000001LL);
	check(sdev(&s) == 0 && statmean(&s) == 3000000001LL);
	stat(Inc, &s, 3000000003LL);
	check(sdev(&s) == 1 && statmean(&s) == 3000000002LL);
	{
		int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
		int mode, flags;

		for(mode = 0; mode < 4; mode++){
			fesetround(modes[mode]);
			feclearexcept(FE_ALL_EXCEPT);
			feraiseexcept(FE_DIVBYZERO);
			flags = fetestexcept(FE_ALL_EXCEPT);
			benchcontrol = RND_PINF;
			before = s;
			stat(Inc, &s, 3000000004LL);
			check(fegetround() == modes[mode] && fetestexcept(FE_ALL_EXCEPT) == flags);
			check(benchcontrol == RND_PINF && !benchfpactive);
			s = before;
		}
		fesetround(FE_TONEAREST);
		feclearexcept(FE_ALL_EXCEPT);
	}
#if defined(__GLIBC__) && defined(__linux__)
	{
		extern int feenableexcept(int), fedisableexcept(int), fegetexcept(void);
		int original, enabled;

		original = fegetexcept();
		feclearexcept(FE_ALL_EXCEPT);
		enabled = FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW|FE_INEXACT;
		feenableexcept(enabled);
		benchcontrol = INVAL|OVFL|UNFL|INEX;
		before = s;
		stat(Inc, &s, 3000000004LL);
		check((fegetexcept()&enabled) == enabled);
		fedisableexcept(FE_ALL_EXCEPT);
		feenableexcept(original);
		s = before;
	}
#endif
	before = s;
	statinc(&s, &s);
	check(s.n == 4 && sdev(&s) == 1 && statmean(&s) == 3000000002LL);
	s = before; s.n = LLONG_MAX;
	before = s;
	stat(Inc, &s, 1);
	check(!memcmp(&s, &before, sizeof(s)));
	statinc(&s, &s);
	check(!memcmp(&s, &before, sizeof(s)));
}


/* Check exact units, threshold equality, zero calibration and overflow-safe scaling. */
static void
formatting(void)
{
	MS2TS = 1000000;
	check(!strcmp(ts2str(1000000000LL), "1.00 s"));
	check(!strcmp(ts2str(2000000000LL), "2.00 s"));
	check(!strcmp(ts2str(1000000), "1.00 ms"));
	check(!strcmp(ts2str(1000), "1.00 us"));
	check(!strcmp(ts2str(1), "1.00 ns"));
	check(!strcmp(ts2str(-1000000000LL), "-1.00 s"));
	check(fabs(ts2us(1234567)-1234.567) < 1e-8);
	MS2TS = LLONG_MAX;
	check(!strcmp(ts2str(LLONG_MAX), "1.00 ms"));
	MS2TS = 0;
	check(!strcmp(ts2str(17), "17 ticks"));
	check(ts2us(17) == 0.0);
	MS2TS = -1;
	check(ts2us(17) == 0.0);
	MS2TS = 1;
	check(isfinite(ts2us(LLONG_MAX)));
}


/* Verify counter midpoint crossing, backward samples, bounded conversion and early reset. */
static void
elapsed(void)
{
	MS2TS = US2TS = 0; ticks = 17;
	check(bus(1) == 0 && bench.tickstart == 17);
	check(bus(0) == 0);
	MS2TS = 999;
	ticks = 1016;
	check(bus(0) == 1000);
	MS2TS = 1000; US2TS = 1;
	ticks = 0x7ffffffffffffffcULL;
	bus(1); ticks += 9;
	check(bus(0) == 9);
	ticks = 100; bus(1); ticks = 99;
	check(bus(0) == 0);
	ticks = ~0ULL-3; bus(1); ticks = 5;
	check(bus(0) == 9);
	MS2TS = 1; ticks = 0; bus(1); ticks = LLONG_MAX;
	check(bus(0) == LLONG_MAX);
	MS2TS = 1000; ticks = 0; bus(1); ticks = LLONG_MAX-1;
	check(bus(0) == LLONG_MAX-1);
}


/* Exact scaled remainders use an independent wide-integer oracle on the native host. */
static void
scaling(void)
{
#ifdef __SIZEOF_INT128__
	uvlong random, numerator;
	__uint128_t expected;
	Chan channel = { { Benchusqid } };
	char out[64];
	long length;
	int i, flags, mode;

	random = 271828;
	for(i = 0; i < 4096; i++){
		random = random*6364136223846793005ULL+1;
		MS2TS = (vlong)(random&0x7fffffffffffffffULL);
		if(MS2TS == 0) MS2TS = 1;
		random = random*6364136223846793005ULL+1;
		numerator = random;
		expected = (__uint128_t)numerator*1000/(uvlong)MS2TS;
		if(expected > LLONG_MAX) expected = LLONG_MAX;
		ticks = numerator;
		length = benchread(&channel, out, sizeof(out)-1, 0); out[length] = 0;
		check(strtoull(out, nil, 10) == (uvlong)expected);
	}
	for(mode = 0; mode < 4; mode++){
		int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };

		fesetround(modes[mode]);
		feclearexcept(FE_ALL_EXCEPT);
		feraiseexcept(FE_DIVBYZERO);
		flags = fetestexcept(FE_ALL_EXCEPT);
		MS2TS = LLONG_MAX; ticks = LLONG_MAX-1;
		length = benchread(&channel, out, sizeof(out)-1, 0); out[length] = 0;
		check(!strcmp(out, "999"));
		check(fegetround() == modes[mode] && fetestexcept(FE_ALL_EXCEPT) == flags);
	}
	fesetround(FE_TONEAREST);
	feclearexcept(FE_ALL_EXCEPT);
#endif
}


/* Sample real calibration/base/operation loops with intervals above signed-total capacity. */
static void
samplers(void)
{
	Psync ps = { 0, 1 };
	unsigned long long total;
	unsigned low, high;
	int pid, count;

	stride = 10000000; ticks = 0xfffffff0;
	MS2TS = 0; cal(0, &ps, 0, 0);
	check(MS2TS == 10000000);
	base(0, &ps, 0, 0);
	check(sscanf(reportline, "%d %d %llu %u %u", &pid, &count, &total, &low, &high) == 5);
	check(pid == 17 && count == 1000 && total == 10000000000ULL && low == stride && high == stride);
	calls = 0; bench.op = operation;
	timeop0(&ps);
	check(calls == 1000);
	check(sscanf(reportline, "%d %d %llu %u %u", &pid, &count, &total, &low, &high) == 5);
	check(total == 10000000000ULL && low == stride && high == stride);
}


/* Read boundaries never form pointers for negative lengths or offsets beyond the buffer. */
static void
reading(void)
{
	char text[] = "abcdef", output[16];
	unsigned long offsets[] = { 0, 1, 5, 6, 7, ULONG_MAX, ULONG_MAX/2 };
	long sizes[] = { LONG_MIN, -1, 0, 1, 3, LONG_MAX };
	int i, j;
	long expected, got;

	bench.buf = text; bench.wpos = text+6; bench.bufsz = sizeof(text);
	for(i = 0; i < 7; i++)
	for(j = 0; j < 6; j++){
		memset(output, 0x5a, sizeof(output));
		expected = sizes[j] <= 0 || offsets[i] >= 6 ? 0 : sizes[j] < (long)(6-offsets[i]) ? sizes[j] : 6-offsets[i];
		got = rep(output, sizes[j], offsets[i]);
		check(got == expected);
		if(got > 0)
			check(!memcmp(output, text+offsets[i], got));
		check(output[got] == 0x5a);
	}
	bench.buf = bench.wpos = nil;
	check(rep(output, 5, 0) == 0);
}


/* Check zero calibration, sub-microsecond frequency, unsigned absolute ticks and read offsets. */
static void
clockreading(void)
{
	Chan channel = { { Benchusqid } };
	char output[64];
	long size;

	memset(output, 0, sizeof(output));
	MS2TS = US2TS = 0; ticks = 1234;
	size = benchread(&channel, output, sizeof(output)-1, 0);
	check(size == 1 && !strcmp(output, "0"));
	MS2TS = 999;
	size = benchread(&channel, output, sizeof(output)-1, 0); output[size] = 0;
	check(!strcmp(output, "1235"));
	size = benchread(&channel, output, sizeof(output)-1, 2); output[size] = 0;
	check(!strcmp(output, "35"));
	check(benchread(&channel, output, sizeof(output)-1, 4) == 0);
	MS2TS = 1000; ticks = 0x8000000000000000ULL;
	size = benchread(&channel, output, sizeof(output)-1, 0); output[size] = 0;
	check(!strcmp(output, "9223372036854775807"));
}


/* Keep square overflow, merge overflow and low-spread cancellation independently replayable. */
static void
focused(char *selected)
{
	Stat left, right;

	stat(Reset, &left, 0);
	stat(Reset, &right, 0);
	if(!strcmp(selected, "large")){
		stat(Inc, &left, 3037000500LL);
		check(sdev(&left) == 0 && statmean(&left) == 3037000500LL);
	}else if(!strcmp(selected, "merging")){
		stat(Inc, &left, 3000000000LL);
		stat(Inc, &right, 3000000000LL);
		statinc(&left, &right);
		check(left.n == 2 && sdev(&left) == 0 && statmean(&left) == 3000000000LL);
	}else{
		stat(Inc, &left, 1000);
		stat(Inc, &left, 1001);
		check(sdev(&left) == 0);
	}
}


/* Select independent parent-replay families without hiding any failed predicates. */
int
main(void)
{
	char *selected;

	selected = getenv("BENCH_NUMERIC_CASE");
	if(selected == nil || !strcmp(selected, "statistics")) statistics();
	if(selected == nil || !strcmp(selected, "large")) focused("large");
	if(selected == nil || !strcmp(selected, "merging")) focused("merging");
	if(selected == nil || !strcmp(selected, "spread")) focused("spread");
	if(selected == nil || !strcmp(selected, "formatting")) formatting();
	if(selected == nil || !strcmp(selected, "elapsed")) elapsed();
	if(selected == nil || !strcmp(selected, "scaling")) scaling();
	if(selected == nil || !strcmp(selected, "samplers")) samplers();
	if(selected == nil || !strcmp(selected, "reading")) reading();
	if(selected == nil || !strcmp(selected, "clockreading")) clockreading();
	printf("PASS benchmark numeric %s: %lu predicates\n", selected ? selected : "all", checks);
	return 0;
}
