#include "lib9.h"
#include "draw.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct CurveBlock CurveBlock;

struct CurveBlock {
	void *p; // Owned native allocation under test.
	size_t bytes; // Guarded capacity independently tracked by the fixture.
};

static CurveBlock blocks[8]; // Only real native vertex-list owners are tracked.
static unsigned long checks; // Active predicates independent of NDEBUG.
static int live, allocations, deny, limitalloc; // Native allocation ledger and pre-effect guard controls.
static Point inputs[65536], expected[65536]; // Accessible original inputs and independent reference vertices.
static int wanted, property, draws, filling; // Expected geometry/transport behavior of the current case.
static Point wantedsource; // Source origin derived independently from expected geometry.
static int inputcount; // Current independent input population.
static Image destination, sourceimage; // Bounded native draw transport identities, never device images.


/* Fail semantically before unsafe work; clean private control-test storage before sanitizer exit. */
static void
check(int condition)
{
	int i;

	if(!condition){
		for(i=0; i<8; i++)
			if(blocks[i].p != nil){
				free(blocks[i].p);
				blocks[i].p = nil;
			}
		fprintf(stderr, "curve semantic mismatch after %lu checks\n", checks);
		exit(10);
	}
	checks++;
}


/* Allocate guarded native storage, rejecting attempted work beyond a tested resource guard. */
static void*
curvealloc(size_t bytes)
{
	uchar *p;
	int i;

	check(!deny && bytes <= 1024*1024 && (!limitalloc || allocations < 12));
	p = malloc(bytes+32);
	check(p != nil);
	memset(p+bytes, 0xa5, 32);
	for(i=0; i<8 && blocks[i].p != nil; i++)
		;
	check(i < 8);
	blocks[i].p = p;
	blocks[i].bytes = bytes;
	live++;
	allocations++;
	return p;
}


/* Verify every allocation guard before releasing its sole native owner. */
static void
curvefree(void *p)
{
	int i, j;

	if(p == nil)
		return;
	for(i=0; i<8 && blocks[i].p != p; i++)
		;
	check(i < 8);
	for(j=0; j<32; j++)
		check(((uchar*)p)[blocks[i].bytes+j] == 0xa5);
	free(p);
	blocks[i].p = nil;
	live--;
}


/* Preserve actual realloc ownership and all initialized bytes while moving the native list. */
static void*
curverealloc(void *p, size_t bytes)
{
	void *next;
	size_t count;
	int i;

	for(i=0; i<8 && blocks[i].p != p; i++)
		;
	check(i < 8);
	count = blocks[i].bytes < bytes ? blocks[i].bytes : bytes;
	next = curvealloc(bytes);
	memmove(next, p, count);
	curvefree(p);
	return next;
}


/* Compare exact reference vertices or the bounded convex-hull/endpoint contract for huge spans. */
static void
vertices(Point *points, int count)
{
	int i, minx, maxx, miny, maxy;

	check(count >= 0 && count <= 65536 && (count == 0 || points != nil));
	if(!property){
		check(count == wanted);
		for(i=0; i<count; i++)
			check(points[i].x == expected[i].x && points[i].y == expected[i].y);
		return;
	}
	check(count > 0 && points[0].x == inputs[0].x && points[0].y == inputs[0].y &&
		points[count-1].x == inputs[3].x && points[count-1].y == inputs[3].y);
	minx = maxx = inputs[0].x;
	miny = maxy = inputs[0].y;
	for(i=1; i<4; i++){
		if(inputs[i].x < minx) minx = inputs[i].x;
		if(inputs[i].x > maxx) maxx = inputs[i].x;
		if(inputs[i].y < miny) miny = inputs[i].y;
		if(inputs[i].y > maxy) maxy = inputs[i].y;
	}
	for(i=0; i<count; i++)
		check(points[i].x >= minx && points[i].x <= maxx && points[i].y >= miny && points[i].y <= maxy);
}


/* Observe actual public curve wrappers at a bounded native polygon transport. */
void
polyop(Image *dst, Point *points, int count, int end0, int end1, int radius, Image *src, Point sp, Drawop op)
{
	check(dst == &destination && src == &sourceimage && end0 == 3 && end1 == 5 && radius == 2 && op == SoverD);
	check(sp.x == wantedsource.x && sp.y == wantedsource.y && !filling);
	vertices(points, count);
	draws++;
}


/* Check the corresponding filled transport without substituting curve generation. */
void
fillpolyop(Image *dst, Point *points, int count, int wind, Image *src, Point sp, Drawop op)
{
	check(dst == &destination && src == &sourceimage && wind == 3 && op == SoverD && filling);
	check(sp.x == wantedsource.x && sp.y == wantedsource.y);
	vertices(points, count);
	draws++;
}

CURVE_IMPLEMENTATION


/* Decode independent big-endian signed words without copying host integer representations. */
static int
number(FILE *file)
{
	uchar data[4];
	u32int bits;

	check(fread(data, 1, 4, file) == 4);
	bits = ((u32int)data[0]<<24)|((u32int)data[1]<<16)|((u32int)data[2]<<8)|data[3];
	return bits&(1U<<31) ? (int)((vlong)bits-4294967296LL) : (int)bits;
}


/* Exercise exhausted real list budgets and checked source translations before any draw side effect. */
static void
guards(void)
{
	Plist list;
	Point zero, out, sp, first, vertex, controls[3];
	BPoint wide;

	zero.x = zero.y = 0;
	wide = widept(zero);
	list.p = nil;
	list.np = 0;
	list.capacity = 0;
	list.work = 0;
	deny = 1;
	bpts1(&list, wide, wide, wide, wide, 1, 0);
	check(list.np == -1 && list.p == nil && live == 0);
	list.np = 0;
	list.work = BezierWork;
	bpts1(&list, wide, wide, wide, wide, 1, BezierDepth+1);
	check(list.np == -1 && list.p == nil && live == 0);
	deny = 0;
	list.p = curvealloc(BezierPoints*sizeof(Point));
	list.np = BezierPoints;
	list.capacity = BezierPoints;
	list.work = BezierWork;
	deny = 1;
	appendpt(&list, zero);
	check(list.np == -1 && list.p == nil && live == 0);
	deny = 0;
	sp.x = 0x7fffffff;
	sp.y = -0x7fffffff-1;
	first.x = first.y = -0x7fffffff-1;
	vertex = first;
	check(beziersource(sp, first, vertex, &out) && out.x == sp.x && out.y == sp.y);
	out.x = out.y = 12345;
	vertex.x = 0x7fffffff;
	check(!beziersource(sp, first, vertex, &out) && out.x == 12345 && out.y == 12345);
	check(getbezsplinepts(nil, 3, &list.p) == -1 && list.p == nil && live == 0);
	check(getbezsplinepts(nil, 0, nil) == -1 && live == 0);
	controls[0] = controls[2] = zero;
	controls[1].x = 100;
	controls[1].y = 0;
	sp.x = 0x7fffffff;
	sp.y = 0;
	draws = 0;
	check(bezspline(&destination, controls, 3, 3, 5, 2, &sourceimage, sp) == 0 && draws == 0 && live == 0);
	check(fillbezspline(&destination, controls, 3, 3, &sourceimage, sp) == 0 && draws == 0 && live == 0);
}


/* Read exact independent vertex sequences and execute real private/public native curve bodies. */
int
main(int argc, char **argv)
{
	FILE *file;
	Plist list;
	Point *points, sp;
	int cases, row, mode, count, i, fill, result, completed, refused;

	check(argc == 2 && sizeof(int) == 4 && sizeof(vlong) == 8);
	guards();
	file = fopen(argv[1], "rb");
	check(file != nil);
	cases = number(file);
	completed = refused = 0;
	for(row=0; row<cases; row++){
		mode = number(file);
		inputcount = number(file);
		wanted = number(file);
		property = wanted < 0;
		check(inputcount >= 0 && inputcount <= 65536 && wanted <= 65536);
		for(i=0; i<inputcount; i++){
			inputs[i].x = number(file);
			inputs[i].y = number(file);
		}
		for(i=0; i<wanted; i++){
			expected[i].x = number(file);
			expected[i].y = number(file);
		}
		check(live == 0);
		allocations = 0;
		limitalloc = 1;
		if(mode == 0 || mode == 2){
			check(inputcount == 4);
			list.p = nil;
			list.np = 0;
			list.capacity = 0;
			list.work = BezierWork;
			bezierpts(&list, inputs[0], inputs[1], inputs[2], inputs[3]);
			points = list.p;
			count = list.np;
		}else{
			count = getbezsplinepts(inputs, inputcount, &points);
		}
		if(count == -1){
			check(wanted == -2 && points == nil && live == 0);
			refused++;
			continue;
		}
		vertices(points, count);
		curvefree(points);
		completed++;
		if((mode == 2 || mode == 3) && count > 0){
			sp.x = 0;
			sp.y = -7;
			wantedsource.x = (vlong)sp.x-inputs[0].x+expected[0].x;
			wantedsource.y = (vlong)sp.y-inputs[0].y+expected[0].y;
			for(fill=0; fill<2; fill++){
				allocations = 0;
				draws = 0;
				filling = fill;
				if(mode == 3){
					if(fill) result = fillbezspline(&destination, inputs, inputcount, 3, &sourceimage, sp);
					else result = bezspline(&destination, inputs, inputcount, 3, 5, 2, &sourceimage, sp);
				}else if(fill) result = fillbezier(&destination, inputs[0], inputs[1], inputs[2], inputs[3], 3, &sourceimage, sp);
				else result = bezier(&destination, inputs[0], inputs[1], inputs[2], inputs[3], 3, 5, 2, &sourceimage, sp);
				check(result == 1 && draws == 1 && live == 0);
			}
		}
		check(live == 0);
	}
	check(fgetc(file) == EOF && fclose(file) == 0 && live == 0);
	printf("PASS bounded curves: %d exact/property cases, %d bounded refusals, %lu predicates; real native geometry/owners\n", completed, refused, checks);
	return 0;
}
