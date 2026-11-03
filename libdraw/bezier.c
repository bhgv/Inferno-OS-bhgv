#include "lib9.h"
#include "draw.h"

#define PINC 32 // Native point-list allocation granularity.

typedef struct Plist Plist;

struct Plist
{
	Point *p; // Owned native vertices, retained until replacement succeeds.
	int np; // Initialized vertex count, or minus one after owned allocation refusal.
};

/* Preserve the old allocation on realloc refusal, then retire the whole failed list once. */
static void
appendpt(Plist *l, Point p)
{
	Point *next;

	if(l->np < 0)
		return;
	if(l->np == 0)
		next = malloc(PINC*sizeof(Point));
	else if(l->np%PINC == 0){
		if(l->np > 0x7fffffff/sizeof(Point)-PINC)
			next = nil;
		else
			next = realloc(l->p, (l->np+PINC)*sizeof(Point));
	}else
		next = l->p;
	if(next == nil){
		free(l->p);
		l->p = nil;
		l->np = -1;
		return;
	}
	l->p = next;
	l->p[l->np++] = p;
}


/* Measure the existing integer squared distance for the retained curve algorithm. */
static int
normsq(Point p)
{
	return p.x*p.x+p.y*p.y;
}


/* Measure the existing control-point distance from a segment. */
static int
psdist(Point p, Point a, Point b)
{
	int num, den;

	p = subpt(p, a);
	b = subpt(b, a);
	num = p.x*b.x + p.y*b.y;
	if(num <= 0)
		return normsq(p);
	den = normsq(b);
	if(num >= den)
		return normsq(subpt(b, p));
	return normsq(subpt(divpt(mulpt(b, num), den), p));
}

/*
 * Convert cubic Bezier curve control points to polyline
 * vertices.  Leaves the last vertex off, so you can continue
 * with another curve.
 */

/* Stop subdivision after allocation refusal; otherwise retain existing curve geometry. */
static void
bpts1(Plist *l, Point p0, Point p1, Point p2, Point p3, int scale)
{
	Point p01, p12, p23, p012, p123, p0123;
	Point tp0, tp1, tp2, tp3;
	if(l->np < 0)
		return;
	tp0=divpt(p0, scale);
	tp1=divpt(p1, scale);
	tp2=divpt(p2, scale);
	tp3=divpt(p3, scale);
	if(psdist(tp1, tp0, tp3)<=1 && psdist(tp2, tp0, tp3)<=1){
		appendpt(l, tp0);
		appendpt(l, tp1);
		appendpt(l, tp2);
	}
	else{
		/*
		 * if scale factor is getting too big for comfort,
		 * rescale now & concede the rounding error
		 */
		if(scale>(1<<12)){
			p0=tp0;
			p1=tp1;
			p2=tp2;
			p3=tp3;
			scale=1;
		}
		p01=addpt(p0, p1);
		p12=addpt(p1, p2);
		p23=addpt(p2, p3);
		p012=addpt(p01, p12);
		p123=addpt(p12, p23);
		p0123=addpt(p012, p123);
		bpts1(l, mulpt(p0, 8), mulpt(p01, 4), mulpt(p012, 2), p0123, scale*8);
		bpts1(l, p0123, mulpt(p123, 2), mulpt(p23, 4), mulpt(p3, 8), scale*8);
	}
}


/* Start subdivision with the existing unit scale. */
static void
bpts(Plist *l, Point p0, Point p1, Point p2, Point p3)
{
	bpts1(l, p0, p1, p2, p3, 1);
}


/* Append the final endpoint after the existing curve subdivision. */
static void
bezierpts(Plist *l, Point p0, Point p1, Point p2, Point p3)
{
	bpts(l, p0, p1, p2, p3);
	appendpt(l, p3);
}


/* Stop scanning spline segments when vertex allocation has failed. */
static void
bezsplinepts(Plist *l, Point *pt, int npt)
{
	Point *p, *ep;
	Point a, b, c, d;
	int periodic;

	if(npt<3)
		return;
	ep = &pt[npt-3];
	periodic = eqpt(pt[0], ep[2]);
	if(periodic){
		a = divpt(addpt(ep[1], pt[0]), 2);
		b = divpt(addpt(ep[1], mulpt(pt[0], 5)), 6);
		c = divpt(addpt(mulpt(pt[0], 5), pt[1]), 6);
		d = divpt(addpt(pt[0], pt[1]), 2);
		bpts(l, a, b, c, d);
	}
	for(p=pt; p<=ep && l->np >= 0; p++){
		if(p==pt && !periodic){
			a = p[0];
			b = divpt(addpt(p[0], mulpt(p[1], 2)), 3);
		}
		else{
			a = divpt(addpt(p[0], p[1]), 2);
			b = divpt(addpt(p[0], mulpt(p[1], 5)), 6);
		}
		if(p==ep && !periodic){
			c = divpt(addpt(mulpt(p[1], 2), p[2]), 3);
			d = p[2];
		}
		else{
			c = divpt(addpt(mulpt(p[1], 5), p[2]), 6);
			d = divpt(addpt(p[1], p[2]), 2);
		}
		bpts(l, a, b, c, d);
	}
	appendpt(l, d);
}


/* Return the owned spline vertex list or nil after allocation refusal. */
int
getbezsplinepts(Point *pt, int npt, Point **pp)
{
	Plist l;
	l.np = 0;
	l.p = nil;
	bezsplinepts(&l, pt, npt);
	*pp  = l.p;
	return l.np;
}


/* Draw the curve with the default compositing operator. */
int
bezier(Image *dst, Point p0, Point p1, Point p2, Point p3, int end0, int end1, int radius, Image *src, Point sp)
{
	return bezierop(dst, p0, p1, p2, p3, end0, end1, radius, src, sp, SoverD);
}


/* Build owned vertices, draw them and release their native storage. */
int
bezierop(Image *dst, Point p0, Point p1, Point p2, Point p3, int end0, int end1, int radius, Image *src, Point sp, Drawop op)
{
	Plist l;

	l.np = 0;
	l.p = nil;
	bezierpts(&l, p0, p1, p2, p3);
	if(l.np == -1)
		return 0;
	if(l.np != 0){
		polyop(dst, l.p, l.np, end0, end1, radius, src, addpt(subpt(sp, p0), l.p[0]), op);
		free(l.p);
	}
	return 1;
}


/* Draw a spline with the default compositing operator. */
int
bezspline(Image *dst, Point *pt, int npt, int end0, int end1, int radius, Image *src, Point sp)
{
	return bezsplineop(dst, pt, npt, end0, end1, radius, src, sp, SoverD);
}


/* Build a spline vertex list and release it after drawing. */
int
bezsplineop(Image *dst, Point *pt, int npt, int end0, int end1, int radius, Image *src, Point sp, Drawop op)
{
	Plist l;

	l.np = 0;
	l.p = nil;
	bezsplinepts(&l, pt, npt);
	if(l.np==-1)
		return 0;
	if(l.np != 0){
		polyop(dst, l.p, l.np, end0, end1, radius, src, addpt(subpt(sp, pt[0]), l.p[0]), op);
		free(l.p);
	}
	return 1;
}


/* Fill the curve with the default compositing operator. */
int
fillbezier(Image *dst, Point p0, Point p1, Point p2, Point p3, int w, Image *src, Point sp)
{
	return fillbezierop(dst, p0, p1, p2, p3, w, src, sp, SoverD);
}


/* Fill the owned curve vertices and release their native storage. */
int
fillbezierop(Image *dst, Point p0, Point p1, Point p2, Point p3, int w, Image *src, Point sp, Drawop op)
{
	Plist l;

	l.np = 0;
	l.p = nil;
	bezierpts(&l, p0, p1, p2, p3);
	if(l.np == -1)
		return 0;
	if(l.np != 0){
		fillpolyop(dst, l.p, l.np, w, src, addpt(subpt(sp, p0), l.p[0]), op);
		free(l.p);
	}
	return 1;
}


/* Fill a spline with the default compositing operator. */
int
fillbezspline(Image *dst, Point *pt, int npt, int w, Image *src, Point sp)
{
	return fillbezsplineop(dst, pt, npt, w, src, sp, SoverD);
}


/* Fill owned spline vertices or return allocation refusal without leaking. */
int
fillbezsplineop(Image *dst, Point *pt, int npt, int w, Image *src, Point sp, Drawop op)
{
	Plist l;

	l.np = 0;
	l.p = nil;
	bezsplinepts(&l, pt, npt);
	if(l.np == -1)
		return 0;
	if(l.np > 0){
		fillpolyop(dst, l.p, l.np, w, src, addpt(subpt(sp, pt[0]), l.p[0]), op);
		free(l.p);
	}
	return 1;
}
