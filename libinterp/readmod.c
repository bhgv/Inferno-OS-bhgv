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


#if defined(DIS_NATIVE64_SCALAR_BOOT) && defined(DIS64_PILOT_LOADER)
typedef struct Dis64PairRead Dis64PairRead;

struct Dis64PairRead {
	char *consumer; // Explicit namespace path for the startup module.
	char *provider; // Explicit namespace path for its dependency.
	uchar *images[2]; // Owned read buffers, kept through linking.
	ulong sizes[2]; // Complete file extents.
	int fd; // Current open namespace descriptor, or -1.
	Dir *dir; // Current file metadata for the existing admission check.
	Modlink *link; // Rooted consumer returned by the private builder.
};


/* Read a complete bounded file and apply the existing Dis admission policy. */
static int
read64pairimage(Dis64PairRead *read, char *path, int index)
{
	ulong offset, length;
	long count;

	read->fd = kopen(path, OREAD);
	if(read->fd < 0)
		return 0;
	read->dir = kdirfstat(read->fd);
	if(read->dir == nil)
		return 0;
	if(read->dir->length <= 0 || read->dir->length >= 8*1024*1024){
		kwerrstr("implausible Dis64 pair length");
		return 0;
	}
	length = read->dir->length;
	read->images[index] = mallocz(length, 0);
	if(read->images[index] == nil)
		return 0;
	for(offset=0; offset<length; offset+=count){
		count = kread(read->fd, read->images[index]+offset, length-offset);
		if(count <= 0 || (ulong)count > length-offset){
			kwerrstr("incomplete Dis64 pair read");
			return 0;
		}
	}
	if(mustbesigned(path, read->images[index], length, read->dir)){
		kwerrstr("security violation: not signed");
		return 0;
	}
	kclose(read->fd);
	read->fd = -1;
	free(read->dir);
	read->dir = nil;
	read->sizes[index] = length;
	return 1;
}


/* Build the linked instance only after both namespace files pass admission. */
static int
read64pairprotected(void *argument)
{
	Dis64PairRead *read;

	read = argument;
	if(!read64pairimage(read, read->consumer, 0) ||
	   !read64pairimage(read, read->provider, 1))
		return 0;
	read->link = prepare64consumerlink(read->consumer, read->images[0], read->sizes[0],
		read->provider, read->images[1], read->sizes[1], cflag != 0);
	return read->link != nil;
}


/* Return one rooted linked instance for the explicit scalar-console boot. */
Modlink*
read64pair(char *consumer, char *provider)
{
	Dis64PairRead read;

	memset(&read, 0, sizeof(read));
	read.consumer = consumer;
	read.provider = provider;
	read.fd = -1;
	kprotect(read64pairprotected, &read);
	if(read.fd >= 0)
		kclose(read.fd);
	free(read.dir);
	free(read.images[0]);
	free(read.images[1]);
	return read.link;
}
#endif
