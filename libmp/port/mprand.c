#include "os.h"
#include <mp.h>
#include <libsec.h>
#include "dat.h"

/* Reject impossible sizes and reserve scratch storage before changing a result. */
mpint*
mprand(int bits, void (*gen)(uchar*, int), mpint *b)
{
	int n, m;
	mpdigit mask;
	uchar *p;

	if(bits < 0 || bits > 0x7fffffff-(Dbits-1))
		return nil;
	if(bits == 0){
		if(b == nil)
			b = mpnew(0);
		mpassign(mpzero, b);
		return b;
	}
	n = DIGITS(bits);
	p = malloc(n*Dbytes);
	if(p == nil)
		return nil;
	if(b == nil)
		b = mpnew(bits);
	else
		mpbits(b, bits);

	(*gen)(p, n*Dbytes);
	betomp(p, n*Dbytes, b);
	free(p);

	/* Only the highest imported limb needs masking; higher storage may be stale. */
	m = bits%Dbits;
	if(m > 0 && b->top == n){
		mask = ((mpdigit)1 << m)-1;
		b->p[n-1] &= mask;
	}
	mpnorm(b);
	return b;
}
