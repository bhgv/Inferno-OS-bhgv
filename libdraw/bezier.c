#include "lib9.h"
#include "draw.h"

#define PINC 32 // Initial native point-list capacity.
#define BezierPoints 65536 // Maximum vertices representable by one polygon message.
#define BezierWork 1048576U // Per-list subdivision admission, including all spline segments.
#define BezierDepth 64U // Maximum live subdivision depth before clean refusal.
#define BezierDistance 1048576LL // Exact projection domain keeping cubic products within signed64.

typedef struct Plist Plist;

struct Plist {
	Point *p; // Owned native vertices, retained until replacement succeeds.
	int np; // Initialized count, or minus one after clean allocation/resource refusal.
	unsigned int capacity; // Actual native vertex slots, grown geometrically up to the output bound.
	unsigned int work; // Remaining per-list subdivision admissions, never process-global.
};

typedef struct BPoint BPoint;

struct BPoint {
	vlong x; // Widened fixed-point coordinate; scale never exceeds32768.
	vlong y; // Convex subdivision bounds intermediate magnitude below2^47.
};


/* Retire the sole native vertex owner once and leave all later appends inert. */
static void
failpts(Plist *l)
{
	free(l->p);
	l->p = nil;
	l->capacity = 0;
	l->np = -1;
}


/* Bound wire-sized output before growth and preserve the old owner on realloc refusal. */
static void
appendpt(Plist *l, Point p)
{
	Point *next;
	unsigned int capacity;

	if(l->np < 0)
		return;
	if(l->np >= BezierPoints){
		failpts(l);
		return;
	}
	if((unsigned int)l->np == l->capacity){
		capacity = l->capacity == 0 ? PINC : l->capacity*2;
		if(capacity > BezierPoints)
			capacity = BezierPoints;
		if(l->p == nil)
			next = malloc(capacity*sizeof(Point));
		else
			next = realloc(l->p, capacity*sizeof(Point));
		if(next == nil){
			failpts(l);
			return;
		}
		l->p = next;
		l->capacity = capacity;
	}
	l->p[l->np++] = p;
}

/* Square only bounded relative components established by the distance calculation. */
static vlong
normsq(vlong x, vlong y)
{
	return x*x+y*y;
}


/* Retain integer projection semantics in a safe domain; larger spans subdivide conservatively. */
static vlong
psdist(Point p, Point a, Point b)
{
	vlong px, py, bx, by, num, den, x, y;

	px = (vlong)p.x-a.x;
	py = (vlong)p.y-a.y;
	bx = (vlong)b.x-a.x;
	by = (vlong)b.y-a.y;
	if((px == 0 && py == 0) || (px == bx && py == by))
		return 0;
	if(px < -BezierDistance || px > BezierDistance || py < -BezierDistance || py > BezierDistance ||
	   bx < -BezierDistance || bx > BezierDistance || by < -BezierDistance || by > BezierDistance)
		return 2;
	num = px*bx+py*by;
	if(num <= 0)
		return normsq(px, py);
	den = normsq(bx, by);
	if(num >= den)
		return normsq(bx-px, by-py);
	x = bx*num/den-px;
	y = by*num/den-py;
	return normsq(x, y);
}


/* Widen native control coordinates before any fixed-point sum or multiplication. */
static BPoint
widept(Point p)
{
	BPoint q;

	q.x = p.x;
	q.y = p.y;
	return q;
}


/* Add bounded fixed-point coordinates without narrowing through native Point fields. */
static BPoint
addwide(BPoint a, BPoint b)
{
	a.x += b.x;
	a.y += b.y;
	return a;
}


/* Scale bounded convex control coordinates while preserving their existing integer representation. */
static BPoint
mulwide(BPoint p, int n)
{
	p.x *= n;
	p.y *= n;
	return p;
}


/* Validate the narrowed native result; only positive bounded scales are internally generated. */
static int
nativept(BPoint p, int scale, Point *result)
{
	vlong x, y;

	if(scale <= 0)
		return 0;
	x = p.x/scale;
	y = p.y/scale;
	if(x < (-0x7fffffff-1) || x > 0x7fffffff || y < (-0x7fffffff-1) || y > 0x7fffffff)
		return 0;
	result->x = x;
	result->y = y;
	return 1;
}


/* Preserve fixed-point subdivision order with per-list work/output and per-path depth bounds. */
static void
bpts1(Plist *l, BPoint p0, BPoint p1, BPoint p2, BPoint p3, int scale, unsigned int depth)
{
	BPoint p01, p12, p23, p012, p123, p0123;
	Point tp0, tp1, tp2, tp3;

	if(l->np < 0)
		return;
	if(l->work == 0 || depth > BezierDepth){
		failpts(l);
		return;
	}
	l->work--;
	if(!nativept(p0, scale, &tp0) || !nativept(p1, scale, &tp1) ||
	   !nativept(p2, scale, &tp2) || !nativept(p3, scale, &tp3)){
		failpts(l);
		return;
	}
	if(psdist(tp1, tp0, tp3) <= 1 && psdist(tp2, tp0, tp3) <= 1){
		appendpt(l, tp0);
		appendpt(l, tp1);
		appendpt(l, tp2);
		return;
	}
	if(scale > (1<<12)){
		p0 = widept(tp0);
		p1 = widept(tp1);
		p2 = widept(tp2);
		p3 = widept(tp3);
		scale = 1;
	}
	p01 = addwide(p0, p1);
	p12 = addwide(p1, p2);
	p23 = addwide(p2, p3);
	p012 = addwide(p01, p12);
	p123 = addwide(p12, p23);
	p0123 = addwide(p012, p123);
	bpts1(l, mulwide(p0, 8), mulwide(p01, 4), mulwide(p012, 2), p0123, scale*8, depth+1);
	bpts1(l, p0123, mulwide(p123, 2), mulwide(p23, 4), mulwide(p3, 8), scale*8, depth+1);
}


/* Start one curve inside the current list's shared work and output budgets. */
static void
bpts(Plist *l, Point p0, Point p1, Point p2, Point p3)
{
	bpts1(l, widept(p0), widept(p1), widept(p2), widept(p3), 1, 0);
}


/* Append the original final endpoint only if subdivision completed successfully. */
static void
bezierpts(Plist *l, Point p0, Point p1, Point p2, Point p3)
{
	bpts(l, p0, p1, p2, p3);
	appendpt(l, p3);
}


/* Compute existing spline convex averages without overflowing at large coordinate translations. */
static Point
blendpt(Point a, int wa, Point b, int wb, int divisor)
{
	Point p;

	p.x = ((vlong)a.x*wa+(vlong)b.x*wb)/divisor;
	p.y = ((vlong)a.y*wa+(vlong)b.y*wb)/divisor;
	return p;
}


/* Validate the source span and share one bounded work/output list across every spline segment. */
static void
bezsplinepts(Plist *l, Point *pt, int npt)
{
	Point *p, *ep;
	Point a, b, c, d;
	int periodic;

	if(npt < 0 || npt > BezierPoints || (npt != 0 && (pt == nil ||
	   (uintptr)pt > (uintptr)-1-(ulong)npt*sizeof(Point)))){
		failpts(l);
		return;
	}
	if(npt < 3)
		return;
	ep = &pt[npt-3];
	periodic = eqpt(pt[0], ep[2]);
	if(periodic){
		a = blendpt(ep[1], 1, pt[0], 1, 2);
		b = blendpt(ep[1], 1, pt[0], 5, 6);
		c = blendpt(pt[0], 5, pt[1], 1, 6);
		d = blendpt(pt[0], 1, pt[1], 1, 2);
		bpts(l, a, b, c, d);
	}
	for(p=pt; p<=ep && l->np >= 0; p++){
		if(p == pt && !periodic){
			a = p[0];
			b = blendpt(p[0], 1, p[1], 2, 3);
		}else{
			a = blendpt(p[0], 1, p[1], 1, 2);
			b = blendpt(p[0], 1, p[1], 5, 6);
		}
		if(p == ep && !periodic){
			c = blendpt(p[1], 2, p[2], 1, 3);
			d = p[2];
		}else{
			c = blendpt(p[1], 5, p[2], 1, 6);
			d = blendpt(p[1], 1, p[2], 1, 2);
		}
		bpts(l, a, b, c, d);
	}
	appendpt(l, d);
}


/* Check translated source coordinates before publishing native geometry to a draw transport. */
static int
beziersource(Point sp, Point first, Point vertex, Point *result)
{
	vlong x, y;

	x = (vlong)sp.x-first.x+vertex.x;
	y = (vlong)sp.y-first.y+vertex.y;
	if(x < (-0x7fffffff-1) || x > 0x7fffffff || y < (-0x7fffffff-1) || y > 0x7fffffff)
		return 0;
	result->x = x;
	result->y = y;
	return 1;
}


/* Return the owned spline vertex list or nil after allocation refusal. */
int
getbezsplinepts(Point *pt, int npt, Point **pp)
{
	Plist l;

	if(pp == nil)
		return -1;
	l.np = 0;
	l.p = nil;
	l.capacity = 0;
	l.work = BezierWork;
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
	Point source;

	l.np = 0;
	l.p = nil;
	l.capacity = 0;
	l.work = BezierWork;
	bezierpts(&l, p0, p1, p2, p3);
	if(l.np == -1)
		return 0;
	if(l.np != 0){
		if(!beziersource(sp, p0, l.p[0], &source)){
			free(l.p);
			return 0;
		}
		polyop(dst, l.p, l.np, end0, end1, radius, src, source, op);
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
	Point source;

	l.np = 0;
	l.p = nil;
	l.capacity = 0;
	l.work = BezierWork;
	bezsplinepts(&l, pt, npt);
	if(l.np==-1)
		return 0;
	if(l.np != 0){
		if(!beziersource(sp, pt[0], l.p[0], &source)){
			free(l.p);
			return 0;
		}
		polyop(dst, l.p, l.np, end0, end1, radius, src, source, op);
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
	Point source;

	l.np = 0;
	l.p = nil;
	l.capacity = 0;
	l.work = BezierWork;
	bezierpts(&l, p0, p1, p2, p3);
	if(l.np == -1)
		return 0;
	if(l.np != 0){
		if(!beziersource(sp, p0, l.p[0], &source)){
			free(l.p);
			return 0;
		}
		fillpolyop(dst, l.p, l.np, w, src, source, op);
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
	Point source;

	l.np = 0;
	l.p = nil;
	l.capacity = 0;
	l.work = BezierWork;
	bezsplinepts(&l, pt, npt);
	if(l.np == -1)
		return 0;
	if(l.np > 0){
		if(!beziersource(sp, pt[0], l.p[0], &source)){
			free(l.p);
			return 0;
		}
		fillpolyop(dst, l.p, l.np, w, src, source, op);
		free(l.p);
	}
	return 1;
}
