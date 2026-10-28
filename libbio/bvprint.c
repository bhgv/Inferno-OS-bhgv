#include	"lib9.h"
#include	<bio.h>


/* Validate output geometry before constructing pointers or dispatching formatters. */
static int
fmtBbuffer(Biobuf *bp)
{
	uintptr start, end;

	if(bp == nil || bp->state != Bwactive || bp->bsize <= 0 ||
	   bp->ocount > 0 || bp->ocount < -bp->bsize ||
	   bp->bbuf == nil || bp->ebuf == nil)
		return 0;
	start = (uintptr)bp->bbuf;
	end = (uintptr)bp->ebuf;
	return end >= start && end-start == (uintptr)bp->bsize;
}


/* Keep every published formatter cursor inside the current output buffer. */
static int
fmtBcursor(Biobuf *bp, Fmt *f)
{
	return fmtBbuffer(bp) && f->stop == bp->ebuf &&
	       (uintptr)f->to >= (uintptr)bp->bbuf && (uintptr)f->to <= (uintptr)bp->ebuf;
}


/* Flush only a matching output view; retain Bflush's compacted suffix on failure. */
static int
fmtBflush(Fmt *f)
{
	Biobuf *bp;

	bp = f->farg;
	if(!fmtBcursor(bp, f))
		return 0;
	bp->ocount = -(int)((uintptr)f->stop-(uintptr)f->to);
	if(Bflush(bp) < 0 || !fmtBbuffer(bp))
		return 0;
	f->stop = bp->ebuf;
	f->to = (char*)f->stop + bp->ocount;
	f->start = f->to;
	return 1;
}


/* Format only into active output storage and publish a validated final cursor. */
int
Bvprint(Biobuf *bp, char *fmt, va_list arg)
{
	int n;
	Fmt f;

	if(fmt == nil || (uintptr)fmt == (uintptr)-1 || !fmtBbuffer(bp))
		return Beof;
	f.runes = 0;
	f.stop = bp->ebuf;
	f.start = (char*)f.stop + bp->ocount;
	f.to = f.start;
	f.flush = fmtBflush;
	f.farg = bp;
	f.nfmt = 0;
#ifdef va_copy
	va_copy(f.args, arg);
#else
	f.args = arg;
#endif
	n = dofmt(&f, fmt);
#ifdef va_copy
	va_end(f.args);
#endif
	if(!fmtBcursor(bp, &f))
		return Beof;
	bp->ocount = -(int)((uintptr)f.stop-(uintptr)f.to);
	return n;
}
