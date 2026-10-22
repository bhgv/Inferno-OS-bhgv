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

/* Retain native base fallback rules without truncating a full VM word. */
static int
ipintbase(WORD base)
{
	if(base < (-0x7fffffff-1) || base > 0x7fffffff)
		error(exInval);
	return (int)base;
}

/* Format a number before replacing the caller's previous string result. */
void
IPint_iptostr(void *fp)
{
	F_IPint_iptostr *f;

	f = fp;
	ipinttext(MP(f->i), ipintbase(f->base), 0, f->ret);
}

/* Parse the whole VM string, rejecting embedded terminators and unrelated padding. */
static IPints_IPint*
strtoipint(String *s, int base)
{
	char *p, *q;
	mpint *b;
	int i, count;

	if(s == H)
		return H;
	if(s == nil)
		error(exNilref);
	if(D2H(s)->t != &Tstring)
		error(exType);
	if(s->len >= 0){
		if(memchr(s->Sascii, 0, s->len) != nil)
			return H;
	}else{
		if(s->len < -(0x7fffffff-UTFmax)/UTFmax)
			error(exHeap);
		count = -s->len;
		for(i = 0; i < count; i++)
			if(s->Srune[i] == 0)
				return H;
	}
	p = string2c(s);
	b = strtomp(p, &q, base, nil);
	if(b == nil)
		return H;
	if(base == 32 || base == 64)
		while(*q == '=')
			q++;
	if(q == p || *q != 0){
		mpfree(b);
		return H;
	}
	return newIPint(b);
}

/* Finish base64 parsing before replacing the caller's previous result. */
void
IPint_b64toip(void *fp)
{
	F_IPint_b64toip *f;
	IPints_IPint *result, *old;

	f = fp;
	result = strtoipint(f->str, 64);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* Validate full VM-array lengths before the native byte-count conversion. */
static void
ipintfrombytes(Array *bytes, IPints_IPint **result)
{
	mpint *number;
	IPints_IPint *value, *old;

	if(bytes == H || bytes == nil)
		error(exNilref);
	if(D2H(bytes)->t != &Tarray || bytes->t != &Tbyte)
		error(exType);
	if(bytes->len < 0 || (uvlong)bytes->len > (0x7fffffff-(Dbits-1))/8)
		error(exHeap);
	number = betomp(bytes->data, (uint)bytes->len, nil);
	value = newIPint(number);
	old = *result;
	*result = value;
	destroy(old);
}

/* Preserve the historical unsigned byte representation with staged ownership. */
void
IPint_bytestoip(void *fp)
{
	F_IPint_bytestoip *f;

	f = fp;
	ipintfrombytes(f->buf, f->ret);
}

/* Import an unsigned big-endian magnitude without narrowing unchecked lengths. */
void
IPint_bebytestoip(void *fp)
{
	F_IPint_bebytestoip *f;

	f = fp;
	ipintfrombytes(f->mag, f->ret);
}

/* Invalid text returns nil; VM errors leave the previous result untouched. */
void
IPint_strtoip(void *fp)
{
	F_IPint_strtoip *f;
	IPints_IPint *result, *old;

	f = fp;
	result = strtoipint(f->str, ipintbase(f->base));
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* Validate the VM-word size before narrowing or releasing the interpreter lock. */
void
IPint_random(void *fp)
{
	F_IPint_random *f;
	mpint *number;
	IPints_IPint *result, *old;

	f = fp;
	if(f->nbits < 0 || (uvlong)f->nbits > 0x7fffffff-(Dbits-1))
		error(exInval);
	release();
	number = mprand((int)f->nbits, genrandom, nil);
	acquire();
	result = newIPint(number);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* Validate before replacing the scalar result and keep the legacy zero width. */
void
IPint_bits(void *fp)
{
	F_IPint_bits *f;
	mpint *number;
	int bits;

	f = fp;
	if(f->i == H){
		*f->ret = 0;
		return;
	}
	number = MP(f->i);
	ipintexportsize(number);
	bits = mpsignif(number);
	*f->ret = bits == 0 ? 1 : bits;
}

/* Import the complete VM word rather than an implicitly narrowed C int. */
void
IPint_inttoip(void *fp)
{
	F_IPint_inttoip *f;
	IPints_IPint *result, *old;

	f = fp;
	result = newIPint(vtomp((vlong)f->i, nil));
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* Saturate to the active VM-word width and retain scalar output on validation errors. */
void
IPint_iptoint(void *fp)
{
	F_IPint_iptoint *f;
	mpint *number;
	WORD value;

	f = fp;
	if(f->i == H){
		*f->ret = 0;
		return;
	}
	number = MP(f->i);
	if(sizeof(WORD) == sizeof(vlong))
		value = mptov(number);
	else
		value = mptoi(number);
	*f->ret = value;
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

/* Reject noninvertible inputs as VM errors rather than native fatal errors. */
void
IPint_invert(void *fp)
{
	F_IPint_invert *f;
	mpint *input, *base, *mod, *gcd, *number, *other;
	IPints_IPint *result, *old;
	int invertible;

	f = fp;
	input = MP(f->base);
	mod = MP(f->mod);
	if(mod->top == 0)
		error(exZdiv);
	if(mod->sign < 0)
		error(exInval);
	/* Normalize negative bases before the nonnegative extended-GCD routine. */
	base = mpnew(0);
	gcd = mpnew(0);
	number = mpnew(0);
	other = mpnew(0);
	mpmod(input, mod, base);
	mpextendedgcd(base, mod, gcd, number, other);
	invertible = mpcmp(gcd, mpone) == 0;
	mpfree(base);
	mpfree(gcd);
	mpfree(other);
	if(!invertible){
		mpfree(number);
		error(exInval);
	}
	mpmod(number, mod, number);
	result = newIPint(number);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* Finish binary arithmetic before releasing a possibly aliased old result. */
static void
ipintbinary(IPints_IPint *a, IPints_IPint *b, IPints_IPint **result,
	void (*calculate)(mpint*, mpint*, mpint*))
{
	mpint *left, *right, *number;
	IPints_IPint *value, *old;

	left = MP(a);
	right = MP(b);
	number = mpnew(0);
	calculate(left, right, number);
	value = newIPint(number);
	old = *result;
	*result = value;
	destroy(old);
}

/* Add only after validating both operands and retain the result on failure. */
void
IPint_add(void *fp)
{
	F_IPint_add *f;

	f = fp;
	ipintbinary(f->i1, f->i2, f->ret, mpadd);
}

/* Subtract before replacing an output that can also be an input. */
void
IPint_sub(void *fp)
{
	F_IPint_sub *f;

	f = fp;
	ipintbinary(f->i1, f->i2, f->ret, mpsub);
}

/* Multiply with the same ownership and error guarantees as addition. */
void
IPint_mul(void *fp)
{
	F_IPint_mul *f;

	f = fp;
	ipintbinary(f->i1, f->i2, f->ret, mpmul);
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

/* Copy a magnitude before replacing its owner and keep zero nonnegative. */
static void
ipintcopy(IPints_IPint *input, IPints_IPint **result, int negate)
{
	mpint *number;
	IPints_IPint *value, *old;

	number = mpcopy(MP(input));
	if(number == nil)
		error(exHeap);
	if(negate && number->top != 0)
		number->sign = -number->sign;
	if(number->top == 0)
		number->sign = 1;
	value = newIPint(number);
	old = *result;
	*result = value;
	destroy(old);
}

/* Negate without freeing an aliased source or introducing negative zero. */
void
IPint_neg(void *fp)
{
	F_IPint_neg *f;

	f = fp;
	ipintcopy(f->i, f->ret, 1);
}

/* Publish an independent copy only after its VM allocation succeeds. */
void
IPint_copy(void *fp)
{
	F_IPint_copy *f;

	f = fp;
	ipintcopy(f->i, f->ret, 0);
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

/* Intersect magnitudes while retaining the historical left-operand sign. */
static void
mpand(mpint *b, mpint *m, mpint *res)
{
	int i;

	res->sign = b->sign;
	if(b->top == 0 || m->top == 0){
		res->top = 0;
		res->sign = 1;
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

/* Combine magnitudes using the existing larger-operand sign convention. */
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

/* XOR magnitudes and normalize complete cancellation to positive zero. */
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

/* Complement the stored limbs, preserving the existing finite-width syntax. */
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

/* Intersect only after both operands are validated and before replacing output. */
void
IPint_and(void *fp)
{
	F_IPint_and *f;

	f = fp;
	ipintbinary(f->i1, f->i2, f->ret, mpand);
}

/* Publish a bitwise union with the binary arithmetic ownership guarantees. */
void
IPint_ori(void *fp)
{
	F_IPint_ori *f;

	f = fp;
	ipintbinary(f->i1, f->i2, f->ret, mpor);
}

/* Retain both inputs until the XOR result has its VM heap object. */
void
IPint_xor(void *fp)
{
	F_IPint_xor *f;

	f = fp;
	ipintbinary(f->i1, f->i2, f->ret, mpxor);
}

/* Complete a finite-width complement before releasing an output-owned input. */
void
IPint_not(void *fp)
{
	F_IPint_not *f;
	mpint *input, *number;
	IPints_IPint *result, *old;

	f = fp;
	input = MP(f->i1);
	number = mpnew(0);
	mpnot(input, number);
	result = newIPint(number);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/*
 * primes
 */

/* Validate references and repetition counts before releasing the interpreter. */
void
IPints_probably_prime(void *fp)
{
	F_IPints_probably_prime *f;
	mpint *number;
	int repetitions, result;

	f = fp;
	number = MP(f->n);
	if(f->nrep > 0x7fffffff)
		error(exInval);
	repetitions = f->nrep <= 0 ? 18 : (int)f->nrep;
	release();
	result = probably_prime(number, repetitions);
	acquire();
	*f->ret = result;
}

/* Reject invalid sizes before native fatal paths and retain output on VM failure. */
void
IPints_genprime(void *fp)
{
	F_IPints_genprime *f;
	mpint *number;
	IPints_IPint *result, *old;
	int repetitions;

	f = fp;
	if(f->nbits < 2 || (uvlong)f->nbits > 0x7fffffff-(Dbits-1) ||
	   f->nrep > 0x7fffffff)
		error(exInval);
	repetitions = f->nrep <= 0 ? 18 : (int)f->nrep;
	number = mpnew(0);
	release();
	genprime(number, (int)f->nbits, repetitions);
	acquire();
	result = newIPint(number);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* Bound full-word arguments before native generation and retain output on failure. */
void
IPints_genstrongprime(void *fp)
{
	F_IPints_genstrongprime *f;
	mpint *number;
	IPints_IPint *result, *old;
	int bits, repetitions;

	f = fp;
	/* Gordon's algorithm historically rounds requests below 64 bits upward. */
	if(f->nbits < 0 || (uvlong)f->nbits > 0x7fffffff-2*Dbits ||
	   f->nrep > 0x7fffffff)
		error(exInval);
	bits = f->nbits < 64 ? 64 : (int)f->nbits;
	repetitions = f->nrep <= 0 ? 18 : (int)f->nrep;
	number = mpnew(0);
	release();
	genstrongprime(number, bits, repetitions);
	acquire();
	result = newIPint(number);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

typedef struct IPintPrimePair IPintPrimePair;
struct IPintPrimePair
{
	mpint *p;
	mpint *alpha;
	IPints_IPint *prime;
	IPints_IPint *generator;
};

/* Transfer each native magnitude to its VM owner, even on allocation failure. */
static int
ipintprimepair(void *arg)
{
	IPintPrimePair *pair;
	mpint *number;

	pair = arg;
	number = pair->p;
	pair->p = nil;
	pair->prime = newIPint(number);
	number = pair->alpha;
	pair->alpha = nil;
	pair->generator = newIPint(number);
	return 1;
}

/* Validate native sizes and publish the safe prime and generator atomically. */
void
IPints_gensafeprime(void *fp)
{
	F_IPints_gensafeprime *f;
	IPintPrimePair pair;
	IPints_IPint *oldp, *olda;
	int repetitions;

	f = fp;
	if(f->nbits < 3 || (uvlong)f->nbits > 0x7fffffff-(Dbits-1) ||
	   f->nrep > 0x7fffffff)
		error(exInval);
	repetitions = f->nrep <= 0 ? 18 : (int)f->nrep;
	pair.p = mpnew(0);
	pair.alpha = mpnew(0);
	pair.prime = H;
	pair.generator = H;
	release();
	gensafeprime(pair.p, pair.alpha, (int)f->nbits, repetitions);
	acquire();
	if(!kprotect(ipintprimepair, &pair)){
		mpfree(pair.p);
		mpfree(pair.alpha);
		destroy(pair.prime);
		destroy(pair.generator);
		krethrow();
	}
	oldp = f->ret->t0;
	olda = f->ret->t1;
	f->ret->t0 = pair.prime;
	f->ret->t1 = pair.generator;
	destroy(oldp);
	destroy(olda);
}

typedef struct IPintDSAPrimes IPintDSAPrimes;
struct IPintDSAPrimes
{
	IPintPrimePair pair;
	Array *seed;
	uchar bytes[SHA1dlen];
};

/* Build all three VM owners before publishing any part of the DSA result. */
static int
ipintdsaprimes(void *arg)
{
	IPintDSAPrimes *primes;

	primes = arg;
	ipintprimepair(&primes->pair);
	primes->seed = mem2array(primes->bytes, SHA1dlen);
	return 1;
}

/* Keep the seed private while unlocked and reclaim the entire tuple on error. */
void
IPints_DSAprimes(void *fp)
{
	F_IPints_DSAprimes *f;
	IPintDSAPrimes primes;
	IPints_IPint *oldq, *oldp;
	Array *oldseed;

	f = fp;
	primes.pair.p = mpnew(0);
	primes.pair.alpha = mpnew(0);
	primes.pair.prime = H;
	primes.pair.generator = H;
	primes.seed = H;
	release();
	DSAprimes(primes.pair.p, primes.pair.alpha, primes.bytes);
	acquire();
	if(!kprotect(ipintdsaprimes, &primes)){
		mpfree(primes.pair.p);
		mpfree(primes.pair.alpha);
		destroy(primes.pair.prime);
		destroy(primes.pair.generator);
		destroy(primes.seed);
		krethrow();
	}
	oldq = f->ret->t0;
	oldp = f->ret->t1;
	oldseed = f->ret->t2;
	f->ret->t0 = primes.pair.prime;
	f->ret->t1 = primes.pair.generator;
	f->ret->t2 = primes.seed;
	destroy(oldq);
	destroy(oldp);
	destroy(oldseed);
}
