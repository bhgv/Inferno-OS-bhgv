#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "draw.h"
#include "kernel.h"
#include "draw-native64.h"
#include <stdio.h>
#include <limits.h>
#include <stddef.h>

static Display display, other; // Independent native identities at the bounded lookup boundary.
static Image native[2]; // Actual C image records, not managed Heap objects.
static Draw_Image objects[2]; // Borrowed caller identities, resolved only by checkimage.
static int checks, cases, lookups, locks, unlocks, draws, flushes, held; // Active predicates and service observations.
static int variant, mutation, local, alias, foreign, accepted, requestedop; // Independent dispatch expectations.
static Point expected[5]; // Four control points and one source origin.
static int end0, end1, radius, wind; // Exact signed32 caller scalars.
static void *framebytes; // Live caller frame, deliberately overwritten by controlled service callbacks.
static ulong frameextent; // Exact generated union capacity, not arbitrary pointer provenance.

union CurveFrame {
	F_Draw_Image_bezier outline; // Default outlined cubic frame.
	F_Draw_Image_bezierop outlineop; // Explicit outlined cubic frame.
	F_Draw_Image_fillbezier fill; // Default filled cubic frame.
	F_Draw_Image_fillbezierop fillop; // Explicit filled cubic frame.
};


/* Refuse only a named semantic predicate; no assert or sanitizer failure can satisfy a control. */
static void
check(int condition, char *stage)
{
	if(!condition){
		fprintf(stderr,"Bezier consumer mismatch: %s\n",stage);
		exit(10);
	}
	checks++;
}


/* Resolve known borrowed VM identities without inventing Heap headers or ownership. */
Image*
checkimage(Draw_Image *image)
{
	Image *resolved;

	check(image == objects || image == objects+1,"lookup identity");
	resolved = image == objects ? native : native+1;
	lookups++;
	if(lookups == 1 && mutation == 1)
		memset(framebytes,0xa5,frameextent);
	return resolved;
}


/* Model both ordinary exclusion and the native local-display no-lock path. */
int
lockdisplay(Display *d)
{
	check(d == &display && held == 0,"lock identity");
	locks++;
	if(mutation == 2)
		memset(framebytes,0xa5,frameextent);
	held = !local;
	return !local;
}


/* Release only an actually acquired display exclusion. */
void
unlockdisplay(Display *d)
{
	check(d == &display && held == 1,"unlock identity");
	unlocks++;
	held = 0;
}


/* The consumer must use its captured destination, even after a callback changes caller storage. */
void
checkflush(Draw_Image *image)
{
	check(image == objects,"flush identity");
	check(held == !local,"flush exclusion");
	flushes++;
}


/* Compare point fields directly, independent from frame offsets or the consumer's IPOINT macro. */
static int
samepoint(Point a, Point b)
{
	return a.x == b.x && a.y == b.y;
}


/* Observe native C dispatch arguments; subdivision/rasterization remains a separately tested implementation. */
static void
curvearguments(Image *dst, Point a, Point b, Point c, Point d, Image *src, Point sp, Drawop op)
{
	check(accepted,"dispatch admission");
	check(dst == native && src == native+(alias ? 0 : 1),"source identity");
	check(samepoint(a,expected[0]) && samepoint(b,expected[1]) &&
		samepoint(c,expected[2]) && samepoint(d,expected[3]) && samepoint(sp,expected[4]),"curve arguments");
	check(op == (variant%2 ? requestedop : SoverD),"operator argument");
	check(held == !local,"native exclusion");
	draws++;
}


/* The public VM entry ignores the native curve status; exercise success and refusal without inventing acknowledgment. */
int
bezierop(Image *dst, Point a, Point b, Point c, Point d, int first, int last, int width, Image *src, Point sp, Drawop op)
{
	curvearguments(dst,a,b,c,d,src,sp,op);
	check(variant < 2 && first == end0 && last == end1 && width == radius,"outline scalars");
	return mutation%2 ? 0 : 1;
}


/* Preserve all winding bits and control points through the filled native C boundary. */
int
fillbezierop(Image *dst, Point a, Point b, Point c, Point d, int winding, Image *src, Point sp, Drawop op)
{
	curvearguments(dst,a,b,c,d,src,sp,op);
	check(variant >= 2 && winding == wind,"fill scalars");
	return mutation%2 ? 0 : 1;
}

IMPLEMENTATION


/* Exercise every actual generated entry at signed32 boundaries and with caller-storage changes. */
static void
onecase(int row)
{
	struct {
		uchar before[32]; // Independent guard before the complete generated frame.
		union CurveFrame frame; // Aligned real generated Frame64 storage.
		uchar after[32]; // Independent guard after the complete frame.
	} storage;
	F_Draw_Image_bezier *outline;
	F_Draw_Image_fillbezier *fill;
	int index, op;
	static int values[] = {INT_MIN,INT_MIN+1,-4194304,-1,0,1,4194303,INT_MAX}; // Full signed32 point/scalar patterns.
	static int widths[] = {-1,0,1,255,INT_MAX,INT_MIN}; // Native radius admission boundaries.
	static int operators[] = {-1,0,1,2,3,4,5,6,7,8,9,10,11,12,INT_MIN,INT_MAX}; // Valid operators and explicit refusals.

	memset(&storage,0,sizeof(storage));
	memset(storage.before,0x5a,sizeof(storage.before));
	memset(storage.after,0x5a,sizeof(storage.after));
	for(index=0; index<5; index++){
		expected[index].x = values[(row+index)%nelem(values)];
		expected[index].y = values[(row+2*index+3)%nelem(values)];
	}
	end0 = values[row%nelem(values)];
	end1 = values[(row+3)%nelem(values)];
	radius = widths[row%nelem(widths)];
	wind = values[(row+5)%nelem(values)];
	requestedop = operators[row%nelem(operators)];
	op = variant%2 ? requestedop : SoverD;
	accepted = !(foreign && !alias) && (variant >= 2 || radius >= 0) && (unsigned int)op < Ncomp;
	lookups = locks = unlocks = draws = flushes = held = 0;
	native[0].display = &display;
	native[1].display = foreign ? &other : &display;
	if(variant < 2){
		outline = &storage.frame.outline;
		outline->dst = objects;
		outline->src = objects+(alias ? 0 : 1);
		outline->a.x = expected[0].x; outline->a.y = expected[0].y;
		outline->b.x = expected[1].x; outline->b.y = expected[1].y;
		outline->c.x = expected[2].x; outline->c.y = expected[2].y;
		outline->d.x = expected[3].x; outline->d.y = expected[3].y;
		outline->sp.x = expected[4].x; outline->sp.y = expected[4].y;
		outline->end0 = end0; outline->end1 = end1; outline->radius = radius;
		storage.frame.outlineop.op = requestedop;
	}else{
		fill = &storage.frame.fill;
		fill->dst = objects;
		fill->src = objects+(alias ? 0 : 1);
		fill->a.x = expected[0].x; fill->a.y = expected[0].y;
		fill->b.x = expected[1].x; fill->b.y = expected[1].y;
		fill->c.x = expected[2].x; fill->c.y = expected[2].y;
		fill->d.x = expected[3].x; fill->d.y = expected[3].y;
		fill->sp.x = expected[4].x; fill->sp.y = expected[4].y;
		fill->wind = wind;
		storage.frame.fillop.op = requestedop;
	}
	framebytes = &storage.frame;
	frameextent = sizeof(storage.frame);
	switch(variant){
	case 0: Draw_Image_bezier(framebytes); break;
	case 1: Draw_Image_bezierop(framebytes); break;
	case 2: Draw_Image_fillbezier(framebytes); break;
	case 3: Draw_Image_fillbezierop(framebytes); break;
	}
	check(lookups == 2,"lookup ordering");
	check(draws == accepted && locks == accepted && flushes == accepted,"dispatch counts");
	check(held == 0 && unlocks == accepted*!local,"balanced exclusion");
	for(index=0; index<32; index++)
		check(storage.before[index] == 0x5a && storage.after[index] == 0x5a,"frame guards");
	cases++;
}


/* Check the independently specified int32/ref64 offsets, not just agreement between two generated users. */
static void
framegeometry(void)
{
	check(sizeof(F_Draw_Image_bezier) == 136 && offsetof(F_Draw_Image_bezier,noret) == 32 &&
		offsetof(F_Draw_Image_bezier,temps) == 40,"outline header");
	check(offsetof(F_Draw_Image_bezier,dst) == 64 && offsetof(F_Draw_Image_bezier,a) == 72 &&
		offsetof(F_Draw_Image_bezier,b) == 80 && offsetof(F_Draw_Image_bezier,c) == 88 &&
		offsetof(F_Draw_Image_bezier,d) == 96,"outline points");
	check(offsetof(F_Draw_Image_bezier,end0) == 104 && offsetof(F_Draw_Image_bezier,end1) == 108 &&
		offsetof(F_Draw_Image_bezier,radius) == 112,"outline words");
	check(offsetof(F_Draw_Image_bezier,src) == 120 && offsetof(F_Draw_Image_bezier,sp) == 128,"outline source padding");
	check(sizeof(F_Draw_Image_bezierop) == 144 && offsetof(F_Draw_Image_bezierop,noret) == 32 &&
		offsetof(F_Draw_Image_bezierop,temps) == 40 && offsetof(F_Draw_Image_bezierop,dst) == 64 &&
		offsetof(F_Draw_Image_bezierop,a) == 72 && offsetof(F_Draw_Image_bezierop,b) == 80 &&
		offsetof(F_Draw_Image_bezierop,c) == 88 && offsetof(F_Draw_Image_bezierop,d) == 96 &&
		offsetof(F_Draw_Image_bezierop,end0) == 104 && offsetof(F_Draw_Image_bezierop,end1) == 108 &&
		offsetof(F_Draw_Image_bezierop,radius) == 112 && offsetof(F_Draw_Image_bezierop,src) == 120 &&
		offsetof(F_Draw_Image_bezierop,sp) == 128 && offsetof(F_Draw_Image_bezierop,op) == 136,"explicit outline layout");
	check(sizeof(F_Draw_Image_fillbezier) == 128 && offsetof(F_Draw_Image_fillbezier,noret) == 32 &&
		offsetof(F_Draw_Image_fillbezier,temps) == 40,"fill header");
	check(offsetof(F_Draw_Image_fillbezier,dst) == 64 && offsetof(F_Draw_Image_fillbezier,a) == 72 &&
		offsetof(F_Draw_Image_fillbezier,b) == 80 && offsetof(F_Draw_Image_fillbezier,c) == 88 &&
		offsetof(F_Draw_Image_fillbezier,d) == 96,"fill points");
	check(offsetof(F_Draw_Image_fillbezier,wind) == 104 && offsetof(F_Draw_Image_fillbezier,src) == 112 &&
		offsetof(F_Draw_Image_fillbezier,sp) == 120,"fill source padding");
	check(sizeof(F_Draw_Image_fillbezierop) == 136 && offsetof(F_Draw_Image_fillbezierop,noret) == 32 &&
		offsetof(F_Draw_Image_fillbezierop,temps) == 40 && offsetof(F_Draw_Image_fillbezierop,dst) == 64 &&
		offsetof(F_Draw_Image_fillbezierop,a) == 72 && offsetof(F_Draw_Image_fillbezierop,b) == 80 &&
		offsetof(F_Draw_Image_fillbezierop,c) == 88 && offsetof(F_Draw_Image_fillbezierop,d) == 96 &&
		offsetof(F_Draw_Image_fillbezierop,wind) == 104 && offsetof(F_Draw_Image_fillbezierop,src) == 112 &&
		offsetof(F_Draw_Image_fillbezierop,sp) == 120 && offsetof(F_Draw_Image_fillbezierop,op) == 128,"explicit fill layout");
	check(sizeof(((F_Draw_Image_bezier*)0)->end0) == 4 && sizeof(((F_Draw_Image_bezier*)0)->end1) == 4 &&
		sizeof(((F_Draw_Image_bezier*)0)->radius) == 4 && sizeof(((F_Draw_Image_fillbezier*)0)->wind) == 4 &&
		sizeof(((F_Draw_Image_bezierop*)0)->op) == 4 && sizeof(((F_Draw_Image_fillbezierop*)0)->op) == 4,"scalar field widths");
}


/* Check all four fresh interfaces in both WORD32 profiles with active predicates. */
int
main(void)
{
	int row;

	check(sizeof(WORD) == 4 && sizeof(void*) == 8 && sizeof(Draw_Point) == 8,"native model");
	framegeometry();
	for(variant=0; variant<4; variant++)
	for(row=0; row<64; row++)
	for(mutation=0; mutation<3; mutation++)
	for(local=0; local<2; local++)
	for(alias=0; alias<2; alias++)
	for(foreign=0; foreign<2; foreign++)
		onecase(row);
	printf("PASS Bezier Frame64 consumers: %d active checks, %d cases, four actual entries; C curve boundary bounded\n",checks,cases);
	return 0;
}
