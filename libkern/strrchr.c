#include <lib9.h>

/* Retain the final matching byte, testing the terminator exactly once. */
char*
strrchr(char *s, int c)
{
	char *last;
	uchar byte;

	byte = c;
	last = nil;
	for(;;){
		if(*(uchar*)s == byte)
			last = s;
		if(*s == 0)
			return last;
		s++;
	}
}
