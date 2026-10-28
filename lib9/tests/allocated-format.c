#include "lib9.h"
#include "fmtdef.h"
#include <stdio.h>
#include <limits.h>

#define check(test) predicate(!!(test), __LINE__) // Keep ownership and complexity checks active with NDEBUG.

static void *owner; // The single string buffer currently owned by production formatting.
static ulong ownersize; // Allocated extent available to the moving allocator.
static int attempts, failat, invoked, mode; // Allocation failures and custom formatter controls.
static ulong cases; // Verified complete formatting scenarios.


/* Diagnose the first bad byte, leaked owner, callback or growth bound. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "allocated format failure at %d\n", line);
		abort();
	}
}


/* Retain instrumentation without unavailable LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Link the actual error formatter without VM thread state. */
void
rerrstr(char *p, uint n)
{
	if(n)
		snprintf(p, n, "fixture");
}


/* Observe owned string allocations without intercepting formatter-registry storage. */
static void*
ownedmalloc(ulong n)
{
	check(owner == nil && n > 0 && n < 1048576);
	attempts++;
	if(attempts == failat)
		return nil;
	owner = malloc(n);
	check(owner != nil);
	ownersize = n;
	memset(owner, 0xa5, n);
	return owner;
}


/* Always move successful growth, retaining the old allocation on failure. */
static void*
ownedrealloc(void *p, ulong n)
{
	void *grown;

	check(p == owner && p != nil && n > 0 && n < 1048576);
	attempts++;
	if(attempts == failat)
		return nil;
	grown = malloc(n);
	check(grown != nil && grown != owner);
	memset(grown, 0xa5, n);
	memmove(grown, owner, n < ownersize ? n : ownersize);
	free(owner);
	owner = grown;
	ownersize = n;
	return grown;
}


/* Every success or error path must release exactly the tracked current owner. */
static void
ownedfree(void *p)
{
	if(p == nil)
		return;
	check(p == owner);
	free(p);
	owner = nil;
	ownersize = 0;
}

#define malloc ownedmalloc // Track only production string ownership.
#define realloc ownedrealloc // Move every successful production growth.
#define free ownedfree // Check exactly-once production releases.
IMPLEMENTATION
#undef malloc
#undef realloc
#undef free


/* Exercise formatter failure and a late cursor one past the reserved NUL slot. */
static int
custom(Fmt *f)
{
	invoked++;
	if(mode == 1)
		return -1;
	if(mode == 2){
		f->to = (char*)f->stop+1;
		return 0;
	}
	return fmtstrcpy(f, "side");
}


/* Start an isolated owner/fault scenario. */
static void
reset(int failure)
{
	check(owner == nil);
	attempts = invoked = mode = 0;
	failat = failure;
}


/* Require a logarithmic allocation bound and byte-exact independently built output. */
static void
growth(void)
{
	char payload[65537], expected[65600], *result;
	int lengths[] = {0, 1, 15, 30, 31, 32, 33, 63, 64, 65, 255, 256,
	                 257, 511, 512, 1023, 1024, 4095, 4096, 8192, 16384, 65536};
	int values[] = {INT_MIN, -1234567, 0, 1, INT_MAX};
	int i, j, value, length, bits, size;

	for(i = 0; i < 1025+nelem(lengths); i++){
		length = i < 1025 ? i : lengths[i-1025];
		for(j = 0; j < length; j++)
			payload[j] = 'a'+j%26;
		payload[length] = 0;
		for(value = 0; value < nelem(values); value++){
			reset(0);
			size = snprintf(expected, sizeof(expected), "[%s]:%d", payload, values[value]);
			result = smprint("[%s]:%d", payload, values[value]);
			check(result == owner && result != nil && !memcmp(result, expected, size+1));
			bits = 0;
			for(j = size+1; j; j >>= 1)
				bits++;
			/* Loose geometric bound, independent of a particular factor or size sequence. */
			check(attempts <= 2*bits+4);
			ownedfree(result);
			cases++;
		}
	}
}


/* Fail each allocation boundary and verify no partial result or retained owner. */
static void
failures(void)
{
	char payload[8193], *result;
	int i, allocations;

	memset(payload, 'x', sizeof(payload)-1);
	payload[sizeof(payload)-1] = 0;
	reset(0);
	result = smprint("%s", payload);
	check(result != nil);
	allocations = attempts;
	ownedfree(result);
	for(i = 1; i <= allocations+1; i++){
		reset(i);
		result = smprint("%s", payload);
		check((result == nil) == (i <= allocations));
		if(result != nil){
			check(!memcmp(result, payload, sizeof(payload)));
			ownedfree(result);
		}
		check(owner == nil);
		cases++;
	}
	reset(0);
	check(smprint(nil) == nil && attempts == 0 && owner == nil);
	check(smprint((char*)-1) == nil && attempts == 0 && owner == nil);
	for(i = 1; i <= 2; i++){
		reset(0);
		mode = i;
		check(smprint("%Z") == nil && invoked == 1 && owner == nil);
		cases++;
	}
	check(fmtstrinit(nil) == -1);
	cases += 3;
}


/* Malformed private growth views must decline before allocation or owner mutation. */
static void
views(void)
{
	Fmt f, saved;
	uchar before[32];
	int kind, oldattempts;

	for(kind = 0; kind < 10; kind++){
		reset(0);
		check(fmtstrinit(&f) == 0 && ownersize == sizeof(before));
		switch(kind){
		case 0: f.runes = 1; break;
		case 1: f.farg = nil; break;
		case 2: f.farg = (void*)((uintptr)INT_MAX+1); break;
		case 3: f.stop = (char*)f.stop-1; break;
		case 4: f.to = (void*)((uintptr)f.start-1); break;
		case 5: f.to = (char*)f.stop+1; break;
		case 6: f.start = nil; break;
		case 7: f.stop = nil; break;
		case 8: f.to = nil; break;
		default:
			f.farg = (void*)(uintptr)INT_MAX;
			f.stop = (void*)((uintptr)f.start+INT_MAX-1);
			break;
		}
		saved = f;
		memmove(before, owner, sizeof(before));
		oldattempts = attempts;
		check(f.flush(&f) == 0 && attempts == oldattempts);
		check(!memcmp(&f, &saved, sizeof(f)) && !memcmp(owner, before, sizeof(before)));
		ownedfree(owner);
		cases++;
	}
}


/* Public termination rejects invalid views while retaining the caller's owned buffer. */
static void
flushes(void)
{
	Fmt f, saved;
	uchar before[32];
	int kind;

	for(kind = 0; kind < 7; kind++){
		reset(0);
		check(fmtstrinit(&f) == 0);
		switch(kind){
		case 0: f.runes = 1; break;
		case 1: f.start = nil; break;
		case 2: f.to = nil; break;
		case 3: f.stop = nil; break;
		case 4: f.to = (void*)((uintptr)owner-1); break;
		case 5: f.to = (char*)owner+32; break;
		default: f.start = f.to = f.stop = (void*)-1; break;
		}
		saved = f;
		memmove(before, owner, sizeof(before));
		check(fmtstrflush(&f) == nil && owner != nil);
		check(!memcmp(&f, &saved, sizeof(f)) && !memcmp(owner, before, sizeof(before)));
		ownedfree(owner);
		cases++;
	}
	check(fmtstrflush(nil) == nil);
	cases++;
}


/* Compare bounded Rune output with a fixed Unicode sequence and unchanged guards. */
static void
runes(void)
{
	Rune output[32], before[32], expected[] = {'a', 'b', 0x20ac, 0x416, '-', '7'};
	Rune *start, *end, *result;
	char *format;
	int capacity, n, i, kind;

	for(capacity = 1; capacity <= 16; capacity++){
		memset(output, 0xa5, sizeof(output));
		memmove(before, output, sizeof(output));
		n = capacity-1 < nelem(expected) ? capacity-1 : nelem(expected);
		result = runeseprint(output+4, output+4+capacity, "ab\xe2\x82\xac%s-%d", "\xd0\x96", 7);
		check(result == output+4+n);
		check(!memcmp(output+4, expected, n*sizeof(Rune)) && output[4+n] == 0);
		for(i = 0; i < 32; i++)
			if(i < 4 || i > 4+n)
				check(output[i] == before[i]);
		cases++;
	}
	for(kind = 0; kind < 9; kind++){
		memset(output, 0xa5, sizeof(output));
		memmove(before, output, sizeof(output));
		start = output+4;
		end = output+16;
		format = "%Z";
		switch(kind){
		case 0: end = start; break;
		case 1: start = nil; break;
		case 2: start = (Rune*)-1; break;
		case 3: end = output+3; break;
		case 4: start = (Rune*)((uintptr)start+1); break;
		case 5: end = (Rune*)((uintptr)end-1); break;
		case 6: format = nil; break;
		case 7: format = (char*)-1; break;
		default: end = (Rune*)-1; break;
		}
		invoked = mode = 0;
		check(runeseprint(start, end, format) == nil && invoked == 0);
		check(!memcmp(output, before, sizeof(output)));
		cases++;
	}
}


/* Isolate before/after probes without replacing the real formatter engine. */
int
main(int argc, char **argv)
{
	check(argc == 2 && fmtinstall('Z', custom) == 0);
	if(!strcmp(argv[1], "growth"))
		growth();
	else if(!strcmp(argv[1], "failures"))
		failures();
	else if(!strcmp(argv[1], "views"))
		views();
	else if(!strcmp(argv[1], "flush"))
		flushes();
	else
		runes();
	check(owner == nil);
	printf("PASS allocated format %s: %lu ownership/value/growth cases\n", argv[1], cases);
	return 0;
}
