#include	"lib9.h"
#include	<bio.h>

/* Seek only after validated arithmetic/flush, retaining read buffers on native failure. */
vlong
Bseek(Biobuf *bp, vlong offset, int base)
{
	vlong n, current, maximum;
	uvlong distance;

	switch(bp->state) {
	default:
		fprint(2, "Bseek: unknown state %d\n", bp->state);
		return Beof;

	case Bracteof:
	case Bractive:
		n = offset;
		if(base == 1){
			current = Boffset(bp);
			maximum = (vlong)(~(uvlong)0 >> 1);
			if((offset > 0 && current > maximum-offset) ||
			   (offset < 0 && current < (-maximum-1)-offset))
				return Beof;
			n += current;
			base = 0;
		}

		/* Subtract only after ordering, in an unsigned type that cannot wrap here. */
		if(base == 0 && n <= bp->offset){
			distance = (uvlong)bp->offset-(uvlong)n;
			if(distance <= (uvlong)(bp->ebuf-bp->gbuf)){
				bp->icount = -(int)distance;
				bp->state = Bractive;
				return n;
			}
		}

		n = seek(bp->fid, n, base);
		if(n == Beof)
			return Beof;
		bp->icount = 0;
		bp->gbuf = bp->ebuf;
		bp->state = Bractive;
		break;

	case Bwactive:
		if(Bflush(bp) == Beof)
			return Beof;
		n = seek(bp->fid, offset, base);
		if(n == Beof)
			return Beof;
		break;
	}
	bp->offset = n;
	return n;
}
