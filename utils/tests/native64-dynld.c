#include "lib9.h"
#include "dynld.h"
#include <stdio.h>

static int allocations; // Observe that unsupported admission never enters the host pool.
static int reads; // A declined architecture must not ask for object bytes.
static int seeks; // A declined architecture must not move the borrowed file offset.
static int errors; // Exactly one loader rejection is reported to its caller.


/* Supply the selected host allocation boundary and count any unexpected construction attempt. */
void*
mallocz(ulong size, int clear)
{
	allocations++;
	return clear ? calloc(1, size) : malloc(size);
}


/* Bounded file adapter permits a parent implementation to reach an active outcome check. */
static long
readobject(void *file, void *data, long length)
{
	(void)file;
	reads++;
	memset(data, 0, length);
	return length;
}


/* Preserve a stable synthetic position without interpreting executable object metadata. */
static vlong
seekobject(void *file, vlong offset, int origin)
{
	(void)file;
	(void)offset;
	(void)origin;
	seeks++;
	return 0;
}


/* Observe the explicit unsupported-ABI diagnostic through the actual loader API. */
static void
reporterror(char *message)
{
	if(message == nil || strstr(message, "unsupported") == nil)
		errors = -100;
	else
		errors++;
}


/* No code cache is entered for a target whose dynamic-object ABI is unavailable. */
int
segflush(void *address, ulong length)
{
	(void)address;
	(void)length;
	return -1;
}


/* Check refusal before metadata reads, allocation paths or writes through relocation operands. */
int
main(void)
{
	uchar data[64], original[64];
	int i;

	if(dynmagic() != -1) return 11;
	memset(data, 0xa5, sizeof(data));
	memcpy(original, data, sizeof(data));
	for(i = -1; i < 16; i++){
		if(dynreloc(data, ~(ulong)0, i, nil, -1) == nil) return 12;
		if(memcmp(data, original, sizeof(data)) != 0) return 13;
	}
	if(dynloadable(nil, readobject, seekobject) != 0) return 14;
	if(reads != 0 || seeks != 0) return 15;
	if(dynloadgen(nil, readobject, seekobject, reporterror, nil, 0, 0) != nil) return 16;
	if(reads != 0 || seeks != 0 || errors != 1 || allocations != 0) return 17;
	puts("PASS native64 dynamic objects: explicit unsupported ABI; no file callbacks or relocation writes");
	return 0;
}
