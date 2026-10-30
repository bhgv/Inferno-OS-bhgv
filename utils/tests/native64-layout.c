#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "dis-layout.h"
#include "native64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int predicates; // Active layout/map checks remain enabled under NDEBUG.


/* Fail on a concrete C layout or independently expected bitmap mismatch. */
static void
check(int condition)
{
	if(!condition)
		abort();
	predicates++;
}


/* Compare emitted metadata with native C and the shared map8 validator. */
static void
verify(unsigned int actual, unsigned int declared, const uchar *map, unsigned int bytes,
	const uchar *expected, unsigned int wanted)
{
	DisAbi abi;

	check(actual == declared && bytes == wanted);
	check(bytes == (actual+63)/64 && memcmp(map, expected, bytes) == 0);
	check(disabinit(&abi, 8) && disabmap(&abi, actual, map, bytes));
}

#define Verify(type, wanted) verify(sizeof(type), type##_size, (uchar[])type##_map, sizeof((uchar[])type##_map), wanted, sizeof(wanted)) // Compare complete actual declarations, not mirrored layouts.


/* Inspect offsets, full maps, nested/pointer fields, channel payloads and large frames. */
int
main(void)
{
	static const uchar zero[] = {0};
	static const uchar pointer[] = {0x40};
	static const uchar outer[] = {0x48};
	static const uchar work[] = {0x80};
	static const uchar frame[] = {0, 0x52};
	static const uchar noop[] = {0};
	static const uchar vari[] = {0, 0x80};
	uchar many[20];
	int i;

#ifdef NATIVE64_UNSUPPORTED
	check(!limbo_native64_model());
	printf("PASS native64 unsupported host/model gate\n");
	return 0;
#endif
	check(limbo_native64_model());
	Verify(Probe_Tail, pointer);
	Verify(Probe_Pair, zero);
	Verify(Probe_Byte4, zero);
	Verify(Probe_Inner, pointer);
	Verify(Probe_Outer, outer);
	Verify(Probe_Tuple, pointer);
	Verify(Probe_Reply, pointer);
	Verify(Probe_Work, work);
	Verify(Probe_Work_out, pointer);
	Verify(F_Probe_value, frame);
	Verify(F_Probe_noop, noop);
	Verify(F_Probe_vari, vari);
	check(sizeof(Probe_Outer) == 48 && offsetof(Probe_Outer, lead) == 24);
	check(offsetof(Probe_Outer, link) == 32 && offsetof(Probe_Outer, ints) == 40);
	check(sizeof(Probe_Byte4) == 4 && offsetof(Probe_Tail, next) == 8);
	check(offsetof(F_Probe_value, ret) == 32 && offsetof(F_Probe_value, temps) == 40);
	check(offsetof(F_Probe_value, x) == 64 && offsetof(F_Probe_value, y) == 72);
	check(offsetof(F_Probe_value, z) == 80 && sizeof(F_Probe_value) == 128);
	check(F_Probe_vari_variadic == 1 && F_Probe_value_variadic == 0);
	check(offsetof(F_Probe_noop, noret) == 32 && sizeof(F_Probe_noop) == 64);
	many[0] = 0;
	for(i=1; i<19; i++)
		many[i] = 0xff;
	many[19] = 0xfc;
	Verify(F_Probe_many, many);
	check(sizeof(F_Probe_many) == 1264 && offsetof(F_Probe_many, a149) == 1256);
	printf("PASS native64 declarations: %d predicates, 13 actual maps, natural aggregates, channel payloads and Frame64\n", predicates);
	return 0;
}
