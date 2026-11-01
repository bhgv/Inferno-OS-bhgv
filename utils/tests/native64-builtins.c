#define LIMBO_NATIVE64_TABLES // Fresh compiler-produced specifications and actual C entry records.
#include "builtin-native64.h"
#include "math-native64.h"

#ifdef NATIVE64_NO_LSAN


/* Optional host limitation: process-inspection leak scans are unavailable; allocation balance stays active. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
#endif

static int calls; // Number of actual C invocations through owned runtime links.


/* Execute a scalar generated frame through the runtime's real export callback. */
void
Probe_add(void *pointer)
{
	F_Probe_add *frame;

	frame = pointer;
	check(frame->x == 17 && frame->y == -9);
	*frame->ret = frame->x + frame->y;
	calls++;
}


/* Observe both ends of the generated 150-reference frame without dereferencing borrowed owners. */
void
Probe_many(void *pointer)
{
	F_Probe_many *frame;

	frame = pointer;
	check(frame->a0 == H && frame->a149 == H);
	calls++;
}


/* Every refusal and unload must restore the exact module chain, allocation and error-frame state. */
static void
balanced(void)
{
	check(modules == &baseline && baseline.ref == 7 && live == 0 && worker.nerr == 0);
	checks++;
}


/* Malformed metadata must fail before allocating or publishing any partial owner. */
static void
refused(const LimboNative64Entry *specs, unsigned int count,
	const LimboNative64Entry *const *entries, unsigned int selected)
{
	int before;

	before = attempts;
	check(builtinmod64("$Probe", specs, count, entries, selected) == nil);
	check(attempts == before);
	balanced();
}


/* Registered names, maps and callback identities must no longer borrow the temporary records. */
#ifndef NATIVE64_REFUSED
static void
metadataownership(void)
{
	LimboNative64Entry specs[2], entry;
	const LimboNative64Entry *selected[1];
	Module *module;
	uchar map[20];
	char name[16] = "many";
	int i;

	memmove(specs, limbo_native64_Probe_specs, sizeof(specs));
	entry = limbo_native64_Probe_many_entry;
	memmove(map, entry.map, sizeof(map));
	entry.name = name;
	entry.map = map;
	selected[0] = &entry;
	for(i=0; i<2; i++)
		if(strcmp(specs[i].name, "many") == 0){
			specs[i].name = name;
			specs[i].map = map;
		}
	module = builtinmod64("$Owned", specs, 2, selected, 1);
	check(module != nil);
	memset(name, 'x', 4);
	memset(map, 0, sizeof(map));
	entry.function = nil;
	check(strcmp(module->ext[0].name, "many") == 0);
	check(module->ext[0].u.runt == Probe_many);
	check(module->type[0]->np == 20);
	check(memcmp(module->type[0]->map, limbo_native64_Probe_many_entry.map, 20) == 0);
	unload(module);
	balanced();
}
#endif


/* A failed second constructor must leave an already published module and its descriptors intact. */
#ifndef NATIVE64_REFUSED
static void
publicationisolation(void)
{
	const LimboNative64Entry *entries[] = { &limbo_native64_Probe_many_entry };
	Module *outer, *inner;
	Type *frame;
	int cutoff, owners;

	for(cutoff=0; cutoff<12; cutoff++){
		outer = builtinmod64("$Outer", limbo_native64_Probe_specs, 2, entries, 1);
		check(outer != nil);
		frame = outer->type[0];
		owners = live;
		fail_after = cutoff;
		inner = builtinmod64("$Inner", limbo_native64_Probe_specs, 2, entries, 1);
		fail_after = -1;
		if(inner != nil){
			check(modules == inner && inner->link == outer);
			unload(inner);
		}
		check(modules == outer && outer->link == &baseline && outer->ref == 1);
		check(outer->type[0] == frame && frame->ref == 1 && frame->np == 20);
		check(live == owners && worker.nerr == 0);
		unload(outer);
		balanced();
	}
}
#endif


/* Register actual Math implementations through the complete generated specification table. */
#ifndef NATIVE64_REFUSED
static void
actualmath(void)
{
	const LimboNative64Entry *entries[] = {
		&limbo_native64_Math_bits32real_entry,
		&limbo_native64_Math_realbits32_entry,
		&limbo_native64_Math_bits64real_entry,
		&limbo_native64_Math_realbits64_entry,
	};
	Module *module;
	F_Math_bits32real from32;
	F_Math_realbits32 to32;
	F_Math_bits64real from64;
	F_Math_realbits64 to64;
	REAL real;
	WORD word;
	LONG big;
	int cutoff, i, successful;

	successful = 0;
	for(cutoff=0; cutoff<17; cutoff++){
		fail_after = cutoff;
		module = builtinmod64("$Math", limbo_native64_Math_specs,
			limbo_native64_Math_spec_count, entries, nelem(entries));
		fail_after = -1;
		if(module != nil){
			successful++;
			check(module->ntype == 4 && module->ext[4].name == nil);
			for(i=0; i<16; i++){
				memset(&from32, 0, sizeof(from32));
				memset(&to32, 0, sizeof(to32));
				memset(&from64, 0, sizeof(from64));
				memset(&to64, 0, sizeof(to64));
				from32.ret = &real;
				from32.b = 0xbf800000U;
				module->ext[0].u.runt(&from32);
				check(real == -1.0);
				to32.ret = &word;
				to32.x = real;
				module->ext[1].u.runt(&to32);
				check((UWORD)word == 0xbf800000U);
				from64.ret = &real;
				from64.b = (LONG)0xbff0000000000000ULL;
				module->ext[2].u.runt(&from64);
				check(real == -1.0);
				to64.ret = &big;
				to64.x = real;
				module->ext[3].u.runt(&to64);
				check((ULONG)big == 0xbff0000000000000ULL);
			}
			unload(module);
		}
		balanced();
	}
	check(successful == 4);
}
#endif


/* Test all allocation cutoffs, complete long maps, export ordering, owned metadata and actual calls. */
int
main(void)
{
	LimboNative64Entry specs[2], changed;
	const LimboNative64Entry *entries[2];
	Module *module;
	F_Probe_add scalar;
	F_Probe_many many;
	WORD result;
	String *empty;
	uchar badmap[20];
	int cutoff, failed, succeeded, i;

	baseline.ref = 7;
	baseline.path = "$Baseline";
	entries[0] = &limbo_native64_Probe_many_entry;
	entries[1] = &limbo_native64_Probe_add_entry;
	memmove(specs, limbo_native64_Probe_specs, sizeof(specs));
#ifdef NATIVE64_REFUSED
	refused(specs, 2, entries, 2);
	return 0;
#else
	check(limbo_native64_model());
	failed = succeeded = calls = 0;
	for(cutoff=0; cutoff<18; cutoff++){
		fail_after = cutoff;
		module = builtinmod64("$Probe", specs, 2, entries, 2);
		fail_after = -1;
		if(module == nil)
			failed++;
		else{
			succeeded++;
			check(modules == module && module->link == &baseline && module->ntype == 2);
			check(module->ext[2].name == nil);
			check(module->type[0]->size == sizeof(F_Probe_many) && module->type[0]->np == 20);
			check(memcmp(module->type[0]->map, entries[0]->map, 20) == 0);
			check(module->ext[0].frame == module->type[0] && module->ext[1].frame == module->type[1]);
			check(strcmp(module->ext[0].name, "many") == 0 && strcmp(module->ext[1].name, "add") == 0);
			check(module->ext[0].name != entries[0]->name && module->type[0]->map != entries[0]->map);
			check(memcmp(&module->ext[0].sig, &entries[0]->legacy_signature, 4) == 0);
			memset(&scalar, 0, sizeof(scalar));
			result = -777;
			scalar.ret = &result;
			scalar.x = 17;
			scalar.y = -9;
			module->ext[1].u.runt(&scalar);
			check(result == 8);
			memset(&many, 0, sizeof(many));
			empty = H;
			for(i=0; i<150; i++)
				memmove((uchar*)&many+64+i*sizeof(empty), &empty, sizeof(empty));
			module->ext[0].u.runt(&many);
			unload(module);
		}
		balanced();
	}
	check(failed == 9 && succeeded == 9 && calls == 18);
	refused(nil, 2, entries, 2);
	refused(specs, 2, nil, 2);
	refused(specs, Native64BindEntries+1, entries, 2);
	refused(specs, 2, entries, Native64BindEntries+1);
	changed = *entries[1];
	entries[1] = &changed;
	changed.legacy_signature ^= 1;
	refused(specs, 2, entries, 2);
	changed = limbo_native64_Probe_add_entry;
	changed.function = nil;
	refused(specs, 2, entries, 2);
	changed = limbo_native64_Probe_add_entry;
	changed.name = "missing";
	refused(specs, 2, entries, 2);
	entries[1] = entries[0];
	refused(specs, 2, entries, 2);
	changed = *entries[0];
	entries[1] = &limbo_native64_Probe_add_entry;
	entries[0] = &changed;
	changed.map_bytes = 16;
	refused(specs, 2, entries, 2);
	changed = limbo_native64_Probe_many_entry;
	memmove(badmap, changed.map, sizeof(badmap));
	changed.map = badmap;
	badmap[0] = 0x80;
	refused(specs, 2, entries, 2);
	memmove(badmap, limbo_native64_Probe_many_entry.map, sizeof(badmap));
	badmap[19] |= 1;
	refused(specs, 2, entries, 2);
	/* Even unselected specifications are validated before the first allocation. */
	specs[0].function = Probe_add;
	refused(specs, 2, nil, 0);
	memmove(specs, limbo_native64_Probe_specs, sizeof(specs));
	for(cutoff=0; cutoff<7; cutoff++){
		fail_after = cutoff;
		module = builtinmod64("$Empty", nil, 0, nil, 0);
		fail_after = -1;
		if(module != nil){
			check(module->ntype == 0 && module->type == nil && module->ext[0].name == nil);
			unload(module);
		}
		balanced();
	}
	metadataownership();
	publicationisolation();
	actualmath();
	printf("PASS native64 builtin runtime: %d balanced transactions, long maps, actual calls and metadata refusals\n", checks);
	return 0;
#endif
}
