#include "limbo.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static Node constants[512];
static void *allocation;
static ulong allocated;
static int used, checks;

/* Model compiler arena allocation while retaining exact requested label sizes. */
void*
allocmem(ulong size)
{
	assert(allocation == nil && size <= 512*sizeof(Label));
	allocated = size;
	allocation = malloc(size);
	assert(allocation != nil);
	return allocation;
}

/* Create observable wide integer constants with copied source locations. */
Node*
mkconst(Src *src, Long value)
{
	Node *n;

	assert(used < 512);
	n = &constants[used++];
	memset(n,0,sizeof(*n));
	n->src = *src;
	n->val = value;
	return n;
}

/* Every fixture supplies a valid typed/sorted case, so internal errors are bugs. */
void
fatal(char *message, ...)
{
	fprintf(stderr,"unexpected compiler failure: %s\n",message);
	abort();
}

IMPLEMENTATION

/* Check expanded values, gap defaults and range targets against a linear oracle. */
static void
scenario(Long base, int n, int width, int gap)
{
	Node starts[16] = {{0}}, stops[16] = {{0}}, range = {0};
	Label labels[16] = {{0}}, saved[16];
	Inst targets[16];
	Case c = {0};
	Long finish, value, span;
	int i, j, expected, actual, target;

	c.labs = labels;
	c.nlab = n;
	assert(n <= 16);
	allocation = nil;
	allocated = used = 0;
	for(i = 0; i < n; i++){
		starts[i].val = base+i*(width+gap);
		starts[i].src.start.line = i+101;
		stops[i].val = starts[i].val+width-1;
		labels[i].start = &starts[i];
		labels[i].stop = &stops[i];
		labels[i].inst = &targets[i];
		labels[i].node = &starts[i];
		targets[i].pc = i;
	}
	finish = stops[n-1].val;
	assert(base >= INT32_MIN && finish <= INT32_MAX);
	memcpy(saved,labels,sizeof(labels));
	span = finish-base+1;
	expected = span >= 3 && span <= 3*n;
	actual = dogoto(&c);
	assert(actual == expected);
	if(!actual){
		assert(c.nlab == n && c.labs == labels && used == 0 && allocation == nil);
		assert(memcmp(saved,labels,sizeof(labels)) == 0);
	}else{
		assert(c.nlab == span);
		if(span == n)
			assert(c.labs == labels && used == 0 && allocation == nil);
		else
			assert(c.labs == allocation && allocated == span*sizeof(Label) && used == span);
		for(j = 0; j < c.nlab; j++){
			assert(c.labs[j].start->val == base+j);
			assert(c.labs[j].stop->val == base+j);
			assert(c.labs[j].inst == nil);
			if(span != n)
				assert(c.labs[j].start->src.start.line == c.labs[j].node->src.start.line);
		}
		for(i = 0; i < n; i++){
			range.left = &starts[i];
			range.right = &stops[i];
			fillrange(&c,&range,&targets[i]);
		}
		for(value = base-1; value <= finish+1; value++){
			target = -1;
			for(i = 0; i < n; i++)
				if(value >= starts[i].val && value <= stops[i].val)
					target = i;
			if(value < base || value > finish)
				actual = -1;
			else{
				Inst *in = c.labs[value-base].inst;
				actual = in == nil ? -1 : in->pc;
			}
			assert(actual == target);
		}
	}
	free(allocation);
	allocation = nil;
	checks++;
}

/* A maximum singleton assignment must terminate without incrementing an int. */
static void
maximumfill(void)
{
	Node starts[3] = {{0}}, range = {0};
	Label labels[3] = {{0}};
	Inst target = {0};
	Case c = {0};
	int i;

	c.labs = labels;
	c.nlab = 3;
	target.pc = 17;
	for(i = 0; i < 3; i++){
		starts[i].val = (Long)INT32_MAX-2+i;
		labels[i].start = labels[i].stop = &starts[i];
	}
	range.left = range.right = &starts[2];
	fillrange(&c,&range,&target);
	assert(labels[0].inst == nil && labels[1].inst == nil && labels[2].inst == &target);
	checks++;
}

/* Reject sparse full-width int endpoints without narrowing the span into int. */
static void
sparse(void)
{
	Node nodes[2] = {{0}};
	Label labels[2] = {{0}}, saved[2];
	Case c = {0}, empty = {0};
	int i;

	c.labs = labels;
	c.nlab = 2;
	nodes[0].val = INT32_MIN;
	nodes[1].val = INT32_MAX;
	for(i = 0; i < 2; i++)
		labels[i].start = labels[i].stop = &nodes[i];
	memcpy(saved,labels,sizeof(labels));
	assert(dogoto(&c) == 0 && c.labs == labels && c.nlab == 2);
	assert(memcmp(saved,labels,sizeof(labels)) == 0 && dogoto(&empty) == 0);
	checks += 2;
}

/* Explore dense/sparse ranges at both signed-int extremes and ordinary values. */
int
main(int argc, char **argv)
{
	Long bases[] = {INT32_MIN,(Long)INT32_MIN+256,-1024,-129,-1,0,127,1024,(Long)INT32_MAX-256};
	int b, n, width, gap;

	if(argc == 1 || strcmp(argv[1],"minimum") == 0)
		scenario(INT32_MIN,1,3,0);
	if(argc == 1 || strcmp(argv[1],"maximum") == 0)
		scenario((Long)INT32_MAX-2,1,3,0);
	if(argc == 1 || strcmp(argv[1],"fill") == 0)
		maximumfill();
	if(argc == 1 || strcmp(argv[1],"sparse") == 0)
		sparse();
	if(argc == 1 || strcmp(argv[1],"grid") == 0)
		for(b = 0; b < (int)(sizeof(bases)/sizeof(*bases)); b++)
		for(n = 1; n <= 16; n++)
		for(width = 1; width <= 6; width++)
		for(gap = 0; gap <= 8; gap++)
			scenario(bases[b],n,width,gap);
	printf("dense case ranges: %d scenarios passed\n",checks);
	return 0;
}
