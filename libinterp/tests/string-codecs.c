#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <setjmp.h>
#include <stddef.h>
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
	Heap *h;
	int n;
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

	assert(n >= 0);
	if(failure == 0)
		error(exNomem);
	if(failure > 0)
		failure--;
	calls++;
	if(n > 4096)
		error(exHeap);
	h = calloc(1, sizeof(Heap)+n+32);
	assert(h);
	h->ref = 1;
	for(i = 0; i < 100; i++)
		if(!blocks[i].h)
			break;
	assert(i < 100);
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
	assert(h->ref > 0);
	if(--h->ref)
		return;
	for(i = 0; i < 100; i++)
		if(blocks[i].h == h)
			break;
	assert(i < 100);
	n = blocks[i].n;
	for(j = 0; j < 32; j++)
		assert(((uchar*)(h+1))[n+j] == 0xa5);
	blocks[i].h = 0;
	if(h->t == &Tstring)
		free(((String*)v)->tmp);
	else {
		assert(h->t == &Tarray && ((Array*)v)->t == &Tbyte);
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

#define malloc cachemalloc
#define OP(fn) void fn(void)
#define W(r) *((WORD*)(R.r))
#define A(r) *((Array**)(R.r))
#define V(r) *((LONG*)(R.r))
#define S(r) *((String**)(R.r))
IMPLEMENTATION
#undef malloc

/* Require an expected VM error without changing the old destination. */
#define EXPECT(expr, why) do { \
	raised = 0; \
	if(setjmp(boundary) == 0) { expr; assert(0); } \
	assert(raised == (why)); checks++; \
} while(0)

/* Encode independently of the production UTF helpers for byte-exact checks. */
static int
reference(Rune r, uchar *p)
{
	if(r > 0x10ffff || (r >= 0xd800 && r <= 0xdfff)) r = 0xfffd;
	if(r < 0x80) { p[0] = r; return 1; }
	if(r < 0x800) { p[0] = 0xc0|(r>>6); p[1] = 0x80|(r&63); return 2; }
	if(r < 0x10000) {
		p[0] = 0xe0|(r>>12); p[1] = 0x80|((r>>6)&63); p[2] = 0x80|(r&63); return 3;
	}
	p[0] = 0xf0|(r>>18); p[1] = 0x80|((r>>12)&63);
	p[2] = 0x80|((r>>6)&63); p[3] = 0x80|(r&63); return 4;
}

/* Round-trip boundary and generated runes through the actual VM operations. */
static void
checkrunes(void)
{
	Rune cases[] = {0,1,0x7f,0x80,0x7ff,0x800,0xd7ff,0xd800,0xdfff,0xe000,
		0xfffd,0xffff,0x10000,0x10ffff,0x110000,0x7fffffff,0x80000000,0xffffffff};
	String *s, *decoded, *old;
	Array *bytes;
	uchar expected[4];
	WORD rune, index;
	Rune r;
	uint state = 0x55544638;
	int i, n, before;

	for(i = 0; i < 4114; i++) {
		state = state*1664525U+1013904223U;
		r = i < 18 ? cases[i] : i&1 ? state%0x110000 : state;
		n = reference(r,expected);
		/* Store also checks high-bit runes are never misclassified as ASCII. */
		s = H; rune = (WORD)r; index = 0;
		R.s = &rune; R.m = &index; R.d = &s; insc();
		assert((r < 128 && s->len == 1) || (r >= 128 && s->len == -1));
		assert(!memcmp(string2c(s),expected,n) && string2c(s)[n] == 0);
		bytes = H; R.s = &s; R.d = &bytes; cvtca();
		assert(bytes->len == n && !memcmp(bytes->data,expected,n));
		decoded = H; R.s = &bytes; R.d = &decoded; cvtac();
		assert(decoded->len == (r < 128 ? 1 : -1));
		assert(!memcmp(string2c(decoded),expected,n));
		/* Either conversion must leave an existing result on VM heap failure. */
		old = decoded; failure = 0;
		EXPECT(cvtac(),exNomem); assert(decoded == old);
		failure = -1; destroy(decoded);
		old = s; R.s = &bytes; R.d = &s; failure = 0;
		EXPECT(cvtac(),exNomem); assert(s == old);
		failure = -1;
		before = live; R.s = &s; R.d = &bytes; failure = 0;
		EXPECT(cvtca(),exNomem); assert(live == before);
		failure = -1;
		destroy(s); destroy(bytes); checks += 5;
	}
}

/* Validate wide lengths/types before poisoned data or old results are touched. */
static void
checkbounds(void)
{
	struct { Heap h; Array a; } forged;
	struct { Heap h; String s; } text;
	Array *a, *bytes, *oldbytes;
	String *s, *old;
	int mode, before;

	memset(&forged,0,sizeof(forged));
	memset(&text,0,sizeof(text));
	forged.h.t = &Tarray; forged.a.t = &Tbyte; forged.a.data = (uchar*)1;
	a = &forged.a;
	s = c2string("keep",4); old = s;
	R.s = &a; R.d = &s;
	for(mode = 0; mode < 11; mode++) {
		forged.a.len = mode == 0 ? -1 : mode == 1 ? 0x7fffffff :
			mode == 2 ? (WORD)((uvlong)1<<32) : 1;
		if(mode == 2 && sizeof(WORD) == 4) forged.a.len = -2;
		if(mode == 3) forged.a.t = &Tstring;
		if(mode == 4) forged.h.t = &Tstring;
		if(mode == 5) forged.a.data = nil;
		if(mode == 8) forged.a.data = H;
		if(mode == 9) forged.a.t = nil;
		if(mode == 10) forged.a.t = H;
		if(mode == 6) a = nil;
		if(mode == 7) { a = H; cvtac(); assert(s == H); s = c2string("keep",4); old = s; }
		else {
			before = calls;
			EXPECT(cvtac(), mode <= 2 ? exHeap : mode <= 4 || mode>=9 ? exType : exNilref);
			assert(s == old && calls == before);
		}
		forged.a.t = &Tbyte; forged.h.t = &Tarray; forged.a.data = (uchar*)1; a = &forged.a;
	}
	destroy(s);
	s = c2string(nil,0); assert(s->len == 0); destroy(s); checks++;
	EXPECT(c2string((char*)1,-1),exNegsize);
	EXPECT(c2string((char*)1,0x7fffffff),exHeap);
	EXPECT(c2string(nil,1),exNilref);
	EXPECT(c2string(H,1),exNilref);
	s = c2string(H,0); assert(s->len == 0); destroy(s); checks++;
	forged.a.data = nil; forged.a.len = 0;
	s = H; R.d = &s; cvtac(); assert(s != H && s->len == 0); destroy(s); checks++;
	text.h.t = &Tstring; text.s.len = (-0x7fffffff-1); s = &text.s;
	bytes = mem2array("keep",4); oldbytes = bytes; R.s = &s; R.d = &bytes;
	EXPECT(cvtca(),exHeap); assert(bytes == oldbytes);
	EXPECT(string2c(s),exNomem);
	text.s.len = 0x7fffffff;
	EXPECT(cvtca(),exHeap); assert(bytes == oldbytes);
	text.s.len = -1; text.h.t = &Tarray;
	EXPECT(cvtca(),exType); assert(bytes == oldbytes);
	s = nil; EXPECT(cvtca(),exNilref); assert(bytes == oldbytes);
	s = H; cvtca(); assert(bytes != H && bytes->len == 0); destroy(bytes); checks++;
	/* The nil-string result can replace an array in the same register slot. */
	a = H; R.s = R.d = &a; cvtac(); assert(a == H); checks++;
}

/* Complete representation changes before releasing an aliased source owner. */
static void
checkaliases(void)
{
	void *slot, *old;
	String *s;
	Array *a;
	int mode;

	for(mode = 0; mode < 4; mode++) {
		s = c2string("abc",3);
		slot = s; old = slot; R.s = R.d = &slot;
		if(mode&1) failure = 0;
		if(mode&1) { EXPECT(cvtca(),exNomem); assert(slot == old); }
		else {
			cvtca(); a = slot;
			assert(a->len == 3 && !memcmp(a->data,"abc",3));
		}
		failure = -1; destroy(slot); checks++;
		a = mem2array("abc",3); slot = a; old = slot;
		if(mode&1) failure = 0;
		if(mode&1) { EXPECT(cvtac(),exNomem); assert(slot == old); }
		else {
			cvtac(); s = slot;
			assert(s->len == 3 && !memcmp(s->Sascii,"abc",3));
		}
		failure = -1; destroy(slot); checks++;
	}
}

/* Exercise complete VM words without relying on the host printf integer width. */
static void
checkwords(void)
{
	WORD values[] = {0,1,-1,2147483647,(-2147483647-1),
		(WORD)((UWORD)1 << (sizeof(WORD)*8-1)),
		(WORD)(((UWORD)1 << (sizeof(WORD)*8-1))-1)};
	String *s;
	char expected[32];
	unsigned int i;

	for(i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
		s = H; R.s = &values[i]; R.d = &s; cvtwc();
		snprintf(expected,sizeof(expected),"%lld",(long long)values[i]);
		assert(s->len == (int)strlen(expected));
		assert(!memcmp(s->Sascii,expected,s->len));
		destroy(s); checks++;
	}
}

/* Check UTF truncation compatibility, embedded NULs and empty byte spans. */
static void
checktext(void)
{
	String *s;
	uchar data[] = {'A',0,'B',0xe2,0x82,0xac,0xf0,0x9f};

	s = c2string((char*)data,sizeof(data));
	assert(s->len == -4 && s->Srune[0] == 'A' && s->Srune[1] == 0);
	assert(s->Srune[2] == 'B' && s->Srune[3] == 0x20ac);
	destroy(s); checks++;
}

/* Stores allocate at most once and keep shared sources/results intact on failure. */
static void
checkstores(void)
{
	String *old;
	String *volatile slot;
	WORD index, rune;
	Rune expected, actual;
	int length, wide, shared, nonascii, fault, position, i;
	int allocations, before, needcopy, resultwide;

	for(length = 0; length <= 20; length++)
	for(wide = 0; wide < 2; wide++)
	for(shared = 0; shared < 2; shared++)
	for(nonascii = 0; nonascii < 2; nonascii++)
	for(fault = 0; fault < 3; fault++)
	for(position = 0; position <= length; position++){
		old = wide ? newrunes(length) : newstring(length);
		for(i = 0; i < length; i++){
			if(wide)old->Srune[i] = 0x400+i;
			else old->Sascii[i] = 'A'+i;
		}
		if(shared)D2H(old)->ref++;
		slot = old; index = position; rune = nonascii ? 0x1f600 : 'x';
		R.s = &rune; R.m = &index; R.d = (void*)&slot;
		resultwide = old->len < 0 || nonascii;
		needcopy = shared || (resultwide && old->len >= 0) || index >= old->max;
		allocations = calls; before = live;
		failure = fault == 2 ? -1 : fault;
		if(fault == 0 && needcopy){
			EXPECT(insc(), exNomem);
			assert(slot == old && live == before);
		}else{
			if(setjmp(boundary) != 0)assert(!"unexpected character-store error");
			insc();
			assert(calls-allocations <= 1);
			assert(slot->len == (resultwide ? -(length+(position == length)) : length+(position == length)));
			for(i = 0; i < length+(position == length); i++){
				expected = i == position ? rune : wide ? 0x400+i : 'A'+i;
				actual = resultwide ? slot->Srune[i] : (uchar)slot->Sascii[i];
				assert(actual == expected);
			}
		}
		failure = -1;
		if(shared){
			for(i = 0; i < length; i++){
				actual = wide ? old->Srune[i] : (uchar)old->Sascii[i];
				assert(actual == (wide ? 0x400+i : 'A'+i));
			}
			destroy(old);
		}
		destroy(slot); assert(live == 0); checks++;
	}
}

/* An allocated empty ASCII buffer has byte capacity, never Rune capacity. */
static void
checkemptycapacity(void)
{
	String *s;
	WORD index, rune = 0x20ac;
	int i;

	s = newstring(64); s->len = 0;
	for(i = 0; i < 40; i++){
		index = i; R.s = &rune; R.m = &index; R.d = &s;
		insc();
		assert(s->len == -(i+1));
		assert(s->max <= (hmsize(D2H(s))-sizeof(Heap)-sizeof(String))/sizeof(Rune));
		assert(s->Srune[i] == rune); checks++;
	}
	destroy(s); assert(live == 0);
}

/* Invalid read/store indices never narrow, allocate or change result ownership. */
static void
checkindices(void)
{
	String *s, *old;
	WORD index, rune = 'x', output;
	int mode, before;

	for(mode = 0; mode < 6; mode++){
		s = c2string("abc", 3); old = s;
		if(mode == 0)index = -1;
		if(mode == 1)index = sizeof(WORD)>4 ? (WORD)((uvlong)1<<32) : -1;
		if(mode == 2)index = sizeof(WORD)>4 ? (WORD)(((uvlong)1<<32)+1) : -1;
		if(mode == 3)index = 4;
		if(mode == 4)index = sizeof(WORD)>4 ? -(WORD)((uvlong)1<<32) : -1;
		if(mode == 5)index = sizeof(WORD)>4 ? (WORD)(((uvlong)1<<63)+1) : -1;
		before = calls; output = 77;
		R.s = &s; R.m = &index; R.d = &output;
		EXPECT(indc(), exBounds); assert(output == 77 && calls == before);
		R.s = &rune; R.d = &s;
		EXPECT(insc(), exBounds);
		assert(s == old && calls == before && !memcmp(s->Sascii, "abc", 3));
		destroy(s); checks++;
	}
}

/* A failed UTF cache replacement retains ownership and permits a clean retry. */
static void
checkcachefailures(void)
{
	String *s;
	char *previous;
	int i;

	for(i = 0; i < 64; i++){
		s = newrunes(2);
		s->Srune[0] = 0x20ac;
		s->Srune[1] = 'A'+i%26;
		cachefailure = 1;
		EXPECT(string2c(s), exNomem);
		assert(s->tmp == nil);
		cachefailure = 0;
		previous = string2c(s);
		cachefailure = 1;
		EXPECT(string2c(s), exNomem);
		assert(s->tmp == previous);
		assert(!memcmp(previous, "\342\202\254", 3));
		assert(previous[3] == 'A'+i%26 && previous[4] == 0);
		cachefailure = 0;
		assert(!memcmp(string2c(s), "\342\202\254", 3));
		destroy(s);
		assert(live == 0);
		checks++;
	}
}

#ifndef STRING_INDEX_CASE
#define STRING_INDEX_CASE 0
#endif

/* Read both representations with aliased, four-byte-aligned operands. */
static void
checkindexedchars(void)
{
	union { uvlong align; uchar bytes[96]; } slots;
	uchar original[64], expected[64], wantslots[96];
	Rune values[] = {0, 0x7f, 0x80, 0xff, 0x10ffff, 0x7fffffff, 0x80000000U, 0xffffffffU};
	String *s, forged = {0}, *owner;
	WORD index, result, value = 'x';
	int wide, i, alias, mode, size;

	size = offsetof(String, data)+32;
	assert(size <= sizeof(original));
	if(STRING_INDEX_CASE == 0 || STRING_INDEX_CASE == 5)
	for(wide = 0; wide < 2; wide++){
		s = wide ? newrunes(32) : newstring(32);
		s->len = wide ? -8 : 8;
		for(i = 0; i < 8; i++){
			if(wide)s->Srune[i] = values[i];
			else s->Sascii[i] = values[i];
		}
		memcpy(original, s, size);
		for(i = 0; i < 8; i++)
		for(alias = 0; alias < 4; alias++){
			memcpy(s, original, size);
			memcpy(expected, original, size);
			memset(slots.bytes, 0xa5, sizeof(slots.bytes));
			memcpy(slots.bytes+4, &s, sizeof(s));
			index = i;
			memcpy(slots.bytes+20, &index, sizeof(index));
			memcpy(wantslots, slots.bytes, sizeof(wantslots));
			result = wide ? (int)values[i] : (uchar)values[i];
			R.s = slots.bytes+4;
			R.m = slots.bytes+20;
			if(alias < 3){
				int offset = alias == 0 ? 36 : alias == 1 ? 4 : 20;
				R.d = slots.bytes+offset;
				memcpy(wantslots+offset, &result, sizeof(result));
			}else{
				R.d = s->Sascii+4;
				memcpy(expected+offsetof(String, data)+4, &result, sizeof(result));
			}
			indc();
			assert(!memcmp(s, expected, size));
			assert(!memcmp(slots.bytes, wantslots, sizeof(wantslots)));
			checks++;
		}
		memcpy(s, original, size);
		destroy(s);
	}
	for(mode = 0; mode < 4; mode++){
		if(STRING_INDEX_CASE && mode != STRING_INDEX_CASE-1)continue;
		owner = mode&1 ? &forged : nil;
		forged.len = INT_MIN;
		index = 0;
		result = 77;
		R.m = &index;
		if(mode < 2){
			R.s = &owner;
			R.d = &result;
			EXPECT(indc(), mode&1 ? exHeap : exNilref);
			assert(result == 77);
		}else{
			R.s = &value;
			R.d = &owner;
			EXPECT(insc(), mode&1 ? exHeap : exNilref);
			assert(owner == (mode&1 ? &forged : nil));
		}
		checks++;
	}
}

/* Stores must capture unaligned operands before sharing, growth or replacement. */
static void
checkindexedstores(void)
{
	union { uvlong align; uchar bytes[96]; } slots;
	uchar before[96];
	String *s, *old, *result;
	Rune initial[] = {0x400, 0x500, 0x600}, expected[4];
	WORD value, index;
	int wide, code, shared, position, i, n;

	for(wide = 0; wide < 2; wide++)
	for(code = 0; code < 3; code++)
	for(shared = 0; shared < 2; shared++)
	for(position = 0; position < 4; position++){
		s = wide ? newrunes(32) : newstring(32);
		s->len = wide ? -3 : 3;
		for(i = 0; i < 3; i++){
			expected[i] = wide ? initial[i] : 'a'+i;
			if(wide)s->Srune[i] = expected[i];
			else s->Sascii[i] = expected[i];
		}
		old = s;
		if(shared)D2H(old)->ref++;
		value = code == 0 ? 'x' : code == 1 ? 0x20ac : -1;
		index = position == 2 ? 3 : position == 3 ? 4 : position;
		memset(slots.bytes, 0xa5, sizeof(slots.bytes));
		memcpy(slots.bytes+4, &s, sizeof(s));
		memcpy(slots.bytes+20, &value, sizeof(value));
		memcpy(slots.bytes+36, &index, sizeof(index));
		memcpy(before, slots.bytes, sizeof(before));
		R.d = slots.bytes+4;
		R.s = slots.bytes+20;
		R.m = slots.bytes+36;
		if(index > 3){
			EXPECT(insc(), exBounds);
			assert(!memcmp(slots.bytes, before, sizeof(before)));
			result = old;
		}else{
			insc();
			memcpy(&result, R.d, sizeof(result));
			n = index == 3 ? 4 : 3;
			expected[index] = value;
			assert(result->len == (wide || code ? -n : n));
			for(i = 0; i < n; i++)assert((result->len < 0 ? result->Srune[i] : (uchar)result->Sascii[i]) == expected[i]);
			assert(!memcmp(slots.bytes+12, before+12, sizeof(before)-12));
		}
		if(shared){
			assert(D2H(old)->ref == (result == old ? 2 : 1));
			assert(old->len == (wide ? -3 : 3));
			destroy(old);
		}
		destroy(result);
		checks++;
	}
}

/* Invalid logical capacities must not write terminators, scan runes or refresh caches. */
static void
checkcapacities(void)
{
	String *s;
	uchar before[256],cache[9];
	int wide,mode,bytes,allocations,oldlive;
	char *selection=getenv("STRING_CAPACITY_MODE");

	for(wide=0;wide<2;wide++)for(mode=0;mode<4;mode++){
		if(selection && atoi(selection)!=mode)continue;
		s=wide ? newrunes(4) : newstring(4);
		if(wide){
			int j;
			for(j=0;j<4;j++)s->Srune[j]=0x400+j;
			string2c(s);memmove(cache,s->tmp,sizeof(cache));
		}
		bytes=sizeof(Heap)+sizeof(String)+(wide ? 4*sizeof(Rune) : 4)+32;
		assert(bytes<sizeof(before));
		if(mode==0)s->max=3;
		if(mode==1)s->max=-1;
		if(mode==2)s->len=wide ? -1000 : 1000;
		if(mode==3){s->len=0;s->max=-1;}
		memmove(before,D2H(s),bytes);allocations=calls;oldlive=live;
		EXPECT(string2c(s),exHeap);
		assert(!memcmp(before,D2H(s),bytes) && calls==allocations && live==oldlive);
		if(wide)assert(!memcmp(cache,s->tmp,sizeof(cache)));
		s->len=wide ? -4 : 4;s->max=4;destroy(s);assert(!live);checks++;
	}
}

/* Complete all real-header codec checks with balanced VM and native ownership. */
int
main(void)
{
	if(getenv("STRING_CAPACITY_ONLY")){checkcapacities();goto finished;}
	if(STRING_INDEX_CASE == 0){
	checkcapacities();checkcachefailures(); checkrunes(); checkbounds(); checkaliases(); checkwords(); checktext(); checkstores(); checkindices(); checkemptycapacity();
	}
	if(STRING_INDEX_CASE != 6)checkindexedchars();
	if(STRING_INDEX_CASE == 0 || STRING_INDEX_CASE == 6)checkindexedstores();
	finished:
	assert(live == 0 && Tstring.ref == 1 && Tarray.ref == 1 && Tbyte.ref == 1);
	printf("PASS string codecs: %d UTF/word/bounds/ownership checks, WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
