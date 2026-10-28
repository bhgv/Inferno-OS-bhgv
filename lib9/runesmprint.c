#include "lib9.h"


/* Return an owned Rune string through the shared variadic allocator. */
Rune*
runesmprint(char *fmt, ...)
{
	va_list args;
	Rune *p;

	va_start(args, fmt);
	p = runevsmprint(fmt, args);
	va_end(args);
	return p;
}
