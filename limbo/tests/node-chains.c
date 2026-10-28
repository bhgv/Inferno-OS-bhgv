#include "limbo.h"
#include <stdio.h>
#include <sys/resource.h>

#define chaincheck(test) chainpredicate(!!(test), __LINE__) // Keep AST checks active under NDEBUG.

Node znode;
static Node *arena;
static ulong capacity, used;
static unsigned long checks;


/* Diagnose structural failures without depending on compiler assertions. */
static void
chainpredicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "node chain predicate failed at line %d\n", line);
		abort();
	}
}


/* This fixture accounts for all storage without LSan process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Production constructors may allocate exactly one node at a time. */
void*
allocmem(ulong size)
{
	chaincheck(size == sizeof(Node) && used < capacity);
	return &arena[used++];
}

/* The production counter fails explicitly rather than overflowing a signed count. */
void
fatal(char *format, ...)
{
	fprintf(stderr, "unexpected compiler fatal: %s\n", format);
	abort();
}

IMPLEMENTATION


/* Check assignment association, one RHS evaluation site, source spans and references. */
static void
initializers(int count, int pattern)
{
	Decl *declarations, *expected;
	Node expression, original, *tree, *assignment, *name;
	Type types[2];
	Decl absent = {0};
	uchar *storage;
	int i;

	declarations = calloc(count, sizeof(*declarations));
	expected = calloc(count, sizeof(*expected));
	capacity = 2*(ulong)count;
	storage = malloc(capacity*sizeof(Node)+64);
	chaincheck(declarations != nil && expected != nil && storage != nil);
	memset(storage, 0x5a, capacity*sizeof(Node)+64);
	arena = (void*)(storage+32);
	used = 0;
	memset(&expression, 0, sizeof(expression));
	memset(types, 0, sizeof(types));
	expression.op = pattern%4 == 2 ? Oname : pattern&1 ? Ocall : Oconst;
	expression.decl = pattern%4 == 2 ? &declarations[0] : nil;
	expression.flags = pattern;
	expression.val = 0x12345678;
	expression.src.start.line = pattern+1;
	expression.src.stop.line = pattern+2;
	expression.src.start.pos = 7;
	expression.src.stop.pos = 19;
	expression.ty = &types[pattern&1];
	original = expression;
	for(i = 0; i < count; i++){
		declarations[i].next = i+1 == count ? nil : &declarations[i+1];
		declarations[i].ty = &types[(i+pattern)&1];
		declarations[i].refs = (i+pattern)%17;
		declarations[i].offset = i*4;
		expected[i] = declarations[i];
		expected[i].refs++;
	}
	tree = varinit(declarations, &expression);
	assignment = tree;
	for(i = 0; i < count; i++){
		chaincheck(assignment != nil && assignment != &expression && assignment->op == Oas);
		name = assignment->left;
		chaincheck(name != nil && name->op == Oname && name->decl == &declarations[i]);
		chaincheck(name->ty == declarations[i].ty && name->left == nil && name->right == nil);
		chaincheck(!memcmp(&name->src, &original.src, sizeof(Src)));
		chaincheck(!memcmp(&assignment->src, &original.src, sizeof(Src)));
		chaincheck(assignment->ty == znode.ty && assignment->decl == znode.decl);
		chaincheck(name->flags == znode.flags && assignment->flags == znode.flags);
		assignment = assignment->right;
	}
	chaincheck(assignment == &expression && used == capacity);
	for(i = 0; i < 3; i++){
		int index = i == 0 ? 0 : i == 1 ? count/2 : count-1;

		chaincheck(occurs(&declarations[index], tree) == 1+
		           (expression.op == Oname && index == 0));
	}
	chaincheck(occurs(&absent, tree) == 0);
	chaincheck(!memcmp(&original, &expression, sizeof(expression)));
	chaincheck(!memcmp(declarations, expected, count*sizeof(*expected)));
	for(i = 0; i < 32; i++)
		chaincheck(storage[i] == 0x5a && storage[32+capacity*sizeof(Node)+i] == 0x5a);
	free(storage);
	free(expected);
	free(declarations);
	arena = nil;
	checks++;
}


/* Compare every label byte with a direct permutation oracle and unchanged suffix. */
static void
labels(int count, int suffix, int pattern)
{
	Node *nodes, *expected, *result, *tail;
	int i, total = count+suffix;

	nodes = calloc(total, sizeof(*nodes));
	expected = calloc(total, sizeof(*expected));
	chaincheck(nodes != nil && expected != nil);
	for(i = 0; i < total; i++){
		nodes[i].op = Oseq;
		nodes[i].left = &nodes[(i+pattern)%total];
		nodes[i].right = i+1 == count || i+1 == total ? nil : &nodes[i+1];
		nodes[i].val = i+pattern;
		nodes[i].flags = pattern;
		nodes[i].src.start.line = i;
		nodes[i].src.stop.pos = pattern;
	}
	memmove(expected, nodes, total*sizeof(*expected));
	tail = suffix ? &nodes[count] : nil;
	for(i = 0; i < count; i++)
		expected[i].right = i == 0 ? tail : &nodes[i-1];
	used = 0;
	result = caselist(nodes, tail);
	chaincheck(result == &nodes[count-1] && used == 0);
	chaincheck(!memcmp(nodes, expected, total*sizeof(*expected)));
	for(i = count-1; i >= 0; i--){
		chaincheck(result == &nodes[i]);
		result = result->right;
	}
	chaincheck(result == tail);
	free(expected);
	free(nodes);
	checks++;
}


/* Build the reference-counting input independently of either chain constructor. */
static void
references(int count, int left)
{
	Node *nodes;
	Decl declarations[17];
	int i, selected, expected;

	nodes = calloc(2*(ulong)count, sizeof(*nodes));
	chaincheck(nodes != nil);
	memset(declarations, 0, sizeof(declarations));
	for(i = 0; i < count; i++){
		nodes[2*i].op = Oas;
		nodes[2*i].left = &nodes[2*i+1];
		nodes[2*i].right = i+1 == count ? nil : &nodes[2*i+2];
		nodes[2*i+1].op = Oname;
		nodes[2*i+1].decl = &declarations[i%16];
		/* Name nodes are terminal; even populated child links must be ignored. */
		nodes[2*i+1].left = &nodes[2*i];
		nodes[2*i+1].right = nodes;
	}
	if(left){
		for(i = 0; i < count; i++){
			Node *child = nodes[2*i].left;
			nodes[2*i].left = nodes[2*i].right;
			nodes[2*i].right = child;
		}
	}
	for(selected = 0; selected < 17; selected++){
		expected = selected == 16 || selected >= count ? 0 : (count-1-selected)/16+1;
		chaincheck(occurs(&declarations[selected], nodes) == expected);
	}
	free(nodes);
	checks++;
}


/* Bound the real C stack independently of allocator or AST size. */
static void
smallstack(void)
{
	struct rlimit limit;

	chaincheck(getrlimit(RLIMIT_STACK, &limit) == 0);
	limit.rlim_cur = 256*1024;
	chaincheck(setrlimit(RLIMIT_STACK, &limit) == 0);
}


/* Exhaust ordinary chains or isolate either deep production transformation. */
int
main(int argc, char **argv)
{
	int count, pattern, suffix;

	if(argc > 1){
		smallstack();
		if(strcmp(argv[1], "varinit") == 0)
			initializers(100000, 7);
		else if(strcmp(argv[1], "caselist") == 0)
			labels(100000, 4, 7);
		else if(strcmp(argv[1], "occurs") == 0)
			references(100000, 0);
		else if(strcmp(argv[1], "occurs-left") == 0)
			references(100000, 1);
		else
			chaincheck(0);
	}else{
		for(count = 1; count <= 512; count++){
			references(count, 0);
			references(count, 1);
			for(pattern = 0; pattern < 16; pattern++)
				initializers(count, pattern);
			for(suffix = 0; suffix <= 4; suffix++)
			for(pattern = 0; pattern < 8; pattern++)
				labels(count, suffix, pattern);
		}
	}
	printf("PASS compiler node chains: %lu structural cases\n", checks);
	return 0;
}
