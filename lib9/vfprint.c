/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
#include "lib9.h"
#include "fmtdef.h"


#ifdef ANDROID
#include <android/log.h>

#define  LOG_TAG    "Inferno OS"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif




/* Complete short writes, retaining only an unwritten suffix after failure. */
int
_fmtFdFlush(Fmt *f)
{
	char *p;
	int n, nr, fd;

	p = f->start;
	n = (char*)f->to-p;
	fd = (int)(uintptr)f->farg;
#ifdef ANDROID
	/* Log only the occupied span; a full formatting buffer has no spare NUL. */
	if(fd == 1 || fd == 2){
		if(n > 0){
			if(fd == 1)
				LOGI("%.*s", n, p);
			else
				LOGE("%.*s", n, p);
		}
		f->to = f->start;
		return 1;
	}
#endif
	while(n > 0){
		nr = write(fd, p, n);
		if(nr <= 0){
			memmove(f->start, p, n);
			f->to = (char*)f->start+n;
			return 0;
		}
		p += nr;
		n -= nr;
	}
	f->to = f->start;
	return 1;
}

int
vfprint(int fd, char *fmt, va_list args)
{
	Fmt f;
	char buf[256];
	int n;

//#ifdef ANDROID
//  if(fd > 2){
//#endif
	fmtfdinit(&f, fd, buf, sizeof(buf));
	va_copy(f.args, args);
	n = dofmt(&f, fmt);
	va_end(f.args);
	if(n > 0 && _fmtFdFlush(&f) == 0)
		return -1;
	return n;
//#ifdef ANDROID
//  }else{
//	return -1;
//  }
//#endif
}
