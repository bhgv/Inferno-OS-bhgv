#include	"lib9.h"
#include	<bio.h>

/*
 * Decode one rune while retaining bytes beyond the decoder's consumed prefix.
 * Invalid or truncated UTF advances by one byte and can be ungot like a rune.
 */
long
Bgetrune(Biobuf *bp)
{
	int c, i;
	Rune rune;
	char str[UTFmax];

	bp->runesize = 0;
	c = Bgetc(bp);
	if(c < 0)
		return c;
	if(c < Runeself) {
		bp->runesize = 1;
		return c;
	}
	str[0] = c;
	i = 1;
	while(!fullrune(str, i)) {
		c = Bgetc(bp);
		if(c < 0) {
			bp->runesize = 1;
			for(; i > 1; i--)
				Bungetc(bp);
			return Runeerror;
		}
		str[i++] = c;
	}
	bp->runesize = chartorune(&rune, str);
	for(; i > bp->runesize; i--)
		Bungetc(bp);
	return rune;
}

/* Rewind exactly the bytes returned by the last successful rune read. */
int
Bungetrune(Biobuf *bp)
{

	if(bp->state == Bracteof)
		bp->state = Bractive;
	if(bp->state != Bractive)
		return Beof;
	bp->icount -= bp->runesize;
	bp->runesize = 0;
	return 1;
}
