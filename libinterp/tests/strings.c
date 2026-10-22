#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <setjmp.h>
/* LeakSanitizer cannot inspect processes in this sandbox; count blocks below. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

static jmp_buf boundary;
static char *raised;
char exBounds[] = "bounds";
char exHeap[] = "heap";
char exNegsize[] = "negative";
char exNomem[] = "memory";
REG R;
Type Tstring = {1};
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
	Tstring.ref--;
	free(h);
	live--;
}

/* cvtlc uses only standard decimal formatting. */
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

/* The overflow test rejects UTF conversion before consulting a cache. */
ulong
msize(void *p)
{
	(void)p;
	return 0;
}

/* No tested path should encode a rune after the size guard has failed. */
int
runetochar(char *p, Rune *r)
{
	(void)p;
	(void)r;
	abort();
	return 0;
}
#define OP(fn) void fn(void)
#define W(r) *((WORD*)(R.r))
#define V(r) *((LONG*)(R.r))
#define S(r) *((String**)(R.r))
IMPLEMENTATION
/* Errors must be raised before reaching the allocator. */
#define FAIL(expr, why) do { \
	int before = calls; \
	raised = 0; \
	if(setjmp(boundary) == 0) { \
		expr; \
		assert(0); \
	} \
	assert(raised == why); \
	assert(calls == before); \
} while(0)

/* Exercise aliases, failure ownership, size limits and valid operations. */
int
main(void)
{
	String *s, *t, *d;
	struct {
		Heap h;
		String s;
	} forged;
	WORD index, rune;
	LONG value;
	int i;

	/* Self append, with and without capacity, both representations. */
	for(i = 0; i < 4; i++) {
		s = i&1 ? newrunes(i&2 ? 2 : 4) : newstring(i&2 ? 2 : 4);
		s->len = i&1 ? -2 : 2;
		if(i&1) {
			s->Srune[0] = 0x400;
			s->Srune[1] = 0x401;
		} else {
			memmove(s->Sascii, "ab", 2);
			s->Sascii[4] = 0x5a;
			s->Sascii[5] = 0x5a;
		}
		t = addstring(s, s, 1);
		assert(t->len == (i&1 ? -4 : 4));
		if(i&1)
			assert(t->Srune[2] == 0x400 && t->Srune[3] == 0x401);
		else
			assert(memcmp(t->Sascii, "abab", 4) == 0);
		if(t == s && !(i&1))
			assert(t->Sascii[4] == 0x5a && t->Sascii[5] == 0x5a);
		if(t != s)
			destroy(s);
		destroy(t);
	}

	/* Rejected stores may neither allocate nor split shared destinations. */
	rune = 'a';
	index = 1;
	d = H;
	R.s = &rune;
	R.m = &index;
	R.d = &d;
	FAIL(insc(), exBounds);
	assert(d == H);
	d = newstring(1);
	d->Sascii[0] = 'x';
	D2H(d)->ref++;
	s = d;
	index = 2;
	FAIL(insc(), exBounds);
	assert(d == s && D2H(d)->ref == 2);
	destroy(d);
	destroy(d);
	FAIL(newstring(-1), exNegsize);
	FAIL(newstring(INT_MAX), exHeap);
	FAIL(newrunes(INT_MIN), exHeap);
	FAIL(newrunes(INT_MAX), exHeap);
	t = newrunes(-3);
	assert(t->len == -3);
	destroy(t);
	memset(&forged, 0, sizeof forged);
	forged.h.ref = 1;
	forged.s.len = INT_MAX;
	s = newstring(1);
	s->Sascii[0] = 'z';
	FAIL(addstring(&forged.s, s, 0), exHeap);
	destroy(s);
	forged.s.len = -(INT_MAX/UTFmax+1);
	FAIL(string2c(&forged.s), exNomem);

	/* Signed big extrema and alias-safe destination replacement. */
	value = (-9223372036854775807LL-1);
	d = H;
	R.s = &value;
	R.d = &d;
	cvtlc();
	assert(d->len == 20 && !memcmp(d->Sascii, "-9223372036854775808", 20));
	destroy(d);
	value = 9223372036854775807LL;
	d = H;
	cvtlc();
	assert(d->len == 19 && !memcmp(d->Sascii, "9223372036854775807", 19));
	destroy(d);

	/* Normal append and ASCII-to-rune store. */
	d = newstring(1);
	d->Sascii[0] = 'a';
	index = 1;
	rune = 0x400;
	R.s = &rune;
	R.m = &index;
	R.d = &d;
	insc();
	assert(d->len == -2 && d->Srune[0]=='a' && d->Srune[1]==0x400);
	destroy(d);
	d = H;
	index = 0;
	rune = 'a';
	insc();
	assert(d->len == 1 && d->Sascii[0]=='a');
	destroy(d);
	d = H;
	rune = 0x400;
	insc();
	assert(d->len == -1 && d->Srune[0]==0x400);
	destroy(d);
	assert(live == 0 && Tstring.ref == 1);
	puts("string allocation, aliasing, bounds and big conversion: PASS");
	return 0;
}
