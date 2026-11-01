#include "../../emu/port/alloc.c"
#include <stdio.h>

static uchar arena[4*1024*1024]; // Bounded OS arena, separate from the tested pool.
static ulong used; // Bytes supplied by the fixture OS.
Bhdr *ptr; // Collector cursor; this test does not invoke a collector.


/* Model an idle scheduler without replacing pool operations. */
Prog*
currun(void)
{
	return nil;
}


/* Supply aligned backing memory without using the host allocator. */
void*
sbrk(intptr_t size)
{
	void *p;

	if(size < 0 || (ulong)size > sizeof(arena)-used)
		return (void*)-1;
	p = arena+used;
	used += size;
	return p;
}


/* This fixture is single-threaded. */
void
lock(Lock *p)
{
	USED(p);
}


/* This fixture is single-threaded. */
void
unlock(Lock *p)
{
	USED(p);
}


/* Caller tags do not affect block geometry. */
uintptr
getcallerpc(void *p)
{
	USED(p);
	return 0x12345678;
}


/* Pool faults must fail even under NDEBUG. */
void
panic(char *fmt, ...)
{
	USED(fmt);
	abort();
}


/* Ignore Inferno formatting while preserving the actual fault path. */
int
print(char *fmt, ...)
{
	USED(fmt);
	return 0;
}


/* Ignore Inferno formatting while preserving the actual fault path. */
int
fprint(int fd, char *fmt, ...)
{
	USED(fd);
	USED(fmt);
	return 0;
}


/* Exercise minimum blocks, both coalescing directions and preserved payloads. */
int
main(void)
{
	void *p[1024], *grown;
	int i, j, k;

	if(POOLQUANTA+1 < sizeof(Bhdr)+sizeof(Btail))
		return 3;
	for(j=0; j<10; j++){
		for(i=0; i<1024; i++){
			p[i] = poolalloc(mainmem, i%113);
			if(p[i] == nil)
				return 1;
			memset(p[i], i, i%113);
		}
		for(i=0; i<1024; i+=2)
			poolfree(mainmem, p[i]);
		for(i=1; i<1024; i+=2){
			for(k=0; k<i%113; k++)
				if(((uchar*)p[i])[k] != (uchar)i)
					return 2;
			grown = poolrealloc(mainmem, p[i], 256);
			if(grown == nil)
				return 4;
			for(k=0; k<i%113; k++)
				if(((uchar*)grown)[k] != (uchar)i)
					return 5;
			poolfree(mainmem, grown);
		}
		if(mainmem->cursize != 0)
			return 6;
	}
	p[0] = malloc(32);
	if(p[0] == nil)
		return 7;
	memset(p[0], 0x5a, 32);
	for(i=0; i<32; i++){
		ulong before = mainmem->nalloc;
		volatile size_t huge = (size_t)-1-i;
		if(mallocz(huge, 0) != nil || malloc(huge) != nil || kmalloc(huge) != nil || realloc(p[0], huge) != nil)
			return 8;
		if(mainmem->nalloc != before || mainmem->cursize == 0)
			return 9;
		for(k=0; k<32; k++)
			if(((uchar*)p[0])[k] != 0x5a)
				return 10;
	}
	free(p[0]);
	p[0] = realloc(nil, 32);
	if(p[0] == nil || getmalloctag(p[0]) != 0x12345678 || getrealloctag(p[0]) != 0x12345678)
		return 11;
	free(p[0]);
	if(mainmem->cursize != 0)
		return 12;
	puts("PASS actual allocation API: 128 oversized refusals retain owner; realloc(nil) initializes both tags");
	puts("PASS actual pool: 10240 allocations, 5120 reallocations, payloads and coalescing");
	return 0;
}
