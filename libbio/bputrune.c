#include	"lib9.h"
#include	<bio.h>

/* Report write failures consistently for ASCII and multibyte runes. */
int
Bputrune(Biobuf *bp, long c)
{
	Rune rune;
	char str[UTFmax];
	int n;

	rune = c;
	if(rune < Runeself) {
		if(Bputc(bp, rune) == Beof)
			return Beof;
		return 1;
	}
	n = runetochar(str, &rune);
	if(n == 0)
		return Bbad;
	if(Bwrite(bp, str, n) != n)
		return Beof;
	return n;
}
