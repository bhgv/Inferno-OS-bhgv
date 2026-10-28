#include	"lib9.h"
#include	<bio.h>


/* Append within the signed line-length limit, consuming the old owner on failure. */
static char*
badd(char *p, int *np, char *data, int ndata, int delim, int nulldelim)
{
	int n;
	char *grown;

	n = *np;
	if(n < 0 || ndata < 0 || n > 0x7fffffff-ndata-1){
		free(p);
		*np = 0;
		return nil;
	}
	grown = realloc(p, n+ndata+1);
	if(grown == nil){
		free(p);
		*np = 0;
		return nil;
	}
	p = grown;
	memmove(p+n, data, ndata);
	n += ndata;
	if(n > 0 && nulldelim && (uchar)p[n-1] == (uchar)delim)
		p[--n] = '\0';
	else
		p[n] = '\0';
	*np = n;
	return p;
}


/* Stop immediately if a line cannot be allocated; do not return a later suffix. */
char*
Brdstr(Biobuf *bp, int delim, int nulldelim)
{
	char *ip, *ep, *p;
	int i, j;
	long nread;

	i = -bp->icount;
	bp->rdline = 0;
	if(i == 0) {
		/*
		 * eof or other error
		 */
		if(bp->state != Bractive) {
			if(bp->state == Bracteof)
				bp->state = Bractive;
			bp->gbuf = bp->ebuf;
			return nil;
		}
	}

	/*
	 * first try in remainder of buffer (gbuf doesn't change)
	 */
	ip = (char*)bp->ebuf - i;
	ep = memchr(ip, delim, i);
	if(ep) {
		j = (ep - ip) + 1;
		bp->icount += j;
		return badd(nil, &bp->rdline, ip, j, delim, nulldelim);
	}

	/*
	 * copy data to beginning of buffer
	 */
	p = nil;
	if(i >= bp->bsize){
		/* Append from the true unread start, including the reserved prefix. */
		p = badd(nil, &bp->rdline, ip, i, 0, 0);
		i = 0;
		bp->icount = 0;
		bp->gbuf = bp->ebuf;
		if(p == nil)
			return nil;
	}else{
		memmove(bp->bbuf, ip, i);
		bp->gbuf = bp->bbuf;
	}

	/*
	 * append to buffer looking for the delim
	 */
	for(;;){
		ip = (char*)bp->bbuf + i;
		while(i < bp->bsize) {
			nread = bp->state == Bractive ? read(bp->fid, ip, bp->bsize-i) : -1;
			if(nread > bp->bsize-i){
				bp->state = Binactive;
				nread = -1;
			}
			j = nread <= 0 ? -1 : nread;
			if(j <= 0 && i == 0)
				return p;
			if(j <= 0 && i > 0){
				/*
				 * end of file but no delim. pretend we got a delim
				 * by making the delim \0 and smashing it with nulldelim.
				 */
				j = 1;
				ep = ip;
				delim = '\0';
				nulldelim = 1;
				*ep = delim;	/* there will be room for this */
			}else{
				bp->offset += j;
				ep = memchr(ip, delim, j);
			}
			i += j;
			if(ep) {
				/*
				 * found in new piece
				 * copy back up and reset everything
				 */
				ip = (char*)bp->ebuf - i;
				if(i < bp->bsize){
					memmove(ip, bp->bbuf, i);
					bp->gbuf = (uchar*)ip;
				}
				j = (ep - (char*)bp->bbuf) + 1;
				bp->icount = j - i;
				return badd(p, &bp->rdline, ip, j, delim, nulldelim);
			}
			ip += j;
		}
	
		/*
		 * full buffer without finding; add to user string and continue
		 */
		p = badd(p, &bp->rdline, (char*)bp->bbuf, bp->bsize, 0, 0);
		i = 0;
		bp->icount = 0;
		bp->gbuf = bp->ebuf;
		if(p == nil)
			return nil;
	}
}
