#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include <isa.h>
#include <interp.h>
#include "kernel.h"

#define	QP(l)	(Prog**)((char*)(l)+sizeof(QLock))

void*
libqlowner(void *l)
{
	return *QP(l);
}

void
libqlock(void *l)
{
	Prog *p;
	QLock *q;

	q = l;
	p = currun();
	if(p == nil)
		panic("libqlock");

	if(!canqlock(q)) {
		release();
		qlock(q);
		acquire();
	}
	*QP(l) = p;
}

void
libqunlock(void *l)
{
	Prog *p;
	QLock *q;

	q = l;
	p = currun();
	if(p == nil)
		panic("libqunlock 1");
	if(*QP(l) != p)
		panic("libqunlock 2");

	*QP(l) = nil;
	qunlock(q);
}

void*
libqlalloc(void)
{
	QLock *q;

	q = mallocz(sizeof(QLock)+sizeof(Prog*), 1);
	return q;
}

void
libqlfree(void *l)
{
	free(l);
}

vlong
libseek(int fd, vlong off, int whence)
{
	release();
	off = kseek(fd, off, whence);
	acquire();
	return off;
}


/* Bound an int-result transfer before native calls or caller-pointer arithmetic. */
static int
libiospan(void *buffer, long count)
{
	if(count < 0 || count > 0x7fffffff ||
	   (count != 0 && (buffer == nil || buffer == H ||
	   (uintptr)buffer > (uintptr)-1-(uintptr)count))){
		kwerrstr("invalid library I/O span");
		return 0;
	}
	return 1;
}


/* Preserve representable errors and reject native replies before int narrowing. */
static int
libioresult(long result, int requested)
{
	if(result < 0)
		return result < (-0x7fffffff-1) ? -1 : (int)result;
	if(result > requested){
		kwerrstr("invalid library I/O count");
		return -1;
	}
	return (int)result;
}


/* Keep the complete native read result until the interpreter lock is reacquired. */
int
libread(int fd, void *buf, int n)
{
	long result;

	if(!libiospan(buf, n))
		return -1;
	release();
	result = kread(fd, buf, n);
	acquire();
	return libioresult(result, n);
}


/* Advance only by checked native replies, retaining ordinary partial-read semantics. */
int
libreadn(int fd, void *av, long n)
{
	char *a;
	long m, t;
	int bad;

	if(!libiospan(av, n))
		return -1;
	a = av;
	t = m = 0;
	bad = 0;
	release();
	while(t < n){
		m = kread(fd, a+t, n-t);
		if(m <= 0)
			break;
		if(m > n-t){
			bad = 1;
			break;
		}
		t += m;
	}
	acquire();
	if(bad)
		return libioresult(m, (int)(n-t));
	if(t == 0 && m < 0)
		return libioresult(m, (int)n);
	return (int)t;
}


/* Validate the complete native write reply before publishing its int result. */
int
libwrite(int fd, void *buf, int n)
{
	long result;

	if(!libiospan(buf, n))
		return -1;
	release();
	result = kwrite(fd, buf, n);
	acquire();
	return libioresult(result, n);
}


int
libopen(char *name, int omode)
{
	int fd;

	release();
	fd = kopen(name, omode);
	acquire();
	return fd;
}

int
libclose(int fd)
{
	release();
	fd = kclose(fd);
	acquire();
	return fd;
}

Dir*
libdirfstat(int fd)
{
	Dir *d;

	release();
	d = kdirfstat(fd);
	acquire();
	return d;
}

int
libbind(char *s, char *t, int f)
{
	int n;

	release();
	n = kbind(s, t, f);
	acquire();
	return n;
}

void
libchanclose(void *chan)
{
	release();
	cclose(chan);
	acquire();
}

void*
libfdtochan(int fd, int mode)
{
	Chan *c;

	release();
	if(waserror()) {
		acquire();
		return nil;
	}
	c = fdtochan(up->env->fgrp, fd, mode, 0, 1);
	poperror();
	acquire();
	return c;
}
