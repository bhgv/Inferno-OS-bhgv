/* Linux leaf-code ownership: pages are writable during emission and executable afterwards. */
#ifdef __linux__
#include <sys/mman.h>
#endif

/* Release the complete mapping with the same byte extent used at allocation. */
static void
leafcodefree(uchar *code, int size)
{
#ifdef __linux__
	if(code != nil) munmap(code, size);
#else
	USED(code); USED(size);
#endif
}

/* Allocate private code pages without making ordinary VM heap pages executable. */
static uchar*
leafcodealloc(int size)
{
#ifdef __linux__
	void *code;
	code = mmap(nil, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	return code == MAP_FAILED ? nil : code;
#else
	USED(size);
	return nil;
#endif
}

/* Synchronize generated instructions before switching the mapping from RW to RX. */
static int
leafcodepublish(uchar *code, int size)
{
#ifdef __linux__
	__builtin___clear_cache((char*)code, (char*)code+size);
	return mprotect(code, size, PROT_READ|PROT_EXEC);
#else
	USED(code); USED(size);
	return -1;
#endif
}
