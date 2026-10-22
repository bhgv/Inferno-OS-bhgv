#include "lib9.h"
#include "kernel.h"
#include <isa.h>
#include "interp.h"
#include "runt.h"
#include <mp.h>
#include <libsec.h>
#include "pool.h"
#include "ipint.h"
#include "raise.h"

#include "ipintsmod.h"

/* infinite precision integer */
struct IPint
{
	IPints_IPint x;
	mpint*	b;
};

Type	*TIPint;
static uchar IPintmap[] = IPints_IPint_map;

#define	MP(x)	checkIPint((x))

void
ipintsmodinit(void)
{
	/* can be called from modinit, Keyring or Crypt */
	if(TIPint == nil)
		TIPint = dtype(freeIPint, sizeof(IPint), IPintmap, sizeof(IPintmap));
	builtinmod("$IPints", IPintsmodtab, IPintsmodlen);
}

typedef struct IPintBuild IPintBuild;
struct IPintBuild
{
	mpint *number;
	IPint *result;
};

/* Attach an owned magnitude only after its VM heap object exists. */
static int
ipintbuild(void *arg)
{
	IPintBuild *build;

	build = arg;
	build->result = H2D(IPint*, heap(TIPint));
	build->result->b = build->number;
	return 1;
}

/* Consume the supplied magnitude on both success and VM allocation failure. */
void*
newIPint(mpint *number)
{
	IPintBuild build;

	if(number == nil)
		error(exHeap);
	build.number = number;
	build.result = nil;
	if(!kprotect(ipintbuild, &build)){
		mpfree(number);
		krethrow();
	}
	return (IPints_IPint*)build.result;
}

mpint*
checkIPint(void *a)
{
	IPints_IPint *v;
	IPint *ip;

	v = a;
	ip = (IPint*)v;
	if(ip == H || ip == nil)
		error(exNilref);
	if(D2H(ip)->t != TIPint)
		error(exType);
	return ip->b;	/* non-nil by construction */
}

void
freeIPint(Heap *h, int swept)
{
	IPint *ip;

	USED(swept);
	ip = H2D(IPint*, h);
	if(ip->b)
		mpfree(ip->b);
	freeheap(h, 0);
}

typedef struct IPintText IPintText;
struct IPintText
{
	mpint *number;
	int base;
	int leadingzero;
	uchar *bytes;
	char *text;
	String **result;
};

/* Bound the signed size arithmetic used by libmp's formatting helpers. */
static void
ipintexportsize(mpint *number)
{
	if(number->top < 0 ||
	   (uvlong)number->top > (0x7fffffff-Dbits-2)/Dbits)
		error(exHeap);
}

/* Build a dynamically sized encoding before publishing the VM string. */
static int
ipinttextbuild(void *arg)
{
	IPintText *build;
	int n, capacity;

	build = arg;
	if(build->leadingzero){
		n = mptobe(build->number, nil, 0, &build->bytes);
		if(n < 0)
			error(exHeap);
		if(n > 0 && (build->bytes[0]&0x80)){
			/* mptobe's extra digit leaves space for this compatibility byte. */
			memmove(build->bytes+1, build->bytes, n);
			build->bytes[0] = 0;
			n++;
		}
		capacity = 4*((n+2)/3)+1;
		build->text = malloc(capacity);
		if(build->text == nil)
			error(exHeap);
		if(enc64(build->text, capacity, build->bytes, n) != capacity-1)
			error(exHeap);
	}else{
		build->text = mptoa(build->number, build->base, nil, 0);
		if(build->text == nil)
			error(exHeap);
	}
	retstr(build->text, build->result);
	return 1;
}

/* Reclaim native buffers after either a successful export or a VM exception. */
static void
ipinttext(mpint *number, int base, int leadingzero, String **result)
{
	IPintText build;
	int ok;

	ipintexportsize(number);
	build.number = number;
	build.base = base;
	build.leadingzero = leadingzero;
	build.bytes = nil;
	build.text = nil;
	build.result = result;
	ok = kprotect(ipinttextbuild, &build);
	free(build.bytes);
	free(build.text);
	if(!ok)
		krethrow();
}

/* Export the magnitude directly into an exactly sized VM byte array. */
static void
ipintbytes(mpint *number, Array **result)
{
	Array *bytes, *old;
	int bits, n;

	ipintexportsize(number);
	bits = mpsignif(number);
	n = bits/8 + (bits%8 != 0);
	if(n == 0)
		n = 1;
	bytes = mem2array(nil, n);
	if(mptobe(number, bytes->data, n, nil) != n){
		destroy(bytes);
		error(exHeap);
	}
	old = *result;
	*result = bytes;
	destroy(old);
}

/* Retain the legacy unsigned base64 representation with a leading zero byte. */
void
IPint_iptob64z(void *fp)
{
	F_IPint_iptob64z *f;

	f = fp;
	ipinttext(MP(f->i), 64, 1, f->ret);
}

/* Export base64 without a fixed stack-buffer limit. */
void
IPint_iptob64(void *fp)
{
	F_IPint_iptob64 *f;

	f = fp;
	ipinttext(MP(f->i), 64, 0, f->ret);
}

/* Preserve the historical magnitude-only byte representation. */
void
IPint_iptobytes(void *fp)
{
	F_IPint_iptobytes *f;

	f = fp;
	ipintbytes(MP(f->i), f->ret);
}

/* Export the complete big-endian magnitude, including values over 1024 bytes. */
void
IPint_iptobebytes(void *fp)
{
	F_IPint_iptobebytes *f;

	f = fp;
	ipintbytes(MP(f->i), f->ret);
}

/* Format a number before replacing the caller's previous string result. */
void
IPint_iptostr(void *fp)
{
	F_IPint_iptostr *f;

	f = fp;
	ipinttext(MP(f->i), f->base, 0, f->ret);
}

static IPints_IPint*
strtoipint(String *s, int base)
{
	char *p, *q;
	mpint *b;

	p = string2c(s);
	b = strtomp(p, &q, base, nil);
	if(b == nil)
		return H;
	while(*q == '=')
		q++;
	if(q == p || *q != 0){
		mpfree(b);
		return H;
	}
	return newIPint(b);
}

void
IPint_b64toip(void *fp)
{
	F_IPint_b64toip *f;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	*f->ret = strtoipint(f->str, 64);
}

void
IPint_bytestoip(void *fp)
{
	F_IPint_bytestoip *f;
	mpint *b;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	if(f->buf == H)
		error(exNilref);

	b = betomp(f->buf->data, f->buf->len, nil);	/* for now we'll ignore sign */
	*f->ret = newIPint(b);
}

void
IPint_bebytestoip(void *fp)
{
	F_IPint_bebytestoip *f;
	mpint *b;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	if(f->mag == H)
		error(exNilref);

	b = betomp(f->mag->data, f->mag->len, nil);
	*f->ret = newIPint(b);
}

void
IPint_strtoip(void *fp)
{
	F_IPint_strtoip *f;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	*f->ret = strtoipint(f->str, f->base);
}

/* create a random integer */
void
IPint_random(void *fp)
{
	F_IPint_random *f;
	mpint *b;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	release();
	b = mprand(f->nbits, genrandom, nil);
	acquire();
	*f->ret = newIPint(b);
}

/* number of bits in number */
void
IPint_bits(void *fp)
{
	F_IPint_bits *f;
	int n;

	f = fp;
	*f->ret = 0;
	if(f->i == H)
		return;

	n = mpsignif(MP(f->i));
	if(n == 0)
		n = 1;	/* compatibility */
	*f->ret = n;
}

/* create a new IP from an int */
void
IPint_inttoip(void *fp)
{
	F_IPint_inttoip *f;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	*f->ret = newIPint(itomp(f->i, nil));
}

void
IPint_iptoint(void *fp)
{
	F_IPint_iptoint *f;

	f = fp;
	*f->ret = 0;
	if(f->i == H)
		return;
	*f->ret = mptoi(MP(f->i));
}

/* Validate modular powers before entering native fatal-error paths. */
void
IPint_expmod(void *fp)
{
	F_IPint_expmod *f;
	mpint *number, *mod, *base, *exp;
	IPints_IPint *result, *old;

	f = fp;
	base = MP(f->base);
	exp = MP(f->exp);
	if(exp->top != 0 && exp->sign < 0)
		error(exInval);
	mod = nil;
	if(f->mod != H){
		mod = MP(f->mod);
		if(mod->top == 0)
			error(exZdiv);
		if(mod->sign < 0)
			error(exInval);
	}
	number = mpnew(0);
	if(number == nil)
		error(exHeap);
	mpexp(base, exp, mod, number);
	result = newIPint(number);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* multiplicative inverse */
void
IPint_invert(void *fp)
{
	F_IPint_invert *f;
	mpint *ret;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	ret = mpnew(0);
	if(ret != nil)
		mpinvert(MP(f->base), MP(f->mod), ret);
	*f->ret = newIPint(ret);
}

/* basic math */
void
IPint_add(void *fp)
{
	F_IPint_add *f;
	mpint *i1, *i2, *ret;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	i1 = MP(f->i1);
	i2 = MP(f->i2);
	ret = mpnew(0);
	if(ret != nil)
		mpadd(i1, i2, ret);

	*f->ret = newIPint(ret);
}
void
IPint_sub(void *fp)
{
	F_IPint_sub *f;
	mpint *i1, *i2, *ret;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	i1 = MP(f->i1);
	i2 = MP(f->i2);
	ret = mpnew(0);
	if(ret != nil)
		mpsub(i1, i2, ret);

	*f->ret = newIPint(ret);
}
void
IPint_mul(void *fp)
{
	F_IPint_mul *f;
	mpint *i1, *i2, *ret;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	i1 = MP(f->i1);
	i2 = MP(f->i2);
	ret = mpnew(0);
	if(ret != nil)
		mpmul(i1, i2, ret);

	*f->ret = newIPint(ret);
}
typedef struct IPintDivide IPintDivide;
struct IPintDivide
{
	mpint *dividend;
	mpint *divisor;
	mpint *quotient;
	mpint *remainder;
	IPints_IPint *q;
	IPints_IPint *r;
};

/* Stage both results so any VM allocation failure can release the whole pair. */
static int
ipintdivide(void *arg)
{
	IPintDivide *divide;
	mpint *number;

	divide = arg;
	divide->quotient = mpnew(0);
	if(divide->quotient == nil)
		error(exHeap);
	divide->remainder = mpnew(0);
	if(divide->remainder == nil)
		error(exHeap);
	mpdiv(divide->dividend, divide->divisor, divide->quotient, divide->remainder);
	/* newIPint consumes its argument even when its VM allocation fails. */
	number = divide->quotient;
	divide->quotient = nil;
	divide->q = newIPint(number);
	number = divide->remainder;
	divide->remainder = nil;
	divide->r = newIPint(number);
	return 1;
}

/* Reject zero divisors in the VM and publish quotient/remainder atomically. */
void
IPint_div(void *fp)
{
	F_IPint_div *f;
	IPintDivide divide;
	IPints_IPint *oldq, *oldr;

	f = fp;
	divide.dividend = MP(f->i1);
	divide.divisor = MP(f->i2);
	if(divide.divisor->top == 0)
		error(exZdiv);
	divide.quotient = nil;
	divide.remainder = nil;
	divide.q = H;
	divide.r = H;
	if(!kprotect(ipintdivide, &divide)){
		mpfree(divide.quotient);
		mpfree(divide.remainder);
		destroy(divide.q);
		destroy(divide.r);
		krethrow();
	}
	oldq = f->ret->t0;
	oldr = f->ret->t1;
	f->ret->t0 = divide.q;
	f->ret->t1 = divide.r;
	destroy(oldq);
	destroy(oldr);
}

/* Validate the modulus and finish allocation before replacing the old result. */
void
IPint_mod(void *fp)
{
	F_IPint_mod *f;
	mpint *i1, *i2, *number;
	IPints_IPint *result, *old;

	f = fp;
	i1 = MP(f->i1);
	i2 = MP(f->i2);
	if(i2->top == 0)
		error(exZdiv);
	number = mpnew(0);
	if(number == nil)
		error(exHeap);
	mpmod(i1, i2, number);
	result = newIPint(number);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

void
IPint_neg(void *fp)
{
	F_IPint_neg *f;
	mpint *ret;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	ret = mpcopy(MP(f->i));
	if(ret == nil)
		error(exHeap);
	ret->sign = -ret->sign;

	*f->ret = newIPint(ret);
}

/* copy */
void
IPint_copy(void *fp)
{
	F_IPint_copy *f;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	*f->ret = newIPint(mpcopy(MP(f->i)));
}


/* equality */
void
IPint_eq(void *fp)
{
	F_IPint_eq *f;

	f = fp;
	*f->ret = mpcmp(MP(f->i1), MP(f->i2)) == 0;
}

/* compare */
void
IPint_cmp(void *fp)
{
	F_IPint_eq *f;

	f = fp;
	*f->ret = mpcmp(MP(f->i1), MP(f->i2));
}

/* Bound and direct a full VM-word count before narrowing to libmp's int. */
static void
ipintshift(mpint *number, WORD count, int left, IPints_IPint **result)
{
	UWORD amount;
	uvlong bits, limit;
	mpint *shifted;
	IPints_IPint *value, *old;

	amount = count;
	if(count < 0){
		amount = (UWORD)0-amount;
		left = !left;
	}
	limit = 0x7fffffff-(Dbits-1);
	bits = (uvlong)number->top*Dbits;
	if(number->top < 0 || bits > limit)
		error(exHeap);
	if(left && bits != 0 && (uvlong)amount > limit-bits)
		error(exHeap);

	shifted = mpnew(0);
	if(shifted == nil)
		error(exHeap);
	if(bits == 0 || (!left && (uvlong)amount >= bits))
		mpassign(mpzero, shifted);
	else if(left)
		mpleft(number, (int)amount, shifted);
	else
		mpright(number, (int)amount, shifted);
	value = newIPint(shifted);
	old = *result;
	*result = value;
	destroy(old);
}

/* Preserve the previous result when a left shift cannot be represented. */
void
IPint_shl(void *fp)
{
	F_IPint_shl *f;

	f = fp;
	ipintshift(MP(f->i), f->n, 1, f->ret);
}

/* Large right shifts produce zero without truncating the VM-word count. */
void
IPint_shr(void *fp)
{
	F_IPint_shr *f;

	f = fp;
	ipintshift(MP(f->i), f->n, 0, f->ret);
}

static void
mpand(mpint *b, mpint *m, mpint *res)
{
	int i;

	res->sign = b->sign;
	if(b->top == 0 || m->top == 0){
		res->top = 0;
		return;
	}
	mpbits(res, b->top*Dbits);
	res->top = b->top;
	for(i = b->top; --i >= 0;){
		if(i < m->top)
			res->p[i] = b->p[i] & m->p[i];
		else
			res->p[i] = 0;
	}
	mpnorm(res);
}

static void
mpor(mpint *b1, mpint *b2, mpint *res)
{
	mpint *t;
	int i;

	if(b2->top > b1->top){
		t = b1;
		b1 = b2;
		b2 = t;
	}
	if(b1->top == 0){
		mpassign(b2, res);
		return;
	}
	if(b2->top == 0){
		mpassign(b1, res);
		return;
	}
	mpassign(b1, res);
	for(i = b2->top; --i >= 0;)
		res->p[i] |= b2->p[i];
	mpnorm(res);
}

static void
mpxor(mpint *b1, mpint *b2, mpint *res)
{
	mpint *t;
	int i;

	if(b2->top > b1->top){
		t = b1;
		b1 = b2;
		b2 = t;
	}
	if(b1->top == 0){
		mpassign(b2, res);
		return;
	}
	if(b2->top == 0){
		mpassign(b1, res);
		return;
	}
	mpassign(b1, res);
	for(i = b2->top; --i >= 0;)
		res->p[i] ^= b2->p[i];
	mpnorm(res);
}

static void
mpnot(mpint *b1, mpint *res)
{
	int i;

	mpbits(res, Dbits*b1->top);
	res->sign = 1;
	res->top = b1->top;
	for(i = res->top; --i >= 0;)
		res->p[i] = ~b1->p[i];
	mpnorm(res);
}

/* bits */
void
IPint_and(void *fp)
{
	F_IPint_and *f;
	mpint *ret, *i1, *i2;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	i1 = MP(f->i1);
	i2 = MP(f->i2);
	ret = mpnew(0);
	if(ret != nil)
		mpand(i1, i2, ret);
	*f->ret = newIPint(ret);
}

void
IPint_ori(void *fp)
{
	F_IPint_ori *f;
	mpint *ret, *i1, *i2;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	i1 = MP(f->i1);
	i2 = MP(f->i2);
	ret = mpnew(0);
	if(ret != nil)
		mpor(i1, i2, ret);
	*f->ret = newIPint(ret);
}

void
IPint_xor(void *fp)
{
	F_IPint_xor *f;
	mpint *ret, *i1, *i2;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	i1 = MP(f->i1);
	i2 = MP(f->i2);
	ret = mpnew(0);
	if(ret != nil)
		mpxor(i1, i2, ret);
	*f->ret = newIPint(ret);
}

void
IPint_not(void *fp)
{
	F_IPint_not *f;
	mpint *ret, *i1;
	void *v;

	f = fp;
	v = *f->ret;
	*f->ret = H;
	destroy(v);

	i1 = MP(f->i1);
	ret = mpnew(0);
	if(ret != nil)
		mpnot(i1, ret);
	*f->ret = newIPint(ret);
}

/*
 * primes
 */

void
IPints_probably_prime(void *fp)
{
	F_IPints_probably_prime *f;

	f = fp;
	release();
	*f->ret = probably_prime(checkIPint(f->n), f->nrep);
	acquire();
}

void
IPints_genprime(void *fp)
{
	F_IPints_genprime *f;
	mpint *p;
	void *r;

	f = fp;
	r = *f->ret;
	*f->ret = H;
	destroy(r);
	p = mpnew(0);
	release();
	genprime(p, f->nbits, f->nrep);
	acquire();
	*f->ret = newIPint(p);
}

void
IPints_genstrongprime(void *fp)
{
	F_IPints_genstrongprime *f;
	mpint *p;
	void *r;

	f = fp;
	r = *f->ret;
	*f->ret = H;
	destroy(r);
	p = mpnew(0);
	release();
	genstrongprime(p, f->nbits, f->nrep);
	acquire();
	*f->ret = newIPint(p);
}

void
IPints_gensafeprime(void *fp)
{
	F_IPints_gensafeprime *f;
	mpint *p, *alpha;
	void *v;

	f = fp;
	v = f->ret->t0;
	f->ret->t0 = H;
	destroy(v);
	v = f->ret->t1;
	f->ret->t1 = H;
	destroy(v);

	p = mpnew(0);
	alpha = mpnew(0);
	release();
	gensafeprime(p, alpha, f->nbits, f->nrep);
	acquire();
	f->ret->t0 = newIPint(p);
	f->ret->t1 = newIPint(alpha);
}

void
IPints_DSAprimes(void *fp)
{
	F_IPints_DSAprimes *f;
	mpint *p, *q;
	Heap *h;
	void *v;

	f = fp;
	v = f->ret->t0;
	f->ret->t0 = H;
	destroy(v);
	v = f->ret->t1;
	f->ret->t1 = H;
	destroy(v);
	v = f->ret->t2;
	f->ret->t2 = H;
	destroy(v);

	h = heaparray(&Tbyte, SHA1dlen);
	f->ret->t2 = H2D(Array*, h);

	p = mpnew(0);
	q = mpnew(0);
	release();
	DSAprimes(q, p, f->ret->t2->data);
	acquire();
	f->ret->t0 = newIPint(q);
	f->ret->t1 = newIPint(p);
}
