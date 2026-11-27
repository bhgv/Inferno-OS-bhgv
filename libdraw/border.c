#include "lib9.h"
#include "draw.h"


/* Narrow a computed coordinate only after checking the native signed32 Point range. */
static int
borderpoint(Point *point, vlong x, vlong y)
{
	if(x < (-2147483647-1) || x > 2147483647 || y < (-2147483647-1) || y > 2147483647)
		return 0;
	point->x = x;
	point->y = y;
	return 1;
}


/* Prove every edge and source origin before emitting any part of an inside/outside border. */
void
borderop(Image *im, Rectangle r, int i, Image *color, Point sp, Drawop op)
{
	Rectangle edges[4];
	Point sources[4];
	vlong x0, y0, x1, y1, sx, sy, width;
	int edge;

	if(im == nil || im->display == nil || (color != nil && color->display != im->display) ||
	   (unsigned int)op >= Ncomp || r.min.x > r.max.x || r.min.y > r.max.y)
		return;
	x0 = r.min.x;
	y0 = r.min.y;
	x1 = r.max.x;
	y1 = r.max.y;
	sx = sp.x;
	sy = sp.y;
	width = i;
	if(width < 0){
		x0 += width;
		y0 += width;
		x1 -= width;
		y1 -= width;
		sx += width;
		sy += width;
		width = -width;
	}
	if(!borderpoint(&edges[0].min, x0, y0) || !borderpoint(&edges[0].max, x1, y0+width) ||
	   !borderpoint(&sources[0], sx, sy) ||
	   !borderpoint(&edges[1].min, x0, y1-width) || !borderpoint(&edges[1].max, x1, y1) ||
	   !borderpoint(&sources[1], sx, sy+y1-y0-width) ||
	   !borderpoint(&edges[2].min, x0, y0+width) || !borderpoint(&edges[2].max, x0+width, y1-width) ||
	   !borderpoint(&sources[2], sx, sy+width) ||
	   !borderpoint(&edges[3].min, x1-width, y0+width) || !borderpoint(&edges[3].max, x1, y1-width) ||
	   !borderpoint(&sources[3], sx+x1-x0-width, sy+width))
		return;
	for(edge=0; edge<4; edge++)
		drawop(im, edges[edge], color, nil, sources[edge], op);
}


/* Draw an inside/outside border with default compositing. */
void
border(Image *im, Rectangle r, int i, Image *color, Point sp)
{
	borderop(im, r, i, color, sp, SoverD);
}
