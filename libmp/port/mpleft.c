#include "os.h"
#include <mp.h>
#include "dat.h"

/*
 * Shift a magnitude without overflowing the signed bit-capacity calculation.
 * INT_MIN in the reverse direction shifts every representable input to zero.
 */
void
mpleft(mpint *b, int shift, mpint *res)
{
	int d, l, r, i, otop;
	mpdigit this, last;

	res->sign = b->sign;
	if(b->top==0){
		res->top = 0;
		res->sign = 1;
		return;
	}

	// a negative left shift is a right shift
	if(shift < 0){
		if(shift == (-0x7fffffff-1)){
			mpassign(mpzero, res);
			return;
		}
		mpright(b, -shift, res);
		return;
	}

	// b and res may be the same so remember the old top
	otop = b->top;
	if((uvlong)otop*Dbits+(uint)shift > 0x7fffffff-(Dbits-1))
		sysfatal("mpleft: shift exceeds bit capacity");

	// shift
	mpbits(res, otop*Dbits + shift);	// overkill
	res->top = DIGITS(otop*Dbits + shift);
	d = shift/Dbits;
	l = shift - d*Dbits;
	r = Dbits - l;

	if(l == 0){
		for(i = otop-1; i >= 0; i--)
			res->p[i+d] = b->p[i];
	} else {
		last = 0;
		for(i = otop-1; i >= 0; i--) {
			this = b->p[i];
			res->p[i+d+1] = (last<<l) | (this>>r);
			last = this;
		}
		res->p[d] = last<<l;
	}
	for(i = 0; i < d; i++)
		res->p[i] = 0;

	// normalize
	while(res->top > 0 && res->p[res->top-1] == 0)
		res->top--;
}
