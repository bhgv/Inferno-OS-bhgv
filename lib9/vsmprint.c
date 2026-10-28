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


/* Validate the owned byte view before growth or pointer-difference narrowing. */
static int
fmtStrValid(Fmt *f)
{
	uintptr start, capacity;

	if(f == nil || f->runes || f->start == nil || f->to == nil || f->stop == nil)
		return 0;
	start = (uintptr)f->start;
	capacity = (uintptr)f->farg;
	if(capacity == 0 || capacity > 0x7fffffff ||
	   start > (uintptr)-1-capacity ||
	   (uintptr)f->stop != start+capacity-1 ||
	   (uintptr)f->to < start || (uintptr)f->to > (uintptr)f->stop)
		return 0;
	return 1;
}


/* Capture the cursor before geometric growth and clear the owner if realloc fails. */
static int
fmtStrFlush(Fmt *f)
{
	char *old, *grown;
	uintptr capacity;
	int n, used;

	if(!fmtStrValid(f))
		return 0;
	capacity = (uintptr)f->farg;
	if(capacity == 0x7fffffff)
		return 0;
	/* Keep the existing first 256-byte expansion for ordinary short strings. */
	if(capacity < 256)
		n = capacity+256;
	else
		n = capacity > 0x7fffffff/2 ? 0x7fffffff : capacity*2;
	old = f->start;
	used = (uintptr)f->to-(uintptr)old;
	grown = realloc(old, n);
	if(grown == nil){
		free(old);
		f->start = nil;
		f->to = nil;
		f->stop = nil;
		f->farg = nil;
		return 0;
	}
	f->start = grown;
	f->to = grown+used;
	f->stop = grown+n-1;
	f->farg = (void*)(uintptr)n;
	return 1;
}


/* Initialize one owned byte string; a failed allocation leaves an empty formatter. */
int
fmtstrinit(Fmt *f)
{
	int n;

	if(f == nil)
		return -1;
	memset(f, 0, sizeof(*f));
	n = 32;
	f->start = malloc(n);
	if(f->start == nil)
		return -1;
	f->to = f->start;
	f->stop = (char*)f->start+n-1;
	f->flush = fmtStrFlush;
	f->farg = (void*)(uintptr)n;
	return 0;
}


/* Return a complete allocated string, releasing the owner on every formatting failure. */
char*
vsmprint(char *fmt, va_list args)
{
	Fmt f;
	char *result;
	int n;

	if(fmt == nil || (uintptr)fmt == (uintptr)-1 || fmtstrinit(&f) < 0)
		return nil;
	va_copy(f.args, args);
	n = dofmt(&f, fmt);
	va_end(f.args);
	if(n < 0){
		free(f.start);
		return nil;
	}
	result = fmtstrflush(&f);
	if(result == nil)
		free(f.start);
	return result;
}
