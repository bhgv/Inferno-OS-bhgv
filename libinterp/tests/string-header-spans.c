#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <stdio.h>
#include <setjmp.h>

typedef unsigned short HeaderAddress; // Reduced arithmetic domain; payload pointers stay native.

char exHeap[] = "heap", exNilref[] = "nil";
static jmp_buf boundary;
static char *raised;
static String *result;
static int allocations;
static union {
	uvlong align; // Align the private destination and both character representations.
	uchar bytes[1024]; // Single unpublished output with surrounding canaries.
} destination;


/* Surface production VM errors without permitting an invalid source read. */
void
error(char *message)
{
	raised = message;
	longjmp(boundary, 1);
}


/* Retain address instrumentation without process-based leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Count allocations so rejected inputs cannot silently allocate first. */
String*
newstring(int length)
{
	String *s;

	if(length < 0 || length > 128)
		abort();
	allocations++;
	memset(destination.bytes, 0xa5, sizeof(destination.bytes));
	s = (String*)destination.bytes;
	s->len = s->max = length;
	return s;
}


/* Use the same output storage for signed Rune-length duplicates. */
String*
newrunes(int length)
{
	String *s;

	s = newstring(length);
	s->len = -length;
	return s;
}

IMPLEMENTATION


/* Keep nonlocal exits inside one function, outside the test loop's locals. */
static int
copyresult(String *s, int reduced)
{
	raised = nil;
	result = nil;
	if(setjmp(boundary))
		return 0;
	result = reduced ? modeldup(s) : stringdup(s);
	return 1;
}


/* Require header rejection before even an identity comparison can return. */
static int
compareresult(String *left, String *right)
{
	raised = nil;
	if(setjmp(boundary))
		return 0;
	stringcmp(left, right);
	return 1;
}


/* Compare production arithmetic against independent source-span and byte-copy oracles. */
int
main(void)
{
	uchar *allocation, *base, saved[1024];
	String *s, *bad;
	int offsets[] = {0xfff8, 0xfff0, 0xffe0, 0xff00, 0x800};
	int lengths[] = {0, 1, 7, 32};
	int offset, length, wide, mode, reduced, before, success, expected, checks = 0;
	int bytes, i;
	uintptr begin;

	allocation = malloc(0x40000);
	if(allocation == nil)
		return 1;
	base = (uchar*)(((uintptr)allocation+0xffff)&~(uintptr)0xffff);
	for(offset = 0; offset < nelem(offsets); offset++)
	for(length = 0; length < nelem(lengths); length++)
	for(wide = 0; wide < 2; wide++)
	for(mode = 0; mode < 4; mode++)
	for(reduced = 0; reduced < 2; reduced++){
		if(getenv("STRING_HEADER_PROBE") && !strcmp(getenv("STRING_HEADER_PROBE"), "payload") &&
		   (offset != 2 || length != 3 || wide != 1 || mode != 0 || reduced != 1))
			continue;
		s = (String*)(base+offsets[offset]);
		memset((uchar*)s-sizeof(Heap), 0, 1024+sizeof(Heap));
		D2H(s)->ref = mode == 3 ? 0 : 1;
		s->len = wide ? -lengths[length] : lengths[length];
		s->max = mode == 1 ? -1 : lengths[length];
		if(mode == 2)s->len = (-0x7fffffff-1);
		bytes = lengths[length]*(wide ? sizeof(Rune) : 1);
		for(i = 0; i < bytes; i++)s->Sascii[i] = (uchar)(i*37+11);
		memmove(saved, s, sizeof(saved));
		begin = offsets[offset];
		expected = mode == 0 && (!reduced ||
			(begin+offsetof(String, max)+sizeof(s->max) <= 0xffff &&
			 begin+offsetof(String, data)+bytes <= 0xffff));
		before = allocations;
		success = copyresult(s, reduced);
		if(success != expected || memcmp(saved, s, sizeof(saved)))return 2;
		if(expected){
			if(allocations != before+1 || result->len != s->len ||
			   memcmp(result->Sascii, s->Sascii, bytes))return 3;
		}else if(raised != exHeap || allocations != before)return 4;
		checks++;
	}
	for(i = 1; i < 8; i++){
		bad = (String*)((uintptr)-1-i);
		before = allocations;
		if(copyresult(bad, 0) || raised != exHeap || allocations != before)return 5;
		if(compareresult(bad, bad) || raised != exHeap)return 6;
		if(compareresult(bad, H) || raised != exHeap)return 7;
		if(compareresult(H, bad) || raised != exHeap)return 8;
		checks += 4;
	}
	for(i = 1; i <= sizeof(Heap); i++){
		if(copyresult((String*)(uintptr)i, 0) || raised != exHeap)return 9;
		checks++;
	}
	if(copyresult(nil, 0) || raised != exNilref)return 10;
	before = allocations;
	if(!copyresult(H, 0) || result != H || allocations != before)return 11;
	free(allocation);
	printf("PASS string headers/copy spans: %d cases WORD%zu\n", checks+2, sizeof(WORD)*8);
	return 0;
}
