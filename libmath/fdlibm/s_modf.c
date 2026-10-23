/* derived from /netlib/fdlibm */

/* @(#)s_modf.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/*
 * modf(double x, double *iptr) 
 * return fraction part of x, and return x's integral part in *iptr.
 * Method:
 *	Bit twiddling.
 *
 * Exception:
 *	No exception.
 */

#include "fdlibm.h"

static const double one = 1.0;

/* Split finite values by their binary exponent and preserve NaNs in both components. */
double
modf(double x, double *iptr)
{
	int high, low, exponent;
	unsigned int mask;

	high = __HI(x);
	low = __LO(x);
	exponent = ((high>>20)&0x7ff)-0x3ff;
	if(exponent < 20){
		if(exponent < 0){
			/* Magnitudes below one have a signed zero integral component. */
			__HIp(iptr) = high&0x80000000;
			__LOp(iptr) = 0;
			return x;
		}
		mask = 0x000fffff>>exponent;
		if(((high&mask)|low) == 0){
			*iptr = x;
			__HI(x) &= 0x80000000;
			__LO(x) = 0;
			return x;
		}
		__HIp(iptr) = high&(~mask);
		__LOp(iptr) = 0;
		return x-*iptr;
	}
	if(exponent > 51){
		*iptr = x*one;
		if(exponent == 0x400 && ((high&0x000fffff)|low) != 0)
			return *iptr;
		/* Finite integers and infinities have a signed zero fractional component. */
		__HI(x) &= 0x80000000;
		__LO(x) = 0;
		return x;
	}
	mask = 0xffffffffU>>(exponent-20);
	if((low&mask) == 0){
		*iptr = x;
		__HI(x) &= 0x80000000;
		__LO(x) = 0;
		return x;
	}
	__HIp(iptr) = high;
	__LOp(iptr) = low&(~mask);
	return x-*iptr;
}
