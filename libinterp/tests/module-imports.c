#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef unsigned char uchar;
typedef struct Import Import;
typedef struct Module Module;
typedef struct Modlink Modlink;
typedef struct Heap Heap;
typedef struct String String;
struct Import { char *name; };
struct Module { int rt, ref; Import **ldt; };
struct Modlink { Module *m; uchar *MP; };
struct Heap { int ref, marked; };
struct String { char *text; };
static struct { Modlink *M; void *s, *m, *d; } R;
#define nil NULL
#define H ((void*)-1)
#define HASLDT 1
#define D2H(p) ((Heap*)((uchar*)(p)-sizeof(Heap)))
#define Setmark(h) ((h)->marked++)
#define OP(name) static void name(void)
#define W(r) (*(WORD*)R.r)
#define S(r) (*(String**)R.r)
static char exModule[] = "module";
static jmp_buf boundary;
static int failed, conversions, reads, links, destroyed, fail_link, checks;
static Import *expected;
static Module loaded;
static Modlink result;

/* Record VM exceptions without allowing destination publication to proceed. */
static void
error(char *message)
{
	assert(message != nil);
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
	assert(strcmp(name, "external") == 0);
	return nil;
}

/* Supply a newly owned module reference at the filesystem boundary. */
static Module*
readmod(char *name, Module *cached, int sync)
{
	assert(strcmp(name, "external") == 0 && cached == nil && sync == 1);
	reads++;
	loaded.ref = 1;
	return &loaded;
}

/* Model linkmod's ownership contract; production linking has its own regression. */
static Modlink*
linkmod(Module *m, Import *table, int mkmp)
{
	assert(table == expected && m->ref > 0);
	assert(mkmp == (m == &loaded));
	links++;
	if(fail_link){
		m->ref--;
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
	assert(*(Modlink**)R.d == (fail_link ? H : &result));
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
		assert(0);
	}
	assert(failed == 1 && !conversions && !reads && !links && !destroyed);
	assert(destination == (Modlink*)(uintptr_t)1234);
	checks++;
}

/* Cover legal self/external loads, aliasing, failed links and MP reference counts. */
static void
loads(Modlink *current)
{
	struct { Heap heap; uchar data[8]; } block = {{1, 0}, {0}};
	WORD index = 1;
	String name, *source = &name;
	Modlink *destination;
	int self, data, alias, failure, before;

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
		assert(0);
	}
	assert(failed == 1 && conversions == 1 && !reads && !links && !destroyed);
	assert(destination == current && current->m->ref == 1);
	checks++;

	for(self = 0; self < 2; self++)
	for(data = 0; data < 2; data++)
	for(alias = 0; alias < 2; alias++)
	for(failure = 0; failure < 2; failure++){
		name.text = self ? "$self" : "external";
		current->MP = data ? block.data : H;
		current->m->ref = 1;
		destination = current;
		R.M = current;
		R.m = &index;
		R.s = &source;
		R.d = alias ? (void*)&R.M : (void*)&destination;
		conversions = reads = links = destroyed = 0;
		fail_link = failure;
		before = block.heap.ref;
		iload();
		assert(conversions == 1 && reads == !self && links == 1 && destroyed == 1);
		assert(block.heap.ref == before+(self && data && !failure));
		assert(current->m->ref == 1+(self && !failure));
		if(!failure && self)
			assert(result.MP == current->MP);
		checks++;
	}
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
	printf("module imports: %d cases, WORD%d\n", checks, WORD_BITS);
	return 0;
}
