#include "lib9.h"
#include "kernel.h"
#include "draw.h"

/* Reject unrepresentable file lengths before narrowing or allocating the text. */
Font*
openfont(Display *d, char *name)
{
	Font *fnt;
	int fd, i, n;
	char *buf;
	Dir *dir;

	if( is_subfont_ft(name) )
		return nil;

	fd = libopen(name, OREAD);
	if(fd < 0)
		return 0;

	if((dir = libdirfstat(fd)) == nil){
    Err0:
		libclose(fd);
		return 0;
	}
	if(dir->length < 0 || dir->length > 0x7fffffff-1){
		free(dir);
		kwerrstr("font file too large");
		goto Err0;
	}
	n = dir->length;
	free(dir);
	buf = malloc(n+1);
	if(buf == 0)
		goto Err0;
	buf[n] = 0;
	i = libreadn(fd, buf, n);
	libclose(fd);
	if(i != n){
		free(buf);
		return 0;
	}
	fnt = buildfont(d, buf, name);
	free(buf);
	return fnt;
}
