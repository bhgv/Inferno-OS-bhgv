#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "pool.h"

void cvtup(Rune*, String*);

#define OP(fn)	void fn(void)
#define B(r)	*((BYTE*)(R.r))
#define W(r)	*((WORD*)(R.r))
#define F(r)	*((REAL*)(R.r))
#define V(r)	*((LONG*)(R.r))
#define	S(r)	*((String**)(R.r))
#define	A(r)	*((Array**)(R.r))
#define	L(r)	*((List**)(R.r))
#define P(r)	*((WORD**)(R.r))
#define C(r)	*((Channel**)(R.r))
#define T(r)	*((void**)(R.r))

/* Validate representation and bounds before publishing an alignment-safe character. */
OP(indc)
{
	WORD index, result;
	int length;
	String *string;

	memmove(&index, R.m, sizeof(index));
	memmove(&string, R.s, sizeof(string));
	if(string == H || string == nil)
		error(exNilref);
	length = string->len;
	if(length == (-0x7fffffff-1))
		error(exHeap);
	if(length < 0)
		length = -length;
	if(index < 0 || index >= length)
		error(exBounds);
	if(string->len < 0)
		result = (int)string->Srune[index];
	else
		result = (uchar)string->Sascii[index];
	memmove(R.d, &result, sizeof(result));
}

/* Capture operands and validate the representation before ownership changes.
 * Publish replacement storage only after its contents are complete. */
OP(insc)
{
	WORD index, value;
	int length, wide, shared;
	uvlong capacity;
	Rune rune;
	String *old, *result;
	void *slot;

	memmove(&value, R.s, sizeof(value));
	rune = value;
	memmove(&index, R.m, sizeof(index));
	slot = R.d;
	memmove(&old, slot, sizeof(old));
	if(old == nil)
		error(exNilref);
	length = old == H ? 0 : old->len;
	if(length == (-0x7fffffff-1))
		error(exHeap);
	if(length < 0)
		length = -length;
	if(index < 0 || index > length)
		error(exBounds);

	wide = rune >= Runeself || (old != H && old->len < 0);
	shared = old != H && D2H(old)->ref > 1;
	if(old != H && !shared && (old->len < 0 || !wide) && index < old->max){
		if(wide)
			old->Srune[index] = rune;
		else
			old->Sascii[index] = rune;
		if(index == length)
			old->len = wide ? -(length+1) : length+1;
		return;
	}

	capacity = length;
	if(index == length)
		capacity = (uvlong)length+1+length/4;
	if(capacity > 0x7fffffff)
		error(exHeap);
	result = wide ? newrunes((int)capacity) : newstring((int)capacity);
	if(old != H){
		if(wide && old->len >= 0)
			cvtup(result->Srune, old);
		else if(wide)
			memmove(result->Srune, old->Srune, length*sizeof(Rune));
		else
			memmove(result->Sascii, old->Sascii, length);
	}
	if(wide)
		result->Srune[index] = rune;
	else
		result->Sascii[index] = rune;
	if(index == length)
		length++;
	result->len = wide ? -length : length;
	memmove(slot, &result, sizeof(result));
	destroy(old);
}

/* Validate the representation before negation, subtraction or allocation. */
String*
slicer(ulong start, ulong v, String *ds)
{
	String *ns;
	int l, nc;

	if(ds == nil)
		error(exNilref);
	if(ds != H && ds->len == (-0x7fffffff-1))
		error(exHeap);
	if(ds == H) {
		if(start == 0 && v == 0)
			return H;

		error(exBounds);
	}

	if(v < start || v > 0x7fffffff)
		error(exBounds);
	nc = v - start;
	if(ds->len < 0) {
		l = -ds->len;
		if(v < start || v > l)
			error(exBounds);
		if(nc == 0)
			return H;
		ns = newrunes(nc);
		memmove(ns->Srune, &ds->Srune[start], nc*sizeof(Rune));
	}
	else {
		l = ds->len;
		if(v < start || v > l)
			error(exBounds);
		if(nc == 0)
			return H;
		ns = newstring(nc);
		memmove(ns->Sascii, &ds->Sascii[start], nc);
	}

	return ns;
}

/* Capture complete VM words before narrowing validated slice bounds. */
OP(slicec)
{
	WORD start, end;
	String *old, *result;

	memmove(&start, R.s, sizeof(start));
	memmove(&end, R.m, sizeof(end));
	memmove(&old, R.d, sizeof(old));
	if(start < 0 || end < start || (uvlong)end > 0x7fffffff)
		error(exBounds);
	result = slicer((ulong)start, (ulong)end, old);
	memmove(R.d, &result, sizeof(result));
	destroy(old);
}

void
cvtup(Rune *r, String *s)
{
	uchar *bp, *ep;

	bp = (uchar*)s->Sascii;
	ep = bp + s->len;
	while(bp < ep)
		*r++ = *bp++;
}

/*
 * Concatenate strings, reusing a unique destination when requested.
 * Keep source lengths unchanged until copying finishes: s1 and s2 may alias.
 */
String*
addstring(String *s1, String *s2, int append)
{
	Rune *r;
	String *ns;
	int l, l1, l2;

	if(s1 == nil || s2 == nil)
		error(exNilref);
	if((s1 != H && s1->len == (-0x7fffffff-1)) ||
	   (s2 != H && s2->len == (-0x7fffffff-1)))
		error(exHeap);
	if(s1 == H) {
		if(s2 == H)
			return H;
		return stringdup(s2);
	}
	if(D2H(s1)->ref > 1)
		append = 0;
	if(s2 == H) {
		if(append)
			return s1;
		return stringdup(s1);
	}

	l1 = s1->len < 0 ? -s1->len : s1->len;
	l2 = s2->len < 0 ? -s2->len : s2->len;
	if(l1 > 0x7fffffff-l2)
		error(exHeap);
	l = l1+l2;
	if(append && l > 0x7fffffff-l/4)
		error(exHeap);

	if(s1->len < 0) {
		l1 = -s1->len;
		if(append && l <= s1->max)
			ns = s1;
		else {
			ns = newrunes(append? (l+l/4): l);
			memmove(ns->Srune, s1->Srune, l1*sizeof(Rune));
		}
		r = &ns->Srune[l1];
		if(s2->len < 0)
			memmove(r, s2->Srune, -s2->len*sizeof(Rune));
		else
			cvtup(r, s2);
		ns->len = -l;

		return ns;
	}

	if(s2->len < 0) {
		l2 = -s2->len;
		ns = newrunes(append? (l+l/4): l);
		ns->len = -l;
		cvtup(ns->Srune, s1);
		memmove(&ns->Srune[s1->len], s2->Srune, l2*sizeof(Rune));
		return ns;
	}

	l1 = s1->len;
	if(append && l <= s1->max)
		ns = s1;
	else {
		ns = newstring(append? (l+l/4): l);
		memmove(ns->Sascii, s1->Sascii, l1);
	}
	memmove(ns->Sascii+l1, s2->Sascii, s2->len);
	ns->len = l;

	return ns;
}

/* Capture both owners before allocation and publish before releasing the old owner. */
OP(addc)
{
	String *left, *right, *old, *result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	memmove(&old, R.d, sizeof(old));
	result = addstring(left, right, R.m == R.d);
	if(result != old){
		memmove(R.d, &result, sizeof(result));
		destroy(old);
	}
}

/* Export UTF-8 only after validating the string representation and byte bound. */
OP(cvtca)
{
	int l;
	Rune *r;
	char *p;
	String *ss;
	Array *a, *old;

	memmove(&ss, R.s, sizeof(ss));
	if(ss == H) {
		a = mem2array(nil, 0);
		goto r;
	}
	if(ss == nil)
		error(exNilref);
	if(D2H(ss)->t != &Tstring)
		error(exType);
	if(ss->len < 0) {
		if(ss->len < -(int)((0x7fffffff-sizeof(Heap)-sizeof(Array))/UTFmax))
			error(exHeap);
		l = -ss->len;
		a = mem2array(nil, runenlen(ss->Srune, l));
		p = (char*)a->data;
		r = ss->Srune;
		while(l--)
			p += runetochar(p, r++);	
		goto r;
	}
	if((ulong)ss->len > 0x7fffffff-sizeof(Heap)-sizeof(Array))
		error(exHeap);
	a = mem2array(ss->Sascii, ss->len);

r:	memmove(&old, R.d, sizeof(old));
	memmove(R.d, &a, sizeof(a));
	destroy(old);
}

/* Validate complete VM-array lengths before narrowing to the string constructor. */
OP(cvtac)
{
	Array *a;
	String *ds, *old;

	ds = H;
	memmove(&a, R.s, sizeof(a));
	if(a != H){
		if(a == nil)
			error(exNilref);
		/* A compiler may emit its own pointer-free byte descriptor. */
        if(D2H(a)->t != &Tarray ||
            (a->t != &Tbyte &&
             (a->t == nil || a->t == H || a->t->size != 1 || a->t->np != 0)))
			error(exType);
		if(a->len < 0 || (uvlong)a->len > 0x7fffffff)
			error(exHeap);
		ds = c2string((char*)a->data, (int)a->len);
	}

	memmove(&old, R.d, sizeof(old));
	memmove(R.d, &ds, sizeof(ds));
	destroy(old);
}

/* Reject invalid lengths before taking the absolute representation size. */
OP(lenc)
{
	int l;
	WORD length;
	String *ss;

	l = 0;
	memmove(&ss, R.s, sizeof(ss));
	if(ss == nil)
		error(exNilref);
	if(ss != H) {
		l = ss->len;
		if(l == (-0x7fffffff-1))
			error(exHeap);
		if(l < 0)
			l = -l;
	}
	length = l;
	memmove(R.d, &length, sizeof(length));
}

/* Parse through the validated UTF-8 view and store through an unaligned slot. */
OP(cvtcw)
{
	String *s;
	WORD value;

	memmove(&s, R.s, sizeof(s));
	value = strtol(string2c(s), nil, 10);
	memmove(R.d, &value, sizeof(value));
}

/* Capture the owner before parsing, including source/destination aliases. */
OP(cvtcf)
{
	String *s;
	REAL value;

	memmove(&s, R.s, sizeof(s));
	value = strtod(string2c(s), nil);
	memmove(R.d, &value, sizeof(value));
}

/* Format the complete VM word, including its sign and terminating NUL. */
OP(cvtwc)
{
	String *ds, *old;
	WORD value;

	memmove(&value, R.s, sizeof(value));
	ds = newstring(24);
	ds->len = sprint(ds->Sascii, "%lld", (vlong)value);

	memmove(&old, R.d, sizeof(old));
	memmove(R.d, &ds, sizeof(ds));
	destroy(old);
}

/*
 * Format every signed 64-bit value, including its sign and terminating NUL.
 */
OP(cvtlc)
{
	String *ds, *old;
	LONG value;

	memmove(&value, R.s, sizeof(value));
	ds = newstring(24);
	ds->len = sprint(ds->Sascii, "%lld", value);
	
	memmove(&old, R.d, sizeof(old));
	memmove(R.d, &ds, sizeof(ds));
	destroy(old);
}

/* Capture the real value before allocating or replacing its destination owner. */
OP(cvtfc)
{
	String *ds, *old;
	REAL value;

	memmove(&value, R.s, sizeof(value));
	ds = newstring(32);
	ds->len = sprint(ds->Sascii, "%g", value);
	memmove(&old, R.d, sizeof(old));
	memmove(R.d, &ds, sizeof(ds));
	destroy(old);
}

/*
 * Return a NUL-terminated UTF-8 view, caching rune conversion in the string.
 * Bound the temporary byte count before multiplying the rune count.
 */
char*
string2c(String *s)
{
	char *p, *buffer;
	int l, nc;
	Rune c, *r, *er;

	if(s == nil)
		error(exNilref);
	if(s == H)
		return "";

	if(s->len >= 0) {
		s->Sascii[s->len] = '\0';
		return s->Sascii;
	}

	if(s->len < -(0x7fffffff-UTFmax)/UTFmax)
		error(exNomem);
	nc = -s->len;
	l = (nc * UTFmax) + UTFmax;
	if(s->tmp == nil || msize(s->tmp) < l) {
		/* Retain the old cache until its replacement allocation succeeds. */
		buffer = malloc(l);
		if(buffer == nil)
			error(exNomem);
		free(s->tmp);
		s->tmp = buffer;
	}

	p = s->tmp;
	r = s->Srune;
	er = r + nc;
	while(r < er) {
		c = *r++;
		if(c < Runeself)
			*p++ = c;
		else
			p += runetochar(p, r-1);
	}

	*p = 0;

	return s->tmp;
}

/* Reject invalid byte spans before scanning; retain truncated-tail behavior. */
String*
c2string(char *cs, int len)
{
	uchar *p;
	char *ecs;
	String *s;
	Rune *r, junk;
	int c, nc, isrune;

	if(len < 0)
		error(exNegsize);
	if((ulong)len > 0x7fffffff-sizeof(Heap)-sizeof(String))
		error(exHeap);
	if(len == 0)
		return newstring(0);
	if(cs == nil)
		error(exNilref);
	isrune = 0;
	ecs = cs+len;
	p = (uchar*)cs;
	while(len--) {
		c = *p++;
		if(c >= Runeself) {
			isrune = 1;
			break;
		}
	}

	if(isrune == 0) {
		nc = ecs - cs;
		s = newstring(nc);
		memmove(s->Sascii, cs, nc);
		return s;
	}

	p--;
	nc = p - (uchar*)cs;
	while(p < (uchar*)ecs) {
		c = *p;
		if(c < Runeself)
			p++;
		else if((uchar*)ecs-p >= UTFmax || fullrune((char*)p, (uchar*)ecs-p))
			p += chartorune(&junk, (char*)p);
		else
			break;
		nc++;
	}
	s = newrunes(nc);
	r = s->Srune;
	while(nc--)
		cs += chartorune(r++, cs);

	return s;
}

/*
 * Allocate an ASCII string with room for its terminator.
 * Validate the payload and heap headers before narrowing the allocation size.
 */
String*
newstring(int nb)
{
	Heap *h;
	String *s;

	if(nb < 0)
		error(exNegsize);
	if((ulong)nb > 0x7fffffff-sizeof(Heap)-sizeof(String))
		error(exHeap);
	h = nheap(sizeof(String)+nb);
	h->t = &Tstring;
	Tstring.ref++;
	s = H2D(String*, h);
	s->tmp = nil;
	s->len = nb;
	s->max = hmsize(h) - (sizeof(String)+sizeof(Heap));
	return s;
}

/*
 * Allocate rune storage within the same heap limit as ASCII strings.
 * Preserve the historical negative-count convention without negating INT_MIN.
 */
String*
newrunes(int nr)
{
	Heap *h;
	String *s;

	if(nr == 0)
		return newstring(nr);
	if(nr == (-0x7fffffff-1))
		error(exHeap);
	if(nr < 0)
		nr = -nr;
	if((ulong)nr > (0x7fffffff-sizeof(Heap)-sizeof(String))/sizeof(Rune))
		error(exHeap);
	h = nheap(sizeof(String)+nr*sizeof(Rune));
	h->t = &Tstring;
	Tstring.ref++;
	s = H2D(String*, h);
	s->tmp = nil;
	s->len = -nr;
	s->max = (hmsize(h) - (sizeof(String)+sizeof(Heap)))/sizeof(Rune);
	return s;
}

/* Reject invalid owners and INT_MIN before duplicating either representation. */
String*
stringdup(String *s)
{
	String *ns;

	if(s == nil)
		error(exNilref);
	if(s == H)
		return H;
	if(s->len == (-0x7fffffff-1))
		error(exHeap);

	if(s->len >= 0) {
		ns = newstring(s->len);
		memmove(ns->Sascii, s->Sascii, s->len);
		return ns;
	}

	ns = newrunes(-s->len);
	memmove(ns->Srune, s->Srune,-s->len*sizeof(Rune));

	return ns;
}

/* Compare representation lengths and unsigned byte values before walking payloads. */
int
stringcmp(String *s1, String *s2)
{
	Rune *r1, *r2;
	uchar *a1, *a2;
	int v, n, n1, n2, c1, c2;
	static String snil = { 0, 0, nil };

	if(s1 == nil || s2 == nil)
		error(exNilref);
	if((s1 != H && s1->len == (-0x7fffffff-1)) ||
	   (s2 != H && s2->len == (-0x7fffffff-1)))
		error(exHeap);
	if(s1 == H)
		s1 = &snil;
	if(s2 == H)
		s2 = &snil;

	if(s1 == s2)
		return 0;

	v = 0;
	n1 = s1->len;
	if(n1 < 0) {
		n1 = -n1;
		v |= 1;
	}
	n2 = s2->len;
	if(n2 < 0) {
		n2 = -n2;
		v |= 2;
	}

	n = n1;
	if(n2 < n)
		n = n2;

	switch(v) {
	case 0:		/* Ascii Ascii */
		n = memcmp(s1->Sascii, s2->Sascii, n);
		if(n == 0)
			n = n1 - n2;
		return n;
	case 1:		/* Rune Ascii */
		r1 = s1->Srune;
		a2 = (uchar*)s2->Sascii;
		while(n > 0) {
			c1 = *r1++;
			c2 = *a2++;
			if(c1 != c2)
				goto ne;
			n--;
		}
		break;
	case 2:		/* Ascii Rune */
		a1 = (uchar*)s1->Sascii;
		r2 = s2->Srune;
		while(n > 0) {
			c1 = *a1++;
			c2 = *r2++;
			if(c1 != c2)
				goto ne;
			n--;
		}
		break;
	case 3:		/* Rune Rune */
		r1 = s1->Srune;
		r2 = s2->Srune;
		while(n > 0) {
			c1 = *r1++;
			c2 = *r2++;
			if(c1 != c2)
				goto ne;
			n--;
		}
		break;
	}
	return n1 - n2;

ne:	if(c1 < c2)
		return -1;
	return 1;
}

/* Duplicate before publication, including an empty H owner and unaligned slots. */
String*
splitc(String **s, int expand)
{
	String *ss, *ns;

	memmove(&ss, s, sizeof(ss));
	if(ss == nil)
		error(exNilref);
	if(expand && ss != H && ss->len > 0) {
		ns = newrunes(ss->len);
		cvtup(ns->Srune, ss);
	}
	else
		ns = stringdup(ss);

	memmove(s, &ns, sizeof(ns));
	destroy(ss);
	return ns;
}
