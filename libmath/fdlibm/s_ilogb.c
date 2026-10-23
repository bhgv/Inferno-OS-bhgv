/* derived from /netlib/fdlibm */

/* @(#)s_ilogb.c 1.3 95/01/18 */
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

/* ilogb(double x)
 * return the binary exponent of non-zero x
 * ilogb(0) = 0x80000001
 * ilogb(inf/NaN) = 0x7fffffff (no signal is raised)
 */

#include "fdlibm.h"

/* Count subnormal leading bits with unsigned shifts, retaining legacy sentinel values. */
int
ilogb(double x)
{
	unsigned hx, lx, scan;
	int ix;

	hx = __HI(x)&0x7fffffff;
	if(hx < 0x00100000){
		lx = __LO(x);
		if((hx|lx) == 0)
			return -0x7fffffff;
		if(hx == 0){
			ix = -1043;
			scan = lx;
		}else{
			ix = -1022;
			scan = hx<<11;
		}
		for(; (scan&0x80000000U) == 0; scan <<= 1)
			ix--;
		return ix;
	}
	if(hx < 0x7ff00000)
		return (int)(hx>>20)-1023;
	return 0x7fffffff;
}
