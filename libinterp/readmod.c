#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "kernel.h"
#include "dynld.h"
#include "native64-scalar-boot.h"


/* Validate full-width native replies before advancing a complete module image. */
Module*
readmod(char *path, Module *m, int sync)
{
	Dir *d;
	int fd, n, dynld;
	long nr;
	uchar *code;
	Module *ans;
	ulong length;

	if(path[0] == '$') {
#ifdef DIS64_PILOT_LOADER
		if(dis64launchmode()){
			kwerrstr("experimental dis64-calls does not admit builtin imports");
			if(m != nil)
				unload(m);
			return nil;
		}
#endif
#ifdef DIS_NATIVE64_SCALAR_BOOT
		kwerrstr("builtin modules are unavailable in native64 scalar startup");
		return nil;
#endif
		if(m == nil)
			kwerrstr("module not built-in");
		return m;
	}

#ifdef DIS64_PILOT_LOADER
	/* Seal before releasing VM admission and reject cache aliases across profiles. */
	(void)dis64launchmode();
	if(m != nil && !dis64launchcompatible(m)){
		kwerrstr("module cache does not match the selected Dis64 launch profile");
		unload(m);
		return nil;
	}
#endif

	ans = nil;
	code = nil;
	length = 0;
	dynld = 0;

	if(sync)
		release();

	d = nil;
	fd = kopen(path, OREAD);
	if(fd < 0)
		goto done;

	if((d = kdirfstat(fd)) == nil)
		goto done;

	if(m != nil) {
		if(d->dev == m->dev && d->type == m->dtype &&
		   d->mtime == m->mtime &&
		   d->qid.type == m->qid.type && d->qid.path == m->qid.path && d->qid.vers == m->qid.vers) {
			ans = m;
			goto done;
		}
	}

	if(d->length < 0 || d->length >= 8*1024*1024){
		kwerrstr("implausible length");
		goto done;
	}
#ifndef DIS_NATIVE64_SCALAR_BOOT
	if(
#ifdef DIS64_PILOT_LOADER
	   !dis64launchmode() &&
#endif
	   (d->mode&0111) && dynldable(fd)){
		dynld = 1;
		goto done1;
	}
#endif
	length = d->length;
	code = mallocz(length, 0);
	if(code == nil)
		goto done;

	n = 0;
	while(n < length){
		nr = kread(fd, code+n, length-n);
		if(nr <= 0)
			break;
		if((ulong)nr > length-n){
			kwerrstr("invalid module read count");
			break;
		}
		n += (int)nr;
	}
	if(n != length) {
		free(code);
		code = nil;
	}
done:
	if(fd >= 0)
		kclose(fd);
done1:
	if(sync)
		acquire();
	if(m != nil && ans == nil)
		unload(m);
	if(code != nil) {
		ans = parsemod(path, code, length, d);
		free(code);
	}
	else if(dynld){
		kseek(fd, 0, 0);
		ans = newdyncode(fd, path, d);
		kclose(fd);
	}
	free(d);
	return ans;
}
