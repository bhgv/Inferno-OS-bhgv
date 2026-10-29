#include <sys/wait.h>

#define fetchcheck(c) fetchpredicate(!!(c), __LINE__) // Keep every predicate active under NDEBUG.

extern void (*fetchdec[256])(void); // Real production decoder under a private fixture symbol.
static Module fetchmodule, fetchother; // Source and callback-selected module metadata.
static Modlink fetchlink, fetchotherlink; // Source and destination runtime owners.
static Prog fetchprocess; // Real scheduler input/output register storage.
static Jit fetchsidecar; // Borrowed callback sidecar for route selection tests.
static uchar fetchcode[8*sizeof(Inst)+8]; // Bytecode images at every byte alignment.
static uchar fetchdata[1024]; // Valid source/destination/frame storage and pointer aliases.
static WORD fetchleft, fetchright, fetchoutput; // Real ADDW/DIVW operands.
static int fetchdecoded, fetchcalls, fetchfastcalls, fetchsteps, fetchentered, fetchnative; // Observed route counts.
static int fetchdeclines, fetchstop, fetchadjust; // Callback completion and explicit yield controls.
static int fetchowned; // Source module owns a real generated executable mapping.
static ulong fetchchecks; // Completed explicit predicates in this process.
static char fetchkill[] = "fetch kill"; // Pending kill error with precedence over invalid module state.


/* Report exact fixture failures while retaining every check under optimization. */
static void
fetchpredicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "VM fetch failure at %d\n", line);
		abort();
	}
	fetchchecks++;
}


/* Apply the first requested callback budget and bound old-loop parent controls. */
static void
fetchbudget(int calls)
{
	if(fetchadjust)
		R.IC = calls == 1 ? fetchstop : 1;
}


/* Model C work without changing any PC or operand outside the explicit budget test. */
static void
fetchhandler(void)
{
	fetchcalls++;
	fetchbudget(fetchcalls);
}


/* Count genuine decoded steps, including packed instructions and REG aliases. */
static void
fetchdecode(void)
{
	int add;

	fetchdecoded++;
	add = ((uchar*)R.PC)[O(Inst, add)];
	fetchdec[add]();
}


/* Preserve cache-entry ordering observations without changing VM state. */
static void
fetchenter(void)
{
	fetchentered++;
}


/* Exercise accepted or declined fast entries using the actual VM entry contract. */
static int
fetchfast(REG *reg)
{
	fetchcheck(reg == &R);
	fetchfastcalls++;
	fetchbudget(fetchfastcalls);
	return !fetchdeclines;
}


/* Successful AOT steps publish their next original Dis PC; declines publish nothing. */
static int
fetchstep(void)
{
	fetchsteps++;
	if(fetchdeclines)
		return 0;
	R.PC = (Inst*)((uintptr)R.PC+sizeof(Inst));
	fetchbudget(fetchsteps);
	return 1;
}


/* Release only the real compiler-owned mapping after the complete test route returns. */
static void
fetchdrop(void)
{
	Jit *j;

	if(fetchowned){
		j = fetchmodule.jit;
		j->freecode(j->code, j->size);
		free(j);
		fetchowned = 0;
	}
}


/* Restore an exact positive program and real VM headers before selecting a route. */
static void
fetchprepare(int op, int count, int shift)
{
	Inst instruction = {0};
	int i;

	fetchdrop();
	memset(&fetchmodule, 0, sizeof(fetchmodule));
	memset(&fetchother, 0, sizeof(fetchother));
	memset(&fetchlink, 0, sizeof(fetchlink));
	memset(&fetchotherlink, 0, sizeof(fetchotherlink));
	memset(&fetchprocess, 0, sizeof(fetchprocess));
	memset(&fetchsidecar, 0, sizeof(fetchsidecar));
	memset(fetchcode, 0, sizeof(fetchcode));
	instruction.op = op;
	instruction.add = SRC(AXXX)|DST(AXXX);
	for(i = 0; i < count; i++)
		memmove(fetchcode+shift+(uintptr)i*sizeof(Inst), &instruction, sizeof(instruction));
	fetchmodule.prog = (Inst*)(fetchcode+shift);
	fetchmodule.nprog = count;
	fetchlink.m = &fetchmodule;
	fetchlink.MP = fetchdata;
	fetchlink.prog = fetchmodule.prog;
	fetchprocess.R.M = &fetchlink;
	fetchprocess.R.PC = fetchmodule.prog;
	fetchprocess.R.FP = fetchdata+128;
	fetchprocess.R.s = &fetchright;
	fetchprocess.R.m = &fetchleft;
	fetchprocess.R.d = &fetchoutput;
	fetchprocess.quanta = count;
	fetchleft = 17;
	fetchright = 3;
	fetchoutput = 0;
	fetchdecoded = fetchcalls = fetchfastcalls = fetchsteps = fetchentered = fetchnative = 0;
	fetchadjust = fetchstop = fetchdeclines = 0;
	for(i = 0; i < 256; i++){
		dec[i] = fetchdecode;
		optab[i] = fetchhandler;
	}
	comvec = nil;
}


/* Install C, guarded fast, complete, successful/declined AOT routes independently. */
static void
fetchroute(int op, int route)
{
	if(route == 1 || route == 2){
		fetchmodule.jit = &fetchsidecar;
		fetchsidecar.entry[op] = fetchfast;
		fetchsidecar.enter = fetchenter;
		fetchdeclines = route == 2;
	}else if(route == 3){
		fetchmodule.jit = &fetchsidecar;
		fetchsidecar.dispatch[op] = fetchfast;
		fetchsidecar.enter = fetchenter;
	}else if(route == 4 || route == 5){
		fetchmodule.cstep = fetchstep;
		fetchdeclines = route == 5;
		if(route == 4){
			dec[SRC(AXXX)|DST(AXXX)] = nil;
			optab[op] = (void(*)(void))H; /* A completed AOT step must not consult unused tables. */
		}
	}
}


/* Check every active opcode under six routes, eight packed PCs and four quanta. */
static void
fetchroutes(void)
{
	int op, route, shift, count;

	for(op = 1; op < MAXDIS; op++){
		if(op == IECLR)
			continue;
		for(route = 0; route < 6; route++)
		for(shift = 0; shift < 8; shift++)
		for(count = 1; count <= 4; count++){
			fetchprepare(op, count, shift);
			fetchroute(op, route);
			xec(&fetchprocess);
			fetchcheck(fetchprocess.R.IC == 0 && fetchprocess.R.PC == (Inst*)(fetchcode+shift+(uintptr)count*sizeof(Inst)));
			fetchcheck(fetchdecoded == (route == 4 ? 0 : count));
			fetchcheck(fetchcalls == ((route == 0 || route == 2 || route == 5) ? count : 0));
			fetchcheck(fetchfastcalls == ((route >= 1 && route <= 3) ? count : 0));
			fetchcheck(fetchsteps == (route >= 4 ? count : 0));
			fetchcheck(fetchentered == ((route >= 1 && route <= 3) ? count : 0));
		}
	}
}


/* Execute real RX ADDW fast entries, DIVW fallback and C-only ALT tails in the loop. */
static void
fetchnativepaths(void)
{
	int which, shift, count, op;

	for(which = 0; which < 3; which++)
	for(shift = 0; shift < 8; shift++)
	for(count = 1; count <= 4; count++){
		op = which == 0 ? IADDW : which == 1 ? IDIVW : IALT;
		fetchprepare(op, count, shift);
		fetchcheck(compile(&fetchmodule, count, nil));
		fetchowned = 1;
		fetchmodule.jit->enter = fetchenter;
		if(which == 1)
			fetchright = 0;
		if(which == 0)
			optab[IADDW] = shift&1 ? nil : (void(*)(void))H;
		xec(&fetchprocess);
		fetchcheck(fetchdecoded == count && fetchentered == count && fetchprocess.R.IC == 0);
		fetchcheck(fetchcalls == (which == 0 ? 0 : count));
		fetchcheck(fetchoutput == (which == 0 ? 20 : 0));
	}
	fetchdrop();
}


/* All callback routes must yield after zero/negative budgets, including INT_MIN. */
static void
fetchyields(void)
{
	static int budgets[] = {0, -1, (-2147483647-1), 1}; // Explicit stop requests and old signed-decrement boundary.
	int route, value;

	for(route = 0; route < 7; route++)
	for(value = 0; value < nelem(budgets); value++){
		if(getenv("FETCH_ROUTE") && route != atoi(getenv("FETCH_ROUTE")))
			continue;
		fetchprepare(IALT, 2, 0);
		fetchroute(IALT, route);
		if(route == 6){
			fetchcheck(compile(&fetchmodule, 2, nil));
			fetchowned = 1;
		}
		fetchprocess.quanta = 2;
		fetchadjust = 1;
		fetchstop = budgets[value];
		xec(&fetchprocess);
		fetchcheck(fetchprocess.R.IC == 0 && fetchprocess.R.PC == (Inst*)(fetchcode+sizeof(Inst)));
		fetchcheck(fetchdecoded <= 1 && fetchcalls <= 1 && fetchfastcalls <= 1 && fetchsteps <= 1);
	}
	fetchdrop();
}


/* Enter relocated native code only through comvec, without fetching its token as Dis. */
static void
fetchnativeentry(void)
{
	fetchnative++;
	fetchcheck(R.M == &fetchotherlink && R.PC == (Inst*)1 && R.MP == fetchdata+512);
	fetchcheck(R.IC == 2);
	R.IC = 0;
}


/* Publish a callback-selected compiled context while retaining the remaining quantum. */
static void
fetchswitch(void)
{
	fetchcalls++;
	R.M = &fetchotherlink;
	R.PC = (Inst*)1;
	R.MP = fetchdata+512;
}


/* Route an AOT context switch through the same scheduler completion contract. */
static int
fetchswitchstep(void)
{
	fetchsteps++;
	fetchswitch();
	return 1;
}


/* Route a guarded JIT context switch without consulting stale source metadata. */
static int
fetchswitchfast(REG *reg)
{
	fetchcheck(reg == &R);
	fetchfastcalls++;
	fetchswitch();
	return 1;
}


/* Check C, AOT, fast and real complete-tail switches before the next instruction. */
static void
fetchtransitions(void)
{
	int route;

	for(route = 0; route < 4; route++){
		fetchprepare(IALT, 2, 0);
		fetchotherlink.compiled = 1;
		fetchotherlink.m = &fetchother;
		fetchotherlink.MP = fetchdata+512;
		comvec = fetchnativeentry;
		optab[IALT] = fetchswitch;
		fetchprocess.quanta = 3;
		if(route == 1)
			fetchmodule.cstep = fetchswitchstep;
		else if(route == 2){
			fetchmodule.jit = &fetchsidecar;
			fetchsidecar.entry[IALT] = fetchswitchfast;
		}else if(route == 3){
			fetchcheck(compile(&fetchmodule, 2, nil));
			fetchowned = 1;
		}
		xec(&fetchprocess);
		fetchcheck(fetchnative == 1 && fetchcalls == 1 && fetchprocess.R.IC == 0);
		fetchcheck(fetchdecoded == (route == 1 ? 0 : 1));
	}
	fetchdrop();
}


/* Fail if a decoder's REG publication changes the opcode selected by the VM loop. */
static void
fetchwrong(void)
{
	fetchcheck(0);
}


/* Real decoder snapshots protect a program image that aliases REG.s/d/m. */
static void
fetchopcodealias(void)
{
	Inst instruction = {0};
	int route, i;

	for(route = 0; route < 3; route++){
		if(getenv("FETCH_ROUTE") && route != atoi(getenv("FETCH_ROUTE")))
			continue;
		fetchprepare(IRUNT, 1, 0);
		instruction.op = IRUNT;
		instruction.add = SRC(AFP)|DST(AXXX);
		memmove((uchar*)&fetchprocess.R+O(REG, s), &instruction, sizeof(instruction));
		fetchmodule.prog = (Inst*)((uchar*)&R+O(REG, s));
		fetchprocess.R.PC = fetchmodule.prog;
		for(i = 0; i < 512; i++)
			if(((uintptr)(fetchdata+i)&255) == IADDW)
				break;
		fetchcheck(i < 512);
		fetchprocess.R.FP = fetchdata+i;
		memmove((uchar*)&R+O(REG, s), &instruction, sizeof(instruction));
		if(route == 1){
			fetchcheck(compile(&fetchmodule, 1, nil));
			fetchowned = 1;
		}else if(route == 2){
			fetchmodule.cstep = fetchstep;
			fetchdeclines = 1;
		}
		optab[IADDW] = fetchwrong;
		xec(&fetchprocess);
		fetchcheck(fetchdecoded == 1 && fetchprocess.R.IC == 0);
		fetchcheck(fetchcalls == (route == 1 ? 0 : 1));
		fetchcheck(fetchprocess.R.PC == (Inst*)((uchar*)&R+O(REG, s)+sizeof(Inst)));
	}
	fetchdrop();
}


/* Packed module/link/cache fields remain readable without native typed dereferences. */
static void
fetchpacked(void)
{
	uchar modules[sizeof(Module)+8], links[sizeof(Modlink)+8], cache[sizeof(Jit)+8];
	Module *m;
	Jit *j;
	int a, b, c;

	for(a = 0; a < 8; a++)
	for(b = 0; b < 8; b++)
	for(c = 0; c < 8; c++){
		fetchprepare(IADDW, 1, c);
		fetchcheck(compile(&fetchmodule, 1, nil));
		fetchowned = 1;
		memmove(cache+c, fetchmodule.jit, sizeof(Jit));
		j = (Jit*)(cache+c);
		memmove(modules+a, &fetchmodule, sizeof(Module));
		m = (Module*)(modules+a);
		memmove((uchar*)m+O(Module, jit), &j, sizeof(j));
		memmove(links+b, &fetchlink, sizeof(Modlink));
		memmove(links+b+O(Modlink, m), &m, sizeof(m));
		fetchprocess.R.M = (Modlink*)(links+b);
		xec(&fetchprocess);
		fetchcheck(fetchoutput == 20 && fetchprocess.R.IC == 0);
	}
	fetchdrop();
}


/* Capture a single expected fetch failure without exposing abandoned frames to reuse. */
static void
fetchfailure(char *family, int value, int route)
{
	Prog *process;
	Prog before;
	char *wanted;
	int expecteddecoded;

	fetchprepare(route == 1 ? IADDW : IALT, 2, 0);
	fetchroute(route == 1 ? IADDW : IALT, route == 3 ? 4 : route);
	process = &fetchprocess;
	wanted = exBounds;
	expecteddecoded = 0;
	if(!strcmp(family, "bounds")){
		if(value == 0) process->R.PC = nil;
		if(value == 1) process->R.PC = H;
		if(value == 2) process->R.PC = (Inst*)((uintptr)fetchmodule.prog-1);
		if(value == 3) process->R.PC = (Inst*)((uchar*)fetchmodule.prog+1);
		if(value == 4) process->R.PC = (Inst*)(fetchcode+2*sizeof(Inst));
		if(value == 5) process->R.PC = (Inst*)(fetchcode+2*sizeof(Inst)-1);
		if(value == 6){ fetchmodule.prog = (Inst*)((uintptr)-1-sizeof(Inst)); process->R.PC = fetchmodule.prog; }
		if(value == 7) fetchmodule.nprog = 0;
		if(value == 8) fetchmodule.nprog = -1;
		if(value == 9) fetchmodule.prog = nil;
		if(value == 10) fetchmodule.prog = H;
	}else if(!strcmp(family, "module")){
		wanted = exModule;
		if(value < 3) process->R.M = value == 0 ? nil : value == 1 ? H : (Modlink*)((uintptr)-2);
		else fetchlink.m = value == 3 ? nil : value == 4 ? H : (Module*)((uintptr)-2);
	}else if(!strcmp(family, "opcodes")){
		static int invalid[] = {INOP, IECLR, MAXDIS, 255}; // All inactive opcode classes.
		fetchcode[O(Inst, op)] = invalid[value];
		wanted = exOp;
	}else if(!strcmp(family, "budget")){
		static int invalid[] = {0, -1, (-2147483647-1)}; // Invalid scheduler entry budgets.
		process->quanta = invalid[value];
		fetchadjust = 1;
		fetchstop = 1; /* Bound the old parent loop even when its entry budget is invalid. */
		wanted = exInval;
	}else if(!strcmp(family, "process")){
		process = value == 0 ? nil : value == 1 ? H : value == 2 ? (Prog*)((uintptr)-2) : (Prog*)((uchar*)&fetchprocess+1);
		wanted = exHeap;
	}else if(!strcmp(family, "kill")){
		process->kill = fetchkill;
		process->R.M = nil;
		wanted = fetchkill;
	}else if(!strcmp(family, "falloff")){
		fetchmodule.nprog = 1;
		if(route == 1 || route == 2){
			fetchmodule.jit = nil;
			fetchcheck(compile(&fetchmodule, 1, nil));
			fetchowned = 1;
		}
		process->quanta = 2;
		expecteddecoded = route == 3 ? 0 : 1;
	}else{
		fetchmodule.jit = nil;
		wanted = value < 4 ? exOp : exCompile;
		expecteddecoded = value < 2 || value >= 9 ? 0 : 1;
		if(value == 0) dec[SRC(AXXX)|DST(AXXX)] = nil;
		if(value == 1) dec[SRC(AXXX)|DST(AXXX)] = (void(*)(void))H;
		if(value == 2) optab[IALT] = nil;
		if(value == 3) optab[IALT] = (void(*)(void))H;
		if(value == 4) fetchmodule.jit = H;
		if(value == 5) fetchmodule.jit = (Jit*)((uintptr)-2);
		if(value >= 6 && value <= 8){
			fetchmodule.jit = &fetchsidecar;
			fetchsidecar.entry[IALT] = value == 6 ? (int(*)(REG*))H : fetchfast;
			if(value == 7){ fetchsidecar.entry[IALT] = nil; fetchsidecar.dispatch[IALT] = (int(*)(REG*))H; }
			if(value == 8) fetchsidecar.enter = (void(*)(void))H;
		}
		if(value == 9) fetchmodule.cstep = (int(*)(void))H;
		if(value >= 10){ fetchlink.compiled = 1; comvec = value == 10 ? nil : (void(*)(void))H; }
	}
	before = fetchprocess;
	raised = nil;
	if(setjmp(boundary) == 0){
		xec(process);
		fetchcheck(0);
	}
	fetchcheck(raised == wanted && fetchdecoded == expecteddecoded);
	if(!strcmp(family, "kill")){
		fetchcheck(fetchprocess.kill == nil && fetchcalls == 0 && fetchsteps == 0);
		before.kill = nil;
	}
	fetchcheck(!memcmp(&fetchprocess, &before, sizeof(before)));
	if(strcmp(family, "falloff"))
		fetchcheck(fetchcalls == 0 && fetchfastcalls == 0 && fetchsteps == 0);
}


/* Run malformed pointer/error cases in fresh processes with ordinary ASan instrumentation. */
static void
fetcherrors(char *family)
{
	pid_t child;
	int value, route, count, status;

	count = !strcmp(family, "bounds") ? 11 : !strcmp(family, "module") ? 6 :
		!strcmp(family, "opcodes") || !strcmp(family, "process") ? 4 :
		!strcmp(family, "budget") ? 3 : !strcmp(family, "tables") ? 12 : 1;
	for(route = 0; route < ((!strcmp(family, "bounds") || !strcmp(family, "falloff")) ? 4 : 1); route++)
	for(value = 0; value < count; value++){
		if(getenv("FETCH_ROUTE") && route != atoi(getenv("FETCH_ROUTE")))
			continue;
		child = fork();
		fetchcheck(child >= 0);
		if(child == 0){
			fetchfailure(family, value, route);
			_Exit(0);
		}
		fetchcheck(waitpid(child, &status, 0) == child);
		fetchcheck(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
}


/* Execute the complete fetch/routing matrix or an independent parent-replay family. */
int
main(void)
{
	static char *families[] = { // Separate error matrices retain distinct before/after-decoder boundaries.
		"bounds", "module", "opcodes", "budget", "process", "kill", "tables", "falloff",
	};
	char *selected;
	int i;

	selected = getenv("FETCH_CASE");
	if(selected == nil || !strcmp(selected, "routes")) fetchroutes();
	if(selected == nil || !strcmp(selected, "native")) fetchnativepaths();
	if(selected == nil || !strcmp(selected, "yield")) fetchyields();
	if(selected == nil || !strcmp(selected, "transitions")) fetchtransitions();
	if(selected == nil || !strcmp(selected, "alias")) fetchopcodealias();
	if(selected == nil || !strcmp(selected, "packed")) fetchpacked();
	for(i = 0; i < nelem(families); i++)
		if(selected == nil || !strcmp(selected, families[i]))
			fetcherrors(families[i]);
	fetchdrop();
	fetchcheck(fetchchecks != 0);
	printf("PASS VM fetch %s WORD%zu: %lu parent predicates\n", selected == nil ? "all" : selected, sizeof(WORD)*8, fetchchecks);
	return 0;
}
