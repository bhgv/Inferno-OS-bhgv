#include "lib9.h"
#include <bio.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>

static const uchar *source;
static int length, position, chunk, reads, attempts, failat, live, checks;
static void *owner;
/* Feed deterministic short reads without a filesystem dependency. */
static long
inputread(int fd, void *data, long n)
{
	int count;
	assert(fd == 17 && n > 0); reads++;
	count = length-position;
	if(count > n)count = n;
	if(chunk && count > chunk)count = chunk;
	memmove(data, source+position, count); position += count;
	return count;
}
/* Force each realloc to move and track the exact live owner across failures. */
static void*
outputrealloc(void *old, size_t n)
{
	static size_t capacity;
	void *p;
	assert(old == owner); attempts++;
	if(attempts == failat)return nil;
	assert(n > 0 && n < 16384);
	p = malloc(n); assert(p);
	if(old){ memmove(p, old, capacity < n ? capacity : n); free(old); }
	else live++;
	owner = p; capacity = n;
	return p;
}
/* Every returned or discarded line must release exactly one owner. */
static void
outputfree(void *p)
{
	if(p == nil)return;
	assert(p == owner && live == 1);
	free(p); owner = nil; live--;
}
/* LeakSanitizer process inspection is unavailable in the sandbox. */
int __lsan_is_turned_off(void) { return 1; }
#define read inputread
#define realloc outputrealloc
#define free outputfree
IMPLEMENTATION
#undef read
#undef realloc
#undef free

/* Initialize a real Biobuf with tiny payload and surrounding guard bytes. */
static void
setup(Biobuf *b, uchar *storage, int capacity, const uchar *data, int n, int shortread)
{
	memset(b, 0, sizeof(*b)); memset(storage, 0xa5, capacity+2);
	b->bbuf = storage+1; b->ebuf = b->bbuf+capacity; b->gbuf = b->ebuf;
	b->bsize = capacity; b->state = Bractive; b->fid = 17;
	source = data; length = n; position = reads = attempts = 0;
	chunk = shortread; failat = -1;
	assert(owner == nil && live == 0);
}

/* Compare arbitrary bytes and delimiters across refills and repeated calls. */
static void
lines(void)
{
	Biobuf b;
	uchar storage[66], data[260];
	char *result;
	int cap, step, delim, strip, i, n, cursor, end, expected;

	for(cap = 1; cap <= 32; cap++) for(step = 1; step <= 5; step++)
	for(delim = 0; delim <= 255; delim += 17) for(strip = 0; strip < 2; strip++){
		n = 259;
		for(i = 0; i < n; i++)data[i] = (uchar)(i*13+5);
		data[0] = data[63] = data[200] = delim;
		setup(&b, storage, cap, data, n, step);
		cursor = 0;
		while(cursor < n){
			end = cursor;
			while(end < n && data[end] != delim)end++;
			expected = end-cursor+(end < n && !strip);
			result = Brdstr(&b, delim, strip);
			assert(result && b.rdline == expected && !memcmp(result, data+cursor, expected));
			assert(result[expected] == 0);
			assert(storage[0] == 0xa5 && storage[cap+1] == 0xa5);
			outputfree(result); cursor = end+(end < n); checks++;
		}
		assert(Brdstr(&b, delim, strip) == nil && b.rdline == 0);
	}
}

/* Failed growth must stop reading, free accumulated data, and report no partial owner. */
static void
failures(void)
{
	Biobuf b;
	uchar storage[18], data[128];
	char *result;
	int fault, n, before, cap;

	memset(data, 'x', sizeof(data)); data[127] = '\n';
	for(cap = 1; cap <= 16; cap++) for(fault = 1; fault <= 8; fault++){
		setup(&b, storage, cap, data, sizeof(data), 3); failat = fault;
		result = Brdstr(&b, '\n', 0);
		assert(result == nil && live == 0 && b.rdline == 0 && attempts == fault);
		assert(b.icount <= 0 && b.icount >= -cap); checks++;
	}
	/* Overflow and negative metadata are rejected before realloc or source reads. */
	for(fault = 0; fault < 3; fault++){
		attempts = 0; failat = -1;
		result = outputrealloc(nil, 4); assert(result);
		n = fault == 0 ? INT_MAX : fault == 1 ? -1 : 1; before = attempts;
		result = badd(result, &n, (char*)1, fault == 2 ? -1 : 1, 0, 0);
		assert(result == nil && n == 0 && live == 0 && attempts == before); checks++;
	}
}

/* Check real buffered-line control flow with exact allocation/error ownership. */
int
main(void)
{
	lines(); failures();
	assert(live == 0 && owner == nil);
	printf("PASS Brdstr: %d delimiter/refill/allocation cases\n", checks);
	return 0;
}
