#include "lib9.h"
#include "dynld.h"


/* No native64 a.out relocation ABI has been defined; never reuse another CPU's 32-bit magic. */
long
dynmagic(void)
{
	return -1;
}


/* Refuse relocation without reading or modifying an untrusted object or symbol table. */
char*
dynreloc(uchar *base, ulong offset, int mode, Dynsym **symbols, int count)
{
	(void)base;
	(void)offset;
	(void)mode;
	(void)symbols;
	(void)count;
	return "native64 a.out relocation is unsupported";
}
