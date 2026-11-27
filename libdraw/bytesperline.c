#include "lib9.h"
#include "draw.h"


/* Count touched units with mathematical ceil/floor, including negative coordinates.
 * Widen products before arithmetic; the public int result retains its low bits. */
static int
unitsperline(Rectangle r, int d, int bitsperunit)
{
	vlong left, right, first, last;

	left = (vlong)r.min.x*d;
	right = (vlong)r.max.x*d;
	first = left/bitsperunit;
	if(left%bitsperunit < 0)
		first--;
	last = right/bitsperunit;
	if(right%bitsperunit > 0)
		last++;
	return (int)(uint)(last-first);
}



/* Report native ulong units using the same bounded coordinate arithmetic. */
int
wordsperline(Rectangle r, int d)
{
	return unitsperline(r, d, 8*sizeof(ulong));
}


/* Report byte units without signed multiplication or negation overflow. */
int
bytesperline(Rectangle r, int d)
{
	return unitsperline(r, d, 8);
}
