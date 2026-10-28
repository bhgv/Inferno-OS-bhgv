#include "lib9.h"
#include <bio.h>
#include "fmtdef.h"
#include <stdio.h>
#include <limits.h>

#define check(test) predicate(!!(test), __LINE__) // Retain image and callback checks under NDEBUG.

static uchar storage[128], output[1024]; // Guarded stream storage and accepted native output.
static int outsize, step, failure, writes, invoked, mode; // Native/formatter fault selection.
static int reportoversize; // Inject an impossible native result without accepting bytes.
static int currentcapacity; // Maximum legal native write request.
static ulong cases; // Verified complete stream/state scenarios.


/* Stop on the first value, ownership or complete-image mismatch. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "Bio print failure at %d\n", line);
		abort();
	}
}


/* Retain sanitizers without unavailable process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Satisfy the real formatter's error verb without depending on VM thread state. */
void
rerrstr(char *p, uint n)
{
	if(n)
		snprintf(p, n, "fixture");
}


/* Accept deterministic short prefixes, then fail without accepting another byte. */
static long
outputwrite(int fd, void *p, long count)
{
	int n;

	check(fd == 17 && count > 0 && count <= currentcapacity);
	writes++;
	if(reportoversize)
		return reportoversize == 1 ? count+1 : (long)0x100000001ULL;
	if(failure >= 0 && outsize >= failure)
		return -1;
	n = count > step ? step : count;
	if(failure >= 0 && n > failure-outsize)
		n = failure-outsize;
	check(n > 0 && outsize+n <= sizeof(output));
	memmove(output+outsize, p, n);
	outsize += n;
	return n;
}

#define write outputwrite // Observe production flush writes without using real file descriptors.
IMPLEMENTATION
#undef write


/* Make custom formatter dispatch and late state/cursor changes observable. */
static int
custom(Fmt *f)
{
	Biobuf *bp;

	invoked++;
	bp = f->farg;
	switch(mode){
	case 1: bp->state = Bractive; return 0;
	case 2: f->to = (void*)((uintptr)bp->bbuf-1); return 0;
	case 3: f->stop = bp->ebuf-1; return 0;
	case 4: bp->ocount = INT_MAX; return 0;
	default: return fmtstrcpy(f, "custom");
	}
}


/* Initialize a writable buffer with an independently known pending prefix. */
static void
setup(Biobuf *b, int capacity, int pending)
{
	int i;

	memset(b, 0, sizeof(*b));
	memset(storage, 0xa5, sizeof(storage));
	memset(output, 0x5a, sizeof(output));
	b->state = Bwactive;
	b->fid = 17;
	b->bsize = capacity;
	b->bbuf = storage+16;
	b->ebuf = b->bbuf+capacity;
	b->ocount = pending-capacity;
	b->offset = 1000;
	for(i = 0; i < pending; i++)
		b->bbuf[i] = 'A'+i%26;
	currentcapacity = capacity;
	outsize = writes = invoked = mode = reportoversize = 0;
	step = 7;
	failure = -1;
}


/* Reject read/inactive buffers before any custom formatter or output mutation. */
static void
states(void)
{
	Biobuf b, saved;
	uchar before[sizeof(storage)];
	int state, pending;

	for(state = Binactive; state <= Bend; state++)
	for(pending = 0; pending <= 16; pending++){
		if(state == Bwactive)
			continue;
		setup(&b, 16, pending);
		b.state = state;
		saved = b;
		memmove(before, storage, sizeof(storage));
		check(Bprint(&b, "%Z") == Beof);
		check(!invoked && !writes && !memcmp(before, storage, sizeof(storage)));
		check(!memcmp(&b, &saved, sizeof(b)));
		cases++;
	}
}


/* Match complete accepted/pending byte streams across short writes and failures. */
static void
streams(void)
{
	Biobuf b;
	uchar expected[256];
	char payload[128];
	int capacity, pending, length, fragment, fault, total, flushed, result, retained, i;
	int faults[7];

	for(capacity = 1; capacity <= 32; capacity++)
	for(pending = 0; pending <= capacity; pending++)
	for(length = 0; length <= 96; length += 8)
	for(fragment = 1; fragment <= 7; fragment += 3){
		total = pending+length;
		flushed = total > 0 ? ((total-1)/capacity)*capacity : 0;
		faults[0] = -1; faults[1] = 0; faults[2] = 1;
		faults[3] = capacity-1; faults[4] = capacity;
		faults[5] = capacity+1; faults[6] = total;
		for(fault = 0; fault < nelem(faults); fault++){
			setup(&b, capacity, pending);
			step = fragment;
			failure = faults[fault];
			memmove(expected, b.bbuf, pending);
			for(i = 0; i < length; i++)
				payload[i] = 'a'+i%26;
			payload[length] = 0;
			memmove(expected+pending, payload, length);
			result = Bprint(&b, "%s", payload);
			if(failure >= 0 && failure < flushed){
				check(result == Beof && b.state == Binactive && outsize == failure);
				retained = capacity-outsize%capacity;
				check(b.ocount == retained-capacity);
			}else{
				check(result == length && b.state == Bwactive && outsize == flushed);
				retained = total-outsize;
				check(b.ocount == retained-capacity);
			}
			check(!memcmp(output, expected, outsize));
			check(!memcmp(b.bbuf, expected+outsize, retained));
			check(b.offset == 1000+outsize);
			for(i = 0; i < 16; i++)
				check(storage[i] == 0xa5 && storage[16+capacity+i] == 0xa5);
			cases++;
		}
	}
}


/* Malformed geometry and nil format must not evaluate a formatter or form bad spans. */
static void
layouts(void)
{
	Biobuf b, saved;
	uchar before[sizeof(storage)];
	int kind;

	for(kind = 0; kind < 10; kind++){
		setup(&b, 16, 3);
		switch(kind){
		case 0: b.bsize = INT_MIN; break;
		case 1: b.ocount = INT_MIN; break;
		case 2: b.ocount = 1; break;
		case 3: b.ocount = -17; break;
		case 4: b.bbuf = nil; break;
		case 5: b.ebuf = nil; break;
		case 6: b.ebuf = (uchar*)((uintptr)b.bbuf-1); break;
		case 7: b.ebuf--; break;
		default: break;
		}
		saved = b;
		memmove(before, storage, sizeof(storage));
		check(Bprint(&b, kind == 8 ? nil : kind == 9 ? (char*)-1 : "%Z") == Beof);
		check(!invoked && !writes && !memcmp(&b, &saved, sizeof(b)));
		check(!memcmp(before, storage, sizeof(storage)));
		cases++;
	}
	check(Bprint(nil, "%Z") == Beof && !invoked);
	cases++;
}


/* Do not publish stale final cursors after a formatter changes state or geometry. */
static void
cursors(void)
{
	Biobuf b, expected;
	uchar before[sizeof(storage)];
	Fmt f;
	int selected;

	for(selected = 1; selected <= 4; selected++){
		setup(&b, 16, 3);
		mode = selected;
		expected = b;
		if(selected == 1)
			expected.state = Bractive;
		if(selected == 4)
			expected.ocount = INT_MAX;
		memmove(before, storage, sizeof(storage));
		check(Bprint(&b, "%Z") == Beof && invoked == 1 && !writes);
		check(!memcmp(&b, &expected, sizeof(b)) && !memcmp(before, storage, sizeof(storage)));
		cases++;
	}
	setup(&b, 16, 3);
	memset(&f, 0, sizeof(f));
	f.farg = &b;
	f.stop = b.ebuf;
	f.to = (void*)((uintptr)b.bbuf-1);
	expected = b;
	check(fmtBflush(&f) == 0 && !writes && !memcmp(&b, &expected, sizeof(b)));
	cases++;
	/* Both Bio and fd paths must reject before narrowing a native long result. */
	for(selected = 1; selected <= (sizeof(long) > 4 ? 2 : 1); selected++){
		setup(&b, 16, 16);
		reportoversize = selected;
		expected = b;
		expected.state = Binactive;
		memmove(before, storage, sizeof(storage));
		check(Bprint(&b, "x") == Beof && writes == 1 && outsize == 0);
		check(!memcmp(&b, &expected, sizeof(b)) && !memcmp(before, storage, sizeof(storage)));
		cases++;
	}
}


/* Call the actual descriptor formatter with ordinary varargs. */
static int
fileprint(char *format, ...)
{
	va_list args;
	int n;

	va_start(args, format);
	n = vfprint(17, format, args);
	va_end(args);
	return n;
}


/* Reject invalid initial spans before changing any formatter byte; retain empty spans. */
static void
fdinitialization(void)
{
	Fmt f, saved;
	char *buffer;
	int size, kind;

	for(size = 0; size <= 32; size++){
		memset(&f, 0xa5, sizeof(f));
		check(fmtfdinit(&f, 17, (char*)storage+16, size) == 0);
		check(f.start == storage+16 && f.to == f.start && f.stop == storage+16+size);
		check(f.nfmt == 0 && f.runes == 0);
		cases++;
	}
	for(kind = 0; kind < 5; kind++){
		buffer = (char*)storage+16;
		size = 16;
		switch(kind){
		case 0: size = -1; break;
		case 1: size = INT_MIN; break;
		case 2: buffer = nil; break;
		case 3: buffer = (char*)-1; break;
		default: buffer = (char*)((uintptr)-8); break;
		}
		memset(&f, 0xa5, sizeof(f));
		saved = f;
		check(fmtfdinit(&f, 17, buffer, size) == -1);
		check(!memcmp(&f, &saved, sizeof(f)));
		cases++;
	}
	check(fmtfdinit(nil, 17, (char*)storage, 16) == -1);
	cases++;
}


/* Malformed or oversized byte views must never reach native output. */
static void
fdflushing(void)
{
	Biobuf b;
	Fmt f, saved;
	uchar before[sizeof(storage)];
	int kind;

	for(kind = 0; kind < 8; kind++){
		if(getenv("BIO_PRINT_FDFLUSH_CASE") && strcmp(getenv("BIO_PRINT_FDFLUSH_CASE"), "geometry"))
			continue;
		setup(&b, 16, 3);
		check(fmtfdinit(&f, 17, (char*)b.bbuf, b.bsize) == 0);
		f.to = b.bbuf+3;
		switch(kind){
		case 0: f.runes = 1; break;
		case 1: f.start = nil; break;
		case 2: f.stop = nil; break;
		case 3: f.to = (void*)((uintptr)b.bbuf-1); break;
		case 4: f.to = b.ebuf+1; break;
		case 5: f.stop = (void*)((uintptr)b.bbuf-1); break;
		case 6: f.stop = (void*)((uintptr)b.bbuf+((uintptr)INT_MAX+1)); break;
		default: f.start = f.to = f.stop = (void*)-1; break;
		}
		saved = f;
		memmove(before, storage, sizeof(storage));
		check(_fmtFdFlush(&f) == 0 && !writes);
		check(!memcmp(&f, &saved, sizeof(f)) && !memcmp(before, storage, sizeof(storage)));
		cases++;
	}
	if(getenv("BIO_PRINT_FDFLUSH_CASE") && strcmp(getenv("BIO_PRINT_FDFLUSH_CASE"), "oversize"))
		return;
	setup(&b, 16, 3);
	check(fmtfdinit(&f, 17, (char*)b.bbuf, b.bsize) == 0);
	f.to = b.bbuf+3;
	saved = f;
	reportoversize = 1;
	memmove(before, storage, sizeof(storage));
	check(_fmtFdFlush(&f) == 0 && writes == 1 && outsize == 0);
	check(!memcmp(&f, &saved, sizeof(f)) && !memcmp(before, storage, sizeof(storage)));
	check(_fmtFdFlush(nil) == 0 && fmtfdflush(nil) == -1);
	cases += 2;
	if(sizeof(long) > 4){
		setup(&b, 16, 3);
		check(fmtfdinit(&f, 17, (char*)b.bbuf, b.bsize) == 0);
		f.to = b.bbuf+3;
		saved = f;
		reportoversize = 2;
		memmove(before, storage, sizeof(storage));
		check(_fmtFdFlush(&f) == 0 && writes == 1 && outsize == 0);
		check(!memcmp(&f, &saved, sizeof(f)) && !memcmp(before, storage, sizeof(storage)));
		cases++;
	}
}


/* Preserve exact vfprint output and report errors on internal or final flushes. */
static void
fdformatting(void)
{
	Biobuf unused;
	char text[601];
	int length, fragment, fault, result, i;
	int faults[] = {-1, 0, 1, 255, 256, 257, 599, 600};

	for(i = 0; i < 600; i++)
		text[i] = 'a'+i%26;
	for(length = 0; length <= 600; length += 25)
	for(fragment = 1; fragment <= 7; fragment += 3)
	for(fault = 0; fault < nelem(faults); fault++){
		setup(&unused, 16, 0);
		currentcapacity = 256;
		step = fragment;
		failure = faults[fault];
		text[length] = 0;
		result = fileprint("%s", text);
		if(failure >= 0 && failure < length)
			check(result == -1 && outsize == failure);
		else
			check(result == length && outsize == length);
		check(!memcmp(output, text, outsize));
		if(length < 600)
			text[length] = 'a'+length%26;
		cases++;
	}
	setup(&unused, 16, 0);
	check(fileprint(nil) == -1 && !writes && !invoked);
	check(fileprint((char*)-1) == -1 && !writes && !invoked);
	cases += 2;
}


/* Select isolated parent regressions while keeping the production formatter engine. */
int
main(int argc, char **argv)
{
	check(argc == 2 && fmtinstall('Z', custom) == 0);
	if(!strcmp(argv[1], "state"))
		states();
	else if(!strcmp(argv[1], "flush"))
		streams();
	else if(!strcmp(argv[1], "layout"))
		layouts();
	else if(!strcmp(argv[1], "cursor"))
		cursors();
	else if(!strcmp(argv[1], "fdinit"))
		fdinitialization();
	else if(!strcmp(argv[1], "fdflush"))
		fdflushing();
	else
		fdformatting();
	printf("PASS Bio print %s: %lu real-formatter cases\n", argv[1], cases);
	return 0;
}
