#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <limits.h>
#include "isa.h"

typedef unsigned char uchar;
typedef unsigned long ulong;
typedef int64_t Long;
typedef double Real;
typedef struct Sym { char *name; int len; } Sym;
#define nil NULL
static uchar *cache;
static int ncached, ndatum, startoff, lastoff, lastkind, lencache;
static int bout, emitted, readat, requests, rejectalloc, errors, checks;
static ulong requested, ownedsize;
static uchar *owner;
static uchar output[300000], input[65536];
static jmp_buf escape;

static void disflush(int, long, long);

/* Capture compiler failures without replacing any production encoding logic. */
static void
fatal(char *message, ...)
{
	assert(message != nil);
	errors++;
	longjmp(escape, 1);
}

/* Relocate every growth and detect an allocator loop instead of hanging. */
static void*
reallocmem(void *old, ulong size)
{
	uchar *next;

	requests++;
	assert(requests <= 32);
	requested = size;
	if(rejectalloc)
		fatal("injected allocation failure");
	assert(size > 0 && size <= sizeof(output) && old == cache);
	next = malloc(size+32);
	assert(next != nil);
	memset(next, 0xa5, size+32);
	if(old != nil)
		memcpy(next+16, old, ownedsize < size ? ownedsize : size);
	free(owner);
	owner = next;
	ownedsize = size;
	return next+16;
}

/* Preserve byte-order and count information in an independent output buffer. */
static int
Bputc(int stream, int byte)
{
	(void)stream;
	assert(emitted < sizeof(output));
	output[emitted++] = byte;
	return 0;
}

/* Keep record payload writes exact and observable. */
static long
Bwrite(int stream, void *data, long n)
{
	(void)stream;
	assert(n >= 0 && emitted+n <= sizeof(output));
	if(n > 0)
		memcpy(output+emitted, data, n);
	emitted += n;
	return n;
}

IMPLEMENTATION

/* Reset a compilation's cache and test allocator ownership. */
static void
reset(void)
{
	free(owner);
	owner = cache = nil;
	ownedsize = requested = 0;
	ncached = ndatum = lencache = emitted = readat = requests = errors = rejectalloc = 0;
	startoff = lastoff = lastkind = -1;
}

/* Check allocator guards independently of the compiler's capacity counter. */
static void
guards(void)
{
	int i;

	if(owner != nil){
		assert(cache == owner+16 && lencache == ownedsize);
		for(i = 0; i < 16; i++)
			assert(owner[i] == 0xa5 && owner[16+ownedsize+i] == 0xa5);
	}
}

/* Decode signed 7/14/30-bit operands through unsigned arithmetic. */
static long
operand(void)
{
	uint32_t value, sign;
	int first, count, i;

	assert(readat < emitted);
	first = output[readat++];
	count = first < 128 ? 1 : first < 192 ? 2 : 4;
	value = first & (count == 1 ? 127 : 63);
	for(i = 1; i < count; i++){
		assert(readat < emitted);
		value = value*256+output[readat++];
	}
	sign = (uint32_t)1 << (count == 1 ? 6 : count == 2 ? 13 : 29);
	return value & sign ? (long)value-(long)(2*sign) : value;
}

/* Verify a complete record against an independent payload and address oracle. */
static void
record(int kind, int count, int offset, uchar *data, int bytes)
{
	int header;
	long actual;

	assert(readat < emitted);
	header = output[readat++];
	assert((header>>4) == kind);
	actual = header&15;
	if(actual == 0)
		actual = operand();
	assert(actual == count && operand() == offset);
	assert(readat+bytes <= emitted);
	if(bytes)
		assert(memcmp(output+readat, data, bytes) == 0);
	readat += bytes;
	checks++;
}

/* Probe initial/existing capacities at and beyond the old stalled-growth boundary. */
static void
blocks(void)
{
	int lengths[] = {1,15,16,63,64,1023,1024,1025,2047,2048,2049,8193,65536};
	int i, prefix, n;

	for(prefix = 0; prefix <= 33; prefix += 11)
	for(i = 0; i < (int)(sizeof(lengths)/sizeof(*lengths)); i++){
		n = lengths[i];
		if(prefix > n)
			continue;
		reset();
		if(prefix)
			disbytes(17, input, prefix);
		disbytes(17+prefix, input+prefix, n-prefix);
		disflush(-1,-1,0);
		record(DEFB,n,17,input,n);
		assert(readat == emitted);
		guards();
	}
}

/* Mix canonical scalar records, discontinuous offsets and standalone UTF strings. */
static void
mixed(void)
{
	uchar wanted[32];
	uint64_t bits;
	int i;
	Sym string = {"\xD0\x9B\xD0\xB8\xD0\xBC\xD0\xB1\xD0\xBE",10};

	reset();
	for(i = 0; i < 32; i++)
		disbyte(8+i, input[i]);
	for(i = 0; i < 8; i++)
		disint(40+4*i, (int32_t)(0x80000000U+0x10101U*i));
	for(i = 0; i < 4; i++)
		disbig(80+8*i, (Long)(UINT64_C(0x8000000000000000)+17*i));
	disreal(112, -0.0);
	disreal(120, 1.5);
	disstring(128, &string);
	string.len = 0;
	disstring(132, &string);
	disflush(-1,-1,0);
	record(DEFB,32,8,input,32);
	for(i = 0; i < 8; i++){
		uint32_t value = 0x80000000U+0x10101U*i;
		wanted[4*i] = value>>24;
		wanted[4*i+1] = value>>16;
		wanted[4*i+2] = value>>8;
		wanted[4*i+3] = value;
	}
	record(DEFW,8,40,wanted,32);
	for(i = 0; i < 32; i++){
		bits = UINT64_C(0x8000000000000000)+17*(i/8);
		wanted[i] = bits>>(56-8*(i%8));
	}
	record(DEFL,4,80,wanted,32);
	for(i = 0; i < 16; i++){
		bits = i < 8 ? UINT64_C(0x8000000000000000) : UINT64_C(0x3ff8000000000000);
		wanted[i] = bits>>(56-8*(i%8));
	}
	record(DEFF,2,112,wanted,16);
	record(DEFS,10,128,(uchar*)string.name,10);
	record(DEFS,0,132,nil,0);
	assert(readat == emitted);
	guards();
}

/* Empty source spans do not allocate, dereference or split a pending record. */
static void
empty(void)
{
	reset();
	disbytes(0,nil,0);
	assert(cache == nil && requests == 0 && emitted == 0);
	disbyte(5, 0x7a);
	disbytes(999,nil,0);
	disbyte(6, 0x42);
	disflush(-1,-1,0);
	input[0] = 0x7a;
	input[1] = 0x42;
	record(DEFB,2,5,input,2);
	assert(readat == emitted);
	guards();
}

/* Reject malformed spans and headers before any output or allocation. */
static void
invalid(void)
{
	long sizes[] = {-1,LONG_MIN,536870912L,LONG_MAX};
	long offsets[] = {-1,LONG_MIN,536870912L,LONG_MAX};
	int i;

	for(i = 0; i < 4; i++){
		reset();
		if(setjmp(escape) == 0)
			disflush(DEFB,0,sizes[i]);
		assert(errors == 1 && emitted == 0 && requests == 0);
		reset();
		if(setjmp(escape) == 0)
			disflush(DEFB,offsets[i],1);
		assert(errors == 1 && emitted == 0 && requests == 0);
		reset();
		if(setjmp(escape) == 0)
			disdata(DEFB,sizes[i]);
		assert(errors == 1 && emitted == 0);
		checks += 3;
	}
}

/* Observe failed capacity requests without allocating a large data span. */
static void
allocation(void)
{
	long sizes[] = {1,1024,1025,536870911L};
	ulong capacities[] = {1024,1024,2048,536870911UL};
	int i;

	for(i = 0; i < 4; i++){
		reset();
		rejectalloc = 1;
		if(setjmp(escape) == 0)
			disflush(DEFB,0,sizes[i]);
		assert(errors == 1 && requests == 1 && requested == capacities[i]);
		assert(cache == nil && lencache == 0);
		checks++;
	}
}

/* Check header count transitions without materializing their potentially large payloads. */
static void
headers(void)
{
	long counts[] = {0,1,15,16,63,64,8191,8192,536870911L};
	int kinds[] = {-1,16,INT_MIN,INT_MAX};
	int kind, i, byte;
	long count;

	for(kind = 0; kind < 16; kind++)
	for(i = 0; i < 9; i++){
		reset();
		disdata(kind,counts[i]);
		byte = output[readat++];
		assert((byte>>4) == kind);
		count = byte&15;
		if(count == 0)
			count = operand();
		assert(count == counts[i] && readat == emitted);
		checks++;
	}
	for(i = 0; i < 4; i++){
		reset();
		if(setjmp(escape) == 0)
			disdata(kinds[i],1);
		assert(errors == 1 && emitted == 0);
		checks++;
	}
}

/* Keep noncontiguous/rewound byte spans in distinct records in their original order. */
static void
fragmented(void)
{
	reset();
	disbytes(8,input,64);
	disbytes(100,input,37);
	disbytes(20,input,1024);
	disflush(-1,-1,0);
	record(DEFB,64,8,input,64);
	record(DEFB,37,100,input,37);
	record(DEFB,1024,20,input,1024);
	assert(readat == emitted);
	guards();
}

/* Run production data-cache paths with a moving allocator and independent decoder. */
int
main(int argc, char **argv)
{
	int i;

	for(i = 0; i < sizeof(input); i++)
		input[i] = (uchar)(i*37+11);
	if(argc == 1 || strcmp(argv[1],"blocks") == 0)
		blocks();
	if(argc == 1 || strcmp(argv[1],"mixed") == 0)
		mixed();
	if(argc == 1 || strcmp(argv[1],"empty") == 0)
		empty();
	if(argc == 1 || strcmp(argv[1],"invalid") == 0)
		invalid();
	if(argc == 1 || strcmp(argv[1],"allocation") == 0)
		allocation();
	if(argc == 1 || strcmp(argv[1],"headers") == 0)
		headers();
	if(argc == 1 || strcmp(argv[1],"fragmented") == 0)
		fragmented();
	reset();
	printf("Dis data cache: %d scenarios passed\n",checks);
	return 0;
}
