#include <lib9.h>
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "pool.h"
#include "kernel.h"

REG	R;			/* Virtual Machine registers */
String	snil;			/* String known to be zero length */

#define Stmp	*((WORD*)(R.FP+NREG*IBY2WD))
#define Dtmp	*((WORD*)(R.FP+(NREG+2)*IBY2WD))

#define OP(fn)	void fn(void)
#define B(r)	*((BYTE*)(R.r))
#define W(r)	*((WORD*)(R.r))
#define UW(r)	*((UWORD*)(R.r))
#define F(r)	*((REAL*)(R.r))
#define V(r)	*((LONG*)(R.r))	
#define UV(r)	*((ULONG*)(R.r))	
#define	S(r)	*((String**)(R.r))
#define	A(r)	*((Array**)(R.r))
#define	L(r)	*((List**)(R.r))
#define P(r)	*((WORD**)(R.r))
#define C(r)	*((Channel**)(R.r))
#define T(r)	*((void**)(R.r))
#define JMP(r)	R.PC = *(Inst**)(R.r)
#define SH(r)	*((SHORT*)(R.r))
#define SR(r)	*((SREAL*)(R.r))

OP(runt) {}
OP(negf) { F(d) = -F(s); }
OP(jmp)  { JMP(d); }
OP(movpc){ T(d) = &R.M->prog[W(s)]; }
OP(movm) { memmove(R.d, R.s, W(m)); }
OP(lea)  { W(d) = (WORD)R.s; }
OP(movb) { B(d) = B(s); }
OP(movw) { W(d) = W(s); }
OP(movf) { F(d) = F(s); }
OP(movl) { V(d) = V(s); }
OP(cvtbw){ W(d) = B(s); }
OP(cvtwb){ B(d) = W(s); }
OP(cvtrf){ F(d) = SR(s); }
OP(cvtfr){ SR(d) = F(s); }
OP(cvtws){ SH(d) = W(s); }
OP(cvtsw){ W(d) = SH(s); }
OP(cvtwf){ F(d) = W(s); }
OP(addb) { B(d) = B(m) + B(s); }
/* Keep word addition independent of host signed-overflow optimization. */
OP(addw)
{
	W(d) = UW(m) + UW(s);
}
/* Keep big addition independent of host signed-overflow optimization. */
OP(addl)
{
	V(d) = UV(m) + UV(s);
}
OP(addf) { F(d) = F(m) + F(s); }
OP(subb) { B(d) = B(m) - B(s); }
/* Keep word subtraction independent of host signed-overflow optimization. */
OP(subw)
{
	W(d) = UW(m) - UW(s);
}
/* Keep big subtraction independent of host signed-overflow optimization. */
OP(subl)
{
	V(d) = UV(m) - UV(s);
}
OP(subf) { F(d) = F(m) - F(s); }
OP(divb) { B(d) = B(m) / B(s); }
OP(divw) { W(d) = W(m) / W(s); }
OP(divl) { V(d) = V(m) / V(s); }
OP(divf) { F(d) = F(m) / F(s); }
OP(modb) { B(d) = B(m) % B(s); }
OP(modw) { W(d) = W(m) % W(s); }
OP(modl) { V(d) = V(m) % V(s); }
OP(mulb) { B(d) = B(m) * B(s); }
/* Keep word multiplication independent of host signed-overflow optimization. */
OP(mulw)
{
	W(d) = UW(m) * UW(s);
}
/* Keep big multiplication independent of host signed-overflow optimization. */
OP(mull)
{
	V(d) = UV(m) * UV(s);
}
OP(mulf) { F(d) = F(m) * F(s); }
OP(andb) { B(d) = B(m) & B(s); }
OP(andw) { W(d) = W(m) & W(s); }
OP(andl) { V(d) = V(m) & V(s); }
OP(xorb) { B(d) = B(m) ^ B(s); }
OP(xorw) { W(d) = W(m) ^ W(s); }
OP(xorl) { V(d) = V(m) ^ V(s); }
OP(orb)  { B(d) = B(m) | B(s); }
OP(orw)  { W(d) = W(m) | W(s); }
OP(orl)  { V(d) = V(m) | V(s); }
OP(shlb) { B(d) = B(m) << W(s); }
/* Shift the value bits without invoking signed-left-shift overflow. */
OP(shlw)
{
	W(d) = UW(m) << W(s);
}
/* Shift the value bits without invoking signed-left-shift overflow. */
OP(shll)
{
	V(d) = UV(m) << W(s);
}
OP(shrb) { B(d) = B(m) >> W(s); }
OP(shrw) { W(d) = W(m) >> W(s); }
OP(shrl) { V(d) = V(m) >> W(s); }
OP(lsrw) { W(d) = UW(m) >> W(s); }
OP(lsrl) { V(d) = UV(m) >> W(s); }
OP(beqb) { if(B(s) == B(m)) JMP(d); }
OP(bneb) { if(B(s) != B(m)) JMP(d); }
OP(bltb) { if(B(s) <  B(m)) JMP(d); }
OP(bleb) { if(B(s) <= B(m)) JMP(d); }
OP(bgtb) { if(B(s) >  B(m)) JMP(d); }
OP(bgeb) { if(B(s) >= B(m)) JMP(d); }
OP(beqw) { if(W(s) == W(m)) JMP(d); }
OP(bnew) { if(W(s) != W(m)) JMP(d); }
OP(bltw) { if(W(s) <  W(m)) JMP(d); }
OP(blew) { if(W(s) <= W(m)) JMP(d); }
OP(bgtw) { if(W(s) >  W(m)) JMP(d); }
OP(bgew) { if(W(s) >= W(m)) JMP(d); }
OP(beql) { if(V(s) == V(m)) JMP(d); }
OP(bnel) { if(V(s) != V(m)) JMP(d); }
OP(bltl) { if(V(s) <  V(m)) JMP(d); }
OP(blel) { if(V(s) <= V(m)) JMP(d); }
OP(bgtl) { if(V(s) >  V(m)) JMP(d); }
OP(bgel) { if(V(s) >= V(m)) JMP(d); }
OP(beqf) { if(F(s) == F(m)) JMP(d); }
OP(bnef) { if(F(s) != F(m)) JMP(d); }
OP(bltf) { if(F(s) <  F(m)) JMP(d); }
OP(blef) { if(F(s) <= F(m)) JMP(d); }
OP(bgtf) { if(F(s) >  F(m)) JMP(d); }
OP(bgef) { if(F(s) >= F(m)) JMP(d); }
OP(beqc) { if(stringcmp(S(s), S(m)) == 0) JMP(d); }
OP(bnec) { if(stringcmp(S(s), S(m)) != 0) JMP(d); }
OP(bltc) { if(stringcmp(S(s), S(m)) <  0) JMP(d); }
OP(blec) { if(stringcmp(S(s), S(m)) <= 0) JMP(d); }
OP(bgtc) { if(stringcmp(S(s), S(m)) >  0) JMP(d); }
OP(bgec) { if(stringcmp(S(s), S(m)) >= 0) JMP(d); }
OP(iexit){ error(""); }
OP(cvtwl){ V(d) = W(s); }
OP(cvtlw){ W(d) = V(s); }
OP(cvtlf){ F(d) = V(s); }
/* Round first, then reject non-finite or out-of-range big conversions. */
OP(cvtfl)
{
	REAL f;

	f = F(s);
	f = f < 0 ? f - .5 : f + .5;
	if(!(f >= -9223372036854775808.0 && f < 9223372036854775808.0))
		error(exInval);
	V(d) = (LONG)f;
}
/* Check the active VM word width before publishing the rounded integer. */
OP(cvtfw)
{
	REAL f;

	f = F(s);
	f = f < 0 ? f - .5 : f + .5;
	if(sizeof(WORD) == 4){
		if(!(f > -2147483649.0 && f < 2147483648.0))
			error(exInval);
	}else{
		if(!(f >= -9223372036854775808.0 && f < 9223372036854775808.0))
			error(exInval);
	}
	W(d) = (WORD)f;
}
OP(cvtcl)
{
	String *s;

	s = S(s);
	if(s == H)
		V(d) = 0;
	else
		V(d) = strtoll(string2c(s), nil, 10);
}

/* Compute an integer power with word-width wraparound and bounded iteration. */
OP(iexpw)
{
	WORD x, n;
	UWORD base, exponent, result;

	x = W(m);
	n = W(s);
	if(n < 0){
		if(x == 0)
			error(exZdiv);
		if(x == 1)
			W(d) = 1;
		else if(x == -1)
			W(d) = n&1 ? -1 : 1;
		else
			W(d) = 0;
		return;
	}
	base = x;
	exponent = n;
	result = 1;
	for(;;){
		if(exponent&1)
			result *= base;
		if((exponent >>= 1) == 0)
			break;
		base *= base;
	}
	W(d) = result;
}

/* Apply the same rules to big values while the exponent remains a VM word. */
OP(iexpl)
{
	WORD n;
	LONG x;
	ULONG base, result;
	UWORD exponent;

	x = V(m);
	n = W(s);
	if(n < 0){
		if(x == 0)
			error(exZdiv);
		if(x == 1)
			V(d) = 1;
		else if(x == -1)
			V(d) = n&1 ? -1 : 1;
		else
			V(d) = 0;
		return;
	}
	base = x;
	exponent = n;
	result = 1;
	for(;;){
		if(exponent&1)
			result *= base;
		if((exponent >>= 1) == 0)
			break;
		base *= base;
	}
	V(d) = result;
}

/* Handle the minimum negative exponent without signed negation or overflow. */
OP(iexpf)
{
	WORD n;
	UWORD exponent;
	REAL x, result;

	x = F(m);
	n = W(s);
	exponent = n;
	if(n < 0){
		exponent = 0-exponent;
		x = 1/x;
	}
	result = 1;
	for(;;){
		if(exponent&1)
			result *= x;
		if((exponent >>= 1) == 0)
			break;
		x *= x;
	}
	F(d) = result;
}

OP(indx)
{
	ulong i;
	Array *a;

	a = A(s);
	i = W(d);
	if(a == H || i >= a->len)
		error(exBounds);
	W(m) = (WORD)(a->data+i*a->t->size);
}
OP(indw)
{
	ulong i;
	Array *a;

	a = A(s);
	i = W(d);
	if(a == H || i >= a->len)
		error(exBounds);
	W(m) = (WORD)(a->data+i*sizeof(WORD));
}
OP(indf)
{
	ulong i;
	Array *a;

	a = A(s);
	i = W(d);
	if(a == H || i >= a->len)
		error(exBounds);
	W(m) = (WORD)(a->data+i*sizeof(REAL));
}
OP(indl)
{
	ulong i;
	Array *a;

	a = A(s);
	i = W(d);
	if(a == H || i >= a->len)
		error(exBounds);
	W(m) = (WORD)(a->data+i*sizeof(LONG));
}
OP(indb)
{
	ulong i;
	Array *a;

	a = A(s);
	i = W(d);
	if(a == H || i >= a->len)
		error(exBounds);
	W(m) = (WORD)(a->data+i*sizeof(BYTE));
}
OP(movp)
{
	Heap *h;
	WORD *dv, *sv;

	sv = P(s);
	if(sv != H) {
		h = D2H(sv);
		h->ref++;
		Setmark(h);
	}
	dv = P(d);
	P(d) = sv;
	destroy(dv);
}
/* Resolve the full Limbo index through the owner: Loader.tnew may move its table. */
static Type*
moduletype(Modlink *ml, WORD index)
{
	Module *m;
	Type *t;

	if(ml == H || ml == nil || ml->m == nil || ml->m == H)
		error(exModule);
	m = ml->m;
	if(index < 0 || index >= m->ntype || m->type == nil)
		error("invalid type");
	t = m->type[index];
	if(t == nil || t == H || t->size < 0)
		error("invalid type");
	return t;
}

/* Resolve mapped-record metadata before acquiring or replacing references. */
OP(movmp)
{
	Type *t;

	t = moduletype(R.M, W(m));

	incmem(R.s, t);
	if (t->np)
		freeptrs(R.d, t);
	memmove(R.d, R.s, t->size);
}
/* Validate the descriptor before allocating or replacing the destination. */
OP(new)
{
	Heap *h;
	WORD **wp, *t;

	h = heap(moduletype(R.M, W(s)));
	wp = R.d;
	t = *wp;
	*wp = H2D(WORD*, h);
	destroy(t);
}
/* Use the current module table for a zero-initialized object. */
OP(newz)
{
	Heap *h;
	WORD **wp, *t;

	h = heapz(moduletype(R.M, W(s)));
	wp = R.d;
	t = *wp;
	*wp = H2D(WORD*, h);
	destroy(t);
}
/* Resolve the requested descriptor in the source module before allocation. */
OP(mnewz)
{
	Heap *h;
	WORD **wp, *t;
	Modlink *ml;

	ml = *(Modlink**)R.s;
	if(ml == H)
		error(exModule);
	h = heapz(moduletype(ml, W(m)));
	wp = R.d;
	t = *wp;
	*wp = H2D(WORD*, h);
	destroy(t);
}
/* Resolve imports or named exports without narrowing the Limbo link index. */
static int
moduleentry(Modlink *ml, WORD index, Modl *entry)
{
	Link *l;

	if(ml == nil || ml == H || ml->nlinks < 0)
		return 0;
	if(index >= 0){
		if(index >= ml->nlinks)
			return 0;
		*entry = ml->links[index];
		return 1;
	}
	if(ml->m == nil || ml->m == H)
		return 0;
	if(ml->m->ext != nil)
		for(l = ml->m->ext; l->name != nil; l++, index++)
			if(index == -1){
				entry->u = l->u;
				entry->frame = l->frame;
				return 1;
			}
	return 0;
}

/* Compare remaining stack bytes before forming a prospective frame pointer. */
static int
framefits(Type *t)
{
	if(t == nil || t == H || t->size < (int)sizeof(Frame) ||
	   R.SP == nil || R.TS == nil || R.SP > R.TS)
		error(exHeap);
	return (ulong)t->size < (ulong)(R.TS-R.SP);
}

/* Validate the complete descriptor index and stack span before allocating a frame. */
OP(frame)
{
	Type *t;
	Frame *f;

	t = moduletype(R.M, W(s));
	if(!framefits(t)) {
		R.s = t;
		extend();
		T(d) = R.s;
		return;
	}
	f = (Frame*)R.SP;
	R.SP += t->size;
	f->t  = t;
	f->mr = nil;
	if (t->np)
		initmem(t, f);
	T(d) = f;
}
/* Resolve both imported and exported frame types through checked metadata. */
OP(mframe)
{
	Type *t;
	Frame *f;
	Modl entry;
	Modlink *ml;

	ml = *(Modlink**)R.s;
	if(ml == H || ml == nil)
		error(exModule);

	if(!moduleentry(ml, W(m), &entry))
		error("invalid mframe");
	t = entry.frame;
	if(!framefits(t)) {
		R.s = t;
		extend();
		T(d) = R.s;
		return;
	}
	f = (Frame*)R.SP;
	R.SP += t->size;
	f->t = t;
	f->mr = nil;
	if (t->np)
		initmem(t, f);
	T(d) = f;
}
/* Validate the current element descriptor before checking array geometry. */
OP(newa)
{
	WORD sz;
	Type *t;
	Heap *h;
	Array *a, *at, **ap;

	t = moduletype(R.M, W(m));
	sz = W(s);
	acheck(t->size, sz);
	h = nheap(sizeof(Array) + (t->size*sz));
	h->t = &Tarray;
	Tarray.ref++;
	a = H2D(Array*, h);
	a->t = t;
	a->len = sz;
	a->root = H;
	a->data = (uchar*)a + sizeof(Array);
	initarray(t, a);

	ap = R.d;
	at = *ap;
	*ap = a;
	destroy(at);
}
/* Use current element metadata for zeroed storage and pointer initialization. */
OP(newaz)
{
	WORD sz;
	Type *t;
	Heap *h;
	Array *a, *at, **ap;

	t = moduletype(R.M, W(m));
	sz = W(s);
	acheck(t->size, sz);
	h = nheap(sizeof(Array) + (t->size*sz));
	h->t = &Tarray;
	Tarray.ref++;
	a = H2D(Array*, h);
	a->t = t;
	a->len = sz;
	a->root = H;
	a->data = (uchar*)a + sizeof(Array);
	memset(a->data, 0, t->size*sz);
	initarray(t, a);

	ap = R.d;
	at = *ap;
	*ap = a;
	destroy(at);
}
typedef struct ChannelBuild ChannelBuild;
struct ChannelBuild {
	Type *t;
	void (*mover)(void);
	int len;
	Channel *c;
};

static int
channelbuild(void *arg)
{
	ChannelBuild *b;
	Channel *c;
	b=arg;
	b->c=c=H2D(Channel*,heap(&Tchannel));
	/* Make the partial channel safe for freechan before any further call. */
	c->buf=H; c->send=nil; c->recv=nil; c->aux=nil;
	c->mover=nil; c->mid.w=0; c->front=0; c->size=0;
	c->send=mallocz(sizeof(Progq),1);
	if(c->send == nil)
		error(exNomem);
	c->recv=mallocz(sizeof(Progq),1);
	if(c->recv == nil)
		error(exNomem);
	if(b->len > 0)
		c->buf=H2D(Array*,heaparray(b->t,b->len));
	if(b->mover == movtmp) {
		c->mid.t=b->t;
		b->t->ref++;
	}
	c->mover=b->mover;
	return 1;
}

Channel*
cnewc(Type *t, void (*mover)(void), int len)
{
	ChannelBuild b;
	if(len < 0)
		error(exNegsize);
	if((len > 0 || mover == movtmp) && t == nil)
		error(exHeap);
	if(len > 0)
		acheck(t->size,len);
	b.t=t; b.mover=mover; b.len=len; b.c=H;
	if(!kprotect(channelbuild,&b)) {
		destroy(b.c);
		krethrow();
	}
	return b.c;
}
Channel*
newc(Type *t, void (*mover)(void))
{
	Channel **cp, *oldc;
	WORD len;

	len = 0;
	if(R.m != R.d){
		len = W(m);
		if(len < 0)
			error(exNegsize);
	}
	if((uvlong)len > 0x7fffffff)
		error(exHeap);
	cp = R.d;
	oldc = *cp;
	*cp = cnewc(t, mover, len);
	destroy(oldc);
	return *cp;
}
OP(newcl)  { newc(&Tlong, movl);  }
OP(newcb)  { newc(&Tbyte, movb);  }
OP(newcw)  { newc(&Tword, movw);  }
OP(newcf)  { newc(&Treal, movf);  }
OP(newcp)  { newc(&Tptr, movp);  }
typedef struct RawChannel RawChannel;
struct RawChannel {
	Type *t;
};

static int
rawchannel(void *arg)
{
	RawChannel *b;
	Channel *c;
	b=arg;
	c=newc(b->t,movm);
	c->mid.w=W(s);
	return 1;
}

OP(newcm)
{
	RawChannel b;
	int ok;
	if(W(s) < 0)
		error(exNegsize);
	if((uvlong)W(s) > 0x7fffffff)
		error(exHeap);
	b.t=nil;
	if(R.m != R.d && W(m) > 0) {
		b.t=dtype(nil,W(s),nil,0);
		if(b.t == nil)
			error(exNomem);
	}
	ok=kprotect(rawchannel,&b);
	freetype(b.t);
	if(!ok)
		krethrow();
}
/* Create a mapped channel only after validating its payload descriptor. */
OP(newcmp)
{
	newc(moduletype(R.M, W(s)), movtmp);
}
OP(icase)
{
	WORD v, *t, *l, d, n, n2;

	v = W(s);
	t = (WORD*)((WORD)R.d + IBY2WD);
	n = t[-1];
	d = t[n*3];

	while(n > 0) {
		n2 = n >> 1;
		l = t + n2*3;
		if(v < l[0]) {
			n = n2;
			continue;
		}
		if(v >= l[1]) {
			t = l+3;
			n -= n2 + 1;
			continue;
		}
		d = l[2];
		break;
	}
	if(R.M->compiled) {
		R.PC = (Inst*)d;
		return;
	}
	R.PC = R.M->prog + d;
}
OP(casel)
{
	WORD *t, *l, d, n, n2;
	LONG v;

	v = V(s);
	t = (WORD*)((WORD)R.d + 2*IBY2WD);
	n = t[-2];
	d = t[n*6];

	while(n > 0) {
		n2 = n >> 1;
		l = t + n2*6;
		if(v < ((LONG*)l)[0]) {
			n = n2;
			continue;
		}
		if(v >= ((LONG*)l)[1]) {
			t = l+6;
			n -= n2 + 1;
			continue;
		}
		d = l[4];
		break;
	}
	if(R.M->compiled) {
		R.PC = (Inst*)d;
		return;
	}
	R.PC = R.M->prog + d;
}
OP(casec)
{
	WORD *l, *t, *e, n, n2, r;
	String *sl, *sh, *sv;
	
	sv = S(s);
	t = (WORD*)((WORD)R.d + IBY2WD);
	n = t[-1];
	e = t + n*3;
	if(n > 2){
		while(n > 0){
			n2 = n>>1;
			l = t + n2*3;
			sl = (String*)l[0];
			r = stringcmp(sv, sl);
			if(r == 0){
				e = &l[2];
				break;
			}
			if(r < 0){
				n = n2;
				continue;
			}
			sh = (String*)l[1];
			if(sh == H || stringcmp(sv, sh) > 0){
				t = l+3;
				n -= n2+1;
				continue;
			}
			e = &l[2];
			break;
		}
		t = e;
	}
	else{
		while(t < e) {
			sl = (String*)t[0];
			sh = (String*)t[1];
			if(sh == H) {
				if(stringcmp(sl, sv) == 0) {
					t = &t[2];
					goto found;
				}
			}
			else
			if(stringcmp(sl, sv) <= 0 && stringcmp(sh, sv) >= 0) {
				t = &t[2];
				goto found;
			}
			t += 3;
		}
	}
found:
	if(R.M->compiled) {
		R.PC = (Inst*)*t;
		return;
	}
	R.PC = R.M->prog + t[0];
}
OP(igoto)
{
	WORD *t;

	t = (WORD*)((WORD)R.d + (W(s) * IBY2WD));
	if(R.M->compiled) {
		R.PC = (Inst*)t[0];
		return;
	}
	R.PC = R.M->prog + t[0];
}
OP(call)
{
	Frame *f;

	f = T(s);
	f->lr = R.PC;
	f->fp = R.FP;
	R.FP = (uchar*)f;
	JMP(d);
}
typedef struct SpawnProg SpawnProg;
struct SpawnProg {
	Modlink *module;
	Inst *pc;
	Prog initial;
};

/* Build the child's stack before newprog makes it visible to the scheduler. */
static int
spawnready(void *arg)
{
	SpawnProg *c;
	Prog *p;

	c = arg;
	c->initial.R.M = c->module;
	c->initial.R.MP = c->module->MP;
	c->initial.R.PC = c->pc;
	newstack(&c->initial);
	p = newprog(currun(), c->module);
	p->R = c->initial.R;
	return 1;
}

/* On failure the source frame still owns its arguments; release only stack storage. */
static void
spawnmodule(Modlink *ml, Inst *pc)
{
	SpawnProg c;

	memset(&c, 0, sizeof(c));
	c.module = ml;
	c.pc = pc;
	if(!kprotect(spawnready, &c)){
		free(c.initial.R.EX);
		krethrow();
	}
	unframe();
}

/* Spawn only after the frame and process can both be constructed. */
OP(spawn)
{
	spawnmodule(R.M, *(Inst**)R.d);
}

/* Check the full link index before reading a child entry PC. */
OP(mspawn)
{
	Modlink *ml;
	Modl entry;

	ml = *(Modlink**)R.d;
	if(ml == H)
		error(exModule);
	if(ml->prog == nil)
		error(exSpawn);
	if(!moduleentry(ml, W(m), &entry))
		error("invalid mspawn");
	spawnmodule(ml, entry.u.pc);
}
OP(ret)
{
	Frame *f;
	Modlink *m;

	f = (Frame*)R.FP;
	R.FP = f->fp;
	if(R.FP == nil) {
		R.FP = (uchar*)f;
		error("");
	}
	R.SP = (uchar*)f;
	R.PC = f->lr;
	m = f->mr;

	if(f->t == nil)
		unextend(f);
	else if (f->t->np)
		freeptrs(f, f->t);

	if(m != nil) {
		if(R.M->compiled != m->compiled) {
			R.IC = 1;
			R.t = 1;
		}
		destroy(R.M);
		R.M = m;
		R.MP = m->MP;
	}
}
OP(iload)
{
	char *n;
	Import *ldt;
	Module *m;
	Modlink *ml, **mp, *t;
	Heap *h;

	n = string2c(S(s));
	m = R.M->m;
	if(m->rt & HASLDT)
		ldt = m->ldt[W(m)];
	else{
		ldt = nil;
		error("obsolete dis");
	}

	if(strcmp(n, "$self") == 0) {
		m->ref++;
		ml = linkmod(m, ldt, 0);
		if(ml != H) {
			ml->MP = R.M->MP;
			h = D2H(ml->MP);
			h->ref++;
			Setmark(h);
		}
	}
	else {
		m = readmod(n, lookmod(n), 1);
		ml = linkmod(m, ldt, 1);
	}

	mp = R.d;
	t = *mp;
	*mp = ml;
	destroy(t);
}
/* Resolve the complete link index before publishing a frame or changing modules. */
OP(mcall)
{
	Heap *h;
	Prog *p;
	Frame *f;
	Modl entry;
	Modlink *ml;

	ml = *(Modlink**)R.d;
	if(ml == H)
		error(exModule);
	if(!moduleentry(ml, W(m), &entry))
		error("invalid mcall");
	f = T(s);
	f->lr = R.PC;
	f->fp = R.FP;
	f->mr = R.M;

	R.FP = (uchar*)f;
	R.M = ml;
	h = D2H(ml);
	h->ref++;

	if(ml->prog == nil) {
		entry.u.runt(f);
		h->ref--;
		R.M = f->mr;
		R.SP = R.FP;
		R.FP = f->fp;
		if(f->t == nil)
			unextend(f);
		else if (f->t->np)
			freeptrs(f, f->t);
		p = currun();
		if(p->kill != nil)
			error(p->kill);
		R.t = 0;
		return;
	}
	R.MP = R.M->MP;
	R.PC = entry.u.pc;
	R.t = 1;

	if(f->mr->compiled != R.M->compiled)
		R.IC = 1;
}
OP(lena)
{
	WORD l;
	Array *a;

	a = A(s);
	l = 0;
	if(a != H)
		l = a->len;
	W(d) = l;
}
OP(lenl)
{
	WORD l;
	List *a;

	a = L(s);
	l = 0;
	while(a != H) {
		l++;
		a = a->tail;
	}
	W(d) = l;
}
static int
cgetb(Channel *c, void *v)
{
	Array *a;
	void *w;

	if((a = c->buf) == H)
		return 0;
	if(c->size > 0){
		w = a->data+c->front*a->t->size;
		c->front++;
		if(c->front == c->buf->len)
			c->front = 0;
		c->size--;
		R.s = w;
		R.m = &c->mid;
		R.d = v;
		c->mover();
		if(a->t->np){
			freeptrs(w, a->t);
			initmem(a->t, w);
		}
		return 1;
	}
	return 0;
}
static int
cputb(Channel *c, void *v)
{
	Array *a;
	WORD len, r;

	if((a = c->buf) == H)
		return 0;
	len = c->buf->len;
	if(c->size < len){
		r = c->front+c->size;
		if(r >= len)
			r -= len;
		c->size++;
		R.s = v;
		R.m = &c->mid;
		R.d = a->data+r*a->t->size;
		c->mover();
		return 1;
	}
	return 0;
}
/*
int
cqsize(Progq *q)
{
	int n;

	n = 0;
	for( ; q != nil; q = q->next)
		if(q->prog != nil)
			n++;
	return n;
}
*/
void
cqadd(Progq **q, Prog *p)
{
	Progq *n;

	if((*q)->prog == nil){
		(*q)->prog = p;
		return;
	}
	n = (Progq*)malloc(sizeof(Progq));
	if(n == nil)
		error(exNomem);
	n->prog = p;
	n->next = nil;
	for( ; *q != nil; q = &(*q)->next)
		;
	*q = n;
}
void
cqdel(Progq **q)
{
	Progq *f;

	if((*q)->next == nil){
		(*q)->prog = nil;
		return;
	}
	f = *q;
	*q = f->next;
	free(f);
}
/* Remove every registration while retaining the queue's mandatory empty node. */
void
cqdelp(Progq **q, Prog *p)
{
	Progq *f;
	Progq **head;

	head = q;
	while(*q != nil){
		if((*q)->prog != p){
			q = &(*q)->next;
			continue;
		}
		if((*q)->next == nil){
			/* Only an entirely empty queue needs a placeholder node. */
			if(q == head)
				(*q)->prog = nil;
			else{
				free(*q);
				*q = nil;
			}
			return;
		}
		f = *q;
		*q = f->next;
		free(f);
	}
}
/* Queue a blocked sender before removing it from the runnable scheduler list. */
OP(isend)
{
	Channel *c;
 	Prog *p;

	c = C(d);
	if(c == H)
		error(exNilref);

	if((p = c->recv->prog) == nil) {
		if(c->buf != H && cputb(c, R.s))
			return;
		p = currun();
		cqadd(&c->send, p);
		p = delrun(Psend);
		p->ptr = R.s;
		p->chan = c;	/* for killprog */
		R.IC = 1;	
		R.t = 1;
		return;
	}

	if(c->buf != H && c->size > 0)
		print("non-empty buffer in isend\n");

	cqdel(&c->recv);
	if(p->state == Palt)
		altdone(p->R.s, p, c, 1);

	R.m = &c->mid;
	R.d = p->ptr;
	p->ptr = nil;
	c->mover();
	addrun(p);
	R.t = 0;
}
/* Queue a blocked receiver before changing its scheduler state. */
OP(irecv)
{
	Channel *c;
	Prog *p;

	c = C(s);
	if(c == H)
		error(exNilref);

	if((p = c->send->prog) == nil) {
		if(c->buf != H && cgetb(c, R.d))
			return;
		p = currun();
		cqadd(&c->recv, p);
		p = delrun(Precv);
		p->ptr = R.d;
		p->chan = c;	/* for killprog */
		R.IC = 1;
		R.t = 1;
		return;
	}

	if(c->buf != H && c->size != c->buf->len)
		print("non-full buffer in irecv\n");

	cqdel(&c->send);
	if(p->state == Palt)
		altdone(p->R.s, p, c, 0);

	if(c->buf != H){
		cgetb(c, R.d);
		cputb(c, p->ptr);
		p->ptr = nil;
	}
	else{
		R.m = &c->mid;
		R.s = p->ptr;
		p->ptr = nil;
		c->mover();
	}
	addrun(p);
	R.t = 0;
}
int
csendalt(Channel *c, void *ip, Type *t, int len)
{
	REG rsav;

	if(c == H)
		error(exNilref);

	if(c->recv->prog == nil && (c->buf == H || c->size == c->buf->len)){
		if(c->buf != H){
			print("csendalt failed\n");
			freeptrs(ip, t);
			return 0;
		}
		c->buf = H2D(Array*, heaparray(t, len));
	}

	rsav = R;
	R.s = ip;
	R.d = &c;
	isend();
	R = rsav;
	freeptrs(ip, t);
	return 1;
}

List*
cons(ulong size, List **lp)
{
	Heap *h;
	List *lv, *l;

	if(size > 0x7fffffff-sizeof(Heap)-
	   (sizeof(List)-sizeof(((List*)0)->data)))
		error(exHeap);
	h = nheap(sizeof(List) + size - sizeof(((List*)0)->data));
	h->t = &Tlist;
	Tlist.ref++;
	l = H2D(List*, h);
	l->t = nil;

	lv = *lp;
	if(lv != H) {
		h = D2H(lv);
		Setmark(h);
	}
	l->tail = lv;
	*lp = l;
	return l;
}

/* Capture source values before publishing a node into a possibly aliased slot. */
OP(consb)
{
	BYTE value;
	List *l;

	value = B(s);
	l = cons(IBY2WD, R.d);
	*(BYTE*)l->data = value;
}

/* Host WORD storage must fit the complete interpreter value. */
OP(consw)
{
	WORD value;
	List *l;

	value = W(s);
	l = cons(sizeof(value), R.d);
	*(WORD*)l->data = value;
}

/* Retain the source big integer if publishing the list changes its source slot. */
OP(consl)
{
	LONG value;
	List *l;

	value = V(s);
	l = cons(sizeof(value), R.d);
	*(LONG*)l->data = value;
}

/* Allocate native pointer storage and acquire the original source owner. */
OP(consp)
{
	List *l;
	Heap *h;
	WORD *sv;

	sv = P(s);
	l = cons(sizeof(sv), R.d);
	if(sv != H){
		h = D2H(sv);
		h->ref++;
		Setmark(h);
	}
	l->t = &Tptr;
	Tptr.ref++;
	*(WORD**)l->data = sv;
}

/* Capture the real argument before the destination slot becomes a list owner. */
OP(consf)
{
	REAL value;
	List *l;

	value = F(s);
	l = cons(sizeof(value), R.d);
	*(REAL*)l->data = value;
}

/* Check the complete VM size and copy raw payload before replacing its source slot. */
OP(consm)
{
	WORD size;
	List *l, *result;

	size = W(m);
	if(size < 0 || (uvlong)size > 0x7fffffff-sizeof(Heap)-
	   (sizeof(List)-sizeof(((List*)0)->data)))
		error(exHeap);
	result = L(d);
	l = cons((ulong)size, &result);
	if(size != 0)
		memmove(l->data, R.s, (int)size);
	L(d) = result;
}

/* Copy mapped payload and acquire its references before publishing an aliased result. */
OP(consmp)
{
	List *l, *result;
	Type *t;

	t = moduletype(R.M, W(m));
	result = L(d);
	l = cons(t->size, &result);
	incmem(R.s, t);
	if(t->size != 0)
		memmove(l->data, R.s, t->size);
	l->t = t;
	t->ref++;
	L(d) = result;
}
OP(headb)
{
	List *l;

	l = L(s);
	B(d) = *(BYTE*)l->data;
}
OP(headw)
{
	List *l;

	l = L(s);
	W(d) = *(WORD*)l->data;
}
OP(headl)
{
	List *l;

	l = L(s);
	V(d) = *(LONG*)l->data;
}
OP(headp)
{
	List *l;

	l = L(s);
	R.s = l->data;
	movp();
}
OP(headf)
{
	List *l;

	l = L(s);
	F(d) = *(REAL*)l->data;
}
OP(headm)
{
	List *l;

	l = L(s);
	memmove(R.d, l->data, W(m));
}
OP(headmp)
{
	List *l;

	l = L(s);
	R.s = l->data;
	movmp();
}
OP(tail)
{
	List *l;

	l = L(s);
	R.s = &l->tail;
	movp();
}
OP(slicea)
{
	Type *t;
	Heap *h;
	Array *at, *ss, *ds;
	WORD v, n, start;

	v = W(m);
	start = W(s);
	ds = A(d);

	if(ds == H) {
		if(v == start)
			return;
		error(exNilref);
	}
	if(start < 0 || v < start || v > ds->len)
		error(exBounds);
	n = v-start;

	t = ds->t;
	h = heap(&Tarray);
	ss = H2D(Array*, h);
	ss->len = n;
	ss->data = ds->data + start*t->size;
	ss->t = t;
	t->ref++;

	if(ds->root != H) {			/* slicing a slice */
		ds = ds->root;
		h = D2H(ds);
		h->ref++;
		at = A(d);
		A(d) = ss;
		ss->root = ds;
		destroy(at);
	}
	else {
		h = D2H(ds);
		ss->root = ds;
		A(d) = ss;
	}
	Setmark(h);
}
/* Validate even empty copies, then avoid touching their possibly empty data spans. */
OP(slicela)
{
	Type *t;
	int l;
	WORD dl;
	Array *ss, *ds;
	uchar *sp, *dp, *ep;

	ss = A(s);
	dl = W(m);
	ds = A(d);
	if(ss == H)
		return;
	if(ds == H)
		error(exNilref);
	if(dl < 0 || dl > ds->len || ss->len < 0 || ss->len > ds->len-dl)
		error(exBounds);
	if(ss->len == 0)
		return;

	t = ds->t;
	if(t->np == 0) {
		memmove(ds->data+dl*t->size, ss->data, ss->len*t->size);
		return;
	}
	sp = ss->data;
	dp = ds->data+dl*t->size;

	if(dp > sp) {
		l = ss->len * t->size;
		sp = ss->data + l;
		ep = dp + l;
		while(ep > dp) {
			ep -= t->size;
			sp -= t->size;
			incmem(sp, t);
			if (t->np)
				freeptrs(ep, t);
		}
	}
	else {
		ep = dp + ss->len*t->size;
		while(dp < ep) {
			incmem(sp, t);
			if (t->np)
				freeptrs(dp, t);
			dp += t->size;
			sp += t->size;
		}
	}
	memmove(ds->data+dl*t->size, ss->data, ss->len*t->size);
}
OP(alt)
{
	R.t = 0;
	xecalt(1);
}
OP(nbalt)
{
	xecalt(0);
}
OP(tcmp)
{
	void *s, *d;

	s = T(s);
	d = T(d);
	if(s != H && (d == H || D2H(s)->t != D2H(d)->t))
		error(exTcheck);
}
OP(eclr)
{
	/* spare slot */
}
OP(badop)
{
	error(exOp);
}
OP(iraise)
{
	void *v;
	Heap *h;
	Prog *p;

	p = currun();
	v = T(s);
	if(v == H)
		error(exNilref);
	p->exval = v;
	h = D2H(v);
	h->ref++;
	if(h->t == &Tstring)
		error(string2c((String*)v));
	else
		error(string2c(*(String**)v));
}
/* Shift a fixed intermediate with bounded counts and unsigned left-shift bits. */
static LONG
fixedshift(LONG value, WORD count)
{
	UWORD amount;

	if(count >= 0){
		if((UWORD)count > 63)
			error(exInval);
		return (LONG)((uvlong)value<<count);
	}
	amount = (UWORD)0-(UWORD)count;
	if(amount > 63)
		error(exInval);
	return value>>amount;
}

/* Divide without a host zero fault or signed minimum/-1 overflow. */
static LONG
fixeddivide(LONG value, LONG divisor)
{
	if(divisor == 0)
		error(exZdiv);
	if(divisor == -1)
		return (LONG)((uvlong)0-(uvlong)value);
	return value/divisor;
}

/* Construct a right-shift rounding mask only for a valid negative exponent. */
static LONG
fixedroundmask(WORD count)
{
	UWORD amount;

	amount = (UWORD)0-(UWORD)count;
	if(count >= 0 || amount > 63)
		error(exInval);
	return (LONG)(((uvlong)1<<amount)-1);
}

/* Multiply and shift fixed values using defined intermediate bit arithmetic. */
OP(mulx)
{
	WORD p;
	LONG r;

	p = Dtmp;
	r = (LONG)((uvlong)(LONG)W(m)*(uvlong)(LONG)W(s));
	r = fixedshift(r, p);
	W(d) = (WORD)r;
}

/* Shift and divide a fixed numerator with VM errors on invalid parameters. */
OP(divx)
{
	WORD p;
	LONG s;

	p = Dtmp;
	if(W(s) == 0)
		error(exZdiv);
	s = (LONG)W(m);
	s = fixedshift(s, p);
	s = fixeddivide(s, (LONG)W(s));
	W(d) = (WORD)s;
}

/* Convert fixed scales without signed left-shift undefined behavior. */
OP(cvtxx)
{
	WORD p;
	LONG r;

	p = W(m);
	r = (LONG)W(s);
	r = fixedshift(r, p);
	W(d) = (WORD)r;
}

/* Validate the coefficient before a zero shortcut and stage the product. */
OP(mulx0)
{
	WORD x, y, p, a;
	LONG r;

	x = W(m);
	y = W(s);
	p = Dtmp;
	a = Stmp;
	if(a == 0)
		error(exZdiv);
	if(x == 0 || y == 0){
		W(d) = 0;
		return;
	}
	r = (LONG)((uvlong)(LONG)x*(uvlong)(LONG)y);
	r = fixedshift(r, p);
	r = fixeddivide(r, (LONG)a);
	W(d) = (WORD)r;
}

/* Reject zero divisors even when the fixed numerator is zero. */
OP(divx0)
{
	WORD x, y, p, b;
	LONG s;

	x = W(m);
	y = W(s);
	p = Dtmp;
	b = Stmp;
	if(y == 0)
		error(exZdiv);
	if(x == 0){
		W(d) = 0;
		return;
	}
	s = (LONG)((uvlong)(LONG)b*(uvlong)(LONG)x);
	s = fixedshift(s, p);
	s = fixeddivide(s, (LONG)y);
	W(d) = (WORD)s;
}

/* Scale a fixed value before publishing the divided result. */
OP(cvtxx0)
{
	WORD x, p, a;
	LONG r;

	x = W(s);
	p = W(m);
	a = Stmp;
	if(a == 0)
		error(exZdiv);
	if(x == 0){
		W(d) = 0;
		return;
	}
	r = (LONG)x;
	r = fixedshift(r, p);
	r = fixeddivide(r, (LONG)a);
	W(d) = (WORD)r;
}

/* Apply fixed multiplication rounding with bounded masks and modular corrections. */
OP(mulx1)
{
	WORD x, y, p, a, v;
	int vnz, wnz;
	LONG w, r;

	x = W(m);
	y = W(s);
	p = Dtmp;
	a = Stmp;
	if(a == 0)
		error(exZdiv);
	if(x == 0 || y == 0){
		W(d) = 0;
		return;
	}
	vnz = p&2;
	wnz = p&1;
	p >>= 2;
	v = 0;
	w = 0;
	if(vnz){
		v = (WORD)((UWORD)a-1);
		if(x >= 0 && y < 0 || x < 0 && y >= 0)
			v = (WORD)((UWORD)0-(UWORD)v);
	}
	if(wnz){
		if((!vnz && (x > 0 && y < 0 || x < 0 && y > 0)) ||
		(vnz && (x > 0 && y > 0 || x < 0 && y < 0)))
			w = fixedroundmask(p);
	}
	r = (LONG)((uvlong)(LONG)x*(uvlong)(LONG)y+(uvlong)w);
	r = fixedshift(r, p);
	r = (LONG)((uvlong)r+(uvlong)(LONG)v);
	r = fixeddivide(r, (LONG)a);
	W(d) = (WORD)r;
}

/* Apply fixed division rounding without signed intermediate overflow. */
OP(divx1)
{
	WORD x, y, p, b, v;
	int vnz, wnz;
	LONG w, s;

	x = W(m);
	y = W(s);
	p = Dtmp;
	b = Stmp;
	if(y == 0)
		error(exZdiv);
	if(x == 0){
		W(d) = 0;
		return;
	}
	vnz = p&2;
	wnz = p&1;
	p >>= 2;
	v = 0;
	w = 0;
	if(vnz){
		v = 1;
		if(x >= 0 && y < 0 || x < 0 && y >= 0)
			v = (WORD)((UWORD)0-(UWORD)v);
	}
	if(wnz){
		if(x <= 0)
			w = fixedroundmask(p);
	}
	s = (LONG)((uvlong)(LONG)b*(uvlong)(LONG)x+(uvlong)w);
	s = fixedshift(s, p);
	s = fixeddivide(s, (LONG)y);
	W(d) = (WORD)((UWORD)s+(UWORD)v);
}

/* Round and scale a fixed conversion without invalid native shifts. */
OP(cvtxx1)
{
	WORD x, p, a, v;
	int vnz, wnz;
	LONG w, r;

	x = W(s);
	p = W(m);
	a = Stmp;
	if(a == 0)
		error(exZdiv);
	if(x == 0){
		W(d) = 0;
		return;
	}
	vnz = p&2;
	wnz = p&1;
	p >>= 2;
	v = 0;
	w = 0;
	if(vnz){
		v = (WORD)((UWORD)a-1);
		if(x < 0)
			v = (WORD)((UWORD)0-(UWORD)v);
	}
	if(wnz){
		if(!vnz && x < 0 || vnz && x > 0)
			w = fixedroundmask(p);
	}
	r = (LONG)((uvlong)(LONG)x+(uvlong)w);
	r = fixedshift(r, p);
	r = (LONG)((uvlong)r+(uvlong)(LONG)v);
	r = fixeddivide(r, (LONG)a);
	W(d) = (WORD)r;
}
/*
OP(cvtxx)
{
	REAL v;

	v = (REAL)W(s)*F(m);
	v = v < 0 ? v-0.5: v+0.5;
	W(d) = (WORD)v;
}
*/
/* Reject exceptional real-to-fixed casts before a native integer conversion. */
OP(cvtfx)
{
	REAL v;

	v = F(s)*F(m);
	v = v < 0 ? v-0.5: v+0.5;
	if(sizeof(WORD) == 4){
		if(!(v > -2147483649.0 && v < 2147483648.0))
			error(exInval);
	}else{
		if(!(v >= -9223372036854775808.0 && v < 9223372036854775808.0))
			error(exInval);
	}
	W(d) = (WORD)v;
}
OP(cvtxf)
{
	F(d) = (REAL)W(s)*F(m);
}

OP(self)
{
	Modlink *ml, **mp, *t;
	Heap *h;

	ml = R.M;
	h = D2H(ml);
	h->ref++;
	Setmark(h);
	mp = R.d;
	t = *mp;
	*mp = ml;
	destroy(t);
}

void
destroystack(REG *reg)
{
	Type *t;
	Frame *f, *fp;
	Modlink *m;
	Stkext *sx;
	uchar *ex;

	ex = reg->EX;
	reg->EX = nil;
	while(ex != nil) {
		sx = (Stkext*)ex;
		fp = sx->reg.tos.fr;
		do {
			f = (Frame*)reg->FP;
			if(f == nil)
				break;
			reg->FP = f->fp;
			t = f->t;
			if(t == nil)
				t = sx->reg.TR;
			m = f->mr;
			if (t->np)
				freeptrs(f, t);
			if(m != nil) {
				destroy(reg->M);
				reg->M = m;
			}
		} while(f != fp);
		ex = sx->reg.EX;
		free(sx);
	}
	destroy(reg->M);
	reg->M = H;	/* for devprof */
}

Prog*
isave(void)
{
	Prog *p;

	p = delrun(Prelease);
	p->R = R;
	return p;
}

void
irestore(Prog *p)
{
	R = p->R;
	R.IC = 1;
}

void
movtmp(void)		/* Used by send & receive */
{
	Type *t;

	t = (Type*)W(m);

	incmem(R.s, t);
	if (t->np)
		freeptrs(R.d, t);
	memmove(R.d, R.s, t->size);
}

extern OP(cvtca);
extern OP(cvtac);
extern OP(cvtwc);
extern OP(cvtcw);
extern OP(cvtfc);
extern OP(cvtcf);
extern OP(insc);
extern OP(indc);
extern OP(addc);
extern OP(lenc);
extern OP(slicec);
extern OP(cvtlc);

#include "optab.h"

void
opinit(void)
{
	int i;

	for(i = 0; i < 256; i++)
		if(optab[i] == nil)
			optab[i] = badop;
}

void
xec(Prog *p)
{
	int op;
	Jit *j;

	R = p->R;
	R.MP = R.M->MP;
	R.IC = p->quanta;

	if(p->kill != nil) {
		char *m;
		m = p->kill;
		p->kill = nil;
		error(m);
	}

// print("%lux %lux %lux %lux %lux\n", (ulong)&R, R.xpc, R.FP, R.MP, R.PC);

	if(R.M->compiled)
		comvec();
	else do {
		j = R.M->m->jit;
		dec[R.PC->add]();
		op = R.PC->op;
		R.PC++;
		if(j != nil && j->entry[op] != nil) {
			if(j->enter != nil)
				j->enter();
			if(!j->entry[op](&R))
				optab[op]();
		}else
			optab[op]();
	} while(--R.IC != 0);

	p->R = R;
}
