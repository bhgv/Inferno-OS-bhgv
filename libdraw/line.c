#include "lib9.h"
#include "draw.h"
#include "draw-packet.h"


/* Draw an outlined segment using the default compositing operator. */
void
line(Image *dst, Point p0, Point p1, int end0, int end1, int radius, Image *src, Point sp)
{
	lineop(dst, p0, p1, end0, end1, radius, src, sp, SoverD);
}


/* Refuse invalid image/radius/operator input before reserving the complete line packet. */
void
lineop(Image *dst, Point p0, Point p1, int end0, int end1, int radius, Image *src, Point sp, Drawop op)
{
	uchar *a;

	if(dst == nil || src == nil || dst->display == nil || dst->display != src->display || radius < 0)
		return;
	a = drawpacket(dst->display, 45, op);
	if(a == 0){
		_drawprint(2, "image line: %r\n");
		return;
	}
	a[0] = 'L';
	BPLONG(a+1, dst->id);
	BPLONG(a+5, p0.x);
	BPLONG(a+9, p0.y);
	BPLONG(a+13, p1.x);
	BPLONG(a+17, p1.y);
	BPLONG(a+21, end0);
	BPLONG(a+25, end1);
	BPLONG(a+29, radius);
	BPLONG(a+33, src->id);
	BPLONG(a+37, sp.x);
	BPLONG(a+41, sp.y);
}
