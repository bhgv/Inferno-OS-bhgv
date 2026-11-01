#include "lib9.h"


/* Obtain a diagnostic return PC through the compiler's target ABI, including AArch64 PAC. */
uintptr
getcallerpc(void *unused)
{
	(void)unused;
	return (uintptr)__builtin_extract_return_addr(__builtin_return_address(0));
}
