#include "lib9.h"
#include "mathi.h"

double
dot(int n, double *x, double *y)
{
	double	sum = 0;
	if (n <= 0) 
		return 0;
	while (n--) {
		sum += *x++ * *y++;
	}
	return sum;
}


int
iamax(int n, double *x)
{
	int	i, m;
	double	xm, a;
	if (n <= 0) 
		return 0;
	m = 0;
	xm = fabs(*x);
	for (i = 1; i < n; i++) {
		a = fabs(*++x);
		if (xm < a) {
			m = i;
			xm = a;
		}
	}
	return m;
}


double
norm1(int n, double *x)
{
	double	sum = 0;
	if (n <= 0) 
		return 0;
	while (n--) {
		sum += fabs(*x);
		x++;
	}
	return sum;
}


/* Scale squares so representable norms do not overflow or underflow internally. */
double
norm2(int n, double *x)
{
	double scale, sum, a, ratio, infinity;
	int i;

	scale = 0;
	sum = 1;
	infinity = 0;
	for(i = 0; i < n; i++){
		a = fabs(x[i]);
		if(isNaN(a))
			return a;
		if(isInf(a, 1)){
			infinity = a;
			continue;
		}
		if(a == 0)
			continue;
		if(scale < a){
			ratio = scale/a;
			sum = 1+sum*ratio*ratio;
			scale = a;
		}else{
			ratio = a/scale;
			sum += ratio*ratio;
		}
	}
	if(infinity != 0)
		return infinity;
	return scale*sqrt(sum);
}
