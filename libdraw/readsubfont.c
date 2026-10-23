#include "lib9.h"
#include "kernel.h"
#include "draw.h"

/* Parse one fixed-width decimal field without reading into the next field. */
static int
subfontnumber(char *field, int *result)
{
	char text[13], *end;
	long value;

	memmove(text, field, 12);
	text[12] = '\0';
	value = strtol(text, &end, 10);
	if(end == text || value < -0x7fffffff-1 || value > 0x7fffffff)
		return -1;
	while(*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
		end++;
	if(*end != '\0')
		return -1;
	*result = value;
	return 0;
}

/* Transfer image ownership only after a complete, representable subfont is built. */
Subfont*
readsubfonti(Display *d, char *name, int fd, Image *ai, int dolock)
{
	char hdr[3*12];
	int n, height, ascent;
	uchar *p;
	Fontchar *fc;
	Subfont *f;
	Image *i;

	p = nil;
	fc = nil;
	i = ai;
	if(i == nil){
		i = readimage(d, fd, dolock|2);
		if(i == nil)
			return nil;
	}
	if(libreadn(fd, hdr, sizeof hdr) != sizeof hdr){
		kwerrstr("rdsubfonfile: header read error: %r");
		goto Err;
	}
	if(subfontnumber(hdr, &n) < 0 ||
	   subfontnumber(hdr+12, &height) < 0 ||
	   subfontnumber(hdr+24, &ascent) < 0 ||
	   n < 0 || n > 0x7fff || height <= 0 || height > 0xff ||
	   ascent != (char)ascent){
		kwerrstr("rdsubfonfile: bad subfont header");
		goto Err;
	}
	p = malloc(6*(n+1));
	if(p == nil)
		goto Err;
	if(libreadn(fd, p, 6*(n+1)) != 6*(n+1)){
		kwerrstr("rdsubfonfile: fontchar read error: %r");
		goto Err;
	}
	fc = malloc(sizeof(Fontchar)*(n+1));
	if(fc == nil)
		goto Err;
	_unpackinfo(fc, p, n);
	if(dolock)
		lockdisplay(d);
	f = allocsubfont(name, n, height, ascent, fc, i);
	if(dolock)
		unlockdisplay(d);
	if(f == nil)
		goto Err;
	free(p);
	return f;

Err:
	free(fc);
	free(p);
	if(ai == nil){
		if(dolock)
			lockdisplay(d);
		freeimage(i);
		if(dolock)
			unlockdisplay(d);
	}
	return nil;
}

/* Read and transfer a newly loaded image together with its subfont metadata. */
Subfont*
readsubfont(Display *d, char *name, int fd, int dolock)
{
	return readsubfonti(d, name, fd, nil, dolock);
}
