#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include <assert.h>
#include <stdio.h>

char Ebadarg[] = "bad argument", Enomem[] = "out of memory";
static Prog process;
static int released, allocations, live, failalloc, errors, checks;
static int length, position, used, readchunk, writechunk, failread, failwrite, zerowrite;
static int reads, writes, killed, virtualio;
static uvlong virtualleft;
static uchar input[128], output[128];
static void *owner;
#define fdchk(x) ((x) == (Sys_FD*)H ? -1 : (x)->fd)

/* Explicit ownership checks replace unavailable LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Require native operations to run outside the interpreter lock. */
void release(void) { assert(!released); released = 1; }
void acquire(void) { assert(released); released = 0; }
Prog* currun(void) { return &process; }
void seterror(char *s, ...) { assert(!released && s == Ebadarg); errors++; }
void kwerrstr(char *s, ...) { assert(!released && s == Enomem); errors++; }

/* Allocate guarded production storage, including a virtual large-count mode. */
static void*
streamalloc(ulong size)
{
	allocations++;
	if(failalloc)
		return nil;
	assert(live == 0 && size > 0 && size <= 0x7fffffff);
	owner = malloc(virtualio ? 1 : size);
	assert(owner != nil);
	live++;
	return owner;
}

/* Every allocation must be released after reacquiring the VM lock. */
static void
streamfree(void *p)
{
	assert(!released && p == owner && live == 1);
	free(p);
	owner = nil;
	live--;
}

/* Fill exact requested spans, with deterministic fragmentation and read errors. */
long
kread(int fd, void *p, long n)
{
	int count;

	assert(released && fd == 17 && n > 0);
	reads++;
	if(virtualio){
		assert(p == owner);
		if(virtualleft == 0)
			return 0;
		count = n;
		if((uvlong)count > virtualleft)
			count = virtualleft;
		virtualleft -= count;
		return count;
	}
	if(failread >= 0 && position >= failread)
		return -1;
	count = length-position;
	if(count > n)
		count = n;
	if(readchunk && count > readchunk)
		count = readchunk;
	if(failread >= 0 && position+count > failread)
		count = failread-position;
	memmove(p, input+position, count);
	position += count;
	if(killed && reads == killed)
		process.kill = "stop";
	return count;
}

/* Acknowledge only a bounded prefix; errors must never replay earlier bytes. */
long
kwrite(int fd, void *p, long n)
{
	int count;

	assert(released && fd == 18 && n > 0);
	writes++;
	if(virtualio){
		assert(p == owner);
		return n;
	}
	if(failwrite >= 0 && used >= failwrite)
		return zerowrite ? 0 : -1;
	count = n;
	if(writechunk && count > writechunk)
		count = writechunk;
	if(failwrite >= 0 && used+count > failwrite)
		count = failwrite-used;
	assert(used+count <= sizeof(output));
	memmove(output+used, p, count);
	used += count;
	return count;
}

#define malloc streamalloc
#define free streamfree
IMPLEMENTATION
#undef malloc
#undef free

/* Reset only the fixture state; production owns the temporary stream buffer. */
static void
setup(void)
{
	int i;

	assert(!released && live == 0);
	memset(&process, 0, sizeof(process));
	memset(output, 0xa5, sizeof(output));
	for(i = 0; i < sizeof(input); i++)
		input[i] = i*37+11;
	allocations = failalloc = errors = reads = writes = position = used = 0;
	readchunk = writechunk = killed = virtualio = zerowrite = 0;
	failread = failwrite = -1;
}

static int returnoffset;

/* Enter through the actual generated builtin frame and native pointer ABI. */
static WORD
invoke(WORD size)
{
	F_Sys_stream f = {0};
	Sys_FD src = {17}, dst = {18};
	WORD result = -77;
	union { uvlong align; uchar bytes[32]; } slot;
	uchar expected[32];
	WORD *destination=(void*)(slot.bytes+8+returnoffset);

	memset(slot.bytes,0xa5,sizeof(slot.bytes));
	memmove(slot.bytes+8+returnoffset,&result,sizeof(result));
	memmove(expected,slot.bytes,sizeof(expected));
	f.src = &src;
	f.dst = &dst;
	f.bufsiz = size;
	f.ret = destination;
	Sys_stream(&f);
	assert(!released && live == 0);
	memmove(&result,slot.bytes+8+returnoffset,sizeof(result));
	memmove(expected+8+returnoffset,&result,sizeof(result));
	assert(!memcmp(slot.bytes,expected,sizeof(expected)));
	return result;
}

/* Check all combinations of buffered read/write fragmentation against a byte oracle. */
static void
transfers(void)
{
	int size, len, r, w;

	for(size = 1; size <= 16; size++)
	for(len = 0; len <= 40; len++)
	for(r = 0; r <= 3; r++)
	for(w = 0; w <= 3; w++){
		setup();
		length = len;
		readchunk = r;
		writechunk = w;
		assert(invoke(size) == len && used == len);
		assert(memcmp(input, output, len) == 0);
		assert(output[len] == 0xa5 && allocations == 1);
		checks++;
	}
}

/* Fail writes in full and final buffers, retaining only acknowledged prefixes. */
static void
failures(void)
{
	int size, stop, zero;

	for(size = 1; size <= 16; size++)
	for(stop = 0; stop <= 32; stop++)
	for(zero = 0; zero <= 1; zero++){
		setup();
		length = 32;
		writechunk = 3;
		failwrite = stop;
		zerowrite = zero;
		assert(invoke(size) == stop && used == stop);
		assert(memcmp(input, output, stop) == 0 && output[stop] == 0xa5);
		checks++;
	}
	for(stop = 0; stop <= 32; stop++){
		setup();
		length = 32;
		failread = stop;
		readchunk = writechunk = 3;
		assert(invoke(8) == stop && used == stop);
		checks++;
	}
	setup();
	failalloc = 1;
	assert(invoke(8) == -1 && errors == 1 && reads == 0 && writes == 0);
	checks++;
	setup();
	length = 32;
	readchunk = writechunk = 3;
	killed = 2;
	assert(invoke(8) == 6 && used == 6 && reads == 2);
	checks++;
}

/* Reject invalid full-width sizes before allocation or native I/O. */
static void
bounds(void)
{
	WORD sizes[6];
	int i, count;

	sizes[0] = 0;
	sizes[1] = -1;
	sizes[2] = (WORD)(-0x7fffffff-1);
	count = 3;
	if(sizeof(WORD) == 8){
		sizes[count++] = (WORD)((uvlong)1<<32);
		sizes[count++] = (WORD)(((uvlong)1<<32)+8);
		sizes[count++] = (WORD)(~(UWORD)0>>1);
	}
	for(i = 0; i < count; i++){
		setup();
		assert(invoke(sizes[i]) == -1 && errors == 1 && allocations == 0 && reads == 0);
		checks++;
	}
}

/* Model multi-gigabyte acknowledgments without allocating or accessing a huge buffer. */
static void
counts(void)
{
	WORD result, expected;
	int blocks;

	for(blocks = 2; blocks <= 4; blocks++){
		setup();
		virtualio = 1;
		virtualleft = (uvlong)blocks*0x40000000;
		if(sizeof(WORD) == 8)
			expected = virtualleft;
		else
			expected = blocks == 2 ? -0x7fffffff-1 : blocks == 3 ? -0x40000000 : 0;
		result = invoke(0x40000000);
		assert(result == expected);
		assert(reads == blocks+1 && writes == blocks);
		checks++;
	}
}

/* Keep baseline selectors independent so each old behavior is demonstrable. */
int
main(int argc, char **argv)
{
	for(returnoffset=0;returnoffset<8;returnoffset++){
		if(argc == 1 || strcmp(argv[1], "transfer") == 0)
			transfers();
		if(argc == 1 || strcmp(argv[1], "failure") == 0)
			failures();
		if(argc == 1 || strcmp(argv[1], "bounds") == 0)
			bounds();
		if(argc == 1 || strcmp(argv[1], "count") == 0)
			counts();
	}
	printf("Sys.stream WORD%u: %d transfer/count/ownership cases passed\n", (unsigned)(8*sizeof(WORD)), checks);
	return 0;
}
