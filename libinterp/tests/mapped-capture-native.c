#define TEST_LEAF_DISPATCH_RUNTIME // Execute the complete production fallback route.
#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
#include <stdio.h>
#include <pthread.h>

Type Tstring = {1}, Tarray = {1};
REG R;
int mutator, nprop;
void (*optab[256])(void);
char exModule[] = "module", exHeap[] = "heap", exNilref[] = "nil";
char exType[] = "type", exBounds[] = "bounds", exOp[] = "opcode";
static int operation, route, count, maskkind, sa, da, redirect, pattern;
static int callbacks, destroyed;
static unsigned long cases;
static uchar alternate[128];

struct CaptureOwner {
	Heap heap; // Real refcount and mark-barrier fields.
	uvlong payload; // Stable owner identity; bounded fixture storage is never freed.
};

static struct CaptureOwner sources[3], previous[3];
static union {
	void *alignment; // Native Heap/List baseline.
	uchar bytes[512]; // List source and payload canaries.
} liststorage;
static uchar input[128], output[128], wanted[128], sourcebefore[128];
static uchar sourcecell[32], indexcell[32];
static Type record;
static Type *types[1] = {&record};
static Module owner;
static Modlink modulelink;
static List *list;
static int expected[3];


/* Diagnose the complete active geometry with predicates enabled under NDEBUG. */
static void
capturecheck(int test)
{
	if(!test) {
		fprintf(stderr, "mapped capture op=%d route=%d count=%d mask=%d sa=%d da=%d redirect=%d pattern=%d\n",
			operation, route, count, maskkind, sa, da, redirect, pattern);
		abort();
	}
}


/* Allocate only compiler publication storage, independently of static VM owners. */
void*
mallocz(ulong size, int clear)
{
	void *value;

	value = malloc(size);
	if(value && clear)
		memset(value, 0, size);
	return value;
}


/* Valid callback cases must never leave the protected execution boundary. */
int
kprotect(int (*function)(void*), void *argument)
{
	return function(argument);
}


/* A metadata or ownership error in a valid fixture is always a test failure. */
void
error(char *message)
{
	fprintf(stderr, "unexpected mapped error: %s\n", message);
	abort();
}


/* Model a destructor borrowing REG at the exact zero-reference callback boundary. */
void
destroy(void *value)
{
	Heap *heap;

	if(value == H)
		return;
	heap = D2H(value);
	capturecheck(heap->ref != 0);
	if(--heap->ref != 0)
		return;
	destroyed++;
	if(redirect && (redirect == 2 || callbacks == 0)) {
		R.s = alternate;
		R.d = alternate+32;
		R.m = alternate+64;
		R.M = nil;
		callbacks++;
	}
}


/* Keep ASan enabled while avoiding unavailable /proc-based leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define OP(name) void name(void) // Production decoded opcode entry.
IMPLEMENTATION


/* Check map ownership and captured publication under register-borrowing destructors. */
static void
checkcopies(Module *code)
{
	void *source, *destination, *pointer;
	WORD index;
	Type *descriptor;
	Heap *heap;
	uchar *payload;
	int i, mapped, id, opcodes[] = {IMOVMP, IHEADMP};
	REG initial;

	owner.type = types;
	owner.ntype = 1;
	modulelink.m = &owner;
	for(operation = 0; operation < 3; operation++)
	for(route = 0; route < (operation == 2 ? 1 : 3); route++)
	for(count = 1; count <= 8; count++)
	for(maskkind = 0; maskkind < 3; maskkind++)
	for(sa = 0; sa < 8; sa++)
	for(da = 0; da < 8; da++)
	for(redirect = 0; redirect < 3; redirect++)
	for(pattern = 0; pattern < 2; pattern++) {
		if(getenv("MAPPED_CAPTURE_OPERATION") && operation != atoi(getenv("MAPPED_CAPTURE_OPERATION")))
			continue;
		if(getenv("MAPPED_CAPTURE_ROUTE") && route != atoi(getenv("MAPPED_CAPTURE_ROUTE")))
			continue;
		memset(&record, 0, sizeof(record));
		record.size = count*sizeof(void*);
		record.np = 1;
		record.ref = 1;
		record.map[0] = maskkind == 0 ? (uchar)(0xff << (8-count)) :
			maskkind == 1 ? 0x80 : (uchar)((0xff << (8-count)) & 0xaa);
		memset(input, 0x5a, sizeof(input));
		memset(output, 0xa5, sizeof(output));
		memset(alternate, 0x79, sizeof(alternate));
		memset(sources, 0, sizeof(sources));
		memset(previous, 0, sizeof(previous));
		memset(expected, 0, sizeof(expected));
		for(i = 0; i < 3; i++)
			sources[i].heap.ref = 1;
		for(i = 0; i < count; i++) {
			mapped = record.map[0] & (0x80>>i);
			if(!mapped)
				continue;
			id = i%3;
			pointer = pattern && i%2 ? H : &sources[id].payload;
			if(pointer != H)
				expected[id]++;
			memmove(input+sa+i*sizeof(pointer), &pointer, sizeof(pointer));
			pointer = &previous[id].payload;
			previous[id].heap.ref++;
			memmove(output+da+i*sizeof(pointer), &pointer, sizeof(pointer));
		}
		memmove(sourcebefore, input, sizeof(input));
		memmove(wanted, output, sizeof(output));
		memmove(wanted+da, input+sa, record.size);
		memset(liststorage.bytes, 0, sizeof(liststorage.bytes));
		list = (List*)(liststorage.bytes+sizeof(Heap));
		heap = D2H(list);
		heap->ref = 1;
		payload = (uchar*)list+offsetof(List, data);
		memmove(payload, input+sa, record.size);
		source = operation == 1 ? (void*)payload : input+sa;
		destination = output+da;
		memset(&R, 0, sizeof(R));
		R.s = source;
		R.d = destination;
		R.M = &modulelink;
		index = 0;
		memmove(indexcell+sa, &index, sizeof(index));
		R.m = indexcell+sa;
		if(operation == 1) {
			memmove(sourcecell+sa, &list, sizeof(list));
			R.s = sourcecell+sa;
		} else if(operation == 2) {
			descriptor = &record;
			memmove(indexcell+sa, &descriptor, sizeof(descriptor));
		}
		initial = R;
		callbacks = destroyed = 0;
		mutator = 1;
		nprop = 0;
		if(route == 0) {
			if(operation == 0)
				movmp();
			else if(operation == 1)
				headmp();
			else
				movtmp();
		} else if(route == 1) {
			capturecheck(code->jit->entry[opcodes[operation]](&R) == 0);
			capturecheck(memcmp(&R, &initial, sizeof(R)) == 0);
			capturecheck(cjitexec(&R, opcodes[operation], code->jit->entry[opcodes[operation]]) == 1);
		} else
			capturecheck(code->jit->dispatch[opcodes[operation]](&R) == 1);
		capturecheck(memcmp(output, wanted, sizeof(output)) == 0);
		capturecheck(memcmp(input, sourcebefore, sizeof(input)) == 0);
		capturecheck(memcmp(payload, input+sa, record.size) == 0);
		for(i = 0; i < 3; i++) {
			capturecheck(sources[i].heap.ref == 1+expected[i]);
			capturecheck(sources[i].heap.color == (expected[i] ? propagator : 0));
			capturecheck(previous[i].heap.ref == 0);
		}
		capturecheck(D2H(list)->ref == 1 && destroyed > 0 && nprop == 1);
		if(redirect) {
			capturecheck(callbacks > 0);
			initial.s = alternate;
			initial.d = alternate+32;
			initial.m = alternate+64;
			initial.M = nil;
		} else {
			if(operation == 1)
				initial.s = payload;
		}
		capturecheck(memcmp(&R, &initial, sizeof(R)) == 0);
		for(i = 0; i < sizeof(alternate); i++)
			capturecheck(alternate[i] == 0x79);
		cases++;
	}
}


/* Compile actual guarded/complete leaves once for the single VM worker. */
static void*
captureworker(void *unused)
{
	Module code = {0};
	Inst instructions[2] = {{0}};

	(void)unused;
	instructions[0].op = IMOVMP;
	instructions[1].op = IHEADMP;
	code.prog = instructions;
	code.nprog = 2;
	capturecheck(compile(&code, 2, nil));
	optab[IMOVMP] = movmp;
	optab[IHEADMP] = headmp;
	checkcopies(&code);
	if(getenv("MAPPED_CAPTURE_OPERATION") == nil && getenv("MAPPED_CAPTURE_ROUTE") == nil)
		capturecheck(cases == 64512);
	code.jit->freecode(code.jit->code, code.jit->size);
	free(code.jit);
	printf("PASS mapped captures: %lu callback/map/image/C/RX cases WORD%zu\n", cases, sizeof(WORD)*8);
	return nil;
}


/* Join the only VM worker so ASan tracks all stack ownership and callbacks. */
int
main(void)
{
	pthread_t worker;

	if(pthread_create(&worker, nil, captureworker, nil) != 0 || pthread_join(worker, nil) != 0)
		return 1;
	return 0;
}
