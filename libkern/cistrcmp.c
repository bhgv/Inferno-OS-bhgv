#include "lib9.h"

/* Compare unsigned bytes with ASCII case folding and symmetric NUL ordering. */
int
cistrcmp(char *s1, char *s2)
{
	int c1, c2;

	for(;;){
		c1 = *(uchar*)s1++;
		c2 = *(uchar*)s2++;
		if(c1 >= 'A' && c1 <= 'Z')
			c1 += 'a' - 'A';
		if(c2 >= 'A' && c2 <= 'Z')
			c2 += 'a' - 'A';
		if(c1 != c2 || c1 == 0)
			return c1 - c2;
	}
}
