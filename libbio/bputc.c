#include	"lib9.h"
#include	<bio.h>

/* A read/inactive buffer cannot accept bytes or make output-flush progress. */
int
Bputc(Biobuf *bp, int c)
{
	int i;

	if(bp->state != Bwactive)
		return Beof;
	for(;;) {
		i = bp->ocount;
		if(i) {
			bp->ebuf[i++] = c;
			bp->ocount = i;
			return 0;
		}
		if(Bflush(bp) == Beof)
			break;
	}
	return Beof;
}
