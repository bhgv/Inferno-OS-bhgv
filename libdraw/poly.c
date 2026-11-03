#include "lib9.h"
#include "draw.h"


/* Encode a signed7 delta or exact signed23 absolute coordinate without signed overflow. */
static uchar*
addcoord(uchar *p, int oldx, int newx)
{
	vlong dx;
	u32int value;

	dx = (vlong)newx-oldx;
	if(dx >= -64 && dx <= 63)
		*p++ = (uvlong)dx&0x7f;
	else{
		if(newx < -4194304 || newx > 4194303)
			return nil;
		value = (u32int)newx;
		*p++ = 0x80|(value&0x7f);
		*p++ = value>>7;
		*p++ = value>>15;
	}
	return p;
}


/* Bound the wire count and temporary storage; emit nothing if any coordinate cannot be encoded. */
static void
dopoly(int cmd, Image *dst, Point *pp, int np, int end0, int end1, int radius, Image *src, Point *sp, Drawop op)
{
	uchar *a, *t, *u;
	Point point;
	int i, ox, oy;

	if(np <= 0 || np > 65536 || pp == nil || dst == nil || src == nil ||
	   dst->display == nil || dst->display != src->display ||
	   (uintptr)pp > (uintptr)-1-(ulong)np*sizeof(Point))
		return;
	t = malloc((ulong)np*6);
	if(t == nil)
		return;
	u = t;
	ox = oy = 0;
	for(i=0; i<np; i++){
		memmove(&point, (uchar*)pp+(ulong)i*sizeof(Point), sizeof(point));
		u = addcoord(u, ox, point.x);
		if(u == nil){
			free(t);
			return;
		}
		ox = point.x;
		u = addcoord(u, oy, point.y);
		if(u == nil){
			free(t);
			return;
		}
		oy = point.y;
	}
	_setdrawop(dst->display, op);
	a = bufimage(dst->display, 31+(u-t));
	if(a == nil){
		free(t);
		_drawprint(2, "image poly: %r\n");
		return;
	}
	a[0] = cmd;
	BPLONG(a+1, dst->id);
	BPSHORT(a+5, np-1);
	BPLONG(a+7, end0);
	BPLONG(a+11, end1);
	BPLONG(a+15, radius);
	BPLONG(a+19, src->id);
	BPLONG(a+23, sp->x);
	BPLONG(a+27, sp->y);
	memmove(a+31, t, u-t);
	free(t);
}


/* Draw an outlined polygon using the default compositing operator. */
void
poly(Image *dst, Point *p, int np, int end0, int end1, int radius, Image *src, Point sp)
{
	dopoly('p', dst, p, np, end0, end1, radius, src, &sp, SoverD);
}


/* Draw an outlined polygon with an explicit compositing operator. */
void
polyop(Image *dst, Point *p, int np, int end0, int end1, int radius, Image *src, Point sp, Drawop op)
{
	dopoly('p', dst, p, np, end0, end1, radius, src, &sp, op);
}


/* Fill a polygon using the default compositing operator and winding rule. */
void
fillpoly(Image *dst, Point *p, int np, int wind, Image *src, Point sp)
{
	dopoly('P', dst, p, np, wind, 0, 0, src, &sp, SoverD);
}


/* Fill a polygon with an explicit compositing operator and winding rule. */
void
fillpolyop(Image *dst, Point *p, int np, int wind, Image *src, Point sp, Drawop op)
{
	dopoly('P', dst, p, np, wind, 0, 0, src, &sp, op);
}
