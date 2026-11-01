#include "native64-tail.h"

/* Private fixture modes; these are neither callback ABI nor wire identifiers. */
enum {
	ResolvedMoveProducer, // Producer relocates the logical result before resolution.
	ResolvedMoveResolver, // Resolver relocates it after forcing another collection.
	ResolvedDecline, // Resolver declines with a diagnostic.
	ResolvedCancel, // Cancellation after relocation preserves the logical old result.
	ResolvedError, // Resolver throws before relocation.
	ResolvedMovedError, // Resolver throws after relocation.
	ResolvedBadH, // Reject H as a destination address.
	ResolvedBadWrap, // Reject a wrapping destination extent.
	ResolvedNilOwner, // Reject a nil value in an otherwise live owner cell.
	ResolvedAlias, // New result aliases its old owner across relocation.
	ResolvedNilResult, // H replaces an old owner after relocation.
	ResolvedNested, // An inner protected transaction must preserve outer pins and argument roots.
	ResolvedProducerDecline, // Producer declines before the resolver is called.
	ResolvedProducerError, // Producer throws before the resolver is called.
	ResolvedPinFailure, // New result pin allocation fails before resolution.
	ResolvedModes // Number of explicit transaction behaviors.
};

typedef struct ResolvedTest ResolvedTest;

struct ResolvedTest
{
	ResultTest base; // Reuse actual producer/pin/heap boundaries with an ordinary caller owner.
	F_Roots_caller *first; // The stale physical cell must remain H after relocation.
	F_Roots_caller *second; // A distinct mapped cell representing the relocated logical owner.
	uchar *arguments; // Borrowed rooted dynamic argument frame during the protected result call.
	Type *argument_type; // Context-owned Type reference until argument-frame retirement.
	int mode; // One fixture behavior above.
	int moved; // Whether logical ownership was transferred between mapped caller cells.
	int resolver_calls; // Verify that failed producers cannot reach the resolver.
	int disposing_nested; // Exclude the resolver's unrelated temporary from result-owner observations.
};

static ResolvedTest *resolvedtest; // Active fixture only during protected result cleanup.
static int stalecontrol, argumentcontrol; // Explicit negative controls, never production fallback.
static int resolvedtransactions; // Completed owner-balanced result transactions.

/* State whether this complete transaction is expected to publish. */
static int
resolvedsuccess(int mode)
{
	return mode == ResolvedMoveProducer || mode == ResolvedMoveResolver || mode == ResolvedAlias ||
		   mode == ResolvedNilResult || mode == ResolvedNested;
}


/* Transfer exactly one logical result owner; do not acquire or destroy it during relocation. */
static void
relocate(ResolvedTest *test)
{
	check(!test->moved && test->second->result == H);
	test->second->result = test->base.caller->result;
	test->base.caller->result = H;
	test->base.caller = test->second;
	test->moved = 1;
}


/* Observe detached temporary ownership and the correct logical destination before actual destruction. */
static void
resolvedfree(Heap *heap, int swept)
{
	if (resolvedtest->disposing_nested) {
		arrayfree(heap, swept);
		return;
	}
	check(!swept && heap->ref == 0 && gchalt > 0);
	check(activeguard->value == H && activeguard->rooted == 0);
	check(rootcount(H2D(void *, heap)) == 0);
	if (resolvedsuccess(resolvedtest->mode))
		check(resolvedtest->base.caller->result != resolvedtest->base.old);
	else {
		check(resolvedtest->base.caller->result == resolvedtest->base.old);
		check(strcmp(up->env->errstr, resolvedtest->base.expected) == 0);
	}
	arrayfree(heap, swept);
	kwerrstr("%s", "resolved destructor changed error");
}


/* Validate exact argument Type without modifying roots, values or descriptors. */
static int
resolvedreference(void *context, void *value, Type *type)
{
	USED(context);
	return D2H(value)->t == type;
}


/* Read actual typed argument storage only after verifying that its owning root survived collection. */
static void
resolvedarguments(ResolvedTest *test)
{
	Array *value;
	uvlong scalar;

	check(!freed[3] && test->argument_type->ref == 1);
	check(test->argument_type->size == 1184 && test->argument_type->np == 19);
	memmove(&value, test->arguments + 64 + 139 * 8, sizeof(value));
	memmove(&scalar, test->arguments + 64, sizeof(scalar));
	check(value->data[0] == 0x71 && scalar == ~(uvlong)0);
}


/* Keep actual dynamic argument roots live across an allocating C producer. */
static int
resolvedproducer(Native64Result *guard, void *argument)
{
	ResolvedTest *test;
	int ok;

	test = argument;
	ok = producer(guard, &test->base);
	resolvedarguments(test);
	if (test->mode == ResolvedMoveProducer)
		relocate(test);
	return ok;
}


/* Execute an independent producer while the outer result, arguments and previous inner cell remain rooted. */
static int
resolvednestedproduce(Native64Result *guard, void *argument)
{
	ResolvedTest *test;
	Array *value;

	test = argument;
	value = H2D(Array *, heaparray(&Tbyte, 0));
	check(native64resultadopt(guard, value));
	epochs(6);
	resolvedarguments(test);
	check(rootcount(activeguard->value) == 1 && rootcount(value) == 1);
	return 1;
}


/* The inner transaction owns a separate mapped result cell, never the outer destination. */
static void *
resolvednestedslot(void *argument)
{
	ResolvedTest *test;

	test = argument;
	return &test->second->tail;
}


/* Resolve the current destination after nested allocation, relocation, cancellation or a VM error. */
static void *
resolvedslot(void *argument)
{
	ResolvedTest *test;
	Array *nested;

	test = argument;
	test->resolver_calls++;
	check(activeguard->rooted || activeguard->value == H);
	allocationcollections = 1;
	nested = H2D(Array *, heaparray(&Tbyte, 0));
	allocationcollections = 0;
	test->disposing_nested = 1;
	if (test->mode == ResolvedNested) {
		/* Keep the temporary rooted until the inner commit retires its owner. */
		test->second->tail = nested;
		check(native64resultresolved(resolvednestedproduce, resolvednestedslot, test));
		nested = test->second->tail;
		check(rootcount(nested) == 0 && rootcount(activeguard->value) == 1);
		test->second->tail = H;
	}
	destroy(nested);
	test->disposing_nested = 0;
	check(!freed[2] && !freed[3]);
	resolvedarguments(test);
	if (test->mode == ResolvedError)
		error("resolver error");
	if (test->mode == ResolvedMoveResolver || test->mode == ResolvedCancel ||
		test->mode == ResolvedMovedError || test->mode == ResolvedAlias || test->mode == ResolvedNilResult)
		relocate(test);
	epochs(6);
	check(!freed[2] && !freed[3]);
	if (test->mode == ResolvedMovedError)
		error("moved resolver error");
	if (test->mode == ResolvedDecline || test->mode == ResolvedCancel) {
		kwerrstr("%s", test->base.expected);
		return nil;
	}
	if (test->mode == ResolvedBadH)
		return H;
	if (test->mode == ResolvedBadWrap)
		return (void *)((uintptr)-1 - 2);
	if (test->mode == ResolvedNilOwner) {
		/* Move the valid owner to the second mapped cell; return a deliberately invalid empty cell. */
		relocate(test);
		test->first->result = nil;
		return &test->first->result;
	}
	if (stalecontrol && test->moved)
		return &test->first->result;
	return &test->base.caller->result;
}


/* Propagate only after result cleanup so outer protection can verify error-stack balance. */
static int
resolvedouter(void *argument)
{
	ResolvedTest *test;

	test = argument;
	test->base.ok = native64resultresolved(resolvedproducer, resolvedslot, test);
	if (!test->base.ok && test->base.rethrow)
		krethrow();
	return 1;
}


/* Keep a borrowed argument frame/context alive while exercising actual result/collector/error owners. */
static void
resolvedtransaction(const LimboNative64Entry *callerrecord, int mode, int phase, int state, int rethrow)
{
	uchar fixedmap[] = {0}, map[19];
	LimboNative64Entry fixed = {"empty", nil, 0, 64, 1, fixedmap};
	Native64TailSlot slots[140];
	Native64TailPlan plan;
	uvlong values[140];
	ResolvedTest test;
	Type *callertype;
	Array *argument;
	void *empty;
	int i, status;

	startcase(phase, state);
	clearrootcache();
	failpin = 0;
	memset(&errorworker, 0, sizeof(errorworker));
	memset(&errorenvironment, 0, sizeof(errorenvironment));
	up = &errorworker;
	up->env = &errorenvironment;
	up->env->errstr = up->env->errbuf0;
	kwerrstr("%s", "initial error");
	memset(&test, 0, sizeof(test));
	callertype = native64frametype(callerrecord);
	check(callertype != nil);
	test.first = (F_Roots_caller *)stackframes(callertype, 2);
	test.second = (F_Roots_caller *)((uchar *)test.first + callertype->size);
	test.base.caller = test.first;
	test.base.old = H2D(Array *, heaparray(&Tbyte, 4));
	test.first->result = test.base.old;
	argument = H2D(Array *, heaparray(&Tbyte, 1));
	argument->data[0] = 0x71;
	test.first->tail = argument;
	for (i = 0; i < 140; i++) {
		slots[i].kind = Native64TailBig;
		slots[i].offset = i * 8;
		slots[i].bytes = 8;
		slots[i].type = nil;
		values[i] = ~(uvlong)0;
	}
	slots[139].kind = Native64TailRef;
	slots[139].type = &Tarray;
	values[139] = (uvlong)(uintptr)argument;
	check(native64tailplan(&fixed, slots, 140, &plan));
	check(native64tailvalues(&fixed, slots, 140, values, sizeof(values), resolvedreference, nil));
	check(native64tailmap(&fixed, slots, 140, map, sizeof(map)));
	test.argument_type = dtype(freeheap, plan.size, map, sizeof(map));
	check(test.argument_type != nil);
	test.arguments = (uchar *)test.first + 2 * callertype->size;
	check(test.arguments + plan.size <= stackspace.bytes + sizeof(stackspace.bytes));
	memset(test.arguments, 0, plan.size);
	((Frame *)test.arguments)->t = test.argument_type;
	initmem(test.argument_type, test.arguments);
	process.R.SP = test.arguments + plan.size;
	memmove(test.arguments + plan.tail, values, sizeof(values));
	incmem(test.arguments, test.argument_type);
	test.first->tail = H;
	destroy(argument);
	check(D2H(argument)->ref == 1);
	if (argumentcontrol)
		test.argument_type->np = 16;
	test.mode = mode;
	test.base.mode = mode == ResolvedAlias			   ? ResultAlias
					 : mode == ResolvedNilResult	   ? ResultNil
					 : mode == ResolvedProducerDecline ? ResultDecline
					 : mode == ResolvedProducerError   ? ResultError
					 : mode == ResolvedPinFailure	   ? ResultPinFail
													   : ResultNormal;
	test.base.rethrow = rethrow;
	test.base.expected = mode == ResolvedDecline						   ? "resolver declined"
						 : mode == ResolvedCancel						   ? "resolver cancelled"
						 : mode == ResolvedError						   ? "resolver error"
						 : mode == ResolvedMovedError					   ? "moved resolver error"
						 : mode == ResolvedBadH || mode == ResolvedBadWrap ? "bad native64 result destination"
						 : mode == ResolvedNilOwner						   ? "nil native64 result owner"
						 : mode == ResolvedProducerDecline				   ? "producer declined"
						 : mode == ResolvedProducerError				   ? "after pin"
																		   : "ptradd malloc";
	resolvedtest = &test;
	activecase = &test.base;
	Tarray.free = resolvedfree;
	status = kprotect(resolvedouter, &test);
	Tarray.free = arrayfree;
	activeguard = nil;
	failpin = 0;
	check(errdepth == 0 && gchalt == 0 && !freed[3]);
	check(test.base.ok == resolvedsuccess(mode));
	check(status == (resolvedsuccess(mode) || !rethrow));
	check(test.resolver_calls == (mode < ResolvedProducerDecline));
	if (!test.base.ok) {
		check(strcmp(up->env->errstr, test.base.expected) == 0);
		check(test.base.caller->result == test.base.old && D2H(test.base.old)->ref == 1 && !freed[2]);
	} else if (mode == ResolvedNilResult)
		check(test.base.caller->result == H && freed[2] == 1);
	else
		check(D2H(test.base.caller->result)->ref == 1);
	check(rootcount(test.base.caller->result) == 0);
	if (mode == ResolvedNilOwner)
		test.first->result = H;
	if (test.moved)
		check(test.first->result == H);
	epochs(6);
	check(!freed[3] && argument->data[0] == 0x71);
	freeptrs(test.arguments, test.argument_type);
	initmem(test.argument_type, test.arguments);
	empty = H;
	check(memcmp(test.arguments + plan.tail + 139 * 8, &empty, sizeof(empty)) == 0 && freed[3] == 1);
	process.R.SP = (uchar *)test.first + 2 * callertype->size;
	freetype(test.argument_type);
	freeptrs(test.first, callertype);
	freeptrs(test.second, callertype);
	initmem(callertype, test.first);
	initmem(callertype, test.second);
	process.R.EX = process.R.SP = nil;
	epochs(6);
	clearrootcache();
	check(Tarray.ref == 1 && Tbyte.ref == 1);
	freetype(callertype);
	resolvedtransactions++;
}


/* Combine moved destinations and cancellation with actual argument/result roots in both error routes. */
int
main(int argc, char **argv)
{
	int mode, phase, state, rethrow;

	check(argc == 1 || argc == 2);
	if (argc == 2) {
		if (strcmp(argv[1], "stale") == 0)
			stalecontrol = 1;
		else if (strcmp(argv[1], "arguments") == 0)
			argumentcontrol = 1;
		else
			check(0);
	}
	arrayfree = Tarray.free;
	check(limbo_native64_Roots_layout());
	check(!native64resultresolved(nil, resolvedslot, nil));
	check(!native64resultresolved(resolvedproducer, nil, nil));
	for (phase = 0; phase < 3; phase++)
		for (state = Palt; state <= Pbroken; state++)
			for (mode = 0; mode < ResolvedModes; mode++)
				for (rethrow = 0; rethrow < 2; rethrow++)
					resolvedtransaction(&limbo_native64_Roots_caller_entry, mode, phase, state, rethrow);
	check(resolvedtransactions == 720);
	printf("PASS resolved native64 results: %d predicates, 720 transactions with typed arguments, actual "
		   "roots/GC/errors and moved logical destinations\n",
		   checks);
	return 0;
}
