/* Appended to the complete collector and bounded test services by native64-gc.py. */
#include "isa.h"
#define LIMBO_NATIVE64_TABLES // Bind actual Math records before creating their frame descriptors.
#include "math-native64.h"
#include "roots-native64.h"
#include "native64-frame.h"

extern void *arraycpy(Array*); // Existing internal heap helper, called only by this explicit test.

static int allocationfailure, allocations, allocationcollections; // Observe actual dtype/heap allocation boundaries.
static int control; // Deliberately remove one root only in explicit negative-control runs.
static F_Roots_caller *observedcaller; // Published caller slot inspected by a real destructor callback.
static Array *observedcopy; // New owner must already be visible before old destruction.
static Heap *observedold; // The only destructor that attempts a blocked real collector slice.
static void (*arrayfree)(Heap*, int); // Canonical actual freearray callback, never replaced numerically.
static int oldcallbacks; // Actual old-owner destruction checkpoints.
static int descriptors; // Successful real dtype backing allocations.
static int graphs, calls; // Real reference graphs and selected actual Math calls.
static union {
	void *alignment; // Ordinary native stack extent alignment.
	uchar bytes[sizeof(Stkext)+2*80+1264]; // Two fixed frames or one complete150-reference frame.
} stackspace;

char exHeap[]="heap", exNilref[]="nil", exType[]="type", exNegsize[]="size";
char exMathia[]="math array";


/* Keep active collector predicates while locating the selected fixture boundary. */
static void
framecheck(int condition, int line)
{
	if(!condition)
		fprintf(stderr, "native64 GC line %d, control %d, graphs %d, calls %d\n", line, control, graphs, calls);
	check(condition);
}

#define check(condition) framecheck(!!(condition), __LINE__) // Preserve original active checks and line evidence.


/* Provide the native descriptor allocator; failure must leave generated metadata unchanged. */
void*
mallocz(ulong bytes, int zero)
{
	void *value;

	if(allocationfailure)
		return nil;
	descriptors++;
	value = malloc(bytes);
	check(value != nil);
	if(zero)
		memset(value, 0, bytes);
	return value;
}


/* Give actual nheap a bounded pool block; collect before a new uninitialized block exists. */
void*
poolalloc(Pool *pool, ulong bytes)
{
	Bhdr *block;
	int index;

	check(pool == heapmem && bytes <= SlotBytes-O(Bhdr, u.data));
	if(allocationcollections){
		epochs(6);
		check(!freed[2]);
	}
	for(index=2; index<ArenaSlots; index++){
		block = (Bhdr*)(arena.bytes+index*SlotBytes);
		if(block->magic != MAGIC_F)
			continue;
		check(freed[index] == 0);
		block->magic = MAGIC_A;
		allocations++;
		return B2D(block);
	}
	check(0);
	return nil;
}


/* Descriptor allocation does not need a C call-site address in this bounded pool. */
uintptr
getcallerpc(void *pointer)
{
	USED(pointer);
	return 0;
}


/* Treat every native pool fault as an active failure. */
static void
badpool(void *pointer, char *message, ulong caller)
{
	USED(pointer);
	USED(message);
	USED(caller);
	check(0);
}

void (*poolfault)(void*, char*, ulong) = badpool; // Exact actual heap consistency boundary.


/* Unused module destruction is outside this bounded graph, even when ASan retains globals. */
void
freedyndata(Modlink *module)
{
	USED(module);
	check(0);
}


/* No real module owner is unloaded by an array/frame root case. */
void
unload(Module *module)
{
	USED(module);
	check(0);
}


/* Retained canonical channel metadata must not invoke an unrelated channel mover. */
void
movtmp(void)
{
	check(0);
}


/* Communication teardown is outside these single-owner array graph cases. */
void
killcomm(Progq **queue)
{
	USED(queue);
	check(0);
}


/* Valid arraycpy needs no nonlocal error transfer; execute the actual callback once. */
int
kprotect(int (*function)(void*), void *argument)
{
	return function(argument);
}


/* A valid copy must never enter the partial-result rethrow branch. */
void
krethrow(void)
{
	check(0);
}


/* Keep an unrelated generated wide-frame entry available without inventing a builtin. */
void
Roots_many(void *frame)
{
	USED(frame);
}


/* A generated result frame is traced here; actual arraycpy supplies its C result below. */
void
Roots_copy(void *frame)
{
	USED(frame);
}


/* The caller record supplies a separately mapped array-result slot. */
void
Roots_caller(void *frame)
{
	USED(frame);
}


/* Build aligned complete native frames and publish their ordinary descriptor roots. */
static uchar*
stackframes(Type *type, int count)
{
	Stkext *extent;
	Frame *frame;
	uchar *first, *current;
	int index;

	memset(&stackspace, 0, sizeof(stackspace));
	extent = (Stkext*)stackspace.bytes;
	extent->reg.TR = type;
	first = extent->reg.tos.fu;
	check(first+count*type->size <= stackspace.bytes+sizeof(stackspace.bytes));
	for(index=0; index<count; index++){
		current = first+index*type->size;
		frame = (Frame*)current;
		frame->t = type;
		frame->mr = nil;
		initmem(type, current);
	}
	process.R.EX = extent->stack;
	process.R.SP = first+count*type->size;
	return first;
}


/* Reset actual collector phases without losing the canonical scalar descriptor owners. */
static void
startcase(int phase, int state)
{
	setup();
	allocationfailure = allocations = allocationcollections = 0;
	gccolor = 3+phase;
	mutator = gccolor%3;
	marker = (gccolor-1)%3;
	sweeper = (gccolor-2)%3;
	D2H(rootlink)->color = mutator;
	process.state = state;
	check(Tarray.ref == 1 && Tbyte.ref == 1 && Tword.ref == 1 && Tptr.ref == 1);
}


/* Force real collections around actual C import/export calls with only generated argument roots. */
static void
matharguments(const LimboNative64Entry *import, const LimboNative64Entry *export, int phase, int state)
{
	static const uchar encoded[] = {0x80,0,0,0, 0xff,0xff,0xff,0xff, 0,0,0,0, 0x7f,0xff,0xff,0xff};
	Type *type;
	F_Math_import_int *frame;
	Array *bytes, *words;
	WORD expected[] = {(-0x7fffffff-1), -1, 0, 0x7fffffff};

	startcase(phase, state);
	type = native64frametype(import);
	check(type != nil && type->size == sizeof(*frame) && type->np == 2);
	frame = (F_Math_import_int*)stackframes(type, 1);
	if(control == 1)
		type->map[1] = 0;
	bytes = H2D(Array*, heaparray(&Tbyte, sizeof(encoded)));
	frame->b = bytes;
	memmove(bytes->data, encoded, sizeof(encoded));
	words = H2D(Array*, heaparray(&Tword, nelem(expected)));
	frame->x = words;
	epochs(6);
	check(!freed[2] && !freed[3]);
	import->function(frame);
	calls++;
	check(memcmp(words->data, expected, sizeof(expected)) == 0);
	epochs(6);
	memset(bytes->data, 0xa5, sizeof(encoded));
	export->function(frame);
	calls++;
	check(memcmp(bytes->data, encoded, sizeof(encoded)) == 0);
	epochs(6);
	check(!freed[2] && !freed[3] && D2H(bytes)->ref == 1 && D2H(words)->ref == 1);
	freeptrs(frame, type);
	initmem(type, frame);
	check(freed[2] == 1 && freed[3] == 1);
	process.R.EX = process.R.SP = nil;
	epochs(6);
	check(Tarray.ref == 1 && Tbyte.ref == 1 && Tword.ref == 1);
	freetype(type);
	graphs++;
}


/* Observe slot publication before actual old-owner destruction; collection is correctly halted. */
static void
oldarrayfree(Heap *heap, int swept)
{
	ulong before, halted;

	if(heap == observedold){
		check(!swept && heap->ref == 0 && gchalt > 0);
		check(observedcaller->result == observedcopy && D2H(observedcopy)->ref == 1);
		before = gcepochs;
		halted = gchalted;
		rungc(&process);
		check(gcepochs == before && gchalted == halted+1);
		oldcallbacks++;
	}
	arrayfree(heap, swept);
}


/* Move an actual arraycpy result from an external temporary root into a mapped caller slot.
 * A shared non-array child has a self-cycle, requiring sweeping after ordinary owners leave. */
static void
copyresult(const LimboNative64Entry *record, const LimboNative64Entry *callerrecord,
	int phase, int state, int replace)
{
	Type *type, *callertype, *nodekind;
	uchar nodemap[] = Roots_Node_map;
	void *node;
	F_Roots_caller *caller;
	F_Roots_copy *callee;
	Array *source, *copy, *old;
	int copyindex, before;
	void *empty;

	startcase(phase, state);
	type = native64frametype(record);
	check(type != nil);
	callertype = native64frametype(callerrecord);
	check(callertype != nil && callertype != type && callertype->size == type->size);
	caller = (F_Roots_caller*)stackframes(type, 2);
	caller->regs.t = callertype;
	callee = (F_Roots_copy*)((uchar*)caller+type->size);
	callee->ret = &caller->result;
	source = H2D(Array*, heaparray(&Tptr, 1));
	callee->source = source;
	nodekind = dtype(freeheap, sizeof(Roots_Node), nodemap, sizeof(nodemap));
	check(nodekind != nil);
	node = H2D(void*, heapz(nodekind));
	memmove(source->data, &node, sizeof(node));
	memmove(node, &node, sizeof(node));
	D2H(node)->ref++;
	old = H;
	if(replace){
		old = H2D(Array*, heaparray(&Tptr, 0));
		caller->result = old;
	}
	copyindex = replace ? 5 : 4;
	before = oldcallbacks;
	epochs(6);
	allocationcollections = 1;
	copy = arraycpy(source);
	allocationcollections = 0;
	check(copy != source && copy->len == 1 && copy->t == &Tptr);
	check(memcmp(copy->data, &node, sizeof(node)) == 0 && D2H(node)->ref == 3);
	if(control != 2)
		ptradd(D2H(copy));
	empty = H;
	memmove(&callee->source, &empty, sizeof(empty));
	destroy(source);
	epochs(6);
	check(freed[2] == 1 && !freed[3] && !freed[copyindex] && D2H(node)->ref == 2);
	check(!replace || !freed[4]);
	memmove(callee->ret, &copy, sizeof(copy));
	ptrdel(D2H(copy));
	if(replace){
		observedcaller = caller;
		observedcopy = copy;
		observedold = D2H(old);
		Tarray.free = oldarrayfree;
		destroy(old);
		Tarray.free = arrayfree;
		observedold = nil;
		check(oldcallbacks == before+1 && freed[4] == 1);
	}
	if(control == 3)
		callertype->map[1] = 0;
	epochs(6);
	check(freed[2] == 1 && !freed[3] && !freed[copyindex] && D2H(copy)->ref == 1);
	freeptrs(caller, type);
	initmem(type, caller);
	check(freed[copyindex] == 1 && !freed[3] && D2H(node)->ref == 1);
	process.R.EX = process.R.SP = nil;
	epochs(6);
	check(freed[2] == 1 && freed[3] == 1 && Tarray.ref == 1 && Tptr.ref == 1);
	freetype(nodekind);
	freetype(callertype);
	freetype(type);
	graphs++;
}


/* A reference in byte20 of a real dynamic descriptor must survive and later release normally. */
static void
widetype(const LimboNative64Entry *record, int phase)
{
	Type *type;
	uchar *frame, *copiedmap;
	Array *source;
	unsigned int offset;
	LimboNative64Entry invalid;
	int before;

	startcase(phase, Pready);
	invalid = *record;
	invalid.map_bytes = 16;
	before = descriptors;
	check(native64frametype(&invalid) == nil && descriptors == before);
	invalid = *record;
	invalid.function = nil;
	check(native64frametype(&invalid) == nil && descriptors == before);
	allocationfailure = 1;
	check(native64frametype(record) == nil);
	allocationfailure = 0;
	type = native64frametype(record);
	check(type != nil && type->np == 20 && type->size == 1264);
	copiedmap = (uchar*)type+offsetof(Type, map);
	check(memcmp(copiedmap, record->map, 20) == 0);
	copiedmap[19] ^= 0x80;
	check(copiedmap[19] != record->map[19]);
	copiedmap[19] ^= 0x80;
	frame = stackframes(type, 1);
	source = H2D(Array*, heaparray(&Tbyte, 1));
	source->data[0] = 0x5a;
	offset = offsetof(F_Roots_many, a149);
	memmove(frame+offset, &source, sizeof(source));
	if(control == 4)
		type->np = 16;
	epochs(6);
	check(!freed[2] && source->data[0] == 0x5a);
	freeptrs(frame, type);
	initmem(type, frame);
	check(freed[2] == 1);
	process.R.EX = process.R.SP = nil;
	freetype(type);
	graphs++;
}


/* Exercise real frame/type/heap/collector bodies across every process state and color phase. */
int
main(int argc, char **argv)
{
	const LimboNative64Entry *selected[] = {
		&limbo_native64_Math_import_int_entry,
		&limbo_native64_Math_export_int_entry,
	};
	const LimboNative64Entry *bound[2];
	const LimboNative64Entry *roots[] = {
		&limbo_native64_Roots_copy_entry,
		&limbo_native64_Roots_caller_entry,
		&limbo_native64_Roots_many_entry,
	};
	const LimboNative64Entry *boundroots[3];
	int phase, state;

	if(argc == 2){
		if(strcmp(argv[1], "arguments") == 0) control = 1;
		else if(strcmp(argv[1], "temporary") == 0) control = 2;
		else if(strcmp(argv[1], "caller") == 0) control = 3;
		else if(strcmp(argv[1], "wide") == 0) control = 4;
		else check(0);
	}else
		check(argc == 1);
	arrayfree = Tarray.free;
	check(limbo_native64_Math_layout() && limbo_native64_Roots_layout());
	check(native64bind(limbo_native64_Math_specs, limbo_native64_Math_spec_count,
		selected, nelem(selected), bound, nelem(bound)));
	check(native64bind(limbo_native64_Roots_specs, limbo_native64_Roots_spec_count,
		roots, nelem(roots), boundroots, nelem(boundroots)));
	for(phase=0; phase<3; phase++){
		for(state=Palt; state<=Pbroken; state++){
			if(control == 0 || control == 1)
				matharguments(bound[0], bound[1], phase, state);
			if(control == 0 || control == 2 || control == 3)
				copyresult(boundroots[0], boundroots[1], phase, state, 0);
			if(control == 0)
				copyresult(boundroots[0], boundroots[1], phase, state, 1);
		}
		if(control == 0 || control == 4)
			widetype(boundroots[2], phase);
	}
	check(control != 0 || oldcallbacks == 24);
	printf("PASS native64 GC: %d predicates, %d graphs, %d actual Math calls; real dtype/arraycpy/collector, bounded pool\n", checks, graphs, calls);
	return 0;
}
