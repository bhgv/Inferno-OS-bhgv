#include "lib9.h"
#include "mathi.h"

/* Preserve NaNs while returning positive zero for non-positive differences. */
double
fdim(double x, double y)
{
	if(x != x || y != y)
		return x+y;
	if(x > y)
		return x-y;
	return 0;
}

/* Prefer the numeric operand to a lone NaN and positive zero to negative zero. */
double
fmax(double x, double y)
{
	if(x != x)
		return y;
	if(y != y)
		return x;
	if(x == 0 && y == 0)
		return copysign(1, x) > 0 ? x : y;
	return x > y ? x : y;
}

/* Prefer the numeric operand to a lone NaN and negative zero to positive zero. */
double
fmin(double x, double y)
{
	if(x != x)
		return y;
	if(y != y)
		return x;
	if(x == 0 && y == 0)
		return copysign(1, x) < 0 ? x : y;
	return x < y ? x : y;
}
