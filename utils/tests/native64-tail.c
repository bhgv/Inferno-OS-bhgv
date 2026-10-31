#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "native64-tail.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef NATIVE64_NO_LSAN


/* Optional host limitation: this metadata-only test owns no allocations requiring process leak
 * scans. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
#endif

static Type wrong; // A different trusted identity for the rejection case.
static Type expected; // Trusted reference identity; no heap lookup.
static int references; // Observe reference checks after complete scalar/descriptor preflight.
static int checks; // Keep all predicates active under NDEBUG.

/* Preserve an independent failure oracle and report the failing stage. */
static void
check(int condition)
{
	if (!condition)
		abort();
	checks++;
}


/* Observe the trusted reference boundary without reading the deliberately opaque test pointer. */
static int
reference(void *context, void *value, Type *type)
{
	check(context == &references && value == (void *)(uintptr)0x1230);
	references++;
	return type == &expected;
}


/* Failures must not publish a plan/map or touch values and reference services. */
static void
refused(LimboNative64Entry *fixed, Native64TailSlot *slots, unsigned int count)
{
	Native64TailPlan plan;
	uchar map[32], image[32];
	int before;

	memset(&plan, 0xa5, sizeof(plan));
	memset(map, 0x5a, sizeof(map));
	memmove(image, map, sizeof(map));
	before = references;
	check(!native64tailplan(fixed, slots, count, &plan));
	check(plan.tail == 0xa5a5a5a5U && plan.size == plan.tail && plan.map_bytes == plan.tail &&
		  plan.count == plan.tail);
	check(!native64tailmap(fixed, slots, count, map, sizeof(map)));
	check(memcmp(map, image, sizeof(map)) == 0);
	check(
		!native64tailvalues(fixed, slots, count, (uvlong *)(uintptr)1, 0, reference, &references));
	check(references == before);
}


/* Validate exact signed extension, bit-preserving reals, limits and late malformed descriptors. */
int
main(void)
{
	uchar prefixmap[] = {0, 0x80};
	LimboNative64Entry fixed = {"fixed", nil, 0, 72, 2, prefixmap};
	LimboNative64Entry changed;
	Native64TailSlot slots[150], changedslot, first;
	Native64TailPlan plan;
	uvlong values[150], original;
	uchar map[24], largemap[Native64BindFrame / 64];
	unsigned int i;
	int before;

#ifdef NATIVE64_REFUSED
	refused((LimboNative64Entry *)(uintptr)1, (Native64TailSlot *)(uintptr)1, 0);
	puts("PASS typed-tail model refusal before descriptor reads");
	return 0;
#else
	for (i = 0; i < 150; i++) {
		slots[i].kind = Native64TailBig;
		slots[i].offset = i * 8;
		slots[i].bytes = 8;
		slots[i].type = nil;
		values[i] = ~(uvlong)0;
	}
	slots[0].kind = Native64TailByte;
	values[0] = 255;
	slots[1].kind = Native64TailInt;
	values[1] = 0xffffffff80000000ULL;
	slots[2].kind = Native64TailFix;
	values[2] = 0x7fffffffULL;
	slots[3].kind = Native64TailReal;
	values[3] = 0x8000000000000000ULL;
	slots[4].kind = Native64TailReal;
	values[4] = 0x7ff0000000000001ULL;
	slots[149].kind = Native64TailRef;
	slots[149].type = &expected;
	values[149] = (uvlong)(uintptr)H;
	check(native64tailplan(&fixed, slots, 150, &plan));
	check(plan.tail == 72 && plan.size == 1272 && plan.map_bytes == 20 && plan.count == 150);
	memset(map, 0x5a, sizeof(map));
	check(native64tailmap(&fixed, slots, 150, map, sizeof(map)));
	for (i = 0; i < 24; i++)
		check(map[i] == (i == 1 ? 0x80 : i == 19 ? 2 : i < 20 ? 0 : 0x5a));
	check(native64tailvalues(&fixed, slots, 150, values, sizeof(values), nil, nil));
	check(values[3] == 0x8000000000000000ULL && values[4] == 0x7ff0000000000001ULL);
	values[149] = 0x1230;
	check(native64tailvalues(&fixed, slots, 150, values, sizeof(values), reference, &references));
	check(references == 1);
	before = references;
	check(!native64tailvalues(&fixed, slots, 150, (uvlong *)(uintptr)1, sizeof(values) - 1,
							  reference, &references));
	check(references == before);
	for (i = 0; i < 3; i++) {
		original = values[i];
		values[i] = i == 0 ? 256 : i == 1 ? 0x80000000ULL : 0x100000000ULL;
		check(!native64tailvalues(&fixed, slots, 150, values, sizeof(values), reference,
								  &references));
		check(references == before);
		values[i] = original;
	}
	values[149] = 0;
	check(!native64tailvalues(&fixed, slots, 150, values, sizeof(values), reference, &references));
	check(references == before);
	values[149] = 0x1230;
	check(!native64tailvalues(&fixed, slots, 150, values, sizeof(values), nil, nil));
	slots[149].type = &wrong;
	check(!native64tailvalues(&fixed, slots, 150, values, sizeof(values), reference, &references));
	slots[149].type = &expected;
	/* An early reference must not be checked before a later scalar has canonical bits. */
	first = slots[0];
	changedslot = slots[149];
	slots[0].kind = Native64TailRef;
	slots[0].type = &expected;
	values[0] = 0x1230;
	slots[149].kind = Native64TailInt;
	slots[149].type = nil;
	values[149] = 0x80000000ULL;
	before = references;
	check(!native64tailvalues(&fixed, slots, 150, values, sizeof(values), reference, &references));
	check(references == before);
	slots[0] = first;
	slots[149] = changedslot;
	values[0] = 255;
	values[149] = 0x1230;
	changedslot = slots[149];
	slots[149].kind = 99;
	refused(&fixed, slots, 150);
	slots[149] = changedslot;
	slots[149].offset--;
	refused(&fixed, slots, 150);
	slots[149] = changedslot;
	slots[149].bytes = 4;
	refused(&fixed, slots, 150);
	slots[149] = changedslot;
	slots[149].type = nil;
	refused(&fixed, slots, 150);
	slots[149] = changedslot;
	slots[149].type = H;
	refused(&fixed, slots, 150);
	slots[149] = changedslot;
	slots[0].type = &expected;
	refused(&fixed, slots, 150);
	slots[0].type = nil;
	refused(&fixed, nil, 1);
	refused(&fixed, (Native64TailSlot *)(uintptr)1, Native64BindEntries + 1);
	/* Reject oversized fixed extents before reading the map. */
	changed = fixed;
	changed.frame_size = Native64BindFrame + 8;
	refused(&changed, slots, 150);
	memset(largemap, 0, sizeof(largemap));
	changed = fixed;
	changed.frame_size = Native64BindFrame;
	changed.map_bytes = sizeof(largemap);
	changed.map = largemap;
	refused(&changed, (Native64TailSlot *)(uintptr)1, 1);
	changed = fixed;
	changed.map_bytes = 1;
	refused(&changed, slots, 150);
	prefixmap[0] = 0x80;
	refused(&fixed, slots, 150);
	prefixmap[0] = 0;
	prefixmap[1] |= 1;
	refused(&fixed, slots, 150);
	prefixmap[1] &= ~1;
	memset(map, 0x5a, sizeof(map));
	check(!native64tailmap(&fixed, slots, 150, map, 19));
	for (i = 0; i < 24; i++)
		check(map[i] == 0x5a);
	check(native64tailplan(&fixed, nil, 0, &plan) && plan.size == 72 && plan.map_bytes == 2);
	check(native64tailvalues(&fixed, nil, 0, nil, 0, nil, nil));
	printf("PASS typed-tail preflight: %d active checks, canonical bits, long maps and unchanged "
		   "refusals\n",
		   checks);
	return 0;
#endif
}
