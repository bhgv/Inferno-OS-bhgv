#include "limbo.h"
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>

static Node arena[4096];
static int used, live, attempts, failat, checks;
static unsigned raisedwarnings, missingwarnings;
static Type exceptiontype, reraisetype;
static Decl declarations[4];
static Sym symbols[4];
static char *names[] = {"e0", "e1", "e2", "e3"};

/* Track all temporary analysis owners independently of LeakSanitizer. */
int __lsan_is_turned_off(void) { return 1; }

/* Fatal allocation errors must use the compiler's diagnostic exit path. */
void
fatal(char *fmt, ...)
{
	assert(fmt != nil);
	_Exit(73);
}

/* Poison-check each raw allocation and inject failures below allocmem. */
static void*
trackedmalloc(ulong n)
{
	void *p;

	if(++attempts == failat)
		return nil;
	p = malloc(n);
	assert(p != nil);
	live++;
	return p;
}

/* Enforce balanced temporary ownership after every AST analysis. */
static void
trackedfree(void *p)
{
	assert(p != nil && live > 0);
	live--;
	free(p);
}

/* Match the compiler's checked allocation convention. */
void*
allocmem(ulong n)
{
	void *p;

	p = trackedmalloc(n);
	if(p == nil)
		fatal("out of memory");
	return p;
}

/* Record diagnostic declaration identities without depending on native formatting. */
void
nwarn(Node *n, char *fmt, ...)
{
	va_list ap;
	char *name;
	int i;

	(void)n;
	va_start(ap, fmt);
	(void)va_arg(ap, Node*);
	name = va_arg(ap, char*);
	va_end(ap);
	for(i = 0; i < 4; i++)
		if(name == names[i])
			break;
	assert(i < 4);
	if(strstr(fmt, "but not declared") != nil){
		assert((raisedwarnings&(1U<<i)) == 0);
		raisedwarnings |= 1U<<i;
	}else{
		assert(strstr(fmt, "but declared") != nil && (missingwarnings&(1U<<i)) == 0);
		missingwarnings |= 1U<<i;
	}
}

#define malloc trackedmalloc
#define free trackedfree
IMPLEMENTATION
#undef malloc
#undef free

/* Allocate AST nodes from bounded fixture storage, separate from analysis ownership. */
static Node*
node(int op, Node *left, Node *right)
{
	Node *n;

	assert(used < nelem(arena));
	n = &arena[used++];
	memset(n, 0, sizeof(*n));
	n->op = op;
	n->left = left;
	n->right = right;
	return n;
}

/* Build a typed literal raise or a variable re-raise that borrows handler qualifiers. */
static Node*
raising(int id)
{
	Node *e;

	e = node(Oname, nil, nil);
	e->ty = id < 0 ? &reraisetype : &exceptiontype;
	e->decl = id < 0 ? nil : &declarations[id];
	return node(Oraise, e, nil);
}

/* Build the real qualifier/body pair and list shape used by case and exception arms. */
static Node*
arm(unsigned qualifiers, int wildcard, Node *body, Node *next)
{
	Node *list, *name;
	int i;

	list = nil;
	if(wildcard)
		list = node(Oseq, node(Owild, nil, nil), nil);
	for(i = 3; i >= 0; i--)
		if(qualifiers&(1U<<i)){
			name = node(Oname, nil, nil);
			name->ty = &exceptiontype;
			name->decl = &declarations[i];
			list = node(Oseq, name, list);
		}
	return node(Oseq, node(Oseq, list, body), next);
}

/* Every analysis result must be a finite, duplicate-free set with independent ownership. */
static unsigned
analyze(Node *n)
{
	Elist *el, *entry, *next;
	unsigned mask;
	int count, i;

	el = raisecheck(n, nil);
	mask = 0;
	count = 0;
	for(entry = el; entry != nil; entry = next){
		assert(++count <= 4);
		next = entry->nxt;
		for(i = 0; i < 4 && entry->d != &declarations[i]; i++)
			;
		assert(i < 4 && (mask&(1U<<i)) == 0);
		mask |= 1U<<i;
		trackedfree(entry);
	}
	assert(live == 0);
	return mask;
}

/* Keep AST storage and allocator observations independent across cases. */
static void
reset(void)
{
	assert(live == 0);
	used = attempts = failat = 0;
	raisedwarnings = missingwarnings = 0;
}

/* Exercise sequence tails, duplicates, handler scopes, caught sets and wildcards. */
static void
examples(void)
{
	Node *body, *handler, *tree;
	int i, op;

	for(i = 0; i < 4; i++){
		reset();
		tree = node(Oseq, raising(i), node(Oseq, raising((i+1)%4), node(Oconst, nil, nil)));
		assert(analyze(tree) == ((1U<<i)|(1U<<((i+1)%4))));
		checks++;
	}
	for(op = 0; op < 3; op++){
		reset();
		body = node(op == 0 ? Ocase : op == 1 ? Oalt : Opick, nil,
			arm(0, 0, raising(-1), arm(0, 0, raising(-1), nil)));
		handler = node(Oexcept, nil, arm(3, 0, body, nil));
		tree = node(Oexstmt, raising(0), handler);
		assert(analyze(tree) == 3);
		checks++;
	}
	reset();
	body = node(Oif, nil, node(Oseq, raising(-1), raising(-1)));
	tree = node(Oexstmt, raising(0), node(Oexcept, nil, arm(3, 0, body, nil)));
	assert(analyze(tree) == 3);
	checks++;
	reset();
	tree = node(Oif, nil, node(Oseq, raising(0), raising(0)));
	assert(analyze(tree) == 1);
	checks++;
	reset();
	tree = node(Oexstmt, node(Oseq, raising(0), raising(1)),
		node(Oexcept, nil, arm(1, 0, raising(2), nil)));
	assert(analyze(tree) == 6);
	checks++;
	reset();
	tree = node(Oexstmt, raising(0), node(Oexcept, nil, arm(3, 1, raising(-1), nil)));
	assert(analyze(tree) == 0);
	checks++;
	for(i = 0; i < 3; i++){
		reset();
		body = raising(0);
		if(i == 0)
			body->left = node(Ocall, body->left, nil);
		else if(i == 1)
			body->left = node(Omdot, nil, body->left);
		else
			body->left = node(Ocall, node(Omdot, nil, body->left), nil);
		body->left->ty = &exceptiontype;
		assert(analyze(body) == 1);
		checks++;
	}
}

/* Deterministic AST choices supply a separate set-valued construction oracle. */
static unsigned
randomword(void)
{
	static unsigned state = 0x4a15e;

	state = state*1664525U+1013904223U;
	return state;
}

/* Return an AST plus its independently computed escaped exception bitset. */
static Node*
generate(int depth, unsigned context, unsigned *expected)
{
	Node *a, *b;
	unsigned ea, eb, caughtmask;
	int choice, id;

	choice = randomword()%7;
	if(depth == 0 || choice < 2){
		id = randomword()%5;
		*expected = id == 4 ? context : 1U<<id;
		return raising(id == 4 ? -1 : id);
	}
	if(choice == 2){
		a = generate(depth-1, context, &ea);
		*expected = ea;
		return node(Oscope, nil, a);
	}
	a = generate(depth-1, context, &ea);
	if(choice == 6){
		caughtmask = randomword()%16;
		b = generate(depth-1, caughtmask, &eb);
		*expected = (ea&~caughtmask)|eb;
		return node(Oexstmt, a, node(Oexcept, nil, arm(caughtmask, 0, b, nil)));
	}
	b = generate(depth-1, context, &eb);
	*expected = ea|eb;
	if(choice == 3)
		return node(Oseq, a, b);
	if(choice == 4)
		return node(Oif, nil, node(Oseq, a, b));
	return node(Ocase, nil, arm(0, 0, a, arm(0, 0, b, nil)));
}

/* Check mixed nested control flow and nearest typed-handler re-raise contexts. */
static void
randomtrees(void)
{
	Node *tree;
	unsigned expected;
	int i;

	for(i = 0; i < 2000; i++){
		reset();
		tree = generate(5, 0, &expected);
		assert(analyze(tree) == expected);
		checks++;
	}
}

/* Public diagnostics must use the complete set and release it after comparison. */
static void
warnings(void)
{
	Node *fn, *declared;
	Type type;
	int mask;

	for(mask = 0; mask < 16; mask++){
		reset();
		memset(&type, 0, sizeof(type));
		declared = arm(mask, 0, nil, nil)->left->left;
		type.u.eraises = node(Oseq, declared, nil);
		fn = node(Ofunc, node(Oname, nil, nil), node(Oseq, raising(0), raising(1)));
		fn->ty = &type;
		checkraises(fn);
		assert(raisedwarnings == (3U&~mask) && missingwarnings == ((unsigned)mask&~3U));
		assert(live == 0);
		checks++;
	}
}

/* Allocator failures must reach fatal rather than dereferencing a nil list node. */
static void
allocation(void)
{
	Node *tree;
	int i, count, status;
	pid_t child;

	reset();
	tree = node(Oexstmt, raising(0), node(Oexcept, nil, arm(3, 0, raising(-1), nil)));
	assert(analyze(tree) == 3);
	count = attempts;
	for(i = 1; i <= count; i++){
		child = fork();
		assert(child >= 0);
		if(child == 0){
			attempts = 0;
			failat = i;
			(void)raisecheck(tree, nil);
			_Exit(74);
		}
		assert(waitpid(child, &status, 0) == child);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
		checks++;
	}
}

/* Run functional, ownership, diagnostic and allocation checks using real compiler layouts. */
int
main(int argc, char **argv)
{
	int i;

	exceptiontype.kind = reraisetype.kind = Texception;
	exceptiontype.cons = 1;
	for(i = 0; i < 4; i++){
		symbols[i].name = names[i];
		declarations[i].sym = &symbols[i];
	}
	if(argc == 1 || strcmp(argv[1], "examples") == 0)
		examples();
	if(argc == 1 || strcmp(argv[1], "random") == 0)
		randomtrees();
	if(argc == 1 || strcmp(argv[1], "warnings") == 0)
		warnings();
	if(argc == 1 || strcmp(argv[1], "allocation") == 0)
		allocation();
	printf("compiler raises: %d set/context/ownership/diagnostic cases passed\n", checks);
	return 0;
}
