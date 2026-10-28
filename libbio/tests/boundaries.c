#include "lib9.h"
#include <bio.h>
#include <stdio.h>
#include <limits.h>

#define check(test) predicate(!!(test), __LINE__) // Retain every check under NDEBUG.

static ulong cases; // Verified boundary/state combinations.
static int reads, seeks, flushes; // Native callback observations.


/* Fail at the first incorrect value, state mutation or native callback. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "Bio boundary failure at %d\n", line);
		abort();
	}
}


/* Retain instrumentation without unavailable LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Test replay paths must never refill from an external source. */
static long
inputread(int fd, void *p, long count)
{
	(void)fd; (void)p; (void)count;
	reads++;
	return 0;
}


/* Count native seeks; an erroneous fallback must remain observable. */
static vlong
inputseek(int fd, vlong offset, int base)
{
	(void)fd; (void)base;
	seeks++;
	return offset;
}


/* Read-side boundary checks must not flush an output stream. */
int
Bflush(Biobuf *bp)
{
	(void)bp;
	flushes++;
	return 0;
}


/* Unsupported-state diagnostics are observable without the host formatter. */
int
fprint(int fd, char *format, ...)
{
	(void)fd; (void)format;
	return 0;
}

#define read inputread // Observe unexpected native refills.
#define seek inputseek // Observe unexpected native seeks.
IMPLEMENTATION
#undef read
#undef seek


/* Initialize the actual allocated reserve and fill every byte with a distinct pattern. */
static void
setup(Biobuf *b, int size, int state)
{
	int i;

	memset(b, 0, sizeof(*b));
	b->bbuf = b->b+Bungetsize;
	b->ebuf = b->bbuf+size;
	b->gbuf = b->bbuf;
	b->bsize = size;
	b->state = state;
	for(i = 0; i < size+Bungetsize; i++)
		b->b[i] = (i*37+11)&255;
	reads = seeks = flushes = 0;
}


/* Check every pushback cursor around both storage ends and extreme int values. */
static void
pushback(int rune)
{
	Biobuf b, saved;
	int size, state, cursor, width, result, valid;
	int extreme[] = {INT_MIN, INT_MIN+1, INT_MAX};
	vlong next;

	for(size = 1; size <= 32; size++)
	for(state = Binactive; state <= Bend; state++)
	for(cursor = -size-Bungetsize-2; cursor <= 2; cursor++)
	for(width = (rune ? -1 : 1); width <= (rune ? UTFmax+1 : 1); width++){
		setup(&b, size, state);
		b.icount = cursor;
		b.runesize = width;
		saved = b;
		next = (vlong)cursor-(rune ? width : 1);
		valid = (state == Bractive || state == Bracteof) && cursor <= 0 &&
		        (!rune || (width >= 0 && width <= UTFmax)) &&
		        next >= -(vlong)size-Bungetsize;
		result = rune ? Bungetrune(&b) : Bungetc(&b);
		check(result == (valid ? 1 : Beof));
		if(valid){
			saved.icount = next;
			saved.state = Bractive;
			if(rune)
				saved.runesize = 0;
		}
		check(!memcmp(&b, &saved, sizeof(b)) && !reads && !seeks && !flushes);
		if(valid && next < 0){
			check(Bgetc(&b) == b.ebuf[next] && b.icount == next+1 && !reads);
		}
		cases++;
	}
	for(cursor = 0; cursor < nelem(extreme); cursor++){
		setup(&b, 16, Bracteof);
		b.icount = extreme[cursor];
		b.runesize = UTFmax;
		saved = b;
		check((rune ? Bungetrune(&b) : Bungetc(&b)) == Beof);
		check(!memcmp(&b, &saved, sizeof(b)));
		cases++;
	}
	/* The largest int buffer still cannot push the count below INT_MIN. */
	setup(&b, 1, Bractive);
	b.bsize = INT_MAX;
	b.icount = INT_MIN;
	b.runesize = UTFmax;
	saved = b;
	check((rune ? Bungetrune(&b) : Bungetc(&b)) == Beof);
	check(!memcmp(&b, &saved, sizeof(b)));
	cases++;
}


/* Use a wider independent arithmetic domain for all signed-position boundaries. */
static void
positions(void)
{
	Biobuf b, saved;
	vlong maximum, offsets[9], expected;
	int values[] = {INT_MIN, -8192, -1, 0, 1, 8192, INT_MAX};
	int i, j, k, state;
	__int128 total, delta;

	maximum = (vlong)(~(uvlong)0 >> 1);
	offsets[0] = -maximum-1; offsets[1] = -maximum; offsets[2] = -maximum+8192;
	offsets[3] = -1; offsets[4] = 0; offsets[5] = 1;
	offsets[6] = maximum-8192; offsets[7] = maximum-1; offsets[8] = maximum;
	for(state = Bractive; state <= Bracteof; state++)
	for(i = 0; i < nelem(offsets); i++)
	for(j = 0; j < nelem(values); j++)
	for(k = 0; k < nelem(values); k++){
		setup(&b, 1, state);
		b.offset = offsets[i];
		b.icount = b.ocount = values[j];
		b.bsize = values[k];
		saved = b;
		delta = state == Bwactive ? (__int128)b.bsize+b.ocount : b.icount;
		total = (__int128)b.offset+delta;
		expected = total < -(__int128)maximum-1 || total > maximum ? Beof : (vlong)total;
		check(Boffset(&b) == expected && !memcmp(&b, &saved, sizeof(b)));
		cases++;
	}
}


/* Query counts without signed int negation or addition overflow. */
static void
buffered(void)
{
	Biobuf b, saved;
	int values[] = {INT_MIN, INT_MIN+1, -8192, -1, 0, 1, 8192, INT_MAX-1, INT_MAX};
	int state, i, j, expected;
	vlong n;

	for(state = Binactive; state <= Bend; state++)
	for(i = 0; i < nelem(values); i++)
	for(j = 0; j < nelem(values); j++){
		setup(&b, 1, state);
		b.icount = b.ocount = values[i];
		b.bsize = values[j];
		saved = b;
		n = state == Bwactive ? (vlong)b.bsize+b.ocount : -(vlong)b.icount;
		expected = state == Binactive || state == Bend ? 0 : n < 0 || n > INT_MAX ? Beof : n;
		check(Bbuffered(&b) == expected && !memcmp(&b, &saved, sizeof(b)));
		cases++;
	}
}


/* A failed logical-position query must stop relative seeking before any callback. */
static void
seekfailure(void)
{
	Biobuf b, saved;
	vlong maximum;
	int state;

	maximum = (vlong)(~(uvlong)0 >> 1);
	for(state = Bractive; state <= Bracteof; state++){
		if(state == Bwactive)
			continue;
		setup(&b, 16, state);
		b.offset = -maximum-1;
		b.icount = -1;
		saved = b;
		check(Bseek(&b, 1, 1) == Beof && !seeks && !flushes);
		check(!memcmp(&b, &saved, sizeof(b)));
		cases++;
	}
}


/* Isolate independent old-source regressions and report explicit active checks. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(!strcmp(argv[1], "byte"))
		pushback(0);
	else if(!strcmp(argv[1], "rune"))
		pushback(1);
	else if(!strcmp(argv[1], "offset"))
		positions();
	else if(!strcmp(argv[1], "buffered"))
		buffered();
	else
		seekfailure();
	printf("PASS Bio %s boundaries: %lu cases\n", argv[1], cases);
	return 0;
}
