#include "native64-arguments.h"

char exNomem[] = "out of memory: main"; // Match the runtime allocation diagnostic from raise.c.

static void *contextblocks[64]; // Real mallocz allocations owned by Type/vector/map/scratch state.
static int contextlive, contextattempt, contextfailure; // Exact C allocation accounting and refusal point.
static Native64Arguments *observedcontext; // Caller-owned context observed during retirement.
static int contextfreediagnostic; // Exercise diagnostic preservation during native output retirement.
static int disturbdiagnostic, mutate_type; // Explicit cleanup/race test boundaries, never production policy.
static Type *mutation_type; // Trusted descriptor whose reference reservation changes during allocation.
static int prefixchecks, referencechecks, heapinitializations, cleanupobservations; // Active phase observations.
static int framecontrol, typecontrol, contexttransactions; // Independent root/Type removal controls and completed cases.
extern void __real_free(void*);


/* Track actual native allocations without replacing any constructor or Type statement. */
void*
mallocz(ulong bytes, int zero)
{
	void *value;
	int i;

	contextattempt++;
	if(contextattempt == contextfailure)
		return nil;
	if(mutate_type && contextattempt == 4)
		mutation_type->ref = 0x7fffffff;
	value = malloc(bytes ? bytes : 1);
	check(value != nil);
	if(zero) memset(value, 0, bytes);
	for(i=0; i<nelem(contextblocks); i++)
		if(contextblocks[i] == nil){
			contextblocks[i] = value;
			contextlive++;
			return value;
		}
	check(0);
	return nil;
}


/* Observe real frees from both owning heap.o and snapshot buffers; other runtime allocations use libc normally. */
void
__wrap_free(void *pointer)
{
	int i;

	for(i=0; pointer != nil && i<nelem(contextblocks); i++)
		if(contextblocks[i] == pointer){
			contextblocks[i] = nil;
			contextlive--;
			if(contextfreediagnostic){
				cleanupobservations++;
				kwerrstr("%s", "native output cleanup changed error");
			}
			if(disturbdiagnostic && observedcontext != nil && native64argumentsempty(observedcontext)){
				cleanupobservations++;
				kwerrstr("%s", "argument cleanup changed error");
			}
			break;
		}
	__real_free(pointer);
}


/* Confirm dynamic Type identity remains allocated before reading its ref count. */
static int
contextallocated(void *pointer)
{
	int i;

	for(i=0; i<nelem(contextblocks); i++)
		if(contextblocks[i] == pointer) return 1;
	return 0;
}


/* Allocation monitors may observe initialized fields, but must not yield, allocate or throw. */
static void
contextmonitor(int freeing, void *pointer, ulong bytes)
{
	Heap *heap;
	uchar *map, *frame;
	void *value;
	int slot;

	USED(bytes);
	heap = pointer;
	if(freeing || bytes != 1192)
		return;
	frame = H2D(void*, heap);
	map = (uchar*)heap->t+offsetof(Type, map);
	for(slot=0; slot<heap->t->size/8; slot++)
		if(map[slot/8] & (0x80 >> (slot%8))){
			memmove(&value, frame+slot*8, sizeof(value));
			check(value == H);
		}
	heapinitializations++;
}


/* Phase0 checks only canonical nil bits; phase1 may inspect the trusted live fixed argument. */
static int
contextprefix(void *argument, const void *bytes, unsigned int size, int references)
{
	void *value;

	USED(argument);
	prefixchecks++;
	if(size != sizeof(value)) return 0;
	memmove(&value, bytes, sizeof(value));
	if(value == nil) return 0;
	return !references || value == H || (D2H(value)->t == &Tarray && D2H(value)->ref > 0);
}


/* Live tail references require exact Type identity and valid existing owners. */
static int
contextreference(void *argument, void *value, Type *type)
{
	USED(argument);
	referencechecks++;
	return D2H(value)->t == type && D2H(value)->ref > 0;
}


/* Initialize the native process/error/collector services before each independent transaction. */
static void
contextstart(int phase, int state)
{
	startcase(phase, state);
	clearrootcache();
	check(contextlive == 0);
	contextattempt = contextfailure = contextheapfail = failpin = mutate_type = 0;
	prefixchecks = referencechecks = heapinitializations = cleanupobservations = 0;
	observedcontext = nil;
	disturbdiagnostic = 0;
	memset(&errorworker, 0, sizeof(errorworker));
	memset(&errorenvironment, 0, sizeof(errorenvironment));
	up = &errorworker;
	up->env = &errorenvironment;
	up->env->errstr = up->env->errbuf0;
	kwerrstr("%s", "initial argument error");
	heapmonitor = contextmonitor;
}


/* Construct a generated caller and a wide private context, forcing each independent owner failure. */
static void
contextcase(int phase, int state, int mode)
{
	uchar fixedmap[] = {0, 0x80};
	LimboNative64Entry fixed = {"fixed", nil, 0, 72, 2, fixedmap};
	Native64TailSlot slots[140];
	uvlong values[140];
	Native64ArgumentSource source;
	Native64Arguments owned;
	F_Roots_caller *caller;
	Type *caller_type, *empty_type;
	Array *first, *last;
	void *prefix, *value;
	uvlong scalar;
	int i, before, status, expected, baseallocations;
	char saved[ERRMAX];

	contextstart(phase, state);
	caller_type = native64frametype(&limbo_native64_Roots_caller_entry);
	check(caller_type != nil);
	caller = (F_Roots_caller*)stackframes(caller_type, 1);
	first = H2D(Array*, heaparray(&Tbyte, 1));
	last = H2D(Array*, heaparray(&Tbyte, 1));
	first->data[0] = 0x21; last->data[0] = 0x72;
	caller->result = first; caller->tail = last;
	prefix = first;
	for(i=0; i<140; i++){
		slots[i].kind = Native64TailBig; slots[i].offset = i*8;
		slots[i].bytes = 8; slots[i].type = nil; values[i] = ~(uvlong)0;
	}
	slots[0].kind = slots[139].kind = Native64TailRef;
	slots[0].type = slots[139].type = &Tarray;
	values[0] = (uvlong)(uintptr)first; values[139] = (uvlong)(uintptr)last;
	empty_type = nil;
	if(mode == 8 || mode == 9){
		uchar empty_map[] = {0};
		empty_type = dtype(freeheap, 8, empty_map, 1);
		check(empty_type != nil);
		slots[0].type = slots[139].type = empty_type;
		values[0] = values[139] = (uvlong)(uintptr)H;
		if(mode == 9) slots[0].type = &Tarray;
	}
	memset(&source, 0, sizeof(source));
	source.fixed=&fixed; source.prefix=&prefix; source.prefix_bytes=8;
	source.slots=slots; source.count=140; source.values=values; source.value_bytes=sizeof(values);
	source.prefix_check=contextprefix; source.reference_check=contextreference;
	native64argumentsinit(&owned);
	observedcontext = &owned;
	disturbdiagnostic = 1;
	contextattempt = 0;
	baseallocations = contextlive;
	if(mode < 5) contextfailure = mode+1;
	if(mode == 5) contextheapfail = 1;
	if(mode == 6) failpin = 1;
	if(mode == 7) D2H(first)->ref = 0x7ffffffe;
	if(mode == 9){ mutate_type=1; mutation_type=empty_type; }
	allocationcollections = 1;
	status = native64argumentsbuild(&owned, &source);
	allocationcollections = contextheapfail = failpin = mutate_type = 0;
	expected = mode == 8 || mode == 10;
	check(status == expected && errdepth == 0 && !freed[2] && !freed[3]);
	if(!status){
		check(native64argumentsempty(&owned) && owned.plan.size == 0 && contextlive == baseallocations);
		check(D2H(first)->ref == (mode == 7 ? 0x7ffffffe : 1) && D2H(last)->ref == 1);
		check(Tarray.ref == 3);
		if(mode < 5) check(strcmp(up->env->errstr, exNomem) == 0);
		if(mode == 5) check(strcmp(up->env->errstr, exHeap) == 0);
		if(mode == 6) check(strcmp(up->env->errstr, "ptradd malloc") == 0);
		if(mode == 7) check(strcmp(up->env->errstr, "bad native64 argument owner") == 0);
		if(mode == 9){
			check(strcmp(up->env->errstr, "bad native64 argument Type owner") == 0 && empty_type->ref == 0x7fffffff);
			empty_type->ref = 1;
		}
		if(mode == 7) D2H(first)->ref = 1;
	}else{
		check(owned.type->ref == 2 && owned.frame.rooted == 1 && owned.plan.size == 1192 && owned.plan.map_bytes == 19);
		check(owned.values == nil && owned.prefix == nil && owned.slots != slots && owned.map != fixedmap);
		check(D2H(first)->ref == (mode == 8 ? 2 : 3) && D2H(last)->ref == (mode == 8 ? 1 : 2));
		check(contextlive == baseallocations+3);
		before = contextattempt;
		check(!native64argumentsbuild(&owned, &source) && contextattempt == before);
		if(empty_type != nil){
			if(typecontrol){
				for(i=0; i<owned.typed; i++) if(owned.slots[i].kind == Native64TailRef) freetype(owned.slots[i].type);
				owned.typed = 0;
			}
			freetype(empty_type);
			check(contextallocated(owned.slots[0].type) && owned.slots[0].type->ref == 2);
			empty_type = nil; baseallocations--;
		}
		/* Caller buffers may retire after success; the context's snapshots and managed owners remain. */
		memset(slots, 0xa5, sizeof(slots)); memset(values, 0xa5, sizeof(values)); fixedmap[0] = 0xff; prefix = H;
		check(owned.slots[139].kind == Native64TailRef && owned.slots[139].offset == 139*8);
		if(framecontrol){ ptrdel(D2H(owned.frame.value)); owned.frame.rooted = 0; }
		if(mode == 10){
			caller->result = caller->tail = H; destroy(first); destroy(last);
		}
		epochs(6);
		check(!freed[4]);
		memmove(&value, (uchar*)owned.frame.value+64, sizeof(value));
		check(value == first && first->data[0] == 0x21);
		memmove(&scalar, (uchar*)owned.frame.value+owned.plan.tail+8, sizeof(scalar));
		check(scalar == ~(uvlong)0);
		kwerrstr("%s", "context retirement error");
		native64argumentsclear(&owned);
		check(strcmp(up->env->errstr, "context retirement error") == 0 && native64argumentsempty(&owned));
		check(contextlive == baseallocations && freed[4] == 1);
	}
	kgerrstr(saved, sizeof(saved));
	native64argumentsclear(&owned);
	check(strcmp(up->env->errstr, saved) == 0);
	if(empty_type != nil) freetype(empty_type);
	if(mode != 10){
		freeptrs(caller, caller_type); initmem(caller_type, caller);
	}
	process.R.EX = process.R.SP = nil;
	epochs(6);
	freetype(caller_type);
	clearrootcache();
	check(contextlive == 0 && Tarray.ref == 1 && Tbyte.ref == 1 && errdepth == 0);
	heapmonitor = nil;
	observedcontext = nil;
	contexttransactions++;
}



typedef struct ArgumentTransaction ArgumentTransaction;

struct ArgumentTransaction
{
	F_Roots_caller *caller; // Generated mapped logical result owner, live throughout the transaction.
	int mode; // Target/result failures plus constructor and canonical-input refusals.
	int produced, resolved; // Exact callback entry counts.
};


/* Read actual mapped arguments after collection and adopt the result before any later failure. */
static int
argumenttransactionproduce(Native64Result *result, const Native64Arguments *arguments, void *state)
{
	ArgumentTransaction *transaction;
	Array *input, *output;

	transaction = state;
	transaction->produced++;
	epochs(6);
	memmove(&input, (uchar*)arguments->frame.value+64, sizeof(input));
	check(input->data[0] == 0x21 && arguments->frame.rooted && arguments->type->ref == 2);
	output = H2D(Array*, heaparray(&Tbyte, 1));
	output->data[0] = input->data[0]+1;
	if(transaction->mode == 3) failpin=1;
	check(native64resultadopt(result, output));
	epochs(6);
	if(transaction->mode == 1) error("argument producer failure");
	if(transaction->mode == 2){ kwerrstr("%s", "argument producer decline"); return 0; }
	return 1;
}


/* Resolve the current generated destination only after the producer returns successfully. */
static void*
argumenttransactionresolve(void *state)
{
	ArgumentTransaction *transaction;

	transaction = state;
	transaction->resolved++;
	epochs(6);
	if(transaction->mode == 4){ kwerrstr("%s", "argument resolver decline"); return nil; }
	if(transaction->mode == 5) error("argument resolver failure");
	return &transaction->caller->result;
}


/* Exercise combined target/result/argument retirement with actual collector and both error owners. */
static void
argumenttransactioncase(int phase, int state, int mode)
{
	uchar map[] = {0, 0x80};
	LimboNative64Entry fixed = {"fixed", nil, 0, 72, 2, map};
	Native64TailSlot slot = {Native64TailRef, 0, 8, &Tarray};
	Native64ArgumentSource source;
	ArgumentTransaction transaction;
	Type *caller_type;
	Array *first, *last;
	void *prefix;
	uvlong value;
	int baseline, status;
	char *expected[] = {"initial argument error", "argument producer failure", "argument producer decline",
		"ptradd malloc", "argument resolver decline", "argument resolver failure",
		"out of memory: main", "initial argument error"};

	contextstart(phase, state);
	caller_type = native64frametype(&limbo_native64_Roots_caller_entry);
	check(caller_type != nil);
	memset(&transaction, 0, sizeof(transaction));
	transaction.caller = (F_Roots_caller*)stackframes(caller_type, 1);
	transaction.mode = mode;
	first = H2D(Array*, heaparray(&Tbyte, 1));
	last = H2D(Array*, heaparray(&Tbyte, 1));
	first->data[0] = 0x21; last->data[0] = 0x72;
	transaction.caller->result = first; transaction.caller->tail = last;
	prefix = first; value = (uvlong)(uintptr)last;
	memset(&source, 0, sizeof(source));
	source.fixed=&fixed; source.prefix=&prefix; source.prefix_bytes=8;
	source.slots=&slot; source.count=1; source.values=&value; source.value_bytes=8;
	source.prefix_check=contextprefix; source.reference_check=contextreference;
	baseline = contextlive;
	check(!native64argumentscall(&source, nil, argumenttransactionresolve, &transaction));
	check(contextlive == baseline && transaction.produced == 0);
	if(mode == 6) contextfailure = contextattempt+3;
	if(mode == 7) value = 0;
	allocationcollections = 1;
	status = native64argumentscall(&source, argumenttransactionproduce, argumenttransactionresolve, &transaction);
	allocationcollections = failpin = 0;
	check(status == (mode == 0) && transaction.produced == (mode < 6));
	check(transaction.resolved == (mode == 0 || mode == 4 || mode == 5));
	check(contextlive == baseline && errdepth == 0 && freed[4] == (mode < 6));
	check(strcmp(up->env->errstr, expected[mode]) == 0);
	check(transaction.caller->tail == last && last->data[0] == 0x72 && D2H(last)->ref == 1);
	if(mode == 0){
		check(transaction.caller->result != first && transaction.caller->result->data[0] == 0x22);
		check(freed[2] == 1 && !freed[5]);
	}else{
		check(transaction.caller->result == first && first->data[0] == 0x21 && D2H(first)->ref == 1);
		check(!freed[2] && freed[5] == (mode < 6));
	}
	epochs(6);
	freeptrs(transaction.caller, caller_type); initmem(caller_type, transaction.caller);
	process.R.EX = process.R.SP = nil;
	epochs(6);
	freetype(caller_type); clearrootcache();
	check(contextlive == 0 && Tarray.ref == 1 && Tbyte.ref == 1 && errdepth == 0);
	heapmonitor = nil;
}


/* Exercise all partial acquisition stages, Type-only H slots and long-lived copied buffers in actual VM roots. */
int
main(int argc, char **argv)
{
	int mode, phase, state;

	check(argc == 1 || argc == 2);
	if(argc == 2){
		if(strcmp(argv[1], "frame") == 0) framecontrol=1;
		else if(strcmp(argv[1], "types") == 0) typecontrol=1;
		else check(0);
	}
	arrayfree = Tarray.free;
	for(phase=0; phase<3; phase++)
		for(state=Palt; state<=Pbroken; state++)
			for(mode=0; mode<11; mode++)
				contextcase(phase, state, mode);
	for(phase=0; phase<3; phase++)
		for(state=Palt; state<=Pbroken; state++)
			for(mode=0; mode<8; mode++)
				argumenttransactioncase(phase, state, mode);
	check(contexttransactions == 264);
	printf("PASS managed arguments: %d predicates, 264 owner-balanced constructions + 192 combined C transactions, actual heap/Type/collector and partial failures\n", checks);
	return 0;
}
