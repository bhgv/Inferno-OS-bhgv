#include "os.h"
#include <mp.h>
#include "dat.h"

/*
 *  this code assumes that mpdigit is at least as
 *  big as an int.
 */

/* Preserve INT_MIN by forming its magnitude in unsigned arithmetic. */
mpint*
itomp(int i, mpint *b)
{
	if(b == nil)
		b = mpnew(0);
	mpassign(mpzero, b);
	if(i != 0)
		b->top = 1;
	if(i < 0){
		b->sign = -1;
		*b->p = (uint)0-(uint)i;
	} else
		*b->p = i;
	return b;
}

/* Saturate signed results without negating an unrepresentable magnitude. */
int
mptoi(mpint *b)
{
	mpdigit x;

	if(b->top==0)
		return 0;
	x = *b->p;
	if(b->sign > 0){
		if(b->top > 1 || (x > MAXINT))
			x = (int)MAXINT;
		else
			x = (int)x;
	} else {
		if(b->top > 1 || x > MAXINT+1)
			x = (int)MININT;
		else
			x = (uint)0-(uint)x;
	}
	return x;
}
