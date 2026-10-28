#include "limbo.h"
#include <stdio.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define check(test) predicate(!!(test), __LINE__) // Preserve every oracle under NDEBUG.
#define WalkDepth 100000 // Deep valid value nesting must not consume native recursion.
#define WalkSlots 256 // Complete shallow descriptor extent measured in pointer slots.

char debug[256]; // Keep production debug conversions inactive in this isolated fixture.
Tattr tattr[Tend]; // Pointer classification table for fixture Types.
static Type types[2048], savedtypes[2048]; // Borrowed shallow Types and their mutation oracle.
static Decl decls[2048], saveddecls[2048], alias; // Shallow fields and a skipped alias identity.
static int ntypes, ndecls; // Current shallow arena cursors.
static uchar expected[WalkSlots/8]; // Independently recorded pointer leaf positions.
static long highest; // Highest pointer leaf selected while constructing the tree.
static uint randomstate = 0x721924; // Deterministic construction independent of production traversal.
static ulong checks, outstanding, allocations; // Scenarios and private continuation ownership.


/* Report the exact oracle or ownership failure without assert. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "type walk failure at %d\n", line);
		abort();
	}
}


/* Retain address instrumentation without unavailable LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Fatal probes must exit through the production diagnostic, never a sanitizer crash. */
void
fatal(char *message, ...)
{
	if(message == nil)
		abort();
	_Exit(73);
}


/* Debug output would require unrelated compiler converters and is intentionally disabled. */
int
print(char *format, ...)
{
	USED(format);
	abort();
	return 0;
}


/* Bound and count only production continuation allocations. */
void*
allocmem(ulong bytes)
{
	void *p;

	check(bytes > 0 && bytes < 128*1024*1024);
	p = malloc(bytes);
	check(p != nil);
	outstanding++;
	allocations++;
	return p;
}


/* Check release on complete traversal and on reference short-circuit exits. */
static void
walkfree(void *p)
{
	check(p != nil && outstanding != 0);
	outstanding--;
	free(p);
}

#define free walkfree // Observe only production work-stack releases.
IMPLEMENTATION
#undef free


/* Supply reproducible structure choices without libc random state. */
static uint
nextvalue(void)
{
	randomstate = randomstate*1664525U+1013904223U;
	return randomstate;
}


/* Construct disjoint nested regions while recording pointer leaves independently. */
static Type*
makevalue(int depth, int base, int slots, int active, int *zero)
{
	Type *t, *pick;
	Decl *d, **tail;
	uint choice;
	int i, childzero, store, selected;

	check(ntypes < nelem(types));
	t = &types[ntypes++];
	memset(t, 0, sizeof(*t));
	choice = nextvalue();
	*zero = 0;
	if(depth == 0 || choice%5 == 0){
		t->kind = (int[]){Tint, Tstring, Tref, Tmodule}[choice%4];
		t->size = IBY2WD;
		t->rec = 7;
		*zero = tattr[t->kind].isptr != 0;
		if(active && *zero){
			expected[base/8] |= 0x80>>(base%8);
			if(base*IBY2WD > highest)
				highest = base*IBY2WD;
		}
		return t;
	}
	t->kind = choice&1 ? Ttuple : Tadt;
	t->size = slots*IBY2WD;
	tail = &t->ids;
	for(i = 0; i < 4; i++){
		check(ndecls < nelem(decls));
		d = &decls[ndecls++];
		memset(d, 0, sizeof(*d));
		*tail = d;
		tail = &d->next;
		store = nextvalue()%5;
		d->store = (int[]){Dfield, Dfield, Dconst, Dfn, Dlocal}[store];
		if(store == 4)
			d->link = &alias;
		d->offset = i*(slots/4)*IBY2WD;
		selected = store < 2;
		d->ty = makevalue(depth-1, base+i*(slots/4), slots/4, active && selected, &childzero);
		*zero |= childzero;
	}
	if(choice&8){
		check(ntypes < nelem(types));
		pick = &types[ntypes++];
		memset(pick, 0, sizeof(*pick));
		pick->kind = Tadtpick;
		pick->tof = t;
		pick->size = t->size;
		t = pick;
	}
	return t;
}


/* Verify exact maps, maximum offsets, query answers and unchanged borrowed graphs. */
static void
shallow(void)
{
	uchar guarded[WalkSlots/8+2];
	Type *t, *body;
	Decl root;
	int i, zero;
	long result;

	for(i = 0; i < 2000; i++){
		ntypes = ndecls = 0;
		memset(expected, 0, sizeof(expected));
		highest = -1;
		t = makevalue(4, 0, WalkSlots, 1, &zero);
		memmove(savedtypes, types, ntypes*sizeof(*types));
		memmove(saveddecls, decls, ndecls*sizeof(*decls));
		memset(guarded, 0, sizeof(guarded));
		guarded[0] = guarded[sizeof(guarded)-1] = 0xa5;
		result = tdescmap(t, guarded+1, 0, WalkSlots*IBY2WD);
		check(result == highest && !memcmp(guarded+1, expected, sizeof(expected)));
		check(guarded[0] == 0xa5 && guarded[sizeof(guarded)-1] == 0xa5);
		check(tmustzero(t) == zero && outstanding == 0);
		memset(&root, 0, sizeof(root));
		root.store = Dfield;
		root.ty = t;
		memset(guarded+1, 0, sizeof(expected));
		check(descmap(&root, guarded+1, 0, WalkSlots*IBY2WD) == highest);
		check(!memcmp(guarded+1, expected, sizeof(expected)) && mustzero(&root) == zero);
		check(!memcmp(savedtypes, types, ntypes*sizeof(*types)));
		check(!memcmp(saveddecls, decls, ndecls*sizeof(*decls)));
		body = t->kind == Tadtpick ? t->tof : t;
		if(body->kind == Tadt || body->kind == Ttuple)
			check(mustzero(body->ids) == zero);
		checks++;
	}
}


/* The same aggregate may be revisited at another offset after its active mark clears. */
static void
sharedvalue(void)
{
	Type outer, inner, pointer, scalar;
	Decl fields[4];
	uchar map;

	memset(&outer, 0, sizeof(outer));
	memset(&inner, 0, sizeof(inner));
	memset(&pointer, 0, sizeof(pointer));
	memset(&scalar, 0, sizeof(scalar));
	memset(fields, 0, sizeof(fields));
	outer.kind = inner.kind = Ttuple;
	pointer.kind = Tstring;
	scalar.kind = Tint;
	outer.ids = &fields[0];
	inner.ids = &fields[2];
	fields[0].ty = fields[1].ty = &inner;
	fields[0].next = &fields[1];
	fields[1].offset = 2*IBY2WD;
	fields[2].ty = &pointer;
	fields[2].offset = IBY2WD;
	fields[2].next = &fields[3];
	fields[3].ty = &scalar;
	fields[0].store = fields[1].store = fields[2].store = fields[3].store = Dfield;
	map = 0;
	check(tdescmap(&outer, &map, 0, 4*IBY2WD) == 3*IBY2WD && map == 0x50);
	check(outer.rec == 0 && inner.rec == 0);
	check(tmustzero(&outer) == 1 && outstanding == 0);
	checks++;
}


/* Stress valid value nesting, deferred siblings and early reference exits under 512 KiB. */
static void
deep(int query)
{
	Type *chain, scalar, pointer, empty;
	Decl *fields, first;
	struct rlimit limit;
	uchar map;
	long result;
	int i, kind;

	limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	chain = calloc(WalkDepth, sizeof(*chain));
	fields = calloc(2*WalkDepth, sizeof(*fields));
	check(chain != nil && fields != nil);
	memset(&scalar, 0, sizeof(scalar));
	memset(&pointer, 0, sizeof(pointer));
	memset(&empty, 0, sizeof(empty));
	memset(&first, 0, sizeof(first));
	scalar.kind = Tint;
	scalar.size = IBY2WD;
	pointer.kind = Tstring;
	pointer.size = IBY2WD;
	empty.kind = Ttuple;
	first.store = Dfield;
	first.ty = chain;
	for(kind = 0; kind < 2; kind++){
		for(i = 0; i < WalkDepth; i++){
			chain[i].kind = i&1 ? Tadt : Ttuple;
			chain[i].size = IBY2WD;
			chain[i].ids = &fields[2*i];
			fields[2*i].store = fields[2*i+1].store = Dfield;
			fields[2*i].ty = i+1 < WalkDepth ? &chain[i+1] : kind ? &pointer : &scalar;
			fields[2*i].next = &fields[2*i+1];
			fields[2*i+1].ty = &empty;
			fields[2*i+1].offset = IBY2WD;
		}
		if(query){
			check(tmustzero(chain) == kind && mustzero(&first) == kind);
		}else{
			map = 0;
			result = tdescmap(chain, &map, 0, IBY2WD);
			check(result == (kind ? 0 : -1) && map == (kind ? 0x80 : 0));
			map = 0;
			check(descmap(&first, &map, 0, IBY2WD) == result);
			check(map == (kind ? 0x80 : 0));
		}
		for(i = 0; i < WalkDepth; i++)
			check(chain[i].rec == 0);
		check(empty.rec == 0 && outstanding == 0 && allocations > 0);
		checks += 2;
	}
	/* A later declaration must remain unread after a true pointer result. */
	first.ty = &pointer;
	first.next = (Decl*)1;
	check(mustzero(&first) == 1 && outstanding == 0);
	checks++;
	free(chain);
	free(fields);
}


/* Distinguish expected fatal validation from memory errors on malformed metadata. */
static void
rejects(void)
{
	Type t, pointer;
	Decl d;
	uchar map;
	int kind, status;
	pid_t child;

	for(kind = 0; kind < 8; kind++){
		child = fork();
		check(child >= 0);
		if(child == 0){
			memset(&t, 0, sizeof(t));
			memset(&d, 0, sizeof(d));
			memset(&pointer, 0, sizeof(pointer));
			t.kind = Ttuple;
			t.ids = &d;
			d.store = Dfield;
			d.ty = &t;
			pointer.kind = Tstring;
			if(kind == 0)
				tdescmap(&t, &map, 0, IBY2WD);
			else if(kind == 1){
				t.rec = 1;
				tdescmap(&t, &map, 0, IBY2WD);
			}else if(kind == 2)
				tdescmap(&pointer, &map, 1, IBY2WD);
			else if(kind == 3)
				tdescmap(&pointer, &map, 0, 0);
			else if(kind == 4)
				descmap(nil, &map, -1, IBY2WD);
			else if(kind == 5){
				d.ty = &pointer;
				d.offset = LONG_MAX;
				descmap(&d, &map, 1, IBY2WD);
			}else{
				t.kind = Tend;
				if(kind == 6)
					tdescmap(&t, &map, 0, IBY2WD);
				else
					tmustzero(&t);
			}
			_Exit(0);
		}
		check(waitpid(child, &status, 0) == child);
		check(WIFEXITED(status) && WEXITSTATUS(status) == 73);
		checks++;
	}
	check(tmustzero(nil) == 0 && mustzero(nil) == 0);
	checks++;
}


/* Run each probe independently so parent-source stack failures remain attributable. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	tattr[Tstring].isptr = tattr[Tref].isptr = tattr[Tmodule].isptr = 1;
	if(!strcmp(argv[1], "shallow")){
		shallow();
		sharedvalue();
	}
	else if(!strcmp(argv[1], "map"))
		deep(0);
	else if(!strcmp(argv[1], "zero"))
		deep(1);
	else
		rejects();
	check(outstanding == 0);
	printf("PASS Type walks: %lu %s scenarios, pointer slots %d\n", checks, argv[1], IBY2WD);
	return 0;
}
