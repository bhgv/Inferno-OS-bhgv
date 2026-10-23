#include <lib9.h>
#include <kernel.h>
#include <isa.h>
#include "interp.h"
#include "../libinterp/keyringif.h"
#include "mp.h"
#include "libsec.h"
#include "keys.h"

static char*	pkattr[] = { "p", "q", "alpha", "key", nil };
static char*	skattr[] = { "p", "q", "alpha", "key", "!secret", nil };
static char*	sigattr[] = { "r", "s", nil };

/* Parse fields in order and release the partial key at the first invalid integer. */
static void*
dsa_str2sk(char *str, char **strp)
{
	DSApriv *dsa;
	char *p;

	if(strp)
		*strp = str;
	if(str == nil)
		return nil;
	p = str;
	dsa = dsaprivalloc();
	if(dsa == nil)
		return nil;
	if((dsa->pub.p = base64tobig(p, &p)) == nil)
		goto bad;
	if((dsa->pub.q = base64tobig(p, &p)) == nil)
		goto bad;
	if((dsa->pub.alpha = base64tobig(p, &p)) == nil)
		goto bad;
	if((dsa->pub.key = base64tobig(p, &p)) == nil)
		goto bad;
	if((dsa->secret = base64tobig(p, &p)) == nil)
		goto bad;
	if(strp)
		*strp = p;
	return dsa;
bad:
	dsaprivfree(dsa);
	return nil;
}

/* Parse fields in order and release the partial key at the first invalid integer. */
static void*
dsa_str2pk(char *str, char **strp)
{
	DSApub *dsa;
	char *p;

	if(strp)
		*strp = str;
	if(str == nil)
		return nil;
	p = str;
	dsa = dsapuballoc();
	if(dsa == nil)
		return nil;
	if((dsa->p = base64tobig(p, &p)) == nil)
		goto bad;
	if((dsa->q = base64tobig(p, &p)) == nil)
		goto bad;
	if((dsa->alpha = base64tobig(p, &p)) == nil)
		goto bad;
	if((dsa->key = base64tobig(p, &p)) == nil)
		goto bad;
	if(strp)
		*strp = p;
	return dsa;
bad:
	dsapubfree(dsa);
	return nil;
}

/* Parse fields in order and release the partial key at the first invalid integer. */
static void*
dsa_str2sig(char *str, char **strp)
{
	DSAsig *dsa;
	char *p;

	if(strp)
		*strp = str;
	if(str == nil)
		return nil;
	p = str;
	dsa = dsasigalloc();
	if(dsa == nil)
		return nil;
	if((dsa->r = base64tobig(p, &p)) == nil)
		goto bad;
	if((dsa->s = base64tobig(p, &p)) == nil)
		goto bad;
	if(strp)
		*strp = p;
	return dsa;
bad:
	dsasigfree(dsa);
	return nil;
}

/* Emit every integer field completely; short buffers and failed exports are errors. */
static int
dsa_sk2str(void *value, char *buf, int len)
{
	DSApriv *key;
	mpint *fields[5];

	if(value == nil)
		return keyringfields(buf, len, nil, 1);
	key = value;
	fields[0] = key->pub.p;
	fields[1] = key->pub.q;
	fields[2] = key->pub.alpha;
	fields[3] = key->pub.key;
	fields[4] = key->secret;
	return keyringfields(buf, len, fields, nelem(fields));
}

/* Emit every integer field completely; short buffers and failed exports are errors. */
static int
dsa_pk2str(void *value, char *buf, int len)
{
	DSApub *key;
	mpint *fields[4];

	if(value == nil)
		return keyringfields(buf, len, nil, 1);
	key = value;
	fields[0] = key->p;
	fields[1] = key->q;
	fields[2] = key->alpha;
	fields[3] = key->key;
	return keyringfields(buf, len, fields, nelem(fields));
}

/* Emit every integer field completely; short buffers and failed exports are errors. */
static int
dsa_sig2str(void *value, char *buf, int len)
{
	DSAsig *key;
	mpint *fields[2];

	if(value == nil)
		return keyringfields(buf, len, nil, 1);
	key = value;
	fields[0] = key->r;
	fields[1] = key->s;
	return keyringfields(buf, len, fields, nelem(fields));
}

static void*
dsa_sk2pk(void *vs)
{
	return dsaprivtopub((DSApriv*)vs);
}

/* generate a dsa secret key with new params */
static void*
dsa_gen(int len)
{
	USED(len);
	return dsagen(nil);
}

/* generate a dsa secret key with same params as a public key */
static void*
dsa_genfrompk(void *vpub)
{
	return dsagen((DSApub*)vpub);
}

static void
dsa_freepub(void *a)
{
	dsapubfree((DSApub*)a);
}

static void
dsa_freepriv(void *a)
{
	dsaprivfree((DSApriv*)a);
}

static void
dsa_freesig(void *a)
{
	dsasigfree((DSAsig*)a);
}

static void*
dsa_sign(mpint* md, void *key)
{
	return dsasign((DSApriv*)key, md);
}

static int
dsa_verify(mpint* md, void *sig, void *key)
{
	return dsaverify((DSApub*)key, (DSAsig*)sig, md) == 0;
}

SigAlgVec*
dsainit(void)
{
	SigAlgVec *vec;

	vec = malloc(sizeof(SigAlgVec));
	if(vec == nil)
		return nil;

	vec->name = "dsa";

	vec->pkattr = pkattr;
	vec->skattr = skattr;
	vec->sigattr = sigattr;

	vec->str2sk = dsa_str2sk;
	vec->str2pk = dsa_str2pk;
	vec->str2sig = dsa_str2sig;

	vec->sk2str = dsa_sk2str;
	vec->pk2str = dsa_pk2str;
	vec->sig2str = dsa_sig2str;

	vec->sk2pk = dsa_sk2pk;

	vec->gensk = dsa_gen;
	vec->genskfrompk = dsa_genfrompk;
	vec->sign = dsa_sign;
	vec->verify = dsa_verify;

	vec->skfree = dsa_freepriv;
	vec->pkfree = dsa_freepub;
	vec->sigfree = dsa_freesig;

	return vec;
}
