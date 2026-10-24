/* The leaf compiler preserves Dis PCs; native listing is supplied by host tools. */
#include "lib9.h"

/* Preserve the optional diagnostic hook without decoding another architecture. */
void
das(uchar *code, int size)
{
	USED(code);
	USED(size);
}
