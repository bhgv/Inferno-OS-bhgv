/* Symbolic String/List ownership fixture. No file kind or VM call is admitted here. */
#include "string-native64.h"

static Type *stringelement;
static Prog *managedprocess;
static int managederror, managedcalls;

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

static void
managedvmcase(Type *callerframe, Type *calleeframe, int reverse, int empty, int unwind, int grow)
{
	Prog process = {0};
	Modlink *caller, *callee;
	Frame *entry, *pending;
	WORD ordinal = 0;
	String *text;
	void *argument, *result, *resultslot;
	uchar *root;
	int before, slot;

	caller = managedmodule(0);
	if(grow)
		caller->m->ss = 0;
	ptradd(D2H(caller));
	callee = managedmodule(1);
	ptradd(D2H(callee));
	callee->links[0].frame = calleeframe;
	callee->links[0].u.runt = reverse ? String_quoted : String_unquoted;
	process.exval = H;
	process.state = Pready;
	process.R.M = caller;
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
	text = empty ? H : managedstring();
	argument = reverse && !empty ? (void*)managedlist(text) : text;
	managedput((uchar*)pending+64, argument);
	resultslot = root+64;
	if(reverse)
		((F_String_quoted*)pending)->ret = resultslot;
	else
		((F_String_unquoted*)pending)->ret = resultslot;
	R.s = &pending;
	R.d = &callee;
	managedprocess = &process;
	managederror = unwind;
	before = managedcalls;
	require(kprotect(managedmcall, nil) == !unwind && managedcalls == before+1);
	memmove(&result, resultslot, sizeof(result));
	if(unwind){
		require(result == H && R.M == callee && R.FP == (uchar*)pending);
		require(strcmp(message, "managed MCALL unwind") == 0);
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
	require(Tstring.ref == 1 && Tlist.ref == 1 && stringelement->ref == 1);
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
	int reverse, empty, unwind;

	numericpoolinit();
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
				managedvmcase(callerframe, frames[reverse], reverse, empty, unwind, 0);
				managedvmcase(callerframe, frames[reverse], reverse, empty, unwind, 1);
			}
	managedmapcontrol(frames[0]);
	freetype(callerframe);
	freetype(frames[1]);
	freetype(frames[0]);
	freetype(slots);
	freetype(stringelement);
	numericdrain();
	require(live == 0);
	printf("PASS generated String frames with real GC: 8 ownership cases, missing-map control, %lu epochs, %d checks\n", gcepochs, checks);
	printf("PASS production MFRAME/MCALL with symbolic String callbacks: %d calls\n", managedcalls);
	return 0;
}
