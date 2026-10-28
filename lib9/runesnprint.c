#include "lib9.h"


/* Format a bounded Rune string through the checked variadic core. */
int
runesnprint(Rune *buf, int len, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = runevsnprint(buf, len, fmt, args);
	va_end(args);
	return n;
}

