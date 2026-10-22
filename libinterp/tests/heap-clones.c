#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "pool.h"
#include "raise.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static Proc worker;
static char *raised;
static int live, allocations, failat, checks;
static struct CloneBlock { Heap *heap; ulong size; } blocks[512];
void freearray(Heap*, int);
void freemodlink(Heap*, int);

static Type leaf = {1}, pointer = {1}, record = {1}, atom = {1}, mp = {1};
Type Tarray = {1}, Tmodlink = {1};
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
char exHeap[] = "heap", exNegsize[] = "negative";

/* Use the actual emulator error-stack protocol and preserve its error identity. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *s) { raised = s; nexterror(); }
/* LeakSanitizer process inspection is unavailable in this sandbox. */
int __lsan_is_turned_off(void) { return 1; }

/* Inject allocation failure and guard the exact real heap payload. */
void*
poolalloc(Pool *pool, ulong size)
{
	Heap *h;
	int i;

	(void)pool; allocations++;
	if(allocations == failat || size > 1024*1024)return nil;
	h = malloc(size+32); assert(h); memset(h, 0xa5, size+32);
	for(i = 0; i < 512 && blocks[i].heap; i++);
	assert(i < 512); blocks[i].heap = h; blocks[i].size = size; live++;
	return h;
}

/* Fixture descriptors keep one static owner after all heap references are gone. */
void freetype(Type *t) { assert(t->ref > 1); t->ref--; }

/* Execute real freeheap/freearray and freeptrs before releasing guarded storage. */
void
destroy(void *p)
{
	Heap *h;
	int i, j;

	if(p == H)return;
	h = D2H(p); assert(h->ref > 0);
	if(--h->ref)return;
	for(i = 0; i < 512 && blocks[i].heap != h; i++);
	assert(i < 512);
	for(j = 0; j < 32; j++)assert(((uchar*)h)[blocks[i].size+j] == 0xa5);
	if(h->t == &Tarray)freearray(h, 0);
	else if(h->t == &Tmodlink)freemodlink(h, 0);
	else freeheap(h, 0);
	freetype(h->t); blocks[i].heap = nil; free(h); live--;
}

static int dynamic_error, initcalls, endcalls;

/* Model owned dynamic data at the native hook boundary, including a late error. */
void
newdyndata(Modlink *ml)
{
	initcalls++;
	ml->data = H2D(uchar*, heapz(&atom));
	if(dynamic_error)error(exHeap);
}

/* Release only storage acquired by the injected initializer. */
void
freedyndata(Modlink *ml)
{
	endcalls++;
	if(ml->data != nil)destroy(ml->data);
}

/* A fixture cache owns one extra module reference during each instantiation. */
void
unload(Module *m)
{
	assert(m->ref > 0);
	if(--m->ref == 0)destroy(m->origmp);
}

/* Keep diagnostic formatting separate from the actual binding decisions. */
void kwerrstr(char *s, ...) { (void)s; }
static int quiet(char *fmt, ...) { (void)fmt; return 0; }
#define snprint snprintf
#define print quiet
#define BIT(bt, nb) ((bt)&(1<<(nb)))
IMPLEMENTATION

static Array *source, *result;
static void *mp_source, *mp_dest;
static int operation, make_mp;
static Module module;
static Import imports[2];
static Link exports[2];
static Type *moduletypes[1];
static Modlink *instance;

/* Invoke the real clone through an outer error frame, including recursive rethrows. */
static int
run(void *unused)
{
	(void)unused;
	if(operation == 0)result = arraycpy(source);
	else if(operation == 1)newmp(mp_dest, mp_source, &mp);
	else instance = linkmod(&module, imports, make_mp);
	return 1;
}

/* Build native pointer maps while varying only the Limbo scalar width. */
static void
setup(void)
{
	assert(live == 0 && worker.nerr == 0);
	Tarray.size = sizeof(Array);
	leaf.size = sizeof(WORD);
	pointer.size = sizeof(void*); pointer.np = 1; pointer.map[0] = 0x80;
	record.size = 4*sizeof(void*); record.np = 1; record.map[0] = 0x70;
	atom.size = sizeof(WORD);
	mp.size = 4*sizeof(void*); mp.np = 1; mp.map[0] = 0xf0;
	allocations = 0; failat = -1; source = result = H; mp_source = mp_dest = H;
}

/* A shared source child must produce independent mutable descendants in each clone. */
static Array*
tree(int depth)
{
	Array *a, *child;
	WORD *data;
	void **p;

	if(depth == 0){
		a = H2D(Array*, heaparray(&leaf, 3)); data = (WORD*)a->data;
		data[0] = 17; data[1] = -23; data[2] = 45;
	}else{
		a = H2D(Array*, heaparray(&pointer, 3)); p = (void**)a->data;
		child = tree(depth-1); p[0] = p[2] = child; D2H(child)->ref++;
	}
	return a;
}

/* Compare all scalar bytes and recursively check array separation and retained refs. */
static void
verify(Array *a, Array *b)
{
	void **s, **d;
	int i, bit;

	assert(a != b && a->t == b->t && a->len == b->len && b->root == H);
	for(i = 0; i < a->len; i++){
		s = (void**)(a->data+i*a->t->size); d = (void**)(b->data+i*b->t->size);
		if(a->t == &leaf)assert(!memcmp(s, d, sizeof(WORD)));
		else{
			if(a->t == &record)assert(!memcmp(s, d, sizeof(WORD)));
			for(bit = 0; bit < a->t->size/sizeof(void*); bit++){
				if((a->t->map[0] & (0x80>>bit)) == 0)continue;
				if(s[bit] == H)assert(d[bit] == H);
				else if(D2H(s[bit])->t == &Tarray)verify(s[bit], d[bit]);
				else assert(s[bit] == d[bit]);
			}
			if(a->t == &pointer && a->len == 3)assert(((void**)b->data)[0] != ((void**)b->data)[2]);
		}
	}
}

typedef struct Snapshot Snapshot;
struct Snapshot { Heap *heap[512]; int refs[512], types[6], count; };

/* Record every original object and Type owner, not just a total allocation count. */
static void
save(Snapshot *s)
{
	int i;

	s->count = 0;
	for(i = 0; i < 512; i++)if(blocks[i].heap){
		s->heap[s->count] = blocks[i].heap;
		s->refs[s->count++] = blocks[i].heap->ref;
	}
	s->types[0] = Tarray.ref; s->types[1] = leaf.ref; s->types[2] = pointer.ref;
	s->types[3] = record.ref; s->types[4] = atom.ref; s->types[5] = mp.ref;
}

/* Failed clones and destroyed successful clones must exactly restore the snapshot. */
static void
unchanged(Snapshot *s)
{
	int i;

	assert(live == s->count && worker.nerr == 0);
	for(i = 0; i < s->count; i++)assert(s->heap[i]->ref == s->refs[i]);
	assert(Tarray.ref == s->types[0] && leaf.ref == s->types[1] && pointer.ref == s->types[2]);
	assert(record.ref == s->types[3] && atom.ref == s->types[4] && mp.ref == s->types[5]);
}

/* Clone real mapped nested arrays and inject failure at every node of the clone tree. */
static void
arrays(void)
{
	Snapshot original;
	int depth, count, point, i;
	Array *view;

	for(depth = 0; depth <= 5; depth++){
		setup(); operation = 0; source = tree(depth); save(&original);
		allocations = 0; assert(kprotect(run, nil)); count = allocations;
		verify(source, result); destroy(result); result = H; unchanged(&original); checks++;
		for(point = 1; point <= count; point++){
			allocations = 0; failat = point; raised = nil;
			assert(!kprotect(run, nil)); assert(raised == exHeap && result == H);
			unchanged(&original); checks++;
		}
		failat = -1;
		/* Both source branches still refer to the original, unchanged descendants. */
		if(depth)assert(((void**)source->data)[0] == ((void**)source->data)[2]);
		destroy(source); source = H; assert(live == 0);
	}
	setup(); source = H2D(Array*, heaparray(&record, 3));
	for(i = 0; i < 3; i++){
		void **p = (void**)(source->data+i*record.size);
		*(WORD*)p = 99+i; p[1] = H2D(void*, heapz(&atom)); p[2] = tree(2);
	}
	save(&original); operation = 0; allocations = 0;
	assert(kprotect(run, nil)); count = allocations; verify(source, result);
	destroy(result); result = H; unchanged(&original); checks++;
	for(point = 1; point <= count; point++){
		allocations = 0; failat = point; raised = nil;
		assert(!kprotect(run, nil) && raised == exHeap && result == H);
		unchanged(&original); checks++;
	}
	failat = -1; destroy(source); source = H;
	setup(); source = tree(1);
	view = H2D(Array*, heapz(&Tarray));
	view->t = &pointer; pointer.ref++; view->len = 2;
	view->root = source; D2H(source)->ref++; view->data = source->data+sizeof(void*);
	result = arraycpy(view); verify(view, result); destroy(result); result = H;
	destroy(view); destroy(source); source = H; assert(live == 0); checks++;
	setup(); source = H2D(Array*, heaparray(&pointer, 0)); source->data = nil;
	operation = 0; assert(kprotect(run, nil)); verify(source, result);
	destroy(result); destroy(source); result = source = H; assert(live == 0); checks++;
	assert(arraycpy(H) == H); checks++;
	setup(); leaf.size = 0; source = H2D(Array*, heaparray(&leaf, 9));
	result = arraycpy(source); assert(result != source && result->len == 9);
	destroy(result); destroy(source); result = source = H; assert(live == 0); checks++;
}

/* Every published pointer in a partial MP must already be an acquired owner. */
static void
ownedmp(void)
{
	void **s, **d;
	int i;

	s = mp_source; d = mp_dest;
	for(i = 0; i < mp.size/sizeof(void*); i++){
		if((mp.map[i/8] & (0x80>>(i%8))) == 0 || d[i] == H)continue;
		if(D2H(s[i])->t == &Tarray)assert(d[i] != s[i]);
		else assert(d[i] == s[i] && D2H(s[i])->ref == 2);
	}
}

/* A failed later array clone must not release source pointers that were never acquired. */
static void
moduledata(void)
{
	Snapshot original;
	void **p;
	int count, point;

	setup(); operation = 1;
	mp_source = H2D(void*, heapz(&mp)); p = mp_source;
	p[0] = H2D(void*, heapz(&atom)); p[1] = tree(2);
	p[2] = H2D(void*, heapz(&atom)); p[3] = tree(1); save(&original);
	mp_dest = H2D(void*, heapz(&mp)); allocations = 0;
	assert(kprotect(run, nil)); count = allocations; ownedmp();
	verify(p[1], ((void**)mp_dest)[1]); verify(p[3], ((void**)mp_dest)[3]);
	destroy(mp_dest); mp_dest = H; unchanged(&original); checks++;
	for(point = 1; point <= count; point++){
		failat = -1; mp_dest = H2D(void*, heapz(&mp)); allocations = 0; failat = point;
		raised = nil; assert(!kprotect(run, nil)); assert(raised == exHeap);
		ownedmp(); destroy(mp_dest); mp_dest = H;
		unchanged(&original); checks++;
	}
	failat = -1; destroy(mp_source); mp_source = H; assert(live == 0);
}

/* Link errors and VM errors must both release the instance and its module owner. */
static void
instances(void)
{
	Snapshot original;
	void **p;
	int count, point;

	setup(); operation = 2; make_mp = 1; instance = H;
	mp_source = H2D(void*, heapz(&mp)); p = mp_source;
	p[0] = H2D(void*, heapz(&atom)); p[1] = tree(2);
	p[2] = H2D(void*, heapz(&atom)); p[3] = tree(1);
	memset(&module, 0, sizeof(module)); memset(exports, 0, sizeof(exports));
	memset(imports, 0, sizeof(imports));
	moduletypes[0] = &mp; module.type = moduletypes; module.ntype = 1;
	module.origmp = mp_source; module.ext = exports; module.name = "test";
	exports[0].name = "entry"; exports[0].sig = 17; exports[0].frame = &mp;
	imports[0].name = "entry"; imports[0].sig = 17;
	save(&original); module.ref = 2; allocations = 0;
	assert(kprotect(run, nil) && instance != H); count = allocations;
	verify(p[1], ((void**)instance->MP)[1]);
	destroy(instance); instance = H; unchanged(&original); assert(module.ref == 1); checks++;
	for(point = 1; point <= count; point++){
		module.ref = 2; allocations = 0; failat = point; raised = nil;
		assert(!kprotect(run, nil) && raised == exHeap && instance == H);
		unchanged(&original); assert(module.ref == 1 && Tmodlink.ref == 1); checks++;
	}
	failat = -1; module.ref = 2; imports[0].name = "missing";
	assert(kprotect(run, nil) && instance == H);
	unchanged(&original); assert(module.ref == 1); checks++;
	imports[0].name = "entry"; imports[0].sig++;
	module.ref = 2; assert(kprotect(run, nil) && instance == H);
	unchanged(&original); assert(module.ref == 1); checks++;
	imports[0].sig--; module.ref = 2; make_mp = 0;
	assert(kprotect(run, nil) && instance != H && instance->MP == H);
	destroy(instance); instance = H; unchanged(&original); assert(module.ref == 1); checks++;
	make_mp = 1; module.rt = DYNMOD;
	for(point = 0; point < 4; point++){
		module.ref = 2; allocations = 0; initcalls = endcalls = 0;
		failat = point == 1 || point == 2 ? point : -1; dynamic_error = point == 3;
		if(point == 0){
			assert(kprotect(run, nil) && instance != H);
			destroy(instance); instance = H;
		}else assert(!kprotect(run, nil) && instance == H);
		assert(initcalls == (point != 1) && endcalls == (point != 1));
		unchanged(&original); assert(module.ref == 1 && Tmodlink.ref == 1); checks++;
	}
	failat = -1; dynamic_error = 0; module.rt = 0; unload(&module);
	mp_source = H; assert(live == 0 && module.ref == 0);
}

/* Pointer-map byte transitions must advance source and destination together. */
static void
mapbytes(void)
{
	Snapshot original;
	void **p, **d;
	int count, point, i;

	setup(); operation = 1;
	mp.size = 16*sizeof(void*); mp.np = 2; mp.map[0] = 0x81; mp.map[1] = 0x90;
	mp_source = H2D(void*, heapz(&mp)); p = mp_source;
	p[0] = H2D(void*, heapz(&atom)); p[7] = tree(1);
	p[8] = H2D(void*, heapz(&atom)); p[11] = tree(0); save(&original);
	mp_dest = H2D(void*, heapz(&mp)); allocations = 0;
	assert(kprotect(run, nil)); count = allocations; ownedmp(); d = mp_dest;
	verify(p[7], d[7]); verify(p[11], d[11]);
	for(i = 0; i < 16; i++)
		if((mp.map[i/8] & (0x80>>(i%8))) == 0)assert(!memcmp(p+i, d+i, sizeof(void*)));
	destroy(mp_dest); mp_dest = H; unchanged(&original); checks++;
	for(point = 1; point <= count; point++){
		failat = -1; mp_dest = H2D(void*, heapz(&mp)); allocations = 0; failat = point;
		raised = nil; assert(!kprotect(run, nil) && raised == exHeap);
		ownedmp(); destroy(mp_dest); mp_dest = H; unchanged(&original); checks++;
	}
	failat = -1; destroy(mp_source); mp_source = H; assert(live == 0);
}

/* Isolate recursive copies and MP ownership failures for baseline comparisons. */
int
main(void)
{
	char *only;

	only = getenv("HEAP_CLONE_CASES");
	if(only == nil || !strcmp(only, "arrays"))arrays();
	if(only == nil || !strcmp(only, "mp"))moduledata();
	if(only == nil || !strcmp(only, "instances"))instances();
	if(only == nil || !strcmp(only, "maps"))mapbytes();
	assert(live == 0 && worker.nerr == 0 && Tarray.ref == 1);
	assert(leaf.ref == 1 && pointer.ref == 1 && record.ref == 1 && atom.ref == 1 && mp.ref == 1);
	printf("Heap/module clones: %d cases, WORD=%zu bytes\n", checks, sizeof(WORD));
	return 0;
}
