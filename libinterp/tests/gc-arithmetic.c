#include "dat.h"
#include "fns.h"
#include "interp.h"
#include <stdio.h>
#include <limits.h>

#define check(test) predicate(!!(test), __LINE__) // Retain budget, color, callback and owner checks under NDEBUG.

enum {
	Quanta = 50, // Minimum collector work quantum.
	MaxQuanta = 750, // Maximum collector work quantum.
};

static int visit, gce, gct, quanta, markdepth, marker, sweeper; // Actual collector arithmetic state.
int mutator = 1, nprop, gccolor = 3; // Public collector phase and barrier state.
Module *modules; // No external module roots are needed for phase validation.
static ulong checks; // Completed state/image scenarios.
static int calls, order[16], exhaust; // Canonical callback trace and injected nested work exhaustion.
static Type child; // Canonical child callback descriptor.

/* Native owners keep callback data naturally aligned and refcounts observable. */
typedef struct BudgetOwner BudgetOwner;

struct BudgetOwner {
	Heap heap; // Borrowed owner header; marking never changes references.
	uchar data[sizeof(void*)]; // Child callback identity and optional pointer payload.
};

/* List payload storage remains complete even in the WORD32/native-pointer model. */
typedef struct BudgetList BudgetList;

struct BudgetList {
	Heap heap; // Native prefix used by the actual tail marker.
	List list; // Canonical list header and first payload bytes.
	uchar extra[sizeof(void*)]; // Complete native payload width independent of WORD.
};

static BudgetOwner owners[3]; // Distinct repeated owners with all three initial color roles.
static BudgetList lists[3]; // Root, visited link and deferred link.

/* Python constructs these expected results using arbitrary-precision arithmetic. */
static struct {
	int seed; // Initial signed work budget.
	uvlong count; // Native-width work charge.
	int expected; // Independently clamped mathematical result.
} spendvectors[] = {
SPEND_VECTORS
};

static struct {
	int seed; // Initial bounded estimate.
	int delta; // Signed estimate adjustment.
	int expected; // Independently clamped sum.
} addvectors[] = {
ADD_VECTORS
};

static struct {
	int balance; // Net estimate supplied to the feedback controller.
	int scanned; // Inspected-block count, including defensive invalid states.
	int expected; // Independently calculated and bounded next quantum.
} quantavectors[] = {
QUANTA_VECTORS
};


/* Report the first accounting, callback or immutable-owner discrepancy. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "GC arithmetic failure at %d\n", line);
		abort();
	}
}


/* Retain access sanitizers without unavailable LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* The actual rootset still performs its external-root hook on an empty root graph. */
static void
ptrmark(void)
{
	calls++;
}

IMPLEMENTATION


/* Record exact native child order; nested marking may exhaust the remaining int budget. */
static void
observe(Type *type, void *data)
{
	int i;

	check(type == &child && calls < nelem(order));
	for(i = 0; i < nelem(owners) && owners[i].data != data; i++)
		;
	check(i < nelem(owners));
	order[calls++] = i;
	if(exhaust)
		visit = INT_MIN;
}


/* Enumerate independent host arithmetic vectors, including full unsigned work counts. */
static void
integers(void)
{
	int i;

	for(i = 0; i < nelem(spendvectors); i++){
		visit = spendvectors[i].seed;
		gcspend(spendvectors[i].count);
		check(visit == spendvectors[i].expected);
		checks++;
	}
	for(i = 0; i < nelem(addvectors); i++){
		check(gcintadd(addvectors[i].seed, addvectors[i].delta) == addvectors[i].expected);
		checks++;
	}
}


/* Verify feedback division/truncation and bounds before int-result publication. */
static void
feedback(void)
{
	int i;

	for(i = 0; i < nelem(quantavectors); i++){
		check(gcnextquanta(quantavectors[i].balance, quantavectors[i].scanned) == quantavectors[i].expected);
		checks++;
	}
}


/* Check actual root color assignment across the signed selector rollover. */
static void
colors(void)
{
	int seeds[] = {2, 3, 4, 5, INT_MAX-3, INT_MAX-2, INT_MAX-1, INT_MAX};
	int i, j, old;

	for(i = 0; i < nelem(seeds); i++){
		gccolor = seeds[i];
		for(j = 0; j < 8; j++){
			old = gccolor;
			gccolor = gcnextcolor(gccolor);
			calls = 0;
			rootset(nil);
			check(gccolor >= 2 && mutator == (old%3+1)%3);
			check(marker == (mutator+2)%3 && sweeper == (mutator+1)%3 && calls == 1);
			checks++;
		}
	}
}


/* Model work/estimate clamping in a wider domain, independent of the helpers. */
static int
bounded(vlong value)
{
	if(value < INT_MIN)
		return INT_MIN;
	if(value > INT_MAX)
		return INT_MAX;
	return value;
}


/* Preserve packed payload/descriptor images and the exact ordinary collector callback order. */
static void
heaps(void)
{
	struct {
		Type type; // Complete descriptor copied into shifted native views.
		uchar extra[2]; // Two map bytes with room for canonical structure padding.
	} descriptor;
	uchar packed[sizeof(descriptor)+8], record[160], before[160];
	int budgets[] = {INT_MIN, INT_MIN+1, -2, -1, 0, 1, 2, 50, INT_MAX-1, INT_MAX};
	int balances[] = {INT_MIN, 17, INT_MAX};
	int depths[] = {0, 62, 63};
	int ds, shift, mask, budget, balance, depth, i, id, colors[3], expectedvisit, expectedbalance;
	int expectedcalls, wanted[16], propagated;
	void *value;

	memset(&descriptor, 0, sizeof(descriptor));
	descriptor.type.np = 2;
	descriptor.type.size = 16*sizeof(void*);
	child.mark = observe;
	mutator = 1;
	exhaust = 0;
	for(ds = 0; ds < 8; ds++)
	for(shift = 0; shift < 8; shift++)
	for(mask = 0; mask < 16; mask++)
	for(budget = 0; budget < nelem(budgets); budget++)
	for(balance = 0; balance < nelem(balances); balance++)
	for(depth = 0; depth < nelem(depths); depth++){
		descriptor.type.map[0] = mask*17;
		*((uchar*)&descriptor.type+O(Type, map)+1) = ~(mask*17);
		memmove(packed+ds, &descriptor, sizeof(descriptor));
		memset(record, 0xa5, sizeof(record));
		for(i = 0; i < 3; i++){
			owners[i].heap.ref = 100+i;
			owners[i].heap.t = &child;
			owners[i].heap.color = colors[i] = i == 0 ? 0 : i == 1 ? propagator : mutator;
		}
		expectedvisit = visit = budgets[budget];
		expectedbalance = gce = balances[balance];
		markdepth = depths[depth];
		expectedcalls = calls = nprop = propagated = 0;
		for(i = 0; i < 16; i++){
			id = (i+mask)%4;
			value = id == 3 ? H : owners[id].data;
			memmove(record+8+shift+i*sizeof(value), &value, sizeof(value));
			if(!(*((uchar*)&descriptor.type+O(Type, map)+i/8) & (0x80>>(i%8))) || id == 3)
				continue;
			if(colors[id] != mutator){
				colors[id] = propagator;
				propagated = 1;
			}
			if(colors[id] == propagator){
				expectedvisit = bounded((vlong)expectedvisit-1);
				if(expectedvisit >= 0 && depths[depth]+1 < 64){
					expectedbalance = bounded((vlong)expectedbalance-1);
					colors[id] = mutator;
					wanted[expectedcalls++] = id;
				}
			}
		}
		memmove(before, record, sizeof(record));
		markheap((Type*)(packed+ds), record+8+shift);
		check(visit == expectedvisit && gce == expectedbalance && calls == expectedcalls);
		check(markdepth == depths[depth] && nprop == propagated && !memcmp(record, before, sizeof(record)));
		for(i = 0; i < calls; i++)
			check(order[i] == wanted[i]);
		for(i = 0; i < 3; i++)
			check(owners[i].heap.color == colors[i] && owners[i].heap.ref == 100+i);
		checks++;
	}
}


/* A valid array of H fields still charges its length after an exhausted mark budget. */
static void
arrays(void)
{
	Array array;
	Type type;
	void *data[8];
	int budgets[] = {INT_MIN, INT_MIN+1, -1, 0, 1, INT_MAX};
	int i, n, b, plain;

	memset(&array, 0, sizeof(array));
	memset(&type, 0, sizeof(type));
	type.size = sizeof(void*);
	type.map[0] = 0x80;
	array.t = &type;
	array.root = H;
	array.data = (uchar*)data;
	for(i = 0; i < nelem(data); i++)
		data[i] = H;
	for(plain = 0; plain < 2; plain++)
	for(n = 0; n <= nelem(data); n++)
	for(b = 0; b < nelem(budgets); b++){
		type.np = !plain;
		array.len = n;
		visit = budgets[b];
		gce = 17;
		markdepth = calls = nprop = 0;
		markarray(nil, &array);
		check(visit == (plain ? budgets[b] : bounded((vlong)budgets[b]-n)));
		check(gce == 17 && markdepth == 0 && calls == 0 && nprop == 0);
		checks++;
	}
}


/* A nested child callback can exhaust work before marklist charges the visited link. */
static void
linked(void)
{
	Type payload;
	void *value;
	int i;

	memset(lists, 0, sizeof(lists));
	memset(&payload, 0, sizeof(payload));
	payload.np = 1;
	payload.map[0] = 0x80;
	payload.size = sizeof(value);
	check(O(BudgetList, list) == sizeof(Heap));
	for(i = 0; i < 3; i++){
		lists[i].heap.ref = 11+i;
		lists[i].list.tail = i < 2 ? &lists[i+1].list : H;
	}
	lists[1].list.t = &payload;
	value = owners[0].data;
	memmove(lists[1].list.data, &value, sizeof(value));
	owners[0].heap.t = &child;
	owners[0].heap.color = 0;
	owners[0].heap.ref = 7;
	child.mark = observe;
	mutator = 1;
	gce = 5;
	visit = 1;
	markdepth = nprop = calls = 0;
	exhaust = 1;
	marklist(nil, &lists[0].list);
	check(visit == INT_MIN && gce == 4 && calls == 1 && order[0] == 0 && markdepth == 0);
	check(lists[1].heap.color == propagator && lists[2].heap.color == propagator && nprop == 1);
	for(i = 0; i < 3; i++)
		check(lists[i].heap.ref == 11+i);
	check(owners[0].heap.ref == 7);
	checks++;
}


/* Saturated balance accounting must not reverse its sign during an ordinary mark. */
static void
balances(void)
{
	Type payload;
	void *value;

	memset(&payload, 0, sizeof(payload));
	payload.np = 1;
	payload.map[0] = 0x80;
	value = owners[0].data;
	owners[0].heap.t = &child;
	owners[0].heap.color = 0;
	child.mark = observe;
	mutator = 1;
	gce = INT_MIN;
	visit = 10;
	calls = markdepth = nprop = exhaust = 0;
	markheap(&payload, &value);
	check(gce == INT_MIN && visit == 9 && calls == 1 && markdepth == 0);
	checks++;
}


/* Keep every old-source failure attributable to one arithmetic or callback family. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(!strcmp(argv[1], "integers"))
		integers();
	else if(!strcmp(argv[1], "quanta"))
		feedback();
	else if(!strcmp(argv[1], "color"))
		colors();
	else if(!strcmp(argv[1], "heap"))
		heaps();
	else if(!strcmp(argv[1], "array"))
		arrays();
	else if(!strcmp(argv[1], "list"))
		linked();
	else if(!strcmp(argv[1], "balance"))
		balances();
	else
		check(0);
	check(checks != 0);
	printf("PASS GC arithmetic WORD%zu %s: %lu accounting/color/callback scenarios\n", sizeof(WORD)*8, argv[1], checks);
	return 0;
}
