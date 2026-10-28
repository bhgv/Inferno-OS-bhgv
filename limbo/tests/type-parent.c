#include "limbo.h"
#include <stdio.h>
#include <sys/resource.h>

#define check(test) predicate(!!(test), __LINE__) // Keep every construction and ownership check under NDEBUG.
#define ParentDepth 100000 // Wrapper chains exercise native-stack independence.
#define TupleDepth 20000 // Deferred tuple siblings require checked private stack growth.

static Type ztype; // Real mktype initializes results from this compiler template.
static Type types[4096], savedtypes[4096]; // Borrowed shallow Types and immutable snapshots.
static Decl decls[8192], saveddecls[8192]; // Borrowed fields and pick-parent declarations.
static int ntypes, ndecls; // Current borrowed-graph arena cursors.
static void *results[400000], *privateblocks[64]; // Result ownership and independently tracked private work.
static int nresults, nprivate; // Live allocations in each ownership domain.
static ulong checks, privateallocations; // Completed scenarios and cumulative private stack growth.
static uint randomstate = 0x192741; // Deterministic graph generator independent of traversal.

/* Delegate traces retain source identities so shallow short-circuit order is observable. */
typedef struct ParentTrace ParentTrace;

struct ParentTrace {
	Type *left; // First borrowed delegate argument.
	Type *right; // Second borrowed delegate argument.
	int kind; // Equality or compatibility delegate selected by production code.
};

static ParentTrace trace[4096], savedtrace[4096]; // Current and reference delegate sequences.
static int ntrace; // Number of nonidentity delegate calls in the current attempt.


/* Diagnose the first result, reuse, callback-order or ownership discrepancy. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "parent Type failure at %d\n", line);
		abort();
	}
}


/* Keep sanitizer access checks without unavailable process-wide LSan inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Valid structural test pairs should not trigger a fatal compiler diagnostic. */
void
fatal(char *message, ...)
{
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Retain the original constructors' ownership separately from traversal work. */
void*
allocmem(ulong bytes)
{
	void *p;

	check(nresults < nelem(results) && bytes > 0 && bytes < 1048576);
	p = malloc(bytes);
	check(p != nil);
	results[nresults++] = p;
	return p;
}


/* Track temporary continuations, which must disappear even on predicate rejection. */
static void*
parentalloc(ulong bytes)
{
	void *p;

	check(nprivate < nelem(privateblocks) && bytes > 0 && bytes < 128*1024*1024);
	p = malloc(bytes);
	check(p != nil);
	privateblocks[nprivate++] = p;
	privateallocations++;
	return p;
}


/* Reject double frees and accidental consumption of a returned compiler Type. */
static void
parentfree(void *p)
{
	int i;

	for(i = 0; i < nprivate && privateblocks[i] != p; i++)
		;
	check(i < nprivate);
	privateblocks[i] = privateblocks[--nprivate];
	free(p);
}


/* Record meaningful delegate order while allowing identity checks to be skipped. */
static void
record(int kind, Type *left, Type *right)
{
	if(left == right)
		return;
	check(ntrace < nelem(trace));
	memset(&trace[ntrace], 0, sizeof(trace[ntrace]));
	trace[ntrace].kind = kind;
	trace[ntrace].left = left;
	trace[ntrace++].right = right;
}


/* Model nominal compatibility by family signature, without recursive fixture helpers. */
int
tcompat(Type *left, Type *right, int any)
{
	check(any == 0);
	record(0, left, right);
	return left->kind == right->kind && left->sig == right->sig;
}


/* Distinguish pick tags while recognizing their shared nominal parent. */
int
tequal(Type *left, Type *right)
{
	record(1, left, right);
	return left == right || (left->kind == right->kind && left->sig == right->sig);
}

SUPPORT

#define allocmem parentalloc // Track only continuation storage in the traversal implementation.
#define free parentfree // Require every temporary stack owner to be released.
IMPLEMENTATION
#undef allocmem
#undef free

REFERENCE


/* Release independent output graphs only after their full comparison. */
static void
release(void)
{
	check(nprivate == 0);
	while(nresults != 0)
		free(results[--nresults]);
}


/* Identify returned allocations without confusing borrowed source Types with copies. */
static int
owned(void *p)
{
	int i;

	for(i = 0; i < nresults; i++)
		if(results[i] == p)
			return 1;
	return 0;
}


/* Compare shallow output shape, exact borrowed identities and copied declaration metadata. */
static void
same(Type *left, Type *right)
{
	Type a, b;
	Decl *x, *y, d, e;

	if(left == right)
		return;
	check(left != nil && right != nil && owned(left) && owned(right));
	a = *left;
	b = *right;
	a.tof = b.tof = nil;
	a.ids = b.ids = nil;
	check(!memcmp(&a, &b, sizeof(a)));
	same(left->tof, right->tof);
	for(x = left->ids, y = right->ids; x != nil && y != nil; x = x->next, y = y->next){
		d = *x;
		e = *y;
		d.next = e.next = nil;
		d.ty = e.ty = nil;
		check(!memcmp(&d, &e, sizeof(d)));
		same(x->ty, y->ty);
	}
	check(x == nil && y == nil);
}


/* Advance the reproducible graph constructor. */
static uint
nextvalue(void)
{
	randomstate = randomstate*1664525U+1013904223U;
	return randomstate;
}


/* Reserve a borrowed Type with stable source metadata and no constructor ownership. */
static Type*
borrowtype(int kind)
{
	Type *t;

	check(ntypes < nelem(types));
	t = &types[ntypes++];
	memset(t, 0, sizeof(*t));
	t->kind = kind;
	t->src.start.line = ntypes;
	t->src.stop.line = ntypes+1;
	return t;
}


/* Reserve one borrowed declaration whose complete metadata must survive copying. */
static Decl*
borrowdecl(Type *type)
{
	Decl *d;

	check(ndecls < nelem(decls));
	d = &decls[ndecls++];
	memset(d, 0, sizeof(*d));
	d->ty = type;
	d->store = Dfield;
	d->offset = ndecls*IBY2WD;
	d->refs = ndecls;
	return d;
}


/* Establish scalar, nominal and picked terminals with independent tag/family identities. */
static void
terminals(void)
{
	int i;

	ntypes = ndecls = 0;
	borrowtype(Tint);
	borrowtype(Tint);
	borrowtype(Treal);
	for(i = 0; i < 3; i++)
		borrowtype(Tadt)->sig = i == 2 ? 2 : 1;
	for(i = 0; i < 3; i++){
		Type *pick;
		pick = borrowtype(Tadtpick);
		pick->sig = i;
		pick->decl = borrowdecl(nil);
		pick->decl->dot = borrowdecl(&types[i == 2 ? 5 : 3]);
	}
	for(i = 0; i < 5; i++){
		int kind;
		kind = (int[]){Texception, Tfix, Tfn, Tmodule, Tpoly}[i];
		borrowtype(kind)->sig = 100+i;
		borrowtype(kind)->sig = 100+i+(i&1);
	}
	for(i = 0; i < 7; i++){
		int kind;
		kind = (int[]){Terror, Tstring, Tnone, Tbig, Tbyte, Treal, Tany}[i];
		borrowtype(kind);
		borrowtype(kind);
	}
}


/* Construct matching wrappers/tuples with varied compatible and incompatible terminals. */
static void
pair(int depth, Type **left, Type **right)
{
	Type *a, *b;
	Decl **x, **y;
	uint choice;
	int i, fields;

	choice = nextvalue();
	if(depth == 0 || choice%4 == 0){
		int indices[][2] = {{0,1}, {0,2}, {3,4}, {3,5}, {6,7}, {6,3}, {3,6}, {6,6}, {6,8},
		                   {9,10}, {11,12}, {13,14}, {15,16}, {17,18}, {19,20}, {21,22},
		                   {23,24}, {25,26}, {27,28}, {29,30}, {31,32}};
		*left = &types[indices[choice%nelem(indices)][0]];
		*right = &types[indices[choice%nelem(indices)][1]];
		return;
	}
	if(choice%3 != 0){
		a = borrowtype((int[]){Tref,Tlist,Tarray,Tchan}[choice%4]);
		b = borrowtype(a->kind);
		pair(depth-1, &a->tof, &b->tof);
	}else{
		a = borrowtype(Ttuple);
		b = borrowtype(Ttuple);
		x = &a->ids;
		y = &b->ids;
		fields = nextvalue()%4;
		for(i = 0; i < fields; i++){
			Type *p, *q;
			pair(depth-1, &p, &q);
			*x = borrowdecl(p);
			*y = borrowdecl(q);
			x = &(*x)->next;
			y = &(*y)->next;
		}
		if(choice%11 == 0)
			*y = borrowdecl(&types[0]);
	}
	*left = a;
	*right = b;
}


/* Stop after the first incompatible field and defer tuple-length checks until its prefix returns. */
static void
shortcircuits(void)
{
	Type left, right;
	Decl first, second;

	terminals();
	memset(&left, 0, sizeof(left));
	memset(&right, 0, sizeof(right));
	memset(&first, 0, sizeof(first));
	memset(&second, 0, sizeof(second));
	left.kind = right.kind = Ttuple;
	left.ids = &first;
	right.ids = &second;
	first.ty = &types[3];
	second.ty = &types[5];
	first.next = second.next = (Decl*)1;
	ntrace = 0;
	check(tparent(&left, &right) == &left && ntrace == 1 && trace[0].kind == 0);
	check(nprivate == 0 && nresults == 0);
	checks++;
	first.next = nil;
	second.ty = &types[4];
	ntrace = 0;
	check(tparent(&left, &right) == &left && ntrace == 1 && trace[0].kind == 0);
	check(nprivate == 0 && nresults == 0);
	checks++;
}


/* Copy once after an unchanged tuple prefix, then update later fields in that same copy. */
static void
latecopy(void)
{
	Type *left, *right, *expected, *actual;
	Decl *a[3], *b[3];
	int i;

	terminals();
	left = borrowtype(Ttuple);
	right = borrowtype(Ttuple);
	for(i = 0; i < 3; i++){
		a[i] = borrowdecl(&types[i == 0 ? 0 : 6]);
		b[i] = borrowdecl(&types[i == 0 ? 0 : i == 1 ? 7 : 3]);
		if(i != 0){
			a[i-1]->next = a[i];
			b[i-1]->next = b[i];
		}
	}
	left->ids = a[0];
	right->ids = b[0];
	memmove(savedtypes, types, ntypes*sizeof(*types));
	memmove(saveddecls, decls, ndecls*sizeof(*decls));
	expected = reference_tparent(left, right);
	actual = tparent(left, right);
	same(actual, expected);
	check(nresults == 8 && nprivate == 0 && actual != left);
	check(actual->ids->ty == &types[0] && actual->ids->next->ty == &types[3]);
	check(actual->ids->next->next->ty == &types[3]);
	check(!memcmp(savedtypes, types, ntypes*sizeof(*types)));
	check(!memcmp(saveddecls, decls, ndecls*sizeof(*decls)));
	release();
	checks++;
}


/* Compare historical shallow results, lazy copies, callback order and immutable sources. */
static void
shallow(void)
{
	Type *left, *right, *expected, *actual;
	int i, count, oldblocks;

	for(i = 0; i < 3000; i++){
		terminals();
		pair(6, &left, &right);
		memmove(savedtypes, types, ntypes*sizeof(*types));
		memmove(saveddecls, decls, ndecls*sizeof(*decls));
		ntrace = 0;
		expected = reference_tparent(left, right);
		count = ntrace;
		oldblocks = nresults;
		memmove(savedtrace, trace, count*sizeof(*trace));
		ntrace = 0;
		actual = tparent(left, right);
		check(nresults == 2*oldblocks && nprivate == 0);
		check(ntrace == count && !memcmp(trace, savedtrace, count*sizeof(*trace)));
		same(actual, expected);
		check(!memcmp(savedtypes, types, ntypes*sizeof(*types)));
		check(!memcmp(saveddecls, decls, ndecls*sizeof(*decls)));
		release();
		checks++;
	}
}


/* Verify large wrapper/tuple continuations with actual constructors under a small stack. */
static void
deep(int tuple, int build)
{
	Type *left, *right, *result, *cursor;
	Decl *fields;
	struct rlimit limit;
	int depth, i, j;
	ulong before;

	limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	depth = tuple ? TupleDepth : ParentDepth;
	left = calloc(depth, sizeof(*left));
	right = calloc(depth, sizeof(*right));
	fields = tuple ? calloc(4*depth, sizeof(*fields)) : nil;
	check(left != nil && right != nil && (!tuple || fields != nil));
	terminals();
	for(i = 0; i < depth; i++){
		left[i].kind = right[i].kind = tuple ? Ttuple : (int[]){Tref,Tlist,Tarray,Tchan}[i%4];
		left[i].src.start.line = right[i].src.start.line = i+1;
		if(tuple){
			left[i].ids = &fields[4*i];
			right[i].ids = &fields[4*i+2];
			for(j = 0; j < 4; j++)
				fields[4*i+j].store = Dfield;
			fields[4*i].ty = i+1 < depth ? &left[i+1] : &types[6];
			fields[4*i+2].ty = i+1 < depth ? &right[i+1] : &types[7];
			fields[4*i].next = &fields[4*i+1];
			fields[4*i+2].next = &fields[4*i+3];
			fields[4*i+1].ty = fields[4*i+3].ty = &types[0];
		}else{
			left[i].tof = i+1 < depth ? &left[i+1] : &types[6];
			right[i].tof = i+1 < depth ? &right[i+1] : &types[7];
		}
	}
	ntrace = 0;
	if(build){
		result = tparent1(left, right);
		cursor = result;
		for(i = 0; i < depth; i++){
			check(cursor != &left[i] && cursor != &right[i]);
			check(cursor->kind == left[i].kind && !memcmp(&cursor->src, &left[i].src, sizeof(Src)));
			if(tuple){
				check(cursor->ids != left[i].ids && cursor->ids->next->ty == &types[0]);
				cursor = cursor->ids->ty;
			}else
				cursor = cursor->tof;
		}
		check(cursor == &types[3]);
		check(nresults == depth*(tuple ? 3 : 1));
		release();
	}else
		check(tparent0(left, right) == 1);
	check(nprivate == 0);
	checks++;
	/* Complete unchanged reconstruction and rejected predicates must release grown stacks. */
	if(tuple)
		fields[4*(depth-1)+2].ty = &types[6];
	else
		right[depth-1].tof = &types[6];
	check(tparent(left, right) == left && nprivate == 0 && nresults == 0);
	checks++;
	if(tuple)
		fields[4*(depth-1)+2].ty = &types[0];
	else
		right[depth-1].tof = &types[0];
	check(tparent(left, right) == left && nprivate == 0 && nresults == 0);
	checks++;
	/* Sharing the whole source must avoid traversal and constructor allocations. */
	before = privateallocations;
	check(tparent(left, left) == left && nresults == 0 && nprivate == 0);
	check(privateallocations == before);
	checks++;
	free(left);
	free(right);
	free(fields);
}


/* Separate identity regressions from deep predicate and constructor probes. */
static void
identity(int empty)
{
	struct rlimit limit;
	Type *chain;
	int i;

	if(empty){
		check(tparent(nil, nil) == nil && nprivate == 0 && nresults == 0);
		check(tparent(nil, &types[0]) == nil && tparent(&types[0], nil) == &types[0]);
		checks += 2;
	}else{
		limit.rlim_cur = limit.rlim_max = 512*1024;
		check(setrlimit(RLIMIT_STACK, &limit) == 0);
		chain = calloc(ParentDepth, sizeof(*chain));
		check(chain != nil);
		for(i = 0; i < ParentDepth; i++){
			chain[i].kind = Tref;
			chain[i].tof = i+1 < ParentDepth ? &chain[i+1] : &types[0];
		}
		check(tparent(chain, chain) == chain && nprivate == 0 && nresults == 0);
		free(chain);
	}
	checks++;
}


/* Keep each parent-source crash attributable to one isolated traversal. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	ztype.sbl = -1;
	terminals();
	if(!strcmp(argv[1], "shallow")){
		shallow();
		shortcircuits();
		latecopy();
	}
	else if(!strcmp(argv[1], "check"))
		deep(0, 0);
	else if(!strcmp(argv[1], "build"))
		deep(0, 1);
	else if(!strcmp(argv[1], "tuple")){
		deep(1, 0);
		deep(1, 1);
	}else
		identity(!strcmp(argv[1], "nil"));
	check(nprivate == 0 && nresults == 0);
	printf("PASS parent Types %s: %lu result/reuse/order/stack scenarios\n", argv[1], checks);
	return 0;
}
