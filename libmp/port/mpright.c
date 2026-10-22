#include "os.h"
#include <mp.h>
#include "dat.h"

/*
 * Large right shifts become zero before computing a destination bit count.
 * Reject an unrepresentable reverse shift without negating INT_MIN.
 */
void
mpright(mpint *b, int shift, mpint *res)
{
	int d, l, r, i;
	mpdigit this, last;

	res->sign = b->sign;
	if(b->top==0){
		res->top = 0;
		res->sign = 1;
		return;
	}

	// a negative right shift is a left shift
	if(shift < 0){
		if(shift == (-0x7fffffff-1))
			sysfatal("mpright: reverse shift exceeds bit capacity");
		mpleft(b, -shift, res);
		return;
	}

	d = shift/Dbits;
	r = shift - d*Dbits;
	l = Dbits - r;

	/* Test this before subtracting shift from the source's bit capacity. */
	if(d >= b->top){
		res->top = 0;
		res->sign = 1;
		return;
	}
	if(res != b)
		mpbits(res, b->top*Dbits - shift);

	// special case digit shifts
	if(r == 0){
		for(i = 0; i < b->top-d; i++)
			res->p[i] = b->p[i+d];
	} else {
		last = b->p[d];
		for(i = 0; i < b->top-d-1; i++){
			this = b->p[i+d+1];
			res->p[i] = (this<<l) | (last>>r);
			last = this;
		}
		res->p[i++] = last>>r;
	}
	while(i > 0 && res->p[i-1] == 0)
		i--;
	res->top = i;
	if(i==0)
		res->sign = 1;
}
