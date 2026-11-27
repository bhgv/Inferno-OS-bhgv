#define NOPC ((ulong)-1) // Native absent-handler sentinel, not a new wire value.

static Prog *handlerprocess; // Live process used by the actual handler lookup.
static uchar *handlerroot; // Original caller frame retained through error recovery.
static int borrowregisters, oldreleased, argumentreleased; // Real destructor register borrowing and release counts.


/* Supply the scheduler lookup while retaining actual process/frame ownership. */
Prog*
currun(void)
{
	return handlerprocess;
}


/* Exercise a real heap destructor after publication of nil in the old slot. */
static void
oldslotfree(Heap *heap, int swept)
{
	void *owner;

	memmove(&owner, handlerroot+96, sizeof(owner));
	require(owner == H);
	freeheap(heap, swept);
	oldreleased++;
	if(borrowregisters & 1)
		memset(&R, 0x66, sizeof(R));
}


/* Release the final argument owner through a real callback before resuming. */
static void
argumentfree(Heap *heap, int swept)
{
	require(heap->ref == 0);
	freeheap(heap, swept);
	argumentreleased++;
	if(borrowregisters & 2)
		memset(&R, 0x77, sizeof(R));
}


/* Retain bounded tracked allocation while exercising real diagnostic ownership. */
static void*
handlerresize(void *previous, size_t bytes)
{
	void *result;

	result = trackedallocate(bytes, 1);
	free(previous);
	return result;
}


/* Formatting details are outside the frame and exception-owner contract. */
int
snprint(char *destination, int bytes, char *format, ...)
{
	(void)format;
	return snprintf(destination, bytes, "trusted handler");
}


/* Supply only the diagnostic name; the actual handler computes its own PCs. */
WORD
modstatus(REG *registers, char *name, int bytes)
{
	(void)registers;
	snprintf(name, bytes, "trusted");
	return 0;
}


/* Every selected case already holds an actual heap String exception owner. */
static String*
newestring(char *text)
{
	(void)text;
	require(0);
	return H;
}

#define realloc handlerresize // Keep diagnostic allocations within the guarded arena.
HANDLER_IMPLEMENTATION
#undef realloc


/* Drop the caller retain only after both argument frames have been prepared. */
static Module*
handlermodule(int jit, int grow, int failure, int padding, int route)
{
	Module *module;
	Inst *previous, *program;

	module = callmodule(0, grow, failure, padding);
	previous = module->prog;
	program = mallocz(16*sizeof(Inst), 1);
	require(program != nil);
	memmove(program, previous, 4*sizeof(Inst));
	memmove(program+5, previous+4, 11*sizeof(Inst));
	callinstruction(program+4, IMOVP, AFP, AFP, 104, 88);
	if(route)
		program[1].s.ind = 104;
	program[7].d.ins = failure == 3 ? nil : program+13;
	program[10].d.ins = program+13;
	module->prog = module->entry = program;
	module->nprog = 16;
	free(previous);
	if(jit)
		require(compile(module, module->nprog, nil));
	return module;
}


/* Select the caller handler after real instruction failure, then resume to IEXIT. */
static void
runcatch(int jit, int grow, int failure, int quanta, int padding, int borrow, int route)
{
	Module *module;
	Modlink *link;
	Type *oldtype, *argumenttype;
	Prog process;
	Frame *entry, *source;
	Except *entries;
	void *owner, *exception, *old, *result, *published, *empty;
	uchar *extent;
	int steps, before;

	module = handlermodule(jit, grow, failure, padding, route);
	module->htab = mallocz(2*sizeof(Handler), 1);
	entries = mallocz(2*sizeof(Except), 1);
	require(module->htab != nil && entries != nil);
	module->htab[0].pc1 = 0;
	module->htab[0].pc2 = 13;
	module->htab[0].eoff = 96;
	module->htab[0].etab = entries;
	entries[0].s = strdup("*");
	entries[0].pc = 12;
	entries[1].pc = NOPC;
	memset(&process, 0, sizeof(process));
	handlerprocess = &process;
	module->ref++;
	link = mklinkmod(module, 0);
	link->MP = module->origmp;
	D2H(link->MP)->ref++;
	process.R.M = link;
	process.R.PC = module->entry;
	entry = mallocz(module->entryt->size, 1);
	require(entry != nil);
	entry->t = module->entryt;
	initmem(module->entryt, entry);
	argumenttype = dtype(argumentfree, sizeof(WORD), nil, 0);
	require(argumenttype != nil);
	owner = H2D(void*, heapz(argumenttype));
	freetype(argumenttype);
	memmove((uchar*)entry+88, &owner, sizeof(owner));
	oldtype = dtype(oldslotfree, sizeof(WORD), nil, 0);
	require(oldtype != nil);
	old = H2D(void*, heapz(oldtype));
	freetype(oldtype);
	memmove((uchar*)entry+96, &old, sizeof(old));
	source = entry;
	R.s = &source;
	newstack(&process);
	free(entry);
	handlerroot = process.R.FP;
	extent = process.R.EX;
	empty = H;
	memmove(handlerroot+104, &empty, sizeof(empty));
	result = handlerroot+80;
	memmove(handlerroot+48, &result, sizeof(result));
	process.quanta = quanta;
	before = raised;
	borrowregisters = borrow;
	oldreleased = argumentreleased = 0;
	for(steps=0; steps<64; steps++)
		if(kprotect(execute, &process) == 0)
			break;
	require(steps < 64 && raised == before+1);
	require(strcmp(message, failure == 3 ? exBounds : exZdiv) == 0);
	require(D2H(owner)->ref == (route ? 1 : 2));
	exception = H2D(void*, heapz(&Tstring));
	process.exval = exception;
	require(handler(message) == 1);
	if(R.FP != handlerroot || R.SP != handlerroot+module->entryt->size ||
	   R.EX != extent || R.M != link || R.MP != link->MP || R.PC != module->prog+12 ||
	   memcmp(&process.R, &R, sizeof(R)) != 0){
		fprintf(stderr, "handler register image lost: jit=%d grow=%d failure=%d borrow=%d\n", jit, grow, failure, borrow);
		exit(11);
	}
	if(argumentreleased != 1){
		fprintf(stderr, "prepared handler owner leaked: jit=%d grow=%d failure=%d\n", jit, grow, failure);
		exit(10);
	}
	memmove(&published, handlerroot+96, sizeof(published));
	require(published == exception && process.exval == H && oldreleased == 1);
	before = raised;
	for(steps=0; steps<64; steps++)
		if(kprotect(execute, &process) == 0)
			break;
	require(steps < 64 && raised == before+1 && message[0] == 0);
	destroystack(&R);
	require(R.EX == nil && R.FP == nil && R.M == H && module->ref == 1);
	free(process.exstr);
	unload(module);
	handlerprocess = nil;
	require(live == 0 && Tstring.ref == 1);
}


/* Execute emu/os handler bodies in both numeric word models and C/RX modes. */
int
main(int argc, char **argv)
{
	int jit, grow, failure, q, padding, borrow, route, cases;
	int quanta[] = {1, 2, 3, 7, 16, 64, 1000};

	Tstring.size = sizeof(String);
	Tstring.free = freeheap;
	Tstring.mark = markheap;
	optab[IFRAME] = frame;
	optab[ICALL] = call;
	optab[IRET] = ret;
	optab[IMOVP] = movp;
	optab[IMOVW] = movw;
	optab[IMOVL] = movl;
	optab[IADDW] = addw;
	optab[IDIVW] = divw;
	optab[IEXIT] = iexit;
	if(argc == 3){
		runcatch(atoi(argv[1]), 0, 2, 1, 0, atoi(argv[2]) == 2, 0);
		return 0;
	}
	require(argc == 1);
	cases = 0;
	for(jit=0; jit<2; jit++)
		for(grow=0; grow<2; grow++)
			for(failure=1; failure<=3; failure++)
				for(q=0; q<(int)nelem(quanta); q++)
					for(padding=0; padding<=8; padding+=8)
						for(borrow=0; borrow<4; borrow++)
							for(route=0; route<2; route++){
								runcatch(jit, grow, failure, quanta[q], padding, borrow, route);
								cases++;
							}
	printf("PASS trusted handler resume: %d cases WORD%d; actual heap/calls/handler/C/RX; file ABI unchanged\n", cases, (int)(8*sizeof(WORD)));
	return 0;
}
