#include	<lib9.h>

/* Copy through the overlap-safe portable mover. */
void*
memcpy(void *a1, void *a2, ulong n)
{
	return memmove(a1, a2, n);
}

/* Retain the full native count while copying in the overlap-safe direction. */
void*
memmove(void *a1, void *a2, ulong n)
{
	uchar *s, *d;

	d = a1;
	s = a2;
	if(n == 0 || d == s)
		return a1;
	if((uintptr)d > (uintptr)s){
		s += n;
		d += n;
		while(n > 0){
			*--d = *--s;
			n--;
		}
	}else{
		while(n > 0){
			*d++ = *s++;
			n--;
		}
	}
	return a1;
}
