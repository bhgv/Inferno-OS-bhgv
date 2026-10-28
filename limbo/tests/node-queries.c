#include "limbo.h"
#include <stdio.h>
#include <sys/resource.h>

#define check(test) predicate(!!(test), __LINE__) // Keep checks enabled under NDEBUG.

uchar sideeffect[Oend]; // Independent effect table for query semantics.
uchar isused[Oend]; // Independent consumed-value table.
static ulong outstanding; // Live private traversal allocations.
static ulong allocations; // Successful traversal allocations.


/* Report an oracle failure without relying on assert. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "node query failure at %d\n", line);
		abort();
	}
}


/* Disable unavailable process inspection while retaining address instrumentation. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Unexpected internal compiler failures fail the fixture. */
void
fatal(char *format, ...)
{
	fprintf(stderr, "unexpected fatal: %s\n", format);
	abort();
}


/* Track only the private work storage allocated by extracted queries. */
static void*
querymalloc(ulong bytes)
{
	void *p;

	p = malloc(bytes);
	check(p != nil);
	outstanding++;
	allocations++;
	return p;
}


/* Check balanced cleanup, including early successful predicate exits. */
static void
queryfree(void *p)
{
	check(p != nil && outstanding > 0);
	outstanding--;
	free(p);
}

/* Equal Type pointers have already passed the production comparison's identity gate. */
int
tequal(Type *a, Type *b)
{
	return a == b;
}


#define malloc querymalloc // Observe production work allocations.
#define free queryfree // Observe production work releases.
IMPLEMENTATION
#undef malloc
#undef free


/* Retain the original recursive rules as a small-tree independent oracle. */
static int
oracle(Node *n, int mode)
{
	if(n == nil)
		return 0;
	if(mode == 4)
		return 1+oracle(n->left, mode)+oracle(n->right, mode);
	if(mode < 2 && sideeffect[n->op] && (mode || (n->op != Oadr && n->op != Oind)))
		return 1;
	if(mode == 2 && (n->op == Ocall || n->op == Ospawn))
		return 1;
	if(mode == 3 && n->op != Ocall && isused[n->op] && n->op != Onothing)
		return 1;
	return oracle(n->left, mode) || oracle(n->right, mode);
}


/* Dispatch each public production query, keeping both strictness modes visible. */
static int
query(Node *n, int mode)
{
	switch(mode){
	case 0: return hasside(n, 0);
	case 1: return hasside(n, 1);
	case 2: return hascall(n);
	case 3: return hasasgns(n);
	default: return nodes(n);
	}
}


/* Check mixed trees, nil, terminal metadata, repeated DAG edges and unchanged bytes. */
static void
small(void)
{
	Node tree[63], original[63];
	int ops[] = {Oname, Oconst, Ocall, Ospawn, Oadr, Oind, Onothing, Oadd, Oas};
	int seed, count, i, mode, op;
	ulong cases;

	cases = 0;
	for(seed = 0; seed < 64; seed++){
		for(op = 0; op < Oend; op++){
			sideeffect[op] = (op+seed)%3 == 0;
			isused[op] = (op+seed)%4 == 0;
		}
		for(count = 0; count <= 63; count++){
			memset(tree, 0, sizeof(tree));
			for(i = 0; i < count; i++){
				tree[i].op = ops[(i+seed)%nelem(ops)];
				if(2*i+1 < count)
					tree[i].left = &tree[2*i+1];
				if(2*i+2 < count)
					tree[i].right = &tree[2*i+2];
			}
			/* A shared subtree must be visited once per incoming edge. */
			if(count > 2 && seed&1)
				tree[0].right = tree[0].left;
			memmove(original, tree, sizeof(tree));
			for(mode = 0; mode < 5; mode++){
				check(query(count ? tree : nil, mode) == oracle(count ? tree : nil, mode));
				check(outstanding == 0 && memcmp(original, tree, sizeof(tree)) == 0);
				cases++;
			}
		}
	}
	printf("PASS node queries: %lu structural comparisons\n", cases);
}


/* Exercise both associations and growing pending work under a 256 KiB stack. */
static void
deep(void)
{
	struct rlimit limit;
	Node *tree, *original;
	int count, direction, mode, i, marked;
	ulong before;

	count = 100000;
	check(getrlimit(RLIMIT_STACK, &limit) == 0);
	limit.rlim_cur = 256*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	tree = calloc(2*count, sizeof(*tree));
	original = malloc(2*count*sizeof(*original));
	check(tree != nil && original != nil);
	memset(sideeffect, 0, sizeof(sideeffect));
	memset(isused, 0, sizeof(isused));
	for(direction = 0; direction < 3; direction++){
		memset(tree, 0, 2*count*sizeof(*tree));
		for(i = 0; i < count; i++){
			tree[i].op = Oadd;
			if(i+1 < count){
				if(direction == 0)
					tree[i].right = &tree[i+1];
				else
					tree[i].left = &tree[i+1];
			}
			if(direction == 2)
				tree[i].right = &tree[count+i];
		}
		memmove(original, tree, 2*count*sizeof(*tree));
		for(mode = 0; mode < 5; mode++){
			if(getenv("NODE_QUERY_MODE") && mode != atoi(getenv("NODE_QUERY_MODE")))
				continue;
			before = allocations;
			check(query(tree, mode) == (mode == 4 ? count*(direction == 2 ? 2 : 1) : 0));
			check(outstanding == 0 && !memcmp(original, tree, 2*count*sizeof(*tree)));
			if(direction == 2)
				check(allocations > before);
		}
		/* A deep hit occurs after work has grown; early exit must release it. */
		marked = count-1;
		for(mode = 0; mode < 4; mode++){
			if(getenv("NODE_QUERY_MODE") && mode != atoi(getenv("NODE_QUERY_MODE")))
				continue;
			tree[marked].op = mode == 3 ? Oas : Ocall;
			sideeffect[Ocall] = 1;
			isused[Oas] = 1;
			check(query(tree, mode) == 1 && outstanding == 0);
			tree[marked] = original[marked];
			check(!memcmp(original, tree, 2*count*sizeof(*tree)));
		}
	}
	free(original);
	free(tree);
	puts("PASS node queries: 15 deep full traversals and 12 deep early exits");
}


/* Compare deep distinct trees, then reject a last-node difference after growth. */
static void
equalitydeep(void)
{
	Node *a, *b, *saved;
	int count, direction, i;

	count = 100000;
	a = calloc(2*count, sizeof(*a));
	b = calloc(2*count, sizeof(*b));
	saved = malloc(4*count*sizeof(*saved));
	check(a != nil && b != nil && saved != nil);
	for(direction = 0; direction < 3; direction++){
		memset(a, 0, 2*count*sizeof(*a));
		memset(b, 0, 2*count*sizeof(*b));
		for(i = 0; i < 2*count; i++){
			a[i].op = b[i].op = Oadd;
			if(i+1 < count){
				if(direction == 0){
					a[i].right = &a[i+1];
					b[i].right = &b[i+1];
				}else{
					a[i].left = &a[i+1];
					b[i].left = &b[i+1];
				}
			}
			if(i < count && direction == 2){
				a[i].right = &a[count+i];
				b[i].right = &b[count+i];
			}
		}
		memmove(saved, a, 2*count*sizeof(*a));
		memmove(saved+2*count, b, 2*count*sizeof(*b));
		check(sametree(a, b) == 1 && outstanding == 0);
		check(!memcmp(saved, a, 2*count*sizeof(*a)));
		check(!memcmp(saved+2*count, b, 2*count*sizeof(*b)));
		b[count-1].op = Osub;
		check(sametree(a, b) == 0 && outstanding == 0);
		check(sametree(a, a) == 1 && sametree(nil, nil) == 1);
	}
	free(saved);
	free(b);
	free(a);
	puts("PASS sametree: 3 deep equal pairs and 3 deep mismatches");
}


/* Preserve constant-terminal rules, type/declaration identity and IEEE comparison. */
static void
equalitysmall(void)
{
	Node a, b, la, lb, ra, rb;
	Type types[8];
	Decl declarations[2];
	Sym symbols[2];
	int kinds[] = {Tint, Tbyte, Tbig, Treal, Tfix, Tstring, Ttuple, Tadt};
	int i, value;
	uvlong nanbits;

	memset(types, 0, sizeof(types));
	memset(declarations, 0, sizeof(declarations));
	memset(symbols, 0, sizeof(symbols));
	memset(&a, 0, sizeof(a));
	for(i = 0; i < nelem(kinds); i++){
		types[i].kind = kinds[i];
		for(value = -8; value <= 8; value++){
			a.op = Oconst;
			a.ty = &types[i];
			a.val = value;
			a.rval = value;
			a.decl = &declarations[0];
			/* Constants must never follow cyclic child metadata. */
			a.left = a.right = &a;
			b = a;
			b.left = b.right = &b;
			check(sametree(&a, &b) == (i < 6));
			b.val++;
			b.rval++;
			b.decl = &declarations[1];
			declarations[1].sym = &symbols[1];
			check(sametree(&a, &b) == 0);
			declarations[1].sym = nil;
			check(sametree(&a, &a) == 1 && !sametree(&a, nil));
		}
	}
	a.ty = &types[3];
	a.rval = 0.0;
	b = a;
	b.rval = -0.0;
	check(sametree(&a, &b));
	nanbits = 0x7ff8000000000001ULL;
	check(sizeof(a.rval) == sizeof(nanbits));
	memmove(&a.rval, &nanbits, sizeof(nanbits));
	b = a;
	check(!sametree(&a, &b) && sametree(&a, &a));
	memset(&a, 0, sizeof(a));
	a.op = Oadd;
	b = la = lb = ra = rb = a;
	a.left = &la; a.right = &ra;
	b.left = &lb; b.right = &rb;
	check(sametree(&a, &b));
	a.right = (Node*)1; b.right = (Node*)2;
	lb.decl = &declarations[1];
	check(!sametree(&a, &b));
	lb.decl = nil;
	a.right = &ra; b.right = &rb;
	rb.ty = &types[0];
	check(!sametree(&a, &b));
	check(outstanding == 0);
	puts("PASS sametree: constant/type/declaration/terminal cases");
}


/* Select isolated small or deep runs so stack exhaustion remains attributable. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(strcmp(argv[1], "small") == 0){
		small();
		equalitysmall();
	}else{
		deep();
		equalitydeep();
	}
	return 0;
}
