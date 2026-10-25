#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "pool.h"
#include "raise.h"
#include <kernel.h>

void	freearray(Heap*, int);
void	freelist(Heap*, int);
void	freemodlink(Heap*, int);
void	freechan(Heap*, int);
Type	Tarray = { 1, freearray, markarray, sizeof(Array) };
Type	Tstring = { 1, freestring, noptrs, sizeof(String) };
Type	Tlist = { 1, freelist, marklist, sizeof(List) };
Type	Tmodlink = { 1, freemodlink, markheap, -1, 1, 0, 0, { 0x80 } };
Type	Tchannel = { 1, freechan, markheap, sizeof(Channel), 1,0,0,{0x80} };
Type	Tptr = { 1, 0, markheap, sizeof(WORD*), 1, 0, 0, { 0x80 } };
Type	Tbyte = { 1, 0, 0, 1 };
Type Tword = { 1, 0, 0, sizeof(WORD) };
Type Tlong = { 1, 0, 0, sizeof(LONG) };
Type Treal = { 1, 0, 0, sizeof(REAL) };

extern	Pool*	heapmem;
extern	int	mutator;

void	(*heapmonitor)(int, void*, ulong);

#define	BIT(bt, nb)	(bt & (1<<nb))

/* Release mapped pointers without imposing native alignment on record bytes. */
void
freeptrs(void *v, Type *t)
{
	void *x;
	int i, bit, c, n;

	n = t->np;
	for(i = 0; i < n; i++){
		c = t->map[i];
		if(c == 0)
			continue;
		/* Preserve the historical reverse order within each map byte. */
		for(bit = 0; bit < 8; bit++){
			if(!(c & (1<<bit)))
				continue;
			memmove(&x, (uchar*)v+((ulong)i*8+7-bit)*sizeof(x), sizeof(x));
			if(x != H)
				destroy(x);
		}
	}
}

/*
void
nilptrs(void *v, Type *t)
{
	int c, i;
	WORD **w;
	uchar *p, *ep;

	w = (WORD**)v;
	p = t->map;
	ep = p + t->np;
	while(p < ep) {
		c = *p;
		for(i = 0; i < 8; i++){
			if(BIT(c, 7)) *w = H;
			c <<= 1;
			w++;
		}
		p++;
	}
}
*/

void
freechan(Heap *h, int swept)
{
	Channel *c;

	USED(swept);
	c = H2D(Channel*, h);
	if(c->mover == movtmp)
		freetype(c->mid.t);
	killcomm(&c->send);
	killcomm(&c->recv);
	if (!swept && c->buf != H)
		destroy(c->buf);
}

void
freestring(Heap *h, int swept)
{
	String *s;

	USED(swept);
	s = H2D(String*, h);
	if(s->tmp != nil)
		free(s->tmp);
}

void
freearray(Heap *h, int swept)
{
	int i;
	Type *t;
	uchar *v;
	Array *a;

	a = H2D(Array*, h);
	t = a->t;

	if(!swept) {
		if(a->root != H)
			destroy(a->root);
		else
		if(t->np != 0) {
			v = a->data;
			for(i = 0; i < a->len; i++) {
				freeptrs(v, t);
				v += t->size;
			}
		}
	}
	if(t->ref-- == 1) {
		free(t->initialize);
		free(t);
	}
}

void
freelist(Heap *h, int swept)
{
	Type *t;
	List *l;
	Heap *th;

	l = H2D(List*, h);
	t = l->t;

	if(t != nil) {
		if(!swept && t->np)
			freeptrs(l->data, t);
		t->ref--;
		if(t->ref == 0) {
			free(t->initialize);
			free(t);
		}
	}
	if(swept)
		return;
	l = l->tail;
	while(l != (List*)H) {
		t = l->t;
		th = D2H(l);
		if(th->ref-- != 1)
			break;
		th->t->ref--;	/* should be &Tlist and ref shouldn't go to 0 here nor be 0 already */
		if(t != nil) {
			if (t->np)
				freeptrs(l->data, t);
			t->ref--;
			if(t->ref == 0) {
				free(t->initialize);
				free(t);
			}
		}
		l = l->tail;
		if(heapmonitor != nil)
			heapmonitor(1, th, 0);
		poolfree(heapmem, th);
	}
}

void
freemodlink(Heap *h, int swept)
{
	Modlink *ml;

	ml = H2D(Modlink*, h);
	if(ml->m->rt == DYNMOD)
		freedyndata(ml);
	else if(!swept)
		destroy(ml->MP);
	unload(ml->m);
}

int
heapref(void *v)
{
	return D2H(v)->ref;
}

void
freeheap(Heap *h, int swept)
{
	Type *t;

	if(swept)
		return;

	t = h->t;
	if (t->np)
		freeptrs(H2D(void*, h), t);
}

void
destroy(void *v)
{
	Heap *h;
	Type *t;

	if(v == H)
		return;

	h = D2H(v);
	{ Bhdr *b; D2B(b, h); }		/* consistency check */

	if(--h->ref > 0 || gchalt > 64) 	/* Protect 'C' thread stack */
		return;

	if(heapmonitor != nil)
		heapmonitor(1, h, 0);
	t = h->t;
	if(t != nil) {
		gclock();
		t->free(h, 0);
		gcunlock();
		freetype(t);
	}
	poolfree(heapmem, h);
}

Type*
dtype(void (*destroy)(Heap*, int), int size, uchar *map, int mapsize)
{
	Type *t;
	int i, bit, slots;

	/* The active VM map advances by native pointer slots. Dis64's proposed
	 * four-byte bitmap quantum is a separate, not yet activated ABI. */
	if(size < 0 || mapsize < 0 || (mapsize != 0 && map == nil) ||
	   mapsize > 0x7fffffff-(sizeof(Type)-sizeof(t->map)))
		return nil;
	slots=size/sizeof(void*);
	if(mapsize > size/(8*sizeof(void*))+(size%(8*sizeof(void*)) != 0))
		return nil;
	for(i=0; i<mapsize; i++)
		for(bit=0; bit<8; bit++)
			if((map[i] & (0x80>>bit)) &&
			   (i*8+bit >= slots || size%sizeof(void*) != 0))
				return nil;
	t = mallocz(sizeof(Type)-sizeof(t->map)+mapsize, 1);
	if(t != nil) {
		t->ref = 1;
		t->free = destroy;
		t->mark = markheap;
		t->size = size;
		t->np = mapsize;
		if(mapsize != 0)
			memmove(t->map, map, mapsize);
	}
	return t;
}

void*
checktype(void *v, Type *t, char *name, int newref)
{
	Heap *h;

	if(v == H || v == nil)
		error(exNilref);
	h = D2H(v);
	if(t == nil || h->t != t)
		errorf("%s: %s", exType, name);
	if(newref){
		h->ref++;
		Setmark(h);
	}
	return v;
}

void
freetype(Type *t)
{
	if(t == nil || --t->ref > 0)
		return;

	free(t->initialize);
	free(t);
}

/* Retain mapped fields using byte-safe loads and the existing mark barrier. */
void
incmem(void *v, Type *t)
{
	Heap *h;
	void *x;
	int i, bit, c;

	for(i = 0; i < t->np; i++){
		c = t->map[i];
		if(c == 0)
			continue;
		for(bit = 0; bit < 8; bit++){
			if(!(c & (0x80>>bit)))
				continue;
			memmove(&x, (uchar*)v+((ulong)i*8+bit)*sizeof(x), sizeof(x));
			if(x != H){
				h = D2H(x);
				h->ref++;
				Setmark(h);
			}
		}
	}
}

/* Visit mapped pointers in their original order without aligned-pointer casts. */
void
scanptrs(void *v, Type *t, void (*f)(void*))
{
	void *x;
	int i, bit, c;

	for(i = 0; i < t->np; i++){
		c = t->map[i];
		if(c == 0)
			continue;
		for(bit = 0; bit < 8; bit++){
			if(!(c & (0x80>>bit)))
				continue;
			memmove(&x, (uchar*)v+((ulong)i*8+bit)*sizeof(x), sizeof(x));
			if(x != H)
				f(D2H(x));
		}
	}
}

/* Initialize only mapped fields, leaving scalar bytes and padding unchanged. */
void
initmem(Type *t, void *v)
{
	void *x;
	int i, bit, c;

	x = H;
	for(i = 0; i < t->np; i++){
		c = t->map[i];
		if(c == 0)
			continue;
		for(bit = 0; bit < 8; bit++){
			if(c & (1<<bit))
				memmove((uchar*)v+((ulong)i*8+7-bit)*sizeof(x), &x, sizeof(x));
		}
	}
}

/* Bound the full allocation before multiplication or narrowing to int. */
void
acheck(int tsz, WORD sz)
{
	if(sz < 0)
		error(exNegsize);
	if(tsz < 0 || (uvlong)sz > 0x7fffffff ||
	   (tsz != 0 && (uvlong)sz >
	    (0x7fffffff-sizeof(Heap)-sizeof(Array))/(ulong)tsz))
		error(exHeap);
}

Heap*
nheap(int n)
{
	Heap *h;

	if(n < 0 || (ulong)n > 0x7fffffff-sizeof(Heap))
		error(exHeap);
	h = poolalloc(heapmem, sizeof(Heap)+n);
	if(h == nil)
		error(exHeap);

	h->t = nil;
	h->ref = 1;
	h->color = mutator;
	if(heapmonitor != nil)
		heapmonitor(0, h, n);

	return h;
}


/* Reject dead or saturated descriptors before allocating and zeroing an object. */
Heap*
heapz(Type *t)
{
	Heap *h;

	if(t == nil || t == H || t->ref <= 0 || t->ref == 0x7fffffff ||
	   t->size < 0 || (ulong)t->size > 0x7fffffff-sizeof(Heap))
		error(exHeap);
	h = poolalloc(heapmem, sizeof(Heap)+t->size);
	if(h == nil)
		error(exHeap);

	h->t = t;
	t->ref++;
	h->ref = 1;
	h->color = mutator;
	memset(H2D(void*, h), 0, t->size);
	if(t->np)
		initmem(t, H2D(void*, h));
	if(heapmonitor != nil)
		heapmonitor(0, h, t->size);
	return h;
}


/* Check descriptor lifetime and size before acquiring object storage. */
Heap*
heap(Type *t)
{
	Heap *h;

	if(t == nil || t == H || t->ref <= 0 || t->ref == 0x7fffffff ||
	   t->size < 0 || (ulong)t->size > 0x7fffffff-sizeof(Heap))
		error(exHeap);
	h = poolalloc(heapmem, sizeof(Heap)+t->size);
	if(h == nil)
		error(exHeap);

	h->t = t;
	t->ref++;
	h->ref = 1;
	h->color = mutator;
	if(t->np)
		initmem(t, H2D(void*, h));
	if(heapmonitor != nil)
		heapmonitor(0, h, t->size);
	return h;
}


/* Reserve both descriptor retains before allocating an array header or payload. */
Heap*
heaparray(Type *t, int sz)
{
	Heap *h;
	Array *a;
	int i, bit;

	/* Header and element descriptors can be the same Type, requiring two retains. */
	if(t == nil || t == H || t->ref <= 0 ||
	   t->ref > 0x7ffffffe - (t == &Tarray) || Tarray.ref <= 0 || Tarray.ref == 0x7fffffff)
		error(exHeap);
	acheck(t->size,sz);
	/* Validate before initarray publishes a header requiring mapped cleanup. */
	if(t->np < 0 ||
	   t->np > t->size/(8*sizeof(void*))+(t->size%(8*sizeof(void*)) != 0))
		error(exHeap);
	for(i = 0; i < t->np; i++)
		for(bit = 0; bit < 8; bit++)
			if((t->map[i] & (0x80>>bit)) &&
			   ((ulong)i*8+bit >= (ulong)t->size/sizeof(void*)))
				error(exHeap);
	h = nheap(sizeof(Array) + (t->size*sz));
	h->t = &Tarray;
	Tarray.ref++;
	a = H2D(Array*, h);
	a->t = t;
	a->len = sz;
	a->root = H;
	a->data = (uchar*)a + sizeof(Array);
	initarray(t, a);
	return h;
}

int
hmsize(void *v)
{
	return poolmsize(heapmem, v);
}


/* Allocation callers reserve the element Type retain before acquiring storage. */
void
initarray(Type *t, Array *a)
{
	int i;
	uchar *p;

	t->ref++;
	if(t->np == 0)
		return;

	p = a->data;
	for(i = 0; i < a->len; i++) {
		initmem(t, p);
		p += t->size;
	}
}

typedef struct ArrayCopy ArrayCopy;

struct ArrayCopy {
	Array *source; // Source array stays owned by the caller throughout cloning.
	Array *result; // Partial destination released by the error protector.
};


/* Validate source spans before allocating, then clone mapped fields in every element. */
static int
copyarray(void *arg)
{
	ArrayCopy *c;
	Array *sa, *da;
	Type *t;
	ulong bytes;
	int i;

	c = arg;
	sa = c->source;
	t = sa->t;
	if(t == nil || t == H || sa->root == nil ||
	   (sa->root != H && D2H(sa->root)->ref == 0))
		error(exHeap);
	acheck(t->size, sa->len);
	if(sa->len != 0 && t->size != 0){
		/* acheck has already bounded the complete multiplication and allocation. */
		bytes = (ulong)sa->len*t->size;
		if(sa->data == nil || sa->data == H ||
		   (uintptr)sa->data > (uintptr)-1-bytes)
			error(exHeap);
	}
	da = H2D(Array*, heaparray(t, sa->len));
	c->result = da;
	if(sa->t->np == 0){
		if(da->len != 0 && da->t->size != 0)
			memmove(da->data, sa->data, da->len*da->t->size);
		return 1;
	}
	/* Dis data declarations limit nesting to DADEPTH. */
	for(i = 0; i < sa->len; i++)
		newmp(da->data+i*da->t->size, sa->data+i*sa->t->size, sa->t);
	return 1;
}


/* Release the complete partial tree before propagating the original VM error. */
void*
arraycpy(Array *sa)
{
	ArrayCopy c;

	if(sa == H)
		return H;
	if(sa == nil || D2H(sa)->ref == 0)
		error(exHeap);
	c.source = sa;
	c.result = H;
	if(kprotect(copyarray, &c))
		return c.result;
	destroy(c.result);
	krethrow();
	return H;
}


/* Validate mapped spans, then publish only acquired owners for failure cleanup. */
void
newmp(void *dst, void *src, Type *t)
{
	Heap *h;
	int c, i, bit;
	ulong offset;
	void *wp, *owned;
	uchar *q;

	if(t == nil || t == H || t->size < 0 || t->np < 0 ||
	   t->np > t->size/(8*sizeof(void*))+(t->size%(8*sizeof(void*)) != 0) ||
	   (t->size != 0 && (src == nil || src == H || dst == nil || dst == H)))
		error(exHeap);
	/* Reject partial pointer slots before initmem can write outside the payload. */
	for(i = 0; i < t->np; i++)
		for(bit = 0; bit < 8; bit++)
			if((t->map[i] & (0x80>>bit)) &&
			   ((ulong)i*8+bit >= (ulong)t->size/sizeof(void*)))
				error(exHeap);
	if(t->size != 0)
		memmove(dst, src, t->size);
	initmem(t, dst);
	for(i = 0; i < t->np; i++){
		c = t->map[i];
		if(c == 0)
			continue;
		for(bit = 0; c != 0 && bit < 8; bit++){
			if((c & (0x80>>bit)) == 0)
				continue;
			offset = ((ulong)i*8+bit)*sizeof(void*);
			q = (uchar*)dst+offset;
			memmove(&wp, (uchar*)src+offset, sizeof(wp));
			if(wp == H)
				continue;
			if(wp == nil)
				error(exHeap);
			h = D2H(wp);
			if(h->ref == 0 || h->t == nil || h->t == H)
				error(exHeap);
			if(h->t == &Tarray)
				owned = arraycpy(wp);
			else{
				if(h->ref == (ulong)~0)
					error(exHeap);
				h->ref++;
				Setmark(h);
				owned = wp;
			}
			memmove(q, &owned, sizeof(owned));
		}
	}
}
