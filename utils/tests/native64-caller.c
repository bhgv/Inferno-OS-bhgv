#include "native64-caller.h"
#define LIMBO_NATIVE64_FIELDS // Explicit generated Real field, not scalar-map inference.
#include "probe-native64.h"

REG R; // Actual live-register symbol; scheduler ownership is bounded by this fixture.

static Module *leasedfieldmodule; // Extra typed caller module owner observed before producer allocation.
static Prog *leasecurrent; // Bounded single-thread scheduler identity; owning runtime supplies currun.
static Prog *leaseprog; // Active protected producer observation, nil outside this gate.
static Type *leasetype; // Caller descriptor whose extra owner is independently counted.
static int leasewhich; // Select each real destructive guard independently.
static int leaselive, livecase; // Explicit live-R publication and callback-mutation cases.
static int leasemode, leasetyperefs, leasenested; // Explicit failure/reentry cases.


/* Caller callable identity for compiler field metadata; this fixture never invokes it. */
void
Probe_caller(void *frame)
{
	USED(frame);
}


/* Bound profile availability only; the full emulator links all three owning marker functions. */
int
native64callerruntime(void)
{
	return 1;
}


/* Expose only the current scheduler head; lifecycle bodies remain the owning source guards. */
Prog*
currun(void)
{
	return leasecurrent;
}


/* Inspect the actual guarded producer allocation without replacing any Math computation. */
void
native64callerobserve(void)
{
	Native64MathFrameCall nested;

	if(leaseprog == nil)
		return;
	check(leaseprog->native64holds == 1 && leasetype->ref == leasetyperefs+1);
	if(leasedfieldmodule != nil) check(leasedfieldmodule->ref == 2);
	if(leaselive){
		check(leaseprog->native64live == &R && leaseprog->R.FP == R.FP && leaseprog->R.EX == R.EX);
		if(livecase == 1) R.FP += leasetype->size;
		if(livecase == 2) R.EX = (uchar*)1;
		if(livecase == 3) R.SP--;
		if(livecase == 4) R.M = (Modlink*)1;
		if(livecase == 5) leaseprog->native64live = nil;
		if(livecase == 6) native64calleradvance(leaseprog);
		if(livecase == 7) error("live producer failed");
		if(livecase == 8) R.MP = (uchar*)1;
		if(livecase == 9) R.TS--;
		if(livecase == 10) native64callerleave(leaseprog);
		if(livecase == 11) native64callerenter(leaseprog);
		return;
	}
	if(leasemode == 1)
		native64calleradvance(leaseprog); // Same frame address/Type must still become stale.
	if(leasemode == 2)
		leaseprog->pid++;
	if(leasemode == 3)
		leaseprog->R.FP += leasetype->size;
	if(leasemode == 4)
		error("leased producer failed");
	if(leasemode == 6)
		leasecurrent = nil;
	if(leasemode == 7)
		leaseprog->state = Prelease;
	if(leasemode == 8)
		leaseprog->flags |= Pkilled;
	if(leasemode == 9)
		mode = 7; // Existing copied-callee hook raises only after the actual private result store.
	if(leasemode == 5){
		memset(&nested, 0, sizeof(nested));
		nested.call = request; nested.caller = leaseprog; nested.type = leasetype; nested.offset = 72;
		leasenested++;
		check(!Math_bits64realguard64(&nested));
	}
}

LIFECYCLE_GUARDS


/* Run the exact lifecycle guards through the owning protected-error body. */
static int
leaseexit(void *argument)
{
	Prog *prog;

	prog = argument;
	if(leasewhich == 0)
		test_delprog(prog);
	else if(leasewhich == 1)
		test_progexit(prog);
	else
		test_release(prog);
	return 1;
}


/* Check noncopyable identity, generation exhaustion, transfer and guarded production. */
int
main(void)
{
	union {
		void *alignment; // Natural Frame64 alignment.
		uchar bytes[OA(Stkext, reg.tos.fu)+176]; // Two complete typed frames.
	} stack;
	Native64Caller caller = {0}, second = {0}, copied;
	Native64MathFrameCall call;
	Native64MathFieldCall typed;
	Module ownermodule;
	Modlink ownerinstance;
	Link ownerexports[2];
	Type *ownertypes[1];
	Prog prog;
	Type *type;
	Frame *first, *next;
	REG moved, invalid, saved;
	uchar map[] = {0,0x20}; // Scalar result at72, managed neighbor at80.
	uvlong oldbits = 0x1122334455667788ULL, bits = 0x3ff0000000000000ULL, observed, epoch;
	int i, base, status, modes = 9;

	check(native64_fixed_call_main() == 0);
	memset(&stack, 0, sizeof(stack)); memset(&prog, 0, sizeof(prog));
	type = dtype(freeheap, 88, map, sizeof(map));
	check(type != nil);
	first = (Frame*)((Stkext*)stack.bytes)->reg.tos.fu;
	next = (Frame*)((uchar*)first+88);
	first->t = next->t = type;
	prog.pid = 77; prog.state = Pready; leasecurrent = &prog; prog.R.EX = stack.bytes; prog.R.FP = (uchar*)first;
	prog.R.SP = prog.R.TS = stack.bytes+sizeof(stack.bytes);
	base = type->ref;
	check(native64callerbegin(&caller, &prog, type, 72));
	check(prog.native64holds == 1 && type->ref == base+1);
	check(!native64callerbegin(&second, &prog, type, 72));
	copied = caller;
	check(native64callerresolve(&copied) == nil);
	native64callerclear(&copied);
	check(prog.native64holds == 1 && type->ref == base+1);
	check(native64callerresolve(&caller) == (uchar*)first+72);
	moved = prog.R; moved.FP = (uchar*)next;
	invalid = moved; invalid.FP++;
	epoch = caller.epoch;
	check(!native64callerrebind(&caller, invalid.FP) && caller.epoch == epoch);
	check(native64callerrebind(&caller, moved.FP) && caller.epoch > epoch);
	check(native64callerresolve(&caller) == (uchar*)next+72);
	native64calleradvance(&prog);
	check(native64callerresolve(&caller) == nil && !native64callerrebind(&caller, moved.FP));
	prog.R.EX = (uchar*)1; // Staleness must be checked before dereferencing retired stack storage.
	check(native64callerresolve(&caller) == nil);
	native64callerclear(&caller); native64callerclear(&caller);
	check(prog.native64holds == 0 && type->ref == base);
	prog.R.EX = stack.bytes; prog.R.FP = (uchar*)first;
	prog.native64epoch = ~(uvlong)0-1;
	check(!native64callerbegin(&caller, &prog, type, 72));
	native64calleradvance(&prog); native64calleradvance(&prog);
	check(prog.native64epoch == ~(uvlong)0 && !native64callerbegin(&caller, &prog, type, 72));
	prog.native64epoch = 100;
	check(native64callerbegin(&caller, &prog, type, 72));
	for(leasewhich=0; leasewhich<3; leasewhich++){
		check(!kprotect(leaseexit, &prog));
		check(strcmp(diagnostic, leasewhich == 2 ? "native64 caller cannot release VM" : "native64 caller is retained") == 0);
	}
	check(prog.native64holds == 1 && native64callerresolve(&caller) == nil);
	check(test_killprog(&prog) == 0 && prog.native64holds == 1 && (prog.flags & Pkilled));
	check(native64callerresolve(&caller) == nil);
	native64callerclear(&caller);
	check(!native64callerbegin(&caller, &prog, type, 72));
	check(kprotect(leaseexit, &prog) && test_killprog(&prog) == -1);

	check(kprotect(startmath, nil)); fixedmodule = modules;
	for(i=0; i<fixedmodule->ntype; i++)
		if(strcmp(fixedmodule->ext[i].name, "bits64real") == 0) break;
	check(i < fixedmodule->ntype); fixedtype = fixedmodule->ext[i].frame;
#ifdef FIXED_CALLEE_INJECTED
	modes++;
#endif
	for(leasemode=0; leasemode<modes; leasemode++){
		prog.R.FP = (uchar*)first; prog.pid = 77; prog.state = Pready; prog.flags = 0; prog.kill = nil;
		memmove((uchar*)first+72, &oldbits, 8); memmove((uchar*)next+72, &oldbits, 8);
		fixedrequest(bits); memset(&call, 0, sizeof(call));
		call.call = request; call.caller = &prog; call.type = type; call.offset = 72;
		leaseprog = &prog; leasetype = type; leasetyperefs = type->ref;
		status = Math_bits64realguard64(&call); leaseprog = nil; leasecurrent = &prog;
		check(status == (leasemode == 0 || leasemode == 5));
		check(prog.native64holds == 0 && type->ref == leasetyperefs && live == fixedlive && worker.nerr == 0);
		memmove(&observed, (uchar*)first+72, 8); check(observed == (status ? bits : oldbits));
		memmove(&observed, (uchar*)next+72, 8); check(observed == oldbits);
	}
	check(leasenested == 1);
	/* Publish stale saved REG from a proven live owner before the first allocating producer. */
	leasemode = 0; mode = 0; leaselive = 1;
	for(livecase=0; livecase<12; livecase++){
		memset(&prog.R, 0, sizeof(prog.R));
		prog.R.EX = stack.bytes; prog.R.FP = (uchar*)first;
		prog.R.SP = prog.R.TS = stack.bytes+sizeof(stack.bytes);
		prog.pid = 77; prog.state = Pready; prog.flags = 0; prog.kill = nil;
		R = prog.R; native64callerenter(&prog);
		prog.R.FP = (uchar*)1; prog.R.EX = (uchar*)1; // Saved REG is deliberately unreadable until publication.
		memmove((uchar*)first+72, &oldbits, 8); memmove((uchar*)next+72, &oldbits, 8);
		fixedrequest(bits); memset(&call, 0, sizeof(call));
		call.call = request; call.caller = &prog; call.type = type; call.offset = 72;
		leaseprog = &prog; leasetype = type; leasetyperefs = type->ref;
		status = Math_bits64realvm64(&call); leaseprog = nil;
		check(status == (livecase == 0));
		check(prog.native64holds == 0 && type->ref == leasetyperefs && live == fixedlive && worker.nerr == 0);
		memmove(&observed, (uchar*)first+72, 8); check(observed == (status ? bits : oldbits));
		memmove(&observed, (uchar*)next+72, 8); check(observed == oldbits);
		native64callerleave(&prog); check(prog.native64live == nil);
	}
	leaselive = 0;
	/* Rejected publication must neither repair poisoned saved REG nor touch the destination. */
	R.EX = stack.bytes; R.FP = (uchar*)first; R.SP = R.TS = stack.bytes+sizeof(stack.bytes);
	prog.R = R; prog.R.FP = (uchar*)1; saved = prog.R;
	check(!Math_bits64realvm64(&call) && memcmp(&prog.R, &saved, sizeof(saved)) == 0);
	prog.native64live = &prog.R;
	check(!Math_bits64realvm64(&call) && memcmp(&prog.R, &saved, sizeof(saved)) == 0);
	prog.native64live = nil;
	native64callerenter(&prog);
	R.FP = (uchar*)1;
	check(!Math_bits64realvm64(&call) && memcmp(&prog.R, &saved, sizeof(saved)) == 0);
	R.FP = (uchar*)first;
	prog.native64epoch = ~(uvlong)0;
	check(!Math_bits64realvm64(&call) && memcmp(&prog.R, &saved, sizeof(saved)) == 0);
	prog.native64epoch = 400;
	check(native64callerbeginlive(&caller, &prog, type, 72));
	check(native64callerrebind(&caller, next) && R.FP == (uchar*)next && prog.R.FP == R.FP);
	check(native64callerresolve(&caller) == (uchar*)next+72);
	native64callerclear(&caller); native64callerleave(&prog);
	check(prog.R.FP == R.FP && prog.native64live == nil && type->ref == base);
	/* Execute the new typed adapter through the actual protected owning Math gateway. */
	memset(&ownermodule, 0, sizeof(ownermodule)); memset(&ownerinstance, 0, sizeof(ownerinstance));
	memset(ownerexports, 0, sizeof(ownerexports)); memset(&typed, 0, sizeof(typed));
	ownermodule.ref = 1; ownermodule.ntype = 1; ownermodule.type = ownertypes; ownermodule.ext = ownerexports;
	ownertypes[0] = type; ownerinstance.m = &ownermodule;
	ownerexports[0].name = "caller"; ownerexports[0].frame = type; ownerexports[0].u.runt = Probe_caller;
	memmove(&ownerexports[0].sig, &limbo_native64_Probe_caller_entry.legacy_signature, 4);
	leaselive = 0;
	for(typed.live=0; typed.live<2; typed.live++){
		for(leasemode=0; leasemode<modes; leasemode++){
			prog.R.FP = (uchar*)first; prog.R.EX = stack.bytes; prog.R.SP = prog.R.TS = stack.bytes+sizeof(stack.bytes);
			prog.R.M = &ownerinstance; prog.R.MP = nil; prog.pid = 77; prog.state = Pready; prog.flags = 0; prog.kill = nil;
			R = prog.R;
			if(typed.live) native64callerenter(&prog);
			memmove((uchar*)first+72, &oldbits, 8); fixedrequest(bits); mode = 0;
			typed.call = request; typed.caller = &prog; typed.module = &ownermodule; typed.type = type;
			typed.fields = &limbo_native64_Probe_caller_field_table; typed.ordinal = 1;
			leaseprog = &prog; leasetype = type; leasetyperefs = type->ref;
			leasedfieldmodule = &ownermodule;
			status = Math_bits64realfield64(&typed); leaseprog = nil; leasedfieldmodule = nil; leasecurrent = &prog;
			check(status == (leasemode == 0 || leasemode == 5));
			check(ownermodule.ref == 1 && type->ref == base && prog.native64holds == 0 && live == fixedlive);
			memmove(&observed, (uchar*)first+72, 8); check(observed == (status ? bits : oldbits));
			typed.ordinal = 0; check(!Math_bits64realfield64(&typed));
			if(typed.live) native64callerleave(&prog);
		}
	}
	unload(fixedmodule); freetype(type);
	check(modules == &baseline && live == 0);
	puts("PASS typed field Math: compiler Real/Big identity, module ownership, protected error/cancellation and live/published REG");
	puts("PASS live VM scalar: pre-allocation publication, mutation cancellation, exhaustion rollback, coordinated rebind and retirement");
	puts("PASS caller lease: self/copy, transfer, stale reuse, saturation, destructive guards, cancellation/unwind and nested refusal");
	return 0;
}
