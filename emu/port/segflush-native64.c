#include "dat.h"


/* Use the target OS/compiler cache service, including RV64 cross-hart synchronization. */
int
segflush(void *address, ulong length)
{
	uintptr start;

	if(length == 0)
		return 0;
	start = (uintptr)address;
	if(address == nil || length > ~(uintptr)0-start)
		return -1;
	__asm__ volatile("" ::: "memory");
	__builtin___clear_cache((char*)address, (char*)address+length);
	__asm__ volatile("" ::: "memory");
	return 0;
}
