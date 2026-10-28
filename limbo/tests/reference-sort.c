#include "limbo.h"
#include <stdio.h>
#include <setjmp.h>
#include <sys/resource.h>

#define check(c) predicate(!!(c), __LINE__) // Keep checks active under NDEBUG.

char debug[256]; // Disable unrelated debug-print paths.
char *kindname[Tend]; // Debug names are not consulted with debug output disabled.
int superwarn, scope; // Actual warning/scope inputs for production propagation.
Type *tstring; // Builtin identity used for constant-string propagation.
static Decl *scopes[MaxScope]; // Real scope head storage for popscope.
static Decl ids[16]; // Borrowed declarations and interface/import destinations.
static Type types[4]; // Borrowed ADT/module/scalar Type metadata.
static Sym symbols[16]; // Borrowed named symbols and restored bindings.
static Node initnodes[4]; // String initializers and function definitions.
static uvlong trace; // Ordered diagnostic source/message identities.
static int expectfatal; // Only explicit invalid count vectors expect rejection.
static jmp_buf rejection; // Return from the expected compiler fatal.
static ulong scenarios; // Completed comparisons and propagation vectors.
static uint randomstate = 7319; // Reproducible grouped-list generator state.
static void freeloc(Decl*); // Production scope cleanup helper defined in the extracted source.


/* Fail on the first ordering, arithmetic, metadata or scope-state discrepancy. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "reference sort check at %d\n", line);
		abort();
	}
}


/* Retain address instrumentation without process-wide leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Catch explicit arithmetic rejection; fail all unexpected compiler fatals. */
void
fatal(char *message, ...)
{
	if(expectfatal){
		check(strcmp(message, "declaration reference count overflow") == 0);
		longjmp(rejection, 1);
	}
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Fingerprint ordered diagnostics without requiring the separate compiler formatter. */
static void
diagnostic(int kind, Line line, char *message)
{
	trace = trace*1099511628211ULL+kind;
	trace = trace*1099511628211ULL+line.line;
	for(; *message; message++)
		trace = trace*1099511628211ULL+(uchar)*message;
}


/* Preserve the sequence and source position of undefined-member diagnostics. */
void
error(Line line, char *message, ...)
{
	diagnostic(1, line, message);
}


/* Preserve warning enablement, message selection and declaration order. */
void
warn(Line line, char *message, ...)
{
	diagnostic(2, line, message);
}


/* Any streamed debug output means the fixture accidentally enabled another path. */
int
print(char *message, ...)
{
	fatal(message);
	return -1;
}

IMPLEMENTATION

REFERENCE


/* Restore a clean borrowed graph before each vector or differential attempt. */
static void
reset(void)
{
	int i;

	memset(ids, 0, sizeof(ids));
	memset(types, 0, sizeof(types));
	memset(symbols, 0, sizeof(symbols));
	memset(initnodes, 0, sizeof(initnodes));
	memset(scopes, 0, sizeof(scopes));
	for(i = 0; i < nelem(ids); i++){
		ids[i].sym = &symbols[i];
		ids[i].ty = &types[0];
		ids[i].src.start.line = i+1;
		symbols[i].name = i%2 ? "name" : ".hidden";
		symbols[i].decl = &ids[i];
		ids[i].store = Dglobal;
	}
	types[0].kind = Tint;
	types[1].kind = Tadt;
	types[2].kind = Tmodule;
	types[3].kind = Tstring;
	tstring = &types[3];
	scope = ScopeGlobal+1;
	superwarn = 1;
	trace = 1469598103934665603ULL;
}


/* Exercise the real destination/aggregation path selected by a Python integer oracle. */
static void
vector(int action, int a, int b, int valid, int expected)
{
	Decl *destination;

	reset();
	destination = &ids[0];
	ids[0].store = Dtype;
	ids[0].ty = &types[1];
	ids[0].refs = a;
	types[1].ids = &ids[1];
	ids[1].refs = b;
	switch(action){
	case 0: break;
	case 1: ids[0].ty = &types[2]; types[2].ids = &ids[1]; break;
	case 2:
		ids[0].ty = &types[2]; types[2].ids = &ids[1];
		ids[0].refs = 0; ids[1].iface = &ids[2]; ids[2].refs = a;
		destination = &ids[2];
		break;
	case 3:
	case 4:
		ids[0].ty = &types[2]; types[2].ids = &ids[1];
		ids[1].store = Dtype; ids[1].ty = &types[1];
		types[1].ids = &ids[2]; ids[1].refs = 0; ids[2].refs = b;
		if(action == 4){
			ids[0].refs = 0; ids[2].iface = &ids[3]; ids[3].refs = a;
			destination = &ids[3];
		}
		break;
	case 5:
		ids[0].store = Dconst; ids[0].ty = tstring; ids[0].refs = b;
		ids[0].init = &initnodes[0]; initnodes[0].decl = &ids[1]; ids[1].refs = a;
		destination = &ids[1];
		break;
	case 6:
		ids[0].store = Dlocal; ids[0].das = 1;
		break;
	case 7: ids[0].das = 1; break;
	case 8:
		ids[0].store = Dlocal; ids[0].refs = b; ids[0].importid = &ids[1];
		ids[1].refs = a; ids[0].old = &ids[2];
		ids[0].link = &ids[3]; ids[3].tref = 1;
		scopes[scope] = &ids[0]; destination = &ids[1];
		break;
	default: check(0);
	}
	expectfatal = !valid;
	if(setjmp(rejection) == 0){
		if(action == 8){
			check(popscope() == &ids[0]);
			check(scope == ScopeGlobal && symbols[0].decl == &ids[2]);
			check(ids[0].old == nil && ids[3].tref == 0);
		}else
			checkrefs(&ids[0]);
		check(valid && destination->refs == expected);
	}else{
		check(!valid);
		if(action != 7)
			check(destination->refs == a);
	}
	expectfatal = 0;
	scenarios++;
}


/* Read exact integer-oracle results instead of reproducing production arithmetic here. */
static void
vectors(char *path)
{
	FILE *input;
	int action, a, b, valid, expected, count;

	input = fopen(path, "r");
	check(input != nil);
	while((count = fscanf(input, "%d %d %d %d %d", &action, &a, &b, &valid, &expected)) == 5)
		vector(action, a, b, valid, expected);
	check(count == EOF);
	fclose(input);
}


/* Compare actual pair merging with independent lexicographic priorities from Python. */
static void
pairs(char *path)
{
	FILE *input;
	Type et, ft;
	Decl e, f, dummy, *result;
	Sym en, fn;
	long es, fs;
	int er, fr, links, names, expected, count;
	static char *enames[] = { "same", "a", "same", "z" };
	static char *fnames[] = { "z", "same", "same", "a" };

	input = fopen(path, "r");
	check(input != nil);
	while((count = fscanf(input, "%ld %ld %d %d %d %d %d", &es, &fs, &er, &fr, &links, &names, &expected)) == 7){
		memset(&e, 0, sizeof(e)); memset(&f, 0, sizeof(f));
		memset(&et, 0, sizeof(et)); memset(&ft, 0, sizeof(ft));
		memset(&en, 0, sizeof(en)); memset(&fn, 0, sizeof(fn));
		et.size = es; ft.size = fs;
		e.ty = &et; f.ty = &ft; e.sym = &en; f.sym = &fn;
		e.refs = er; f.refs = fr; e.nid = f.nid = 1;
		e.link = links&1 ? &dummy : nil; f.link = links&2 ? &dummy : nil;
		en.name = enames[names]; fn.name = fnames[names];
		result = declmerge(&e, &f);
		check(result == (expected ? &e : &f));
		check(result->next == (expected ? &f : &e) && result->next->next == nil);
		check(e.refs == er && f.refs == fr && et.size == es && ft.size == fs);
		scenarios++;
	}
	check(count == EOF);
	fclose(input);
}


/* Compare every propagated count, restored binding and ordered diagnostic trace. */
static uvlong
ordinary(int seed, int original)
{
	uvlong hash;
	int i;

	reset();
	for(i = 0; i < nelem(ids); i++){
		ids[i].refs = (seed*13+i*7)%31;
		ids[i].das = (seed+i)%3 == 0;
		ids[i].store = (seed+i)%Dend;
		ids[i].next = i == nelem(ids)-1 ? nil : &ids[i+1];
		if((seed+i)%4 == 0)
			ids[i].init = &initnodes[0];
	}
	initnodes[0].decl = &ids[15];
	ids[15].next = nil;
	ids[0].store = Dtype; ids[0].ty = &types[seed%2+1];
	types[1].ids = &ids[10]; types[2].ids = &ids[8];
	for(i = 8; i < 14; i++)
		ids[i].iface = &ids[14+i%2];
	ids[8].store = Dtype; ids[8].ty = &types[1];
	/* The aggregate member chains are borrowed; avoid traversing them again as roots. */
	ids[7].next = nil;
	ids[12].store = Dfn;
	ids[13].ty = tstring;
	ids[13].store = Dconst;
	superwarn = seed&1;
	if(original)
		reference_checkrefs(ids);
	else
		checkrefs(ids);
	/* Also check actual import/symbol/type cleanup with an already initialized scope. */
	ids[4].store = Dtype; ids[4].ty = &types[1]; types[1].decl = &ids[5];
	ids[5].timport = &ids[4]; ids[4].timport = &ids[6];
	ids[4].importid = &ids[15]; ids[4].old = &ids[9]; ids[4].next = nil;
	scopes[scope] = &ids[4];
	if(original)
		reference_popscope();
	else
		popscope();
	hash = trace;
	for(i = 0; i < nelem(ids); i++){
		hash = hash*1099511628211ULL+(uint)ids[i].refs;
		hash = hash*1099511628211ULL+(ids[i].old == nil ? 0 : ids[i].old-ids+1);
		hash = hash*1099511628211ULL+(ids[i].timport == nil ? 0 : ids[i].timport-ids+1);
		hash = hash*1099511628211ULL+(symbols[i].decl-symbols[0].decl);
	}
	return hash*1099511628211ULL+scope;
}


/* Preserve stable whole-group order and every continuation node after local sorting. */
static void
groups(int seed, int count)
{
	Decl *list, *copy, *a, *b, *dummy;
	Type ts[6];
	Sym syms[8];
	int i, group, size, used, capacity;

	capacity = count*12+1200;
	list = calloc(capacity, sizeof(*list)); copy = calloc(capacity, sizeof(*copy));
	check(list != nil && copy != nil);
	memset(ts, 0, sizeof(ts)); memset(syms, 0, sizeof(syms));
	for(i = 0; i < nelem(ts); i++)
		ts[i].size = i == 0 ? 1 : 1<<(i+1);
	for(i = 0; i < nelem(syms); i++)
		syms[i].name = i%2 ? "same" : "other";
	used = 0; dummy = &ids[0]; randomstate = seed+1;
	for(group = 0; group < count; group++){
		randomstate = randomstate*1664525+1013904223;
		size = group == 0 && count > 32 ? 1000 : 1+(randomstate>>16)%9;
		for(i = 0; i < size; i++){
			check(used < capacity);
			a = &list[used]; a->tag = used; a->nid = i == 0 ? size : 0;
			a->ty = &ts[(randomstate>>8)%nelem(ts)];
			a->sym = &syms[randomstate%nelem(syms)];
			a->refs = (randomstate>>16)%1024;
			a->link = randomstate&1 ? dummy : nil;
			a->next = &list[used+1]; used++;
		}
	}
	if(used)
		list[used-1].next = nil;
	memmove(copy, list, used*sizeof(*copy));
	for(i = 0; i < used; i++)
		copy[i].next = i+1 == used ? nil : &copy[i+1];
	a = declsort(used ? list : nil); b = reference_declsort(used ? copy : nil);
	for(i = 0; i < used; i++){
		check(a != nil && b != nil && a->tag == b->tag);
		check(a->nid == b->nid && a->refs == b->refs && a->ty == b->ty && a->link == b->link);
		a = a->next; b = b->next;
	}
	check(a == nil && b == nil);
	free(list); free(copy);
	scenarios++;
}


/* Run integer vectors, independent pair priorities and historical ordinary behavior. */
int
main(int argc, char **argv)
{
	struct rlimit limit;
	int i;

	check(argc >= 2 && sizeof(long) == 8);
	limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	if(strcmp(argv[1], "refs") == 0){
		check(argc == 3); vectors(argv[2]);
	}else if(strcmp(argv[1], "pairs") == 0){
		check(argc == 3); pairs(argv[2]);
	}else if(strcmp(argv[1], "shallow") == 0){
		for(i = 0; i < 3000; i++){
			check(ordinary(i, 0) == ordinary(i, 1)); scenarios++;
		}
	}else if(strcmp(argv[1], "groups") == 0){
		for(i = 0; i < 1000; i++)
			groups(i, i%47);
		groups(981, 4096);
	}else
		check(0);
	printf("reference sort WORD%d %s: %lu scenarios\n", IBY2WD*8, argv[1], scenarios);
	return 0;
}
