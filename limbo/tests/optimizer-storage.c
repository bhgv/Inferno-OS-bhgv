#include "limbo.h"
#include <setjmp.h>
#include <stdio.h>

#define BLEN 32 // Fixed optimizer cell width, independent of the target WORD.

static jmp_buf escape; // Capture fatal rejection separately from attempted allocation.
static int trapping; // Nonzero only while an expected boundary is under observation.
static int live; // Number of private allocations still owned by production code.
static ulong requested; // Byte count observed at the allocator boundary.
static int checks; // Number of completed regression predicates.


/* Keep address/undefined instrumentation without unavailable process leak scanning. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Preserve all test predicates even in optimized NDEBUG builds. */
static void
check(int condition)
{
	if(!condition)
		abort();
	checks++;
}


/* Distinguish compiler rejection from an attempted oversized allocation. */
void
fatal(char *format, ...)
{
	USED(format);
	if(trapping)
		longjmp(escape, 1);
	abort();
}


/* Track actual ownership and observe huge sizes without allocating huge buffers. */
void*
allocmem(ulong n)
{
	void *p;

	requested = n;
	if(n > 1024*1024){
		if(trapping)
			longjmp(escape, 2);
		abort();
	}
	p = malloc(n == 0 ? 1 : n);
	check(p != nil);
	live++;
	return p;
}


/* Count every release while retaining the real host allocator's diagnostics. */
static void
release(void *p)
{
	if(p != nil)
		live--;
	free(p);
}

#define free release // Observe production graph and bitmap ownership.

IMPLEMENTATION



/* Run an independent boundary family without using huge input programs. */
static void
boundary(char *family)
{
	Array array = {0};
	Inst instruction = {0};
	Bits a = {0}, b = {0};
	int caught, which;

	for(which = 0; which < 4; which++){
		requested = 0;
		trapping = 1;
		caught = setjmp(escape);
		if(caught == 0){
			if(strcmp(family, "bitmap") == 0)
				bnew(which < 2 ? -1-which : 0x7fffffff/BLEN+2+which, which < 2);
			else if(strcmp(family, "rows") == 0)
				allocbits(which == 0 ? 0 : which == 1 ? -1 : 0x7fffffff/sizeof(Bits)+which, 32);
			else if(strcmp(family, "positions") == 0)
				optpositions(which == 0 ? -1 : 0x7fffffff, which == 1 ? 0 : which == 2 ? -1 : 1);
			else if(strcmp(family, "leaders") == 0){
				array.n = array.m = which < 2 ? 0x7fffffff/sizeof(Block*)+which : 0x7fffffff-which;
				leader(&instruction, &array);
			}else if(strcmp(family, "lookup") == 0){
				array.n = 1;
				instruction.pc = which < 2 ? which-1 : which;
				findb(&instruction, &array);
			}else{
				a.n = which+1;
				b.n = 0;
				beq(a, b);
			}
		}
		trapping = 0;
		check(caught == 1 && requested == 0 && live == 0);
	}
}


/* Observe exact legal upper extents without allocating multi-gigabyte fixtures. */
static void
extents(void)
{
	Array array = {0};
	Inst instruction = {0};
	int caught, which;
	ulong expected;

	for(which = 0; which < 5; which++){
		expected = which < 2 ? (ulong)(0x7fffffff/BLEN+1)*sizeof(u32int) :
			which == 2 ? (ulong)(0x7fffffff/sizeof(Bits))*sizeof(Bits) :
			(ulong)(0x7fffffff/sizeof(Block*))*sizeof(Block*);
		requested = 0;
		trapping = 1;
		caught = setjmp(escape);
		if(caught == 0){
			if(which < 2)
				bnew(which == 0 ? 0x7fffffff : 0x7fffffff/BLEN+1, which == 0);
			else if(which == 2)
				allocbits(0x7fffffff/sizeof(Bits), 1);
			else{
				array.n = array.m = 0x7fffffff/sizeof(Block*)-which;
				leader(&instruction, &array);
			}
		}
		trapping = 0;
		check(caught == 2 && requested == expected && live == 0);
	}
}


/* Compare ordinary bit/row counts and leader identities against direct oracles. */
static void
ordinary(void)
{
	Bits b, *rows;
	Array array = {0};
	Inst instructions[129] = {0};
	Decl declarations[129] = {0};
	int n, j;

	for(n = 0; n < 4097; n++){
		b = bnew(n, 1);
		check(b.n == n/32+(n%32 != 0));
		for(j = 0; j < b.n; j++)
			check(b.b[j] == 0);
		bfree(b);
		check(optpositions(n, 1) == n+1);
	}
	rows = allocbits(129, 65);
	for(n = 0; n < 129; n++){
		check(rows[n].n == 3);
		bfree(rows[n]);
	}
	free(rows);
	array.m = 1;
	array.a = allocmem(sizeof(Block*));
	for(n = 0; n < 129; n++){
		leader(&instructions[n], &array);
		check(array.n == n+1 && instructions[n].pc == n+1);
		check(findb(&instructions[n], &array) == array.a[n]);
		leader(&instructions[n], &array);
		check(array.n == n+1);
		instructions[n].next = n == 128 ? nil : &instructions[n+1];
		declarations[n].next = n == 128 ? nil : &declarations[n+1];
	}
	check(inspc(instructions) == 129 && len(declarations) == 129);
	for(n = 0; n < 129; n++){
		check(instructions[n].pc == n);
		free(array.a[n]);
	}
	free(array.a);
	check(optpositions(0x7fffffff-1, 1) == 0x7fffffff);
	check(live == 0);
}


/* Repeatedly construct real graphs and require every private allocation back. */
static void
ownership(void)
{
	Inst instructions[129];
	Block *graph, *b;
	int n, j, blocks;

	for(n = 1; n <= 129; n++){
		memset(instructions, 0, sizeof(instructions));
		for(j = 0; j < n; j++){
			instructions[j].op = j == n-1 ? IRET : IMOVW;
			instructions[j].next = j == n-1 ? nil : &instructions[j+1];
			if(j < n-1)
				instructions[j].branch = &instructions[n-1];
		}
		graph = mkblocks(instructions, &blocks);
		j = 0;
		for(b = graph; b != nil; b = b->next){
			check(b->first != nil && b->last != nil);
			j++;
		}
		check(j == blocks);
		freeblks(graph);
		check(live == 0);
	}
}


/* Select independent parent-replay probes and report actual production checks. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(strcmp(argv[1], "ordinary") == 0)
		ordinary();
	else if(strcmp(argv[1], "extents") == 0)
		extents();
	else if(strcmp(argv[1], "ownership") == 0)
		ownership();
	else
		boundary(argv[1]);
	printf("PASS optimizer storage %s WORD%d: %d checks\n", argv[1], IBY2WD*8, checks);
	return 0;
}
