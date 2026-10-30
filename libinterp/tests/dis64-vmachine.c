static sigjmp_buf hostboundary; // Test-only OS stop, outside the VM error stack.
static int vmactive, idlecalls, handlercalls, unhandled; // Observations at real selection boundaries.
static int expectedroots; // Number of terminal processes required before host exit.
static int selectedbroken; // Idle stop retains one actual Pbroken root.
static int lockdepth; // Single-worker startup lock balance, without simulated concurrency.
static int inspecthandler, selectedhandlers, diagnosticfailures; // Legacy in-memory handler injection observations.
static void *exceptionvalue, *previousvalue; // Independently retained exception payload owners.
uvlong gcbusy;


/* Legacy table-injection cases; these names do not assign wire IDs. */
enum {
	TableTransfer, // Transfer into a valid mapped slot.
	TableCleanup, // Clear mapped owners before transferring the exception.
	TableMisaligned, // Reject an unaligned exception destination.
	TableOutside, // Reject an out-of-program named and fallback target.
	TableUnmatched, // Ignore a different exception pattern.
	TableUnmarked, // Reject an aligned destination absent from the frame map.
	TableOversized, // Reject cleanup metadata larger than the live frame.
	TableFallback, // Skip a bad named target and select the valid fallback.
	TableCases // Number of independent legacy injection cases.
};


/* End a bounded run only at the actual final-host-exit request. */
void
cleanexit(int status)
{
	schedulercleanexit(status);
	require(vmactive && terminals == expectedroots && worker.nerr == 0);
	siglongjmp(hostboundary, 1);
}


/* Model only the nonblocking startup lock in this single OS worker. */
void
lock(Lock *lock)
{
	require(lock == &isched.l && lockdepth++ == 0);
}


/* Require startup to release its host lock before executing any Dis process. */
void
unlock(Lock *lock)
{
	require(lock == &isched.l && lockdepth-- == 1);
}


/* Startup selects its existing idle worker; no host thread creation is modeled. */
void
osblock(void)
{
	require(0);
}


/* Stop at the genuine idle sleep after progexit has retained a broken root. */
void
Sleep(Rendez *rendezvous, int (*ready)(void*), void *argument)
{
	require(vmactive && selectedbroken && rendezvous == &isched.irend);
	require(!ready(argument) && currun() == nil && nprog() == 1);
	require(isched.head->state == Pbroken && worker.env == &worker.defenv);
	require(worker.nerr == 1 && lockdepth == 0);
	siglongjmp(hostboundary, 2);
}


/* Observe the real vmachine idle branch without claiming collector execution. */
static void
execatidle(void)
{
	require(selectedbroken && currun() == nil);
	idlecalls++;
}


/* This bounded scalar run never requests pressure-driven collection. */
int
memlow(void)
{
	return 0;
}


/* Fail if a collector epoch escapes the declared scalar fixture boundary. */
void
rungc(Prog *process)
{
	(void)process;
	require(0);
}


/* No external worker is queued during a bounded vmachine run. */
void
iyield(void)
{
	require(0);
}


/* Blocked channel propagation is outside the ready-root profile. */
void
altdone(Alt *channel, Prog *process, Channel *array, int selection)
{
	(void)channel;
	(void)process;
	(void)array;
	(void)selection;
	require(0);
}


/* No blocked sender or receiver is installed in the actual process group. */
void
cqdelp(Progq **queue, Prog *process)
{
	(void)queue;
	(void)process;
	require(0);
}


/* A released worker cannot enter the ready-root propagation scenarios. */
static void
swiprog(Prog *process)
{
	(void)process;
	require(0);
}


/* Notification-only group killing is a separate scheduler policy. */
int
killprog(Prog *process, char *cause)
{
	(void)process;
	(void)cause;
	require(0);
	return 0;
}


/* Injected tables transfer an existing payload; allocating exception strings is outside this fixture. */
String*
c2string(char *text, int bytes)
{
	(void)text;
	(void)bytes;
	require(0);
	return H;
}


/* Exercise the actual handler's retained-diagnostics branch on allocation failure. */
void*
realloc(void *pointer, ulong bytes)
{
	require(inspecthandler && pointer == nil && bytes > 0);
	diagnosticfailures++;
	return nil;
}


/* No frame is uncalled or partially unwound by the table-free root profile. */
void
unextend(Frame *frame)
{
	(void)frame;
	require(0);
}


VMACHINE_IMPLEMENTATION


/* Count actual handler selection, including its real table-free frame traversal. */
int
handler(char *text)
{
	int result;

	handlercalls++;
	result = actualhandler(text);
	if(!result)
		unhandled++;
	else{
		void *slot;

		require(inspecthandler && currun()->exval == H && currun()->kill == nil);
		require(R.PC == R.M->prog+1 && currun()->R.PC == R.PC);
		memmove(&slot, R.FP+64, sizeof(slot));
		require(slot == exceptionvalue && D2H(slot)->ref == 2);
		require(D2H(previousvalue)->ref == 1);
		selectedhandlers++;
	}
	return result;
}


/* Start a real root and retain its instance data independently of Module lifetime. */
static Prog*
vmroot(uchar *image, unsigned int bytes, int jit, void **snapshot)
{
	Prog *process;

	starting = prepare64pilot("vmachine", image, bytes, jit);
	require(starting != nil);
	observedispatch(starting, jit);
	require(kprotect(start, nil));
	process = started;
	rootenvironment(process);
	worker.env = &worker.defenv;
	process->quanta = 1;
	*snapshot = process->R.M->MP;
	D2H(*snapshot)->ref++;
	return process;
}


/* Execute the unchanged vmachine function until a bounded OS boundary is reached. */
static void
runvm(int broken, int deletions)
{
	int stopped;

	require(worker.nerr == 0 && worker.iprog == nil && lockdepth == 0);
	selectedbroken = broken;
	expectedroots = terminals+deletions;
	isched.idle = 1;
	vmactive = 1;
	stopped = sigsetjmp(hostboundary, 1);
	if(stopped == 0){
		vmachine(nil);
		require(0);
	}
	require(stopped == (broken ? 2 : 1));
	/* The bounded OS handoff deliberately abandons vmachine's standing error frame. */
	require(worker.nerr == (broken ? 1 : 0));
	worker.nerr = 0;
	worker.env = &worker.defenv;
	vmactive = 0;
	selectedbroken = 0;
	require(lockdepth == 0 && releases == acquires);
}


/* Check retained MP only after actual selection, cleanup and OS stop have completed. */
static void
result(void *snapshot, WORD expected)
{
	WORD value;

	memmove(&value, snapshot, sizeof(value));
	require(value == expected && D2H(snapshot)->ref == 1);
	destroy(snapshot);
}


/* Capture the running owner before xec can yield, exit or raise an arithmetic error. */
static void
observeexecution(Prog *process)
{
	observed = process;
	xec(process);
}


/* Cover normal/error exits, retained broken roots, and round-robin ready roots. */
static void
scenarios(uchar *image, unsigned int bytes, int jit)
{
	Prog *process, *first, *second;
	void *snapshots[2];
	int before, calls;

	before = terminals;
	calls = handlercalls;
	process = vmroot(image, bytes, jit, &snapshots[0]);
	observed = process;
	runvm(0, 1);
	require(terminals == before+1 && (uint)terminalstate == 0xdeadbeefU);
	require(handlercalls == calls+1);
	result(snapshots[0], expectedvalue);
	emptyqueues();
	require(live == 0);

	first = vmroot(image, bytes, jit, &snapshots[0]);
	second = vmroot(image, bytes, jit, &snapshots[1]);
	require(currun() == first && isched.runtl == second);
	/* Observe every freed process by updating the current owner at execution entry. */
	first->xec = second->xec = observeexecution;
	before = terminals;
	calls = handlercalls;
	runvm(0, 2);
	require(terminals == before+2 && handlercalls == calls+2);
	result(snapshots[0], expectedvalue);
	result(snapshots[1], expectedvalue);
	emptyqueues();
	require(live == 0);

	if(expecterror){
		process = vmroot(image, bytes, jit, &snapshots[0]);
		observed = process;
		before = terminals;
		keepbroken = 1;
		runvm(1, 0);
		require(terminals == before && process->state == Pbroken);
		require(*pidlook(process->pid) == process && process->exval == H);
		require(process->R.PC == process->R.M->m->prog+1);
		require(strcmp(((Osenv*)process->osenv)->errstr, expecterror == 1 ? exZdiv : exInval) == 0);
		/* Re-enter actual vmachine with a terminal kill; no direct progexit call. */
		keepbroken = 0;
		process->kill = "fail: retained root cleanup";
		pushrun(process);
		runvm(0, 1);
		require(terminals == before+1);
		result(snapshots[0], expectedvalue);
		emptyqueues();
		require(live == 0);
	}
}


/* Propagate an actual arithmetic exception from either group member to the other.
 * The recipient must consume its pending kill before decoding its first instruction. */
static void
groupfailure(uchar *image, unsigned int bytes, int jit, int follower)
{
	Prog *leader, *member, *source;
	void *snapshots[2];
	int before, calls, natives, helpers;

	leader = vmroot(image, bytes, jit, &snapshots[0]);
	member = vmroot(image, bytes, jit, &snapshots[1]);
	delgrp(member);
	addgrp(member, leader);
	leader->flags |= Ppropagate;
	member->flags |= Ppropagate;
	leader->xec = member->xec = observeexecution;
	if(follower){
		delrunq(member);
		pushrun(member);
	}
	source = follower ? member : leader;
	require(currun() == source && grpleader(source) == leader);
	before = terminals;
	calls = handlercalls;
	natives = nativecalls;
	helpers = ccalls;
	runvm(0, 2);
	/* No dead process or Module is dereferenced after the actual delprog calls. */
	require(terminals == before+2 && handlercalls == calls+2);
	require(nativecalls == natives+jit && ccalls == helpers+1);
	result(snapshots[0], 40);
	result(snapshots[1], 40);
	emptyqueues();
	require(live == 0);
}


/* Inject only legacy in-memory metadata, never a new Dis64 control-section format.
 * Genuine handler selection transfers a held exception into a mapped root slot,
 * resumes IEXIT and lets actual stack destruction release that slot afterward. */
static void
tablehandler(uchar *image, unsigned int bytes, int jit, int scenario)
{
	uchar map[2] = {0, 0x80};
	Module *module;
	Type *frame, *cleanup;
	Handler *table;
	Except *entries;
	Prog *process;
	void *snapshot;
	int calls, selected, diagnostics, matches;

	module = prepare64pilot("legacy-table-injection", image, bytes, jit);
	require(module != nil);
	frame = dtype(freeheap, 72, map, sizeof(map));
	require(frame != nil);
	freetype(module->type[1]);
	module->entryt = module->type[1] = frame;
	table = mallocz(2*sizeof(*table), 1);
	entries = mallocz(2*sizeof(*entries), 1);
	require(table != nil && entries != nil);
	module->htab = table;
	table->pc1 = 0;
	table->pc2 = 1;
	table->eoff = 64;
	if(scenario == TableMisaligned)
		table->eoff = 65;
	if(scenario == TableUnmarked)
		table->eoff = 56;
	cleanup = scenario == TableOversized ? dtype(freeheap, 80, nil, 0) : nil;
	require(scenario != TableOversized || cleanup != nil);
	table->t = scenario == TableCleanup ? frame : cleanup;
	table->etab = entries;
	entries[0].s = trackedstrdup(scenario == TableUnmatched ? "different error" : "*");
	require(entries[0].s != nil);
	entries[0].pc = scenario == TableOutside || scenario == TableFallback ? module->nprog : 1;
	entries[1].pc = scenario == TableFallback ? 1 : NOPC;
	observedispatch(module, jit);
	starting = module;
	require(kprotect(start, nil));
	process = started;
	rootenvironment(process);
	worker.env = &worker.defenv;
	process->quanta = 1;
	snapshot = process->R.M->MP;
	D2H(snapshot)->ref++;
	exceptionvalue = H2D(void*, heapz(&Tstring));
	previousvalue = H2D(void*, heapz(&Tstring));
	D2H(exceptionvalue)->ref++;
	D2H(previousvalue)->ref++;
	process->exval = exceptionvalue;
	memmove(process->R.FP+64, &previousvalue, sizeof(previousvalue));
	observed = process;
	calls = handlercalls;
	selected = selectedhandlers;
	diagnostics = diagnosticfailures;
	inspecthandler = 1;
	runvm(0, 1);
	inspecthandler = 0;
	matches = scenario == TableTransfer || scenario == TableCleanup || scenario == TableFallback;
	require(handlercalls == calls+(matches ? 2 : 1));
	require(selectedhandlers == selected+matches);
	require(diagnosticfailures == diagnostics+matches);
	if(cleanup != nil)
		freetype(cleanup);
	require(D2H(exceptionvalue)->ref == 1 && D2H(previousvalue)->ref == 1);
	destroy(exceptionvalue);
	destroy(previousvalue);
	exceptionvalue = previousvalue = H;
	result(snapshot, 40);
	emptyqueues();
	require(live == 0 && Tstring.ref == 1);
}


/* Drive actual exception selection without any fixture invocation of progexit. */
int
main(int argc, char **argv)
{
	uchar image[4096], original[4096];
	unsigned int bytes;
	FILE *file;
	int jit, scenario;

	require(argc == 3);
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
	Tstring.free = freeheap;
	Tstring.size = sizeof(String);
	file = fopen(argv[1], "rb");
	require(file != nil);
	bytes = fread(image, 1, sizeof(image), file);
	require(!ferror(file) && feof(file) && fclose(file) == 0);
	memmove(original, image, bytes);
	optab[IMOVW]=countmovw;
	optab[IADDW]=countaddw;
	optab[IDIVW]=countdivw;
	optab[ISHLW]=countshlw;
	optab[IEXIT]=countexit;
	for(jit=0; jit<2; jit++){
		scenarios(image, bytes, jit);
		if(expecterror){
			groupfailure(image, bytes, jit, 0);
			groupfailure(image, bytes, jit, 1);
			for(scenario=0; scenario<TableCases; scenario++)
				tablehandler(image, bytes, jit, scenario);
		}
	}
	require(memcmp(original, image, bytes) == 0);
	require(unhandled+selectedhandlers == handlercalls && rootpgrp.r.ref == 1 && rootfgrp.r.ref == 1 && rootegrp.r.ref == 1);
	printf("PASS Dis64 vmachine: %d predicates, handlers=%d selected=%d terminal=%d idle=%d native=%d accepted=%d declined=%d C=%d tails=%d; actual vmachine/handler/progexit; host worker/GC bounded\n", checks, handlercalls, selectedhandlers, terminals, idlecalls, nativecalls, nativeaccepted, nativedeclined, ccalls, bridgecalls);
	return 0;
}
