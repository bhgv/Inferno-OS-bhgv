/* Symbolic String/List ownership fixture. No file kind or VM call is admitted here. */
#define LIMBO_NATIVE64_MANAGED
#include "string-native64.h"
#undef LIMBO_NATIVE64_MANAGED
#include "dis64-managed-signature.h"

typedef struct ManagedImageCase ManagedImageCase;
struct ManagedImageCase {
	int direction, accepted;
	const uchar *consumer;
	unsigned int consumerbytes;
	const uchar *provider;
	unsigned int providerbytes;
};
#include "managed-images.h"

static Type *stringelement;
static Prog *managedprocess;
static int managederror, managedcalls;
static int managedbytecodes, managedfast;
static Dis64ManagedBody managedbodies[2];
static const ManagedImageCase *managedfixtures[2];

/* Independent trial bytes, with deliberately local version/tag values. */
static const Dis64ManagedSchema trialschema = {0xface0010U, 0x71, 0x72};
static const uchar trialrecords[2][Dis64ManagedSignatureBytes] = {
	{0xfa,0xce,0,0x10, 0,0,0,1, 0,0,0,0x72, 0,0,0,0x71, 0,0,0,0x71},
	{0xfa,0xce,0,0x10, 0,0,0,1, 0,0,0,0x71, 0,0,0,0x72, 0,0,0,0x71},
};

static void
managedimagecontrol(void)
{
	static const Dis64StorageLimits limits = {8, 8, 128, 8, 8, 8, 2, 2};
	Dis64StorageImage scalar;
	Dis64ManagedPlan plan;
	const ManagedImageCase *test;
	const LimboNative64Managed *shape;
	unsigned int i;
	int before = allocations;

	for(i=0; i<nelem(managedimages); i++){
		test = &managedimages[i];
		shape = test->direction ? &limbo_native64_String_quoted_managed :
			&limbo_native64_String_unquoted_managed;
		memset(&plan, 0, sizeof(plan));
		plan.selected.entry = 99;
		require(!dis64storageimagecheck(test->consumer, test->consumerbytes, &limits, &scalar));
		require(dis64managedimagepair(test->consumer, test->consumerbytes,
			test->provider, test->providerbytes, &limits, &trialschema, shape,
			&plan) == test->accepted);
		require(plan.selected.entry == (test->accepted ? 1 : 99));
		if(test->accepted){
			require(plan.selected.frame == 0 && plan.provider.code.bytes == 14);
			managedbodies[test->direction] = (Dis64ManagedBody){plan.provider.code.data+6, 8};
			managedfixtures[test->direction] = test;
		}else
			require(plan.consumer.code.data == nil && plan.provider.code.data == nil);
	}
	require(allocations == before);
}

static void
managedrecordcontrol(void)
{
	Dis64ManagedSignature signature, sentinel = {LimboNative64String, LimboNative64String};
	Dis64Signature scalar;
	Dis64ManagedSchema wrong = trialschema;
	uchar bad[Dis64ManagedSignatureBytes+1];
	static const uchar scalarclock[] = {0,0,0,1, 0,0,0,Dis64Int, 0,0,0,0};
	unsigned int direction, size, byte, bit;
	int before = allocations;

	require(dis64signaturecheck(scalarclock, sizeof(scalarclock), 1, &scalar));
	require(!dis64managedread(scalarclock, sizeof(scalarclock), &trialschema, &signature));
	for(direction=0; direction<2; direction++){
		memmove(bad, trialrecords[direction], Dis64ManagedSignatureBytes);
		bad[Dis64ManagedSignatureBytes] = 0;
		require(!dis64signaturecheck(bad, Dis64ManagedSignatureBytes, 1, &scalar));
		for(size=0; size<=sizeof(bad); size++){
			if(size == Dis64ManagedSignatureBytes)
				continue;
			signature = sentinel;
			require(!dis64managedread(bad, size, &trialschema, &signature));
			require(signature.argument == sentinel.argument && signature.result == sentinel.result);
		}
		for(byte=0; byte<Dis64ManagedSignatureBytes; byte++)
			for(bit=0; bit<8; bit++){
				bad[byte] ^= 1U<<bit;
				signature = sentinel;
				require(!dis64managedread(bad, Dis64ManagedSignatureBytes, &trialschema, &signature));
				require(signature.argument == sentinel.argument && signature.result == sentinel.result);
				bad[byte] ^= 1U<<bit;
			}
		wrong.version = 1;
		require(!dis64managedread(bad, Dis64ManagedSignatureBytes, &wrong, &signature));
		wrong = trialschema;
		wrong.listtag = wrong.stringtag;
		require(!dis64managedread(bad, Dis64ManagedSignatureBytes, &wrong, &signature));
		wrong = trialschema;
		dis64put32(bad+(direction ? 16 : 12), trialschema.listtag);
		require(!dis64managedread(bad, Dis64ManagedSignatureBytes, &trialschema, &signature));
	}
	require(allocations == before);
}

#define TRIALWORD(x) 0, 0, 0, (x)
static const uchar trialpool[] = {
	TRIALWORD(3),
	TRIALWORD(6), TRIALWORD('S'), TRIALWORD('t'), TRIALWORD('r'), TRIALWORD('i'), TRIALWORD('n'), TRIALWORD('g'),
	TRIALWORD(8), TRIALWORD('u'), TRIALWORD('n'), TRIALWORD('q'), TRIALWORD('u'),
	TRIALWORD('o'), TRIALWORD('t'), TRIALWORD('e'), TRIALWORD('d'),
	TRIALWORD(6), TRIALWORD('q'), TRIALWORD('u'), TRIALWORD('o'), TRIALWORD('t'), TRIALWORD('e'), TRIALWORD('d'),
};
#undef TRIALWORD

static int
managedpair(int reverse, const LimboNative64Managed *shape)
{
	uchar imported[12+Dis64ManagedSignatureBytes], exported[16+Dis64ManagedSignatureBytes];
	uchar descriptors[] = {0,0,0,1, 0,0,0,72, 0,0,0,8, 0,0x80};
	DisStringPoolView names;
	Dis64TypeTable types;
	Dis64Binding selected;
	unsigned int symbol = reverse+1;

	require(disstringpoolcheck(trialpool, sizeof(trialpool), 3, &names));
	require(dis64typescheck(descriptors, sizeof(descriptors), 1, 72, &types));
	dis64put32(imported, 0);
	dis64put32(imported+4, symbol);
	dis64put32(imported+8, Dis64ManagedSignatureBytes);
	memmove(imported+12, trialrecords[reverse], Dis64ManagedSignatureBytes);
	dis64put32(exported, symbol);
	dis64put32(exported+4, 1);
	dis64put32(exported+8, 0);
	dis64put32(exported+12, Dis64ManagedSignatureBytes);
	memmove(exported+16, trialrecords[reverse], Dis64ManagedSignatureBytes);

	selected.entry = 99;
	require(!dis64managedpair(imported, sizeof(imported)-1, exported, sizeof(exported),
		&trialschema, &names, &names, 0, shape, &types, 2, &selected));
	require(!dis64managedpair(imported, sizeof(imported), exported, sizeof(exported),
		&trialschema, &names, &names, 1, shape, &types, 2, &selected));
	require(!dis64managedpair(imported, sizeof(imported), exported, sizeof(exported),
		&trialschema, &names, &names, 0, shape, &types, 1, &selected));
	dis64put32(exported, 3-symbol);
	require(!dis64managedpair(imported, sizeof(imported), exported, sizeof(exported),
		&trialschema, &names, &names, 0, shape, &types, 2, &selected));
	dis64put32(exported, symbol);
	dis64put32(exported+8, 1);
	require(!dis64managedpair(imported, sizeof(imported), exported, sizeof(exported),
		&trialschema, &names, &names, 0, shape, &types, 2, &selected));
	dis64put32(exported+8, 0);
	memmove(exported+16, trialrecords[!reverse], Dis64ManagedSignatureBytes);
	require(!dis64managedpair(imported, sizeof(imported), exported, sizeof(exported),
		&trialschema, &names, &names, 0, shape, &types, 2, &selected));
	memmove(exported+16, trialrecords[reverse], Dis64ManagedSignatureBytes);
	descriptors[13] = 0;
	require(dis64typescheck(descriptors, sizeof(descriptors), 1, 72, &types));
	require(!dis64managedpair(imported, sizeof(imported), exported, sizeof(exported),
		&trialschema, &names, &names, 0, shape, &types, 2, &selected));
	descriptors[13] = 0x80;
	require(dis64typescheck(descriptors, sizeof(descriptors), 1, 72, &types));
	require(selected.entry == 99);
	return dis64managedpair(imported, sizeof(imported), exported, sizeof(exported),
		&trialschema, &names, &names, 0, shape, &types, 2, &selected) &&
		selected.entry == 1 && selected.frame == 0;
}

Prog*
currun(void)
{
	return managedprocess;
}

static void
managedput(void *slot, void *owned)
{
	void *old;

	memmove(&old, slot, sizeof(old));
	memmove(slot, &owned, sizeof(owned));
	if(owned != H)
		Setmark(D2H(owned));
	destroy(old);
}

static int
managedfail(void *unused)
{
	USED(unused);
	error("managed fixture unwind");
	return 0;
}

static String*
managedstring(void)
{
	String *s;

	s = H2D(String*, heapz(&Tstring));
	s->len = 1;
	s->max = 1;
	s->Sascii[0] = 'x';
	return s;
}

static List*
managedlist(String *value)
{
	List *list = H;
	int i;

	D2H(value)->ref++;
	for(i=0; i<2; i++){
		cons(sizeof(value), &list);
		list->t = stringelement;
		stringelement->ref++;
		memmove(list->data, &value, sizeof(value));
		Setmark(D2H(value));
	}
	return list;
}

static String*
managedhead(List *list)
{
	String *value;

	memmove(&value, list->data, sizeof(value));
	return value;
}

/* Exercise native dispatch and the generated result cell with fixture callbacks. */
static void
managedcallback(void *argument, void *destination, int reverse)
{
	String *text;
	void *result;
	Heap *staging;
	void **stage;

	managedcalls++;
	managedprocess->R = R;
	numericcollect(managedprocess);
	text = argument == H ? H : reverse ? managedhead(argument) : argument;
	if(text != H){
		require(numericowner(D2H(text))->magic == MAGIC_A);
		D2H(text)->ref++;
	}
	result = text == H ? H : reverse ? (void*)text : managedlist(text);
	staging = heapz(stringelement);
	ptradd(staging);
	stage = H2D(void**, staging);
	managedput(stage, result);
	numericcollect(managedprocess);
	if(result != H)
		require(numericowner(D2H(result))->magic == MAGIC_A);
	if(!managederror){
		*stage = H;
		managedput(destination, result);
	}
	ptrdel(staging);
	destroy(stage);
	if(managederror)
		error("managed MCALL unwind");
}

void
String_quoted(void *frame)
{
	F_String_quoted *call = frame;
	managedcallback(call->argv, call->ret, 1);
}

void
String_unquoted(void *frame)
{
	F_String_unquoted *call = frame;
	managedcallback(call->args, call->ret, 0);
}

static int
managedmcall(void *unused)
{
	USED(unused);
	mcall();
	return 1;
}

static int
managedbind(void *unused)
{
	USED(unused);
	return native64managedlea8step();
}

static Modlink*
managedmodule(int links)
{
	Module *module;

	module = mallocz(sizeof(*module), 1);
	require(module != nil);
	module->ref = 1;
	module->origmp = H;
	module->ss = 4096;
	return mklinkmod(module, links);
}

/* Decode a checked body through the same operand reader as Dis64. */
static Inst*
manageddecode(const uchar *bytes, unsigned int size, unsigned int count)
{
	unsigned int at, i, used;
	Dis64CallDecoded decoded;
	Inst *body;

	body = mallocz(count*sizeof(*body), 1);
	require(body != nil);
	at = 0;
	for(i=0; i<count; i++){
		require(dis64callread(bytes+at, size-at, &decoded, &used));
		dis64runtimeinst(body+i, &decoded.instruction, body);
		if(UDST(decoded.instruction.mode) == (AFP|AIND)){
			body[i].d.i.f = decoded.instruction.destination;
			body[i].d.i.s = decoded.secondary;
		}
		at += used;
	}
	require(at == size);
	return body;
}

static Inst*
managedprogram(int reverse, int unwind)
{
	static const uchar fault[] = {IDIVW, SRC(AIMM)|DST(AFP), 0,40, IRET, SRC(AXXX)|DST(AXXX)};

	if(unwind)
		return manageddecode(fault, sizeof(fault), 2);
	return manageddecode(managedbodies[reverse].data, managedbodies[reverse].bytes, 2);
}

static Modlink*
managedlinkpair(int reverse, int jit)
{
	const ManagedImageCase *test = managedfixtures[reverse];

	return prepare64managedtrial("managed-caller", test->consumer, test->consumerbytes,
		"managed-provider", test->provider, test->providerbytes, jit);
}

static int
managedlinkcontrol(void)
{
	Modlink *caller;
	int before, first, attempts, failure, slot;

	numericdrain();
	before = live;
	first = allocations;
	caller = managedlinkpair(0, 0);
	require(caller != nil);
	attempts = allocations-first;
	ptrdel(D2H(caller));
	destroy(caller);
	numericdrain();
	require(live == before);
	for(failure=1; failure<=attempts; failure++){
		failat = allocations+failure;
		require(managedlinkpair(0, 0) == nil);
		failat = 0;
		numericdrain();
		require(live == before && Tmodlink.ref == 1 && worker.nerr == 0);
		for(slot=1; slot<GcSlots; slot++)
			require(numericblock(slot)->magic == MAGIC_F);
	}
	return attempts;
}

/* Both instruction streams come from the checked images. Only the input is supplied by C. */
static int
managedfullcase(int reverse, int jit, int grow)
{
	Prog process = {0};
	Modlink *caller, *callee;
	Type *callerframe, *calleeframe;
	Frame *entry, *pending;
	String *text;
	void *argument, *result;
	int step, before;

	caller = managedlinkpair(reverse, jit);
	require(caller != nil);
	numericcollect(nil);
	memmove(&callee, caller->MP, sizeof(callee));
	require(callee != nil && callee != H && callee->links[0].u.pc == callee->prog+1);
	require(numericowner(D2H(callee))->magic == MAGIC_A);
	callerframe = caller->m->entryt;
	calleeframe = callee->links[0].frame;
	caller->m->ss = grow ? 0 : 4096;
	if(jit){
		require(callee->m->jit != nil && callee->m->jit->code != nil);
		rxbegin = (uintptr)callee->m->jit->code;
		rxend = rxbegin+callee->m->jit->size;
	}
	process.exval = H;
	process.state = Pready;
	process.quanta = 1;
	process.R.M = caller;
	process.R.PC = caller->prog;
	entry = mallocz(callerframe->size, 1);
	require(entry != nil);
	entry->t = callerframe;
	R.s = &entry;
	newstack(&process);
	free(entry);
	initmem(callerframe, process.R.FP);
	text = managedstring();
	argument = reverse ? (void*)managedlist(text) : text;
	managedput(process.R.FP+64, argument);
	ptrdel(D2H(caller));
	managedprocess = &process;
	before = rxcompleted;
	for(step=0; step<6; step++){
		require(kprotect(execute, &process));
		numericcollect(&process);
		if(step == 0){
			memmove(&pending, process.R.FP+40, sizeof(pending));
			require(pending != nil && pending->t == (grow ? nil : calleeframe));
			if(grow)
				require(((Stkext*)process.R.EX)->reg.TR == calleeframe);
		}
	}
	require(process.R.M == caller && process.R.PC == caller->prog+4);
	memmove(&result, process.R.FP+72, sizeof(result));
	require(result != H && result != nil);
	require((reverse ? (String*)result : managedhead(result))->Sascii[0] == 'x');
	require(!kprotect(execute, &process) && message[0] == 0);
	destroystack(&R);
	require(Tstring.ref == 1 && Tlist.ref == 1 && Tptr.ref == 1 && Tmodlink.ref == 1);
	return rxcompleted-before;
}

static void
managedvmcase(Type *callerframe, Type *calleeframe, int reverse, int empty, int unwind, int grow, int bytecode)
{
	Prog process = {0};
	Modlink *caller, *callee;
	Frame *entry, *pending;
	WORD ordinal = 0;
	String *text;
	void *argument, *result, *resultslot;
	uchar *root;
	int before, slot;
	const LimboNative64Managed *signature;
	const LimboNative64Entry *entryspec;
	Dis64ManagedSignature decoded, swapped;

	signature = reverse ? &limbo_native64_String_quoted_managed : &limbo_native64_String_unquoted_managed;
	entryspec = reverse ? &limbo_native64_String_quoted_entry : &limbo_native64_String_unquoted_entry;
	before = allocations;
	require(dis64managedread(trialrecords[reverse], Dis64ManagedSignatureBytes, &trialschema, &decoded));
	require(dis64managedread(trialrecords[!reverse], Dis64ManagedSignatureBytes, &trialschema, &swapped));
	require(!dis64managedmatch(&swapped, signature, entryspec));
	require(!dis64managedmatch(&decoded, signature, reverse ? &limbo_native64_String_unquoted_entry :
		&limbo_native64_String_quoted_entry));
	require(managedpair(reverse, signature));
	require(allocations == before);
	require(dis64managedmatch(&decoded, signature, entryspec));
	require(entryspec->frame_size == calleeframe->size &&
		entryspec->map_bytes == calleeframe->np &&
		memcmp(entryspec->map, calleeframe->map, entryspec->map_bytes) == 0);

	caller = managedmodule(0);
	if(grow)
		caller->m->ss = 0;
	caller->m->cstep = native64managedlea8step;
	caller->m->nprog = 1;
	caller->m->prog = mallocz(sizeof(Inst), 1);
	require(caller->m->prog != nil);
	caller->prog = caller->m->prog;
	caller->prog[0].op = ILEA;
	caller->prog[0].add = SRC(AFP)|DST(AFP|AIND);
	caller->prog[0].s.ind = 64;
	caller->prog[0].d.i.f = 40;
	caller->prog[0].d.i.s = 32;
	ptradd(D2H(caller));
	callee = managedmodule(1);
	ptradd(D2H(callee));
	callee->links[0].frame = calleeframe;
	callee->links[0].u.runt = entryspec->function;
	if(bytecode){
		require(!empty);
		callee->m->prog = callee->prog = managedprogram(reverse, unwind);
		callee->m->nprog = 2;
		callee->links[0].u.pc = callee->prog;
		if(bytecode == 2){
			require(compile(callee->m, callee->m->nprog, nil));
			require(callee->m->jit != nil && callee->m->jit->code != nil);
			rxbegin = (uintptr)callee->m->jit->code;
			rxend = rxbegin+callee->m->jit->size;
		}
	}
	process.exval = H;
	process.state = Pready;
	process.R.M = caller;
	process.R.PC = caller->prog;
	entry = mallocz(callerframe->size, 1);
	require(entry != nil);
	entry->t = callerframe;
	R.s = &entry;
	newstack(&process);
	free(entry);
	R = process.R;
	root = R.FP;
	initmem(callerframe, root);
	ptrdel(D2H(caller));
	R.s = &callee;
	R.m = &ordinal;
	R.d = &pending;
	mframe();
	require(pending != nil && pending->t == (grow ? nil : calleeframe));
	if(grow)
		require(((Stkext*)R.EX)->reg.TR == calleeframe);
	memmove(root+40, &pending, sizeof(pending));
	resultslot = root+64;
	/* Neither an absent map bit nor a non-nil cell may publish the address. */
	result = nil;
	memmove((uchar*)pending+32, &result, sizeof(result));
	callerframe->map[1] = 0;
	require(!kprotect(managedbind, nil) && R.PC == caller->prog);
	callerframe->map[1] = 0x80;
	memmove(resultslot, &result, sizeof(result));
	require(!kprotect(managedbind, nil) && R.PC == caller->prog);
	memmove(&result, (uchar*)pending+32, sizeof(result));
	require(result == nil);
	result = H;
	memmove(resultslot, &result, sizeof(result));
	result = (uchar*)pending+sizeof(void*);
	memmove(root+40, &result, sizeof(result));
	require(!kprotect(managedbind, nil) && R.PC == caller->prog);
	memmove(&result, (uchar*)pending+32, sizeof(result));
	require(result == nil);
	memmove(root+40, &pending, sizeof(pending));
	calleeframe->map[1] = 0;
	require(!kprotect(managedbind, nil) && R.PC == caller->prog);
	calleeframe->map[1] = 0x80;
	pending->mr = caller;
	require(!kprotect(managedbind, nil) && R.PC == caller->prog);
	pending->mr = nil;
	memmove(&result, (uchar*)pending+32, sizeof(result));
	require(result == nil);
	require(kprotect(managedbind, nil) && R.PC == caller->prog+1);
	memmove(&result, (uchar*)pending+32, sizeof(result));
	require(result == resultslot);
	text = empty ? H : managedstring();
	argument = reverse && !empty ? (void*)managedlist(text) : text;
	managedput((uchar*)pending+64, argument);
	R.s = &pending;
	R.m = &ordinal;
	R.d = &callee;
	managedprocess = &process;
	managederror = unwind;
	before = managedcalls;
	if(bytecode){
		require(kprotect(managedmcall, nil) && managedcalls == before);
		require(R.M == callee && R.FP == (uchar*)pending && R.PC == callee->prog);
		process.quanta = 1;
		process.R = R;
		numericcollect(&process);
		before = rxcompleted;
		require(kprotect(execute, &process) == !unwind);
		managedfast += rxcompleted-before;
		if(!unwind){
			require(process.R.M == callee && process.R.PC == callee->prog+1);
			numericcollect(&process);
			require(kprotect(execute, &process));
		}
		managedbytecodes++;
	}else
		require(kprotect(managedmcall, nil) == !unwind && managedcalls == before+1);
	memmove(&result, resultslot, sizeof(result));
	if(unwind){
		require(result == H && R.M == callee && R.FP == (uchar*)pending);
		require(strcmp(message, bytecode ? exZdiv : "managed MCALL unwind") == 0);
	}else{
		require(R.M == caller && R.FP == root && D2H(callee)->ref == 1);
		require((result == H) == empty);
		process.R = R;
		numericcollect(&process);
		if(!empty)
			require((reverse ? (String*)result : managedhead(result))->Sascii[0] == 'x');
	}
	destroystack(&R);
	ptrdel(D2H(callee));
	destroy(callee);
	require(worker.nerr == 0 && Tmodlink.ref == 1);
	for(slot=1; slot<GcSlots; slot++)
		require(numericblock(slot)->magic == MAGIC_F);
	require(Tstring.ref == 1 && Tlist.ref == 1 && stringelement->ref == 1 && Tptr.ref == 1);
}

/* Trace only actual mapped storage. Neither the transient address nor C locals root values. */
static void
managedcase(Type *slots, Type *frame, int reverse, int empty, int unwind)
{
	Heap *caller, *callee, *staging;
	void **outer, *parameter, *stage, *result, *argument;
	void *bound;
	String *text;
	int slot;

	caller = heapz(slots);
	ptradd(caller);
	callee = heapz(frame);
	ptradd(callee);
	staging = heapz(stringelement);
	ptradd(staging);
	outer = H2D(void**, caller);
	stage = H2D(void*, staging);
	if(reverse){
		F_String_quoted *call = H2D(F_String_quoted*, callee);
		call->ret = (String**)(outer+1);
		parameter = &call->argv;
	}else{
		F_String_unquoted *call = H2D(F_String_unquoted*, callee);
		call->ret = (List**)(outer+1);
		parameter = &call->args;
	}
	memmove(&argument, parameter, sizeof(argument));
	require(outer[0] == H && outer[1] == H && argument == H && *(void**)stage == H);

	text = empty ? H : managedstring();
	argument = reverse && !empty ? (void*)managedlist(text) : text;
	managedput(outer, argument);
	if(argument != H)
		D2H(argument)->ref++;
	managedput(parameter, argument);
	managedput(outer, H);
	numericcollect(nil);
	if(argument != H){
		require(numericowner(D2H(argument))->magic == MAGIC_A);
		require(numericowner(D2H(text))->magic == MAGIC_A);
		if(reverse)
			require(managedhead(argument) == text &&
				managedhead(((List*)argument)->tail) == text);
		D2H(text)->ref++;
	}
	result = empty ? H : reverse ? (void*)text : managedlist(text);
	managedput(stage, result);
	numericcollect(nil);
	require(outer[1] == H);
	if(result != H)
		require(numericowner(D2H(result))->magic == MAGIC_A);
	if(!unwind){
		bound = reverse ? (void*)H2D(F_String_quoted*, callee)->ret :
			(void*)H2D(F_String_unquoted*, callee)->ret;
		require(bound == outer+1);
		*(void**)stage = H;
		managedput(bound, result);
	}else{
		require(!kprotect(managedfail, nil) && worker.nerr == 0);
		require(strcmp(message, "managed fixture unwind") == 0 && outer[1] == H);
	}
	ptrdel(staging);
	destroy(stage);
	ptrdel(callee);
	destroy(H2D(void*, callee));
	numericcollect(nil);
	require(outer[1] == (unwind ? H : result));
	if(result != H)
		require(numericowner(D2H(result))->magic == (unwind ? MAGIC_F : MAGIC_A));
	if(!unwind && !empty)
		require((reverse ? (String*)outer[1] : managedhead(outer[1]))->Sascii[0] == 'x');
	ptrdel(caller);
	destroy(H2D(void*, caller));
	numericcollect(nil);
	for(slot=1; slot<GcSlots; slot++)
		require(numericblock(slot)->magic == MAGIC_F);
	require(Tstring.ref == 1 && Tlist.ref == 1 && stringelement->ref == 1);
}

/* An omitted frame trace must lose the argument after the collection epochs. */
static void
managedmapcontrol(Type *frame)
{
	Heap *owner;
	F_String_unquoted *call;
	String *text;

	owner = heapz(frame);
	ptradd(owner);
	call = H2D(F_String_unquoted*, owner);
	text = managedstring();
	managedput(&call->args, text);
	frame->mark = noptrs;
	numericcollect(nil);
	require(numericowner(D2H(text))->magic == MAGIC_F);
	call->args = H;
	frame->mark = markheap;
	ptrdel(owner);
	destroy(call);
	require(Tstring.ref == 1);
}

int
main(void)
{
	static uchar two[] = {0xc0}, one[] = {0x80}, resultmap[] = {0, 0x80};
	static uchar quotedmap[] = F_String_quoted_map, unquotedmap[] = F_String_unquoted_map;
	Type *slots, *frames[2], *callerframe;
	int reverse, empty, unwind, jit, grow, refusals, imagefast = 0;

	numericpoolinit();
	Tptr.size = sizeof(void*);
	Tptr.np = 1;
	Tptr.map[0] = 0x80;
	optab[ICONSP] = consp;
	optab[IHEADP] = headp;
	optab[IDIVW] = divw;
	optab[IRET] = ret;
	optab[IMOVP] = movp;
	optab[IMFRAME] = mframe;
	optab[IMCALL] = mcall;
	optab[IEXIT] = iexit;
	managedrecordcontrol();
	managedimagecontrol();
	Tstring.size = sizeof(String);
	Tstring.free = freestring;
	Tstring.mark = noptrs;
	stringelement = dtype(freeheap, sizeof(String*), one, sizeof(one));
	slots = dtype(freeheap, 2*sizeof(void*), two, sizeof(two));
	frames[0] = dtype(freeheap, sizeof(F_String_unquoted), unquotedmap, sizeof(unquotedmap));
	frames[1] = dtype(freeheap, sizeof(F_String_quoted), quotedmap, sizeof(quotedmap));
	callerframe = dtype(freeheap, 72, resultmap, sizeof(resultmap));
	require(stringelement != nil && slots != nil && frames[0] != nil && frames[1] != nil && callerframe != nil);
	for(reverse=0; reverse<2; reverse++)
		for(empty=0; empty<2; empty++)
			for(unwind=0; unwind<2; unwind++){
				managedcase(slots, frames[reverse], reverse, empty, unwind);
				managedvmcase(callerframe, frames[reverse], reverse, empty, unwind, 0, 0);
				managedvmcase(callerframe, frames[reverse], reverse, empty, unwind, 1, 0);
				if(!empty){
					managedvmcase(callerframe, frames[reverse], reverse, empty, unwind, 0, 1);
					managedvmcase(callerframe, frames[reverse], reverse, empty, unwind, 1, 1);
					managedvmcase(callerframe, frames[reverse], reverse, empty, unwind, 0, 2);
					managedvmcase(callerframe, frames[reverse], reverse, empty, unwind, 1, 2);
				}
			}
	managedmapcontrol(frames[0]);
	refusals = managedlinkcontrol();
	for(reverse=0; reverse<2; reverse++)
		for(jit=0; jit<2; jit++)
			for(grow=0; grow<2; grow++)
				imagefast += managedfullcase(reverse, jit, grow);
	freetype(callerframe);
	freetype(frames[1]);
	freetype(frames[0]);
	freetype(slots);
	freetype(stringelement);
	numericdrain();
	require(live == 0);
#if defined(__x86_64__)
	require(managedfast == 4);
	require(imagefast == 4);
#endif
	printf("PASS generated String frames with real GC: 8 ownership cases, missing-map control, %lu epochs, %d checks\n", gcepochs, checks);
	printf("PASS production MFRAME/MCALL with symbolic String callbacks: %d calls\n", managedcalls);
	printf("PASS managed bytecode providers: %d calls with CONS/HEAD/RET or unwind, %d native completions\n",
		managedbytecodes, managedfast);
	printf("PASS paired managed image bytecode: 8 calls, %d native completions\n", imagefast);
	printf("PASS paired managed image constructor: %d allocation refusals\n", refusals);
	return 0;
}
