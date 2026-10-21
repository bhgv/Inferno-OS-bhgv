#include <sys/types.h>
#include <sys/syscall.h>

#include "dat.h"

#define	SYS_cacheflush	__ARM_NR_cacheflush

int
segflush(void *a, ulong n)
{
	if(n)
		/* Linux ARM cacheflush takes an exclusive end and zero flags. */
		return syscall(SYS_cacheflush, a, (char*)a+n, 0);
	return 0;
}
