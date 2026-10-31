#define _GNU_SOURCE 1 // Expose MAP_ANONYMOUS for the real W^X code-publication check.
#include "lib9.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

extern int segflush(void*, ulong);


/* Execute newly published and rewritten native code using the production cache service. */
int
main(void)
{
	long page;
	uchar *code;
	int (*entry)(void);

	page = sysconf(_SC_PAGESIZE);
	if(page <= 0) return 1;
	code = mmap(nil, page, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(code == MAP_FAILED) return 2;
	if(segflush(nil, 0) != 0 || segflush(nil, 1) != -1 ||
	   segflush((void*)(~(uintptr)0-2), 4) != -1) return 3;
#if defined(__x86_64__)
	code[0] = 0xb8;
	code[1] = 37;
	code[2] = code[3] = code[4] = 0;
	code[5] = 0xc3;
#else
#error this hosted execution gate currently requires native x86-64
#endif
	if(segflush(code, 6) != 0) return 4;
	if(mprotect(code, page, PROT_READ|PROT_EXEC) != 0) return 5;
	entry = (int(*)(void))code;
	if(entry() != 37) return 6;
	if(mprotect(code, page, PROT_READ|PROT_WRITE) != 0) return 7;
	code[1] = 99;
	if(segflush(code, 6) != 0) return 8;
	if(mprotect(code, page, PROT_READ|PROT_EXEC) != 0) return 9;
	if(entry() != 99) return 10;
	if(munmap(code, page) != 0) return 11;
	printf("PASS native cache publication/rewrite; OS page size %ld; invalid ranges rejected\n", page);
	return 0;
}
