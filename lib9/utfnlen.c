#include "lib9.h"

/* Count complete runes without forming an end pointer beyond the actual string. */
int
utfnlen(char *s, long m)
{
	int c, consumed;
	long n;
	Rune rune;

	for(n = 0; m > 0; n++){
		c = *(uchar*)s;
		if(c < Runeself){
			if(c == '\0')
				break;
			consumed = 1;
		}else{
			if(!fullrune(s, m < UTFmax ? (int)m : UTFmax))
				break;
			consumed = chartorune(&rune, s);
		}
		s += consumed;
		m -= consumed;
	}
	return n;
}
