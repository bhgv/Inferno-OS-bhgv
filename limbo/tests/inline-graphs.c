#include "limbo.h"
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>

#define check(test) predicate(!!(test), __LINE__) // Keep graph oracles active under NDEBUG.

static ulong livework, allocations; // Private traversal allocation accounting.


/* Diagnose failed semantics or unrestored declaration state. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "inline graph failure at %d\n", line);
		abort();
	}
}


/* Retain sanitizer instrumentation without unavailable LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Unexpected compiler resource failures are test failures. */
void
fatal(char *message, ...)
{
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Observe only traversal work; ordinary AST and declaration storage stays borrowed. */
static void*
workmalloc(ulong bytes)
{
	void *p;

	p = malloc(bytes);
	check(p != nil);
	livework++;
	allocations++;
	return p;
}


/* Verify every work owner is released on both acceptance and rejection. */
static void
workfree(void *p)
{
	check(p != nil && livework > 0);
	livework--;
	free(p);
}

#define malloc workmalloc // Observe production work ownership.
#define free workfree // Observe production work cleanup.
IMPLEMENTATION
#undef malloc
#undef free


/* Retain the original semantic rules for small acyclic inline graphs. */
static int
oracle(Node *n, Decl *d)
{
	if(n == nil)
		return 1;
	if(n->op == Oname){
		if(n->decl == d)
			return 0;
		if(n->decl->caninline == 1)
			return oracle(n->decl->init->right, d);
		return 1;
	}
	return oracle(n->left, d) && oracle(n->right, d);
}


/* Compare acyclic graphs, terminal names and shared dependencies without mutations. */
static void
small(void)
{
	Decl decls[9], saveddecls[9];
	Node functions[8], bodies[8], input[31], savedinput[31];
	int pattern, i, target;
	ulong cases;

	cases = 0;
	for(pattern = 0; pattern < 256; pattern++){
		memset(decls, 0, sizeof(decls));
		memset(functions, 0, sizeof(functions));
		memset(bodies, 0, sizeof(bodies));
		memset(input, 0, sizeof(input));
		for(i = 0; i < 8; i++){
			decls[i].caninline = (pattern>>i)&1;
			decls[i].init = &functions[i];
			functions[i].right = &bodies[i];
			bodies[i].op = Oname;
			bodies[i].decl = &decls[i+1];
			/* Name child metadata must remain opaque. */
			bodies[i].left = (Node*)1;
			bodies[i].right = (Node*)2;
		}
		for(i = 0; i < 31; i++){
			input[i].op = i < 15 ? Oadd : Oname;
			if(i < 15){
				input[i].left = &input[2*i+1];
				input[i].right = &input[2*i+2];
			}else
				input[i].decl = &decls[(i+pattern)%9];
		}
		memmove(saveddecls, decls, sizeof(decls));
		memmove(savedinput, input, sizeof(input));
		for(target = 0; target < 9; target++){
			check(ckinline0(input, &decls[target]) == oracle(input, &decls[target]));
			check(!memcmp(decls, saveddecls, sizeof(decls)));
			check(!memcmp(input, savedinput, sizeof(input)) && livework == 0);
			cases++;
		}
	}
	check(ckinline0(nil, decls) == 1);
	decls[0].caninline = -1;
	decls[0].init = nil;
	input[0].op = Oname;
	input[0].decl = decls;
	input[0].left = (Node*)1;
	input[0].right = (Node*)2;
	check(ckinline0(input, &decls[8]) == 1 && decls[0].caninline == -1);
	printf("PASS inline graphs: %lu acyclic oracle comparisons and nil\n", cases);
}


/* Test deep ASTs, transitive candidate chains and cycles not containing the root. */
static void
deep(char *family)
{
	struct rlimit limit;
	Decl *decls, *saveddecls, target;
	Node *tree, *functions, *savedtree;
	int count, i, direction, expected;
	ulong before;

	check(getrlimit(RLIMIT_STACK, &limit) == 0);
	limit.rlim_cur = 256*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	count = 100000;
	decls = calloc(count, sizeof(*decls));
	saveddecls = malloc(count*sizeof(*decls));
	tree = calloc(2*count, sizeof(*tree));
	functions = calloc(count, sizeof(*functions));
	savedtree = malloc(2*count*sizeof(*tree));
	check(decls != nil && saveddecls != nil && tree != nil && functions != nil && savedtree != nil);
	memset(&target, 0, sizeof(target));
	for(direction = 0; direction < 3; direction++){
		memset(tree, 0, 2*count*sizeof(*tree));
		memset(decls, 0, count*sizeof(*decls));
		before = allocations;
		for(i = 0; i < count; i++){
			if(strcmp(family, "tree") == 0){
				tree[i].op = Oadd;
				if(i+1 < count){
					if(direction == 0)
						tree[i].right = &tree[i+1];
					else
						tree[i].left = &tree[i+1];
				}
				if(direction == 2)
					tree[i].right = &tree[count+i];
			}else{
				decls[i].caninline = 1;
				decls[i].init = &functions[i];
				functions[i].right = i+1 < count ? &tree[i+1] : nil;
				tree[i].op = Oname;
				tree[i].decl = &decls[i];
			}
		}
		expected = 1;
		if(strcmp(family, "cycle") == 0){
			functions[count-1].right = &tree[direction == 0 ? count-1 : direction == 1 ? count-2 : count/2];
			expected = 0;
		}else if(direction == 1){
			if(strcmp(family, "tree") == 0){
				tree[count-1].op = Oname;
				tree[count-1].decl = &target;
			}else
				functions[count-1].right = &tree[count];
			tree[count].op = Oname;
			tree[count].decl = &target;
			expected = 0;
		}
		memmove(saveddecls, decls, count*sizeof(*decls));
		memmove(savedtree, tree, 2*count*sizeof(*tree));
		check(ckinline0(tree, &target) == expected && livework == 0);
		check(!memcmp(saveddecls, decls, count*sizeof(*decls)));
		check(!memcmp(savedtree, tree, 2*count*sizeof(*tree)));
		if(strcmp(family, "tree") != 0 || direction == 2)
			check(allocations > before);
	}
	free(savedtree);
	free(functions);
	free(tree);
	free(saveddecls);
	free(decls);
	printf("PASS deep inline %s: three 100000-level cases, restored flags and work ownership\n", family);
}


/* A compact candidate diamond must not expand exponentially through shared callees.
 * Query success, a later independent rejection and success again with exact flag
 * restoration. The final missing body is a valid empty candidate for this oracle. */
static void
shared(void)
{
	enum { Count = 256 };
	Decl decls[Count], saved[Count], target;
	Node functions[Count], bodies[Count], names[Count], root, terminal;
	int i, pass;

	memset(decls, 0, sizeof(decls));
	memset(functions, 0, sizeof(functions));
	memset(bodies, 0, sizeof(bodies));
	memset(names, 0, sizeof(names));
	memset(&target, 0, sizeof(target));
	memset(&root, 0, sizeof(root));
	memset(&terminal, 0, sizeof(terminal));
	root.op = Oseq;
	root.left = names;
	terminal.op = Oname;
	terminal.decl = &target;
	alarm(10); // Bound old-source regressions instead of leaving exponential work running.
	for(i = 0; i < Count; i++){
		decls[i].caninline = 1;
		decls[i].init = &functions[i];
		names[i].op = Oname;
		names[i].decl = &decls[i];
		if(i+1 < Count){
			functions[i].right = &bodies[i];
			bodies[i].op = Oseq;
			bodies[i].left = &names[i+1];
			bodies[i].right = &names[i+1];
		}
	}
	memmove(saved, decls, sizeof(saved));
	for(pass = 0; pass < 4; pass++){
		names[Count-1].decl = pass == 1 ? &target : &decls[Count-1];
		root.right = pass == 2 ? &terminal : nil;
		check(ckinline0(&root, &target) == (pass == 0 || pass == 3));
		check(!memcmp(saved, decls, sizeof(saved)) && livework == 0);
	}
	alarm(0);
	printf("PASS shared inline DAG: 256 candidates, query-local cache and exact cleanup\n");
}


/* Isolate each regression so stack failures remain attributable to their graph shape. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(strcmp(argv[1], "small") == 0)
		small();
	else if(strcmp(argv[1], "shared") == 0)
		shared();
	else
		deep(argv[1]);
	return 0;
}
