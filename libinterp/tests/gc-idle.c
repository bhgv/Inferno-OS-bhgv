#include "dat.h"
#include "fns.h"
#include "interp.h"
#include <stdio.h>
#include <limits.h>

#define check(test) predicate(!!(test), __LINE__) // Preserve wait-count and scheduler checks under NDEBUG.
#define log benchlog // Isolate the benchmark's output sink from libc's math symbol.

ulong gcepochs; // Public unsigned completed-epoch counter used by production waiters.
int gccolor, gchalt; // Color selector and ordinary collector run permission.
static uvlong gcidle, gcpartial, gcidlepass; // Scheduler accounting observed by the actual waiters.
static int nidle, ready, interval, steps, added, removed, worklimit; // Controlled native/scheduler boundaries.
static vlong ticks; // Deterministic benchmark timing source.
static Proc worker; // The current runtime process used by actual idle code.
static Prog program; // Borrowed scheduler/program identity.
static ulong checks; // Complete waiter scenarios.

static struct {
	Proc *vmq; // Pending VM owner that interrupts idle collection.
	Prog *runhd; // Scheduler head, equal to tail while truly idle.
	Prog *runtl; // Scheduler tail.
	Prog *head; // Root graph passed to the collector boundary.
} isched;


/* Report a bad epoch count, phase, callback or scheduler transition. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "GC idle failure at %d\n", line);
		abort();
	}
}


/* Keep address instrumentation without unavailable process-wide LSan inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Supply the actual up macro with a stable process object. */
Proc*
getup(void)
{
	return &worker;
}


/* Model ready work at the original preflight/benchmark polling boundary. */
static int
tready(void *unused)
{
	USED(unused);
	return ready;
}


/* Observe the native scheduler ownership acquired by an idle pass. */
void
addrun(Prog *p)
{
	check(p == &program && worker.type == IdleGC);
	added++;
}


/* The production idle waiter must restore process state before leaving the run queue. */
void
delrunq(Prog *p)
{
	check(p == &program && worker.type == Interp);
	removed++;
}


/* Yield hooks should not be reached in these bounded short idle passes. */
void
osyield(void)
{
	abort();
}


/* Preserve the kernel waiter's corresponding scheduling hook. */
void
sched(void)
{
	abort();
}


/* Resolve the benchmark's borrowed root program. */
Prog*
progn(int index)
{
	check(index == 0);
	return &program;
}


/* Produce monotonic short timing observations for the real benchmark wrapper. */
vlong
archrdtsc(void)
{
	return ++ticks;
}


/* Only the conversion's data flow matters to epoch waiting. */
static double
ts2us(vlong elapsed)
{
	return elapsed;
}


/* Retain the benchmark output boundary without logging during each test case. */
static void
benchlog(char *format, ...)
{
	check(format != nil);
}


/* Complete an epoch every selected number of GC slices, with mathematically correct phase rollover. */
void
rungc(Prog *p)
{
	check(p == &program && steps < 32);
	steps++;
	if(steps%interval == 0){
		gcepochs++;
		gccolor = gccolor == INT_MAX ? 2 : gccolor+1;
	}
	if(worklimit != 0 && steps == worklimit)
		gchalt = 1;
}

IMPLEMENTATION


/* Compare exact elapsed epochs across both counters' limits and normal interruption paths. */
int
main(void)
{
	int colors[] = {3, 4, 5, INT_MAX-3, INT_MAX-2, INT_MAX-1, INT_MAX};
	ulong epochs[] = {0, 1, 7, ULONG_MAX-3, ULONG_MAX-2, ULONG_MAX-1, ULONG_MAX};
	int c, e, mode, expected;

	for(c = 0; c < nelem(colors); c++)
	for(e = 0; e < nelem(epochs); e++)
	for(interval = 1; interval <= 4; interval++)
	for(mode = 0; mode < 5; mode++){
		memset(&worker, 0, sizeof(worker));
		memset(&isched, 0, sizeof(isched));
		worker.type = Interp;
		worker.prog = &program;
		isched.head = &program;
		isched.runhd = isched.runtl = &program;
		gccolor = colors[c];
		gcepochs = epochs[e];
		gchalt = mode == 2;
		ready = mode == 1;
		worklimit = mode == 3 ? 2 : 0;
		if(mode == 4)
			isched.vmq = &worker;
		steps = added = removed = 0;
		gcidle = gcidlepass = gcpartial = ticks = 0;
		nidle = 1;
#ifdef TEST_BENCH
		check(idlegc(nil) == 1);
		expected = mode == 1 || mode == 2 ? 0 : mode == 3 ? 2 : 3*interval;
		check(added == 0 && removed == 0 && ticks == 2*expected);
#else
		execatidle();
		expected = mode == 1 || mode == 2 || mode == 4 ? 0 : mode == 3 ? 2 : 3*interval;
		check(added == (mode != 1) && removed == added && gcidle == added);
		check(gcidlepass == expected && gcpartial == (mode == 4));
		check(worker.type == Interp);
#endif
		check(steps == expected && (ulong)(gcepochs-epochs[e]) == expected/interval);
		check(gccolor >= 2);
		checks++;
	}
	printf("PASS GC idle: %lu epoch/phase/scheduler scenarios\n", checks);
	return 0;
}
