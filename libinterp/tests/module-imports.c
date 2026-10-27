#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <stddef.h>

#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef unsigned char uchar;
typedef uintptr_t uintptr; // Native address arithmetic in the real loader.
typedef unsigned long ulong;
typedef struct Import Import;
typedef struct Module Module;
typedef struct Modlink Modlink;
typedef struct Heap Heap;
typedef struct String String;
typedef struct FixtureReg FixtureReg;
typedef struct SelfData SelfData;

struct Import {
	char *name; // Import entry identity used by the fixture linker.
};

struct Module {
	int rt; // Runtime/import-table flags.
	int ref; // Signed module owner count.
	Import **ldt; // Sentinel-terminated import table.
};

struct Modlink {
	Module *m; // Referenced module descriptor.
	uchar *MP; // Owned module-data heap payload or VM nil.
};

struct Heap {
	ulong ref; // Native unsigned heap owner count, independent of WORD.
	int marked; // Number of observed write barriers.
};

struct String {
	char *text; // Fixture spelling returned by string conversion.
};

struct FixtureReg {
	Modlink *M; // Current module link, optionally aliased by the destination.
	void *s, *m, *d; // Byte-addressed source, index and destination operands.
};

struct SelfData {
	Heap heap; // Native header immediately before module-data bytes.
	uchar data[8]; // Small independently retained module-data payload.
};

static FixtureReg R;
#define O(t,e) offsetof(t,e) // Actual fixture field offsets for byte-safe metadata reads.
#define nil NULL // Native null pointer distinct from VM nil.
#define H ((void*)-1) // VM nil sentinel.
#define HASLDT 1 // Module has a live import table.
#define D2H(p) ((Heap*)((uchar*)(p)-sizeof(Heap))) // Recover the real native header.
#define Setmark(h) ((h)->marked++) // Observe publication barriers.
#define OP(name) static void name(void) // Production interpreter entry signature.
#define W(r) (*(WORD*)R.r) // Legacy WORD fixture access.
#define S(r) (*(String**)R.r) // Legacy string fixture access.
static char exModule[] = "module";
static char exHeap[] = "heap";
static char exBounds[] = "bounds"; // Invalid decoded cell span.
static jmp_buf boundary;
static int failed, conversions, reads, links, destroyed, fail_link, checks;
static Import *expected;
static Module loaded;
static Modlink result;


/* Record VM exceptions without allowing destination publication to proceed. */
static void
error(char *message)
{
	if(!(message != nil))abort();
	failed++;
	longjmp(boundary, 1);
}


/* Observe that import validation precedes string conversion and filesystem work. */
static char*
string2c(String *s)
{
	conversions++;
	return s->text;
}


/* Keep external lookup distinct from module reading. */
static Module*
lookmod(char *name)
{
	if(!(strcmp(name, "external") == 0))abort();
	return nil;
}


/* Supply a newly owned module reference at the filesystem boundary. */
static Module*
readmod(char *name, Module *cached, int sync)
{
	if(!(strcmp(name, "external") == 0 && cached == nil && sync == 1))abort();
	reads++;
	loaded.ref = 1;
	return &loaded;
}


/* Model linkmod's ownership contract; production linking has its own regression. */
static Modlink*
linkmod(Module *m, Import *table, int mkmp)
{
	if(!(table == expected && m->ref > 0))abort();
	if(!(mkmp == (m == &loaded)))abort();
	links++;
	if(fail_link){
		m->ref--;
		if(fail_link == 2)error(exHeap);
		return H;
	}
	result.m = m;
	result.MP = H;
	return &result;
}


/* Check publication happens before the previous destination is released. */
static void
destroy(void *old)
{
	(void)old;
	Modlink *published;
	memmove(&published,R.d,sizeof(published));
	if(!(published == (fail_link ? H : &result)))abort();
	destroyed++;
}


/* LeakSanitizer cannot inspect threads in this sandbox. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION


/* Rejected input preserves the old destination and performs no external work. */
static void
reject(Modlink *current, WORD index)
{
	Modlink *destination = (Modlink*)(uintptr_t)1234;
	String name = {"external"}, *source = &name;

	R.M = current;
	R.m = &index;
	R.s = &source;
	R.d = &destination;
	failed = conversions = reads = links = destroyed = 0;
	if(setjmp(boundary) == 0){
		iload();
		if(!(0))abort();
	}
	if(!(failed == 1 && !conversions && !reads && !links && !destroyed))abort();
	if(!(destination == (Modlink*)(uintptr_t)1234))abort();
	checks++;
}


/* Cover legal self/external loads, aliasing, failed links and MP reference counts. */
static void
loads(Modlink *current)
{
	SelfData block = {{1, 0}, {0}};
	WORD index = 1;
	String name, *source = &name;
	Modlink *destination;
	int self, data, alias, failure, before, sa, ma, da;
	uchar slots[3][24];

	/* Raw native NULL is invalid; unlike the VM nil sentinel it owns no heap. */
	name.text = "$self";
	current->MP = nil;
	current->m->ref = 1;
	destination = current;
	R.M = current;
	R.m = &index;
	R.s = &source;
	R.d = &destination;
	failed = conversions = reads = links = destroyed = 0;
	if(setjmp(boundary) == 0){
		iload();
		if(!(0))abort();
	}
	if(!(failed == 1 && conversions == 1 && !reads && !links && !destroyed))abort();
	if(!(destination == current && current->m->ref == 1))abort();
	checks++;

	for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++)
	for(self = 0; self < 2; self++)
	for(data = 0; data < 2; data++)
	for(alias = 0; alias < 2; alias++)
	for(failure = 0; failure < 2; failure++){
		name.text = self ? "$self" : "external";
		current->MP = data ? block.data : H;
		current->m->ref = 1;
		destination = current;
		R.M = current;
		R.m = slots[0]+ma;R.s = slots[1]+sa;
		R.d = alias ? (void*)&R.M : slots[2]+da;
		memmove(R.m,&index,sizeof(index));memmove(R.s,&source,sizeof(source));
		if(!alias)memmove(R.d,&destination,sizeof(destination));
		conversions = reads = links = destroyed = 0;
		fail_link = failure;
		before = block.heap.ref;
		iload();
		if(!(conversions == 1 && reads == !self && links == 1 && destroyed == 1))abort();
		if(!(block.heap.ref == before+(self && data && !failure)))abort();
		if(!(current->m->ref == 1+(self && !failure)))abort();
		if(!failure && self)
			if(!(result.MP == current->MP))abort();
		checks++;
	}
}


/* Reserve both owners before linking, including errors and aliased destinations. */
static void
selfrefs(Modlink *current)
{
	SelfData block;
	uchar slots[3][32], saved[3][32];
	String name = {"$self"}, *source = &name;
	Modlink *destination, *published;
	WORD index = 1;
	ulong oldheap;
	int mode, sa, ma, da, alias, oldmodule, ok, valid, success;
	char *selection = getenv("SELF_RETAIN_MODE");

	for(mode = 0; mode < 12; mode++)
	for(sa = 0; sa < 8; sa++)for(ma = 0; ma < 8; ma++)for(da = 0; da < 8; da++)
	for(alias = 0; alias < 2; alias++){
		if(selection && mode != atoi(selection))continue;
		memset(&block, 0, sizeof(block));
		block.heap.ref = 5;
		current->m->ref = 1;
		current->MP = block.data;
		if(mode == 0)current->m->ref = 0;
		if(mode == 1)current->m->ref = -1;
		if(mode == 2)current->m->ref = 0x7fffffff;
		if(mode == 3)current->m->ref = 0x7ffffffe;
		if(mode == 4)block.heap.ref = 0;
		if(mode == 5)block.heap.ref = (ulong)-1;
		if(mode == 6)block.heap.ref = (ulong)-2;
		if(mode == 7){current->MP = H; block.heap.ref = (ulong)-1;}
		if(mode == 11){current->m->ref = 0x7ffffffe; block.heap.ref = (ulong)-2;}
		fail_link = mode == 8 ? 1 : mode == 9 ? 2 : 0;
		memset(slots, 0xa5, sizeof(slots));
		R.M = current;
		R.s = slots[0]+sa;
		R.m = slots[1]+ma;
		R.d = alias ? (void*)&R.M : slots[2]+da;
		destination = current;
		memmove(R.s, &source, sizeof(source));
		memmove(R.m, &index, sizeof(index));
		memmove(R.d, &destination, sizeof(destination));
		memmove(saved, slots, sizeof(slots));
		oldmodule = current->m->ref;
		oldheap = block.heap.ref;
		failed = conversions = reads = links = destroyed = 0;
		ok = 0;
		if(setjmp(boundary) == 0){iload(); ok = 1;}
		valid = mode >= 3 && mode != 4 && mode != 5 && mode != 9;
		success = valid && mode != 8;
		if(ok != valid || conversions != 1 || reads ||
		   links != (valid || mode == 9) || destroyed != valid || failed != !valid)abort();
		if(current->m->ref != oldmodule+success ||
		   block.heap.ref != oldheap+(success && current->MP != H) ||
		   block.heap.marked != (success && current->MP != H))abort();
		memmove(&published, R.d, sizeof(published));
		if(published != (valid ? (success ? &result : H) : current))abort();
		if(!valid && (R.M != current || memcmp(saved, slots, sizeof(slots))))abort();
		if(success && result.MP != current->MP)abort();
		checks++;
	}
	current->m->ref = 1;
	current->MP = H;
	fail_link = 0;
}


/* Sentinel and large indices must be rejected in both VM word models. */
int
main(void)
{
	Import first = {"first"}, second = {"second"};
	Import *tables[] = {&first, &second, nil};
	Module module = {HASLDT, 1, tables};
	Modlink current = {&module, H};

	expected = &second;
	reject(H, 0);
	reject(nil, 0);
	current.m = nil; reject(&current, 0);
	current.m = H; reject(&current, 0);
	current.m = &module;
	reject(&current, -1);
	reject(&current, 2);
	reject(&current, 0x7fffffff);
	if(WORD_BITS == 64)
		reject(&current, (WORD)(UINT64_C(1)<<32));
	module.ldt = nil; reject(&current, 0);
	module.ldt = H; reject(&current, 0);
	module.ldt = tables;
	module.rt = 0; reject(&current, 0);
	module.rt = HASLDT;
	loads(&current);
	selfrefs(&current);
	printf("module imports: %d cases, WORD%d\n", checks, WORD_BITS);
	return 0;
}
