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


/* Flush a validated byte view and retain the formatter's total character count. */
int
fmtfdflush(Fmt *f)
{
	if(_fmtFdFlush(f) <= 0)
		return -1;
	return f->nfmt;
}


/* Reject invalid spans before changing the caller's formatter; empty buffers remain valid. */
int
fmtfdinit(Fmt *f, int fd, char *buf, int size)
{
	if(f == nil || buf == nil || (uintptr)buf == (uintptr)-1 || size < 0 ||
	   (uintptr)buf > (uintptr)-1-(uintptr)size)
		return -1;
	f->runes = 0;
	f->start = buf;
	f->to = buf;
	f->stop = buf + size;
	f->flush = _fmtFdFlush;
	f->farg = (void*)(uintptr)fd;
	f->nfmt = 0;
	return 0;
}
