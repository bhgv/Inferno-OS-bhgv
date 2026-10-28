#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "kernel.h"
#include <limits.h>
#include <stdio.h>

#define check(test) predicate(!!(test), __LINE__) // Keep native reply and ownership checks active under NDEBUG.

static int injected, injectat, seenbad, errorcalls; // Full-width reply injection and native error observations.
static long injectedreply; // A service result retained at its actual native width.

static int syncflag, outside, opens, closes, acquired, released, unloaded, parsed, dynamiccalls;
static int failopen, failstat, failalloc, dynamic, cached, fragment, stopat, readerror, position, reads, live, checks;
static vlong filelength;
static Module oldmodule, answer;

/* Diagnose a parser, ownership, native reply or lock invariant without assert. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "module I/O failure at %d\n", line);
		abort();
	}
}


/* Keep allocator checking without unsupported LeakSanitizer thread inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Acquire native metadata storage independently of module image allocation. */
static void*
owned(ulong n)
{
	void *p;

	p = malloc(n ? n : 1);
	check(p != nil);
	live++;
	return p;
}


/* Every temporary image and stat record must be released exactly once. */
static void
ownedfree(void *p)
{
	if(p != nil)
		live--;
	free(p);
}


/* Expose byte-image allocation failure after a descriptor and stat are owned. */
void*
mallocz(ulong n, int clear)
{
	check(outside == syncflag && clear == 0);
	return failalloc ? nil : owned(n);
}


/* Record the interpreter lock boundary around native file access. */
void
release(void)
{
	check(outside == 0);
	outside = 1;
	released++;
}


/* Require reacquisition after every released file-I/O path. */
void
acquire(void)
{
	check(outside == 1);
	outside = 0;
	acquired++;
}


/* Count loader-generated errors independently of service replies. */
void
kwerrstr(char *fmt, ...)
{
	check(fmt != nil);
	errorcalls++;
}


/* Open only the selected file while observing the native lock state. */
int
kopen(char *path, int mode)
{
	check(outside == syncflag && strcmp(path, "module.dis") == 0 && mode == OREAD);
	opens++;
	return failopen ? -1 : 7;
}


/* Each acquired descriptor is closed exactly once. */
int
kclose(int fd)
{
	check(fd == 7);
	closes++;
	return 0;
}


/* Return matching or changed module identity with configurable file length. */
Dir*
kdirfstat(int fd)
{
	Dir *d;
	check(fd == 7 && outside == syncflag);
	if(failstat)
		return nil;
	d = owned(sizeof *d);
	memset(d, 0, sizeof *d);
	d->length = filelength;
	d->mtime = cached ? oldmodule.mtime : oldmodule.mtime+1;
	d->mode = dynamic ? 0111 : 0;
	return d;
}


/* Legal short reads fill a deterministic prefix; EOF/error may cut it short. */
long
kread(int fd, void *data, long n)
{
	long i, count;
	check(fd == 7 && outside == syncflag && n > 0);
	check(!seenbad);
	if(injected && reads == injectat){
		reads++;
		seenbad = 1;
		return injectedreply;
	}
	reads++;
	if(stopat >= 0 && position >= stopat)
		return readerror ? -1 : 0;
	count = n < fragment ? n : fragment;
	if(stopat >= 0 && count > stopat-position)
		count = stopat-position;
	for(i = 0; i < count; i++)((uchar*)data)[i] = (position+i)*17+3;
	position += count;
	return count;
}


/* The dynamic loader must see a live descriptor after reacquiring the VM lock. */
int
dynldable(int fd)
{
	check(fd == 7 && outside == syncflag);
	return dynamic;
}


/* Dynamic loading rewinds its still-live descriptor after reacquisition. */
vlong
kseek(int fd, vlong off, int base)
{
	check(fd == 7 && off == 0 && base == 0 && outside == 0);
	return 0;
}


/* The dynamic loader borrows the path and metadata, never the image owner. */
Module*
newdyncode(int fd, char *path, Dir *d)
{
	check(fd == 7 && path != nil && d != nil && outside == 0 && closes == 0);
	dynamiccalls++;
	return &answer;
}


/* Retain the existing cached-module replacement policy. */
void
unload(Module *m)
{
	check(m == &oldmodule && outside == 0);
	unloaded++;
}


/* Reject partial images and verify byte order after any sequence of short reads. */
Module*
parsemod(char *path, uchar *code, ulong length, Dir *d)
{
	ulong i;

	check(path != nil && d != nil && outside == 0 && closes == 1);
	check(length == filelength && position == length);
	for(i = 0; i < length; i++)
		check(code[i] == (uchar)(i*17+3));
	parsed++;
	return &answer;
}

#define free ownedfree // Track complete image and metadata ownership.
IMPLEMENTATION
#undef free

/* Reset native observations while keeping an independent cached-module identity. */
static void
reset(int sync)
{
	syncflag = sync;
	outside = 0;
	opens = closes = acquired = released = unloaded = parsed = dynamiccalls = 0;
	failopen = failstat = failalloc = dynamic = cached = position = reads = 0;
	fragment = 256;
	stopat = -1;
	readerror = 0;
	filelength = 64;
	injected = injectat = seenbad = errorcalls = 0;
	check(live == 0);
}


/* Reject oversized/aliased replies at both an empty and a partially acquired image. */
static void
nativecounts(void)
{
	long replies[] = {65, (1LL<<32)+1, -(1LL<<32)+1, LONG_MAX, LONG_MIN};
	int sync, prefix, existing, which;
	Module *result;

	check(sizeof(long) == 8);
	for(sync = 0; sync <= 1; sync++)
	for(prefix = 0; prefix < 2; prefix++)
	for(existing = 0; existing <= 1; existing++)
	for(which = 0; which < nelem(replies); which++){
		if(getenv("READMOD_IO_REPLY") && which != atoi(getenv("READMOD_IO_REPLY")))
			continue;
		reset(sync);
		fragment = 4;
		injected = 1;
		injectat = prefix;
		injectedreply = replies[which];
		result = readmod("module.dis", existing ? &oldmodule : nil, sync);
		check(result == nil && parsed == 0 && dynamiccalls == 0);
		check(reads == prefix+1 && position == prefix*4 && seenbad == 1);
		check(errorcalls == (injectedreply > 0));
		check(unloaded == existing && closes == 1 && live == 0 && outside == 0);
		check(acquired == sync && released == sync);
		checks++;
	}
}


/* Check fragmentation, truncation, native errors and module ownership separately. */
int
main(void)
{
	int sync, length, chunk, cut, errorcase, existing;
	Module *m, *result;
	oldmodule.mtime = 10;
	if(getenv("READMOD_IO_CASE")){
		nativecounts();
		printf("PASS module native I/O: %d reply/ownership/lock scenarios\n", checks);
		return 0;
	}
	for(sync = 0; sync <= 1; sync++)
		for(existing = 0; existing <= 1; existing++)
			for(length = 1; length <= 129; length += length < 10 ? 1 : 13)
				for(chunk = 1; chunk <= length+1; chunk++){
					reset(sync);
					filelength = length;
					fragment = chunk;
					m = existing ? &oldmodule : nil;
					result = readmod("module.dis", m, sync);
					check(result == &answer && parsed == 1 && reads == (length+chunk-1)/chunk);
					check(unloaded == existing && closes == 1 && live == 0 && outside == 0);
					check(acquired == sync && released == sync);
					checks++;
				}
	for(sync = 0; sync <= 1; sync++)
		for(cut = 0; cut < 64; cut++)
			for(errorcase = 0; errorcase <= 1; errorcase++){
				reset(sync);
				fragment = 7;
				stopat = cut;
				readerror = errorcase;
				check(readmod("module.dis", &oldmodule, sync) == nil);
				check(parsed == 0 && unloaded == 1 && closes == 1 && live == 0 && outside == 0);
				checks++;
			}
	for(sync = 0; sync <= 1; sync++)
		for(errorcase = 0; errorcase < 7; errorcase++){
			reset(sync);
			if(errorcase == 0)
				failopen = 1;
			if(errorcase == 1)
				failstat = 1;
			if(errorcase == 2)
				failalloc = 1;
			if(errorcase == 3)
				filelength = -1;
			if(errorcase == 4)
				filelength = 8*1024*1024;
			if(errorcase == 5)
				cached = 1;
			if(errorcase == 6)
				dynamic = 1;
			result = readmod("module.dis", &oldmodule, sync);
			check(result == (cached ? &oldmodule : dynamic ? &answer : nil));
			check(closes == (failopen ? 0 : 1) && unloaded == (cached ? 0 : 1));
			check(parsed == 0 && dynamiccalls == dynamic && live == 0 && outside == 0);
			check(acquired == sync && released == sync);
			checks++;
		}
	reset(1);
	check(readmod("$Sys", nil, 1) == nil);
	check(readmod("$Sys", &oldmodule, 1) == &oldmodule);
	check(opens == 0 && acquired == 0 && released == 0 && live == 0);
	checks++;
	nativecounts();
	printf("readmod I/O: %d checks passed\n", checks);
	return 0;
}
