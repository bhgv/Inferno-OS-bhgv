/* Appended to the actual constructors and existing generated single-module/Math tests. */
static int batchwatch, batchobservations, batchtransactions; // Visibility checkpoints and completed set transactions.
static Module *batchbase; // Exact pre-existing registry head, unchanged throughout every allocation.
static Module **batchoutput; // Caller-owned nil outputs observed before commit.
static unsigned int batchcount; // Exact bounded output span, not a sentinel search.


/* No allocator entry may observe an early module or partially changed output vector. */
static void
batchobserve(void)
{
	unsigned int i;

	if(!batchwatch)
		return;
	check(modules == batchbase);
	for(i=0; i<batchcount; i++) check(batchoutput[i] == nil);
	batchobservations++;
}

#ifndef NATIVE64_REFUSED


/* Invoke actual generated scalar and Math callbacks through the committed links. */
static void
batchcalls(Module **output)
{
	F_Probe_add add;
	F_Math_bits64real from;
	F_Math_realbits64 to;
	REAL real;
	WORD result;
	LONG big;

	memset(&add, 0, sizeof(add)); add.ret=&result; add.x=17; add.y=-9;
	output[0]->ext[0].u.runt(&add); check(result == 8);
	memset(&from, 0, sizeof(from)); from.ret=&real; from.b=(LONG)0xbff0000000000000ULL;
	output[1]->ext[0].u.runt(&from); check(real == -1.0);
	memset(&to, 0, sizeof(to)); to.ret=&big; to.x=real;
	output[1]->ext[1].u.runt(&to); check((ULONG)big == 0xbff0000000000000ULL);
	check(output[2]->type[0]->np == 20 && output[2]->type[0]->ref == 1);
	check(memcmp(output[2]->type[0]->map, limbo_native64_Probe_many_entry.map, 20) == 0);
}


/* Fail each allocation in three modules, with and without a live previously published owner. */
static void
batchfaults(int outer)
{
	const LimboNative64Entry *scalar[] = {&limbo_native64_Probe_add_entry};
	const LimboNative64Entry *math[] = {&limbo_native64_Math_bits64real_entry, &limbo_native64_Math_realbits64_entry};
	const LimboNative64Entry *wide[] = {&limbo_native64_Probe_many_entry};
	Native64ModuleSpec specs[3];
	Module *output[3], *previous;
	char names[3][16] = {"$SetA", "$SetMath", "$SetWide"};
	int cutoff, owners, status, failed, succeeded, i;

	failed=succeeded=0;
	for(cutoff=0; cutoff<=24; cutoff++){
		previous = outer ? builtinmod64("$Previous", limbo_native64_Probe_specs, 2, wide, 1) : nil;
		check(!outer || previous != nil);
		owners=live;
		memset(output, 0, sizeof(output));
		specs[0]=(Native64ModuleSpec){names[0], limbo_native64_Probe_specs, 2, scalar, 1};
		specs[1]=(Native64ModuleSpec){names[1], limbo_native64_Math_specs, limbo_native64_Math_spec_count, math, 2};
		specs[2]=(Native64ModuleSpec){names[2], limbo_native64_Probe_specs, 2, wide, 1};
		batchbase=modules; batchoutput=output; batchcount=3;
		batchwatch=batchdisturb=1;
		kwerrstr("%s", "original set diagnostic");
		fail_after=cutoff;
		status=builtinmods64(specs, 3, output);
		fail_after=-1; batchwatch=batchdisturb=0;
		if(status){
			succeeded++;
			check(cutoff == 24 && modules == output[2]);
			check(output[2]->link == output[1] && output[1]->link == output[0] && output[0]->link == batchbase);
			check(strcmp(diagnostic, "original set diagnostic") == 0);
			batchcalls(output);
			/* Module names must be independent of the caller's request/name storage. */
			for(i=0; i<3; i++) names[i][1]='X';
			check(strcmp(output[0]->name, "$SetA") == 0 && strcmp(output[1]->name, "$SetMath") == 0);
			unload(output[1]);
			check(output[2]->link == output[0] && output[2]->type[0]->np == 20);
			unload(output[0]); unload(output[2]);
		}else{
			failed++;
			check(modules == batchbase && live == owners && output[0] == nil && output[1] == nil && output[2] == nil);
			check(strcmp(diagnostic, "cleanup changed error") != 0 && strcmp(diagnostic, "%s") != 0);
			check(strcmp(diagnostic, exNomem) == 0 || strcmp(diagnostic, "bad native64 builtin frame or out of memory") == 0);
			check(worker.nerr == 0);
		}
		if(previous != nil){
			check(modules == previous && previous->ref == 1 && previous->type[0]->np == 20);
			unload(previous);
		}
		balanced(); batchtransactions++;
	}
	check(failed == 24 && succeeded == 1);
}


/* Reject a malformed final request, duplicate names, occupied outputs and spans before allocation. */
static void
batchpreflight(void)
{
	const LimboNative64Entry *entries[] = {&limbo_native64_Probe_add_entry};
	Native64ModuleSpec specs[2];
	Module *output[2];
	LimboNative64Entry changed;
	const LimboNative64Entry *bad[] = {&changed};
	int mode, before;

	before=attempts;
	check(builtinmods64(nil, 0, nil) && attempts == before);
	for(mode=0; mode<9; mode++){
		specs[0]=(Native64ModuleSpec){"$One", limbo_native64_Probe_specs, 2, entries, 1};
		specs[1]=(Native64ModuleSpec){"$Two", limbo_native64_Probe_specs, 2, entries, 1};
		output[0]=output[1]=nil;
		if(mode == 0) specs[1].name=specs[0].name;
		if(mode == 1){ changed=*entries[0]; changed.legacy_signature^=1; specs[1].entries=bad; }
		if(mode == 2) specs[1].count=Native64BindEntries+1;
		if(mode == 3) specs[1].selected=Native64BindEntries+1;
		if(mode == 4) specs[1].name="";
		if(mode == 5) output[1]=&baseline;
		before=attempts;
		check(!builtinmods64(mode == 6 ? nil : specs, mode == 7 ? Native64BindEntries+1 : 2,
			mode == 8 ? (Module**)((uintptr)-1-sizeof(void*)) : output));
		check(attempts == before && output[0] == nil && output[1] == (mode == 5 ? &baseline : nil));
		balanced(); batchtransactions++;
	}
}


/* Aggregate declaration bounds must reject the later module after a complete maximum-size valid specification. */
static void
batchbounds(void)
{
	Native64ModuleSpec specs[2];
	LimboNative64Entry *many;
	char (*names)[16];
	uchar map[] = {0};
	Module *output[2] = {nil};
	unsigned int i;
	int before;

	many=malloc(Native64BindEntries*sizeof(*many));
	names=malloc(Native64BindEntries*sizeof(*names));
	check(many != nil && names != nil);
	for(i=0; i<Native64BindEntries; i++){
		snprintf(names[i], sizeof(names[i]), "entry%u", i);
		many[i]=(LimboNative64Entry){names[i], nil, 0, 64, 1, map};
	}
	specs[0]=(Native64ModuleSpec){"$Maximum", many, Native64BindEntries, nil, 0};
	specs[1]=(Native64ModuleSpec){"$Overflow", limbo_native64_Probe_specs, 2, nil, 0};
	before=attempts;
	check(!builtinmods64(specs, 2, output) && attempts == before && output[0] == nil && output[1] == nil);
	check(builtinmods64(specs, 1, output) && output[0]->ntype == 0 && output[0]->ext[0].name == nil);
	unload(output[0]);
	/* Release the independently owned input buffers after the committed module no longer borrows them. */
	free(names); free(many);
	balanced(); batchtransactions++;
}
#endif


/* Retain all actual single-module gates and check the shared set engine's guarded host-model refusal. */
int
main(void)
{
	int before;

	native64_single_main();
#ifdef NATIVE64_REFUSED
	before=attempts;
	check(!builtinmods64((Native64ModuleSpec*)1, Native64BindEntries, (Module**)1));
	check(attempts == before);
	balanced();
#else
	USED(before);
	batchfaults(0); batchfaults(1); batchpreflight(); batchbounds();
	check(batchtransactions == 60 && batchobservations > 0);
	printf("PASS native64 module sets: %d transactions, %d atomic visibility checkpoints, every allocation and actual C/Math links\n", batchtransactions, batchobservations);
#endif
	return 0;
}
