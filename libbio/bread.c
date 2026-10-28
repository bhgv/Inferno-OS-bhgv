#include	"lib9.h"
#include	<bio.h>


/* Bound native refill results before moving bytes or publishing buffer counts. */
long
Bread(Biobuf *bp, void *ap, long count)
{
	long c, i;
	uchar *p;
	int n, ic;

	p = ap;
	c = count;
	ic = bp->icount;

	while(c > 0) {
		n = -ic;
		if(n > c)
			n = c;
		if(n == 0) {
			if(bp->state != Bractive)
				break;
			i = read(bp->fid, bp->bbuf, bp->bsize);
			if(i <= 0 || i > bp->bsize) {
				bp->state = Bracteof;
				if(i < 0 || i > bp->bsize)
					bp->state = Binactive;
				break;
			}
			bp->gbuf = bp->bbuf;
			bp->offset += i;
			if(i < bp->bsize) {
				memmove(bp->ebuf-i, bp->bbuf, i);
				bp->gbuf = bp->ebuf-i;
			}
			ic = -i;
			continue;
		}
		memmove(p, bp->ebuf+ic, n);
		c -= n;
		ic += n;
		p += n;
	}
	bp->icount = ic;
	if(count == c && bp->state == Binactive)
		return -1;
	return count-c;
}
