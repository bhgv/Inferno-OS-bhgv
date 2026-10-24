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
	ca = (Channel**)a->data;
	for(i = 0; i < a->len; i++){
		c = ca[i];
		if(c != H) {
			if(c->send->prog || CANGET(c))
				nrdy++;
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
	ca = (Channel**)a->data;
	for(n = 0; n < a->len; n++){
		c = ca[n];
		if(c != H && c->recv->prog)
			cqdelp(&c->recv, p);
		if(sr == 1 && *sel == c) {
			W(p->R.d) = dn;
			p->ptr = ptr + 1;
			ptr[0] = n;
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
	int nrdy;
	Channel *c;
	Altc *ac, *eac;

	e = nil;
	nrdy = 0;

	ac = a->ac + a->nsend;
	eac = ac + a->nrecv;
	while(ac < eac) {
		c = ac->c;
		ac++;
		if(c == H) {
			e = exNilref;
			continue;
		}
		t = D2H(c)->t;
		if(t == &Tarray)
			nrdy += altmark(c, p);
		else {
			if(c->send->prog || CANGET(c))
				nrdy++;
			cqadd(&c->recv, p);
		}
	}

	ac = a->ac;
	eac = ac + a->nsend;
	while(ac < eac) {
		c = ac->c;
		ac++;
		if(c == H) {
			e = exNilref;
			continue;
		}
		if(altreceives(c, p)){
			e = exAlt;
			continue;
		}
		if(c->recv->prog || CANPUT(c))
			nrdy++;
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
	int n;
	Type *t;
	Channel *c;
	Altc *ac, *eac;

	n = 0;
	ac = a->ac;
	eac = a->ac + a->nsend;
	while(ac < eac) {
		c = ac->c;
		if(c != H) {
			if(c->send->prog)
				cqdelp(&c->send, p);
			if(sr == 0 && c == sel) {
				p->ptr = ac->ptr;
				W(p->R.d) = n;
				sel = nil;
			}
		}
		ac++;
		n++;
	}

	eac = a->ac + a->nsend + a->nrecv;
	while(ac < eac) {
		c = ac->c;
		if(c != H) {
			t = D2H(c)->t;
//printf("t=%X\n", t);
			if(t == &Tarray)
				altunmark(c, ac->ptr, p, sr, &sel, n);
			else {
//printf("c=%X\n", c);
//printf("c->recv=%X\n", c->recv);
//printf("c->recv->prog=%X\n", c->recv->prog);
				if(c->recv && c->recv->prog)
					cqdelp(&c->recv, p);
				if(sr == 1 && c == sel) {
					p->ptr = ac->ptr;
					W(p->R.d) = n;
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
	int n;
	WORD an;
	WORD *ptr;
	Altc *ac, *eac;
	Channel *c, **ca;

	n = 0;
	ac = a->ac;
	eac = ac + a->nsend;
	while(ac < eac) {
		c = ac->c;
		if((c->recv->prog != nil || CANPUT(c)) && which-- == 0) {
			W(R.d) = n;
			R.s = ac->ptr;
			R.d = &c;
			isend();
			return;
		}
		ac++;
		n++;
	}

	eac = eac + a->nrecv;
	while(ac < eac) {
		c = ac->c;
		t = D2H(c)->t;
		if(t == &Tarray) {
			r = (Array*)c;
			ca = (Channel**)r->data;
			for(an = 0; an < r->len; an++){
				c = ca[an];
				if(c != H && (c->send->prog != nil || CANGET(c)) && which-- == 0) {
					W(R.d) = n;
					R.s = &c;
					ptr = ac->ptr;
					R.d = ptr + 1;
					ptr[0] = an;
					irecv();
					return;
				}
			}
		}
		else
		if((c->send->prog != nil || CANGET(c)) && which-- == 0) {
			W(R.d) = n;
			R.s = &c;
			R.d = ac->ptr;
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
	int nrdy;
	AltReady ready;
	static ulong xrand = 0x20342;

	p = currun();

	a = R.s;
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
		W(R.d) = a->nsend + a->nrecv;
		altdone(a, p, nil, -1);
		return;
	}

	xrand = xrand*1103515245 + 12345;
	altcomm(a, (xrand>>8)%nrdy);
	altdone(a, p, nil, -1);
}
