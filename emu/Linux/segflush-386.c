#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

#include "dat.h"

int
segflush(void *a, ulong n)
{
	uintptr_t address, start, end, extra;
	long page;

	if(n == 0)
		return 0;
	address = (uintptr_t)a;
	if(a == nil || n > UINTPTR_MAX-address)
		return -1;
	page = sysconf(_SC_PAGESIZE);
	if(page <= 0)
		return -1;
	start = address-address%(uintptr_t)page;
	end = address+n;
	extra = end%(uintptr_t)page;
	if(extra != 0){
		extra = page-extra;
		if(extra > UINTPTR_MAX-end)
			return -1;
		end += extra;
	}
	/* Classic 386 code/type helpers use malloc and share allocator pages with
	 * writable data. Keep those pages writable while enabling execution; an
	 * RX-only transition requires a separate code allocator and free contract. */
	return mprotect((void*)start, end-start, PROT_READ|PROT_WRITE|PROT_EXEC);
}
