#include "limbo.h"
#include <stdio.h>
#include <sys/resource.h>

#define check(c) predicate(!!(c), __LINE__) // Keep result, ordering and ownership checks under NDEBUG.
#define Depth 100000 // Deep ASTs must not consume a C frame per edge.

static Node forest[8192], saved[8192]; // Borrowed shallow graphs and immutable alias snapshots.
static int used; // Number of shallow nodes in the current graph.
static Type valuetype; // Borrowed value layout copied into optimizer LEA metadata.
static Decl declarations[8], nilvalue; // Stable declaration identities for alias checks.
Decl *nildecl = &nilvalue; // Compiler nil-name identity used by receive assignment handling.
static Node communication, channel, value, temporary, slotnode, injected; // Shared communication operands.
static Inst emitted[4096]; // Actual instruction records receiving optimizer metadata.
static int calls[7]; // Exact callback counts for independent fast-path checks.
static int nemitted; // Number of LEA records returned by the generator fixture.
static void *workblocks[64]; // Private traversal allocations independent of borrowed ASTs.
static int nwork; // Number of live continuation blocks.
static uint randomstate; // Reproducible mixed AST generator state.
static uvlong trace; // Ordered callback identities and argument positions.
static Node **mutationcell; // Cell changed by the first generator callback in a mutation case.
static Node *mutationvalue; // Replacement read or overwritten at the historical return point.
static ulong scenarios; // Completed differential and independent scenarios.


/* Diagnose result, callback-order, source-mutation or ownership differences. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "communication alias check at %d\n", line);
		abort();
	}
}


/* Retain access instrumentation without process-wide leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* No bounded valid fixture graph should require a compiler fatal. */
void
fatal(char *message, ...)
{
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Own only private traversal blocks, never the borrowed source Nodes. */
void*
allocmem(ulong bytes)
{
	void *p;

	check(nwork < nelem(workblocks) && bytes > 0 && bytes < 128*1024*1024);
	p = malloc(bytes);
	check(p != nil);
	workblocks[nwork++] = p;
	return p;
}


/* Require cleanup after complete traversal and every successful short circuit. */
static void
workfree(void *p)
{
	int i;

	for(i = 0; i < nwork; i++)
		if(workblocks[i] == p){
			workblocks[i] = workblocks[--nwork];
			free(p);
			return;
		}
	check(0);
}


/* Stable identities distinguish shared operands even after their source positions change. */
static int
identity(Node *n)
{
	if(n == nil) return 0;
	if(n == &communication) return 100001;
	if(n == &channel) return 100002;
	if(n == &value) return 100003;
	if(n == &temporary) return 100004;
	if(n == &slotnode) return 100005;
	if(n == &injected) return 100006;
	return n->src.start.line;
}


/* Keep callback order and argument identity independently of output-tree fingerprints. */
static void
event(int kind, Node *n)
{
	check(kind > 0 && kind < nelem(calls));
	calls[kind]++;
	trace = (trace^(uvlong)kind)*1099511628211ULL;
	trace = (trace^(uvlong)identity(n))*1099511628211ULL;
}


/* Addressability is controlled; the real rewrite decides when to consult it. */
Node*
sumark(Node *n)
{
	check(n != nil);
	event(1, n);
	return n;
}


/* Model only the allocation callback contract, leaving operand selection to production. */
Node*
talloc(Node *n, Type *t, Node *candidate)
{
	check(n == &temporary && t == &valuetype && candidate == nil);
	event(2, n);
	n->op = Oname;
	n->ty = t;
	n->decl = &declarations[7];
	return n;
}


/* Observe evaluation order and the captured right operand of a nonaddressable send. */
Node*
ecom(Src *src, Node *target, Node *source)
{
	check(target == &temporary && source == &value && src == &value.src);
	event(3, source);
	return target;
}


/* Observe send temporary cleanup without linking the separate lifetime implementation. */
void
tfreenow(void)
{
	event(4, &temporary);
}


/* Observe the receive temporary hold release at its original callback point. */
void
trelease(Node *n)
{
	check(n == &temporary);
	event(5, n);
}


/* Return real Inst storage and optionally mutate a borrowed AST cell during generation. */
Inst*
genrawop(Src *src, int op, Node *left, Node *middle, Node *right)
{
	Inst *p;

	check(src == &channel.src && op == ILEA && middle == nil && right == &slotnode);
	check(nemitted < nelem(emitted));
	event(6, left);
	p = &emitted[nemitted++];
	memset(p, 0, sizeof(*p));
	if(mutationcell != nil){
		*mutationcell = mutationvalue;
		mutationcell = nil;
	}
	return p;
}

#define free workfree // Observe only production-owned traversal continuations.
IMPLEMENTATION
#undef free

REFERENCE


/* Reset callbacks, graph owners and operand identities for an independent attempt. */
static void
reset(int send, int addressable)
{
	check(nwork == 0);
	memset(forest, 0, sizeof(forest));
	memset(&communication, 0, sizeof(communication));
	memset(&channel, 0, sizeof(channel));
	memset(&value, 0, sizeof(value));
	memset(&temporary, 0, sizeof(temporary));
	memset(&slotnode, 0, sizeof(slotnode));
	memset(&injected, 0, sizeof(injected));
	memset(&valuetype, 0, sizeof(valuetype));
	used = nemitted = 0;
	memset(calls, 0, sizeof(calls));
	mutationcell = nil;
	trace = 1469598103934665603ULL;
	valuetype.size = 32;
	communication.op = send ? Osnd : Orcv;
	communication.ty = value.ty = temporary.ty = &valuetype;
	communication.left = &channel;
	communication.right = &value;
	communication.src.start.line = -10;
	value.op = Oname;
	value.addable = addressable ? Rreg : Rcant;
	value.decl = &declarations[1];
	injected.op = Oname;
	injected.ty = &valuetype;
}


/* Allocate a shallow source node with an immutable traversal identity. */
static Node*
newnode(int op)
{
	Node *n;

	check(used < nelem(forest));
	n = &forest[used++];
	n->op = op;
	n->src.start.line = used;
	n->ty = &valuetype;
	n->addable = used%2 ? Rreg : Rcant;
	n->decl = used%5 == 0 ? nildecl : &declarations[used%8];
	return n;
}


/* Mix shared communication nodes, receives in assignments and name metadata children. */
static Node*
makeforest(int depth)
{
	Node *n;
	int choice;

	randomstate = randomstate*1664525+1013904223;
	choice = (randomstate>>16)%7;
	if(choice == 0) return &communication;
	n = newnode(choice == 1 ? Oas : choice < 4 ? Oname : Oadd);
	if(depth){
		n->left = makeforest(depth-1);
		n->right = makeforest(depth-1);
	}else if(n->op == Oas){
		n->left = &value;
		n->right = &communication;
	}
	return n;
}


/* Capture complete output links, callback order, temporary source and optimizer fields. */
static uvlong
fingerprint(Node *result)
{
	uvlong h;
	int i;

	h = trace^(uvlong)identity(result);
	for(i = 0; i < used; i++){
		h = (h^(uvlong)identity(forest[i].left))*1099511628211ULL;
		h = (h^(uvlong)identity(forest[i].right))*1099511628211ULL;
	}
	h = (h^(uvlong)nemitted)*1099511628211ULL;
	for(i = 0; i < nemitted; i++){
		h = (h^(uvlong)emitted[i].m.offset)*1099511628211ULL;
		h = (h^(uvlong)emitted[i].m.reg)*1099511628211ULL;
	}
	check(nwork == 0);
	return (h^(uvlong)temporary.src.start.line)*1099511628211ULL;
}


/* Compare query answers/source preservation and exact shallow rewrite action order. */
static uvlong
attempt(int seed, int send, int addressable, int original)
{
	Node *n, *e, *result;
	int one, many;

	reset(send, addressable);
	randomstate = seed+1;
	n = makeforest(5);
	e = makeforest(4);
	memmove(saved, forest, sizeof(forest));
	if(original){
		one = reference_tupaliased(&value, e);
		many = reference_tupsaliased(n, e);
	}else{
		one = tupaliased(&value, e);
		many = tupsaliased(n, e);
	}
	check(memcmp(saved, forest, sizeof(forest)) == 0 && nwork == 0);
	result = original ? reference_rewritecomm(n, &communication, &temporary, &slotnode)
		: rewritecomm(n, &communication, &temporary, &slotnode);
	return fingerprint(result)^(uvlong)(one+2*many);
}


/* Preserve delayed sibling reads and captured return values when callbacks mutate AST cells. */
static uvlong
mutation(int send, int addressable, int variant, int original)
{
	Node *root, *child, *result;

	reset(send, addressable);
	root = newnode(Oadd);
	child = newnode(Oadd);
	root->left = child;
	child->left = &communication;
	if(variant == 0){
		mutationcell = &root->right;
		mutationvalue = &communication;
	}else if(variant == 1){
		mutationcell = &root->left;
		mutationvalue = &injected;
	}else{
		child->right = &communication;
		mutationcell = &child->right;
		mutationvalue = &injected;
	}
	result = original ? reference_rewritecomm(root, &communication, &temporary, &slotnode)
		: rewritecomm(root, &communication, &temporary, &slotnode);
	check(result == root && root->left == child);
	check(nemitted == (variant == 0 ? 2 : 1));
	if(variant == 2) check(child->right == &injected);
	return fingerprint(result);
}


/* Verify deep left paths, early hits and remaining sibling cleanup without recursive frames. */
static void
deepcase(int mode)
{
	Node *chain, leaf, name, expression, *result;
	int i, pass;

	reset(0, 0);
	chain = calloc(Depth, sizeof(*chain));
	check(chain != nil);
	memset(&leaf, 0, sizeof(leaf));
	memset(&name, 0, sizeof(name));
	memset(&expression, 0, sizeof(expression));
	leaf.op = name.op = expression.op = Oname;
	leaf.decl = &declarations[2];
	name.decl = &declarations[1];
	expression.decl = &declarations[3];
	for(i = 0; i < Depth; i++){
		chain[i].op = Oadd;
		chain[i].left = i+1 == Depth ? (mode == 0 ? &communication : &leaf) : &chain[i+1];
		chain[i].right = mode == 0 ? nil : &value;
	}
	if(mode == 0){
		result = rewritecomm(chain, &communication, &temporary, &slotnode);
		check(result == chain && chain[Depth-1].left == &temporary && nemitted == 1);
		check(emitted[0].m.offset == valuetype.size && emitted[0].m.reg == 0);
		scenarios++;
	}else
		for(pass = 0; pass < 2; pass++){
			if(mode == 1){
				name.decl = pass ? leaf.decl : &declarations[3];
				check(tupaliased(&name, chain) == pass);
			}else{
				expression.decl = pass ? leaf.decl : &declarations[3];
				check(tupsaliased(chain, &expression) == pass);
			}
			check(nwork == 0);
			scenarios++;
		}
	for(i = 0; i < Depth; i++){
		check(chain[i].op == Oadd);
		check(chain[i].right == (mode == 0 ? nil : &value));
	}
	check(nwork == 0);
	free(chain);
}


/* Cover null queries, matching nil declarations and metadata children of mismatched names. */
static void
edges(void)
{
	Node n, e, child;

	reset(0, 0);
	memset(&n, 0, sizeof(n)); memset(&e, 0, sizeof(e)); memset(&child, 0, sizeof(child));
	check(tupaliased(nil, nil) == 0 && tupsaliased(nil, &e) == 0); scenarios++;
	n.op = e.op = child.op = Oname;
	check(tupaliased(&n, &e) == 1 && tupsaliased(&n, &e) == 1); scenarios++;
	n.decl = child.decl = &declarations[1]; e.decl = &declarations[2]; e.left = &child;
	check(tupaliased(&n, &e) == 1); scenarios++;
	e.left = nil; n.decl = &declarations[3]; n.right = &child; e.decl = child.decl;
	check(tupsaliased(&n, &e) == 1); scenarios++;
	check(rewritecomm(nil, &communication, &temporary, &slotnode) == nil && nemitted == 0); scenarios++;
	reset(1, 1);
	check(rewritecomm(&communication, &communication, &temporary, &slotnode) == &value);
	check(nemitted == 1 && calls[1] == 1 && calls[2] == 0 && emitted[0].m.reg == 1);
	check(emitted[0].m.offset == valuetype.size); scenarios++;
	reset(1, 0);
	check(rewritecomm(&communication, &communication, &temporary, &slotnode) == &temporary);
	check(calls[2] == 1 && calls[3] == 1 && calls[4] == 1 && calls[5] == 0);
	check(temporary.src.start.line == communication.src.start.line && emitted[0].m.reg == 1); scenarios++;
	reset(0, 0);
	check(rewritecomm(&communication, &communication, &temporary, &slotnode) == &temporary);
	check(calls[1] == 0 && calls[2] == 1 && calls[5] == 1 && emitted[0].m.reg == 0); scenarios++;
	reset(0, 1);
	memset(&n, 0, sizeof(n)); n.op = Oas; n.left = &value; n.right = &communication;
	check(rewritecomm(&n, &communication, &temporary, &slotnode) == &value);
	check(calls[1] == 1 && calls[2] == 0 && calls[5] == 0 && nemitted == 1); scenarios++;
	reset(0, 1);
	value.decl = nildecl;
	memset(&n, 0, sizeof(n)); n.op = Oas; n.left = &value; n.right = &communication;
	check(rewritecomm(&n, &communication, &temporary, &slotnode) == &n);
	check(n.left == &value && n.right == &temporary && calls[2] == 1 && calls[5] == 1); scenarios++;

}


/* Run ordinary differential checks and independent callback/depth regressions. */
int
main(int argc, char **argv)
{
	struct rlimit limit;
	uvlong expected;
	int i, send, addressable, variant;

	check(argc == 2);
	limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	if(strcmp(argv[1], "shallow") == 0){
		for(i = 0; i < 2000; i++)
			for(send = 0; send < 2; send++)
				for(addressable = 0; addressable < 2; addressable++){
					expected = attempt(i, send, addressable, 1);
					check(attempt(i, send, addressable, 0) == expected);
					scenarios++;
				}
	}else if(strcmp(argv[1], "mutation") == 0){
		for(send = 0; send < 2; send++)
			for(addressable = 0; addressable < 2; addressable++)
				for(variant = 0; variant < 3; variant++){
					expected = mutation(send, addressable, variant, 1);
					check(mutation(send, addressable, variant, 0) == expected);
					scenarios++;
				}
	}else if(strcmp(argv[1], "rewrite") == 0)
		deepcase(0);
	else if(strcmp(argv[1], "alias") == 0)
		deepcase(1);
	else if(strcmp(argv[1], "tuple") == 0)
		deepcase(2);
	else if(strcmp(argv[1], "edges") == 0)
		edges();
	else
		check(0);
	printf("comm alias WORD%d %s: %lu scenarios\n", IBY2WD*8, argv[1], scenarios);
	return 0;
}
