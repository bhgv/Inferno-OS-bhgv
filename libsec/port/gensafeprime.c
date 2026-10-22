#include "os.h"
#include <mp.h>
#include <libsec.h>

/* Find an n-bit safe prime and a nonzero generator of its multiplicative group. */
void
gensafeprime(mpint *p, mpint *alpha, int n, int accuracy)
{
	mpint *q, *b;

	if(n < 3 || n > 0x7fffffff-(Dbits-1))
		sysfatal("gensafeprime: invalid bit count");
	q = mpnew(n-1);
	while(1){
		genprime(q, n-1, accuracy);
		mpleft(q, 1, p);
		mpadd(p, mpone, p); // p = 2*q+1
		if(probably_prime(p, accuracy))
			break;
	}
	// now find a generator alpha of the multiplicative
	// group Z*_p of order p-1=2q
	b = mpnew(0);
	while(1){
		if(mprand(n, genrandom, alpha) == nil)
			sysfatal("gensafeprime: random allocation failed");
		mpmod(alpha, p, alpha);
		if(mpcmp(alpha, mpone) <= 0)
			continue;
		mpmul(alpha, alpha, b);
		mpmod(b, p, b);
		if(mpcmp(b, mpone) == 0)
			continue;
		mpexp(alpha, q, p, b);
		if(mpcmp(b, mpone) != 0)
			break;
	}
	mpfree(b);
	mpfree(q);
}
