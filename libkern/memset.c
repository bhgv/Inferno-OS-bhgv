#include	<lib9.h>

/* Fill the complete native-size span with the low byte of c. */
void*
memset(void *ap, int c, ulong n)
{
	uchar *p;

	p = ap;
	while(n > 0){
		*p++ = c;
		n--;
	}
	return ap;
}
