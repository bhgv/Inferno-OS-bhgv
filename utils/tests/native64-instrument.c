/* Isolated profile bookkeeping; the executor below is extracted unchanged from devprof.c. */
typedef struct InstrumentRecord InstrumentRecord;

struct InstrumentRecord
{
	int size; // Complete fixture instruction count.
	Inst *base; // Borrowed current module program.
	unsigned long bucket[256]; // Per-PC execution counts, never retained across VM callbacks.
};

static InstrumentRecord instrumentrecord; // Single-threaded profile table for this owning fixture.


/* Model a LOAD callback that republishes its source register after decoding. */
static void
instrumentload(void)
{
	R.s = H;
}


/* Preserve the decoded notification slot even when the handler changes REG.s. */
static void
instrumentloadprobe(void)
{
	REG saved;
	Inst instruction;
	VmFetch fetched;
	void (*handler)(void);
	void *source;
	uchar frame[64];

	saved = R;
	memset(&instruction, 0, sizeof(instruction));
	instruction.op = ILOAD;
	instruction.add = SRC(AIMM)|DST(AFP);
	memset(&fetched, 0, sizeof(fetched));
	fetched.pc = &instruction;
	fetched.op = instruction.op;
	fetched.add = instruction.add;
	R.PC = &instruction;
	R.FP = frame;
	handler = optab[ILOAD];
	optab[ILOAD] = instrumentload;
	vminstrument(&fetched, &source);
	require(source == (uchar*)&instruction+O(Inst, s) && R.s == H);
	require(R.PC == &instruction+1);
	optab[ILOAD] = handler;
	R = saved;
}


/* Exercise the debugger's source-output convention on the same shared C step.
 * This is a helper probe, not a replacement for an interactive debugger test. */
static void
instrumentsource(VmFetch *fetched, void **unused)
{
	void *source;
	int trusted;

	(void)unused;
	trusted = fetched->op == ILEA && fetched->module->cstep == native64lea8step;
	source = H;
	vminstrument(fetched, &source);
	if(trusted)
		require(source == nil);
	else if(fetched->op == ILEA)
		require(source == R.s);
	else
		require(source != H);
}


/* Resolve profile storage before execution, just like the production coverage loop. */
static InstrumentRecord*
instrumentlook(Module *module, int a, int b, int c, int d)
{
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	require(module->nprog <= 256);
	instrumentrecord.base = module->prog;
	instrumentrecord.size = module->nprog;
	return &instrumentrecord;
}


/* These fixtures contain no SPAWN; reaching its scheduler branch is a failure. */
static Prog*
instrumentchild(int state)
{
	(void)state;
	require(0);
	return nil;
}


/* Reject an unexpected child publication rather than emulating a scheduler. */
static void
instrumentadd(Prog *process)
{
	(void)process;
	require(0);
}

#define Record InstrumentRecord // Use isolated profile storage with the actual coverage executor.
#define profiler 1 // Exercise the active coverage path on every instruction.
#define Pcov 1 // Select coverage rather than sampling in this fixture.
#define mlook instrumentlook // Bind only profile lookup, not instruction execution.
#define delruntail instrumentchild // SPAWN is outside these result-call fixtures.
#define addrun instrumentadd // Fail if a fixture unexpectedly creates a child.
INSTRUMENT_EXECUTOR
#undef Record
#undef profiler
#undef Pcov
#undef mlook
#undef delruntail
#undef addrun
