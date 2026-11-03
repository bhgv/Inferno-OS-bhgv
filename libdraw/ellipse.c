#include "lib9.h"
#include "draw.h"
#include "draw-packet.h"



/* Validate native ellipse input and encode arc flags as unsigned wire bits. */
static void
doellipse(int cmd, Image *dst, Point *c, int xr, int yr, int thick, Image *src, Point *sp, u32int alpha, int phi, Drawop op)
{
	uchar *a;

	if(dst == nil || src == nil || dst->display == nil || dst->display != src->display ||
	   c == nil || sp == nil || xr < 0 || yr < 0 || thick < 0)
		return;
	a = drawpacket(dst->display, 45, op);
	if(a == 0){
		_drawprint(2, "image ellipse: %r\n");
		return;
	}
	a[0] = cmd;
	BPLONG(a+1, dst->id);
	BPLONG(a+5, src->id);
	BPLONG(a+9, c->x);
	BPLONG(a+13, c->y);
	BPLONG(a+17, xr);
	BPLONG(a+21, yr);
	BPLONG(a+25, thick);
	BPLONG(a+29, sp->x);
	BPLONG(a+33, sp->y);
	BPLONG(a+37, alpha);
	BPLONG(a+41, phi);
}


/* Encode the ellipse native primitive through the checked packet builder. */
void
ellipse(Image *dst, Point c, int a, int b, int thick, Image *src, Point sp)
{
	doellipse('e', dst, &c, a, b, thick, src, &sp, 0, 0, SoverD);
}


/* Encode the ellipseop native primitive through the checked packet builder. */
void
ellipseop(Image *dst, Point c, int a, int b, int thick, Image *src, Point sp, Drawop op)
{
	doellipse('e', dst, &c, a, b, thick, src, &sp, 0, 0, op);
}


/* Encode the fillellipse native primitive through the checked packet builder. */
void
fillellipse(Image *dst, Point c, int a, int b, Image *src, Point sp)
{
	doellipse('E', dst, &c, a, b, 0, src, &sp, 0, 0, SoverD);
}


/* Encode the fillellipseop native primitive through the checked packet builder. */
void
fillellipseop(Image *dst, Point c, int a, int b, Image *src, Point sp, Drawop op)
{
	doellipse('E', dst, &c, a, b, 0, src, &sp, 0, 0, op);
}


/* Encode the arc native primitive through the checked packet builder. */
void
arc(Image *dst, Point c, int a, int b, int thick, Image *src, Point sp, int alpha, int phi)
{
	doellipse('e', dst, &c, a, b, thick, src, &sp, (u32int)alpha|0x80000000U, phi, SoverD);
}


/* Encode the arcop native primitive through the checked packet builder. */
void
arcop(Image *dst, Point c, int a, int b, int thick, Image *src, Point sp, int alpha, int phi, Drawop op)
{
	doellipse('e', dst, &c, a, b, thick, src, &sp, (u32int)alpha|0x80000000U, phi, op);
}


/* Encode the fillarc native primitive through the checked packet builder. */
void
fillarc(Image *dst, Point c, int a, int b, Image *src, Point sp, int alpha, int phi)
{
	doellipse('E', dst, &c, a, b, 0, src, &sp, (u32int)alpha|0x80000000U, phi, SoverD);
}


/* Encode the fillarcop native primitive through the checked packet builder. */
void
fillarcop(Image *dst, Point c, int a, int b, Image *src, Point sp, int alpha, int phi, Drawop op)
{
	doellipse('E', dst, &c, a, b, 0, src, &sp, (u32int)alpha|0x80000000U, phi, op);
}
