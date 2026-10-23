#include <lib9.h>
#include <kernel.h>
#include <isa.h>
#include "interp.h"
#include "../libinterp/keyringif.h"
#include "mp.h"
#include "libsec.h"
#include "keys.h"

static char*	pkattr[] = { "n", "ek", nil };
static char*	skattr[] = { "n", "ek", "!dk", "!p", "!q", "!kp", "!kq", "!c2", nil };
static char*	sigattr[] = { "val", nil };

/* Parse fields in order and release the partial key at the first invalid integer. */
static void*
rsa_str2sk(char *str, char **strp)
{
	RSApriv *rsa;
	char *p;

	if(strp)
		*strp = str;
	if(str == nil)
		return nil;
	p = str;
	rsa = rsaprivalloc();
	if(rsa == nil)
		return nil;
	if((rsa->pub.n = base64tobig(p, &p)) == nil)
		goto bad;
	if((rsa->pub.ek = base64tobig(p, &p)) == nil)
		goto bad;
	if((rsa->dk = base64tobig(p, &p)) == nil)
		goto bad;
	if((rsa->p = base64tobig(p, &p)) == nil)
		goto bad;
	if((rsa->q = base64tobig(p, &p)) == nil)
		goto bad;
	if((rsa->kp = base64tobig(p, &p)) == nil)
		goto bad;
	if((rsa->kq = base64tobig(p, &p)) == nil)
		goto bad;
	if((rsa->c2 = base64tobig(p, &p)) == nil)
		goto bad;
	if(strp)
		*strp = p;
	return rsa;
bad:
	rsaprivfree(rsa);
	return nil;
}

/* Parse fields in order and release the partial key at the first invalid integer. */
static void*
rsa_str2pk(char *str, char **strp)
{
	RSApub *rsa;
	char *p;

	if(strp)
		*strp = str;
	if(str == nil)
		return nil;
	p = str;
	rsa = rsapuballoc();
	if(rsa == nil)
		return nil;
	if((rsa->n = base64tobig(p, &p)) == nil)
		goto bad;
	if((rsa->ek = base64tobig(p, &p)) == nil)
		goto bad;
	if(strp)
		*strp = p;
	return rsa;
bad:
	rsapubfree(rsa);
	return nil;
}

/* Publish a signature cursor only after the integer has been accepted. */
static void*
rsa_str2sig(char *str, char **strp)
{
	mpint *rsa;
	char *p;

	if(strp)
		*strp = str;
	rsa = base64tobig(str, &p);
	if(rsa == nil)
		return nil;
	if(strp)
		*strp = p;
	return rsa;
}

/* Emit every integer field completely; short buffers and failed exports are errors. */
static int
rsa_sk2str(void *value, char *buf, int len)
{
	RSApriv *key;
	mpint *fields[8];

	if(value == nil)
		return keyringfields(buf, len, nil, 1);
	key = value;
	fields[0] = key->pub.n;
	fields[1] = key->pub.ek;
	fields[2] = key->dk;
	fields[3] = key->p;
	fields[4] = key->q;
	fields[5] = key->kp;
	fields[6] = key->kq;
	fields[7] = key->c2;
	return keyringfields(buf, len, fields, nelem(fields));
}

/* Emit every integer field completely; short buffers and failed exports are errors. */
static int
rsa_pk2str(void *value, char *buf, int len)
{
	RSApub *key;
	mpint *fields[2];

	if(value == nil)
		return keyringfields(buf, len, nil, 1);
	key = value;
	fields[0] = key->n;
	fields[1] = key->ek;
	return keyringfields(buf, len, fields, nelem(fields));
}

/* Emit every integer field completely; short buffers and failed exports are errors. */
static int
rsa_sig2str(void *value, char *buf, int len)
{
	mpint *key;
	mpint *fields[1];

	if(value == nil)
		return keyringfields(buf, len, nil, 1);
	key = value;
	fields[0] = key;
	return keyringfields(buf, len, fields, nelem(fields));
}

static void*
rsa_sk2pk(void *vs)
{
	return rsaprivtopub((RSApriv*)vs);
}

/* generate an rsa secret key */
static void*
rsa_gen(int len)
{
	RSApriv *key;

	for(;;){
		key = rsagen(len, 6, 0);
		if(mpsignif(key->pub.n) == len)
			return key;
		rsaprivfree(key);
	}
}

/* generate an rsa secret key with same params as a public key */
static void*
rsa_genfrompk(void *vpub)
{
	RSApub *pub;

	pub = vpub;
	return rsagen(mpsignif(pub->n), mpsignif(pub->ek), 0);
}

static void*
rsa_sign(mpint* m, void *key)
{
	return rsadecrypt((RSApriv*)key, m, nil);
}

static int
rsa_verify(mpint* m, void *sig, void *key)
{
	mpint *t;
	int r;

	t = rsaencrypt((RSApub*)key, (mpint*)sig, nil);
	r = mpcmp(t, m) == 0;
	mpfree(t);
	return r;
}

static void
rsa_freepriv(void *a)
{
	rsaprivfree((RSApriv*)a);
}

static void
rsa_freepub(void *a)
{
	rsapubfree((RSApub*)a);
}

static void
rsa_freesig(void *a)
{
	mpfree(a);
}

SigAlgVec*
rsainit(void)
{
	SigAlgVec *vec;

	vec = malloc(sizeof(SigAlgVec));
	if(vec == nil)
		return nil;

	vec->name = "rsa";

	vec->pkattr = pkattr;
	vec->skattr = skattr;
	vec->sigattr = sigattr;

	vec->str2sk = rsa_str2sk;
	vec->str2pk = rsa_str2pk;
	vec->str2sig = rsa_str2sig;

	vec->sk2str = rsa_sk2str;
	vec->pk2str = rsa_pk2str;
	vec->sig2str = rsa_sig2str;

	vec->sk2pk = rsa_sk2pk;

	vec->gensk = rsa_gen;
	vec->genskfrompk = rsa_genfrompk;
	vec->sign = rsa_sign;
	vec->verify = rsa_verify;

	vec->skfree = rsa_freepriv;
	vec->pkfree = rsa_freepub;
	vec->sigfree = rsa_freesig;

	return vec;
}
