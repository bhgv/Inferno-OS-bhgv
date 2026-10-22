#include "lib9.h"

/* Invalid rune values must not alias a byte after libc character conversion. */
char*
utfrune(char *s, long c)
{
	long c1;
	Rune r;
	int n;

	if(c < 0 || c > Runemax)
		return nil;
	if(c < Runesync)		/* not part of utf sequence */
		return strchr(s, c);

	for(;;) {
		c1 = *(uchar*)s;
		if(c1 < Runeself) {	/* one byte rune */
			if(c1 == 0)
				return 0;
			if(c1 == c)
				return s;
			s++;
			continue;
		}
		n = chartorune(&r, s);
		if(r == c)
			return s;
		s += n;
	}
}
