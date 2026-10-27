static int bridgeselected, bridgemode, bridgefastcalls, bridgeccalls; // Synchronous route observations.
static int bridgedecoded, bridgeentered; // Decoder/cache-boundary observations.
static Modlink bridgelink; // Stable destination after callback-driven module switches.
static Inst bridgetarget[2]; // Independent callback-selected Dis PCs.
static Module *bridgedoomed; // Mapping owner deliberately released by a C callback.


/* Record the selected slot and publish explicit callback-controlled VM state. */
static void
bridgerecord(int op)
{
	UWORD value;

	mappedcheck(op == bridgeselected);
	bridgeccalls++;
	if(bridgemode == 9)
		error(bridgeexception);
	memmove(&value, R.s, sizeof(value));
	value ^= (UWORD)(op+1);
	memmove(R.d, &value, sizeof(value));
	R.PC = &bridgetarget[bridgemode&1];
	R.M = &bridgelink;
	R.MP = bridgelink.MP;
	R.IC = bridgemode&1 ? 1 : 4;
	R.t = op*17;
}


/* A declined fast path may change counters outside VM state, never operands. */
static int
bridgerefuse(REG *r)
{
	mappedcheck(r == &R);
	bridgefastcalls++;
	return 0;
}


/* Complete a fast path without requiring the C table to be consulted. */
static int
bridgeaccept(REG *r)
{
	UWORD value;

	mappedcheck(r == &R);
	bridgefastcalls++;
	memmove(&value, r->s, sizeof(value));
	value += (UWORD)31;
	memmove(r->d, &value, sizeof(value));
	r->PC = &bridgetarget[1];
	r->IC = 1;
	r->t = -7;
	return bridgemode == 3 ? -1 : 1;
}


/* Release the source code owner while executing only stable host C code. */
static void
bridgerelease(void)
{
	Module *m;
	Jit *j;

	m = bridgedoomed;
	mappedcheck(R.PC == m->prog+1 && R.IC == 4);
	j = m->jit;
	R.M = &bridgelink;
	R.MP = bridgelink.MP;
	R.PC = &bridgetarget[1];
	R.IC = 2;
	R.t = 1;
	bridgedoomed = nil;
	j->freecode(j->code, j->size);
	free(j);
	free(m);
	bridgeccalls++;
}


/* Finish the next module's decoded command after the old mapping was released. */
static void
bridgefinish(void)
{
	mappedcheck(R.PC == &bridgetarget[2] && R.IC == 1);
	R.PC = &bridgetarget[0];
	R.IC = 1;
	R.t = 2;
	bridgeccalls++;
}


/* Decode into independent cells without changing the already selected opcode. */
static void
bridgedecode(void)
{
	bridgedecoded++;
}


/* The VM's direct hot path must not enter the complete wrapper a second time. */
static int
bridgeunreachable(REG *r)
{
	(void)r;
	abort();
	return 0;
}


/* Count cache-entry calls without modifying PC, operands or ownership. */
static void
bridgeenter(void)
{
	bridgeentered++;
}


/* Verify complete compiler binding, absent slots and cached reuse for every byte opcode. */
static unsigned long
bridgecompilerentries(void)
{
	Module module = {0};
	Inst instruction = {0};
	LeafOp operation;
	u32int expected[LeafMax];
	Jit *jit;
	uintptr first, last, target;
	unsigned long checks = 0, before;
	int op, other, active, n;

	module.prog = &instruction;
	module.nprog = 1;
	for(op = 0; op < 256; op++){
		instruction.op = op;
		active = op > INOP && op < MAXDIS && op != IECLR;
		before = bridgeallocations;
		if(!active){
			mappedcheck(!compile(&module, 1, nil) && module.jit == nil && bridgeallocations == before);
			checks++;
			continue;
		}
		mappedcheck(compile(&module, 1, nil) && module.jit != nil);
		jit = module.jit;
		first = (uintptr)jit->code;
		last = first+jit->size;
		for(other = 0; other < 256; other++){
			mappedcheck((jit->dispatch[other] != nil) == (other == op));
			if(other != op)
				mappedcheck(jit->entry[other] == nil);
		}
		target = (uintptr)jit->dispatch[op];
		mappedcheck(target >= first && target < last && target%4 == 0);
		mappedcheck((jit->entry[op] != nil) == leafop(&operation, op, sizeof(WORD)));
		if(jit->entry[op] != nil)
			mappedcheck((uintptr)jit->entry[op] == first && first < target);
		n = leafbridgeemit(expected, LeafMax, (void*)cjitexec, op, (void*)jit->entry[op]);
		mappedcheck(n > 0 && target+(uintptr)n*4 == last && !memcmp((void*)target, expected, n*4));
		before = bridgeallocations;
		mappedcheck(compile(&module, 1, nil) && module.jit == jit && bridgeallocations == before);
		jit->freecode(jit->code, jit->size);
		free(jit);
		module.jit = nil;
		checks++;
	}
	return checks;
}


/* Reject reserved byte opcodes before allocation, including a cached mixed program. */
static unsigned long
bridgemixedchecks(void)
{
	Module m = {0};
	Inst program[2] = {{0}};
	Jit *cache;
	unsigned long checks = 0, before;
	int op, position;
	char *phase = getenv("BRIDGE_OPCODE_PHASE");

	m.prog = program;
	m.nprog = 2;
	if(phase == nil || strcmp(phase, "cached"))
	for(position = 0; position < 2; position++){
		for(op = 0; op < 256; op++){
			if(op > INOP && op < MAXDIS && op != IECLR)
				continue;
			program[0].op = program[1].op = IADDW;
			program[position].op = op;
			before = bridgeallocations;
			mappedcheck(!compile(&m, 2, nil) && m.jit == nil);
			mappedcheck(bridgeallocations == before);
			checks++;
		}
	}
	program[0].op = program[1].op = IADDW;
	mappedcheck(compile(&m, 2, nil) && m.jit != nil);
	cache = m.jit;
	before = bridgeallocations;
	if(phase == nil || strcmp(phase, "uncached"))
	for(position = 0; position < 2; position++){
		for(op = 0; op < 256; op++){
			if(op > INOP && op < MAXDIS && op != IECLR)
				continue;
			program[0].op = program[1].op = IADDW;
			program[position].op = op;
			mappedcheck(!compile(&m, 2, nil) && m.jit == cache);
			mappedcheck(bridgeallocations == before);
			checks++;
		}
	}
	cache->freecode(cache->code, cache->size);
	free(cache);
	return checks;
}


/* Cached native mappings must retain instruction alignment and whole-word extents. */
static unsigned long
bridgegeometrychecks(void)
{
	Module m = {0};
	Inst program = {0};
	Jit *cache;
	uchar *code;
	unsigned long checks = 0, before;
	int size, shift;
	char *phase = getenv("BRIDGE_GEOMETRY_PHASE");

	m.prog = &program;
	m.nprog = 1;
	program.op = IADDW;
	mappedcheck(compile(&m, 1, nil) && m.jit != nil);
	cache = m.jit;
	code = cache->code;
	size = cache->size;
	before = bridgeallocations;
	if(phase == nil || strcmp(phase, "entries"))
	for(shift = 1; shift < 4; shift++){
		cache->code = code+shift;
		mappedcheck(!compile(&m, 1, nil));
		mappedcheck(m.jit == cache && bridgeallocations == before);
		cache->code = code;
		cache->size = size+shift;
		mappedcheck(!compile(&m, 1, nil));
		mappedcheck(m.jit == cache && bridgeallocations == before);
		cache->size = size;
		mappedcheck(compile(&m, 1, nil));
		checks += 3;
	}
	if(phase == nil || strcmp(phase, "alignment")){
		int (*fast)(REG*) = cache->entry[IADDW];
		int (*dispatch)(REG*) = cache->dispatch[IADDW];
		uintptr targets[6];
		int index;

		targets[0] = 0;
		targets[1] = (uintptr)-1;
		targets[2] = (uintptr)code-4;
		targets[3] = (uintptr)code+size;
		targets[4] = (uintptr)code+1;
		targets[5] = (uintptr)code+size-4;
		for(index = 0; index < 6; index++){
			cache->dispatch[IADDW] = (int(*)(REG*))targets[index];
			mappedcheck(!compile(&m, 1, nil) && m.jit == cache);
			mappedcheck(bridgeallocations == before);
			cache->dispatch[IADDW] = dispatch;
			if(index > 0){
				cache->entry[IADDW] = (int(*)(REG*))targets[index];
				mappedcheck(!compile(&m, 1, nil) && m.jit == cache);
				mappedcheck(bridgeallocations == before);
				cache->entry[IADDW] = fast;
				checks++;
			}
			checks++;
		}
		cache->dispatch[IECLR] = dispatch;
		mappedcheck(!compile(&m, 1, nil));
		cache->dispatch[IECLR] = nil;
		cache->entry[IALT] = fast;
		mappedcheck(!compile(&m, 1, nil));
		cache->entry[IALT] = nil;
		program.op = ISUBW;
		mappedcheck(!compile(&m, 1, nil));
		program.op = IADDW;
		mappedcheck(compile(&m, 1, nil) && bridgeallocations == before);
		checks += 4;
	}
	cache->freecode(code, size);
	free(cache);
	return checks;
}


/* Reject invalid headers, bytecode spans and stale cache geometry before allocation. */
static unsigned long
bridgecompilerchecks(void)
{
	Module m = {0};
	Inst program[2] = {{0}};
	Jit cache = {0};
	unsigned long before, checks = 0;
	uintptr i;
	char *selected;

	union {
		uvlong align; // Native alignment for deliberately shifted program/cache images.
		uchar bytes[sizeof(Jit)+2*sizeof(Inst)+8]; // Full object images plus alignment canaries.
	} image;

	selected = getenv("BRIDGE_COMPILER_CASE");
	m.nprog = 2;
	m.prog = program;
	program[0].op = program[1].op = IADDW;
	before = bridgeallocations;
	if(selected == nil || !strcmp(selected, "header")){
		mappedcheck(!compile(nil, 2, nil));
		for(i = 0; i < sizeof(Module); i++){
			mappedcheck(!compile((Module*)((uintptr)-1-i), 2, nil));
			mappedcheck(bridgeallocations == before);
			checks++;
		}
	}
	if(selected == nil || !strcmp(selected, "program")){
		for(i = 0; i < 2*sizeof(Inst); i++){
			m.prog = (Inst*)((uintptr)-1-i);
			mappedcheck(!compile(&m, 2, nil) && m.jit == nil);
			mappedcheck(bridgeallocations == before);
			checks++;
		}
		m.prog = program;
	}
	if(selected == nil || !strcmp(selected, "cache")){
		for(i = 0; i < sizeof(Jit); i++){
			m.jit = (Jit*)((uintptr)-1-i);
			mappedcheck(!compile(&m, 2, nil));
			mappedcheck(bridgeallocations == before);
			checks++;
		}
		cache.nprog = 1;
		m.jit = &cache;
		mappedcheck(!compile(&m, 2, nil) && bridgeallocations == before);
		checks++;
		cache.nprog = 2;
		for(i = 0; i < 6; i++){
			cache.code = i == 0 ? nil : i == 1 ? H : i == 2 ? (void*)((uintptr)-2) : i == 5 ? (void*)((uintptr)-9) : (void*)program;
			cache.size = i == 3 ? 0 : i == 4 ? -1 : i == 5 ? 16 : 4;
			mappedcheck(!compile(&m, 2, nil) && bridgeallocations == before);
			checks++;
		}
		m.jit = nil;
	}
	if(selected == nil || !strcmp(selected, "unaligned")){
		for(i = 1; i < 8; i++){
			memset(image.bytes, 0, sizeof(image.bytes));
			image.bytes[i+offsetof(Inst, op)] = IADDW;
			image.bytes[i+sizeof(Inst)+offsetof(Inst, op)] = IRUNT;
			m.prog = (Inst*)(image.bytes+i);
			mappedcheck(compile(&m, 2, nil) && m.jit && m.jit->dispatch[IADDW]);
			m.jit->freecode(m.jit->code, m.jit->size);
			free(m.jit);
			m.jit = nil;
			checks++;
		}
	}
	if(selected == nil || !strcmp(selected, "opcodes"))
		checks += bridgemixedchecks();
	if(selected == nil || !strcmp(selected, "geometry"))
		checks += bridgegeometrychecks();
	if(selected == nil)
		checks += bridgecompilerentries();
	return checks;
}


/* Compare all opcode routes against full memory/REG images through actual RX tails. */
static unsigned long
bridgeroutes(void)
{
	uchar cells[128], before[128], expected[128];
	REG saved, wanted, other;
	u32int *code;
	int (*invoke)(REG*), (*fast[4])(REG*) = {nil, bridgerefuse, bridgeaccept, bridgeaccept};
	UWORD input, result;
	int op, mode, shift, alias, value, n, flags;
	unsigned long checks = 0;

	n = leafbridgeemit(nil, LeafMax, (void*)cjitexec, IADDW, nil);
	mappedcheck(n > 0);
	code = (u32int*)leafcodealloc(n*sizeof(u32int));
	mappedcheck(code != nil);
	for(op = 1; op < MAXDIS; op++){
		if(op == IECLR)
			continue;
		optab[op] = bridgehandlers[op];
		for(mode = 0; mode < 4; mode++){
			/* Re-publish only after restoring write permission through a fresh mapping. */
			leafcodefree((uchar*)code, n*sizeof(u32int));
			code = (u32int*)leafcodealloc(n*sizeof(u32int));
			mappedcheck(code != nil && leafbridgeemit(code, n, (void*)cjitexec, op, (void*)fast[mode]) == n);
			mappedcheck(leafcodepublish((uchar*)code, n*sizeof(u32int)) == 0);
			invoke = (int(*)(REG*))code;
			for(shift = 0; shift < 8; shift++)
			for(alias = 0; alias < 4; alias++)
			for(value = 0; value < 8; value++){
				memset(cells, 0xa5, sizeof(cells));
				memset(&R, 0xa5, sizeof(R));
				input = ((UWORD)1 << (value*4)) ^ (UWORD)-value;
				R.s = cells+16+shift;
				R.d = alias == 0 ? cells+64+shift : alias == 1 ? R.s : alias == 2 ? (uchar*)R.s+1 : (uchar*)R.s-1;
				R.m = (void*)1;
				R.PC = bridgetarget;
				memmove(R.s, &input, sizeof(input));
				memmove(before, cells, sizeof(before));
				memmove(expected, cells, sizeof(expected));
				memmove(&saved, &R, sizeof(saved));
				memmove(&wanted, &R, sizeof(wanted));
				bridgeselected = op;
				bridgemode = mode;
				bridgefastcalls = bridgeccalls = 0;
				result = mode >= 2 ? input+31 : input^(UWORD)(op+1);
				memmove(expected+((uchar*)R.d-cells), &result, sizeof(result));
				wanted.PC = &bridgetarget[mode >= 2 ? 1 : mode&1];
				wanted.IC = mode >= 2 || (mode&1) ? 1 : 4;
				wanted.t = mode >= 2 ? -7 : op*17;
				if(mode < 2){ wanted.M = &bridgelink; wanted.MP = bridgelink.MP; }
				feclearexcept(FE_ALL_EXCEPT);
				feraiseexcept(FE_DIVBYZERO);
				flags = fetestexcept(FE_ALL_EXCEPT);
				mappedcheck(invoke(&R) == 1);
				mappedcheck(bridgeccalls == (mode < 2) && bridgefastcalls == (mode != 0));
				mappedcheck(!memcmp(&R, &wanted, sizeof(R)) && !memcmp(cells, expected, sizeof(cells)));
				mappedcheck(fetestexcept(FE_ALL_EXCEPT) == flags);
				/* The direct shared API rejects foreign REG identities without reads. */
				memmove(&other, &saved, sizeof(other));
				mappedcheck(cjitexec(&other, op, nil) == 0 && !memcmp(&other, &saved, sizeof(other)));
				checks++;
			}
		}
	}
	leafcodefree((uchar*)code, n*sizeof(u32int));
	return checks;
}


/* Check real fast arithmetic, C exception propagation and C-only table ownership. */
static unsigned long
bridgeintegration(void)
{
	Module module = {0}, destination = {0};
	Modlink link = {0};
	Inst instructions[MAXDIS] = {{0}};
	Prog program = {0};
	Jit *j;
	WORD left = 17, right = -5, output = 0;
	uchar image[sizeof(Inst)+8]; // One instruction at every native byte alignment.
	Inst imageinst = {0};
	int i, count, before, shift;
	unsigned long checks = 0;

	count = 0;
	for(i = 1; i < MAXDIS; i++){
		if(i == IECLR)continue;
		instructions[count++].op = i;
		optab[i] = bridgehandlers[i];
	}
	module.prog = instructions;
	module.nprog = count;
	mappedcheck(compile(&module, count, nil) && module.jit);
	j = module.jit;
	for(i = 0; i < 256; i++){
		mappedcheck((j->dispatch[i] != nil) == (i > INOP && i < MAXDIS && i != IECLR));
		checks++;
	}
	R.s = &right; R.m = &left; R.d = &output;
	optab[IADDW] = addw;
	mappedcheck(j->dispatch[IADDW](&R) == 1 && output == 12);
	checks++;
	/* Keep successful native operations on the original direct VM fast route. */
	for(i = 0; instructions[i].op != IADDW; i++)
		;
	link.m = &module;
	link.MP = (uchar*)1;
	program.R.M = &link;
	program.R.PC = &instructions[i];
	program.R.s = &right;
	program.R.m = &left;
	program.R.d = &output;
	program.quanta = 1;
	j->dispatch[IADDW] = bridgeunreachable;
	j->enter = bridgeenter;
	dec[0] = bridgedecode;
	bridgeentered = bridgedecoded = 0;
	output = 0;
	xec(&program);
	mappedcheck(output == 12 && bridgeentered == 1 && bridgedecoded == 1 && program.R.IC == 0 && program.R.PC == &instructions[i+1]);
	checks++;
	imageinst.op = IADDW;
	for(shift = 0; shift < 8; shift++){
		memmove(image+shift, &imageinst, sizeof(imageinst));
		module.prog = (Inst*)(image+shift);
		module.nprog = 1;
		program.R.PC = module.prog;
		bridgeentered = bridgedecoded = 0;
		output = 0;
		xec(&program);
		mappedcheck(output == 12 && bridgeentered == 1 && bridgedecoded == 1);
		mappedcheck(program.R.PC == (Inst*)(image+shift+sizeof(Inst)) && program.R.IC == 0);
		checks++;
	}
	module.prog = instructions;
	module.nprog = count;
	if(!getenv("BRIDGE_NO_ERRORS")){
		for(shift = 0; shift <= sizeof(Inst); shift++){
			program.R.PC = shift == 0 ? nil : (Inst*)((uintptr)0-(uintptr)shift);
			bridgeentered = bridgedecoded = 0;
			raised = nil;
			if(setjmp(boundary) == 0){ xec(&program); mappedcheck(0); }
			mappedcheck(raised == exBounds && bridgeentered == 0 && bridgedecoded == 0);
			checks++;
		}
		optab[IDIVW] = divw;
		right = 0; output = 123;
		raised = nil;
		if(setjmp(boundary) == 0){ j->dispatch[IDIVW](&R); mappedcheck(0); }
		mappedcheck(raised == exZdiv && output == 123);
		checks++;
		optab[IALT] = nil;
		raised = nil;
		if(setjmp(boundary) == 0){ j->dispatch[IALT](&R); mappedcheck(0); }
		mappedcheck(raised == exOp);
		checks++;
		optab[IALT] = bridgehandlers[IALT];
		bridgemode = 9; bridgeselected = IALT;
		raised = nil;
		if(setjmp(boundary) == 0){ j->dispatch[IALT](&R); mappedcheck(0); }
		mappedcheck(raised == bridgeexception);
		checks++;
	}
	j->freecode(j->code, j->size); free(j);
	module.jit = nil;
	/* Execute the real VM loop while the C handler destroys the source Jit/Module. */
	bridgedoomed = calloc(1, sizeof(Module));
	mappedcheck(bridgedoomed != nil);
	instructions[0].op = ILOAD;
	instructions[0].add = 0;
	bridgedoomed->prog = instructions;
	bridgedoomed->nprog = 1;
	mappedcheck(compile(bridgedoomed, 1, nil));
	bridgedoomed->jit->enter = bridgeenter;
	link.m = bridgedoomed;
	link.MP = (uchar*)1;
	bridgetarget[1].op = IALT;
	bridgetarget[1].add = 0;
	destination.prog = &bridgetarget[1];
	destination.nprog = 1;
	mappedcheck(compile(&destination, 1, nil));
	destination.jit->enter = bridgeenter;
	bridgelink.m = &destination;
	bridgelink.MP = (uchar*)2;
	program.R.M = &link;
	program.R.PC = instructions;
	program.quanta = 4;
	dec[0] = bridgedecode;
	optab[ILOAD] = bridgerelease;
	optab[IALT] = bridgefinish;
	before = bridgeccalls;
	bridgeentered = bridgedecoded = 0;
	xec(&program);
	mappedcheck(bridgedoomed == nil && bridgeccalls == before+2 && bridgeentered == 2 && bridgedecoded == 2);
	mappedcheck(program.R.M == &bridgelink && program.R.MP == (uchar*)2 && program.R.PC == &bridgetarget[0] && program.R.IC == 0);
	checks++;
	destination.jit->freecode(destination.jit->code, destination.jit->size);
	free(destination.jit);
	return checks;
}


/* Validate both native WORD models without translating complex C handlers to assembly. */
int
main(void)
{
	unsigned long compiler, routes, integration;

	compiler = bridgecompilerchecks();
	if(getenv("BRIDGE_COMPILER_CASE") || getenv("BRIDGE_PREFLIGHT_ONLY")){
		printf("PASS compiler bridge preflight WORD%zu: %lu cases\n", sizeof(WORD)*8, compiler);
		return 0;
	}
	routes = bridgeroutes();
	integration = bridgeintegration();
	printf("PASS complete C/RX dispatch WORD%zu: %lu routes, %lu compiler, %lu integration checks\n", sizeof(WORD)*8, routes, compiler, integration);
	return 0;
}
