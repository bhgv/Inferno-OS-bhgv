#include	"lib9.h"
#include	<bio.h>


/* Compute logical positions in the wide domain and reject signed overflow. */
vlong
Boffset(Biobuf *bp)
{
	vlong delta, maximum;

	switch(bp->state){
	default:
		fprint(2, "Boffset: unknown state %d\n", bp->state);
		return Beof;
	case Bracteof:
	case Bractive:
		delta = bp->icount;
		break;
	case Bwactive:
		delta = (vlong)bp->bsize+bp->ocount;
		break;
	}
	maximum = (vlong)(~(uvlong)0 >> 1);
	if((delta > 0 && bp->offset > maximum-delta) ||
	   (delta < 0 && bp->offset < (-maximum-1)-delta))
		return Beof;
	return bp->offset+delta;
}
