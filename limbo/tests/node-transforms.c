#include "limbo.h"
#include <stdio.h>
#include <sys/resource.h>

#define check(test) predicate(!!(test), __LINE__) // Checks remain enabled with NDEBUG.

static Node *arena; // Fixed compiler allocation arena used by production copies.
static ulong used, capacity; // Allocated node count and private arena extent.
static ulong livework, workallocations; // Private traversal ownership accounting.
static Decl **events; // Ordered namesort/sizeids callback arguments.
static ulong nevents, eventcapacity; // Callback log cursor and extent.


/* Fail the test at the first violated semantic or ownership invariant. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "node transform failure at %d\n", line);
		abort();
	}
}


/* Preserve sanitizers while disabling unavailable process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Compiler resource failures are fatal rather than a recoverable AST result. */
void
fatal(char *message, ...)
{
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Preserve exactly one arena allocation per copied node and its preorder index. */
void*
allocmem(ulong bytes)
{
	check(bytes == sizeof(Node) && used < capacity);
	return &arena[used++];
}


/* Log sorting before returning a visibly changed declaration head. */
Decl*
namesort(Decl *d)
{
	check(nevents < eventcapacity && d != nil);
	events[nevents++] = d;
	return d->next;
}


/* The sizing callback must see the sorted head immediately after sorting. */
Szal
sizeids(Decl *d, long offset)
{
	Szal size;

	check(nevents < eventcapacity && offset == 0);
	events[nevents++] = d;
	memset(&size, 0, sizeof(size));
	return size;
}


/* Count private work separately from compiler arena allocations. */
static void*
workmalloc(ulong bytes)
{
	void *p;

	p = malloc(bytes);
	check(p != nil);
	livework++;
	workallocations++;
	return p;
}


/* Early exits and normal completion must release all private traversal storage. */
static void
workfree(void *p)
{
	check(p != nil && livework > 0);
	livework--;
	free(p);
}

#define malloc workmalloc // Observe production work allocations only.
#define free workfree // Observe production work cleanup only.
IMPLEMENTATION
#undef malloc
#undef free


/* Independently retain the recursive allocation contract on bounded small trees. */
static Node*
copyoracle(Node *n, Node *output, ulong *cursor, int resrc, Src *src)
{
	Node *copy;

	copy = &output[(*cursor)++];
	*copy = *n;
	if(resrc)
		copy->src = *src;
	if(n->left != nil)
		copy->left = copyoracle(n->left, output, cursor, resrc, src);
	if(n->right != nil)
		copy->right = copyoracle(n->right, output, cursor, resrc, src);
	return copy;
}


/* First-match ordering includes root communication before all child metadata. */
static Node*
commoracle(Node *n)
{
	Node *left;

	if(n == nil || n->op == Osnd || n->op == Orcv)
		return n;
	left = commoracle(n->left);
	return left != nil ? left : commoracle(n->right);
}


/* Log only verified module terminals in the original left-before-right order. */
static void
sortoracle(Node *n, Decl **expected, ulong *cursor)
{
	if(n == nil)
		return;
	if(n->op == Oseq){
		sortoracle(n->left, expected, cursor);
		sortoracle(n->right, expected, cursor);
	}else if(n->op == Omoddecl && (n->ty->ok & OKverify)){
		expected[(*cursor)++] = n->ty->ids;
		expected[(*cursor)++] = n->ty->ids->next;
	}
}


/* Cover DAG copies, source overrides, terminal-node rules and exact callback order. */
static void
small(void)
{
	Node input[63], saved[63], expected[4096];
	Type types[63];
	Decl declarations[126], *expectedlog[126];
	Src src;
	ulong expectedused, expectedevents, i;
	int count, pattern, resrc;

	capacity = 4096;
	arena = calloc(capacity, sizeof(*arena));
	eventcapacity = 126;
	events = calloc(eventcapacity, sizeof(*events));
	check(arena != nil && events != nil);
	memset(&src, 0, sizeof(src));
	src.start.line = 123;
	src.stop.line = 456;
	for(count = 1; count <= 63; count++)
	for(pattern = 0; pattern < 16; pattern++){
		memset(input, 0, sizeof(input));
		memset(types, 0, sizeof(types));
		memset(declarations, 0, sizeof(declarations));
		for(i = 0; i < (ulong)count; i++){
			input[i].op = (i+pattern)%5 == 0 ? Osnd : (i+pattern)%7 == 0 ? Orcv : Oadd;
			input[i].ty = &types[i];
			input[i].decl = &declarations[i];
			input[i].val = i*17+pattern;
			input[i].src.start.line = i+1;
			if(2*i+1 < (ulong)count)
				input[i].left = &input[2*i+1];
			if(2*i+2 < (ulong)count)
				input[i].right = &input[2*i+2];
		}
		if(count > 2 && pattern&1)
			input[0].right = input[0].left;
		memmove(saved, input, sizeof(input));
		check(hascomm(input) == commoracle(input) && livework == 0);
		for(resrc = 0; resrc < 3; resrc++){
			expectedused = used = 0;
			copyoracle(input, expected, &expectedused, resrc, resrc == 2 ? &input[0].src : &src);
			check(dupn(resrc, resrc == 2 ? &input[0].src : resrc ? &src : nil, input) == arena);
			check(used == expectedused && livework == 0);
			for(i = 0; i < expectedused; i++){
				if(expected[i].left != nil)
					expected[i].left = arena+(expected[i].left-expected);
				if(expected[i].right != nil)
					expected[i].right = arena+(expected[i].right-expected);
			}
			check(!memcmp(expected, arena, used*sizeof(*arena)));
			check(!memcmp(saved, input, sizeof(input)));
		}
		for(i = 0; i < (ulong)count; i++){
			input[i].op = input[i].left != nil && (i+pattern)%3 ? Oseq : Omoddecl;
			types[i].ok = (i+pattern)%2 ? OKverify : 0;
			types[i].ids = &declarations[2*i];
			declarations[2*i].next = &declarations[2*i+1];
		}
		/* Global ASTs are trees: do not sort the same mutable Type twice. */
		if(count > 2)
			input[0].right = &input[2];
		expectedevents = nevents = 0;
		sortoracle(input, expectedlog, &expectedevents);
		memmove(saved, input, sizeof(input));
		gsort(input);
		check(nevents == expectedevents && livework == 0);
		check(!memcmp(events, expectedlog, nevents*sizeof(*events)));
		check(!memcmp(saved, input, sizeof(input)));
	}
	memset(input, 0, sizeof(input));
	input[0].op = Osnd;
	input[0].left = (Node*)1;
	input[0].right = (Node*)2;
	check(hascomm(input) == input && livework == 0);
	check(hascomm(nil) == nil);
	gsort(nil);
	free(events);
	free(arena);
	puts("PASS node transforms: 3024 preorder copies, 1008 searches, 1008 callback traces");
}


/* Verify a full source/copy image with analytically known preorder indexes. */
static void
copydeep(Node *input, Node *saved, int count, int comb)
{
	Node expected;
	ulong i, total, left, right;

	total = count*(comb ? 2UL : 1UL);
	capacity = total;
	used = 0;
	arena = calloc(total, sizeof(*arena));
	check(arena != nil && dupn(0, nil, input) == arena && used == total);
	for(i = 0; i < total; i++){
		expected = saved[i < (ulong)count ? i : 3*(ulong)count-1-i];
		if(expected.left != nil){
			left = expected.left-input;
			expected.left = arena+left;
		}
		if(expected.right != nil){
			right = expected.right-input;
			expected.right = arena+(comb ? 3*(ulong)count-1-right : right);
		}
		check(!memcmp(&expected, &arena[i], sizeof(expected)));
	}
	check(!memcmp(input, saved, total*sizeof(*input)) && livework == 0);
	free(arena);
}


/* Exercise deep chains and growing pending branches at a 256 KiB C-stack limit. */
static void
deep(char *family)
{
	struct rlimit limit;
	Node *input, *saved;
	Type *types;
	Decl *declarations;
	int count, direction, i;
	ulong before;

	check(getrlimit(RLIMIT_STACK, &limit) == 0);
	limit.rlim_cur = 256*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	count = 100000;
	input = calloc(2*count, sizeof(*input));
	saved = malloc(2*count*sizeof(*saved));
	types = calloc(count, sizeof(*types));
	declarations = calloc(2*count, sizeof(*declarations));
	eventcapacity = 2*count;
	events = calloc(eventcapacity, sizeof(*events));
	check(input != nil && saved != nil && types != nil && declarations != nil && events != nil);
	for(direction = 0; direction < 3; direction++){
		memset(input, 0, 2*count*sizeof(*input));
		for(i = 0; i < 2*count; i++)
			input[i].op = Oadd;
		for(i = 0; i < count; i++){
			if(i+1 < count){
				if(direction == 0)
					input[i].right = &input[i+1];
				else
					input[i].left = &input[i+1];
			}
			if(direction == 2)
				input[i].right = &input[count+i];
		}
		before = workallocations;
		if(strcmp(family, "copy") == 0){
			memmove(saved, input, 2*count*sizeof(*input));
			copydeep(input, saved, count, direction == 2);
		}else if(strcmp(family, "comm") == 0){
			memmove(saved, input, 2*count*sizeof(*input));
			check(hascomm(input) == nil && livework == 0);
			input[count-1].op = Orcv;
			check(hascomm(input) == &input[count-1] && livework == 0);
			input[count-1] = saved[count-1];
			check(!memcmp(saved, input, 2*count*sizeof(*input)));
		}else{
			for(i = 0; i < count; i++){
				input[i].op = Oseq;
				input[count+i].op = Omoddecl;
				input[count+i].ty = &types[i];
				types[i].ok = OKverify;
				types[i].ids = &declarations[2*i];
				declarations[2*i].next = &declarations[2*i+1];
			}
			if(direction < 2)
				input[count-1] = input[count];
			nevents = 0;
			gsort(input);
			check(nevents == (direction == 2 ? 2*(ulong)count : 2) && livework == 0);
			for(i = 0; i < (direction == 2 ? count : 1); i++){
				int index = direction == 2 ? count-1-i : 0;

				check(events[2*i] == &declarations[2*index]);
				check(events[2*i+1] == &declarations[2*index+1]);
			}
		}
		if(direction == 2)
			check(workallocations > before);
	}
	free(events);
	free(declarations);
	free(types);
	free(saved);
	free(input);
	printf("PASS deep %s: right/left/comb traversals, 100000 levels\n", family);
}


/* Isolate each deep regression so the original failing pass remains identifiable. */
int
main(int argc, char **argv)
{
	check(argc == 2);
	if(strcmp(argv[1], "small") == 0)
		small();
	else
		deep(argv[1]);
	return 0;
}
