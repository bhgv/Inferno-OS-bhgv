#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <pthread.h>
#include <stdio.h>
#include <limits.h>
#include <setjmp.h>
#include <stddef.h>
#include "dis-rune.h"
/* Keep every ownership and payload predicate active with NDEBUG. */
#define viewcheck(test) do { if(!(test)) { fprintf(stderr, "view check line %d\n", __LINE__); abort(); } } while(0) // Active predicate.


/* LeakSanitizer cannot inspect processes in this sandbox; count blocks below. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

static jmp_buf boundary;
static char *raised;
char exNilref[] = "nil";
char exType[] = "type";
char exBounds[] = "bounds";
char exHeap[] = "heap";
char exNegsize[] = "negative";
char exNomem[] = "memory";
REG R;
Type Tstring = {1}, Tarray = {1}, Tbyte = {1};
static int failure = -1, checks;
static int calls, live;
static struct {
	Heap *h; // Allocated owner.
	int n; // Exact payload extent.
} blocks[100];


/* Deliver VM errors to the active test without aborting the process. */
void
error(char *s)
{
	raised = s;
	longjmp(boundary, 1);
}


/* Bound allocations and place a guard immediately after each payload. */
Heap*
nheap(int n)
{
	Heap *h;
	int i;

	viewcheck(n >= 0);
	if(failure == 0)
		error(exNomem);
	if(failure > 0)
		failure--;
	calls++;
	if(n > 4096)
		error(exHeap);
	h = calloc(1, sizeof(Heap)+n+32);
	viewcheck(h);
	h->ref = 1;
	for(i = 0; i < 100; i++)
		if(!blocks[i].h)
			break;
	viewcheck(i < 100);
	blocks[i].h = h;
	blocks[i].n = n;
	memset((char*)(h+1)+n, 0xa5, 32);
	live++;
	return h;
}


/* Report exact allocation sizes so capacity checks cannot hide overruns. */
int
hmsize(void *h)
{
	int i;

	for(i = 0; i < 100; i++)
		if(blocks[i].h == h)
			return sizeof(Heap)+blocks[i].n;
	abort();
	return 0;
}


/* Check reference ownership and every guard before releasing a string. */
void
destroy(void *v)
{
	Heap *h;
	int i, j, n;

	if(v == H)
		return;
	h = D2H(v);
	viewcheck(h->ref > 0);
	if(--h->ref)
		return;
	for(i = 0; i < 100; i++)
		if(blocks[i].h == h)
			break;
	viewcheck(i < 100);
	n = blocks[i].n;
	for(j = 0; j < 32; j++)
		viewcheck(((uchar*)(h+1))[n+j] == 0xa5);
	blocks[i].h = 0;
	if(h->t == &Tstring)
		free(((String*)v)->tmp);
	else {
		viewcheck(h->t == &Tarray && ((Array*)v)->t == &Tbyte);
		Tbyte.ref--;
	}
	h->t->ref--;
	free(h);
	live--;
}


/* Exercise the VM word formatter with standard signed decimal formatting. */
int
sprint(char *p, char *fmt, ...)
{
	int n;
	va_list ap;

	va_start(ap, fmt);
	n = vsprintf(p, fmt, ap);
	va_end(ap);
	return n;
}


/* Force cache refreshes so repeated conversions exercise native buffer replacement. */
ulong
msize(void *p)
{
	(void)p;
	return 0;
}

static int cachefailure;


/* Fail native cache allocations independently of VM heap allocations. */
static void*
cachemalloc(ulong size)
{
	if(cachefailure)
		return nil;
	return malloc(size);
}

#define malloc cachemalloc // Inject native cache allocation failure.
#define OP(name) void name(void) // Extracted production operator declaration.
IMPLEMENTATION
#undef malloc


/* Catch VM errors without depending on automatic variables across longjmp. */
#define viewerror(expr, expected) do { raised = nil; if(setjmp(boundary) == 0) { expr; viewcheck(0); } viewcheck(raised == (expected)); checks++; } while(0) // Checked VM rejection.

static union {
	void *alignment; // Natural Heap alignment.
	uchar data[512]; // Packed borrowed views and malformed owner images.
} storage;
static String *view;
static char *previous;
static int shift, count, wide, mode;


/* Build a borrowed view without ever dereferencing its packed fields. */
static void
prepareview(int offset, int length, int capacity)
{
	char *cache;

	memset(storage.data, 0xa5, sizeof(storage.data));
	view = (String*)(storage.data+sizeof(Heap)+offset);
	cache = nil;
	memmove((uchar*)view+offsetof(String, len), &length, sizeof(length));
	memmove((uchar*)view+offsetof(String, max), &capacity, sizeof(capacity));
	memmove((uchar*)view+offsetof(String, tmp), &cache, sizeof(cache));
}


/* Compare every byte shift, representation and cache retry with a UTF oracle. */
static void
checkviews(void)
{
	uchar expected[256], *payload;
	Rune rune;
	char *result, *cache;
	int i, bytes;

	for(shift = 0; shift < 16; shift++)
	for(count = 0; count < 65; count++)
	for(wide = 0; wide < 2; wide++) {
		prepareview(shift, wide ? -count : count, count);
		payload = (uchar*)view+offsetof(String, data);
		bytes = 0;
		for(i = 0; i < count; i++) {
			if(wide) {
				rune = i%2 ? 0x20ac : 'A';
				memmove(payload+i*sizeof(Rune), &rune, sizeof(rune));
				if(i%2) {
					expected[bytes++] = 0xe2;
					expected[bytes++] = 0x82;
					expected[bytes++] = 0xac;
				} else
					expected[bytes++] = 'A';
			} else
				expected[bytes++] = payload[i] = 'a'+i;
		}
		expected[bytes] = 0;
		result = string2c(view);
		viewcheck(memcmp(result, expected, bytes+1) == 0);
		if(wide && count) {
			previous = result;
			cachefailure = 1;
			viewerror(string2c(view), exNomem);
			memmove(&cache, (uchar*)view+offsetof(String, tmp), sizeof(cache));
			viewcheck(cache == previous && memcmp(cache, expected, bytes+1) == 0);
			cachefailure = 0;
			result = string2c(view);
			viewcheck(memcmp(result, expected, bytes+1) == 0);
			free(result);
		}
		checks++;
	}
}


/* Reject malformed geometry and packed owners before allocation or payload reads. */
static void
checkrejections(void)
{
	ulong ref;
	String *copy;
	int allocations;

	for(shift = 0; getenv("STRING_VIEW_OWNERS") == nil && shift < 16; shift++)
	for(mode = 0; mode < 5; mode++) {
		prepareview(shift, mode == 0 ? 4 : mode == 1 ? -4 : mode == 2 ? INT_MIN : 0,
			mode < 2 ? 3 : mode == 2 ? INT_MAX : -1);
		viewerror(string2c(view), mode == 2 ? exNomem : exHeap);
		viewcheck(storage.data[sizeof(Heap)+shift+offsetof(String, data)] == 0xa5);
	}
	for(shift = 0; shift < 16; shift++)
	for(wide = 0; wide < 2; wide++)
	for(count = 0; count < 65; count++) {
		prepareview(shift, wide ? -count : count, count);
		ref = 1;
		memmove((uchar*)view-sizeof(Heap)+offsetof(Heap, ref), &ref, sizeof(ref));
		allocations = calls;
		if(shift%sizeof(void*)) {
			viewerror(stringdup(view), exHeap);
			viewcheck(calls == allocations);
		} else {
			copy = stringdup(view);
			viewcheck(copy->len == (wide ? -count : count));
			viewcheck(memcmp(copy->Sascii, (uchar*)view+offsetof(String, data), count*(wide ? sizeof(Rune) : 1)) == 0);
			destroy(copy);
			checks++;
		}
	}
	viewerror(string2c(nil), exNilref);
	viewerror(stringdup(nil), exNilref);
	viewcheck(stringdup(H) == H && strcmp(string2c(H), "") == 0);
}


/* Compare packed headers and Rune prefixes using independent signed keys. */
static void
checkcomparisons(void)
{
	union {
		void *alignment; // Natural baseline alignment.
		uchar bytes[128]; // Independent second operand.
	} right;
	String *other;
	Rune keys[] = {0, 0x7f, 0x80, 0xff, 0x100, 0x20ac, 0xd800, 0x10ffff, 0x110000, 0x7fffffff, 0x80000000, 0xffffffff};
	Rune leftkey, rightkey;
	vlong signedleft, signedright;
	DisRuneView wireleft, wireright;
	unsigned char leftbytes[8], rightbytes[8];
	unsigned int encoded, decoded;
	int wireorder;
	char *cache;
	int ls, rs, lw, rw, li, ri, length, capacity, expected, actual;

	cache = nil;
	capacity = 1;
	for(ls = 0; ls < 8; ls++)
	for(rs = 0; rs < 8; rs++)
	for(lw = 0; lw < 2; lw++)
	for(rw = 0; rw < 2; rw++)
	for(li = 0; li < nelem(keys); li++)
	for(ri = 0; ri < nelem(keys); ri++) {
		prepareview(ls, lw ? -1 : 1, 1);
		other = (String*)(right.bytes+rs);
		length = rw ? -1 : 1;
		memmove((uchar*)other+offsetof(String, len), &length, sizeof(length));
		memmove((uchar*)other+offsetof(String, max), &capacity, sizeof(capacity));
		memmove((uchar*)other+offsetof(String, tmp), &cache, sizeof(cache));
		leftkey = lw ? keys[li] : (uchar)keys[li];
		rightkey = rw ? keys[ri] : (uchar)keys[ri];
		if(lw)
			memmove((uchar*)view+offsetof(String, data), &leftkey, sizeof(leftkey));
		else
			*((uchar*)view+offsetof(String, data)) = leftkey;
		if(rw)
			memmove((uchar*)other+offsetof(String, data), &rightkey, sizeof(rightkey));
		else
			*((uchar*)other+offsetof(String, data)) = rightkey;
		signedleft = leftkey;
		signedright = rightkey;
		if(signedleft > 0x7fffffffLL)
			signedleft -= 0x100000000LL;
		if(signedright > 0x7fffffffLL)
			signedright -= 0x100000000LL;
		expected = signedleft < signedright ? -1 : signedleft > signedright;
		viewcheck(disrunewrite(leftbytes, sizeof(leftbytes), &leftkey, 1, &encoded));
		viewcheck(disrunewrite(rightbytes, sizeof(rightbytes), &rightkey, 1, &encoded));
		viewcheck(disrunecheck(leftbytes, encoded, &wireleft));
		viewcheck(disrunecheck(rightbytes, encoded, &wireright));
		viewcheck(disruneat(&wireleft, 0, &decoded) && decoded == leftkey);
		viewcheck(disrunecompare(&wireleft, &wireright, &wireorder) && wireorder == expected);
		actual = stringcmp(view, other);
		viewcheck((actual < 0 ? -1 : actual > 0) == expected);
		viewcheck(stringcmp(view, view) == 0);
		checks++;
	}
}


/* Execute actual INSC/INDC before comparing high-bit Rune payloads. */
static void
checkrunecells(void)
{
	Rune keys[] = {0, 0xd800, 0x10ffff, 0x110000, 0x7fffffff, 0x80000000, 0xffffffff};
	Rune stored;
	String *owner;
	WORD index, input, output;
	int i;
	vlong signedkey;

	for(i=0; i<nelem(keys); i++){
		owner = newrunes(1);
		index = 0;
		signedkey = keys[i];
		if(signedkey > 0x7fffffffLL)
			signedkey -= 0x100000000LL;
		input = signedkey;
		memset(&R, 0, sizeof(R));
		R.s = &input;
		R.m = &index;
		R.d = &owner;
		insc();
		memmove(&stored, owner->Srune, sizeof(stored));
		viewcheck(stored == keys[i]);
		output = 123;
		R.s = &owner;
		R.d = &output;
		indc();
		viewcheck(output == input);
		viewcheck(stringcmp(owner, owner) == 0);
		destroy(owner);
		checks++;
	}
}


/* Run on a registered pthread stack so ASan tracks VM longjmp correctly. */
static void*
viewworker(void *unused)
{
	(void)unused;
	if(getenv("STRING_VIEW_COMPARE_ONLY")) {
		checkcomparisons();
		return nil;
	}
	if(getenv("STRING_VIEW_OWNERS") == nil)
		checkviews();
	if(getenv("STRING_VIEW_OWNERS") == nil)
		checkcomparisons();
	checkrunecells();
	checkrejections();
	viewcheck(live == 0 && Tstring.ref == 1);
	printf("PASS string views: %d checks, WORD%zu\n", checks, sizeof(WORD)*8);
	return nil;
}


/* Join the only VM worker before returning the fixture status. */
int
main(void)
{
	pthread_t thread;

	if(pthread_create(&thread, nil, viewworker, nil) != 0 || pthread_join(thread, nil) != 0)
		return 1;
	return 0;
}
