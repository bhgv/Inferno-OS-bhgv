#include "dat.h"
#include "fns.h"
#include "interp.h"

#include <stddef.h>
#include <stdio.h>

#ifndef MAP_OPERATION
#define MAP_OPERATION 0 // Run all pointer-map operation families by default.
#endif

typedef struct Child Child;

struct Child {
	Heap heap; // Naturally aligned child owner header.
	uchar data[8]; // Exact native pointer payload identity.
};
static Child children[3];
static int released[24], visited[24], nreleased, nvisited;
int mutator = 1, nprop; // Real mark-barrier state observed by the oracle.


/* Preserve every ownership and byte-image check under NDEBUG. */
static void
mapcheck(int condition)
{
	if(!condition)
		abort();
}


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
	mapcheck(0);
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
	mapcheck(nreleased < 24 && h->ref > 1000);
	released[nreleased++] = i;
	h->ref--;
}


/* The scanner receives heap headers, in the original ascending field order. */
static void
visit(void *value)
{
	mapcheck(nvisited < 24);
	visited[nvisited++] = childindex(value);
}

IMPLEMENTATION

/* Compare all bytes and reference/mark effects for each map and alignment. */
static unsigned long
matrix(void)
{
	struct {
		Type type; // Aligned descriptor image used by the independent oracle.
		uchar extra[8]; // Additional map bytes copied into the packed descriptor.
	} descriptor;
	union {
		uvlong align; // Naturally aligned origin before each independent byte shift.
		uchar bytes[512]; // Complete payload image and surrounding canaries.
	} record;
	uchar before[512], expected[512], *map, *base;
	uchar packed[sizeof(descriptor)+8], packedbefore[sizeof(packed)];
	Type *active;
	void *value;
	int owners[24], counts[3], descriptoroffset, offset, groups, mask, phase, i, slot, id, n;
	unsigned long checks;

	checks = 0;
	map = (uchar*)&descriptor+offsetof(Type, map);
	for(descriptoroffset = 0; descriptoroffset < 8; descriptoroffset++)
	for(offset = 0; offset < 8; offset++)
	for(groups = 1; groups <= 3; groups += 2)
	for(mask = 0; mask < 256; mask++)
	for(phase = 0; phase < 3; phase++){
		memset(&descriptor, 0, sizeof(descriptor));
		descriptor.type.np = groups;
		descriptor.type.size = groups*8*sizeof(void*);
		for(i = 0; i < groups; i++)map[i] = i == 0 ? mask : i == 1 ? mask^0xa5 : (mask<<1 | mask>>7)&255;
		memset(packed,0xa5,sizeof(packed));
		memmove(packed+descriptoroffset,&descriptor,sizeof(descriptor));
		memmove(packedbefore,packed,sizeof(packed));
		active=(Type*)(packed+descriptoroffset);
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
			incmem(base, active);
			for(i = 0; i < 3; i++){
				mapcheck(children[i].heap.ref == 1000+counts[i]);
				mapcheck(children[i].heap.color == (counts[i] ? propagator : 0));
			}
			mapcheck(nprop == !!(counts[0]+counts[1]+counts[2]));
			mapcheck(!memcmp(before, record.bytes, sizeof(before)));
			checks++;
		}
		if(MAP_OPERATION == 0 || MAP_OPERATION == 2){
			for(i = 0; i < 3; i++)children[i].heap.ref = 1000+counts[i];
			nreleased = 0;
			freeptrs(base, active);
			n = 0;
			for(i = 0; i < groups; i++)
			for(slot = i*8+7; slot >= i*8; slot--)
				if(owners[slot] >= 0)mapcheck(released[n++] == owners[slot]);
			mapcheck(nreleased == n);
			for(i = 0; i < 3; i++)mapcheck(children[i].heap.ref == 1000);
			mapcheck(!memcmp(before, record.bytes, sizeof(before)));
			checks++;
		}
		if(MAP_OPERATION == 0 || MAP_OPERATION == 3){
			nvisited = 0;
			scanptrs(base, active, visit);
			n = 0;
			for(slot = 0; slot < groups*8; slot++)
				if(owners[slot] >= 0)mapcheck(visited[n++] == owners[slot]);
			mapcheck(nvisited == n && !memcmp(before, record.bytes, sizeof(before)));
			checks++;
		}
		if(MAP_OPERATION == 0 || MAP_OPERATION == 4){
			memmove(expected, before, sizeof(expected));
			value = H;
			for(slot = 0; slot < groups*8; slot++)
				if(map[slot/8] & (0x80>>(slot%8)))
					memmove(expected+32+offset+slot*sizeof(value), &value, sizeof(value));
			initmem(active, base);
			mapcheck(!memcmp(expected, record.bytes, sizeof(expected)));
			checks++;
		}
		mapcheck(!memcmp(packed,packedbefore,sizeof(packed)));
	}
	/* No pointer field means no data address needs to exist. */
	memset(&descriptor, 0, sizeof(descriptor));
	for(descriptoroffset = 0; descriptoroffset < 8; descriptoroffset++)
	for(groups = 0; groups <= 3; groups += 3){
		descriptor.type.np = groups;
		memmove(packed+descriptoroffset,&descriptor,sizeof(descriptor));
		active=(Type*)(packed+descriptoroffset);
		for(i = 0; i < 3; i++){
			value = i == 0 ? nil : i == 1 ? H : (void*)1;
			if(MAP_OPERATION == 0 || MAP_OPERATION == 1){incmem(value, active);checks++;}
			if(MAP_OPERATION == 0 || MAP_OPERATION == 2){freeptrs(value, active);checks++;}
			if(MAP_OPERATION == 0 || MAP_OPERATION == 3){scanptrs(value, active, visit);checks++;}
			if(MAP_OPERATION == 0 || MAP_OPERATION == 4){initmem(active, value);checks++;}
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
