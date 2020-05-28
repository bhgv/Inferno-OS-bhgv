#include <lib9.h>

#if 0 //{}
ulong
getcallerpc(void *x)
{
ulong *lp;

	lp = x;

	return lp[-1];
}
#endif
