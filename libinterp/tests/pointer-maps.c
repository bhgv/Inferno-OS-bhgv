#include "dat.h"
#include "fns.h"
#include "interp.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#ifndef MAP_OPERATION
#define MAP_OPERATION 0
#endif

typedef struct Child Child;
struct Child {
	Heap heap;
	uchar data[8];
};
static Child children[3];
static int released[24], visited[24], nreleased, nvisited;
int mutator = 1, nprop;

/* Process inspection is unavailable; address and stack instrumentation remain enabled. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Resolve callbacks to one of the three exact, naturally aligned heap objects. */
static int
childindex(Heap *h)
{
	int i;

	for(i = 0; i < 3; i++)
		if(h == &children[i].heap)
			return i;
	assert(0);
	return -1;
}

/* Record destructor order and consume exactly one retained owner. */
void
destroy(void *value)
{
	Heap *h;
	int i;

	h = D2H(value);
	i = childindex(h);
	assert(nreleased < 24 && h->ref > 1000);
	released[nreleased++] = i;
	h->ref--;
}

/* The scanner receives heap headers, in the original ascending field order. */
static void
visit(void *value)
{
	assert(nvisited < 24);
	visited[nvisited++] = childindex(value);
}

IMPLEMENTATION

/* Compare all bytes and reference/mark effects for each map and alignment. */
static unsigned long
matrix(void)
{
	struct { Type type; uchar extra[8]; } descriptor;
	union { uvlong align; uchar bytes[512]; } record;
	uchar before[512], expected[512], *map, *base;
	void *value;
	int owners[24], counts[3], offset, groups, mask, phase, i, slot, id, n;
	unsigned long checks;

	checks = 0;
	map = (uchar*)&descriptor+offsetof(Type, map);
	for(offset = 0; offset < 8; offset++)
	for(groups = 1; groups <= 3; groups += 2)
	for(mask = 0; mask < 256; mask++)
	for(phase = 0; phase < 3; phase++){
		memset(&descriptor, 0, sizeof(descriptor));
		descriptor.type.np = groups;
		descriptor.type.size = groups*8*sizeof(void*);
		for(i = 0; i < groups; i++)map[i] = i == 0 ? mask : i == 1 ? mask^0xa5 : (mask<<1 | mask>>7)&255;
		for(i = 0; i < 512; i++)record.bytes[i] = i*29+17;
		base = record.bytes+32+offset;
		memset(counts, 0, sizeof(counts));
		for(slot = 0; slot < groups*8; slot++){
			owners[slot] = -1;
			if(!(map[slot/8] & (0x80>>(slot%8))))continue;
			id = slot%3;
			value = id == phase ? H : H2D(void*, &children[id].heap);
			if(value != H){owners[slot] = id;counts[id]++;}
			memmove(base+slot*sizeof(value), &value, sizeof(value));
		}
		memmove(before, record.bytes, sizeof(before));
		for(i = 0; i < 3; i++){children[i].heap.ref = 1000;children[i].heap.color = 0;}
		nprop = 0;
		if(MAP_OPERATION == 0 || MAP_OPERATION == 1){
			incmem(base, &descriptor.type);
			for(i = 0; i < 3; i++){
				assert(children[i].heap.ref == 1000+counts[i]);
				assert(children[i].heap.color == (counts[i] ? propagator : 0));
			}
			assert(nprop == !!(counts[0]+counts[1]+counts[2]));
			assert(!memcmp(before, record.bytes, sizeof(before)));
			checks++;
		}
		if(MAP_OPERATION == 0 || MAP_OPERATION == 2){
			for(i = 0; i < 3; i++)children[i].heap.ref = 1000+counts[i];
			nreleased = 0;
			freeptrs(base, &descriptor.type);
			n = 0;
			for(i = 0; i < groups; i++)
			for(slot = i*8+7; slot >= i*8; slot--)
				if(owners[slot] >= 0)assert(released[n++] == owners[slot]);
			assert(nreleased == n);
			for(i = 0; i < 3; i++)assert(children[i].heap.ref == 1000);
			assert(!memcmp(before, record.bytes, sizeof(before)));
			checks++;
		}
		if(MAP_OPERATION == 0 || MAP_OPERATION == 3){
			nvisited = 0;
			scanptrs(base, &descriptor.type, visit);
			n = 0;
			for(slot = 0; slot < groups*8; slot++)
				if(owners[slot] >= 0)assert(visited[n++] == owners[slot]);
			assert(nvisited == n && !memcmp(before, record.bytes, sizeof(before)));
			checks++;
		}
		if(MAP_OPERATION == 0 || MAP_OPERATION == 4){
			memmove(expected, before, sizeof(expected));
			value = H;
			for(slot = 0; slot < groups*8; slot++)
				if(map[slot/8] & (0x80>>(slot%8)))
					memmove(expected+32+offset+slot*sizeof(value), &value, sizeof(value));
			initmem(&descriptor.type, base);
			assert(!memcmp(expected, record.bytes, sizeof(expected)));
			checks++;
		}
	}
	/* No pointer field means no data address needs to exist. */
	memset(&descriptor, 0, sizeof(descriptor));
	for(groups = 0; groups <= 3; groups += 3){
		descriptor.type.np = groups;
		for(i = 0; i < 3; i++){
			value = i == 0 ? nil : i == 1 ? H : (void*)1;
			if(MAP_OPERATION == 0 || MAP_OPERATION == 1){incmem(value, &descriptor.type);checks++;}
			if(MAP_OPERATION == 0 || MAP_OPERATION == 2){freeptrs(value, &descriptor.type);checks++;}
			if(MAP_OPERATION == 0 || MAP_OPERATION == 3){scanptrs(value, &descriptor.type, visit);checks++;}
			if(MAP_OPERATION == 0 || MAP_OPERATION == 4){initmem(&descriptor.type, value);checks++;}
		}
	}
	return checks;
}

/* Run the same native pointer maps with narrow and wide VM scalar models. */
int
main(void)
{
	printf("PASS pointer maps: %lu byte/reference/order checks, WORD%zu\n", matrix(), sizeof(WORD)*8);
	return 0;
}
