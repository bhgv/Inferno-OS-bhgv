#include "lib9.h"
#include <bio.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static int opens, creates, closes, allocations, releases, failopen, failalloc;
static int lastmode, flushwrites, used, checks, registrations, failclose;
static void (*exitfn)(void);
static uchar output[64];

/* Disable only unsupported LeakSanitizer thread inspection; allocation checks stay active. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Capture descriptor acquisition using the native read-only open path. */
static int
inputopen(char *name, int mode)
{
	assert(strcmp(name, "buffer-test") == 0);
	opens++;
	lastmode = mode;
	return failopen ? -1 : 17;
}

/* Keep creation independent from the read-only open path. */
static int
outputcreate(char *name, int mode, int perm)
{
	assert(strcmp(name, "buffer-test") == 0 && perm == 0666);
	creates++;
	lastmode = mode;
	return failopen ? -1 : 17;
}

/* Count descriptor releases, including allocation-failure cleanup. */
static int
inputclose(int fd)
{
	assert(fd == 17);
	closes++;
	return failclose ? -1 : 0;
}

/* Inject allocation failure after the descriptor has already been acquired. */
static void*
bufferalloc(ulong n)
{
	allocations++;
	return failalloc ? nil : malloc(n);
}

/* Count owned-buffer releases without hiding allocator errors. */
static void
bufferfree(void *p)
{
	assert(p != nil);
	releases++;
	free(p);
}

/* Record exit flushing without leaving stack buffers in the host exit list. */
static int
registerexit(void (*fn)(void))
{
	registrations++;
	exitfn = fn;
	return 0;
}

/* Exercise production flushes with a byte-exact output sink. */
static long
outputwrite(int fd, void *data, long n)
{
	assert(fd == 17 && n > 0 && used+n <= sizeof(output));
	memmove(output+used, data, n);
	used += n;
	flushwrites++;
	return n;
}

/* Return a single byte per refill to test the smallest supported buffer. */
static long
inputread(int fd, void *data, long n)
{
	assert(fd == 17 && n > 0);
	*(uchar*)data = 0x71;
	return 1;
}

/* Suppress expected invalid-mode diagnostics in this isolated driver. */
int
fprint(int fd, char *fmt, ...)
{
	(void)fd;
	(void)fmt;
	return 0;
}

#define open inputopen
#define create outputcreate
#define close inputclose
#define malloc bufferalloc
#define free bufferfree
#define atexit registerexit
#define write outputwrite
#define read inputread
IMPLEMENTATION
#undef malloc
#undef free

/* A failed native close still releases storage and is visible to the caller. */
static void
closefailures(void)
{
	Biobuf *b;
	int mode, beforeclose, beforefree;

	for(mode = OREAD; mode <= OWRITE; mode++){
		b = Bopen("buffer-test", mode);
		assert(b != nil);
		beforeclose = closes;
		beforefree = releases;
		failclose = 1;
		assert(Bterm(b) == Beof);
		failclose = 0;
		assert(closes == beforeclose+1 && releases == beforefree+1);
		checks++;
	}
}

/* Reject invalid capacities/modes without touching an existing registration. */
static void
invalidstorage(void)
{
	Biobuf b, saved;
	uchar storage[64];
	int sizes[] = {INT_MIN, -1, 0, 1, Bungetsize-1, Bungetsize};
	int i, mode;

	assert(Binit(&b, 17, OWRITE) == 0);
	saved = b;
	for(mode = OREAD; mode <= OWRITE; mode++){
		for(i = 0; i < nelem(sizes); i++){
			assert(Binits(&b, 17, mode, storage, sizes[i]) == Beof);
			assert(memcmp(&b, &saved, sizeof(b)) == 0);
			assert(wbufs[0] == &b);
			checks++;
		}
		assert(Binits(&b, 17, mode, nil, sizeof(storage)) == Beof);
		assert(memcmp(&b, &saved, sizeof(b)) == 0);
		checks++;
	}
	assert(Binits(&b, 17, 3, storage, sizeof(storage)) == Beof);
	assert(memcmp(&b, &saved, sizeof(b)) == 0 && wbufs[0] == &b);
	assert(Bterm(&b) == 0);
}

/* Check every small capacity through real byte I/O, flush and reuse. */
static void
capacities(void)
{
	Biobuf b;
	uchar storage[64];
	int cap, i;

	for(cap = 1; cap <= 32; cap++){
		memset(storage, 0xa5, sizeof(storage));
		used = flushwrites = 0;
		assert(Binits(&b, 17, OWRITE, storage+1, Bungetsize+cap) == 0);
		assert(b.bsize == cap && b.ocount == -cap && wbufs[0] == &b);
		for(i = 0; i < 48; i++)
			assert(Bputc(&b, i) == 0);
		assert(Bflush(&b) == 0 && used == 48);
		for(i = 0; i < 48; i++)
			assert(output[i] == i);
		assert(Binits(&b, 17, OREAD, storage+1, Bungetsize+cap) == 0);
		for(i = 0; i < MAXBUFS; i++)
			assert(wbufs[i] != &b);
		assert(Bgetc(&b) == 0x71);
		assert(storage[0] == 0xa5 && storage[1+Bungetsize+cap] == 0xa5);
		assert(Bterm(&b) == 0);
		checks++;
	}
}

/* Exhaust mode flags and failures, checking balanced descriptor/heap ownership. */
static void
ownership(void)
{
	Biobuf *b;
	int flags, mode, failure, beforeclose, beforefree, beforealloc;

	for(mode = OREAD; mode <= OWRITE; mode++)
	for(flags = 0; flags < 8; flags++)
	for(failure = 0; failure < 3; failure++){
		int m = mode | ((flags&1) ? OCEXEC : 0) |
			((flags&2) ? ORCLOSE : 0) | ((flags&4) ? OTRUNC : 0);
		failopen = failure == 1;
		failalloc = failure == 2;
		beforeclose = closes;
		beforefree = releases;
		beforealloc = allocations;
		b = Bopen("buffer-test", m);
		assert(lastmode == (mode == OREAD ? OREAD : m));
		if(failure){
			assert(b == nil);
			assert(closes == beforeclose+(failure == 2));
			assert(allocations == beforealloc+(failure == 2));
			assert(releases == beforefree);
		}else{
			assert(b != nil && b->flag == Bmagic && b->fid == 17);
			assert(Bterm(b) == 0);
			assert(closes == beforeclose+1 && releases == beforefree+1);
		}
		checks++;
	}
	failopen = failalloc = 0;
	assert(opens == 24 && creates == 24);
}

/* No buffer remains registered after the tested ownership lifetimes end. */
int
main(int argc, char **argv)
{
	int i;

	if(argc == 1 || strcmp(argv[1], "storage") == 0)
		invalidstorage();
	if(argc == 1 || strcmp(argv[1], "capacity") == 0)
		capacities();
	if(argc == 1 || strcmp(argv[1], "ownership") == 0)
		ownership();
	closefailures();
	assert(registrations == 1 && exitfn != nil);
	for(i = 0; i < MAXBUFS; i++)
		assert(wbufs[i] == nil);
	exitfn();
	printf("libbio lifecycle: %d scenarios passed\n", checks);
	return 0;
}
