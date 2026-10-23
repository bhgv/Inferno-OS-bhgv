#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include "dynld.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>

RUNTAB_TYPE
static Proc worker;
static Module baseline;
Module *modules = &baseline;
static void *owned[256];
static int live, fail_after = -1, attempts, checks, imported, object_live, object_frees;
static int import_failure, object_failure;
static char *expected_import;
static Dynobj object;
static char *native_name = "$Native";
static char diagnostic[128];
char exNomem[] = "out of memory";
static Runtab table[4];

/* Exercise the actual VM error-frame helper with the fixture process. */
Proc*
getup(void)
{
	return &worker;
}

/* Preserve the normal pop-on-error contract used by kprotect. */
void
nexterror(void)
{
	assert(worker.nerr > 0);
	siglongjmp(worker.estack[--worker.nerr], 1);
}

/* Store an observable error before returning through the real error frame. */
void
error(char *message)
{
	snprintf(diagnostic, sizeof(diagnostic), "%s", message);
	nexterror();
}

/* Keep diagnostic formatting independent of the native formatter implementation. */
void
kwerrstr(char *message, ...)
{
	snprintf(diagnostic, sizeof(diagnostic), "%s", message);
}

/* Track every allocation and verify only completed modules can become visible. */
static void*
allocate(size_t size)
{
	void *p;
	int i, count;
	Module *visible;

	count = 0;
	for(visible = modules; visible != &baseline; visible = visible->link){
		assert(visible != nil && visible->ext != nil && ++count < 16);
		for(i = 0; i < visible->ntype; i++)
			assert(visible->type[i] != nil && visible->ext[i].name != nil);
	}
	attempts++;
	if(fail_after == 0)
		return nil;
	if(fail_after > 0)
		fail_after--;
	assert(size < 1048576);
	/* Inferno malloc zeroes its storage; retain that actual allocator contract. */
	p = calloc(1, size ? size : 1);
	assert(p != nil);
	for(i = 0; i < nelem(owned); i++)
		if(owned[i] == nil){
			owned[i] = p;
			live++;
			return p;
		}
	abort();
}

/* Reject borrowed/static frees and make ownership failures explicit. */
static void
releaseowned(void *p)
{
	int i;

	if(p == nil)
		return;
	for(i = 0; i < nelem(owned); i++)
		if(owned[i] == p){
			owned[i] = nil;
			live--;
			free(p);
			return;
		}
	fprintf(stderr, "free of unowned pointer %p\n", p);
	abort();
}

/* Supply owned copies while exposing each name/path allocation to fault injection. */
static char*
duplicate(char *s)
{
	char *p;
	size_t size;

	assert(s != nil);
	size = strlen(s)+1;
	p = allocate(size);
	if(p != nil)
		memmove(p, s, size);
	return p;
}

/* Match the explicit zeroing allocator used by module and descriptor construction. */
static void*
zeroallocate(ulong size, int clear)
{
	(void)clear;
	return allocate(size);
}

/* Frame descriptors reference the real VM marker and destructor symbols. */
void
markheap(Type *t, void *p)
{
	(void)t;
	(void)p;
}

/* The fixture never allocates a live interpreter heap object. */
void
freeheap(Heap *h, int swept)
{
	(void)h;
	(void)swept;
}

/* Module teardown may destroy only its empty initial data segment. */
void
destroy(void *p)
{
	assert(p == H);
}

/* Native exports are installed but never executed by these construction tests. */
static void
native(void *p)
{
	(void)p;
	abort();
}

/* Model a dynamically loaded image independently of constructor allocations. */
Dynobj*
dynld(int fd)
{
	assert(fd == 42 && !object_live);
	if(object_failure)
		return nil;
	object_live = 1;
	return &object;
}

/* Resolve only the two required exports, with deterministic failure points. */
void*
dynimport(Dynobj *o, char *name, ulong signature)
{
	(void)signature;
	assert(o == &object && object_live);
	imported++;
	if(import_failure == imported)
		return nil;
	if(expected_import != nil){
		assert(strcmp(name, expected_import) == 0);
		return table;
	}
	if(strcmp(name, "XXXmodule") == 0)
		return &native_name;
	assert(strcmp(name, "$Nativemodtab") == 0);
	return table;
}

/* Every acquired image must be released exactly once after the module stops owning it. */
void
dynobjfree(Dynobj *o)
{
	assert(o == &object && object_live);
	object_live = 0;
	object_frees++;
}

/* Dynamic loader debug messages do not affect allocation cutoffs. */
int
print(char *format, ...)
{
	(void)format;
	return 0;
}

/* Dynamic import names are bounded by the production helper before formatting. */
int
snprint(char *to, int size, char *format, ...)
{
	va_list args;
	int n;

	va_start(args, format);
	n = vsnprintf(to, size, format, args);
	va_end(args);
	return n;
}

/* Dynamic diagnostics are adapter state, not an additional allocation. */
char*
enverror(void)
{
	return "dynamic fixture error";
}

#define malloc(n) allocate(n)
#define mallocz(n,z) zeroallocate(n,z)
#define free(p) releaseowned(p)
#define strdup(s) duplicate(s)
#define DBG if(0) print
#define signof(t) 0
IMPLEMENTATION

/* Build a small ABI-real table containing scalar and pointer-bearing frames. */
static void
resettable(void)
{
	memset(table, 0, sizeof(table));
	table[0].name = "first";
	table[1].name = "second";
	table[2].name = "third";
	table[0].size = 0;
	table[1].size = sizeof(Frame);
	table[2].size = 8*sizeof(void*);
	table[2].np = 1;
	table[2].map[0] = 0x80;
	table[0].fn = table[1].fn = table[2].fn = native;
	table[0].sig = 1;
	table[1].sig = 0xffffffffUL;
	table[2].sig = 0x80000000UL;
	imported = 0;
	import_failure = object_failure = 0;
	native_name = "$Native";
	expected_import = nil;
}

/* Assert that any failed attempt left the existing module and error stack intact. */
static void
clean(void)
{
	assert(modules == &baseline && baseline.ref == 7 && baseline.link == nil);
	assert(live == 0 && worker.nerr == 0 && !object_live);
	checks++;
}

/* Check complete frame ownership and both module references through teardown. */
static void
builtin(void)
{
	Module *m, *looked;
	char name[] = "$Mutable";
	int cutoff, failures = 0, successes = 0, i, np;
	Runtab empty[1] = {{0}};

	for(cutoff = 0; cutoff < 20; cutoff++){
		resettable();
		fail_after = cutoff;
		m = builtinmod(name, table, 3);
		fail_after = -1;
		if(m == nil){
			failures++;
			clean();
			continue;
		}
		successes++;
		assert(modules == m && m->ntype == 3 && m->ref == 1);
		assert(m->name != name && m->path != name && m->name != m->path);
		assert(strcmp(m->name, name) == 0 && strcmp(m->path, name) == 0);
		for(i = 0; i < 3; i++){
			assert(m->type[i] == m->ext[i].frame && m->type[i]->ref == 1);
			assert(m->ext[i].u.runt == native && strcmp(m->ext[i].name, table[i].name) == 0);
			assert(m->ext[i].sig == (int)(u32int)table[i].sig);
		}
		assert(m->ext[3].name == nil && m->prog == nil && m->jit == nil);
		looked = lookmod(name);
		assert(looked == m && m->ref == 2);
		unload(looked);
		assert(m->ref == 1 && modules == m);
		unload(m);
		clean();
	}
	assert(failures > 8 && successes > 0);
	resettable();
	m = builtinmod(name, table, 0);
	assert(m != nil && m->ntype == 3);
	unload(m);
	clean();
	m = builtinmod(name, empty, 0);
	assert(m != nil && m->ntype == 0 && m->ext[0].name == nil);
	unload(m);
	clean();
	assert(builtinmod(nil, table, 0) == nil);
	assert(builtinmod(name, nil, 0) == nil);
	assert(builtinmod(name, table, -1) == nil);
	assert(builtinmod(name, table, INT_MAX) == nil);
	assert(builtinmod(name, table, 1) == nil);
	assert(builtinmod(name, table, 4) == nil);
	clean();
	resettable(); table[1].np = 17; assert(builtinmod(name, table, 3) == nil); clean();
	resettable(); table[1].np = -1; assert(builtinmod(name, table, 3) == nil); clean();
	resettable(); table[1].size = -1; assert(builtinmod(name, table, 3) == nil); clean();
	resettable(); table[1].fn = nil; assert(builtinmod(name, table, 3) == nil); clean();
	resettable(); table[1].np = 1; table[1].map[0] = 1;
	assert(builtinmod(name, table, 3) == nil); clean();
	for(np = 15; np <= 16; np++){
		resettable();
		table[1].size = 16*8*sizeof(void*);
		table[1].np = np;
		memset(table[1].map, 0x80, np);
		m = builtinmod(name, table, 3);
		assert(m != nil && m->type[1]->np == np);
		unload(m);
		clean();
	}
}

/* Preserve a populated list through failed construction and middle-node unloads. */
static void
multiple(void)
{
	Module *one, *two, *partial;
	int cutoff, held;
	char mutable[] = "$Mutable";

	resettable();
	one = builtinmod("$One", table, 0);
	two = builtinmod("$Two", table, 0);
	assert(one != nil && two != nil && modules == two && two->link == one);
	held = live;
	for(cutoff = 0; cutoff < 13; cutoff++){
		fail_after = cutoff;
		partial = builtinmod("$Temporary", table, 3);
		fail_after = -1;
		if(partial != nil)
			unload(partial);
		assert(live == held && modules == two && two->link == one && one->link == &baseline);
		assert(worker.nerr == 0);
		checks++;
	}
	partial = newmod(mutable);
	mutable[1] = 'X';
	assert(modules == two && strcmp(partial->path, "$Mutable") == 0);
	assert(strcmp(partial->name, "$Mutable") == 0 && partial->ext == nil);
	freemod(partial);
	assert(live == held);
	unload(one);
	assert(modules == two && two->link == &baseline);
	unload(two);
	clean();
}

/* Check exact import-name capacity without asking the object to resolve rejected names. */
static void
names(void)
{
	char limit[65];
	int before;

	resettable();
	assert(dynld(42) == &object);
	memset(limit, 'n', sizeof(limit)-1);
	limit[63] = 0;
	expected_import = limit;
	assert(addr(limit, "", &object, 0) == table);
	assert(addr("", limit, &object, 0) == table);
	before = imported;
	assert(addr(limit, "x", &object, 0) == nil);
	limit[63] = 'n'; limit[64] = 0;
	assert(addr(limit, "", &object, 0) == nil);
	assert(addr(nil, "x", &object, 0) == nil);
	assert(addr("x", nil, &object, 0) == nil);
	assert(addr("x", "y", nil, 0) == nil && imported == before);
	dynobjfree(&object);
	clean();
}

/* Test dynamic image ownership, export failures and the final path allocation. */
static void
dynamic(void)
{
	Dir dir = {0};
	Module *m;
	char longname[128];
	int cutoff, before, i, failures = 0, successes = 0;

	dir.dev = 123;
	dir.type = 4;
	dir.mtime = 567;
	for(cutoff = 0; cutoff < 22; cutoff++){
		resettable(); before = object_frees;
		fail_after = cutoff;
		m = newdyncode(42, "/dynamic/native", &dir);
		fail_after = -1;
		if(m == nil)
			failures++;
		else{
			successes++;
			assert(m->rt == DYNMOD && m->dlm == &object && object_live);
			assert(m->dev == dir.dev && m->mtime == dir.mtime);
			assert(strcmp(m->path, "/dynamic/native") == 0);
			assert(strcmp(m->name, "$Native") == 0 && m->ntype == 3);
			unload(m);
		}
		assert(object_frees == before+1);
		clean();
	}
	assert(failures > 9 && successes > 0);
	for(i = 1; i <= 2; i++){
		resettable(); import_failure = i; before = object_frees;
		assert(newdyncode(42, "/dynamic/native", &dir) == nil);
		assert(object_frees == before+1); clean();
	}
	resettable(); object_failure = 1; before = object_frees;
	assert(newdyncode(42, "/dynamic/native", &dir) == nil);
	assert(object_frees == before); clean();
	resettable(); native_name = nil;
	assert(newdyncode(42, "/dynamic/native", &dir) == nil); clean();
	resettable(); memset(longname, 'x', sizeof(longname)-1); longname[sizeof(longname)-1] = 0;
	native_name = longname;
	assert(newdyncode(42, "/dynamic/native", &dir) == nil); clean();
	/* An absent export table may be cleaned twice without touching borrowed storage. */
	{ Module empty = {0}; destroylinks(&empty); destroylinks(&empty); }
}

/* Run constructors using actual VM type maps, link ownership and error frames. */
int
main(void)
{
	baseline.ref = 7;
	baseline.path = "$Baseline";
	if(getenv("BUILTIN_BAD_COUNT") != nil){
		resettable();
		assert(builtinmod("$Native", table, 1) == nil);
		clean();
		return 0;
	}
	builtin();
	multiple();
	names();
	dynamic();
	printf("module construction: %d cleanup checks, allocation cutoffs, bounds and dynamic ownership passed\n", checks);
	return 0;
}
