#include	"lib9.h"
#include	<bio.h>

/* Complete short writes and account for every byte before reporting a failure. */
int
Bflush(Biobuf *bp)
{
	int n, c, done;

	switch(bp->state) {
	case Bwactive:
		n = bp->bsize+bp->ocount;
		for(done = 0; done < n; done += c){
			c = write(bp->fid, bp->bbuf+done, n-done);
			if(c <= 0){
				memmove(bp->bbuf, bp->bbuf+done, n-done);
				bp->ocount = n-done-bp->bsize;
				bp->state = Binactive;
				return Beof;
			}
			bp->offset += c;
		}
		bp->ocount = -bp->bsize;
		return 0;

	case Bracteof:
		bp->state = Bractive;
		/* fall through */
	case Bractive:
		bp->icount = 0;
		bp->gbuf = bp->ebuf;
		return 0;
	}
	return Beof;
}
