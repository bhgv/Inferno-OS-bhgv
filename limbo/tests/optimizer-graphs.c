#include "limbo.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

static unsigned long checks; // Independent graph and aggregate-count scenarios.
static int live; // Compiler work-stack allocations still owned by the walk.
static volatile uvlong observed; // Force evaluation before comparing a wide aggregate result.


/* Keep every graph, postorder and allocation predicate active under NDEBUG. */
static void
graphcheck(int condition)
{
	if(!condition)
		abort();
}


/* The fixture frees every allocation without process-based leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Observe only production walk allocations, not the independent graph storage. */
void*
allocmem(ulong size)
{
	void *value;

	value = malloc(size);
	graphcheck(value != nil);
	live++;
	return value;
}


/* Release each private work buffer exactly once. */
static void
walkfree(void *value)
{
	graphcheck(value != nil && live > 0);
	live--;
	free(value);
}


/* No valid fixture graph should reach a compiler allocation-limit failure. */
void
fatal(char *format, ...)
{
	(void)format;
	abort();
}

#define free walkfree // Account for production work-stack ownership only.
IMPLEMENTATION
#undef free


/* A recursive oracle is safe for the exhaustive graphs of at most four nodes. */
static void
reference(int root, int count, uint mask, int backwards, int *number, int *flags, int *order)
{
	int child, bit;

	flags[root] = 1;
	for(child = 0; child < count; child++){
		bit = backwards ? child*count+root : root*count+child;
		if((mask & (1U<<bit)) && flags[child] == 0)
			reference(child, count, mask, backwards, number, flags, order);
	}
	if(!backwards)
		order[root] = --(*number);
}


/* Exhaust adjacency matrices, both edge directions, roots and marked barriers. */
static void
smallgraphs(void)
{
	Block nodes[4];
	Blist edges[16], *head;
	int flags[4], order[4];
	int count, backwards, root, preset, i, j, used, bit, number, expected;
	uint mask;

	for(count = 1; count <= 4; count++)for(mask = 0; mask < (1U<<(count*count)); mask++)
	for(backwards = 0; backwards < 2; backwards++){
		memset(nodes, 0, sizeof(nodes));
		used = 0;
		for(i = 0; i < count; i++){
			head = nil;
			for(j = count-1; j >= 0; j--){
				bit = backwards ? j*count+i : i*count+j;
				if(!(mask & (1U<<bit)))
					continue;
				edges[used].block = &nodes[j]; edges[used].next = head;
				head = &edges[used++];
			}
			nodes[i].pred = nodes[i].succ = head;
		}
		for(root = 0; root < count; root++)for(preset = 0; preset < 3; preset++){
			for(i = 0; i < count; i++){
				flags[i] = preset == 0 ? 0 : preset == 1 ? ((mask>>i)&1)*2 : (i == root ? 2 : 0);
				nodes[i].flags = flags[i]; nodes[i].dfn = order[i] = -91;
			}
			number = expected = count;
			reference(root, count, mask, backwards, &expected, flags, order);
			if(backwards)loop0(&nodes[root]);else dfs0(&nodes[root], &number);
			for(i = 0; i < count; i++)
				graphcheck(nodes[i].flags == flags[i] && nodes[i].dfn == order[i]);
			graphcheck(number == expected && live == 0); checks++;
		}
	}
}


/* A long cyclic chain must use heap growth while respecting a small C stack. */
static void
longgraph(int backwards)
{
	Block *nodes;
	Blist *edges;
	struct rlimit limit;
	int count = 30000, i, number;

	limit.rlim_cur = limit.rlim_max = 256*1024;
	graphcheck(setrlimit(RLIMIT_STACK, &limit) == 0);
	nodes = calloc(count, sizeof(*nodes)); edges = calloc(2*count, sizeof(*edges));
	graphcheck(nodes != nil && edges != nil);
	for(i = 0; i < count; i++){
		nodes[i].dfn = -91;
		edges[2*i].block = &nodes[(i+1)%count];
		edges[2*i].next = &edges[2*i+1];
		edges[2*i+1].block = &nodes[0];
		nodes[i].pred = nodes[i].succ = &edges[2*i];
	}
	number = count;
	if(backwards)loop0(nodes);else dfs0(nodes, &number);
	for(i = 0; i < count; i++)
		graphcheck(nodes[i].flags == 1 && nodes[i].dfn == (backwards ? -91 : i));
	graphcheck(number == (backwards ? count : 0) && live == 0);
	free(edges); free(nodes); checks++;
}


/* Count more than UINT32_MAX live bits using shared read-only row storage. */
static void
largecounts(void)
{
	Bits rows[4097];
	u32int *cells;
	uvlong expected;
	int i, count = 32768;

	cells = malloc(count*sizeof(*cells)); graphcheck(cells != nil);
	memset(cells, 0xff, count*sizeof(*cells));
	for(i = 0; i < 4097; i++){rows[i].n = count; rows[i].b = cells;}
	expected = (uvlong)4097*count*32;
	graphcheck(bitcnt(rows[0]) == (uvlong)count*32);
	observed = bitcount(rows, 4097);
	graphcheck(observed == expected && expected > 0xffffffffULL);
	free(cells); checks++;
}


/* Run independent graph/count families in separate sanitizer processes. */
int
main(void)
{
	char *family = getenv("OPTIM_GRAPH_CASE");

	if(!strcmp(family,"small"))smallgraphs();
	else if(!strcmp(family,"dfs"))longgraph(0);
	else if(!strcmp(family,"loop"))longgraph(1);
	else if(!strcmp(family,"count"))largecounts();
	else abort();
	printf("PASS optimizer graphs %s: %lu cases\n", family, checks);
	return 0;
}
