#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "vmfetch.h"
#include <stdio.h>
#include <sys/wait.h>

#define check(c) predicate(!!(c), __LINE__) // Preserve all tests under NDEBUG.
#define OP(name) static void name(void) // Borrow the real scalar VM instrumenthandler.
#define stat instrumentstat // Isolate the kernel's private timing helper from host libc names.

enum { TOKI0, TOKI1, TOKI2, TOKI3, TOKSB, TOKFP }; // Actual tab.h terminal tags.
#include "tab.h"
DECLARATIONS

REG R; // Actual native VM register layout.
char exHeap[]="heap", exInval[]="invalid", exModule[]="module"; // Owner and scheduler errors.
char exBounds[]="bounds", exOp[]="opcode", exCompile[]="compile", exSpawn[]="spawn"; // Execution errors.
char exNilref[]="nil"; // Decoder sentinel identity.
void (*comvec)(void); // Native-PC handoff observed by the fixture.
void (*optab[256])(void); // Real-instrumenthandler selection under test.
void (*dec[256])(void); // Counted decoder wrappers selected by actual addressing bytes.
static jmp_buf boundary, frames[8]; // Outer VM and nested debugger error boundaries.
static int depth; // Number of active debugger error frames.
static void (*volatile childexit)(int) = _Exit; // Exit without invoking unavailable ASan abandoned-stack cleanup.
static char *raised; // Exact VM error identity.
static ulong checks; // Parent/ordinary-process predicates, excluding forked child copies.
static Prog process, child, otherprocess; // Scheduler inputs and observed child/block transitions.
static Module module, othermodule; // Current and callback-selected modules.
static Modlink instance, otherinstance; // Runtime owners with independent bytecode bases.
static Osenv environment; // Real host environment layout used by the extracted device bodies.
static Progctl control; // Actual debugger control declaration.
static uchar text[8*sizeof(Inst)+8], payload[1024]; // Packed code and native operand storage.
static WORD left, right, result; // Real ADDW operands.
static int executed, decoded, nativecalls, lookups, childadds, removed, blocked, closed; // Route observations.
static int blockedmode, childmissing, budgetmode, requestedbudget, resetmode, switchmode; // Callback scenarios.
static int qcount; // Number of queued debugger notifications.
static char queued[16][ERRMAX+10], reply[ERRMAX+10]; // Captured protocol text.
static Record *active; // Current coverage record, replaced by reset callbacks.
static ulong clockticks; // Deterministic native timing source.
static void *owned[128]; // Private timing/profile allocations owned by the fixture.
static int reenter, reentered; // Reentrant allocator callback boundary.
static int live, fail_after = -1, allocations; // Allocation ownership and failure-injection boundary.
static int profiler = 2; // Pcov's production value; other values disable coverage lookup.
enum { Pcov = 2 }; // Coverage selector consumed by real cpxec bodies.
static String loadname; // Stable borrowed owner used by LOAD notification tests.
static uchar loadslot[sizeof(void*)+8]; // Native owner slot at every byte alignment.


/* Diagnose failures without depending on assert or optimization settings. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "instrument loop failure at %d\n", line);
		abort();
	}
	checks++;
}


/* Retain memory instrumentation without unavailable process-based leak scanning. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Allocate zeroed kernel-style storage and observe oversized requests without materializing them. */
static void*
instrumentmalloc(ulong size)
{
	void *p;
	int i;

	allocations++;
	if(fail_after == 0 || size > 16*1024*1024)
		return nil;
	if(fail_after > 0)
		fail_after--;
	p = calloc(1, size == 0 ? 1 : size);
	check(p != nil);
	for(i = 0; i < nelem(owned); i++)
		if(owned[i] == nil){
			owned[i] = p;
			live++;
			return p;
		}
	abort();
}


/* Private record lookup can be re-entered by an injected allocator hook. */
static Mstat *mlookup(Module*);


/* Observe every release through the actual host allocator. */
static void
instrumentfree(void *p)
{
	int i;

	if(p == nil)
		return;
	for(i = 0; i < nelem(owned); i++)
		if(owned[i] == p){
			if(reenter && !reentered){
				reentered = 1;
				check(vmstat.list == nil);
				check(mlookup(&module) != nil);
			}
			owned[i] = nil;
			live--;
			free(p);
			return;
		}
	check(0);
}


/* Model kstrdup's ownership replacement, with explicit nil allocation injection. */
void
kstrdup(char **target, char *source)
{
	char *p;

	p = instrumentmalloc(strlen(source)+1);
	if(p != nil)
		memmove(p, source, strlen(source)+1);
	instrumentfree(*target);
	*target = p;
}

/* Native timestamp source used by the borrowed timing hook definitions. */
static vlong archrdtsc(void);

#define malloc instrumentmalloc // Track production benchmark record ownership.
#define free instrumentfree // Track names, paths, records and instruction rows.
BENCH_HELPERS
#undef malloc
#undef free


/* Allocate a complete flexible coverage bucket array for eight instructions. */
static Record*
newrecord(void)
{
	Record *r;

	r = instrumentmalloc(sizeof(Record)+7*sizeof(ulong));
	check(r != nil);
	r->base = module.prog;
	r->size = module.nprog;
	return r;
}


/* Preserve ordered profiler lookup while allowing controls to replace the record between steps. */
static Record*
mlook(Module *m PROFILE_ARGS)
{
	check(m == &module);
	lookups++;
	return active;
}


/* Keep real timing accounting deterministic and safely below arithmetic overflow limits. */
static vlong
archrdtsc(void)
{
	clockticks += 10;
	return clockticks;
}


/* Enter one debugger error frame through an ordinary C setjmp controlling expression. */
static int
pushframe(void)
{
	check(depth < nelem(frames));
	return depth++;
}
#undef waserror
#undef poperror
#define waserror() setjmp(frames[pushframe()]) // Preserve actual debugger cleanup topology.
#define poperror() (depth--) // Release the successful debugger frame.


/* Raise through debugger cleanup before propagating to the outer VM boundary. */
void
error(char *message)
{
	raised = message;
	if(depth != 0)
		longjmp(frames[depth-1], 1);
	longjmp(boundary, 1);
}


/* Propagate an already recorded VM error after one debugger frame is cleaned up. */
void
nexterror(void)
{
	check(depth > 0);
	depth--;
	if(depth != 0)
		longjmp(frames[depth-1], 1);
	longjmp(boundary, 1);
}


/* Provide the real formatter with a deterministic error-string provider. */
void
rerrstr(char *buf, uint size)
{
	if(size != 0)
		snprintf(buf, size, "fixture");
}


/* Preserve the exact queue payload and length emitted by debugger notifications. */
int
qproduce(Queue *queue, void *text, int length)
{
	USED(queue);
	check(qcount < nelem(queued) && length >= 0 && length < sizeof(queued[0]));
	memmove(queued[qcount], text, length);
	queued[qcount++][length] = 0;
	return length;
}


/* The notification source remains a borrowed VM String owned by its original slot. */
char*
string2c(String *s)
{
	check(s == &loadname);
	return "loaded.dis";
}


/* Model successful debugger cleanup without changing process ownership. */
static void
closedbgctl(Progctl *ctl, Prog *p)
{
	check(ctl == &control && p == &process && ctl->ref > 1);
	ctl->ref--;
	closed++;
}


/* Observe replies, including breakpoints and pre-existing stop requests. */
static void
telldbg(Progctl *ctl, char *text)
{
	check(ctl == &control && strlen(text) < sizeof(reply));
	strcpy(reply, text);
}


/* Model a blocked process becoming runnable again at the debugger boundary. */
static void
dbgblock(Prog *p)
{
	check(p == &process);
	blocked++;
	blockedmode = 0;
}


/* Preserve scheduler identity tests around debugger blocking and final suspension. */
Prog*
currun(void)
{
	return blockedmode ? &otherprocess : &process;
}


/* Debugger completion suspends exactly the current process. */
Prog*
delrun(int state)
{
	check(state == Pdebug);
	removed++;
	return &process;
}


/* A successful spawn exposes one child at the run-queue tail. */
Prog*
delruntail(int state)
{
	check(state == Pdebug);
	return childmissing ? nil : &child;
}


/* Profile/timing loops retain their execution hook when publishing the child. */
void
addrun(Prog *p)
{
	check(p == &child);
	childadds++;
}

SCALARS
LOOP_BODY


/* Count a C step and model yields, record resets, native switches and changed REG.s. */
static void
instrumenthandler(void)
{
	Record *replacement;

	executed++;
	if(budgetmode)
		R.IC = executed == 1 ? requestedbudget : 1;
	if(resetmode && executed == 1){
#ifdef TEST_PROFILE
		replacement = newrecord();
		instrumentfree(active);
		active = replacement;
#else
		USED(replacement);
		mreset();
#endif
	}
	if(switchmode){
		R.M = &otherinstance;
		R.MP = payload+512;
		R.PC = (Inst*)1;
	}
}


/* Exercise the genuine modular helper at arbitrary decoded-slot alignment. */
static void
addhandler(void)
{
	instrumenthandler();
	addw();
}


/* A LOAD callback can borrow/change REG.s while the debugger retains the original slot. */
static void
loadhandler(void)
{
	instrumenthandler();
	R.s = H;
}


/* Decoder publication must never change which opcode the instrumented loop executes. */
static void
wronghandler(void)
{
	check(0);
}


extern void (*realdec[256])(void); // Actual generated VM decoder under a private test symbol.


/* Preserve the production decoder's packed-image and indirect-cell behavior. */
static void
realdecoder(void)
{
	int add;

	decoded++;
	add = ((uchar*)R.PC)[O(Inst, add)];
	realdec[add]();
}


/* Native contexts must not be dereferenced as Dis after a C callback switches modes. */
static void
nativeentry(void)
{
	check(R.M == &otherinstance && R.MP == payload+512 && R.PC == (Inst*)1);
	nativecalls++;
	R.IC = 0;
}


/* Release only test-owned records after each normal instrumented invocation. */
static void
cleanup(void)
{
	if(active != nil){
		instrumentfree(active);
		active = nil;
	}
	mreset();
	check(live == 0 && depth == 0);
}


/* Restore exact bytecode, real owner headers, controller state and C table entries. */
static void
prepare(int op, int count, int shift)
{
	Inst instruction = {0};
	String *name;
	int i;

	cleanup();
	memset(&process, 0, sizeof(process));
	memset(&module, 0, sizeof(module));
	memset(&instance, 0, sizeof(instance));
	memset(&otherinstance, 0, sizeof(otherinstance));
	memset(&othermodule, 0, sizeof(othermodule));
	memset(&control, 0, sizeof(control));
	memset(&environment, 0, sizeof(environment));
	memset(text, 0, sizeof(text));
	instruction.op = op;
	instruction.add = SRC(AXXX)|DST(AXXX);
	for(i = 0; i < 8; i++)
		memmove(text+shift+(uintptr)i*sizeof(Inst), &instruction, sizeof(instruction));
	module.prog = (Inst*)(text+shift);
	module.nprog = count;
	module.name = "instrument";
	module.path = "instrument.dis";
	instance.m = &module;
	instance.prog = module.prog;
	instance.MP = payload;
	otherinstance.m = &othermodule;
	otherinstance.compiled = 1;
	otherinstance.MP = payload+512;
	process.R.M = &instance;
	process.R.PC = module.prog;
	process.R.FP = payload+128;
	process.R.s = &right;
	process.R.m = &left;
	process.R.d = &result;
	process.osenv = &environment;
	process.quanta = count;
	control.ref = 1;
	control.step = count;
	environment.debug = &control;
	child.pid = 42;
	executed = decoded = nativecalls = lookups = childadds = removed = blocked = closed = qcount = 0;
	budgetmode = resetmode = switchmode = blockedmode = childmissing = 0;
	requestedbudget = 1;
	clockticks = 0;
	memset(&C, 0, sizeof(C));
	profiler = Pcov;
	fail_after = -1;
	allocations = 0;
	left = 17;
	right = 3;
	result = 0;
	reply[0] = 0;
	for(i = 0; i < 256; i++){
		optab[i] = instrumenthandler;
		dec[i] = realdecoder;
	}
	optab[IADDW] = addhandler;
	comvec = nativeentry;
	name = &loadname;
	memmove(loadslot+shift, &name, sizeof(name));
#ifdef TEST_PROFILE
	active = newrecord();
#endif
}


/* Compare normal quantum accounting for packed instructions and all scalar slot shifts. */
static void
normal(void)
{
	int shift, count, slot;
	WORD a = 17, b = 3, got;

	for(shift = 0; shift < 8; shift++)
	for(count = 1; count <= 4; count++)
	for(slot = 0; slot < 8; slot++){
		prepare(IADDW, count, shift);
		memmove(payload+16+slot, &a, sizeof(a));
		memmove(payload+48+slot, &b, sizeof(b));
		process.R.m = payload+16+slot;
		process.R.s = payload+48+slot;
		process.R.d = payload+80+slot;
		EXECUTE(&process);
		memmove(&got, payload+80+slot, sizeof(got));
		check(executed == count && decoded == count && got == 20);
		check(process.R.IC == 0 && process.R.PC == (Inst*)(text+shift+(uintptr)count*sizeof(Inst)));
#ifdef TEST_DEBUG
		check(closed == 1 && control.ref == 1 && removed == 1);
#endif
#ifdef TEST_PROFILE
		check(lookups == count);
		check(active->bucket[0] == 1);
#endif
	}
	cleanup();
}


/* Real REG/Inst aliasing must retain the fetched IRUNT after source-register decoding. */
static void
alias(void)
{
	Inst instruction = {0};
	int i;

	prepare(IRUNT, 1, 0);
	instruction.op = IRUNT;
	instruction.add = SRC(AFP)|DST(AXXX);
	memmove((uchar*)&process.R+O(REG, s), &instruction, sizeof(instruction));
	module.prog = (Inst*)((uchar*)&R+O(REG, s));
	instance.prog = module.prog;
	process.R.PC = module.prog;
	for(i = 0; i < 512; i++)
		if(((uintptr)(payload+i)&255) == IADDW)
			break;
	check(i < 512);
	process.R.FP = payload+i;
	memmove((uchar*)&R+O(REG, s), &instruction, sizeof(instruction));
	if(active != nil)
		active->base = module.prog;
	optab[IADDW] = wronghandler;
	EXECUTE(&process);
	check(executed == 1 && decoded == 1 && process.R.IC == 0);
	cleanup();
}


/* Explicit callback yields stop after one instruction without signed decrement overflow. */
static void
yields(void)
{
	static int values[] = {0, -1, (-2147483647-1), 1}; // Stop requests and the signed-decrement boundary.
	int i;

	for(i = 0; i < nelem(values); i++){
		prepare(IRUNT, 2, 0);
		budgetmode = 1;
		requestedbudget = values[i];
		EXECUTE(&process);
		check(executed == 1 && process.R.IC == 0 && process.R.PC == (Inst*)(text+sizeof(Inst)));
	}
	cleanup();
}


/* Discard stale instrumentation pointers when a successful C step resets records. */
static void
resets(void)
{
	prepare(IRUNT, 2, 0);
	resetmode = 1;
	EXECUTE(&process);
	check(executed == 2 && process.R.IC == 0);
#ifdef TEST_PROFILE
	check(lookups == 2 && active->bucket[0] == 0 && active->bucket[1] == 1);
#endif
#ifdef TEST_BENCH
	check(vmstat.list != nil && vmstat.list->inst[0].count == 0 && vmstat.list->inst[1].count == 1);
#endif
	cleanup();
}


/* Switch to a native token between C steps without treating it as an Inst image. */
static void
nativecontext(void)
{
	prepare(IRUNT, 2, 0);
	switchmode = 1;
	EXECUTE(&process);
	check(executed == 1 && nativecalls == 1 && decoded == 1 && process.R.PC == (Inst*)1);
	cleanup();
}


/* Preserve child hooks, load messages, stop requests and instruction breakpoints. */
static void
notifications(void)
{
	Bpt *b;
	int op, shift;

	for(op = 0; op < 2; op++){
		prepare(op == 0 ? ISPAWN : IMSPAWN, 1, 0);
		EXECUTE(&process);
#ifdef TEST_DEBUG
		check(qcount == 1 && !strcmp(queued[0], "new 42"));
#else
		check(childadds == 1 && child.xec == EXECUTE);
#endif
	}
#ifdef TEST_DEBUG
	for(shift = 0; shift < 8; shift++){
		prepare(ILOAD, 1, 0);
		process.R.s = loadslot+shift;
		{
			String *name = &loadname;
			memmove(loadslot+shift, &name, sizeof(name));
		}
		optab[ILOAD] = loadhandler;
		EXECUTE(&process);
		check(qcount == 1 && !strcmp(queued[0], "load loaded.dis"));
	}
	prepare(IRUNT, 2, 0);
	control.stop = 1;
	module.nprog = 0;
	module.prog = H;
	process.R.PC = H; /* A stopped debugger must not fetch even an invalid PC. */
	EXECUTE(&process);
	check(executed == 0 && !strcmp(reply, "stopped"));
	prepare(IRUNT, 2, 0);
	control.step = 0;
	EXECUTE(&process);
	check(executed == 0 && process.R.IC == 0);
	prepare(IRUNT, 3, 0);
	b = calloc(1, sizeof(*b)+32);
	check(b != nil);
	b->file = "instrument.dis";
	b->pc = 1;
	strcpy(b->path, "other.dis");
	control.bpts = b;
	EXECUTE(&process);
	check(executed == 1 && !strcmp(reply, "breakpoint") && process.R.PC == (Inst*)(text+sizeof(Inst)));
	free(b);
	prepare(IRUNT, 1, 0);
	blockedmode = 1;
	EXECUTE(&process);
	check(blocked == 1);
#endif
	cleanup();
}


/* Reused records cannot index past their original timing-row allocation. */
static void
lookupbounds(void)
{
	Mstat *m;
	int i;

	prepare(IRUNT, 3, 0);
	m = mlookup(&module);
	check(m != nil && m->ninst == 3);
	module.nprog = 4;
	process.R.PC = (Inst*)(text+3*sizeof(Inst));
	process.quanta = 1;
	EXECUTE(&process);
	check(executed == 1 && m->inst[0].count == 0);
	cleanup();
	for(i = 0; i < 4; i++){
		prepare(IRUNT, 1, 0);
		fail_after = i;
		check(mlookup(&module) == nil);
		check(live == 0 && vmstat.list == nil);
		fail_after = -1;
	}
	prepare(IRUNT, 1, 0);
	module.nprog = 0x7fffffff;
	check(mlookup(&module) == nil && allocations == 0 && live == 0);
	cleanup();
}


/* Reset detaches roots before frees and preserves records published by a free callback. */
static void
reentrancy(void)
{
	prepare(IRUNT, 1, 0);
	check(mlookup(&module) != nil && live == 4);
	reenter = 1;
	reentered = 0;
	mreset();
	reenter = 0;
	check(reentered == 1 && vmstat.list != nil && live == 4);
	cleanup();
}


/* Pending compiled timing samples must not retain freed rows across a reset. */
static void
compiledhooks(void)
{
	uchar pcbytes[sizeof(ulong)+8], opbytes[sizeof(ulong)+8];
	ulong pc = 0, op = IADDW;
	Mstat *m;
	int a, b;

	for(a = 0; a < 8; a++)
	for(b = 0; b < 8; b++){
		prepare(IRUNT, 1, 0);
		R = process.R;
		instance.compiled = 1;
		memmove(pcbytes+a, &pc, sizeof(pc));
		memmove(opbytes+b, &op, sizeof(op));
		R.m = pcbytes+a;
		R.s = opbytes+b;
		C.timing = 1;
		precomp();
		postcomp();
		m = vmstat.list;
		check(m != nil && m->inst[0].count == 1 && m->inst[0].op == IADDW);
		precomp();
		mreset();
		postcomp();
		check(C.ms == nil && C.set == 0 && live == 0);
		precomp();
		postcomp();
		check(vmstat.list != nil && vmstat.list->inst[0].count == 1);
		cleanup();
	}
}


/* Resolve every real metadata key and keep missing/inactive opcode diagnostics bounded. */
static void
names(void)
{
	uchar saved[sizeof(keywds)], row[sizeof(keywds[0])];
	int i, op, variant;
	char *wanted, *actual;

	memmove(saved, keywds, sizeof(saved));
	for(variant = 0; variant < 3; variant++){
		memmove(keywds, saved, sizeof(saved));
		if(variant == 1){
			memmove(row, &keywds[0], sizeof(row));
			memmove(&keywds[0], &keywds[2], sizeof(row));
			memmove(&keywds[2], row, sizeof(row));
		}else if(variant == 2)
			keywds[nelem(keywds)-2].name = nil; /* A valid shorter metadata table ends at its first nil name. */
		for(op = -1; op <= 255; op++){
			wanted = op < 0 || op >= MAXDIS ? "Unknown" : "<Noname>";
			for(i = 0; keywds[i].name != nil; i++)
				if(op >= 0 && op < MAXDIS && keywds[i].op == op){
					wanted = keywds[i].name;
					break;
				}
			actual = opname(op);
			check(actual != nil && !strcmp(actual, wanted));
		}
	}
	memmove(keywds, saved, sizeof(saved));
}


/* Reject out-of-program PCs before decoder/instrumenthandler access in every device executor. */
static void
badfetch(int which)
{
	prepare(which == 6 ? ISPAWN : IRUNT, 2, 0);
	if(which == 6) childmissing = 1;
	if(which == 0) process.R.PC = (Inst*)(text+2*sizeof(Inst));
	if(which == 1) process.R.PC = (Inst*)(text+1);
	if(which == 2) process.R.PC = nil;
	if(which == 3) process.R.PC = H;
	if(which == 4) module.nprog = 0;
	if(which == 5){ module.nprog = 1; process.quanta = 2; control.step = 2; }
	raised = nil;
	if(setjmp(boundary) == 0){
		EXECUTE(&process);
		check(0);
	}
	check(raised == (which == 6 ? exSpawn : exBounds) && executed == (which >= 5 ? 1 : 0));
	check(depth == 0);
#ifdef TEST_DEBUG
	check(closed == 1 && control.ref == 1);
#endif
}


/* Keep rejected metadata in fresh processes while retaining ASan stack checks. */
static void
bounds(void)
{
	pid_t pid;
	int i, status;

	for(i = 0; i < 7; i++){
		pid = fork();
		check(pid >= 0);
		if(pid == 0){
			badfetch(i);
			childexit(0);
		}
		check(waitpid(pid, &status, 0) == pid);
		check(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
}


/* Select complete or independent parent-replay matrices for each real device body. */
int
main(void)
{
	char *family;

	family = getenv("INSTRUMENT_CASE");
	if(family == nil || !strcmp(family, "normal")) normal();
	if(family == nil || !strcmp(family, "alias")) alias();
	if(family == nil || !strcmp(family, "yield")) yields();
	if(family == nil || !strcmp(family, "native")) nativecontext();
	if(family == nil || !strcmp(family, "notify")) notifications();
	if(family == nil || !strcmp(family, "bounds")) bounds();
#ifndef TEST_DEBUG
	if(family == nil || !strcmp(family, "reset")) resets();
#endif
#ifdef TEST_BENCH
	if(family == nil || !strcmp(family, "lookup")) lookupbounds();
	if(family == nil || !strcmp(family, "names")) names();
	if(family == nil || !strcmp(family, "compiled")) compiledhooks();
	if(family == nil || !strcmp(family, "reentry")) reentrancy();
#endif
	cleanup();
	check(checks != 0);
	printf("PASS instrumented VM %s WORD%zu %s: %lu parent predicates\n", INSTRUMENT_LABEL, sizeof(WORD)*8, family == nil ? "all" : family, checks);
	return 0;
}
