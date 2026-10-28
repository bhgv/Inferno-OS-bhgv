#include "limbo.h"
#include <stdio.h>
#include <setjmp.h>
#include <sys/resource.h>

#define check(c) predicate(!!(c), __LINE__) // Keep traversal and metadata checks under NDEBUG.
#define Depth 100000 // Deep inputs must not depend on the C call stack.

int scope; // Actual declaration installation scope used by production code.
int storespace[Dend] = { [Dlocal] = 1, [Dglobal] = 1 }; // Storage classes needed by idoffsets.
static Decl *scopes[MaxScope], *tails[MaxScope]; // Actual scope list heads and tails.
static Decl nilvalue; // Borrowed nil identifier declaration.
Decl *nildecl = &nilvalue; // Compiler nil-name identity.
static Sym symbols[8]; // Repeated identifiers expose install/redeclaration order.
static Decl initial[8]; // Borrowed outer-scope declarations.
static Node arena[32768]; // Shallow input trees.
static int nnodes; // Current shallow node allocation cursor.
static Decl *owned[Depth+4096]; // Result declarations owned until the attempt ends.
static int nowned; // Number of actual mkdecl allocations.
static void *privateblocks[64]; // Live traversal stack allocations.
static int nprivate; // Private ownership independent of persistent declarations.
static uvlong events; // Ordered warning/error/constructor callback trace.
static uint randomstate; // Reproducible tree generator state.
static ulong scenarios; // Completed independent scenarios.
static int expectfatal; // Only the explicit overflow probe expects a fatal.
static jmp_buf rejection; // Recover from the expected count rejection.


/* Fail explicitly on incorrect order, metadata, ownership or output. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "declaration walk check at %d\n", line);
		abort();
	}
}


/* Keep ASan access checks without process-wide leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Permit only the deliberate exhausted signed-counter diagnostic. */
void
fatal(char *message, ...)
{
	if(expectfatal)
		longjmp(rejection, 1);
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Mix exact callback and state values in a stable order. */
static uvlong
mix(uvlong hash, vlong value)
{
	return (hash^(uvlong)value)*1099511628211ULL;
}


/* Record actual redeclaration callbacks without depending on diagnostic text formatting. */
void
error(Line line, char *format, ...)
{
	va_list args;
	Decl *d, *old;

	USED(format);
	va_start(args, format);
	d = va_arg(args, Decl*);
	old = va_arg(args, Decl*);
	va_end(args);
	events = mix(mix(mix(events, 1), line.line), d->src.start.line);
	events = mix(events, old->src.start.line);
}


/* Preserve the position of each shadowing warning among installation actions. */
void
warn(Line line, char *format, ...)
{
	va_list args;
	Decl *d, *old;

	USED(format);
	va_start(args, format);
	d = va_arg(args, Decl*);
	old = va_arg(args, Decl*);
	va_end(args);
	events = mix(mix(mix(events, 2), line.line), d->src.start.line);
	events = mix(events, old->src.start.line);
}


/* Capture invalid := left sides while allowing later eager declarations to run. */
void
nerror(Node *n, char *format, ...)
{
	USED(format);
	events = mix(mix(events, 3), n->src.start.line);
}


/* Use real declaration constructors with separately tracked result ownership. */
void*
allocmem(ulong bytes)
{
	void *p;

	check(bytes == sizeof(Decl) && nowned < nelem(owned));
	p = malloc(bytes);
	check(p != nil);
	owned[nowned++] = p;
	return p;
}


/* Track only private traversal storage, never the installed declarations. */
static void*
workalloc(ulong bytes)
{
	void *p;

	check(nprivate < nelem(privateblocks));
	p = malloc(bytes);
	check(p != nil);
	privateblocks[nprivate++] = p;
	return p;
}


/* Reject lost or incorrectly freed traversal blocks. */
static void
workfree(void *p)
{
	int i;

	for(i = 0; i < nprivate; i++)
		if(privateblocks[i] == p){
			privateblocks[i] = privateblocks[--nprivate];
			free(p);
			return;
		}
	check(0);
}


/* Types in the layout probe are already sized and have no additional side effects. */
void
usedty(Type *t)
{
	check(t != nil);
}

SUPPORT

#define allocmem workalloc // Observe only private traversal work.
#define free workfree // Keep result declarations installed until test reset.
IMPLEMENTATION
#undef free
#undef allocmem

REFERENCE


/* Reset real scope installation state between current and frozen traversals. */
static void
reset(int local)
{
	int i;

	check(nprivate == 0);
	for(i = 0; i < nowned; i++)
		free(owned[i]);
	nowned = 0;
	memset(scopes, 0, sizeof(scopes));
	memset(tails, 0, sizeof(tails));
	memset(initial, 0, sizeof(initial));
	memset(symbols, 0, sizeof(symbols));
	memset(&nilvalue, 0, sizeof(nilvalue));
	nilvalue.src.start.line = -100;
	scope = ScopeGlobal+local;
	for(i = 0; i < nelem(symbols); i++){
		symbols[i].decl = &initial[i];
		initial[i].sym = &symbols[i];
		initial[i].src.start.line = -i-1;
		initial[i].store = i%2 ? Dglobal : Dfn;
		initial[i].nid = 1;
		initial[i].scope = ScopeBuiltin;
	}
	nnodes = 0;
	events = 1469598103934665603ULL;
}


/* Allocate an immutable shallow AST node with a stable source identity. */
static Node*
node(int op)
{
	Node *n;

	check(nnodes < nelem(arena));
	n = &arena[nnodes++];
	memset(n, 0, sizeof(*n));
	n->op = op;
	n->src.start.line = nnodes;
	return n;
}


/* Generate nested declaration tuples, empty tuples, nil names and invalid members. */
static Node*
tuple(int depth)
{
	Node *n, *list, **tail;
	int i, count;

	randomstate = randomstate*1664525+1013904223;
	count = (randomstate>>16)%6;
	if(depth && count < 3){
		n = node(Otuple);
		tail = &n->left;
		for(i = 0; i < count; i++){
			list = node(Oseq);
			*tail = list;
			list->left = tuple(depth-1);
			tail = &list->right;
		}
		return n;
	}
	n = node(count == 5 ? Oconst : Oname);
	n->decl = count == 4 ? nildecl : &initial[(randomstate>>24)%8];
	return n;
}


/* Mix ordinary expression order with right-before-left nested global := actions. */
static Node*
expression(int depth)
{
	Node *n;
	int assign;

	if(depth == 0)
		return tuple(1);
	randomstate = randomstate*1664525+1013904223;
	assign = (randomstate>>16)&1;
	n = node(assign ? Odas : Oadd);
	n->left = assign ? tuple(3) : expression(depth-1);
	n->right = expression(depth-1);
	return n;
}


/* Capture all observable declaration state and borrowed AST links after a traversal. */
static uvlong
snapshot(void)
{
	uvlong h;
	Decl *d;
	int i;

	h = mix(events, nowned);
	for(i = 0; i < nowned; i++){
		d = owned[i];
		h = mix(mix(mix(h, d->src.start.line), d->store), d->nid);
		h = mix(mix(mix(h, d->refs), d->das), d->scope);
		h = mix(h, d->old == nil ? 0 : d->old->src.start.line);
		h = mix(h, d->next == nil ? 0 : d->next->src.start.line);
	}
	for(i = 0; i < nnodes; i++){
		h = mix(mix(h, arena[i].op), arena[i].decl == nil ? 0 : arena[i].decl->src.start.line);
		h = mix(h, arena[i].left == nil ? 0 : arena[i].left->src.start.line);
		h = mix(h, arena[i].right == nil ? 0 : arena[i].right->src.start.line);
	}
	for(i = 0; i < nelem(symbols); i++){
		h = mix(h, symbols[i].decl->src.start.line);
		h = mix(h, initial[i].nid);
	}
	h = mix(h, nildecl->nid);
	h = mix(h, scopes[scope] == nil ? 0 : scopes[scope]->src.start.line);
	h = mix(h, tails[scope] == nil ? 0 : tails[scope]->src.start.line);
	check(nprivate == 0);
	return h;
}


/* Compare actual constructors, warnings, eager failures, group marks and consumer state. */
static uvlong
attempt(uint seed, int mode, int local, int reference)
{
	Node *n;
	Decl *last;
	int result;

	reset(local);
	randomstate = seed;
	n = mode == 2 ? expression(4) : tuple(5);
	last = nil;
	if(mode == 0)
		result = reference ? reference_dasdecl(n) : dasdecl(n);
	else if(mode == 1)
		result = reference ? reference_recmark(n, 17) : recmark(n, 17);
	else if(mode == 2){
		if(reference)
			reference_gdasdecl(n);
		else
			gdasdecl(n);
		result = 0;
	}else{
		if(reference){
			reference_dasdecl(n);
			result = reference_tupblk0(n, &last);
		}else{
			dasdecl(n);
			result = tupblk0(n, &last);
		}
	}
	return mix(mix(snapshot(), result), last == nil ? 0 : last->src.start.line);
}


/* Independently verify eager invalid-member handling, nil rules and terminal names. */
static void
edgecases(void)
{
	Node n[7];
	int variant, i;

	for(variant = 0; variant < 4; variant++){
		reset(1);
		memset(n, 0, sizeof(n));
		n[0].op = Otuple;
		n[0].left = &n[1];
		for(i = 0; i < 3; i++){
			n[1+i].op = Oseq;
			n[1+i].left = &n[4+i];
			n[1+i].right = i == 2 ? nil : &n[2+i];
			n[4+i].op = Oname;
			n[4+i].decl = &initial[i];
		}
		if(variant == 0){
			n[4].op = Oconst;
			check(!dasdecl(n) && nowned == 2);
			check(n[5].decl == owned[0] && n[6].decl == owned[1]);
			check(owned[0]->nid == 1 && owned[1]->nid == 1);
		}else if(variant == 1){
			n[5].decl = nildecl;
			check(dasdecl(n) && nowned == 2);
			check(owned[0]->nid == 1 && owned[1]->nid == 1);
		}else if(variant == 2){
			n[4].left = &n[5];
			n[4].right = &n[6];
			check(dasdecl(&n[4]) && nowned == 1);
			check(n[5].decl == &initial[1] && n[6].decl == &initial[2]);
		}else{
			n[0].left = nil;
			check(dasdecl(n) && nowned == 0);
		}
		for(i = 0; i < nowned; i++)
			check(owned[i]->refs == 1 && owned[i]->das == 1 && owned[i]->store == Dlocal);
		check(nprivate == 0);
		scenarios++;
	}
}


/* Build a deeply nested tuple with deferred right siblings at every level. */
static Node*
deepnodes(Node **owner, Decl **declowner)
{
	Node *nodes;
	Decl *decls;
	int i;

	nodes = calloc(3*Depth+1, sizeof(*nodes));
	decls = calloc(Depth+1, sizeof(*decls));
	check(nodes != nil && decls != nil);
	for(i = 0; i < Depth; i++){
		nodes[3*i].op = Otuple;
		nodes[3*i].left = &nodes[3*i+1];
		nodes[3*i+1].op = Oseq;
		nodes[3*i+1].left = i+1 == Depth ? &nodes[3*Depth] : &nodes[3*i+3];
		nodes[3*i+1].right = &nodes[3*i+2];
		nodes[3*i+2].op = Oseq;
		/* The list tail borrows a separate name allocated below. */
	}
	*owner = nodes;
	*declowner = decls;
	return nodes;
}


/* Exercise deep declaration and marking traversal with distinct name identities. */
static void
deepcase(int mode)
{
	Node *nodes, *names, *root;
	Decl *decls, *last;
	int i, nid;

	reset(1);
	root = deepnodes(&nodes, &decls);
	names = calloc(Depth, sizeof(*names));
	check(names != nil);
	for(i = 0; i <= Depth; i++){
		decls[i].sym = &symbols[i%8];
		decls[i].nid = 1;
		if(i < Depth){
			names[i].op = Oname;
			names[i].decl = &decls[i];
			nodes[3*i+2].left = &names[i];
		}
	}
	nodes[3*Depth].op = Oname;
	nodes[3*Depth].decl = &decls[Depth];
	if(mode == 0){
		check(dasdecl(root));
		check(nowned == Depth+1 && nodes[3*Depth].decl->nid == Depth+1);
	}else if(mode == 1){
		check(recmark(root, Depth+1) == 0);
		check(decls[Depth].nid == Depth+1);
		for(i = 0; i < Depth; i++)
			check(decls[i].nid == 0);
	}else{
		decls[Depth].nid = Depth+1;
		last = &decls[Depth];
		for(i = Depth-1; i >= 0; i--){
			last->next = &decls[i];
			last = &decls[i];
			decls[i].nid = 0;
		}
		last = nil;
		check(tupblk0(root, &last) && last == &decls[0]);
		/* Failure after deeply deferred siblings must also release private storage. */
		nid = names[Depth/2].op;
		names[Depth/2].op = Oconst;
		last = nil;
		check(!tupblk0(root, &last) && nprivate == 0);
		names[Depth/2].op = nid;
	}
	check(nprivate == 0);
	free(names);
	free(nodes);
	free(decls);
	scenarios++;
}


/* Check a long global assignment chain preserves right-before-left installation. */
static void
globalcase(void)
{
	Node *nodes;
	int i;

	reset(0);
	nodes = calloc(2*Depth+1, sizeof(*nodes));
	check(nodes != nil);
	for(i = 0; i < Depth; i++){
		nodes[2*i].op = Odas;
		nodes[2*i].left = &nodes[2*i+1];
		nodes[2*i].right = i+1 == Depth ? nil : &nodes[2*i+2];
		nodes[2*i+1].op = Oname;
		nodes[2*i+1].decl = &initial[i%8];
		nodes[2*i+1].src.start.line = i+1;
	}
	gdasdecl(nodes);
	check(nowned == Depth && nprivate == 0);
	for(i = 0; i < Depth; i++)
		check(owned[i]->src.start.line == Depth-i && owned[i]->store == Dglobal);
	free(nodes);
	scenarios++;
}


/* Validate full-width group heads and real frame layout around the old byte boundary. */
static void
groupcase(void)
{
	static int counts[] = { 2, 255, 256, 257, 511, 512, 513, 1000 };
	Node *nodes;
	Decl *first;
	Type scalar, wide;
	int i, k, count;

	memset(&scalar, 0, sizeof(scalar));
	memset(&wide, 0, sizeof(wide));
	scalar.size = scalar.align = 4;
	wide.size = wide.align = 8;
	for(k = 0; k < nelem(counts); k++){
		reset(1);
		count = counts[k];
		nodes = calloc(2*count+1, sizeof(*nodes));
		check(nodes != nil);
		nodes[0].op = Otuple;
		nodes[0].left = &nodes[1];
		for(i = 0; i < count; i++){
			nodes[1+2*i].op = Oseq;
			nodes[1+2*i].left = &nodes[2+2*i];
			nodes[1+2*i].right = i+1 == count ? nil : &nodes[3+2*i];
			nodes[2+2*i].op = Oname;
			nodes[2+2*i].decl = &initial[i%8];
		}
		check(dasdecl(nodes) && nowned == count);
		first = owned[0];
		for(i = 0; i < count; i++){
			owned[i]->ty = i == count-1 ? &wide : &scalar;
			if(i)
				check(owned[i]->nid == 0);
		}
		idoffsets(first, 4, 8);
		check(first->offset == 8);
		check(first->nid == count);
		free(nodes);
		scenarios++;
	}
}


/* Reject an exhausted declaration counter before signed arithmetic can overflow. */
static void
overflowcase(void)
{
	Node n;
	int nid;

	reset(1);
	memset(&n, 0, sizeof(n));
	n.op = Oname;
	n.decl = &initial[0];
	nid = 0x7fffffff;
	expectfatal = 1;
	if(setjmp(rejection) == 0){
		recdasdecl(&n, Dlocal, &nid);
		check(0);
	}
	expectfatal = 0;
	check(nprivate == 0);
	scenarios++;
}


/* Run shallow semantic oracles and independent deep/boundary regressions. */
int
main(int argc, char **argv)
{
	struct rlimit limit;
	uvlong expected;
	int i, mode, local;

	check(argc == 2);
	limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	if(strcmp(argv[1], "shallow") == 0){
		for(i = 0; i < 1000; i++)
			for(mode = 0; mode < 4; mode++)
				for(local = 0; local < 2; local++){
					expected = attempt(i+1, mode, local, 1);
					check(attempt(i+1, mode, local, 0) == expected);
					scenarios++;
				}
	}else if(strcmp(argv[1], "edges") == 0)
		edgecases();
	else if(strcmp(argv[1], "declare") == 0)
		deepcase(0);
	else if(strcmp(argv[1], "mark") == 0)
		deepcase(1);
	else if(strcmp(argv[1], "block") == 0)
		deepcase(2);
	else if(strcmp(argv[1], "global") == 0)
		globalcase();
	else if(strcmp(argv[1], "groups") == 0)
		groupcase();
	else if(strcmp(argv[1], "overflow") == 0)
		overflowcase();
	else
		check(0);
	reset(0);
	printf("declaration walks WORD%d %s: %lu scenarios\n", 8*IBY2WD, argv[1], scenarios);
	return 0;
}
