#include "lib9.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <sys/mman.h>

/* Check varied scalar values and every short terminator position. */
static void
small(void)
{
	Rune text[512];
	int n, i;

	for(n = 0; n < nelem(text); n++){
		for(i = 0; i < n; i++)
			text[i] = i%3 == 0 ? 0x1f600 : i%3 == 1 ? 0xfffd : 'x';
		text[n] = 0;
		assert(runestrlen(text) == n);
		assert(runestrlen(text+n) == 0);
	}
}

/* Reuse shared backing inside one owned reservation for a genuine wide span. */
static void
wide(void)
{
	ulong length, bytes, chunk, capacity, offset, page, lastpage;
	Rune *text, *piece;
	char *base;
	FILE *file;
	ulong i;

	if(sizeof(long) <= sizeof(int)){
		puts("wide Rune fixture requires LP64");
		return;
	}
	length = (ulong)INT_MAX+34;
	bytes = (length+1)*sizeof(Rune);
	chunk = 2*1024*1024;
	page = sysconf(_SC_PAGESIZE);
	capacity = (bytes/chunk+1)*chunk;
	base = mmap(nil, capacity, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	assert(base != MAP_FAILED);
	file = tmpfile();
	assert(file != nil && ftruncate(fileno(file), chunk) == 0);
	piece = mmap(nil, chunk, PROT_READ|PROT_WRITE, MAP_SHARED, fileno(file), 0);
	assert(piece != MAP_FAILED);
	for(i = 0; i < chunk/sizeof(Rune); i++)
		piece[i] = 0x1f600;
	assert(munmap(piece, chunk) == 0);
	for(offset = 0; offset < capacity; offset += chunk)
		assert(mmap(base+offset, chunk, PROT_READ, MAP_SHARED|MAP_FIXED,
			fileno(file), 0) == base+offset);
	lastpage = length*sizeof(Rune)/page*page;
	assert(mmap(base+lastpage, page, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0) == base+lastpage);
	piece = (Rune*)(base+lastpage);
	for(i = 0; i < page/sizeof(Rune); i++)
		piece[i] = 0x1f600;
	text = (Rune*)base;
	text[length] = 0;
	assert(runestrlen(text) == length);
	assert(runestrlen(text+length-3) == 3);
	assert(munmap(base, capacity) == 0 && fclose(file) == 0);
	printf("Rune length: %lu scalars passed\n", length);
}

/* Keep large virtual mappings optional for constrained build environments. */
int
main(int argc, char **argv)
{
	(void)argv;
	small();
	if(argc > 1)
		wide();
	puts("Rune length: 512 small lengths passed");
	return 0;
}
