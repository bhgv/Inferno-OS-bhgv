#include "dat.h"
#include "fns.h"
#include "interp.h"
#include "pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define ArenaSlots 8 // Bounded collector arena, including its immutable chain header.
#define SlotBytes 256 // Aligned block stride, sufficient for complete Heap and mapped payload.

static union {
	void *alignment; // Keep arena headers naturally aligned without a target-specific allocator.
	uchar bytes[ArenaSlots*SlotBytes]; // Contiguous native pool geometry traversed by actual rungc.
} arena;

static int checks, freed[ArenaSlots], marks, destructions; // Active observations independent of collector counters.
static Type leaf, pair, framekind; // Native aligned descriptors, copied for packed metadata checks.
static Module owner; // Borrowed module owner for scheduler roots.
static Modlink *rootlink; // Native pool object retained by the process root.
static Prog process; // Actual public process representation supplied to the collector.
Module *modules;
Pool *heapmem;
void (*heapmonitor)(int, void*, ulong);


/* Keep every ownership, color and byte-image predicate active under NDEBUG. */
static void
check(int condition)
{
	if(!condition){
		fprintf(stderr, "GC root predicate failed after %d checks\n", checks);
		abort();
	}
	checks++;
}


/* Keep address/undefined checks without process-inspection leak scans in restricted hosts. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* No allocator failure or malformed root is expected in a valid collector graph. */
void
error(char *message)
{
	fprintf(stderr, "unexpected GC root error: %s\n", message);
	abort();
}


/* Preserve unused collector diagnostics without interpreting Plan 9 formats. */
int
print(char *format, ...)
{
	(void)format;
	check(0);
	return 0;
}


/* Expose the actual contiguous arena header to the production sweep loop. */
Bhdr*
poolchain(Pool *pool)
{
	check(pool == heapmem);
	return (Bhdr*)arena.bytes;
}


/* Observe actual sweep release while retaining the arena bytes for deterministic diagnostics. */
void
poolfree(Pool *pool, void *data)
{
	Bhdr *block;
	int index;

	check(pool == heapmem);
	block = (Bhdr*)((uchar*)data-O(Bhdr, u.data));
	index = ((uchar*)block-arena.bytes)/SlotBytes;
	check(index > 0 && index < ArenaSlots && !freed[index]);
	check(block->magic == MAGIC_A);
	freed[index]++;
	block->magic = MAGIC_F;
}


/* Count descriptor ownership released by the actual sweeper. */
void
freetype(Type *type)
{
	check(type != nil && type->ref > 1);
	type->ref--;
}


COLLECTOR_IMPLEMENTATION


/* Count a real traversal without retaining children outside actual map tracing. */
static void
trace(Type *type, void *data)
{
	marks++;
	markheap(type, data);
}


/* Swept objects release descriptor ownership, without recursively freeing other arena slots. */
static void
dispose(Heap *heap, int swept)
{
	check(swept == 1 && heap->ref == 1);
	destructions++;
}


/* Return a fixed native arena object and initialize its exact collector header. */
static void*
object(int index, Type *type)
{
	Bhdr *block;
	Heap *heap;

	check(index > 0 && index < ArenaSlots);
	block = (Bhdr*)(arena.bytes+index*SlotBytes);
	check(block->magic == MAGIC_F);
	block->magic = MAGIC_A;
	heap = B2D(block);
	heap->ref = 1;
	heap->t = type;
	heap->color = mutator;
	if(type != nil)
		type->ref++;
	return H2D(void*, heap);
}


/* Reset the actual collector state and a complete valid arena chain between independent cases. */
static void
setup(void)
{
	Bhdr *block;
	int i;

	check(sizeof(Modlink)+sizeof(Heap)+O(Bhdr, u.data) <= SlotBytes);
	memset(&arena, 0, sizeof(arena));
	memset(freed, 0, sizeof(freed));
	for(i=0; i<ArenaSlots; i++){
		block = (Bhdr*)(arena.bytes+i*SlotBytes);
		block->magic = MAGIC_F;
		block->size = SlotBytes;
	}
	block = (Bhdr*)arena.bytes;
	block->magic = MAGIC_I;
	block->clink = nil;
	block->csize = sizeof(arena.bytes);
	memset(&leaf, 0, sizeof(leaf));
	leaf.ref = 1;
	leaf.size = sizeof(void*);
	leaf.mark = trace;
	leaf.free = dispose;
	pair = leaf;
	pair.np = 1;
	pair.map[0] = 0xc0;
	pair.size = 2*sizeof(void*);
	memset(&owner, 0, sizeof(owner));
	memset(&process, 0, sizeof(process));
	process.exval = H;
	process.state = Pready;
	gccolor = 3;
	mutator = 0;
	marker = 1;
	sweeper = 2;
	base = limit = ptr = nil;
	visit = gce = markdepth = nprop = gchalt = 0;
	gct = 1;
	quanta = Quanta;
	marks = destructions = 0;
	modules = nil;
	rootlink = object(1, nil);
	rootlink->MP = H;
	rootlink->m = &owner;
	process.R.M = rootlink;
}


/* Complete actual collector epochs, failing if a graph prevents bounded progress. */
static void
epochs(int count)
{
	ulong target;
	int steps;

	target = gcepochs+count;
	for(steps=0; gcepochs != target && steps<200; steps++)
		rungc(&process);
	check(gcepochs == target);
}


/* Retain exception cycles in every process state beside independent ordinary roots.
 * Removing one alias must preserve the other; removing all roots must reclaim the cycle. */
static void
exceptions(void)
{
	void *exception, *child, *ordinary, *value;
	Prog other;
	int state, phase, keep;

	for(state=Palt; state<=Pbroken; state++)
	for(phase=0; phase<3; phase++)
	for(keep=0; keep<4; keep++){
		setup();
		gccolor = 3+phase;
		mutator = gccolor%3;
		marker = (gccolor-1)%3;
		sweeper = (gccolor-2)%3;
		D2H(rootlink)->color = mutator;
		process.state = state;
		exception = object(2, &pair);
		child = object(3, &pair);
		ordinary = object(4, &leaf);
		value = H;
		memmove(exception, &child, sizeof(child));
		memmove((uchar*)exception+sizeof(void*), &value, sizeof(value));
		memmove(child, &exception, sizeof(exception));
		memmove((uchar*)child+sizeof(void*), &value, sizeof(value));
		process.exval = exception;
		if(keep == 1){
			modules = &owner;
			owner.origmp = ordinary;
		}
		if(keep == 2)
			rootlink->MP = ordinary;
		if(keep == 3){
			other = process;
			other.exval = child;
			process.next = &other;
		}
		epochs(6);
		check(!freed[1] && !freed[2] && !freed[3]);
		check(freed[4] == (keep == 0 || keep == 3));
		check(D2H(exception)->ref == 1 && D2H(child)->ref == 1 && marks > 0);
		check(pair.ref == 3 && leaf.ref == (keep == 1 || keep == 2 ? 2 : 1));
		process.exval = H;
		epochs(6);
		if(keep == 3){
			check(!freed[2] && !freed[3]);
			process.next = nil;
			epochs(6);
		}
		check(freed[2] == 1 && freed[3] == 1 && !freed[1]);
		check(pair.ref == 1);
		if(keep == 1 || keep == 2){
			check(!freed[4] && leaf.ref == 2);
			modules = nil;
			rootlink->MP = H;
			epochs(6);
		}
		check(freed[4] == 1 && destructions == 3 && leaf.ref == 1);
	}
}


/* Exercise actual rootset over packed frames and descriptors, preserving source bytes. */
static void
frames(void)
{
	union {
		void *alignment; // Aligned origin before all deliberately packed frame views.
		uchar bytes[sizeof(Stkext)+128]; // Complete extent header and two frames with native payloads.
	} storage;
	uchar descriptor[sizeof(Type)+8], original[sizeof(storage)];
	Frame frame;
	Modlink *caller;
	Stkext *extent;
	uchar *fp;
	void *payload, *moduledata;
	Type *type;
	int shift, packed, second;

	for(shift=0; shift<8; shift++)
	for(packed=0; packed<8; packed++)
	for(second=0; second<2; second++){
		if(getenv("GC_ROOT_FRAME_SHIFT") && shift != atoi(getenv("GC_ROOT_FRAME_SHIFT")))
			continue;
		if(getenv("GC_ROOT_TYPE_SHIFT") && packed != atoi(getenv("GC_ROOT_TYPE_SHIFT")))
			continue;
		setup();
		framekind = leaf;
		framekind.size = sizeof(Frame)+sizeof(void*)+shift;
		framekind.np = 1;
		framekind.map[0] = 0x08;
		memset(descriptor, 0xa5, sizeof(descriptor));
		memmove(descriptor+packed, &framekind, sizeof(framekind));
		type = (Type*)(descriptor+packed);
		memset(&storage, 0, sizeof(storage));
		extent = (Stkext*)storage.bytes;
		extent->reg.TR = type;
		fp = extent->reg.tos.fu;
		payload = object(2, &leaf);
		caller = object(3, nil);
		moduledata = object(4, &leaf);
		caller->m = &owner;
		caller->MP = moduledata;
		memset(&frame, 0, sizeof(frame));
		frame.t = second ? &framekind : nil;
		frame.mr = caller;
		memmove(fp, &frame, sizeof(frame));
		memmove(fp+sizeof(Frame), &payload, sizeof(payload));
		fp += framekind.size;
		frame.t = second ? type : &framekind;
		memmove(fp, &frame, sizeof(frame));
		memmove(fp+sizeof(Frame), &payload, sizeof(payload));
		process.R.EX = extent->stack;
		process.R.SP = fp+framekind.size;
		memmove(original, &storage, sizeof(storage));
		gccolor = 4;
		rootset(&process);
		check(D2H(payload)->color == propagator && D2H(payload)->ref == 1);
		check(nprop == 1 && marks == 2);
		check(memcmp(original, &storage, sizeof(storage)) == 0);
		epochs(6);
		check(!freed[1] && !freed[2] && !freed[3] && !freed[4]);
		process.R.EX = process.R.SP = nil;
		epochs(6);
		check(!freed[1] && freed[2] == 1 && freed[3] == 1 && freed[4] == 1);
		check(leaf.ref == 1 && memcmp(original, &storage, sizeof(storage)) == 0);
	}
}


/* Select independent actual sweep and packed-root failures for parent-source replay. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(strcmp(argv[1], "exception") == 0)
		exceptions();
	else if(strcmp(argv[1], "frames") == 0)
		frames();
	else
		check(0);
	printf("PASS GC roots %s: %d active predicates; actual rootset/markheap/rungc, bounded pool\n", argv[1], checks);
	return 0;
}
