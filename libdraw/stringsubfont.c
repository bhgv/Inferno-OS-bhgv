#include "lib9.h"
#include "draw.h"


/* Validate adjacent glyph metrics before reading a mask or accumulating width. */
static int
subfontglyph(Subfont *f, Rune c)
{
	Fontchar *i;

	if(f == nil || f->info == nil || f->n <= 0 || f->height <= 0 || c >= (Rune)f->n)
		return 0;
	i = f->info+c;
	return i->x <= i[1].x && i->top <= i->bottom && i->bottom <= f->height;
}


/* Check coordinate additions at native width before narrowing to Draw Int32. */
static int
subfontcoordinate(vlong n)
{
	return n >= (-2147483647LL-1) && n <= 2147483647LL;
}


/* Draw complete representable glyphs and return the last valid pen position. */
Point
stringsubfont(Image *b, Point p, Image *color, Subfont *f, char *cs)
{
	int w;
	uchar *s;
	Rune c;
	Fontchar *i;
	vlong left, right, top, bottom, next;

	if(b == nil || color == nil || f == nil || f->bits == nil || cs == nil)
		return p;
	s = (uchar*)cs;
	while((c = *s) != 0){
		if(c < Runeself)
			s++;
		else{
			w = chartorune(&c, (char*)s);
			if(w <= 0)
				return p;
			s += w;
		}
		if(f->n > 0 && c >= (Rune)f->n)
			continue;
		if(!subfontglyph(f, c))
			return p;
		i = f->info+c;
		left = (vlong)p.x+i->left;
		right = left+i[1].x-i[0].x;
		top = (vlong)p.y+i->top;
		bottom = (vlong)p.y+i->bottom;
		next = (vlong)p.x+i->width;
		if(!subfontcoordinate(left) || !subfontcoordinate(right) ||
		   !subfontcoordinate(top) || !subfontcoordinate(bottom) || !subfontcoordinate(next))
			return p;
		draw(b, Rect((int)left, (int)top, (int)right, (int)bottom),
			color, f->bits, Pt(i->x, i->top));
		p.x = (int)next;
	}
	return p;
}


/* Measure only complete glyphs whose accumulated width fits Draw Int32. */
Point
strsubfontwidth(Subfont *f, char *cs)
{
	Rune c;
	Point p;
	uchar *s;
	int w;

	p = Pt(0, f != nil ? f->height : 0);
	if(f == nil || cs == nil)
		return p;
	s = (uchar*)cs;
	while((c = *s) != 0){
		if(c < Runeself)
			s++;
		else{
			w = chartorune(&c, (char*)s);
			if(w <= 0)
				return p;
			s += w;
		}
		if(f->n > 0 && c >= (Rune)f->n)
			continue;
		if(!subfontglyph(f, c) || f->info[c].width > 2147483647-p.x)
			return p;
		p.x += f->info[c].width;
	}
	return p;
}
