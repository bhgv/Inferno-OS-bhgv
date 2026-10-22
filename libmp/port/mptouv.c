#include "os.h"
#include <mp.h>
#include "dat.h"

#define VLDIGITS (sizeof(vlong)/sizeof(mpdigit))

/*
 * Reserve the full scalar bit width before filling magnitude digits.
 * A scalar contains an integral number of digits.
 */
mpint*
uvtomp(uvlong v, mpint *b)
{
	int s;

	if(b == nil)
		b = mpnew(sizeof(uvlong)*8);
	else
		mpbits(b, sizeof(uvlong)*8);
	mpassign(mpzero, b);
	if(v == 0)
		return b;
	for(s = 0; s < VLDIGITS && v != 0; s++){
		b->p[s] = v;
		/* Two half shifts also permit digits as wide as the scalar. */
		v >>= Dbits/2;
		v >>= Dbits/2;
	}
	b->top = s;
	return b;
}

/* Convert magnitudes, saturating overflow to the full unsigned range. */
uvlong
mptouv(mpint *b)
{
	uvlong v;
	int s;

	if(b->top == 0)
		return 0;

	mpnorm(b);
	if(b->top > VLDIGITS)
		return MAXUVLONG;

	v = 0;
	for(s = 0; s < b->top; s++)
		v |= (uvlong)b->p[s]<<(s*sizeof(mpdigit)*8);

	return v;
}
