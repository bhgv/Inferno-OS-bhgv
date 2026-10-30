#include "dat.h"
#include "fns.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#undef up

static Proc worker; // Only ordinary public error-string routing is needed by the selected bodies.
static Osenv environment; // Stable process-owned error buffers.
Proc *up; // Public current-process route used by actual emu/os kgerrstr.
static int predicates; // Active checks survive NDEBUG.


/* Distinguish a measured semantic failure from a compiler or sanitizer failure. */
static void
check(int condition)
{
	if(!condition){
		fprintf(stderr, "error read predicate failed after %d checks\n", predicates);
		abort();
	}
	predicates++;
}


/* Disable only unavailable process-inspection leak scanning on restricted hosts. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

ERROR_IMPLEMENTATION


/* Every positive span has a terminator and preserves bytes outside that span. */
static void
copies(void)
{
	uchar bytes[64];
	char source[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	int span, at;

	for(span=1; span<48; span++){
		memset(bytes, 0xa5, sizeof(bytes));
		kstrcpy((char*)bytes+4, source, span);
		check(memchr(bytes+4, 0, span) != nil);
		for(at=0; at<sizeof(bytes); at++)
			if(at<4 || at>=4+span)
				check(bytes[at] == 0xa5);
		if(span<4){
			check(bytes[4+span-1] == 0);
			check(memcmp(bytes+4, source, span-1) == 0);
		}
	}
	kstrcpy(nil, nil, 0);
	kstrcpy(nil, nil, -1);
	predicates += 2;
}


/* Reading is repeatable, never consumes caller output, and leaves process error bytes intact. */
static void
reads(void)
{
	char before[ERRMAX], output[2*ERRMAX+8];
	unsigned int spans[] = {ERRMAX, 1, 2, 3, 4, 7, 0, ERRMAX+1, UINT_MAX};
	unsigned int i, at, capacity;

	memset(&environment, 0, sizeof(environment));
	memset(&worker, 0, sizeof(worker));
	up = &worker;
	worker.env = &environment;
	environment.errstr = environment.errbuf0;
	strcpy(environment.errstr, "original process error");
	memmove(before, environment.errstr, sizeof(before));
	for(i=0; i<nelem(spans); i++){
		memset(output, 0xa5, sizeof(output));
		strcpy(output+4, "output seed");
		kgerrstr(output+4, spans[i]);
		check(memcmp(environment.errstr, before, sizeof(before)) == 0);
		capacity = spans[i] > ERRMAX ? ERRMAX : spans[i];
		if(capacity != 0)
			check(memchr(output+4, 0, capacity) != nil);
		if(capacity >= sizeof("original process error"))
			check(memcmp(output+4, before, sizeof("original process error")) == 0);
		for(at=0; at<4; at++)
			check((uchar)output[at] == 0xa5);
		for(at=4+ERRMAX; at<sizeof(output); at++)
			check((uchar)output[at] == 0xa5);
	}
	kgerrstr(nil, 0);
	check(memcmp(environment.errstr, before, sizeof(before)) == 0);
	up = nil;
	kgerrstr(output, sizeof(output));
	check(strcmp(output, "<no-up>") == 0);
}


/* Select independent read-state and tiny-copy controls for parent source replay. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(strcmp(argv[1], "read") == 0)
		reads();
	else if(strcmp(argv[1], "copy") == 0)
		copies();
	else
		check(0);
	printf("PASS actual error %s: %d active checks\n", argv[1], predicates);
	return 0;
}
