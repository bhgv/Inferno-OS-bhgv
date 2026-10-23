/* derived from /netlib/fdlibm */

/* @(#)s_scalbn.c 1.3 95/01/18 */
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
 * scalbn (double x, int n)
 * scalbn(x,n) returns x* 2**n  computed by  exponent  
 * manipulation rather than by actually performing an 
 * exponentiation or a multiplication.
 */

#include "fdlibm.h"

static const double
two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
twom54  =  5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
Huge   = 1.0e+300,
tiny   = 1.0e-300;

/* Handle extreme exponents before addition, preserving nonfinite inputs and signed zero. */
double
scalbn(double x, int n)
{
	int k, hx, lx;

	hx = __HI(x);
	lx = __LO(x);
	k = (hx&0x7ff00000)>>20;
	if(k == 0){
		if((lx|(hx&0x7fffffff)) == 0)
			return x;
		x *= two54;
		hx = __HI(x);
		k = ((hx&0x7ff00000)>>20)-54;
	}
	if(k == 0x7ff)
		return x+x;
	if(n > 50000)
		return Huge*copysign(Huge, x);
	if(n < -50000)
		return tiny*copysign(tiny, x);
	k += n;
	if(k > 0x7fe)
		return Huge*copysign(Huge, x);
	if(k > 0){
		__HI(x) = (hx&0x800fffff)|(k<<20);
		return x;
	}
	if(k <= -54)
		return tiny*copysign(tiny, x);
	k += 54;
	__HI(x) = (hx&0x800fffff)|(k<<20);
	return x*twom54;
}
