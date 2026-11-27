static uchar *ctorinput; // Borrowed file bytes deliberately poisoned after each construction.
static uchar ctorsaved[Dis64CallImageBytes]; // Immutable fixture copy used only to restore the next independent input.
static unsigned int ctorbytes; // Exact current file size, never an execution bound after construction.
static int ctorfaults, ctoraccepted, ctorrefused; // Observed actual source failure and ownership cases.


/* Stop at a semantic predicate and release bounded native test allocations before sanitizer exit. */
static void
ctorcheck(int condition)
{
	int i;

	if(condition)
		return;
	for(i=0; i<RuntimeBlocks; i++)
		if(blocks[i].pointer != nil)
			trackedfree(blocks[i].pointer);
	fprintf(stderr, "whole-image decision mismatch: actual call constructor ownership/lowering\n");
	exit(10);
}


/* Inspect copied native lowering before any possibly damaged instruction is executed. */
static void
ctormetadata(Module *module, int jit)
{
	Dis64Type type;
	Inst *instruction;
	unsigned int i;

	ctorcheck(module != nil && module->origmp == H && module->ref == 1 && modules == nil);
	ctorcheck(module->cstep == native64lea8step && (module->jit != nil) == jit &&
		module->ntype == resulttypes.count && module->nprog == resultcount &&
		module->entry == module->prog+resultentry && module->entryt == module->type[resultframe]);
	ctorcheck(module->name != nil && module->path != nil && strcmp(module->path, "actual-call-constructor") == 0);
	for(i=0; i<resulttypes.count; i++){
		ctorcheck(dis64typeget(&resulttypes, i, &type));
		ctorcheck(module->type[i] != nil && module->type[i]->size == type.bytes && module->type[i]->np == 0);
	}
	for(i=0; i<resultcount; i++){
		instruction = module->prog+i;
		ctorcheck(instruction->op == resultdecoded[i].instruction.op && instruction->add == resultdecoded[i].instruction.mode);
		if(UDST(instruction->add) == (AFP|AIND))
			ctorcheck(instruction->d.i.f == resultdecoded[i].instruction.destination &&
				instruction->d.i.s == resultdecoded[i].secondary);
	}
}


/* Exercise every actual constructor allocation failure before executing this accepted file. */
static void
ctorprobe(int valid)
{
	Module *module;
	int jit, count, fault, before;
	static int inputs; // Exercise invalid API shapes once, independently of raw semantic refusals.

	if(!inputs){
		inputs = 1;
		before = allocations;
		ctorcheck(prepare64calls(nil, ctorinput, ctorbytes, 0) == nil);
		ctorcheck(prepare64calls("bad", nil, ctorbytes, 0) == nil);
		ctorcheck(prepare64calls("bad", (uchar*)H, ctorbytes, 0) == nil);
		ctorcheck(prepare64calls("bad", ctorinput, ctorbytes, -1) == nil);
		ctorcheck(prepare64calls("bad", ctorinput, ctorbytes, 2) == nil);
		ctorcheck(prepare64calls("bad", ctorinput, (ulong)-1, 0) == nil);
		ctorcheck(prepare64calls("bad", (uchar*)((uintptr)-1-8), ctorbytes, 0) == nil);
		ctorcheck(allocations == before && live == 0 && worker.nerr == 0);
	}
	for(jit=0; jit<2; jit++){
		allocations = 0;
		before = live;
		module = prepare64calls("actual-call-constructor", ctorinput, ctorbytes, jit);
		if(!valid){
			ctorcheck(module == nil && live == before && worker.nerr == 0 && modules == nil && allocations <= 1);
			ctorrefused++;
			continue;
		}
		ctormetadata(module, jit);
		count = allocations;
		unload(module);
		ctorcheck(live == before && worker.nerr == 0 && modules == nil);
		ctoraccepted++;
		for(fault=1; fault<=count; fault++){
			allocations = 0;
			failat = fault;
			module = prepare64calls("actual-call-constructor", ctorinput, ctorbytes, jit);
			failat = 0;
			ctorcheck(module == nil && live == before && worker.nerr == 0 && modules == nil);
			ctorfaults++;
		}
	}
}


/* Construct via actual owning load64 code, then remove all source/workspace authority before execution. */
static Module*
resultmodule(int jit, int grow)
{
	Module *module;

	memmove(ctorinput, ctorsaved, ctorbytes);
	module = prepare64calls("actual-call-constructor", ctorinput, ctorbytes, jit);
	ctormetadata(module, jit);
	module->ss = grow ? 0 : 4096;
	memset(ctorinput, 0xa5, ctorbytes);
	return module;
}
