#include "isa.h"
#include <setjmp.h>


/* Report the selected production or fixture statement before the active predicate aborts. */
static void
epcheck(int condition, int line)
{
	if(!condition)
		fprintf(stderr, "exception/GC predicate at generated line %d\n", line);
	check(condition);
}

#define check(condition) epcheck(!!(condition), __LINE__) // Preserve active counts with a source location.

static union {
	void *alignment; // Native extent-header alignment, with ordinary Frame64-sized storage.
	uchar bytes[sizeof(Stkext)+160]; // Complete root frame and mapped exception slot.
} exceptionstack;

static Handler handlers[2]; // One native table and its ordinary terminator.
static Except entries[2]; // One named destination and an absent fallback.
static Inst instructions[2]; // Source instruction followed by a resumable target.
static void *exceptionowner; // Borrowed identity, never dereferenced after release.
static int diagnosticcollections, blockedcollections, selected, unselected; // Actual handler/collector observations.
Type Tstring;
REG R;
char exHeap[]="heap", exNilref[]="nil", exType[]="type", exNomem[]="memory";
static jmp_buf raiseboundary; // Expected instruction error transfer, outside collector callbacks.
static int raiseactive, raisecalls; // Only a real RAISE may enter this fixture's nonlocal boundary.
static char raisedmessage[16]; // Copied text remains usable after the source String is reclaimed.
static void *raisesource; // Native source slot, stable across the actual RAISE longjmp.

#define OP(name) void name(void) // Preserve the production instruction entry signature.

/* These selectors describe fixture profiles, never wire opcode or extension IDs. */
enum {
	Transfer, // Move a process exception into a mapped frame slot.
	Cleanup, // Clear an existing mapped owner before transfer.
	Unmatched, // Release the process exception without selecting a table.
	Empty, // Empty terminal text must still release the process-owned exception.
	Alias, // Existing slot and process root own separate references to the same object.
	Profiles // Number of independent ownership profiles.
};


/* Capture actual RAISE after it has prepared the message and retained the process value. */
static void
raiseerror(char *text)
{
	check(raiseactive);
	snprintf(raisedmessage, sizeof(raisedmessage), "%s", text);
	raisecalls++;
	longjmp(raiseboundary, 1);
}


/* ASCII messages never request a Rune conversion cache. */
ulong
msize(void *pointer)
{
	(void)pointer;
	check(0);
	return 0;
}


/* The fixture's real string2c calls must remain on their validated ASCII path. */
int
runetochar(char *destination, Rune *rune)
{
	(void)destination;
	(void)rune;
	check(0);
	return 0;
}


/* Expose the actual process root without replacing production handler selection. */
Prog*
currun(void)
{
	return &process;
}


/* A native pool consistency failure must never be mistaken for successful cleanup. */
static void
badpool(void *pointer, char *text, ulong caller)
{
	(void)pointer;
	(void)text;
	(void)caller;
	check(0);
}

void (*poolfault)(void*, char*, ulong) = badpool;


/* Call-site diagnostics have no bearing on the verified native pool identity. */
uintptr
getcallerpc(void *argument)
{
	(void)argument;
	return 0;
}


/* Tables transfer an existing exception; fresh message allocation has separate fixtures. */
static String*
newestring(char *text)
{
	(void)text;
	check(0);
	return H;
}


/* A single root frame never takes the cross-extent unwind branch. */
void
unextend(Frame *frame)
{
	(void)frame;
	check(0);
}


/* Supply bounded diagnostics while preserving the actual selected PC and owner state. */
WORD
modstatus(REG *registers, char *name, int bytes)
{
	check(registers == &R && bytes > 8 && R.M == rootlink);
	strcpy(name, "gc-owner");
	return 0;
}


/* Collect while the process root still owns its exception, before any transfer. */
void*
realloc(void *pointer, ulong bytes)
{
	check(pointer == nil && bytes > 0 && process.exval == exceptionowner);
	epochs(6);
	check(!freed[2]);
	diagnosticcollections++;
	return nil;
}


/* Failed diagnostics allocation prevents formatting, but must not suppress selection. */
int
snprint(char *destination, int bytes, char *format, ...)
{
	(void)destination;
	(void)bytes;
	(void)format;
	check(0);
	return 0;
}


EXCEPTION_IMPLEMENTATION


/* Observe real destructor callbacks: C cleanup protects itself against a collector slice. */
static void
duringdestroy(Heap *heap, int swept)
{
	ulong epoch, halted;
	Bhdr *cursor;

	check(swept ? heap->ref > 0 : heap->ref == 0);
	if(H2D(void*, heap) == exceptionowner)
		check(process.exval == H);
	if(!swept){
		check(gchalt > 0);
		epoch = gcepochs;
		halted = gchalted;
		cursor = ptr;
		rungc(&process);
		check(gcepochs == epoch && gchalted == halted+1 && ptr == cursor);
		blockedcollections++;
	}
	destructions++;
	freeheap(heap, swept);
}


/* Prepare a genuine native root/frame graph, not a mapped Dis64 wire image. */
static void
prepare(int tuple, int profile, int phase)
{
	Stkext *extent;
	Frame frame;
	void *message, *child, *old, *nilvalue;

	setup();
	gccolor = 3+phase;
	mutator = gccolor%3;
	marker = (gccolor-1)%3;
	sweeper = (gccolor-2)%3;
	D2H(rootlink)->color = mutator;
	Tstring = leaf;
	Tstring.size = sizeof(String);
	exceptionowner = object(2, tuple ? &pair : &Tstring);
	message = exceptionowner;
	if(tuple){
		message = object(3, &Tstring);
		child = object(4, &pair);
		nilvalue = H;
		memmove(exceptionowner, &message, sizeof(message));
		memmove((uchar*)exceptionowner+sizeof(void*), &child, sizeof(child));
		memmove(child, &exceptionowner, sizeof(exceptionowner));
		memmove((uchar*)child+sizeof(void*), &nilvalue, sizeof(nilvalue));
		D2H(exceptionowner)->ref++;
	}
	((String*)message)->len = profile == Empty ? 0 : 6;
	((String*)message)->max = 6;
	memmove(((String*)message)->Sascii, "signal", 7);
	process.exval = H;
	old = exceptionowner;
	if(profile == Alias)
		D2H(old)->ref++;
	else
		old = object(5, &Tstring);
	framekind = leaf;
	framekind.size = 72;
	framekind.np = 2;
	framekind.map[0] = 0;
	*((uchar*)&framekind+O(Type, map)+1) = 0x80;
	memset(&exceptionstack, 0, sizeof(exceptionstack));
	extent = (Stkext*)exceptionstack.bytes;
	extent->reg.TR = &framekind;
	memset(&frame, 0, sizeof(frame));
	memmove(extent->reg.tos.fu, &frame, sizeof(frame));
	memmove(extent->reg.tos.fu+64, &old, sizeof(old));
	process.R.EX = extent->stack;
	process.R.FP = extent->reg.tos.fu;
	process.R.SP = process.R.FP+framekind.size;
	process.R.TS = exceptionstack.bytes+sizeof(exceptionstack);
	memset(handlers, 0, sizeof(handlers));
	memset(entries, 0, sizeof(entries));
	handlers[0].pc2 = 1;
	handlers[0].eoff = 64;
	handlers[0].ne = tuple;
	handlers[0].t = profile == Cleanup ? &framekind : nil;
	handlers[0].etab = entries;
	entries[0].s = profile == Unmatched ? "different" : "*";
	entries[0].pc = 1;
	entries[1].pc = (ulong)-1;
	owner.prog = rootlink->prog = instructions;
	owner.nprog = 2;
	owner.htab = handlers;
	process.R.PC = instructions+1;
	R = process.R;
}


/* Collect before selection, after transfer and after removing the final mapped owner. */
static void
scenario(int tuple, int profile, int phase)
{
	void *slot;
	int matched, before;

	prepare(tuple, profile, phase);
	before = diagnosticcollections;
	raisesource = exceptionowner;
	R.s = &raisesource;
	raiseactive = 1;
	if(setjmp(raiseboundary) == 0){
		iraise();
		check(0);
	}
	raiseactive = 0;
	check(strcmp(raisedmessage, profile == Empty ? "" : "signal") == 0);
	check(process.exval == exceptionowner);
	/* The real instruction retained exval; release the fixture's original source owner. */
	destroy(raisesource);
	matched = handler(raisedmessage);
	check(matched == (profile != Unmatched && profile != Empty));
	check(process.exval == H);
	if(matched){
		selected++;
		check(diagnosticcollections == before+1);
		check(R.PC == instructions+1 && process.R.PC == R.PC);
		memmove(&slot, R.FP+64, sizeof(slot));
		check(slot == exceptionowner && !freed[2]);
		check(D2H(exceptionowner)->ref == (tuple ? 2 : 1));
	}else{
		unselected++;
		check(diagnosticcollections == before);
	}
	epochs(6);
	check(freed[2] == !matched);
	if(tuple)
		check(freed[3] == !matched && freed[4] == !matched);
	freeptrs(process.R.FP, &framekind);
	process.R.EX = process.R.SP = nil;
	R = process.R;
	epochs(6);
	check(freed[2] == 1 && !freed[1] && gchalt == 0);
	if(tuple)
		check(freed[3] == 1 && freed[4] == 1);
	if(profile != Alias)
		check(freed[5] == 1);
	check(Tstring.ref == 1 && pair.ref == 1 && leaf.ref == 1);
	exceptionowner = H;
}


/* Cross string/tuple payloads, ownership profiles and all initial collector colors. */
int
main(void)
{
	int tuple, profile, phase, cases, wanted;

	cases = wanted = 0;
	for(tuple=0; tuple<2; tuple++)
	for(profile=0; profile<Profiles; profile++)
	for(phase=0; phase<3; phase++){
		if(getenv("EXCEPTION_GC_PROFILE") && profile != atoi(getenv("EXCEPTION_GC_PROFILE")))
			continue;
		scenario(tuple, profile, phase);
		cases++;
		if(profile != Empty && profile != Unmatched)
			wanted++;
	}
	check(cases > 0 && selected == wanted && unselected == cases-wanted);
	check(diagnosticcollections == selected && blockedcollections > 0 && raisecalls == cases);
	printf("PASS exception/GC: %d predicates, selected=%d unhandled=%d diagnostic-collections=%d blocked-destructor-collections=%d; actual RAISE/string2c/handler/destroy/rootset/markheap/rungc\n", checks, selected, unselected, diagnosticcollections, blockedcollections);
	return 0;
}
