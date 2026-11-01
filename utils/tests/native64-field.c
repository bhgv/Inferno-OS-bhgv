#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LIMBO_NATIVE64_FIELDS // Select compiler-produced typed argument records, not wire metadata.
#include "probe-native64.h"
#include "math-native64.h"
#include "native64-field.h"

REG R; // Owning live register symbol, selected only while the bounded current process runs.
static Prog process; // Serialized scheduler identity; not a detached-process lifetime proof.
static unsigned int checks; // Active predicates independent of assert/NDEBUG.


/* Keep malformed-metadata and cancellation predicates active in optimized builds. */
static void
check(int valid)
{
	if(!valid){ fprintf(stderr, "field witness predicate failed after %u checks\n", checks); exit(1); }
	checks++;
}


/* Bound scheduler identity; real xec/vmachine ownership is checked by the separate owning gate. */
Prog*
currun(void)
{
	return &process;
}


/* Bound coherent availability; full classic-mk partial-profile linkage remains independently tested. */
int
native64callerruntime(void)
{
	return 1;
}


/* Require exactly one retained descriptor retirement without releasing its fixture owner. */
void
freetype(Type *type)
{
	check(type->ref > 1); type->ref--;
}


/* Keep the module's fixture owner and count the actual witness's extra reference. */
void
unload(Module *module)
{
	check(module->ref > 1); module->ref--;
}


/* An unexpected runtime error is not an accepted metadata refusal. */
void
error(char *message)
{
	fprintf(stderr, "unexpected field error: %s\n", message); exit(2);
}


/* Generated callable identities are metadata anchors; field binding never executes them. */
void
Probe_caller(void *frame)
{
	USED(frame);
}


/* A same-hash export with another implementation must never satisfy the caller's identity. */
void
Probe_other(void *frame)
{
	USED(frame);
}


/* Exercise real Math bit production followed by field-checked callback-free publication. */
int
main(void)
{
	union {
		uvlong align; // Natural Frame64 alignment.
		uchar bytes[sizeof(Stkext)+256]; // Two complete 128-byte typed caller frames.
	} stack;
	Native64FieldWitness witness = {0}, copied;
	LimboNative64Fields table;
	LimboNative64Field fields[8];
	LimboNative64Entry entry;
	Module module, wrong;
	Modlink instance, other;
	Link exports[2];
	Type *type, *types[1], *alien;
	F_Probe_caller *first, *second;
	F_Math_bits64real callee;
	REAL result;
	void *cell;
	uvlong bits, old = 0x1122334455667788ULL, observed;
	REG saved;
	Prog inactive;
	unsigned int mode, ordinal, state;
	int live, signature;

	check(limbo_native64_Probe_layout() && sizeof(F_Probe_caller) == 128);
	check(limbo_native64_Math_bits64real_field_table.count == 1);
	check(limbo_native64_Math_bits64real_fields[0].kind == Dis64Big &&
	      limbo_native64_Math_bits64real_fields[0].offset == 64 && limbo_native64_Math_bits64real_fields[0].bytes == 8);
	check(limbo_native64_Probe_caller_field_table.count == 8);
	check(limbo_native64_Probe_caller_fields[2].kind == Dis64Big && limbo_native64_Probe_caller_fields[2].offset == 72);
	check(limbo_native64_Probe_caller_fields[3].kind == Dis64Real && limbo_native64_Probe_caller_fields[3].offset == 80);
	check(limbo_native64_Probe_caller_fields[6].offset == 112);
	check(limbo_native64_Probe_caller_fields[7].kind == 0 && limbo_native64_Probe_caller_fields[7].offset == 120 &&
	      limbo_native64_Probe_caller_fields[7].bytes == 1);
	check(limbo_native64_Probe_caller_fields[4].kind == 0 && limbo_native64_Probe_caller_fields[5].kind == 0);
	type = calloc(1, sizeof(Type)+2); alien = calloc(1, sizeof(Type)+2);
	check(type != nil && alien != nil);
	type->ref = 1; type->size = 128; type->np = 2;
	memcpy(type->map, limbo_native64_Probe_caller_map, 2); memcpy(alien, type, sizeof(Type)+2);
	memset(&stack, 0, sizeof(stack)); memset(&module, 0, sizeof(module)); memset(&wrong, 0, sizeof(wrong));
	memset(&instance, 0, sizeof(instance)); memset(&other, 0, sizeof(other)); memset(exports, 0, sizeof(exports));
	module.ref = wrong.ref = 1; module.ntype = 1; module.type = types; module.ext = exports;
	types[0] = type; exports[0].name = "caller"; exports[0].frame = type; exports[0].u.runt = Probe_caller;
	memmove(&signature, &limbo_native64_Probe_caller_entry.legacy_signature, sizeof(signature)); exports[0].sig = signature;
	wrong = module; instance.m = &module; other.m = &wrong;
	first = (F_Probe_caller*)((Stkext*)stack.bytes)->reg.tos.fu; second = first+1;
	first->regs.t = second->regs.t = type;
	for(live=0; live<2; live++){
		memset(&process, 0, sizeof(process)); process.pid = 71; process.state = Pready;
		process.R.EX = stack.bytes; process.R.FP = (uchar*)first; process.R.SP = process.R.TS = (uchar*)(second+1);
		process.R.M = &instance; R = process.R;
		if(live) native64callerenter(&process);
		check(native64framecell(&process, type, 120) != nil); // Geometry alone accepts this byte-plus-padding slot.
		/* Every source ordinal is checked independently; same-width Big is not a Real. */
		for(ordinal=0; ordinal<9; ordinal++){
			int accepted = ordinal == 3 || ordinal == 6;
			check(native64fieldbegin(&witness, &process, &module, type, &limbo_native64_Probe_caller_field_table, ordinal, Dis64Real, live) == accepted);
			native64fieldclear(&witness); check(module.ref == 1 && type->ref == 1 && process.native64holds == 0);
		}
		for(state=Palt; state<=Pbroken; state++){
			process.state = state;
			check(native64fieldbegin(&witness, &process, &module, type, &limbo_native64_Probe_caller_field_table, 3, Dis64Real, live) == (state == Pready));
			native64fieldclear(&witness);
		}
		process.state = Pbroken;
		check(!native64fieldbegin(&witness, &process, (Module*)1, (Type*)1, (const LimboNative64Fields*)1, 3, Dis64Real, live));
		process.state = Pready;
		inactive = process;
		check(!native64fieldbegin(&witness, &inactive, (Module*)1, (Type*)1, (const LimboNative64Fields*)1, 3, Dis64Real, live));
		for(mode=0; mode<14; mode++){
			table = limbo_native64_Probe_caller_field_table; memcpy(fields, table.fields, sizeof(fields)); table.fields = fields;
			saved = process.R;
			memcpy(&first->value, &old, 8); memcpy(&second->value, &old, 8);
			check(native64fieldbegin(&witness, &process, &module, type, &table, 3, Dis64Real, live));
			check(module.ref == 2 && type->ref == 2 && process.native64holds == 1);
			copied = witness; check(native64fieldresolve(&copied) == nil); native64fieldclear(&copied);
			bits = 0x3ff0000000000000ULL+mode;
			memset(&callee, 0, sizeof(callee)); callee.ret = &result; memcpy(&callee.b, &bits, 8);
			Math_bits64real(&callee);
			if(mode == 1) check(native64fieldrebind(&witness, second));
			if(mode == 2) native64calleradvance(&process);
			if(mode == 3) fields[3].kind = Dis64Big;
			if(mode == 4) fields[3].offset = 72;
			if(mode == 5) fields[3].bytes = 4;
			if(mode == 6) exports[0].u.runt = Probe_other;
			if(mode == 7) instance.m = &wrong;
			if(mode == 8) process.R.MP = R.MP = (uchar*)1;
			if(mode == 9) process.R.M = R.M = &other;
			if(mode == 10) process.R.SP = R.SP = (uchar*)second;
			if(mode == 11) table.entry = &limbo_native64_Probe_other_entry;
			if(mode == 12) table.count--;
			if(mode == 13) native64fieldclear(&witness);
			cell = native64fieldresolve(&witness);
			check((cell != nil) == (mode < 2));
			if(cell != nil) memcpy(cell, &result, 8);
			memcpy(&observed, &first->value, 8); check(observed == (mode == 0 ? bits : old));
			memcpy(&observed, &second->value, 8); check(observed == (mode == 1 ? bits : old));
			native64fieldclear(&witness); native64fieldclear(&witness);
			check(module.ref == 1 && type->ref == 1 && process.native64holds == 0);
			exports[0].u.runt = Probe_caller; instance.m = &module; process.R = R = saved;
		}
		/* Identical maps cannot substitute another Type, owner, kind or callable. */
		check(!native64fieldbegin(&witness, &process, &wrong, type, &limbo_native64_Probe_caller_field_table, 3, Dis64Real, live));
		check(!native64fieldbegin(&witness, &process, &module, alien, &limbo_native64_Probe_caller_field_table, 3, Dis64Real, live));
		check(!native64fieldbegin(&witness, &process, &module, type, &limbo_native64_Probe_caller_field_table, 2, Dis64Real, live));
		for(mode=0; mode<5; mode++){
			table = limbo_native64_Probe_caller_field_table; memcpy(fields, table.fields, sizeof(fields)); table.fields = fields;
			if(mode == 0) fields[3].offset = 65; // Padding is scalar in the map, but is not a declared Real.
			if(mode == 1) fields[3].ordinal++;
			if(mode == 2) fields[3].kind = Dis64Big;
			if(mode == 3) fields[3].bytes = 16;
			if(mode == 4){ entry = *table.entry; entry.function = Probe_other; table.entry = &entry; }
			check(!native64fieldbegin(&witness, &process, &module, type, &table, 3, Dis64Real, live));
		}
		if(live){
			Modlink *original = process.R.M;
			process.R.M = (Modlink*)1; // Published mode must reject divergence before dereferencing saved roots.
			check(!native64fieldbegin(&witness, &process, &module, type, &limbo_native64_Probe_caller_field_table, 3, Dis64Real, 0));
			process.R.M = original;
			native64callerleave(&process);
		}
	}
	free(type); free(alien);
	printf("PASS compiler scalar fields: %u predicates, exact ordinal/kind/width, real Math, module/Type/epoch and paired-root cancellation\n", checks);
	return 0;
}
