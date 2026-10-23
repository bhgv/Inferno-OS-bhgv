#include "limbo.h"
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>

char debug[256];
Tattr tattr[Tend];
Desc *descriptors;
Except *excs;
static int live, checks;
static Decl generateddecl;
static Sym generatedsym;

/* Explicit descriptor/map ownership tracking supplements sanitizer checks. */
int __lsan_is_turned_off(void) { return 1; }
void fatal(char *fmt, ...) { assert(fmt != nil); _Exit(73); }
void usedty(Type *t) { assert(t != nil); }
int ispoly(Decl *d) { (void)d; return 0; }
void addfnptrs(Decl *d, int n) { (void)d; (void)n; assert(0); }
int print(char *s, ...) { (void)s; return 0; }

/* Bound ordinary map allocations while retaining the production allocator convention. */
void*
allocmem(ulong size)
{
	void *p;

	assert(size > 0 && size < 1000000);
	p = malloc(size);
	assert(p != nil);
	live++;
	return p;
}

/* Interning must consume duplicate maps and release retained descriptors exactly once. */
static void
mapfree(void *p)
{
	assert(p != nil && live > 0);
	live--;
	free(p);
}

/* Type descriptor caching uses actual fields, with bounded declaration adapters. */
Decl* mkdecl(Src *src, int store, Type *t) { (void)src; assert(store == Dtype); memset(&generateddecl, 0, sizeof generateddecl); generateddecl.ty = t; return &generateddecl; }
Sym* enter(char *name, int token) { assert(strcmp(name, "_mktdesc_") == 0 && token == 0); return &generatedsym; }

/* Exception descriptor generation only requests positive, small alignment here. */
long
align(long offset, int alignment)
{
	assert(offset >= 0 && alignment > 0 && offset < 1000000);
	return ((offset+alignment-1)/alignment)*alignment;
}
#define free mapfree
IMPLEMENTATION
#undef free

/* Release the complete interned table between independent oracle cases. */
static void
releaseall(void)
{
	Desc *d, *next;

	for(d = descriptors; d != nil; d = next){
		next = d->next;
		mapfree(d->map);
		mapfree(d);
	}
	descriptors = nil;
	assert(live == 0);
}

/* Compute expected map bytes directly from independent pointer-slot indices. */
static void
checkmap(Decl *ids, long size, uchar *expected, int nmap)
{
	Desc *d, *again;

	d = mkdesc(size, ids);
	assert(d->size == size && d->nmap == nmap);
	assert(memcmp(d->map, expected, nmap) == 0);
	again = mkdesc(size, ids);
	assert(again == d && live == 2);
	releaseall();
	checks++;
}

/* Shuffle declarations independently of storage order, including nested aggregates. */
static void
permutations(void)
{
	Type pointer = {0}, nested = {0};
	Decl ids[4], inner[2];
	uchar expected[16];
	int a, b, c, d, order[4], i, slot, last, pass;
	int slots[] = {0, 7, 9, 25};

	pointer.kind = Tref;
	pointer.size = IBY2WD;
	for(pass = 0; pass < 2; pass++)
	for(a = 0; a < 4; a++)
	for(b = 0; b < 4; b++)
	for(c = 0; c < 4; c++)
	for(d = 0; d < 4; d++){
		if(a == b || a == c || a == d || b == c || b == d || c == d)
			continue;
		order[0] = a; order[1] = b; order[2] = c; order[3] = d;
		memset(ids, 0, sizeof ids);
		memset(expected, 0, sizeof expected);
		for(i = 0; i < 4; i++){
			ids[i].store = Dlocal;
			ids[i].ty = &pointer;
			ids[i].offset = slots[order[i]]*IBY2WD;
			if(i+1 < 4)
				ids[i].next = &ids[i+1];
		}
		last = 0;
		for(i = 0; i < 4; i++){
			slot = slots[i]+(pass ? 3 : 0);
			expected[slot/8] |= 0x80>>(slot%8);
			if(slot/8+1 > last)
				last = slot/8+1;
		}
		if(pass){
			memset(inner, 0, sizeof inner);
			memset(&nested, 0, sizeof nested);
			nested.kind = Ttuple;
			nested.ids = ids;
			nested.size = 26*IBY2WD;
			inner[0].store = Dlocal;
			inner[0].offset = 3*IBY2WD;
			inner[0].ty = &nested;
			checkmap(inner, 32*IBY2WD, expected, last);
			assert(nested.rec == 0);
		}else
			checkmap(ids, 32*IBY2WD, expected, last);
	}
}

/* Compare arbitrary declaration ordering and overlapping reused slots with a byte oracle. */
static void
randommaps(void)
{
	Type pointer = {0};
	Decl ids[24];
	uchar expected[16];
	unsigned state;
	int trial, i, count, slot, last;

	pointer.kind = Tref;
	pointer.size = IBY2WD;
	state = 0xdec5c;
	for(trial = 0; trial < 2000; trial++){
		memset(ids, 0, sizeof ids);
		memset(expected, 0, sizeof expected);
		state = state*1664525U+1013904223U;
		count = 1+(state>>16)%24;
		last = 0;
		for(i = 0; i < count; i++){
			state = state*1664525U+1013904223U;
			slot = (state>>16)%128;
			ids[i].offset = slot*IBY2WD;
			ids[i].store = Dlocal;
			ids[i].ty = &pointer;
			if(i+1 < count)
				ids[i].next = &ids[i+1];
			expected[slot/8] |= 0x80>>(slot%8);
			if(slot/8+1 > last)
				last = slot/8+1;
		}
		checkmap(ids, 128*IBY2WD, expected, last);
	}
}

/* Run the actual exception-cleanup builder, which reverses local declaration order. */
static void
exceptioncleanup(void)
{
	Type pointer = {0};
	Decl ids[2] = {0};
	Node names[2] = {0};
	Except handler = {0};
	uchar expected[4] = {0};

	pointer.kind = Tref;
	pointer.size = IBY2WD;
	ids[0].store = ids[1].store = Dlocal;
	ids[0].ty = ids[1].ty = &pointer;
	ids[0].offset = 0;
	ids[1].offset = 18*IBY2WD;
	ids[0].next = &ids[1];
	names[0].decl = &ids[0];
	names[1].decl = &ids[1];
	names[0].right = &names[1];
	handler.zn = names;
	excs = &handler;
	excdesc();
	expected[0] = 0x80;
	expected[2] = 0x20;
	assert(handler.desc != nil && handler.desc->nmap == 3);
	assert(memcmp(handler.desc->map, expected, 3) == 0);
	assert(ids[0].next == &ids[1] && ids[1].next == nil && handler.zn == nil);
	releaseall();
	excs = nil;
	checks++;
}

/* Check table-specific pointer locations and scalar gaps with an independent bitmap. */
static void
tables(void)
{
	Type type = {0};
	Case table = {0};
	Label labels[16];
	uchar map[16], expected[16];
	int n, i, slot, kind;
	long last, size;

	for(kind = 0; kind < 2; kind++)
	for(n = 0; n <= 16; n++){
		memset(map, 0, sizeof map);
		memset(expected, 0, sizeof expected);
		memset(labels, 0, sizeof labels);
		type.kind = kind ? Tcasec : Talt;
		type.cse = &table;
		table.nlab = n;
		table.labs = labels;
		last = -1;
		for(i = 0; i < n; i++){
			labels[i].isptr = i%3 != 0;
			if(kind){
				slot = 1+3*i;
				expected[slot/8] |= 0x80>>(slot%8);
				slot++;
				expected[slot/8] |= 0x80>>(slot%8);
				last = slot*IBY2WD;
			}else if(labels[i].isptr){
				slot = 2+2*i;
				expected[slot/8] |= 0x80>>(slot%8);
				last = slot*IBY2WD;
			}
		}
		size = ((kind ? 3 : 2)*n+2)*IBY2WD;
		assert(tdescmap(&type, map, 0, size) == last);
		assert(memcmp(map, expected, sizeof map) == 0);
		checks++;
	}
}

/* Invalid pointer slots must fail before the first out-of-capacity map access. */
static void
badmap(int mode)
{
	Type pointer = {0};
	Decl id = {0};
	uchar *map;
	long size, offset;

	pointer.kind = Tref;
	id.store = Dlocal;
	id.ty = &pointer;
	size = 8*IBY2WD;
	offset = mode == 0 ? size : mode == 1 ? -IBY2WD : mode == 2 ? 1 : 0;
	map = malloc(1);
	assert(map != nil);
	*map = 0;
	if(mode == 3)
		size = IBY2WD-1;
	if(mode == 4){
		id.offset = 0x7fffffff;
		(void)descmap(&id, map, IBY2WD, 0x7fffffff);
	}else
		(void)tdescmap(&pointer, map, offset, size);
	_Exit(74);
}

/* Isolate fatal map tests while retaining ASan bounds checking in each child. */
static void
bounds(void)
{
	pid_t child;
	int mode, status;

	for(mode = 0; mode < 5; mode++){
		child = fork();
		assert(child >= 0);
		if(child == 0)
			badmap(mode);
		assert(waitpid(child, &status, 0) == child);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
		checks++;
	}
}

/* Empty descriptors must intern and cache even when malloc(0) would return nil. */
static void
constructors(void)
{
	Type empty = {0}, pointer = {0};
	Desc *first, *cached;
	long sizes[] = {0, 1, 8*IBY2WD-1, 8*IBY2WD, 8*IBY2WD+1, 0x7fffffff};
	int i;

	for(i = 0; i < nelem(sizes); i++){
		assert(descbytes(sizes[i]) == sizes[i]/(8*IBY2WD)+(sizes[i]%(8*IBY2WD) != 0));
		checks++;
	}
	first = mkdesc(0, nil);
	assert(first->size == 0 && first->nmap == 0 && first->map != nil);
	assert(mkdesc(0, nil) == first && live == 2);
	empty.kind = Ttuple;
	empty.size = 0;
	cached = mktdesc(&empty);
	assert(cached == first && mktdesc(&empty) == first);
	releaseall();
	checks++;
	pointer.kind = Tref;
	pointer.size = IBY2WD;
	cached = mktdesc(&pointer);
	assert(cached->nmap == 1 && cached->map[0] == 0x80 && mktdesc(&pointer) == cached);
	releaseall();
	checks++;
}

/* Reject invalid descriptor headers before allocation or declaration creation. */
static void
badconstructor(int mode)
{
	Type type = {0};

	type.kind = Ttuple;
	type.size = -1;
	if(mode == 0)
		(void)mkdesc(-1, nil);
	else if(mode == 1)
		(void)mktdesc(&type);
	else if(mode == 2)
		(void)mktdesc(nil);
	else if(mode == 3)
		(void)descbytes(-1);
	else
		(void)mkdesc((long)0x7fffffff+1, nil);
	_Exit(74);
}

/* Keep rejected constructor state isolated and require the compiler's fatal convention. */
static void
constructorbounds(void)
{
	pid_t child;
	int i, status;

	for(i = 0; i < (sizeof(long) > 4 ? 5 : 4); i++){
		child = fork();
		assert(child >= 0);
		if(child == 0)
			badconstructor(i);
		assert(waitpid(child, &status, 0) == child);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
		checks++;
	}
}

/* Verify compiler map bytes, span, capacity and cleanup descriptor integration. */
int
main(int argc, char **argv)
{
	tattr[Tref].isptr = 1;
	if(argc == 1 || strcmp(argv[1], "permutations") == 0)
		permutations();
	if(argc == 1 || strcmp(argv[1], "random") == 0)
		randommaps();
	if(argc == 1 || strcmp(argv[1], "exception") == 0)
		exceptioncleanup();
	if(argc == 1 || strcmp(argv[1], "tables") == 0)
		tables();
	if(argc == 1 || strcmp(argv[1], "bounds") == 0)
		bounds();
	if(argc == 1 || strcmp(argv[1], "constructors") == 0)
		constructors();
	if(argc == 1 || strcmp(argv[1], "constructor-bounds") == 0)
		constructorbounds();
	printf("compiler maps word=%d: %d bitmap/span/capacity cases passed\n", IBY2WD, checks);
	return 0;
}
