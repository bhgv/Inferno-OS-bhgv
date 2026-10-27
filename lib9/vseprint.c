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


/* Compare native endpoint coordinates before forming the bounded output range. */
char*
vseprint(char *buf, char *e, char *fmt, va_list args)
{
	Fmt f;

	if(buf == nil || (uintptr)buf == (uintptr)-1 ||
	   (uintptr)e <= (uintptr)buf || fmt == nil || (uintptr)fmt == (uintptr)-1)
		return nil;
	f.runes = 0;
	f.start = buf;
	f.to = buf;
	f.stop = e - 1;
	f.flush = nil;
	f.farg = nil;
	f.nfmt = 0;
	va_copy(f.args, args);
	dofmt(&f, fmt);
	va_end(f.args);
	*(char*)f.to = '\0';
	return f.to;
}

