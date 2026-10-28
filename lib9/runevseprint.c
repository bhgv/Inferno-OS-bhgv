#include "lib9.h"
#include "fmtdef.h"


/* Validate aligned native Rune endpoints before pointer formation or format dispatch. */
Rune*
runevseprint(Rune *buf, Rune *e, char *fmt, va_list args)
{
	Fmt f;

	if(buf == nil || (uintptr)buf == (uintptr)-1 || fmt == nil || (uintptr)fmt == (uintptr)-1 ||
	   (uintptr)e <= (uintptr)buf || (uintptr)buf%sizeof(Rune) || (uintptr)e%sizeof(Rune))
		return nil;
	f.runes = 1;
	f.start = buf;
	f.to = buf;
	f.stop = e - 1;
	f.flush = nil;
	f.farg = nil;
	f.nfmt = 0;
	va_copy(f.args, args);
	dofmt(&f, fmt);
	va_end(f.args);
	*(Rune*)f.to = '\0';
	return f.to;
}



/* Check a Rune count before constructing the bounded endpoint. */
int
runevsnprint(Rune *buf, int len, char *fmt, va_list args)
{
	Rune *end;

	if(len <= 0 || buf == nil || (uintptr)buf%sizeof(Rune) ||
	   (uintptr)len > ((uintptr)-1-(uintptr)buf)/sizeof(Rune))
		return -1;
	end = runevseprint(buf, buf+len, fmt, args);
	if(end == nil)
		return -1;
	return end-buf;
}
