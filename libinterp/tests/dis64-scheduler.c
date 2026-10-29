static Prog *observed; // Process whose terminal marker is captured before its allocation is freed.
static int terminalpid, terminalstate, terminals, exitrequests, releases, acquires; // Scheduler lifecycle observations.
static Pgrp rootpgrp; // Retained host namespace boundary, shared by fixture root processes.
static Fgrp rootfgrp; // Retained host file-group boundary.
static Egrp rootegrp; // Retained host environment-group boundary.
static Module *starting; // Module reference consumed by the next protected schedmod call.
static Prog *started; // Published result of the actual scheduler startup.
static int expecterror; // Zero selects IEXIT; one division error; two invalid shift count.
static WORD expectedvalue; // Independently expected result in the retained instance MP.
int cflag, keepbroken;
char Eintr[]="interrupted", Enomem[]="memory";


/* Record the actual terminal marker only while the Prog allocation is still live. */
static void
schedulerfree(void *pointer)
{
	if(pointer == observed && observed != nil){
		terminalpid = observed->pid;
		terminalstate = observed->state;
		require(*pidlook(terminalpid) == nil);
		observed = nil;
		terminals++;
	}
}


/* Preserve Inferno's zeroing malloc contract, including its allocation failures. */
static void*
schedmalloc(size_t bytes)
{
	return trackedallocate(bytes, 1);
}

#define malloc(bytes) schedmalloc(bytes) // Actual newprog/newgrp allocation boundary.


/* Supply bounded OS error-string publication to the actual error handler. */
void
kstrcpy(char *destination, char *source, int bytes)
{
	require(bytes > 0);
	snprintf(destination, bytes, "%s", source);
}


/* Supply the host formatter used by actual terminal process messages. */
int
snprint(char *destination, int bytes, char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = vsnprintf(destination, bytes, format, arguments);
	va_end(arguments);
	return result;
}


/* Preserve diagnostics without interpreting Inferno-specific formats in unused branches. */
int
print(char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = vfprintf(stderr, format, arguments);
	va_end(arguments);
	return result;
}


/* Unexpected runtime panics fail the fixture instead of being mistaken for IEXIT. */
void
panic(char *format, ...)
{
	fprintf(stderr, "unexpected scheduler panic: %s\n", format);
	abort();
}


/* Only single-threaded reference ownership is modeled at the host boundary. */
int
incref(Ref *reference)
{
	return ++reference->ref;
}


/* Account namespace closure after the process has left the execution quantum. */
void
closepgrp(Pgrp *group)
{
	require(group == &rootpgrp && group->r.ref > 1);
	group->r.ref--;
}


/* Account file-group closure without opening host descriptors. */
void
closefgrp(Fgrp *group)
{
	require(group == &rootfgrp && group->r.ref > 1);
	group->r.ref--;
}


/* Account environment closure without invoking host environment services. */
void
closeegrp(Egrp *group)
{
	require(group == &rootegrp && group->r.ref > 1);
	group->r.ref--;
}


/* No signing-key owner is supplied by this scalar execution fixture. */
void
closesigs(Skeyset *keys)
{
	require(keys == nil);
}


/* Root processes have neither debugger nor parent waiter queues. */
int
qproduce(Queue *queue, void *data, int bytes)
{
	(void)queue;
	(void)data;
	(void)bytes;
	require(0);
	return 0;
}


/* Save and remove the actual current process at the OS release boundary. */
void
release(void)
{
	require(worker.iprog == nil && currun() != nil);
	worker.iprog = isave();
	releases++;
}


/* Resume the saved actual process using the production restore and queue helpers. */
void
acquire(void)
{
	Prog *process;

	process = worker.iprog;
	require(process != nil);
	worker.iprog = nil;
	irestore(process);
	pushrun(process);
	acquires++;
}


/* Observe the final host-exit request while allowing ownership checks afterward. */
void
cleanexit(int status)
{
	require(status == 0 && isched.head == nil && isched.runhd == nil);
	exitrequests++;
}


/* Keep the ordinary broken-process retention decision independent of host pressure. */
int
memusehigh(void)
{
	return 0;
}


/* Debugger-driven process retention is outside this root-process fixture. */
void
dbgexit(Prog *process, int broken, char *message)
{
	(void)process;
	(void)broken;
	(void)message;
	require(0);
}


/* Root tests do not exercise native FP environment switching in the OS worker. */
void
FPsave(void *state)
{
	(void)state;
}


/* Root tests do not exercise native FP environment restoration in the OS worker. */
void
FPrestore(void *state)
{
	(void)state;
}


/* Keep the unused child-environment allocation path linkable and owned. */
void
kstrdup(char **destination, char *source)
{
	*destination = trackedstrdup(source);
	if(*destination == nil)
		error(exNomem);
}

/* Scalar MP clones never acquire nested arrays; mapped cloning has separate tests. */
Array*
arraycpy(Array *array)
{
	(void)array;
	require(0);
	return nil;
}


/* Dynamic modules never enter the validated scalar pilot linker. */
void
newdyndata(Modlink *module)
{
	(void)module;
	require(0);
}


SCHEDULER_IMPLEMENTATION


typedef struct NativeObservation NativeObservation;

struct NativeObservation {
	Jit *jit; // Current sidecar identity; never dereferenced after module destruction.
	int (*entry[256])(REG*); // Actual emitted targets behind counting wrappers.
};

static NativeObservation nativeobservations[4]; // At most two concurrently owned test modules.
static int nativecalls, nativeaccepted, nativedeclined, ccalls, bridgecalls; // Observed real dispatch routes.


/* Count an actual generated leaf invocation before returning its genuine result. */
static int
countnative(REG *registers)
{
	Jit *jit;
	int i, op, result;

	require(registers == &R);
	jit = registers->M->m->jit;
	op = registers->PC[-1].op;
	for(i=0; i<nelem(nativeobservations) && nativeobservations[i].jit != jit; i++)
		;
	require(i < nelem(nativeobservations) && nativeobservations[i].entry[op] != nil);
	nativecalls++;
	result = nativeobservations[i].entry[op](registers);
	if(result)
		nativeaccepted++;
	else
		nativedeclined++;
	return result;
}


/* Observe complete native-tail dispatch while retaining the actual C bridge body. */
int
cjitexec(REG *registers, int opcode, int (*fast)(REG*))
{
	bridgecalls++;
	return actualcjitexec(registers, opcode, fast);
}


/* Count ordinary C MOVW execution without changing the production handler. */
static void
countmovw(void)
{
	ccalls++;
	actual_movw();
}


/* Count ordinary C ADDW execution without changing the production handler. */
static void
countaddw(void)
{
	ccalls++;
	actual_addw();
}


/* Count the authoritative divide handler, including native zero-divisor declines. */
static void
countdivw(void)
{
	ccalls++;
	actual_divw();
}


/* Count invalid shift fallback without replacing the production count check. */
static void
countshlw(void)
{
	ccalls++;
	actual_shlw();
}


/* Count the actual terminal instruction through C or its complete native tail. */
static void
countexit(void)
{
	ccalls++;
	actual_iexit();
}


/* Wrap only real emitted leaf addresses; complete native tails remain unchanged. */
static void
observedispatch(Module *module, int requested)
{
	int i, opcode;

	require((module->jit != nil) == requested);
	if(!requested)
		return;
	for(i=0; i<nelem(nativeobservations) && nativeobservations[i].jit != nil &&
	    nativeobservations[i].jit != module->jit; i++)
		;
	/* Finished module identities may differ between allocator reuse cycles. */
	if(i == nelem(nativeobservations))
		for(i=0; i<nelem(nativeobservations); i++)
			if(nativeobservations[i].jit != (currun() == nil ? nil : currun()->R.M->m->jit))
				break;
	require(i < nelem(nativeobservations));
	nativeobservations[i].jit = module->jit;
	for(opcode=0; opcode<256; opcode++){
		nativeobservations[i].entry[opcode] = module->jit->entry[opcode];
		if(module->jit->entry[opcode] != nil)
			module->jit->entry[opcode] = countnative;
	}
	require(module->jit->dispatch[IEXIT] != nil);
}


/* Construct a root using the actual protected scheduler startup. */
static int
start(void *unused)
{
	(void)unused;
	started = schedmod(starting);
	return 1;
}


/* Run the exact published Prog execution entry until actual IEXIT/error transfer. */
static int
executeprocess(void *argument)
{
	Prog *process;

	process = argument;
	process->xec(process);
	return 1;
}


/* Initialize the root OS environment at the same boundary used by disinit. */
static void
rootenvironment(Prog *process)
{
	Osenv *environment;

	environment = process->osenv;
	memmove(environment, &worker.defenv, sizeof(*environment));
	incref(&environment->pgrp->r);
	incref(&environment->fgrp->r);
	incref(&environment->egrp->r);
	environment->user = trackedstrdup("dis64-root");
	require(environment->user != nil);
	environment->errstr = environment->errbuf0;
	environment->syserrstr = environment->errbuf1;
	worker.env = environment;
}


/* Verify that all real queue/PID roots have been detached after terminal cleanup. */
static void
emptyqueues(void)
{
	unsigned int i;

	require(nprog() == 0 && isched.head == nil && isched.tail == nil);
	require(isched.runhd == nil && isched.runtl == nil);
	for(i=0; i<nelem(proghash); i++)
		require(proghash[i] == nil);
	require(worker.iprog == nil && worker.nerr == 0 && gchalt == 0);
}


/* Execute a real root process, retain its result owner, and invoke actual progexit. */
static int
lifecycle(uchar *image, unsigned int bytes, int jit, int broken)
{
	Prog *process;
	void *snapshot;
	WORD value;
	int pid, before, count, natives, helpers, tails, accepted, declined;

	starting = prepare64pilot("scheduler", image, bytes, jit);
	require(starting != nil);
	observedispatch(starting, jit);
	allocations = 0;
	started = nil;
	require(kprotect(start, nil));
	count = allocations;
	process = started;
	rootenvironment(process);
	require(process == currun() && process->state == Pready && process->xec == xec);
	require(nprog() == 1 && *pidlook(process->pid) == process);
	require(process->R.PC == process->R.M->m->entry && process->R.SP <= process->R.TS);
	require(process->R.M->MP != process->R.M->m->origmp);
	snapshot = process->R.M->MP;
	D2H(snapshot)->ref++;
	before = terminals;
	pid = process->pid;
	observed = process;
	natives = nativecalls;
	helpers = ccalls;
	tails = bridgecalls;
	accepted = nativeaccepted;
	declined = nativedeclined;
	require(kprotect(executeprocess, process) == 0);
	require(nativecalls-natives == jit);
	require(nativeaccepted-accepted == (jit && !expecterror));
	require(nativedeclined-declined == (jit && expecterror));
	require(ccalls-helpers == (expecterror || jit ? 1 : 2));
	require(bridgecalls-tails == (jit && !expecterror));
	require(expecterror ? strcmp(worker.env->errstr, expecterror == 1 ? exZdiv : exInval) == 0 : worker.env->errstr[0] == 0);
	if(broken){
		strcpy(worker.env->errstr, "fixture arithmetic failure");
		keepbroken = 1;
		progexit();
		require(process->state == Pbroken && nprog() == 1 && currun() == nil);
		require(*pidlook(pid) == process && terminals == before);
		strcpy(worker.env->errstr, "fail: fixture cleanup");
		keepbroken = 0;
		R = process->R;
		pushrun(process);
	}
	progexit();
	/* The original process allocation is gone; inspect only captured terminal metadata. */
	worker.env = &worker.defenv;
	require(terminals == before+1 && terminalpid == pid && (uint)terminalstate == 0xdeadbeefU);
	emptyqueues();
	require(rootpgrp.r.ref == 1 && rootfgrp.r.ref == 1 && rootegrp.r.ref == 1);
	memmove(&value, snapshot, sizeof(value));
	require(value == expectedvalue && D2H(snapshot)->ref == 1);
	destroy(snapshot);
	require(live == 0 && Tmodlink.ref == 1);
	return count;
}


/* Fail each actual startup allocation before any runnable process can escape. */
static void
startupfailures(uchar *image, unsigned int bytes, int jit, int count)
{
	int failure;

	for(failure=1; failure<=count; failure++){
		starting = prepare64pilot("scheduler", image, bytes, jit);
		require(starting != nil);
		allocations = 0;
		failat = failure;
		started = nil;
		require(kprotect(start, nil) == 0 && started == nil);
		failat = 0;
		emptyqueues();
		require(live == 0 && Tmodlink.ref == 1);
	}
}


/* Reject malformed but still owned roots before MP cloning and process publication. */
static void
entryrejections(uchar *image, unsigned int bytes)
{
	Module saved;
	Module *owner;
	int scenario, baseline, framesize, framepointers;

	for(scenario=0; scenario<10; scenario++){
		owner = prepare64pilot("reject", image, bytes, 0);
		require(owner != nil);
		owner->ref++;
		saved = *owner;
		framesize = owner->entryt->size;
		framepointers = owner->entryt->np;
		switch(scenario){
		case 0: owner->entry = nil; break;
		case 1: owner->entry = owner->prog+owner->nprog; break;
		case 2: owner->entry = (Inst*)((uchar*)owner->prog+1); break;
		case 3: owner->nprog = 0; break;
		case 4: owner->nprog = -1; break;
		case 5: owner->entryt = nil; break;
		case 6: owner->entryt = H; break;
		case 7: owner->entryt->size = sizeof(Frame)-1; break;
		case 8: owner->entryt->np = -1; break;
		case 9: owner->ss = 0x80000000UL; break;
		}
		starting = owner;
		started = nil;
		baseline = live;
		allocations = 0;
		require(!kprotect(start, nil) && started == nil && allocations == 0);
		require(owner->ref == 1 && live == baseline);
		saved.entryt->size = framesize;
		saved.entryt->np = framepointers;
		*owner = saved;
		owner->ref = 1;
		unload(owner);
		emptyqueues();
		require(live == 0);
	}
}


/* Preserve an existing runnable root when admission of another module fails. */
static void
multipleprocesses(uchar *image, unsigned int bytes, int jit)
{
	Prog *processes[2];
	void *snapshots[2];
	Module *rejected;
	Inst *entry;
	WORD value;
	int i, before, baseline;

	for(i=0; i<2; i++){
		starting = prepare64pilot("queue", image, bytes, jit);
		require(starting != nil);
		observedispatch(starting, jit);
		require(kprotect(start, nil));
		processes[i] = started;
		rootenvironment(processes[i]);
		worker.env = &worker.defenv;
		snapshots[i] = processes[i]->R.M->MP;
		D2H(snapshots[i])->ref++;
		if(i == 0){
			rejected = prepare64pilot("reject-beside-ready", image, bytes, 0);
			require(rejected != nil);
			rejected->ref++;
			entry = rejected->entry;
			rejected->entry = nil;
			starting = rejected;
			started = nil;
			baseline = live;
			allocations = 0;
			require(!kprotect(start, nil) && started == nil && allocations == 0);
			require(live == baseline && rejected->ref == 1);
			require(nprog() == 1 && currun() == processes[0] && isched.runtl == processes[0]);
			require(*pidlook(processes[0]->pid) == processes[0]);
			rejected->entry = entry;
			unload(rejected);
		}
	}
	require(nprog() == 2 && currun() == processes[0] && isched.runtl == processes[1]);
	before = exitrequests;
	for(i=0; i<2; i++){
		worker.env = processes[i]->osenv;
		observed = processes[i];
		require(currun() == processes[i] && !kprotect(executeprocess, processes[i]));
		progexit();
		worker.env = &worker.defenv;
		require((uint)terminalstate == 0xdeadbeefU && nprog() == 1-i);
		require(exitrequests == before+i);
		memmove(&value, snapshots[i], sizeof(value));
		require(value == expectedvalue && D2H(snapshots[i])->ref == 1);
		destroy(snapshots[i]);
	}
	emptyqueues();
	require(live == 0 && rootpgrp.r.ref == 1 && rootfgrp.r.ref == 1 && rootegrp.r.ref == 1);
}


/* Reject invalid owner representations without attempting to consume a reference. */
static void
invalidowners(void)
{
	Module *invalid[] = {nil, H, (Module*)1, (Module*)((uintptr)-1-7)};
	unsigned int i;

	for(i=0; i<nelem(invalid); i++){
		starting = invalid[i];
		started = nil;
		allocations = 0;
		require(!kprotect(start, nil) && started == nil && allocations == 0);
		emptyqueues();
		require(live == 0);
	}
}


/* Retain opaque native-PC entry semantics without executing a guessed address. */
static void
nativeentry(uchar *image, unsigned int bytes)
{
	Module *module;
	Module saved;

	module = prepare64pilot("native-admission", image, bytes, 0);
	require(module != nil);
	saved = *module;
	module->compiled = 1;
	module->prog = nil;
	module->nprog = 0;
	module->entry = (Inst*)0x1000;
	allocations = 0;
	require(vmstartup(module) == saved.entryt && allocations == 0);
	*module = saved;
	unload(module);
	require(live == 0);
}


/* Test actual scheduling/terminal state with independent immutable golden images. */
int
main(int argc, char **argv)
{
	uchar image[4096], original[4096];
	unsigned int bytes;
	FILE *file;
	int jit, count, repeat;

	require(argc == 3 && sizeof(WORD) == 4 && sizeof(void*) == 8);
	expecterror = atoi(argv[2]);
	expectedvalue = expecterror ? 40 : 42;
	rootpgrp.r.ref = rootfgrp.r.ref = rootegrp.r.ref = 1;
	worker.defenv.pgrp = &rootpgrp;
	worker.defenv.fgrp = &rootfgrp;
	worker.defenv.egrp = &rootegrp;
	worker.defenv.errstr = worker.defenv.errbuf0;
	worker.defenv.syserrstr = worker.defenv.errbuf1;
	worker.env = &worker.defenv;
	worker.type = Interp;
	file = fopen(argv[1], "rb");
	require(file != nil);
	bytes = fread(image, 1, sizeof(image), file);
	require(!ferror(file) && feof(file) && fclose(file) == 0);
	memmove(original, image, bytes);
	HANDLERS
	for(jit=0; jit<2; jit++){
		for(repeat=0; repeat<3; repeat++)
			count = lifecycle(image, bytes, jit, 0);
		startupfailures(image, bytes, jit, count);
		lifecycle(image, bytes, jit, 1);
		multipleprocesses(image, bytes, jit);
	}
	entryrejections(image, bytes);
	invalidowners();
	nativeentry(image, bytes);
	require(memcmp(original, image, bytes) == 0 && releases == acquires);
	printf("PASS Dis64 scheduler: %d predicates, %d terminal processes, %d final-exit requests; actual schedmod/newprog/queues/progexit/delprog C/RX; native=%d accepted=%d declined=%d C=%d tails=%d; OS worker/GC mocked\n", checks, terminals, exitrequests, nativecalls, nativeaccepted, nativedeclined, ccalls, bridgecalls);
	return 0;
}
