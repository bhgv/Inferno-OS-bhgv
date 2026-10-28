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


/* Rewind the last rune only when its complete byte span fits the pushback area.
 * A zero rune size retains the historical successful no-op for an active reader. */
int
Bungetrune(Biobuf *bp)
{
	vlong next;

	if((bp->state != Bractive && bp->state != Bracteof) ||
	   bp->bsize < 0 || bp->icount > 0 || bp->runesize < 0 || bp->runesize > UTFmax)
		return Beof;
	next = (vlong)bp->icount-bp->runesize;
	if(next < (-2147483647-1) || next < -(vlong)bp->bsize-Bungetsize)
		return Beof;
	bp->icount = next;
	bp->runesize = 0;
	bp->state = Bractive;
	return 1;
}
