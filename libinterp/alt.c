#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"

#define OP(fn)	void fn(void)
#define W(p)	*((WORD*)(p))

#define CANGET(c)	((c)->size > 0)
#define CANPUT(c)	((c)->buf != H && (c)->size < (c)->buf->len)

extern	OP(isend);
extern	OP(irecv);

/* Store a VM selection word without requiring native slot alignment. */
static void
altstore(void *slot, WORD value)
{
	memmove(slot, &value, sizeof(value));
}

/* Capture a possibly four-byte-aligned alt header before forming table spans. */
static int
altheader(Alt *a, int *nsend, int *nrecv)
{
	uvlong bytes;

	if(a == nil || a == H)
		return 0;
	memmove(nsend, (uchar*)a+offsetof(Alt, nsend), sizeof(*nsend));
	memmove(nrecv, (uchar*)a+offsetof(Alt, nrecv), sizeof(*nrecv));
	if(*nsend < 0 || *nrecv < 0 || *nsend > 0x7fffffff/(int)sizeof(Altc)-*nrecv)
		return 0;
	bytes = (uvlong)(*nsend+*nrecv)*sizeof(Altc)+offsetof(Alt, ac);
	return (uintptr)a <= (uintptr)-1-bytes;
}

/* Invalid array geometry must also be safe to revisit during partial cleanup. */
static int
altarrayvalid(Array *a)
{
	uvlong bytes;

	if(a->len < 0 || (uvlong)a->len > 0x7fffffff/sizeof(Channel*))
		return 0;
	if(a->len == 0)
		return 1;
	if(a->data == nil || a->data == H)
		return 0;
	bytes = (uvlong)a->len*sizeof(Channel*);
	return (uintptr)a->data <= (uintptr)-1-bytes;
}

/*
 * Count the number of ready channels in an array of channels
 * Register the owning prog without arithmetic on an empty array data span.
 */
static int
altmark(Channel *c, Prog *p)
{
	int nrdy;
	Array *a;
	Channel **ca;
	WORD i;

	nrdy = 0;
	a = (Array*)c;
	if(!altarrayvalid(a))
		error(exBounds);
	ca = (Channel**)a->data;
	for(i = 0; i < a->len; i++){
		memmove(&c, (uchar*)ca+i*sizeof(c), sizeof(c));
		if(c == nil)
			error(exNilref);
		if(c != H && c != nil) {
			if(c->send->prog || CANGET(c)){
				if(nrdy == 0x7fffffff)
					error(exBounds);
				nrdy++;
			}
			cqadd(&c->recv, p);
		}
	}

	return nrdy;
}

/*
 * Remove alt references by index, including empty views with nil data.
 */
static void
altunmark(Channel *c, WORD *ptr, Prog *p, int sr, Channel **sel, int dn)
{
	WORD n;
	Array *a;
	Channel **ca;

	a = (Array*)c;
	if(!altarrayvalid(a))
		return;
	ca = (Channel**)a->data;
	for(n = 0; n < a->len; n++){
		memmove(&c, (uchar*)ca+n*sizeof(c), sizeof(c));
		if(c != H && c != nil && c->recv->prog)
			cqdelp(&c->recv, p);
		if(sr == 1 && *sel == c) {
			altstore(p->R.d, dn);
			p->ptr = (uchar*)ptr+sizeof(WORD);
			memmove(ptr, &n, sizeof(n));
			*sel = nil;
		}
	}
}

/* Detect this alt's receive registration even behind unrelated waiters. */
static int
altreceives(Channel *c, Prog *p)
{
	Progq *q;

	for(q = c->recv; q != nil; q = q->next)
		if(q->prog == p)
			return 1;
	return 0;
}

/*
 * ALT Pass 1 - Count the number of ready channels and mark
 * each channel as ALT by this prog
 */
static int
altrdy(Alt *a, Prog *p)
{
	char *e;
	Type *t;
	int nrdy, nsend, nrecv, added;
	Channel *c;
	Altc *ac, *eac;

	if(!altheader(a, &nsend, &nrecv))
		error(exBounds);
	e = nil;
	nrdy = 0;

	ac = (Altc*)((uchar*)a+offsetof(Alt, ac))+nsend;
	eac = ac + nrecv;
	while(ac < eac) {
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		ac++;
		if(c == H || c == nil) {
			e = exNilref;
			continue;
		}
		t = D2H(c)->t;
		if(t == &Tarray){
			added = altmark(c, p);
			if(added > 0x7fffffff-nrdy)
				error(exBounds);
			nrdy += added;
		}
		else {
			if(c->send->prog || CANGET(c)){
				if(nrdy == 0x7fffffff)
					error(exBounds);
				nrdy++;
			}
			cqadd(&c->recv, p);
		}
	}

	ac = (Altc*)((uchar*)a+offsetof(Alt, ac));
	eac = ac + nsend;
	while(ac < eac) {
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		ac++;
		if(c == H || c == nil) {
			e = exNilref;
			continue;
		}
		if(altreceives(c, p)){
			e = exAlt;
			continue;
		}
		if(c->recv->prog || CANPUT(c)){
			if(nrdy == 0x7fffffff)
				error(exBounds);
			nrdy++;
		}
		cqadd(&c->send, p);
	}

	if(e != nil)
		error(e);

	return nrdy;
}

/*
 * ALT Pass 3 - Pull out of an ALT cancelling the channel pointers in each item
 */
void
altdone(Alt *a, Prog *p, Channel *sel, int sr)
{
	int n, nsend, nrecv;
	WORD *ptr;
	Type *t;
	Channel *c;
	Altc *ac, *eac;

	if(!altheader(a, &nsend, &nrecv))
		return;
	n = 0;
	ac = (Altc*)((uchar*)a+offsetof(Alt, ac));
	eac = (Altc*)((uchar*)a+offsetof(Alt, ac))+nsend;
	while(ac < eac) {
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		if(c != H && c != nil) {
			if(c->send->prog)
				cqdelp(&c->send, p);
			if(sr == 0 && c == sel) {
				memmove(&p->ptr, (uchar*)ac+offsetof(Altc, ptr), sizeof(p->ptr));
				altstore(p->R.d, n);
				sel = nil;
			}
		}
		ac++;
		n++;
	}

	eac = (Altc*)((uchar*)a+offsetof(Alt, ac))+nsend+nrecv;
	while(ac < eac) {
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		if(c != H && c != nil) {
			t = D2H(c)->t;
			if(t == &Tarray){
				memmove(&ptr, (uchar*)ac+offsetof(Altc, ptr), sizeof(ptr));
				altunmark(c, ptr, p, sr, &sel, n);
			}
			else {
				if(c->recv && c->recv->prog)
					cqdelp(&c->recv, p);
				if(sr == 1 && c == sel) {
					memmove(&p->ptr, (uchar*)ac+offsetof(Altc, ptr), sizeof(p->ptr));
					altstore(p->R.d, n);
					sel = nil;
				}
			}
		}
		ac++;
		n++;
	}
}

/*
 * ALT Pass 2 - Communicate on the chosen channel; empty array guards read no data.
 */
static void
altcomm(Alt *a, int which)
{
	Type *t;
	Array *r;
	int n, nsend, nrecv;
	WORD an;
	WORD *ptr;
	Altc *ac, *eac;
	Channel *c, **ca;

	if(!altheader(a, &nsend, &nrecv))
		error(exBounds);
	n = 0;
	ac = (Altc*)((uchar*)a+offsetof(Alt, ac));
	eac = ac + nsend;
	while(ac < eac) {
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		if((c->recv->prog != nil || CANPUT(c)) && which-- == 0) {
			altstore(R.d, n);
			memmove(&R.s, (uchar*)ac+offsetof(Altc, ptr), sizeof(R.s));
			R.d = &c;
			isend();
			return;
		}
		ac++;
		n++;
	}

	eac = eac + nrecv;
	while(ac < eac) {
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		t = D2H(c)->t;
		if(t == &Tarray) {
			r = (Array*)c;
			ca = (Channel**)r->data;
			for(an = 0; an < r->len; an++){
				memmove(&c, (uchar*)ca+an*sizeof(c), sizeof(c));
				if(c != H && c != nil && (c->send->prog != nil || CANGET(c)) && which-- == 0) {
					altstore(R.d, n);
					R.s = &c;
					memmove(&ptr, (uchar*)ac+offsetof(Altc, ptr), sizeof(ptr));
					R.d = (uchar*)ptr+sizeof(WORD);
					memmove(ptr, &an, sizeof(an));
					irecv();
					return;
				}
			}
		}
		else
		if((c->send->prog != nil || CANGET(c)) && which-- == 0) {
			altstore(R.d, n);
			R.s = &c;
			memmove(&R.d, (uchar*)ac+offsetof(Altc, ptr), sizeof(R.d));
			irecv();
			return;	
		}
		ac++;
		n++;
	}
	return;
}

void
altgone(Prog *p)
{
	Alt *a;

	if (p->state == Palt) {
		a = p->R.s;
		altdone(a, p, nil, -1);
		p->kill = "alt channel hungup";
		addrun(p);
	}
}

typedef struct AltReady AltReady;
struct AltReady
{
	Alt *alt;
	Prog *prog;
	int ready;
};

/* Keep the partial-registration context outside the VM error jump frame. */
static int
altready(void *arg)
{
	AltReady *r;

	r = arg;
	r->ready = altrdy(r->alt, r->prog);
	return 1;
}

/* Cancel all partial registrations if preparing an alt raises a VM error. */
void
xecalt(int block)
{
	Alt *a;
	Prog *p;
	int nrdy, nsend, nrecv;
	AltReady ready;
	static ulong xrand = 0x20342;

	p = currun();

	a = R.s;
	if(a == nil || a == H)
		error(exNilref);
	if(!altheader(a, &nsend, &nrecv))
		error(exBounds);
	ready.alt = a;
	ready.prog = p;
	if(!kprotect(altready, &ready)){
		altdone(a, p, nil, -1);
		krethrow();
	}
	nrdy = ready.ready;
	if(nrdy == 0) {
		if(block) {
			delrun(Palt);
			p->R.s = R.s;
			p->R.d = R.d;
			R.IC = 1;
			R.t = 1;
			return;
		}
		altstore(R.d, nsend+nrecv);
		altdone(a, p, nil, -1);
		return;
	}

	xrand = xrand*1103515245 + 12345;
	altcomm(a, (xrand>>8)%nrdy);
	altdone(a, p, nil, -1);
}
