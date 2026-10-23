#include <lib9.h>

/* Search unsigned bytes, including NUL after narrowing the requested character. */
char*
strchr(char *s, int c)
{
	uchar byte;

	byte = c;
	for(;;){
		if(*(uchar*)s == byte)
			return s;
		if(*s == 0)
			return nil;
		s++;
	}
}
