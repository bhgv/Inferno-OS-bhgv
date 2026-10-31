#include "native64-tail.h"

static int drop; // Explicit root-removal negative control only.
static int checkedreferences; // Pure trusted Type validation observations.

/* Validate exact live Type identity after descriptor/canonical preflight, without allocation. */
static int
tailreference(void *context, void *value, Type *type)
{
	check(context == &checkedreferences);
	checkedreferences++;
	return D2H(value)->t == type;
}


/* Derive a wide dynamic map and trace both fixed and tail arguments with the owning collector. */
static void
tailgraph(int phase, int state)
{
	uchar fixedmap[] = {0, 0x80};
	LimboNative64Entry fixed = {"fixed", nil, 0, 72, 2, fixedmap};
	Native64TailSlot slots[150];
	Native64TailPlan plan;
	uvlong values[150];
	uchar map[20], *frame, *ownedmap;
	Type *type;
	Array *prefix, *tail, *nested;
	void *empty;
	int i;

	startcase(phase, state);
	for (i = 0; i < 150; i++) {
		slots[i].kind = Native64TailBig;
		slots[i].offset = i * 8;
		slots[i].bytes = 8;
		slots[i].type = nil;
		values[i] = ~(uvlong)0;
	}
	slots[149].kind = Native64TailRef;
	slots[149].type = &Tarray;
	check(native64tailplan(&fixed, slots, 150, &plan));
	check(plan.size == 1272 && plan.map_bytes == sizeof(map));
	check(native64tailmap(&fixed, slots, 150, map, sizeof(map)));
	allocationfailure = 1;
	check(dtype(freeheap, plan.size, map, sizeof(map)) == nil);
	allocationfailure = 0;
	type = dtype(freeheap, plan.size, map, sizeof(map));
	check(type != nil && type->np == 20);
	ownedmap = (uchar *)type + offsetof(Type, map);
	map[19] = 0;
	check(ownedmap[19] == 2);
	frame = stackframes(type, 1);
	empty = H;
	check(memcmp(frame + 64, &empty, sizeof(empty)) == 0);
	check(memcmp(frame + plan.tail + 149 * 8, &empty, sizeof(empty)) == 0);
	/* Existing caller owners remain actual external GC roots throughout value checking/copying. */
	prefix = H2D(Array *, heaparray(&Tbyte, 1));
	prefix->data[0] = 0x31;
	ptradd(D2H(prefix));
	tail = H2D(Array *, heaparray(&Tbyte, 1));
	tail->data[0] = 0x7b;
	ptradd(D2H(tail));
	values[149] = (uvlong)(uintptr)tail;
	check(native64tailvalues(&fixed, slots, 150, values, sizeof(values), tailreference,
							 &checkedreferences));
	memmove(frame + 64, &prefix, sizeof(prefix));
	memmove(frame + plan.tail, values, sizeof(values));
	incmem(frame, type);
	check(D2H(prefix)->ref == 2 && D2H(tail)->ref == 2);
	allocationcollections = 1;
	nested = H2D(Array *, heaparray(&Tbyte, 0));
	allocationcollections = 0;
	destroy(nested);
	check(!freed[2] && !freed[3]);
	ptrdel(D2H(prefix));
	ptrdel(D2H(tail));
	destroy(prefix);
	destroy(tail);
	check(D2H(prefix)->ref == 1 && D2H(tail)->ref == 1);
	if (drop == 1)
		ownedmap[1] = 0;
	if (drop == 2)
		ownedmap[19] = 0;
	epochs(6);
	check(!freed[2] && !freed[3]);
	check(prefix->data[0] == 0x31 && tail->data[0] == 0x7b);
	freeptrs(frame, type);
	initmem(type, frame);
	check(freed[2] == 1 && freed[3] == 1 && Tarray.ref == 1 && Tbyte.ref == 1);
	check(memcmp(frame + 64, &empty, sizeof(empty)) == 0);
	check(memcmp(frame + plan.tail + 149 * 8, &empty, sizeof(empty)) == 0);
	process.R.EX = process.R.SP = nil;
	freetype(type);
	graphs++;
}


/* Exercise every process state/collector phase and require actual root-removal controls to fail. */
int
main(int argc, char **argv)
{
	int phase, state;

	check(argc == 1 || argc == 2);
	if (argc == 2) {
		if (strcmp(argv[1], "fixed") == 0)
			drop = 1;
		else if (strcmp(argv[1], "tail") == 0)
			drop = 2;
		else
			check(0);
	}
	for (phase = 0; phase < 3; phase++)
		for (state = Palt; state <= Pbroken; state++)
			tailgraph(phase, state);
	check(graphs == 24 && checkedreferences == 24);
	printf("PASS typed-tail GC: %d predicates, 24 mixed graphs, actual dtype/heap/roots/collector "
		   "and nested allocations\n",
		   checks);
	return 0;
}
