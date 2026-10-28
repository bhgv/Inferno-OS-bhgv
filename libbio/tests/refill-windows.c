#include "lib9.h"
#include <bio.h>
#include <stdio.h>
#include <limits.h>

#define check(test) predicate(!!(test), __LINE__) // Keep byte-image oracles active under NDEBUG.

static uchar input[512], storage[128]; // Deterministic source and guarded actual buffer storage.
static int inputsize, inputpos, fragment, calls, injected, prefix; // Read/write boundary controls.
static long reply; // Native return value before any public int narrowing.
static int interrupted; // Error-string selection for write failure recovery.
static ulong cases; // Verified stream or result-width scenarios.


/* Fail at the first lost byte, bad count or incorrect terminal state. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "Bio refill failure at %d\n", line);
		abort();
	}
}


/* Keep sanitizer instrumentation without unavailable LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Return bytes only for valid requests; malformed replies deliberately write nothing. */
static long
inputread(int fd, void *p, long count)
{
	int n;

	check(fd == 17 && count > 0 && count <= 32 && ++calls < 2048);
	if(injected)
		return calls == 1 ? reply : 0;
	n = inputsize-inputpos;
	if(n > fragment)
		n = fragment;
	if(n > count)
		n = count;
	memmove(p, input+inputpos, n);
	inputpos += n;
	return n;
}


/* Accept an optional prefix before supplying an invalid native write count. */
static long
outputwrite(int fd, void *p, long count)
{
	int n;

	check(fd == 17 && p != nil && count > 0 && count <= 32 && ++calls < 64);
	if(prefix > 0){
		n = prefix < count ? prefix : count;
		prefix -= n;
		return n;
	}
	return reply;
}


/* Preserve the existing interrupted-write retry branch while testing invalid replies. */
int
errstr(char *p, uint n)
{
	if(n)
		snprintf(p, n, "%s", interrupted ? "interrupted" : "I/O failure");
	return 0;
}

#define read inputread // Observe the real production refill request boundaries.
#define write outputwrite // Observe the real production output remainder boundaries.
IMPLEMENTATION
#undef read
#undef write


/* Prepare a real prefix reserve and initialize the whole window's guard image. */
static void
setup(Biobuf *b, int capacity)
{
	memset(b, 0, sizeof(*b));
	memset(storage, 0xa5, sizeof(storage));
	b->bbuf = storage+16+Bungetsize;
	b->ebuf = b->bbuf+capacity;
	b->gbuf = b->ebuf;
	b->bsize = capacity;
	b->state = Bractive;
	b->fid = 17;
	calls = inputpos = injected = prefix = interrupted = 0;
	fragment = capacity;
}


/* Compare line/string results after genuine refills and legal cross-window ungets. */
static void
windows(int strings)
{
	Biobuf b;
	uchar *view, drained[128];
	char *owned;
	int capacity, chunk, rewind, variant, consumed, start, end, n, i, guard;

	for(capacity = 1; capacity <= 16; capacity++)
	for(chunk = 1; chunk <= capacity; chunk++)
	for(rewind = 0; rewind <= chunk+Bungetsize; rewind++)
	for(variant = 0; variant < 3; variant++){
		setup(&b, capacity);
		fragment = chunk;
		inputsize = 200;
		for(i = 0; i < inputsize; i++)
			input[i] = 'a'+i%26;
		/* Drain complete native pieces so the reserved history is known and valid. */
		consumed = 0;
		do{
			check(Bgetc(&b) == input[consumed++]);
		}while(consumed < 32 || b.icount != 0);
		check(consumed == inputpos);
		for(i = 0; i < rewind; i++)
			check(Bungetc(&b) == 1);
		start = consumed-rewind;
		if(variant != 2)
			input[consumed+17] = '\n';
		if(variant == 0)
			input[consumed+41] = '\n';
		for(guard = 0; start < inputsize && guard < 512; guard++){
			end = start;
			while(end < inputsize && input[end] != '\n')
				end++;
			if(strings){
				owned = Brdstr(&b, '\n', variant&1);
				n = end-start+(end < inputsize && !(variant&1));
				check(owned != nil && Blinelen(&b) == n && !memcmp(owned, input+start, n));
				check(owned[n] == 0);
				free(owned);
				start = end+(end < inputsize);
			}else{
				view = Brdline(&b, '\n');
				n = Blinelen(&b);
				check(n > 0 && n <= inputsize-start);
				if(view != nil){
					check(n == end-start+1 && !memcmp(view, input+start, n));
				}else{
					check(n <= sizeof(drained) && Bread(&b, drained, n) == n);
					check(!memcmp(drained, input+start, n));
				}
				start += n;
			}
		}
		check(start == inputsize && guard < 512);
		for(i = 0; i < 16; i++)
			check(storage[i] == 0xa5 && storage[16+Bungetsize+capacity+i] == 0xa5);
		cases++;
	}
}


/* Rewind actual multibyte runes before switching to either production line API. */
static void
runehistory(int strings)
{
	uchar encodings[3][4] = {{0xc2, 0xa2}, {0xe2, 0x82, 0xac}, {0xf0, 0x9f, 0x98, 0x80}};
	Rune runes[] = {0xa2, 0x20ac, 0x1f600};
	int lengths[] = {2, 3, 4};
	Biobuf b;
	uchar result[32], *view;
	char *owned;
	int capacity, chunk, rune, strip, i, n, got;

	for(capacity = 1; capacity <= 8; capacity++)
	for(chunk = 1; chunk <= capacity; chunk++)
	for(rune = 0; rune < 3; rune++)
	for(strip = 0; strip < 2; strip++){
		setup(&b, capacity);
		fragment = chunk;
		memset(input, 'w', 32);
		memmove(input+32, encodings[rune], lengths[rune]);
		memmove(input+32+lengths[rune], "tail\nend", 8);
		inputsize = 40+lengths[rune];
		for(i = 0; i < 32; i++)
			check(Bgetc(&b) == 'w');
		check(Bgetrune(&b) == runes[rune] && b.runesize == lengths[rune]);
		check(Bungetrune(&b) == 1);
		n = lengths[rune]+5;
		if(strings){
			owned = Brdstr(&b, '\n', strip);
			check(owned != nil && Blinelen(&b) == n-strip);
			check(!memcmp(owned, input+32, n-strip) && owned[n-strip] == 0);
			free(owned);
		}else{
			got = 0;
			while(got < n){
				view = Brdline(&b, '\n');
				i = Blinelen(&b);
				check(i > 0 && i <= n-got);
				if(view != nil)
					memmove(result+got, view, i);
				else
					check(Bread(&b, result+got, i) == i);
				got += i;
			}
			check(!memcmp(result, input+32, n));
		}
		owned = Brdstr(&b, '\n', 0);
		check(owned != nil && Blinelen(&b) == 3 && !memcmp(owned, "end", 4));
		free(owned);
		cases++;
	}
}


/* Reject bad positive native replies before using their lengths or advancing offsets. */
static void
readcounts(char *family)
{
	Biobuf b;
	uchar destination[64], before[64];
	char *p;
	int capacity, kind, oldcalls, expectedstate;
	long responses[8];

	for(capacity = 1; capacity <= 32; capacity++){
		responses[0] = capacity+1;
		responses[1] = LONG_MAX;
		responses[2] = sizeof(long) > 4 ? (long)0x100000001ULL : capacity+2;
		responses[3] = sizeof(long) > 4 ? (long)0x100000000ULL : capacity+3;
		responses[4] = 0;
		responses[5] = -1;
		responses[6] = LONG_MIN+1;
		responses[7] = sizeof(long) > 4 ? -(long)0xffffffffULL : -2;
		for(kind = 0; kind < nelem(responses); kind++){
			if(getenv("BIO_REFILL_REPLY_INDEX") && kind != atoi(getenv("BIO_REFILL_REPLY_INDEX")))
				continue;
			setup(&b, capacity);
			injected = 1;
			reply = responses[kind];
			memset(destination, 0x5a, sizeof(destination));
			memmove(before, destination, sizeof(before));
			if(!strcmp(family, "getc"))
				check(Bgetc(&b) == Beof);
			else if(!strcmp(family, "read"))
				check(Bread(&b, destination, 3) == (reply == 0 ? 0 : Beof) && !memcmp(before, destination, sizeof(before)));
			else if(b.state == Binactive && !strcmp(family, "rdline"))
				check(Brdline(&b, '\n') == nil && Blinelen(&b) == 0);
			else{
				p = Brdstr(&b, '\n', 0);
				check(p == nil && Blinelen(&b) == 0);
			}
			expectedstate = reply > capacity ? Binactive :
			    (!strcmp(family, "getc") || !strcmp(family, "read")) ?
			    (reply == 0 ? Bracteof : Binactive) : Bractive;
			check(b.state == expectedstate && b.icount == 0 && b.offset == 0 && calls == 1);
			oldcalls = calls;
			if(b.state == Binactive && !strcmp(family, "rdline"))
				check(Brdline(&b, '\n') == nil && calls == oldcalls);
			if(b.state == Binactive && !strcmp(family, "rdstr"))
				check(Brdstr(&b, '\n', 0) == nil && calls == oldcalls);
			cases++;
		}
	}
	if(!strcmp(family, "rdline") || !strcmp(family, "rdstr")){
		for(capacity = 2; capacity <= 32; capacity++)
		for(kind = 1; kind < capacity; kind++){
			setup(&b, capacity);
			b.icount = -kind;
			b.offset = 100;
			b.gbuf = b.ebuf-kind;
			memset(b.gbuf, 'A', kind);
			injected = 1;
			reply = capacity+1;
			if(!strcmp(family, "rdline")){
				check(Brdline(&b, '\n') == nil && Blinelen(&b) == kind);
				check(b.state == Binactive && b.icount == -kind && b.offset == 100);
				check(Brdline(&b, '\n') == nil && Blinelen(&b) == kind && calls == 1);
				check(Bread(&b, destination, kind) == kind);
				memset(before, 'A', kind);
				check(!memcmp(destination, before, kind));
			}else{
				p = Brdstr(&b, '\n', 0);
				check(p != nil && Blinelen(&b) == kind && p[kind] == 0);
				memset(before, 'A', kind);
				check(!memcmp(p, before, kind));
				free(p);
				check(b.state == Binactive && b.icount == 0 && b.offset == 100);
				check(Brdstr(&b, '\n', 0) == nil && calls == 1);
			}
			cases++;
		}
	}
}


/* Invalid output replies retain exactly the unwritten suffix, even with stale interrupt text. */
static void
writecounts(void)
{
	Biobuf b;
	uchar saved[32], byte;
	int capacity, accepted, kind, interrupt, i;
	long responses[4];

	byte = 'x';
	for(capacity = 1; capacity <= 32; capacity++)
	for(accepted = 0; accepted < capacity; accepted++)
	for(interrupt = 0; interrupt < 2; interrupt++){
		responses[0] = capacity+1;
		responses[1] = LONG_MAX;
		responses[2] = sizeof(long) > 4 ? (long)0x100000001ULL : capacity+2;
		responses[3] = sizeof(long) > 4 ? (long)0x100000000ULL : capacity+3;
		for(kind = 0; kind < nelem(responses); kind++){
			if(getenv("BIO_REFILL_REPLY_INDEX") && kind != atoi(getenv("BIO_REFILL_REPLY_INDEX")))
				continue;
			setup(&b, capacity);
			b.state = Bwactive;
			b.ocount = 0;
			for(i = 0; i < capacity; i++)
				b.bbuf[i] = saved[i] = 'a'+i%26;
			prefix = accepted;
			interrupted = interrupt;
			reply = responses[kind];
			check(Bwrite(&b, &byte, 1) == Beof && b.state == Binactive);
			check(b.offset == accepted && b.ocount == -accepted);
			check(!memcmp(b.bbuf, saved+accepted, capacity-accepted));
			check(byte == 'x' && calls == (accepted ? 2 : 1));
			cases++;
		}
	}
}


/* Isolate both ordinary-data regressions and individual native reply checks. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(!strcmp(argv[1], "line")){
		windows(0);
		runehistory(0);
	}else if(!strcmp(argv[1], "string")){
		windows(1);
		runehistory(1);
	}
	else if(!strcmp(argv[1], "write"))
		writecounts();
	else
		readcounts(argv[1]);
	printf("PASS Bio refill %s: %lu byte-image/state cases\n", argv[1], cases);
	return 0;
}
