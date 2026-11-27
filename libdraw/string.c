#include "lib9.h"
#include "draw.h"
#include "draw-packet.h"

#define TextBatch 100 // Maximum glyphs encoded in one native text command.


/* Draw string with the existing explicit/default native text contract. */
Point
string(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s)
{
	return _string(dst, pt, src, sp, f, s, nil, 1<<24, dst != nil ? dst->clipr : ZR, nil, ZP, SoverD);
}


/* Draw stringop with the existing explicit/default native text contract. */
Point
stringop(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, Drawop op)
{
	return _string(dst, pt, src, sp, f, s, nil, 1<<24, dst != nil ? dst->clipr : ZR, nil, ZP, op);
}


/* Draw stringn with the existing explicit/default native text contract. */
Point
stringn(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, int len)
{
	return _string(dst, pt, src, sp, f, s, nil, len, dst != nil ? dst->clipr : ZR, nil, ZP, SoverD);
}


/* Draw stringnop with the existing explicit/default native text contract. */
Point
stringnop(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, int len, Drawop op)
{
	return _string(dst, pt, src, sp, f, s, nil, len, dst != nil ? dst->clipr : ZR, nil, ZP, op);
}


/* Draw runestring with the existing explicit/default native text contract. */
Point
runestring(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r)
{
	return _string(dst, pt, src, sp, f, nil, r, 1<<24, dst != nil ? dst->clipr : ZR, nil, ZP, SoverD);
}


/* Draw runestringop with the existing explicit/default native text contract. */
Point
runestringop(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, Drawop op)
{
	return _string(dst, pt, src, sp, f, nil, r, 1<<24, dst != nil ? dst->clipr : ZR, nil, ZP, op);
}


/* Draw runestringn with the existing explicit/default native text contract. */
Point
runestringn(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, int len)
{
	return _string(dst, pt, src, sp, f, nil, r, len, dst != nil ? dst->clipr : ZR, nil, ZP, SoverD);
}


/* Draw runestringnop with the existing explicit/default native text contract. */
Point
runestringnop(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, int len, Drawop op)
{
	return _string(dst, pt, src, sp, f, nil, r, len, dst != nil ? dst->clipr : ZR, nil, ZP, op);
}


/* Encode bounded native text with checked progress, geometry and one packet reservation. */
Point
_string(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, Rune *r, int len, Rectangle clipr, Image *bg, Point bgp, Drawop op)
{
	int m, n, wid, max, retries;
	vlong baseline, nextx, nextbg;
	Rune empty;
	ushort cbuf[TextBatch], *c, *ec;
	uchar *b;
	char *subfontname;
	char **sptr;
	Rune **rptr;
	Font *def;

	if(dst == nil || src == nil || f == nil || dst->display == nil ||
	   dst->display != src->display || dst->display != f->display ||
	   (bg != nil && bg->display != dst->display) || (unsigned int)op >= Ncomp || len <= 0 ||
	   (s != nil && r != nil))
		return pt;
	empty = 0;
	retries = 0;
	if(s == nil){
		s = "";
		sptr = nil;
	}else
		sptr = &s;
	if(r == nil){
		r = &empty;
		rptr = nil;
	}else
		rptr = &r;
	while(len > 0 && (*s || *r)){
		max = TextBatch;
		if(len < max)
			max = len;
		wid = 0;
		n = cachechars(f, sptr, rptr, cbuf, max, &wid, &subfontname);
		if(n < 0 || n > max || wid < 0)
			break;
		if(n == 0 && ++retries > 10)
			break;
		if(n > 0){
			retries = 0;
			baseline = (vlong)pt.y+f->ascent;
			nextx = (vlong)pt.x+wid;
			nextbg = (vlong)bgp.x+wid;
			if(baseline < (-2147483647-1) || baseline > 2147483647 ||
			   nextx < (-2147483647-1) || nextx > 2147483647 ||
			   (bg != nil && (nextbg < (-2147483647-1) || nextbg > 2147483647)) ||
			   f->cacheimage == nil || f->cacheimage->display != dst->display)
				break;

			m = 47+2*n;
			if(bg)
				m += 4+2*4;
			b = drawpacket(dst->display, m, op);
			if(b == 0){
				_drawprint(2, "string: %r\n");
				break;
			}
			if(bg)
				b[0] = 'x';
			else
				b[0] = 's';
			BPLONG(b+1, dst->id);
			BPLONG(b+5, src->id);
			BPLONG(b+9, f->cacheimage->id);
			BPLONG(b+13, pt.x);
			BPLONG(b+17, baseline);
			BPLONG(b+21, clipr.min.x);
			BPLONG(b+25, clipr.min.y);
			BPLONG(b+29, clipr.max.x);
			BPLONG(b+33, clipr.max.y);
			BPLONG(b+37, sp.x);
			BPLONG(b+41, sp.y);
			BPSHORT(b+45, n);
			b += 47;
			if(bg){
				BPLONG(b, bg->id);
				BPLONG(b+4, bgp.x);
				BPLONG(b+8, bgp.y);
				b += 12;
			}
			ec = &cbuf[n];
			for(c=cbuf; c<ec; c++, b+=2)
				BPSHORT(b, *c);
			pt.x = nextx;
			if(bg != nil)
				bgp.x = nextbg;
			agefont(f);
			len -= n;
		}
		if(subfontname){
			if(_getsubfont(f->display, subfontname, f->height) == 0){
				def = f->display->defaultfont;
				if(def && f!=def && def->display == dst->display)
					f = def;
				else
					break;
			}
		}
	}
	return pt;
}
