#include "lib9.h"

/* Match ASCII-folded byte prefixes without narrowing a native string length. */
char*
cistrstr(char *s, char *sub)
{
	char *p, *q;
	int c, csub;

	if(*sub == 0)
		return s;
	for(; *s != 0; s++){
		p = s;
		q = sub;
		for(;;){
			csub = *(uchar*)q++;
			if(csub == 0)
				return s;
			c = *(uchar*)p++;
			if(c == 0)
				return nil;
			if(c >= 'A' && c <= 'Z')
				c += 'a' - 'A';
			if(csub >= 'A' && csub <= 'Z')
				csub += 'a' - 'A';
			if(c != csub)
				break;
		}
	}
	return nil;
}
