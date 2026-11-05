static Prog *callprocess; // Active process for the production call handler.


/* Supply the active process through the scheduler lookup boundary. */
Prog*
currun(void)
{
	return callprocess;
}


/* Execute one real quantum and select its current module's RX span. */
static int
callquantum(Prog *process)
{
	Jit *jit;

	jit = process->R.M->m->jit;
	if(jit != nil){
		rxbegin = (uintptr)jit->code;
		rxend = rxbegin+jit->size;
	}
	return kprotect(execute, process);
}


/* Transfer the provider root to the consumer, then check calls across GC epochs. */
static void
providercall(uchar *image, size_t bytes, uchar *consumer, size_t consumerbytes, int jit, int failure, int grow)
{
	Modlink *provider, *caller;
	Frame *entry, *pending;
	Prog process;
	Type *scalar;
	Heap *child;
	void *value;
	uchar *root;
	int before, slot, step;
	WORD result;

	caller = prepare64consumerlink("numeric-consumer", consumer, consumerbytes,
		"numeric-provider", image, bytes, jit);
	require(caller != nil);
	numericcollect(nil);
	memmove(&provider, caller->MP, sizeof(provider));
	require(provider != nil && provider != H);
	require(numericowner(D2H(provider))->magic == MAGIC_A);
	caller->m->ss = grow ? 0 : 4096;
	memset(&process, 0, sizeof(process));
	process.exval = H;
	process.state = Pready;
	process.quanta = 1;
	process.R.M = caller;
	process.R.PC = caller->m->entry;
	entry = mallocz(caller->m->entryt->size, 1);
	require(entry != nil);
	entry->t = caller->m->entryt;
	R.s = &entry;
	newstack(&process);
	initmem(caller->m->entryt, process.R.FP);
	free(entry);
	root = process.R.FP;
	callprocess = &process;
	ptrdel(D2H(caller));

	require(callquantum(&process) == 1 && process.R.M == caller);
	memmove(&pending, root+64, sizeof(pending));
	require(pending != nil && (pending->t == nil) == grow);
	if(pending->t == nil)
		require(((Stkext*)process.R.EX)->reg.TR == provider->links[0].frame);
	else
		require(pending->t == provider->links[0].frame);
	memmove(&value, (uchar*)pending+64, sizeof(value));
	require(value == H);
	scalar = dtype(freeheap, sizeof(WORD), nil, 0);
	require(scalar != nil);
	child = heapz(scalar);
	freetype(scalar);
	value = H2D(void*, child);
	memmove((uchar*)pending+64, &value, sizeof(value));
	numericcollect(&process);
	require(numericowner(child)->magic == MAGIC_A);

	require(callquantum(&process) == 1 && process.R.M == provider);
	require(process.R.FP == (uchar*)pending && pending->mr == caller && pending->fp == root);
	require(process.R.PC == provider->links[0].u.pc && D2H(provider)->ref == 2);
	for(step=1; step<=2; step++){
		numericcollect(&process);
		require(numericowner(child)->magic == MAGIC_A);
		require(numericowner(D2H(caller))->magic == MAGIC_A);
		before = rxcompleted;
		require(callquantum(&process) == 1 && process.R.M == provider);
		if(jit)
			require(rxcompleted == before+1);
		memmove(&result, provider->MP+8, sizeof(result));
		require(result == 40+2*step);
	}
	if(failure){
		require(callquantum(&process) == 0 && R.M == provider);
		require(strcmp(message, exZdiv) == 0);
	}else{
		require(callquantum(&process) == 1 && process.R.M == caller);
		require(process.R.PC == caller->prog+2 && process.R.FP == root);
		require(numericowner(child)->magic == MAGIC_F && D2H(provider)->ref == 1);
		require(callquantum(&process) == 0 && R.M == caller);
		require(message[0] == 0);
	}
	destroystack(&R);
	numericdrain();
	require(R.M == H && R.FP == nil && R.EX == nil && worker.nerr == 0);
	require(live == 0 && Tmodlink.ref == 1);
	for(slot=1; slot<GcSlots; slot++)
		require(numericblock(slot)->magic == MAGIC_F);
}


typedef struct F_Sys_millisec F_Sys_millisec;

struct F_Sys_millisec {
	Frame regs; // Native Frame64 prefix.
	WORD *ret; // Borrowed caller result address at byte 32.
	uchar temps[24]; // Reserved scalar bytes.
};

static int clockreads; // Observe exactly one production handler call per test.


/* Return a deterministic value from the hosted clock boundary. */
long
osmillisec(void)
{
	clockreads++;
	return 123456;
}


CLOCK_HANDLER


#include "native64-entry.h"

static const uchar clockmap[] = {0}; // Complete map of the fixed clock frame.
static const LimboNative64Entry limbo_native64_Sys_millisec_entry = {
	"millisec", Sys_millisec, 0x616977e8U, 64, sizeof(clockmap), clockmap
}; // Generated Frame64 ABI mirrored for this isolated scheduler fixture.
static const LimboNative64Entry *const native64sysio[] = {
	&limbo_native64_Sys_millisec_entry,
}; // Only the clock export is needed by this fixture.

CLOCK_RESOLVER


/* Exercise the real VM stack, native result binding and builtin MCALL. */
static void
clockcall(uchar *image, size_t bytes, int jit, int grow)
{
	Dis64StorageOwner *lease;
	Module sys, *module;
	Type *frame;
	Link export;
	Modlink *caller, *clock;
	Frame *entry, *pending, *wrong;
	Prog process;
	uchar *root;
	WORD stamp;
	int before, slot;

	frame = dtype(freeheap, 64, (uchar*)clockmap, sizeof(clockmap));
	require(frame != nil && sizeof(F_Sys_millisec) == 64 && O(F_Sys_millisec, ret) == 32);
	memset(&sys, 0, sizeof(sys));
	sys.ref = 1;
	sys.path = "$Sys";
	sys.origmp = H;
	sys.ntype = 1;
	sys.type = &frame;
	sys.ext = &export;
	export.name = "millisec";
	export.sig = 0x616977e8U;
	export.frame = frame;
	export.u.runt = Sys_millisec;
	modules = &sys;
	clock = prepare64sysclocklink(image, bytes, 0);
	require(clock != nil && sys.ref == 2);
	lease = nil;
	module = dis64numericbuild("clock-caller", image, bytes, 0, &lease, nil, 0, 1, nil);
	require(module != nil && module->nprog == 5);
	/* The fixture inserts the existing native result bind after decoding. */
	module->prog[2] = module->prog[1];
	module->prog[0].d.ind = 40;
	module->prog[2].s.ind = 40;
	memset(&module->prog[1], 0, sizeof(Inst));
	module->prog[1].op = ILEA;
	module->prog[1].add = SRC(AFP)|DST(AFP|AIND);
	module->prog[1].s.ind = 72;
	module->prog[1].d.i.f = 40;
	module->prog[1].d.i.s = 32;
	module->prog[3] = module->prog[4];
	module->nprog = 4;
	module->cstep = native64lea8step;
	require(!jit || compile(module, module->nprog, nil));
	caller = dis64rootlink(module, lease, 0, nil, nil);
	require(caller != nil);
	memmove(caller->MP, &clock, sizeof(clock));
	Setmark(D2H(clock));
	ptrdel(D2H(clock));
	caller->m->ss = grow ? 0 : 4096;
	memset(&process, 0, sizeof(process));
	process.exval = H;
	process.state = Pready;
	process.quanta = 1;
	process.R.M = caller;
	process.R.PC = caller->m->entry;
	entry = mallocz(caller->m->entryt->size, 1);
	require(entry != nil);
	entry->t = caller->m->entryt;
	R.s = &entry;
	newstack(&process);
	initmem(caller->m->entryt, process.R.FP);
	free(entry);
	root = process.R.FP;
	callprocess = &process;
	ptrdel(D2H(caller));
	before = clockreads;
	require(callquantum(&process) == 1 && process.R.M == caller);
	memmove(&pending, root+40, sizeof(pending));
	require(pending != nil && (pending->t == nil) == grow);
	/* Reject a plausible aligned interior address before publishing a result pointer. */
	wrong = (Frame*)((uchar*)pending+sizeof(void*));
	memmove(root+40, &wrong, sizeof(wrong));
	require(callquantum(&process) == 0 && strcmp(message, exBounds) == 0);
	require(clockreads == before && process.R.PC == caller->prog+1);
	memmove(root+40, &pending, sizeof(pending));
	numericcollect(&process);
	require(callquantum(&process) == 1 && process.R.M == caller);
	require(((F_Sys_millisec*)pending)->ret == (WORD*)(root+72));
	numericcollect(&process);
	require(callquantum(&process) == 1 && process.R.M == caller && process.R.FP == root);
	memmove(&stamp, root+72, sizeof(stamp));
	require(stamp == 123456 && clockreads == before+1 && D2H(clock)->ref == 1);
	require(callquantum(&process) == 0 && message[0] == 0);
	destroystack(&R);
	numericdrain();
	require(sys.ref == 1 && worker.nerr == 0 && Tmodlink.ref == 1);
	modules = nil;
	freetype(frame);
	require(live == 0);
	for(slot=1; slot<GcSlots; slot++)
		require(numericblock(slot)->magic == MAGIC_F);
}


/* Exercise real C and JIT call/return and error paths with the same raw provider. */
int
main(int argc, char **argv)
{
	uchar image[4096], consumer[4096];
	size_t bytes, consumerbytes;
	int jit, failure, grow, slot;

	require(argc == 4);
	bytes = numericread(argv[1], image, sizeof(image));
	consumerbytes = numericread(argv[2], consumer, sizeof(consumer));
	failure = strcmp(argv[3], "error") == 0;
	numericpoolinit();
	if(strncmp(argv[3], "reject", 6) == 0){
		require(prepare64consumerlink("numeric-consumer", consumer, consumerbytes,
			"numeric-provider", image, bytes, 1) == nil);
		if(strcmp(argv[3], "reject-early") == 0)
			require(allocations == 0);
		numericdrain();
		require(live == 0 && Tmodlink.ref == 1 && worker.nerr == 0);
		for(slot=1; slot<GcSlots; slot++)
			require(numericblock(slot)->magic == MAGIC_F);
		printf("PASS linked consumer refusal and cleanup\n");
		return 0;
	}
	optab[IMFRAME] = mframe;
	optab[IMCALL] = mcall;
	optab[IRET] = ret;
	optab[IADDW] = addw;
	optab[IDIVW] = divw;
	optab[IEXIT] = iexit;
	if(strcmp(argv[3], "clock") == 0){
		for(jit=0; jit<=1; jit++)
			for(grow=0; grow<=1; grow++)
				clockcall(consumer, consumerbytes, jit, grow);
		printf("PASS VM Sys clock MFRAME, result bind and C MCALL in four C/JIT stack modes\n");
		return 0;
	}
	for(jit=0; jit<=1; jit++)
		for(grow=0; grow<=1; grow++)
			providercall(image, bytes, consumer, consumerbytes, jit, failure, grow);
	printf("PASS provider MCALL %s: %lu GC epochs, %d checks\n",
		failure ? "error unwind" : "RET", gcepochs, checks);
	return 0;
}
