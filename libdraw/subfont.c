#include "lib9.h"
#include "kernel.h"
#include "draw.h"

/* Validate public field widths and copy the owned name before publishing a subfont. */
Subfont*
allocsubfont(char *name, int n, int height, int ascent, Fontchar *info, Image *i)
{
	Subfont *f;

	if(n < 0 || n > 0x7fff || height <= 0 || height > 0xff ||
	   ascent != (char)ascent){
		kwerrstr("allocsubfont: unrepresentable metrics");
		return nil;
	}

	f = malloc(sizeof(Subfont));
	if(f == 0)
		return 0;
	f->n = n;
	f->height = height;
	f->ascent = ascent;
	f->info = info;
	f->bits = i;
	f->ref = 1;
	if(name){
		f->name = strdup(name);
		if(f->name == nil){
			free(f);
			return nil;
		}
		if(lookupsubfont(i->display, name) == 0)
			installsubfont(name, f);
	}else
		f->name = 0;
	return f;
}
