/* Appended after actual heap/collector services by native64-result.py. */
#include <setjmp.h>
#include "native64-result.h"
#undef up
#undef waserror
#undef poperror

static Proc errorworker; // Public process/environment route for actual kgerrstr bodies.
static Osenv errorenvironment; // Stable VM error string, outside protected C frames.
Proc *up; // Explicit current-process boundary for the selected port bodies.
static jmp_buf errorframes[16]; // Host implementation of the platform error-label boundary.
static int errdepth, transactions, resultcallbacks; // Error-stack balance and real transaction observations.

#define waserror() (errdepth++, setjmp(errorframes[errdepth-1])) // Keep setjmp in actual kprotect's own frame.
#define poperror() (--errdepth) // Match successful protected-frame retirement.


/* Preserve ordinary VM error publication while using the host's portable nonlocal boundary. */
void
kwerrstr(char *format, ...)
{
	va_list args;

	va_start(args, format);
	vsnprintf(up->env->errstr, ERRMAX, format, args);
	va_end(args);
}


/* Model only the platform jump, not the actual protected callback body. */
void
nexterror(void)
{
	check(errdepth > 0 && errdepth <= nelem(errorframes));
	longjmp(errorframes[--errdepth], 1);
}


/* A real collector pin allocation failure and producer error enter the same VM boundary. */
void
error(char *message)
{
	check(errdepth > 0);
	kstrcpy(up->env->errstr, message, ERRMAX);
	nexterror();
}

PROTECTED_IMPLEMENTATION

/* Private fixture modes, never shared call or wire IDs. */
enum {
	ResultNormal, // Allocate, pin, collect and publish a new owner.
	ResultBefore, // Throw before acquiring a new result owner.
	ResultDecline, // Decline after acquiring and pinning an owner.
	ResultError, // Throw after the result has survived collector epochs.
	ResultPinFail, // Actual ptradd metadata allocation throws after adoption.
	ResultAlias, // Two owned references to the same object replace one another.
	ResultAliasPinFail, // Failed new pin must not remove a pre-existing alias pin.
	ResultNil, // A successful H result replaces and releases the previous owner.
	ResultBadAdopt, // A nil C pointer is refused without ownership transfer.
	ResultModes // Number of private producer behaviors.
};

typedef struct ResultTest ResultTest;

struct ResultTest {
	F_Roots_caller *caller; // Generated mapped slot that owns the pre-existing result.
	Array *old; // Borrowed former identity, inspected only while its owner is live.
	int mode; // Select one deterministic allocation/error boundary.
	int ok; // Protected helper status remains outside both actual kprotect frames.
	int rethrow; // Exercise actual krethrow after protected cleanup.
	int basepins; // Independent alias roots that this guard must leave intact.
	char *expected; // Original failure text must survive destructive cleanup callbacks.
};

static ResultTest *activecase; // Stable caller context during actual object destruction.
static Native64Result *activeguard; // Borrowed guard only while native64resultcall is active.


/* Count genuine external-root entries without treating frame roots as pins. */
static int
rootcount(void *value)
{
	Ptrhash *entry;
	int bucket, count;

	count = 0;
	if(value == H)
		return count;
	for(bucket=0; bucket<PTRHASH; bucket++)
		for(entry=ptrtab[bucket]; entry != nil; entry=entry->next)
			if(entry->value == D2H(value))
				count++;
	return count;
}


/* Force actual ptradd's next fresh allocation rather than silently reusing its cache. */
static void
clearrootcache(void)
{
	Ptrhash *entry;
	int bucket;

	for(bucket=0; bucket<PTRHASH; bucket++)
		check(ptrtab[bucket] == nil);
	while(ptrfree != nil){
		entry = ptrfree;
		ptrfree = entry->next;
		free(entry);
	}
}


/* Identify successful producers without conflating a declined callback with a thrown error. */
static int
successmode(int mode)
{
	return mode == ResultNormal || mode == ResultAlias || mode == ResultNil;
}


/* Observe detached guard state and removed pins before delegating actual array destruction. */
static void
resultfree(Heap *heap, int swept)
{
	ulong before, halted;

	check(!swept && heap->ref == 0 && gchalt > 0);
	check(activeguard->value == H && activeguard->rooted == 0);
	check(rootcount(H2D(void*, heap)) == 0);
	check(rootcount(activecase->caller->result) == activecase->basepins);
	if(!successmode(activecase->mode)){
		check(activecase->caller->result == activecase->old);
		check(strcmp(up->env->errstr, activecase->expected) == 0);
	}else
		check(activecase->caller->result != activecase->old);
	before = gcepochs;
	halted = gchalted;
	rungc(&process);
	check(gcepochs == before && gchalted == halted+1);
	arrayfree(heap, swept);
	kwerrstr("%s", "destructor changed error");
	resultcallbacks++;
}


/* Produce actual heap values and real pin-allocation/VM failures at controlled boundaries. */
static int
producer(Native64Result *guard, void *argument)
{
	ResultTest *test;
	Array *value;
	int basepins;

	test = argument;
	activeguard = guard;
	if(test->mode == ResultBefore)
		error("before allocation");
	if(test->mode == ResultNil)
		return native64resultadopt(guard, H);
	if(test->mode == ResultBadAdopt){
		check(!native64resultadopt(guard, nil));
		check(guard->value == H && guard->rooted == 0);
		kwerrstr("%s", test->expected);
		return 0;
	}
	basepins = 0;
	if(test->mode == ResultAlias || test->mode == ResultAliasPinFail){
		value = test->old;
		D2H(value)->ref++;
		basepins = test->basepins;
	}else{
		value = H2D(Array*, heaparray(&Tbyte, 4));
		memset(value->data, 0x5a, 4);
	}
	check(rootcount(value) == basepins);
	if(test->mode == ResultPinFail || test->mode == ResultAliasPinFail)
		failpin = 1;
	check(native64resultadopt(guard, value));
	check(rootcount(value) == basepins+1 && guard->rooted == 1);
	check(!native64resultadopt(guard, value));
	epochs(6);
	check(D2H(value)->ref == (value == test->old ? 2 : 1));
	if(test->mode == ResultDecline){
		kwerrstr("%s", test->expected);
		return 0;
	}
	if(test->mode == ResultError)
		error("after pin");
	return 1;
}


/* Propagate the original error only after native64resultcall has completed its owner cleanup. */
static int
outercall(void *argument)
{
	ResultTest *test;

	test = argument;
	test->ok = native64resultcall(&test->caller->result, producer, test);
	if(!test->ok && test->rethrow)
		krethrow();
	return 1;
}


/* Keep the old owner, unrelated alias pins and actual error-stack depth coherent in every mode. */
static void
transaction(const LimboNative64Entry *record, int mode, int phase, int state, int rethrow)
{
	ResultTest test;
	Type *type;
	int result;

	startcase(phase, state);
	clearrootcache();
	failpin = 0;
	memset(&errorworker, 0, sizeof(errorworker));
	memset(&errorenvironment, 0, sizeof(errorenvironment));
	up = &errorworker;
	up->env = &errorenvironment;
	up->env->errstr = up->env->errbuf0;
	kwerrstr("%s", "initial error");
	type = native64frametype(record);
	check(type != nil);
	memset(&test, 0, sizeof(test));
	test.caller = (F_Roots_caller*)stackframes(type, 1);
	test.old = H2D(Array*, heaparray(&Tbyte, 4));
	memset(test.old->data, 0x33, 4);
	test.caller->result = test.old;
	test.mode = mode;
	test.rethrow = rethrow;
	test.expected = mode == ResultBefore ? "before allocation" : mode == ResultDecline ? "declined result" :
		mode == ResultBadAdopt ? "nil result refused" : mode == ResultError ? "after pin" : "ptradd malloc";
	if(mode == ResultAliasPinFail){
		ptradd(D2H(test.old));
		test.basepins = 1;
	}
	activecase = &test;
	Tarray.free = resultfree;
	result = kprotect(outercall, &test);
	Tarray.free = arrayfree;
	activeguard = nil;
	failpin = 0;
	check(errdepth == 0 && gchalt == 0);
	check(test.ok == successmode(mode));
	check(result == (successmode(mode) || !rethrow));
	if(!test.ok){
		check(strcmp(up->env->errstr, test.expected) == 0);
		check(test.caller->result == test.old && D2H(test.old)->ref == 1 && !freed[2]);
	}else if(mode == ResultNil)
		check(test.caller->result == H && freed[2] == 1);
	else
		check(test.caller->result != H && D2H(test.caller->result)->ref == 1);
	check(rootcount(test.caller->result) == test.basepins);
	if(test.basepins)
		ptrdel(D2H(test.old));
	epochs(6);
	freeptrs(test.caller, type);
	initmem(type, test.caller);
	process.R.EX = process.R.SP = nil;
	epochs(6);
	clearrootcache();
	check(Tarray.ref == 1 && Tbyte.ref == 1);
	freetype(type);
	transactions++;
}


/* Exercise actual port protection/rethrow bodies around real heap/pin/collector operations. */
int
main(void)
{
	const LimboNative64Entry *selected[] = {&limbo_native64_Roots_caller_entry};
	const LimboNative64Entry *bound[1];
	int mode, phase, state, rethrow;
	void *empty = H; // Preflight must reject a missing producer without touching its owner cell.

	arrayfree = Tarray.free;
	check(limbo_native64_Roots_layout());
	check(native64bind(limbo_native64_Roots_specs, limbo_native64_Roots_spec_count,
		selected, nelem(selected), bound, nelem(bound)));
	check(!native64resultcall(nil, producer, nil));
	check(!native64resultcall(H, producer, nil));
	check(!native64resultcall(&empty, 0, nil) && empty == H);
	check(!native64resultcall((void*)((uintptr)-1-2), producer, nil));
	for(mode=0; mode<ResultModes; mode++)
	for(phase=0; phase<3; phase++)
	for(state=Palt; state<=Pbroken; state++)
	for(rethrow=0; rethrow<2; rethrow++)
		transaction(bound[0], mode, phase, state, rethrow);
	printf("PASS protected native64 results: %d predicates, %d transactions, %d destructor checkpoints, balanced actual pins/error frames\n", checks, transactions, resultcallbacks);
	return 0;
}
