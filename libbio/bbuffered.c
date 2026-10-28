#include	"lib9.h"
#include	<bio.h>


/* Report only nonnegative byte counts representable by the public int result. */
int
Bbuffered(Biobuf *bp)
{
	vlong n;

	switch(bp->state){
	case Bracteof:
	case Bractive:
		n = -(vlong)bp->icount;
		break;
	case Bwactive:
		n = (vlong)bp->bsize+bp->ocount;
		break;
	case Binactive:
		return 0;
	default:
		fprint(2, "Bbuffered: unknown state %d\n", bp->state);
		return 0;
	}
	if(n < 0 || n > 2147483647)
		return Beof;
	return n;
}
