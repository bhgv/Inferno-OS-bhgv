#include "lib9.h"
#include "draw.h"
#include "kernel.h"


/* Font fields use ASCII whitespace, including CRLF descriptions from other hosts. */
static int
fontspace(int value)
{
	return value == ' ' || value == '\t' || value == '\n' ||
	       value == '\r' || value == '\f' || value == '\v';
}


/* Skip complete separators without allowing numeric parsing to consume a partial token. */
static char*
skip(char *text)
{
	while(fontspace((uchar)*text))
		text++;
	return text;
}


/* Return one for Int32, minus one for a complete overflowing number, zero for a filename token. */
static int
fontinteger(char *text, char **end, int *result)
{
	char *cursor;
	uvlong value, limit;
	int base, negative, digits, digit, overflow;

	cursor = text;
	negative = 0;
	if(*cursor == '-' || *cursor == '+'){
		negative = *cursor == '-';
		cursor++;
	}
	base = 10;
	if(*cursor == '0'){
		base = 8;
		if(cursor[1] == 'x' || cursor[1] == 'X'){
			base = 16;
			cursor += 2;
		}
	}
	limit = negative ? 2147483648ULL : 2147483647ULL;
	value = 0;
	digits = 0;
	overflow = 0;
	for(;; cursor++){
		if(*cursor >= '0' && *cursor <= '9')
			digit = *cursor-'0';
		else if(*cursor >= 'a' && *cursor <= 'f')
			digit = *cursor-'a'+10;
		else if(*cursor >= 'A' && *cursor <= 'F')
			digit = *cursor-'A'+10;
		else
			break;
		if(digit >= base)
			break;
		if(value > (limit-digit)/base)
			overflow = 1;
		if(!overflow)
			value = value*base+digit;
		digits = 1;
	}
	if(digits == 0 || (*cursor != 0 && !fontspace((uchar)*cursor)))
		return 0;
	*end = cursor;
	if(overflow)
		return -1;
	if(negative)
		*result = value == 2147483648ULL ? (-2147483647-1) : -(int)value;
	else
		*result = value;
	return 1;
}


/* Parse complete C-format numeric fields before narrowing, retaining every owner on refusal. */
Font*
buildfont(Display *d, char *buf, char *name)
{
	Font *fnt;
	Cachefont *c, **sub;
	char *s, *t;
	int min, max, height, ascent, offset, numeric;

	if(buf == nil || name == nil){
		kwerrstr("bad font format: nil description or name");
		return nil;
	}
	s = skip(buf);
	fnt = malloc(sizeof(Font));
	if(fnt == 0)
		return 0;
	memset(fnt, 0, sizeof(Font));
	fnt->display = d;
	fnt->name = strdup(name);
	fnt->ncache = NFCACHE+NFLOOK;
	fnt->nsubf = NFSUBF;
	fnt->cache = malloc(fnt->ncache * sizeof(fnt->cache[0]));
	fnt->subf = malloc(fnt->nsubf * sizeof(fnt->subf[0]));
	if(fnt->name==0 || fnt->cache==0 || fnt->subf==0){
    Err2:
		free(fnt->name);
		free(fnt->cache);
		free(fnt->subf);
		free(fnt->sub);
		free(fnt);
		return 0;
	}
	if(fontinteger(s, &t, &height) != 1 || !fontspace((uchar)*t)){
		kwerrstr("bad font format: invalid height");
		goto Err2;
	}
	s = skip(t);
	if(fontinteger(s, &t, &ascent) != 1 || !fontspace((uchar)*t)){
		kwerrstr("bad font format: invalid ascent");
		goto Err2;
	}
	s = skip(t);
	if(height <= 0 || height > 0x7fff || ascent <= 0 || ascent > 0x7fff){
		kwerrstr("bad height or ascent in font file");
		goto Err2;
	}
	fnt->height = height;
	fnt->ascent = ascent;
	fnt->width = 0;
	fnt->nsub = 0;
	fnt->sub = 0;

	memset(fnt->subf, 0, fnt->nsubf * sizeof(fnt->subf[0]));
	memset(fnt->cache, 0, fnt->ncache*sizeof(fnt->cache[0]));
	fnt->age = 1;
	do{
		if(fontinteger(s, &t, &min) != 1 || !fontspace((uchar)*t)){
			kwerrstr("bad font format: invalid range minimum");
			goto Err3;
		}
		s = skip(t);
		if(fontinteger(s, &t, &max) != 1 || !fontspace((uchar)*t)){
			kwerrstr("bad font format: invalid range maximum");
			goto Err3;
		}
		s = skip(t);
		if(*s == 0 || min < 0 || max < 0 || min > Runemax || max > Runemax || min > max){
			kwerrstr("illegal subfont range");
    Err3:
			freefont(fnt);
			return nil;
		}
		t = s;
		offset = 0;
		numeric = fontinteger(s, &t, &offset);
		/* The last token is a filename, even when it is entirely numeric. */
		if(numeric != 0 && *t != 0 && *skip(t) != 0){
			if(numeric < 0){
				kwerrstr("subfont offset is outside Int32");
				goto Err3;
			}
			s = skip(t);
		}else
			offset = 0;
		/* nsub is a signed short in the public Font layout. */
		if(fnt->nsub == 0x7fff){
			kwerrstr("too many subfonts");
			goto Err3;
		}
		sub = realloc(fnt->sub, (fnt->nsub+1)*sizeof(Cachefont*));
		if(sub == nil)
			goto Err3;
		fnt->sub = sub;
		c = malloc(sizeof(Cachefont));
		if(c == 0)
			goto Err3;
		fnt->sub[fnt->nsub] = c;
		c->min = min;
		c->max = max;
		c->offset = offset;
		t = s;
		while(*s && !fontspace((uchar)*s))
			s++;
		if(*s != 0)
			*s++ = 0;
		c->subfontname = 0;
		c->name = strdup(t);
		if(c->name == 0){
			free(c);
			goto Err3;
		}
		s = skip(s);
		fnt->nsub++;
	}while(*s);
	return fnt;
}


/* Release every owned subfont, cache entry and allocation. */
void
freefont(Font *f)
{
	int i;
	Cachefont *c;
	Subfont *s;

	if(f == 0)
		return;

	for(i=0; i<f->nsub; i++){
		c = f->sub[i];
		free(c->subfontname);
		free(c->name);
		free(c);
	}
	for(i=0; i<f->nsubf; i++){
		s = f->subf[i].f;
/*		if(s && s!=display->defaultsubfont)*/	/* Plan 9 uses this */
		if(s)
			freesubfont(s);
	}
	freeimage(f->cacheimage);
	free(f->name);
	free(f->cache);
	free(f->subf);
	free(f->sub);
	free(f);
}
