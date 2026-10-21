#include <sys/types.h>
#include "dat.h"

int
segflush(void *a, ulong n)
{
	USED(a);
	if(n == 0)
		return 0;
#ifdef __riscv
	__asm__ volatile(".word 0x0000100f" ::: "memory"); /* fence.i */
	return 0;
#else
	return -1;
#endif
}
