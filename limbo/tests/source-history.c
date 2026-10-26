#define _POSIX_C_SOURCE 200809L // Request portable libc declarations without legacy ulong.
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <limits.h>

#ifdef HISTORY_ULONG32 // Model the 32-bit allocator byte-count ABI with native pointers.
typedef unsigned int ulong;
#else // Use the native allocator byte-count ABI.
typedef unsigned long ulong;
#endif // HISTORY_ULONG32

typedef long long Long;
typedef struct File File;

struct File {
	int abs; // First absolute line in this source segment.
	int off; // Offset to the original source line.
};

typedef struct Fline Fline;

struct Fline {
	File *file; // Source segment selected by lookup.
	int line; // Original source line.
};

typedef int Type;
typedef struct Node Node;

struct Node {
	int value; // Ordered case-label value.
};

typedef struct Label Label;

struct Label {
	Node *start; // Inclusive interval start.
	Node *stop; // Inclusive interval end.
};

static File **files;
static int nfiles, lenfiles, lastfile, allocations, failalloc;
static jmp_buf failure;
static ulong requested;


/* Fail visibly without compiling away checks in release configurations. */
static void
check(int condition)
{
	if(!condition){
		fputs("source history check failed\n", stderr);
		exit(1);
	}
}


/* Trap compiler diagnostics before they can access a synthetic giant table. */
static void
fatal(char *message)
{
	(void)message;
	longjmp(failure, 1);
}


/* Count growths and inject allocation failure before publishing the new capacity. */
static void*
reallocmem(void *p, ulong n)
{
	void *result;

	allocations++;
	requested = n;
	if(failalloc)
		fatal("allocation failure");
	result = realloc(p, n);
	if(result == NULL)
		fatal("allocation failure");
	return result;
}


/* Match the compiler's saturating source-line conversion. */
static int
sourceline(Long line)
{
	if(line > INT_MAX)
		return INT_MAX;
	if(line < INT_MIN)
		return INT_MIN;
	return line;
}


/* Compare case values independently of the production interval search. */
static int
casecmp(Type *type, Node *a, Node *b)
{
	(void)type;
	return (a->value > b->value) - (a->value < b->value);
}

IMPLEMENTATION


/* Compare cached and binary lookups with an independent segment-number oracle. */
int
main(void)
{
	File *segments, **saved;
	Label *labels;
	Node *values, query;
	Fline result;
	int i, oldcapacity;
	ulong limit;

	labels = calloc(100000, sizeof *labels);
	values = calloc(100000, sizeof *values);
	check(labels != NULL && values != NULL);
	for(i = 0; i < 100000; i++){
		values[i].value = i;
		labels[i].start = labels[i].stop = &values[i];
	}
	for(i = 99999; i >= 0; i--){
		query.value = i;
		check(findlab(NULL, &query, labels, 100000) == i);
	}
	free(labels);
	free(values);
	segments = calloc(100000, sizeof *segments);
	check(segments != NULL);
	for(i = 0; i < 100000; i++){
		segments[i].abs = i*3+1;
		segments[i].off = -i;
		check(addfile(&segments[i]) == i);
	}
	check(allocations <= 14);
	for(i = 299999; i >= 1; i--){
		result = fline(i);
		check(result.file == &segments[(i-1)/3]);
		check(result.line == i-(i-1)/3);
	}
	saved = files;
	oldcapacity = lenfiles;
	nfiles = lenfiles;
	failalloc = 1;
	if(setjmp(failure) == 0){
		addfile(segments);
		check(0);
	}
	check(files == saved && lenfiles == oldcapacity);
	limit = (~(ulong)0)/sizeof(File*);
	if(limit > INT_MAX)
		limit = INT_MAX;
	nfiles = lenfiles = limit-1;
	if(setjmp(failure) == 0){
		addfile(segments);
		check(0);
	}
	check(files == saved && lenfiles == (int)limit-1);
	check(requested == limit*sizeof(File*));
	failalloc = 0;
	nfiles = lenfiles = limit;
	if(setjmp(failure) == 0){
		addfile(segments);
		check(0);
	}
	check(files == saved && nfiles == (int)limit);
	nfiles = lenfiles = INT_MAX;
	if(setjmp(failure) == 0){
		addfile(segments);
		check(0);
	}
	check(files == saved && nfiles == INT_MAX);
	free(files);
	free(segments);
	puts("source history: 100000 appends, 299999 lookups, capacity/failure bounds passed");
	return 0;
}
