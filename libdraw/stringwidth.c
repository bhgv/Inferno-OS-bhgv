#include "lib9.h"
#include "draw.h"

#define TextWidthBatch 64 // Maximum cached glyph batch for width measurement.


/* Return only measured batches after bounded cache retries and checked width accumulation. */
int
_stringnwidth(Font *f, char *s, Rune *r, int len)
{
	int wid, twid, n, max, l;
	char *name;
	ushort cbuf[TextWidthBatch];
	Rune rune, **rptr;
	char *subfontname, **sptr;
	Font *def;
	static Rune rnull;

	if(f == nil || f->display == nil || len <= 0 || (s != nil && r != nil))
		return 0;
	if(s == nil){
		s = "";
		sptr = nil;
	}else
		sptr = &s;
	if(r == nil){
		r = &rnull;
		rptr = nil;
	}else
		rptr = &r;
	twid = 0;
	while(len>0 && (*s || *r)){
		max = TextWidthBatch;
		if(len < max)
			max = len;
		n = 0;
		wid = 0;
		while((l = cachechars(f, sptr, rptr, cbuf, max, &wid, &subfontname)) <= 0){
			if(++n > 10){
				if(*r)
					rune = *r;
				else
					chartorune(&rune, s);
				if(f->name != nil)
					name = f->name;
				else
					name = "unnamed font";
				_drawprint(2, "stringwidth: bad character set for rune 0x%.4ux in %s\n", rune, name);
				return twid;
			}
			if(subfontname){
				if(_getsubfont(f->display, subfontname, f->height) == 0){
					def = f->display->defaultfont;
					if(def && f!=def && def->display == f->display)
						f = def;
					else
						break;
				}
			}
		}
		if(l <= 0 || l > max || wid < 0 || wid > 2147483647-twid)
			return twid;
		agefont(f);
		twid += wid;
		len -= l;
	}
	return twid;
}


/* Measure stringnwidth through the bounded native glyph-cache path. */
int
stringnwidth(Font *f, char *s, int len)
{
	return _stringnwidth(f, s, nil, len);
}


/* Measure stringwidth through the bounded native glyph-cache path. */
int
stringwidth(Font *f, char *s)
{
	return _stringnwidth(f, s, nil, 1<<24);
}


/* Measure stringsize through the bounded native glyph-cache path. */
Point
stringsize(Font *f, char *s)
{
	return Pt(_stringnwidth(f, s, nil, 1<<24), f != nil ? f->height : 0);
}


/* Measure runestringnwidth through the bounded native glyph-cache path. */
int
runestringnwidth(Font *f, Rune *r, int len)
{
	return _stringnwidth(f, nil, r, len);
}


/* Measure runestringwidth through the bounded native glyph-cache path. */
int
runestringwidth(Font *f, Rune *r)
{
	return _stringnwidth(f, nil, r, 1<<24);
}


/* Measure runestringsize through the bounded native glyph-cache path. */
Point
runestringsize(Font *f, Rune *r)
{
	return Pt(_stringnwidth(f, nil, r, 1<<24), f != nil ? f->height : 0);
}
