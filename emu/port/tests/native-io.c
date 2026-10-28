#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "kernel.h"
#include <stdio.h>
#include <limits.h>

#define check(test) predicate(!!(test), __LINE__) // Keep native-width and lock checks active under NDEBUG.

static uchar storage[160], before[160], expected[160], written[160]; // Complete payload and canary images.
static void *base; // Borrowed native transfer base, including zero-count identities.
static long script[132]; // Full-width native replies selected independently of production code.
static int scriptlen, step, done, amount, writing; // Script cursor and accepted service effects.
static int outside, released, acquired, errors; // Interpreter-lock and diagnostic observations.
static ulong checks; // Verified service/result/cursor scenarios.


/* Diagnose a bad result, buffer image, native request or lock transition. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "native library I/O failure at %d\n", line);
		abort();
	}
}


/* Retain address instrumentation without unavailable LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Observe the single interpreter release around a validated native operation. */
void
release(void)
{
	check(outside == 0);
	outside = 1;
	released++;
}


/* Every released path must reacquire before returning to its VM caller. */
void
acquire(void)
{
	check(outside == 1);
	outside = 0;
	acquired++;
}


/* Distinguish wrapper validation from errors already returned by native services. */
void
kwerrstr(char *format, ...)
{
	check(format != nil && outside == 0);
	errors++;
}


/* Apply bytes only for in-contract replies, exposing oversized replies without unsafe stub writes. */
static long
service(int fd, void *buffer, long n, int write)
{
	long result, i;

	check(fd == 7 && outside == 1 && write == writing && n >= 0);
	check((uintptr)buffer == (uintptr)base+done && n == amount-done);
	check(step < scriptlen);
	result = script[step++];
	if(result > 0 && result <= n){
		for(i = 0; i < result; i++){
			if(write)
				written[done+i] = ((uchar*)buffer)[i];
			else
				((uchar*)buffer)[i] = (done+i)*19+7;
		}
		done += result;
	}
	return result;
}


/* Keep complete long read results at the service boundary. */
long
kread(int fd, void *buffer, long n)
{
	return service(fd, buffer, n, 0);
}


/* Keep complete long write results at the service boundary. */
long
kwrite(int fd, void *buffer, long n)
{
	return service(fd, buffer, n, 1);
}

IMPLEMENTATION


/* Start an isolated transfer with unchanged neighboring bytes and no lock debt. */
static void
reset(int op, int shift, int n)
{
	int i;

	check(outside == 0);
	for(i = 0; i < sizeof(storage); i++)
		storage[i] = i*11+3;
	memmove(before, storage, sizeof(storage));
	memmove(expected, storage, sizeof(storage));
	memset(written, 0xa5, sizeof(written));
	base = storage+16+shift;
	amount = n;
	writing = op == 2;
	scriptlen = step = done = released = acquired = errors = 0;
}


/* Select the actual public bridge without changing argument/result widths. */
static int
invoke(int op, void *buffer, long n)
{
	if(op == 0)
		return libread(7, buffer, (int)n);
	if(op == 1)
		return libreadn(7, buffer, n);
	return libwrite(7, buffer, (int)n);
}


/* Verify single native replies and their int-visible result before/after narrowing. */
static void
replies(void)
{
	long values[10], reply;
	int op, shift, n, which, want, copied, calls, error, i;

	check(sizeof(long) == 8);
	for(op = 0; op < 3; op++)
	for(shift = 0; shift < 8; shift++)
	for(n = 0; n <= 64; n++){
		if(getenv("NATIVE_IO_OP") && op != atoi(getenv("NATIVE_IO_OP")))
			continue;
		values[0] = 0;
		values[1] = n;
		values[2] = n/2;
		values[3] = -1;
		values[4] = -7;
		values[5] = n+1;
		values[6] = (1LL<<32)+3;
		values[7] = -(1LL<<32)+3;
		values[8] = LONG_MAX;
		values[9] = LONG_MIN;
		for(which = 0; which < nelem(values); which++){
			if(getenv("NATIVE_IO_REPLY") && which != atoi(getenv("NATIVE_IO_REPLY")))
				continue;
			reset(op, shift, n);
			reply = values[which];
			script[0] = reply;
			script[1] = 0;
			scriptlen = 2;
			copied = reply > 0 && reply <= n ? reply : 0;
			error = reply > n;
			want = reply < INT_MIN ? -1 : reply < 0 ? (int)reply : error ? -1 : reply;
			calls = 1+(op == 1 && copied > 0 && copied < n);
			if(op == 1 && n == 0){
				want = error = copied = calls = 0;
			}
			if(op != 2)
				for(i = 0; i < copied; i++)
					expected[16+shift+i] = i*19+7;
			check(invoke(op, base, n) == want);
			check(!memcmp(storage, expected, sizeof(storage)));
			if(op == 2){
				for(i = 0; i < copied; i++)
					check(written[i] == before[16+shift+i]);
				check(written[copied] == 0xa5);
			}
			check(step == calls && errors == error && outside == 0);
			check(released == 1 && acquired == 1);
			checks++;
		}
	}
}


/* Retain exact short-read prefixes on EOF/errors and reject an oversized later reply. */
static void
streams(void)
{
	int n, chunk, cut, end, i, count, tail, want;
	long terminal[] = {0, -1, -7, LONG_MIN, LONG_MAX, (1LL<<32)+3};

	for(n = 1; n <= 32; n++)
	for(chunk = 1; chunk <= 8; chunk++)
	for(cut = 0; cut <= n; cut++)
	for(tail = 0; tail < nelem(terminal); tail++){
		reset(1, cut%8, n);
		for(end = 0; end < cut; end += count){
			count = cut-end < chunk ? cut-end : chunk;
			script[scriptlen++] = count;
		}
		if(cut < n)
			script[scriptlen++] = terminal[tail];
		for(i = 0; i < cut; i++)
			expected[16+cut%8+i] = i*19+7;
		want = cut;
		if(cut == 0 && terminal[tail] < 0)
			want = terminal[tail] < INT_MIN ? -1 : terminal[tail];
		if(cut < n && terminal[tail] > n-cut)
			want = -1;
		check(libreadn(7, base, n) == want);
		check(!memcmp(storage, expected, sizeof(storage)));
		check(step == scriptlen && done == cut && outside == 0);
		check(errors == (cut < n && terminal[tail] > n-cut));
		check(released == 1 && acquired == 1);
		checks++;
	}
}


/* Reject invalid requests before lock changes while preserving legal zero-count behavior. */
static void
spans(void)
{
	void *buffer;
	long n;
	int op, kind;

	for(op = 0; op < 3; op++)
	for(kind = 0; kind < 7; kind++){
		if(op != 1 && kind == 6)
			continue;
		reset(op, 0, 16);
		buffer = base;
		n = 16;
		switch(kind){
		case 0: buffer = nil; break;
		case 1: buffer = H; break;
		case 2: buffer = (void*)((uintptr)-1-7); break;
		case 3: n = -1; break;
		case 4: n = INT_MIN; break;
		case 5: buffer = (void*)((uintptr)-1-15); break;
		default: n = (long)INT_MAX+1; break;
		}
		check(invoke(op, buffer, n) == -1);
		check(!memcmp(storage, before, sizeof(storage)));
		check(step == 0 && errors == 1 && released == 0 && acquired == 0 && outside == 0);
		checks++;
	}
	for(op = 0; op < 3; op++)
	for(kind = 0; kind < 2; kind++){
		reset(op, 0, 0);
		base = kind == 0 ? nil : H;
		script[0] = 0;
		scriptlen = 1;
		check(invoke(op, base, 0) == 0);
		check(step == (op == 1 ? 0 : 1) && errors == 0);
		check(released == 1 && acquired == 1 && outside == 0);
		checks++;
	}
}


/* Isolate parent-source reply/stream/span failures without losing service contracts. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(!strcmp(argv[1], "reply"))
		replies();
	else if(!strcmp(argv[1], "stream"))
		streams();
	else
		spans();
	check(checks > 0);
	printf("PASS native library I/O %s: %lu width/prefix/lock/span scenarios\n", argv[1], checks);
	return 0;
}
