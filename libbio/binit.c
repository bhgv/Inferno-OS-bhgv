#include	"lib9.h"
#include	<bio.h>

enum
{
	MAXBUFS	= 20
};

static	Biobuf*	wbufs[MAXBUFS];
static	int		atexitflag;

/* Flush registered output buffers when the process exits. */
static void
batexit(void)
{
	Biobuf *bp;
	int i;

	for(i=0; i<nelem(wbufs); i++) {
		bp = wbufs[i];
		if(bp != 0) {
			wbufs[i] = 0;
			Bflush(bp);
		}
	}
}

/* Remove every registration before a buffer is reused or released. */
static void
deinstall(Biobuf *bp)
{
	int i;

	for(i=0; i<nelem(wbufs); i++)
		if(wbufs[i] == bp)
			wbufs[i] = 0;
}

/* Register an output buffer once for process-exit flushing. */
static void
install(Biobuf *bp)
{
	int i;

	deinstall(bp);
	for(i=0; i<nelem(wbufs); i++)
		if(wbufs[i] == 0) {
			wbufs[i] = bp;
			break;
		}
	if(atexitflag == 0) {
		atexitflag = 1;
		atexit(batexit);
	}
}

/* Reject invalid storage before changing the buffer or its exit registration. */
int
Binits(Biobuf *bp, int f, int mode, uchar *p, int size)
{

	if(p == nil || size <= Bungetsize)
		return Beof;

	switch(mode&~(OCEXEC|ORCLOSE|OTRUNC)) {
	default:
		fprint(2, "Bopen: unknown mode %d\n", mode);
		return Beof;

	case OREAD:
		deinstall(bp);
		bp->state = Bractive;
		bp->ocount = 0;
		break;

	case OWRITE:
		install(bp);
		bp->state = Bwactive;
		break;
	}
	p += Bungetsize;	/* make room for Bungets */
	size -= Bungetsize;
	if(bp->state == Bwactive)
		bp->ocount = -size;
	bp->bbuf = p;
	bp->ebuf = p+size;
	bp->bsize = size;
	bp->icount = 0;
	bp->gbuf = bp->ebuf;
	bp->fid = f;
	bp->flag = 0;
	bp->rdline = 0;
	bp->offset = 0;
	bp->runesize = 0;
	return 0;
}

/* Initialize a buffer using its embedded storage. */
int
Binit(Biobuf *bp, int f, int mode)
{
	return Binits(bp, f, mode, bp->b, sizeof(bp->b));
}

/* Release every acquired descriptor and buffer when initialization fails. */
Biobuf*
Bopen(char *name, int mode)
{
	Biobuf *bp;
	int f;

	switch(mode&~(OCEXEC|ORCLOSE|OTRUNC)) {
	default:
		fprint(2, "Bopen: unknown mode %d\n", mode);
		return 0;

	case OREAD:
		f = open(name, OREAD);
		break;

	case OWRITE:
		f = create(name, mode, 0666);
		break;
	}
	if(f < 0)
		return 0;
	bp = malloc(sizeof(Biobuf));
	if(bp == nil){
		close(f);
		return 0;
	}
	if(Binits(bp, f, mode, bp->b, sizeof(bp->b)) == Beof){
		close(f);
		free(bp);
		return 0;
	}
	bp->flag = Bmagic;
	return bp;
}

/* Release owned storage and report both flush and descriptor-close failures. */
int
Bterm(Biobuf *bp)
{
	int r;

	deinstall(bp);
	r = Bflush(bp);
	if(bp->flag == Bmagic) {
		bp->flag = 0;
		if(close(bp->fid) < 0)
			r = Beof;
		bp->fid = -1;			/* prevent accidents */
		free(bp);
	}
	/* otherwise opened with Binit(s) */
	return r;
}
