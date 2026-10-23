#include "lib9.h"

/* Keep the counter as wide as the public length result on LP64 hosts. */
long
runestrlen(Rune *s)
{
	long i;

	i = 0;
	while(*s++)
		i++;
	return i;
}
