#include	"lib9.h"
#include	<bio.h>


/* Complete short writes without narrowing native results or losing unwritten bytes. */
long
Bwrite(Biobuf *bp, void *ap, long count)
{
	long c, i;
	uchar *p;
	int n, oc, done;
	char errbuf[ERRMAX];

	if(count > 0 && bp->state != Bwactive)
		return Beof;
	p = ap;
	c = count;
	oc = bp->ocount;

	while(c > 0) {
		n = -oc;
		if(n > c)
			n = c;
		if(n == 0) {
			if(bp->state != Bwactive)
				return Beof;
			bp->ocount = oc;
			for(done = 0; done < bp->bsize; done += i){
				i = write(bp->fid, bp->bbuf+done, bp->bsize-done);
				if(i <= 0 || i > bp->bsize-done){
					/* Retry only the unwritten suffix after an interruption. */
					memmove(bp->bbuf, bp->bbuf+done, bp->bsize-done);
					bp->ocount = -done;
					errstr(errbuf, sizeof errbuf);
					if(i > bp->bsize-done || strstr(errbuf, "interrupt") == nil)
						bp->state = Binactive;
					errstr(errbuf, sizeof errbuf);
					return Beof;
				}
				bp->offset += i;
			}
			oc = -bp->bsize;
			continue;
		}
		memmove(bp->ebuf+oc, p, n);
		oc += n;
		c -= n;
		p += n;
	}
	bp->ocount = oc;
	return count-c;
}
