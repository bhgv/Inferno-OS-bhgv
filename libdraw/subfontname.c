#include "lib9.h"
#include "draw.h"

/* Resolve complete paths without truncating a directory or a depth suffix. */
char*
subfontname(char *cfname, char *fname, int maxdepth)
{
	char *path, *slash;
	ulong prefix, n;
	int i;

	if(strcmp(cfname, "*default*") == 0)
		return strdup(cfname);
	n = strlen(cfname);
	prefix = 0;
	slash = nil;
	if(cfname[0] != '/'){
		slash = utfrrune(fname, '/');
		prefix = slash == nil ? 2 : slash-fname+1;
	}
	/* Include an optional .0 suffix and NUL within the signed allocation limit. */
	if(n > 0x7fffffff-3 || prefix > 0x7fffffff-3-n)
		return nil;
	path = malloc(prefix+n+3);
	if(path == nil)
		return nil;
	if(prefix != 0){
		if(slash == nil)
			memmove(path, "./", prefix);
		else
			memmove(path, fname, prefix);
	}
	memmove(path+prefix, cfname, n+1);
	n += prefix;

	if(maxdepth > 8)
		maxdepth = 8;
	for(i = 3; i >= 0; i--){
		if((1<<i) > maxdepth)
			continue;
		path[n] = '.';
		path[n+1] = '0'+i;
		path[n+2] = '\0';
		if(access(path, AREAD) == 0)
			return path;
	}
	path[n] = '\0';
	if(access(path, AREAD) == 0)
		return path;
	free(path);
	return nil;
}
