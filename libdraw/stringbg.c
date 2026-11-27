#include "lib9.h"
#include "draw.h"


/* Draw stringbg through the checked native background-text path. */
Point
stringbg(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, Image *bg, Point bgp)
{
	return _string(dst, pt, src, sp, f, s, nil, 1<<24, dst != nil ? dst->clipr : ZR, bg, bgp, SoverD);
}


/* Draw stringbgop through the checked native background-text path. */
Point
stringbgop(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, Image *bg, Point bgp, Drawop op)
{
	return _string(dst, pt, src, sp, f, s, nil, 1<<24, dst != nil ? dst->clipr : ZR, bg, bgp, op);
}


/* Draw stringnbg through the checked native background-text path. */
Point
stringnbg(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, int len, Image *bg, Point bgp)
{
	return _string(dst, pt, src, sp, f, s, nil, len, dst != nil ? dst->clipr : ZR, bg, bgp, SoverD);
}


/* Draw stringnbgop through the checked native background-text path. */
Point
stringnbgop(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, int len, Image *bg, Point bgp, Drawop op)
{
	return _string(dst, pt, src, sp, f, s, nil, len, dst != nil ? dst->clipr : ZR, bg, bgp, op);
}


/* Draw runestringbg through the checked native background-text path. */
Point
runestringbg(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, Image *bg, Point bgp)
{
	return _string(dst, pt, src, sp, f, nil, r, 1<<24, dst != nil ? dst->clipr : ZR, bg, bgp, SoverD);
}


/* Draw runestringbgop through the checked native background-text path. */
Point
runestringbgop(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, Image *bg, Point bgp, Drawop op)
{
	return _string(dst, pt, src, sp, f, nil, r, 1<<24, dst != nil ? dst->clipr : ZR, bg, bgp, op);
}


/* Draw runestringnbg through the checked native background-text path. */
Point
runestringnbg(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, int len, Image *bg, Point bgp)
{
	return _string(dst, pt, src, sp, f, nil, r, len, dst != nil ? dst->clipr : ZR, bg, bgp, SoverD);
}


/* Draw runestringnbgop through the checked native background-text path. */
Point
runestringnbgop(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, int len, Image *bg, Point bgp, Drawop op)
{
	return _string(dst, pt, src, sp, f, nil, r, len, dst != nil ? dst->clipr : ZR, bg, bgp, op);
}
