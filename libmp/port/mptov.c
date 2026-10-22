#include "os.h"
#include <mp.h>
#include "dat.h"

#define VLDIGITS (sizeof(vlong)/sizeof(mpdigit))

/*
 * Convert signed magnitudes without negating the minimum signed value.
 * A scalar contains an integral number of digits.
 */
mpint*
vtomp(vlong v, mpint *b)
{
	int s;
	uvlong uv;

	if(b == nil)
		b = mpnew(sizeof(vlong)*8);
	else
		mpbits(b, sizeof(vlong)*8);
	mpassign(mpzero, b);
	if(v == 0)
		return b;
	if(v < 0){
		b->sign = -1;
		uv = (uvlong)0-(uvlong)v;
	} else
		uv = v;
	for(s = 0; s < VLDIGITS && uv != 0; s++){
		b->p[s] = uv;
		/* Two half shifts also permit digits as wide as the scalar. */
		uv >>= Dbits/2;
		uv >>= Dbits/2;
	}
	b->top = s;
	return b;
}

/* Widen each digit before shifting and saturate to the signed result range. */
vlong
mptov(mpint *b)
{
	uvlong v;
	int s;

	if(b->top == 0)
		return 0;

	mpnorm(b);
	if(b->top > VLDIGITS){
		if(b->sign > 0)
			return (vlong)MAXVLONG;
		else
			return (vlong)MINVLONG;
	}

	v = 0;
	for(s = 0; s < b->top; s++)
		v |= (uvlong)b->p[s]<<(s*sizeof(mpdigit)*8);

	if(b->sign > 0){
		if(v > MAXVLONG)
			v = MAXVLONG;
	} else {
		if(v > MINVLONG)
			v = MINVLONG;
		else
			v = (uvlong)0-v;
	}

	return (vlong)v;
}
