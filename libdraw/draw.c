#include "lib9.h"
#include "draw.h"
#include "draw-packet.h"


/* Preserve the legacy standalone operator helper for commands not yet using atomic reservation. */
void
_setdrawop(Display *d, Drawop op)
{
	uchar *a;

	if(op != SoverD){
		a = bufimage(d, 1+1);
		if(a == 0)
			return;
		a[0] = 'O';
		a[1] = op;
	}
}
		

/* Resolve defaults and validate displays before reserving one complete draw command. */
static void
draw1(Image *dst, Rectangle *r, Image *src, Point *p0, Image *mask, Point *p1, Drawop op)
{
	uchar *a;

	if(dst == nil || dst->display == nil || r == nil || p0 == nil || p1 == nil ||
	   r->min.x > r->max.x || r->min.y > r->max.y)
		return;
	if(src == nil)
		src = dst->display->black;
	if(mask == nil)
		mask = dst->display->opaque;
	if(src == nil || mask == nil || dst->display != src->display || dst->display != mask->display)
		return;
	a = drawpacket(dst->display, 45, op);
	if(a == nil)
		return;
	a[0] = 'd';
	BPLONG(a+1, dst->id);
	BPLONG(a+5, src->id);
	BPLONG(a+9, mask->id);
	BPLONG(a+13, r->min.x);
	BPLONG(a+17, r->min.y);
	BPLONG(a+21, r->max.x);
	BPLONG(a+25, r->max.y);
	BPLONG(a+29, p0->x);
	BPLONG(a+33, p0->y);
	BPLONG(a+37, p1->x);
	BPLONG(a+41, p1->y);
}


/* Draw a rectangle with shared source and mask origins and default compositing. */
void
draw(Image *dst, Rectangle r, Image *src, Image *mask, Point p1)
{
	draw1(dst, &r, src, &p1, mask, &p1, SoverD);
}


/* Draw a rectangle with shared origins and an explicit compositing operator. */
void
drawop(Image *dst, Rectangle r, Image *src, Image *mask, Point p1, Drawop op)
{
	draw1(dst, &r, src, &p1, mask, &p1, op);
}


/* Draw with independent source and mask origins and default compositing. */
void
gendraw(Image *dst, Rectangle r, Image *src, Point p0, Image *mask, Point p1)
{
	draw1(dst, &r, src, &p0, mask, &p1, SoverD);
}


/* Draw with independent source and mask origins and explicit compositing. */
void
gendrawop(Image *dst, Rectangle r, Image *src, Point p0, Image *mask, Point p1, Drawop op)
{
	draw1(dst, &r, src, &p0, mask, &p1, op);
}
