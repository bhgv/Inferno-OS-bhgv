#include	"lib9.h"
#include	<bio.h>


/* Read a byte while retaining the allocated history prefix across refills. */
int
Bgetc(Biobuf *bp)
{
	int i;

loop:
	i = bp->icount;
	if(i != 0) {
		bp->icount = i+1;
		return bp->ebuf[i];
	}
	if(bp->state != Bractive) {
		if(bp->state == Bracteof)
			bp->state = Bractive;
		return Beof;
	}
	/*
	 * get next buffer, try to keep Bungetsize
	 * characters pre-catenated from the previous
	 * buffer to allow that many ungets.
	 */
	memmove(bp->bbuf-Bungetsize, bp->ebuf-Bungetsize, Bungetsize);
	i = read(bp->fid, bp->bbuf, bp->bsize);
	bp->gbuf = bp->bbuf;
	if(i <= 0) {
		bp->state = Bracteof;
		if(i < 0)
			bp->state = Binactive;
		return Beof;
	}
	if(i < bp->bsize) {
		memmove(bp->ebuf-i-Bungetsize, bp->bbuf-Bungetsize, i+Bungetsize);
		bp->gbuf = bp->ebuf-i;
	}
	bp->icount = -i;
	bp->offset += i;
	goto loop;
}


/* Rewind only within the allocated pushback prefix and representable count range. */
int
Bungetc(Biobuf *bp)
{
	vlong next;

	if((bp->state != Bractive && bp->state != Bracteof) ||
	   bp->bsize < 0 || bp->icount > 0)
		return Beof;
	next = (vlong)bp->icount-1;
	if(next < (-2147483647-1) || next < -(vlong)bp->bsize-Bungetsize)
		return Beof;
	bp->icount = next;
	bp->state = Bractive;
	return 1;
}
