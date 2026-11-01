#include "native64-math-call.h"
#include "fixed-math-golden.h"
#ifdef FRAMECELL_LEASE
#include "native64-caller.h"
static Native64Caller gatewaylease; // One explicit current-process hold, moved only through checked transfer.


/* The combined source gate bounds availability; coherent full-runtime linkage is checked separately. */
int
native64callerruntime(void)
{
	return 1;
}


/* Bound only scheduler identity; actual process roots and collector bodies remain active. */
Prog*
currun(void)
{
	return &process;
}


/* Resolve only the held generation; no physical result address survives production. */
static void*
gatewayleasedresolve(void *argument)
{
	return native64callerresolve(argument);
}
#endif

static Module gatewaymodule; // Bounded registry bootstrap; actual binding and unload bodies run.
static Type *gatewaytype; // Generated native callee Type retained across actual collections.
static Prog *gatewayprog; // Published live collector root during the private fixed call.
static uchar *gatewaynext; // Alternate current frame, resolved only after production.
static int gatewaymode, gatewayframes, gatewaytransactions; // Active ownership and outcome observations.
static void *gatewayallocation; // Exact owned raw native frame, never a GC heap object.


/* Only this call's ordinary producer allocation triggers collector safepoints. */
void*
native64gatewayalloc(ulong bytes, int clear)
{
	check(gatewaymodule.ref == 2 && gatewaytype->ref == 2 && gatewayframes == 0);
	epochs(6);
	check(!freed[2] && !freed[3]);
	if(gatewaymode == 2)
		error("native scalar frame allocation error");
	gatewayallocation = mallocz(bytes, clear);
	check(gatewayallocation != nil);
	gatewayframes++;
	return gatewayallocation;
}


/* Inject a real collector safepoint after the actual callee's private scalar store. */
void
native64gatewayafter(void)
{
	check(gatewaymodule.ref == 2 && gatewaytype->ref == 2 && gatewayframes == 1);
	epochs(6);
	check(!freed[2] && !freed[3]);
#ifdef FRAMECELL_LEASE
	check(gatewayprog->native64holds == 1 && gatewaylease.type->ref == 2);
	if(gatewaymode == 1){
		REG moved = gatewayprog->R;
		moved.FP = gatewaynext;
		check(native64callerrebind(&gatewaylease, moved.FP));
	}
	if(gatewaymode == 3)
		native64calleradvance(gatewayprog); // Reused address/Type is not enough to revive the old capability.
#else
	if(gatewaymode == 1)
		gatewayprog->R.FP = gatewaynext;
	if(gatewaymode == 3)
		gatewayprog->R.FP = gatewaynext+8;
#endif
}


/* Require exactly-once retirement of the raw producer frame. */
void
native64gatewayfree(void *pointer)
{
	if(pointer == nil)
		return;
	check(pointer == gatewayallocation && gatewayframes == 1);
	gatewayframes--;
	gatewayallocation = nil;
	free(pointer);
}


/* The registry owner remains live; entering final module teardown is a lifetime failure. */
void
freemod(Module *module)
{
	USED(module);
	check(0);
}


/* No live registry links may be retired by the temporary call owner. */
void
destroylinks(Module *module)
{
	USED(module);
	check(0);
}


/* This fixture publishes a C builtin, never a dynamic native image. */
void
freedyncode(Module *module)
{
	USED(module);
	check(0);
}


/* Combine actual fixed import matching, Frame64 adapter, protection, collector and scalar callee. */
static void
gatewaytransaction(int phase, int state, int outcome)
{
	Type *caller;
	F_Scalar_caller *first, *second;
	Native64MathFrameCall request;
	Link exports[2];
	Type *types[1];
	uvlong bits, oldbits, observed;
	int signature, status, admitted = 1;

	startcase(phase, state);
	memset(&errorworker, 0, sizeof(errorworker));
	memset(&errorenvironment, 0, sizeof(errorenvironment));
	up = &errorworker; up->env = &errorenvironment; up->env->errstr = up->env->errbuf0;
	caller = native64frametype(&limbo_native64_Scalar_caller_entry);
	gatewaytype = native64frametype(&limbo_native64_Math_bits64real_entry);
	check(caller != nil && gatewaytype != nil);
	first = (F_Scalar_caller*)stackframes(caller, 2);
	second = (F_Scalar_caller*)((uchar*)first+caller->size);
	first->keep = H2D(Array*, heaparray(&Tbyte, 4));
	second->keep = H2D(Array*, heaparray(&Tbyte, 4));
	bits = 0x3ff0000000000000ULL+gatewaytransactions;
	oldbits = 0x1122334455667788ULL;
	memmove(&first->result, &oldbits, 8); memmove(&second->result, &oldbits, 8);
	process.R.FP = (uchar*)first; process.R.TS = stackspace.bytes+sizeof(stackspace.bytes);
	memset(&gatewaymodule, 0, sizeof(gatewaymodule)); memset(exports, 0, sizeof(exports));
	gatewaymodule.ref = 1; gatewaymodule.origmp = H; gatewaymodule.path = "$Math";
	gatewaymodule.ntype = 1; gatewaymodule.type = types; gatewaymodule.ext = exports;
	types[0] = gatewaytype;
	exports[0].name = "bits64real"; exports[0].frame = gatewaytype;
	exports[0].u.runt = Math_bits64real;
	memmove(&signature, &limbo_native64_Math_bits64real_entry.legacy_signature, sizeof(signature));
	exports[0].sig = signature;
	modules = &gatewaymodule;
	memset(&request, 0, sizeof(request));
	request.call.strings = golden_strings; request.call.string_bytes = sizeof(golden_strings);
	request.call.imports = golden_imports; request.call.import_bytes = sizeof(golden_imports);
	request.call.input = bits;
	request.caller = &process; request.type = caller; request.offset = 72;
	gatewayprog = &process; gatewaynext = (uchar*)second; gatewaymode = outcome;
#ifdef FRAMECELL_ROOT_CONTROL
	caller->map[1] = 0; // Actual GC must detect the removed adjacent owners.
#endif
#ifdef FRAMECELL_LEASE
	process.pid = 77;
	memset(&gatewaylease, 0, sizeof(gatewaylease));
	admitted = state == Pready;
	if(admitted)
		check(native64callerbegin(&gatewaylease, &process, caller, 72));
	else
		check(!native64callerbegin(&gatewaylease, &process, caller, 72));
	request.call.resolve = gatewayleasedresolve; request.call.argument = &gatewaylease;
	status = admitted ? Math_bits64real64(&request.call) : 0;
	native64callerclear(&gatewaylease); native64callerclear(&gatewaylease);
	check(process.native64holds == 0 && caller->ref == 1);
#else
	status = Math_bits64realframe64(&request);
#endif
	check(status == (outcome < 2 && admitted));
	check(gatewayframes == 0 && gatewayallocation == nil && gatewaymodule.ref == 1 && gatewaytype->ref == 1);
	check(errdepth == 0 && !freed[2] && !freed[3]);
	if(outcome == 2 && admitted) check(strcmp(up->env->errstr, "native scalar frame allocation error") == 0);
	memmove(&observed, &first->result, 8); check(observed == (outcome == 0 && status ? bits : oldbits));
	memmove(&observed, &second->result, 8); check(observed == (outcome == 1 && status ? bits : oldbits));
	process.R.FP = (uchar*)first;
	epochs(6);
	freeptrs(first, caller); freeptrs(second, caller);
	initmem(caller, first); initmem(caller, second);
	check(freed[2] == 1 && freed[3] == 1);
	process.R.EX = process.R.SP = process.R.FP = process.R.TS = nil;
	modules = nil;
	freetype(gatewaytype); freetype(caller);
	gatewaytransactions++;
}


/* Exercise real protected fixed calls in every process-state/color combination. */
int
main(void)
{
	int phase, state, outcome;

	check(limbo_native64_Scalar_layout() && offsetof(F_Scalar_caller, result) == 72);
	for(phase=0; phase<3; phase++)
	for(state=Palt; state<=Pbroken; state++)
	for(outcome=0; outcome<4; outcome++)
		gatewaytransaction(phase, state, outcome);
	printf("PASS complete mapped scalar gateway: %d transactions with actual Math/heap/GC, current-frame moves, refusal/unwind and balanced owners\n", gatewaytransactions);
	return 0;
}
