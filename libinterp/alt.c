#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include "vmchannel.h"

#define OP(fn)	void fn(void) // Shared C instruction declarations.

#define CANGET(c)	((c)->size > 0) // Buffered receive readiness.
#define CANPUT(c)	((c)->buf != H && (c)->size < (c)->buf->len) // Buffered send readiness.

extern	OP(isend);
extern	OP(irecv);


/* Reject an unusable selection cell before registering or waking any process. */
static void
altresult(void *slot)
{
	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(WORD))
		error(exBounds);
}


/* Store a VM selection word without requiring native slot alignment. */
static void
altstore(void *slot, WORD value)
{
	altresult(slot);
	memmove(slot, &value, sizeof(value));
}


/* Capture a possibly four-byte-aligned alt header before forming table spans. */
static int
altheader(Alt *a, int *nsend, int *nrecv)
{
	uvlong bytes;

	if(a == nil || a == H || (uintptr)a > (uintptr)-1- offsetof(Alt, ac))
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


/* Identify a channel/array only after its complete native header and heap prefix fit. */
static int
altobject(Channel *c)
{
	Type *type;
	uintptr bytes;

	if(c == nil || c == H)
		error(exNilref);
	if((uintptr)c <= sizeof(Heap) || (uintptr)c%sizeof(void*) != 0)
		error(exHeap);
	bytes = sizeof(Array) < sizeof(Channel) ? sizeof(Array) : sizeof(Channel);
	if((uintptr)c > (uintptr)-1-bytes)
		error(exHeap);
	/* Reading only the preceding Type cell does not require a Channel-sized array. */
	memmove(&type, (uchar*)c-sizeof(Heap)+offsetof(Heap, t), sizeof(type));
	bytes = type == &Tarray ? sizeof(Array) : sizeof(Channel);
	if((uintptr)c > (uintptr)-1-bytes)
		error(exHeap);
	return type == &Tarray;
}


/* Validate both chains before registration makes cancellation responsible for this channel. */
static void
altchannel(Channel *c, int receive)
{
	if(altobject(c))
		error(exHeap);
	vmchanready(c, receive);
	vmchanend(&c->send);
	vmchanend(&c->recv);
}


/* Check every guard/member before the first allocation or partial ALT registration. */
static void
altvalidate(Alt *a, int nsend, int nrecv)
{
	Channel *c, *member;
	Array *array;
	WORD n;
	int guard, receive;

	for(guard = 0; guard < nsend+nrecv; guard++){
		memmove(&c, (uchar*)a+offsetof(Alt, ac)+
			(uintptr)guard*sizeof(Altc)+offsetof(Altc, c), sizeof(c));
		receive = guard >= nsend;
		if(!altobject(c)){
			altchannel(c, receive);
			continue;
		}
		if(!receive)
			error(exHeap);
		array = (Array*)c;
		if(!altarrayvalid(array))
			error(exBounds);
		for(n = 0; n < array->len; n++){
			memmove(&member, array->data+(uintptr)n*sizeof(member), sizeof(member));
			if(member != H)
				altchannel(member, 1);
		}
	}
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


typedef struct AltSelection AltSelection;

/* Capture chosen operands before result/tuple/payload writes can replace the guard table. */
struct AltSelection
{
	Channel *channel; // Selected concrete channel, including an array member.
	void *payload; // Original selected send source or receive destination.
	void *result; // Selection output captured before cancellation or publication.
	void *tuple; // Optional array-member index cell before the receive payload.
	WORD guard, member; // Guard ordinal and optional channel-array member ordinal.
	int array; // Nonzero requires publishing a receive-array member index.
	int receive; // Nonzero selects receive rather than send.
	int found; // Complete selection was captured without publishing output.
};


/* Cancel array registrations, including a queue already drained by channel finalization. */
static void
altunmark(Channel *c, void *ptr, Prog *p, int sr, Channel **sel, int dn, AltSelection *selected)
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
		if(c != H && c != nil && c->recv != nil && c->recv->prog)
			cqdelp(&c->recv, p);
		if(sr == 1 && c != nil && c != H && *sel == c){
			selected->array = 1;
			selected->tuple = ptr;
			selected->guard = dn;
			selected->member = n;
			selected->found = 1;
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


/* Cancel surviving registrations before publication; killcomm may have drained one queue. */
void
altdone(Alt *a, Prog *p, Channel *sel, int sr)
{
	int n, nsend, nrecv, selecting;
	void *ptr;
	AltSelection selected;
	Type *t;
	Channel *c;
	Altc *ac, *eac;

	if(!altheader(a, &nsend, &nrecv))
		return;
	selecting = sel != nil && (sr == 0 || sr == 1);
	if(selecting){
		selected.found = 0;
		selected.array = 0;
		selected.result = p->R.d;
	}
	n = 0;
	ac = (Altc*)((uchar*)a+offsetof(Alt, ac));
	eac = (Altc*)((uchar*)a+offsetof(Alt, ac))+nsend;
	while(ac < eac) {
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		if(c != H && c != nil) {
			if(c->send != nil && c->send->prog)
				cqdelp(&c->send, p);
			if(sr == 0 && c == sel) {
				memmove(&selected.payload, (uchar*)ac+offsetof(Altc, ptr), sizeof(selected.payload));
				selected.guard = n;
				selected.found = 1;
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
				altunmark(c, ptr, p, sr, &sel, n, &selected);
			}
			else {
				if(c->recv && c->recv->prog)
					cqdelp(&c->recv, p);
				if(sr == 1 && c == sel) {
					memmove(&selected.payload, (uchar*)ac+offsetof(Altc, ptr), sizeof(selected.payload));
					selected.guard = n;
					selected.found = 1;
					sel = nil;
				}
			}
		}
		ac++;
		n++;
	}
	if(selecting && selected.found){
		altresult(selected.result);
		if(selected.array){
			altresult(selected.tuple);
			selected.payload = (uchar*)selected.tuple+sizeof(WORD);
		}
		p->ptr = selected.payload;
		altstore(selected.result, selected.guard);
		if(selected.array)
			memmove(selected.tuple, &selected.member, sizeof(selected.member));
	}
}


/* Resolve one ready alternative without changing registers, queues or output cells. */
static void
altchoose(Alt *a, int which, AltSelection *selected)
{
	Type *t;
	Array *r;
	int n, nsend, nrecv;
	WORD an;
	void *ptr;
	Altc *ac, *eac;
	Channel *c, **ca;

	if(!altheader(a, &nsend, &nrecv))
		error(exBounds);
	memset(selected, 0, sizeof(*selected));
	selected->result = R.d;
	n = 0;
	ac = (Altc*)((uchar*)a+offsetof(Alt, ac));
	eac = ac+nsend;
	while(ac < eac){
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		if((c->recv->prog != nil || CANPUT(c)) && which-- == 0){
			selected->channel = c;
			selected->guard = n;
			memmove(&selected->payload, (uchar*)ac+offsetof(Altc, ptr), sizeof(selected->payload));
			selected->found = 1;
			return;
		}
		ac++;
		n++;
	}

	eac += nrecv;
	selected->receive = 1;
	while(ac < eac){
		memmove(&c, (uchar*)ac+offsetof(Altc, c), sizeof(c));
		t = D2H(c)->t;
		if(t == &Tarray){
			r = (Array*)c;
			ca = (Channel**)r->data;
			for(an = 0; an < r->len; an++){
				memmove(&c, (uchar*)ca+an*sizeof(c), sizeof(c));
				if(c != H && c != nil && (c->send->prog != nil || CANGET(c)) && which-- == 0){
					memmove(&ptr, (uchar*)ac+offsetof(Altc, ptr), sizeof(ptr));
					altresult(ptr);
					selected->array = 1;
					selected->tuple = ptr;
					selected->member = an;
					selected->payload = (uchar*)ptr+sizeof(WORD);
					selected->channel = c;
					selected->guard = n;
					selected->found = 1;
					return;
				}
			}
		}else if((c->send->prog != nil || CANGET(c)) && which-- == 0){
			selected->channel = c;
			selected->guard = n;
			memmove(&selected->payload, (uchar*)ac+offsetof(Altc, ptr), sizeof(selected->payload));
			selected->found = 1;
			return;
		}
		ac++;
		n++;
	}
}


/* Transfer only captured operands after no registration cleanup can revisit overwritten guards. */
static void
alttransfer(AltSelection *selected)
{
	Channel *c;

	c = selected->channel;
	altstore(selected->result, selected->guard);
	if(selected->array)
		memmove(selected->tuple, &selected->member, sizeof(selected->member));
	if(selected->receive){
		R.s = &c;
		R.d = selected->payload;
		irecv();
	}else{
		R.s = selected->payload;
		R.d = &c;
		isend();
	}
}



/* Wake an abandoned blocked alternative after removing its registrations. */
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
	Alt *alt; // Captured guard table used by preparation and cleanup.
	Prog *prog; // Process whose registrations must be removed on error.
	int ready; // Number of ready guard alternatives.
	int choice; // Selected ready alternative for protected communication.
	int registered; // Cleanup still owns the guard table until cancellation completes.
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


/* Snapshot ready operands, cancel once, then transfer without revisiting mutable guard storage. */
static int
altcommunicate(void *arg)
{
	AltReady *r;
	AltSelection selected;

	r = arg;
	altchoose(r->alt, r->choice, &selected);
	if(selected.found){
		altdone(r->alt, r->prog, nil, -1);
		r->registered = 0;
		alttransfer(&selected);
	}
	return 1;
}


/* Cancel this process's registrations when preparation or communication raises. */
void
xecalt(int block)
{
	Alt *a;
	Prog *p;
	int nrdy, nsend, nrecv;
	AltReady ready;
	void *result;
	static ulong xrand = 0x20342;

	p = currun();

	a = R.s;
	if(a == nil || a == H)
		error(exNilref);
	if(!altheader(a, &nsend, &nrecv))
		error(exBounds);
	result = R.d;
	altresult(result);
	altvalidate(a, nsend, nrecv);
	ready.alt = a;
	ready.prog = p;
	ready.registered = 1;
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
		altdone(a, p, nil, -1);
		altstore(result, nsend+nrecv);
		return;
	}

	xrand = xrand*1103515245 + 12345;
	ready.choice = (xrand>>8)%nrdy;
	if(!kprotect(altcommunicate, &ready)){
		if(ready.registered)
			altdone(a, p, nil, -1);
		krethrow();
	}
	if(ready.registered)
		altdone(a, p, nil, -1);
}
