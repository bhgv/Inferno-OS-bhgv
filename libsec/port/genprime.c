#include "os.h"
#include <mp.h>
#include <libsec.h>

/* Generate an odd prime of exactly n bits without retaining old magnitude data. */
void
genprime(mpint *p, int n, int accuracy)
{
	mpdigit high;

	if(n < 2 || n > 0x7fffffff-(Dbits-1))
		sysfatal("genprime: invalid bit count");
	if(mprand(n, genrandom, p) == nil)
		sysfatal("genprime: random allocation failed");
	mpbits(p, n);
	high = (mpdigit)1 << ((n-1)%Dbits);
	p->p[p->top-1] |= high;
	p->p[0] |= 1;

	for(;;){
		if(probably_prime(p, accuracy))
			return;
		mpadd(p, mptwo, p);
		if(mpsignif(p) > n){
			/* Continue at the first odd candidate instead of growing the result. */
			mpleft(mpone, n-1, p);
			p->p[0] |= 1;
		}
	}
}
