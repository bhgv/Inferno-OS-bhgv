#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

typedef int64_t Long;
typedef int64_t vlong;
typedef uint64_t uvlong;
typedef unsigned int uint;
#define nil NULL
#define isNaN(x) isnan(x)
OPS
enum {Tint, Tbyte, Tbig, Treal, Tfix, Tstring, Ttuple, Tadt, Texception};
enum {Dconst, Dtag, Dtype, Dlocal};
typedef struct Type Type;
struct Type { int kind, size, cons; };
typedef struct { int start; } Src;
typedef struct { char *name; int len; } Sym;
typedef struct Decl Decl;
typedef struct Node Node;
struct Decl { Decl *importid; int store, tag; Node *init; Sym *sym; };
struct Node { int op; Type *ty; Node *left, *right; Decl *decl; Src src; Long val; double rval; };
static Type ti = {Tint, 4}, tb = {Tbyte, 1}, tl = {Tbig, 8}, tr = {Treal, 8}, ts = {Tstring, 8};
static Type *tint = &ti, *tbyte = &tb, *tbig = &tl, *treal = &tr, *tstring = &ts;
static int sideeffect[Nops], isrelop[Nops], opcommute[Nops], checks, visits;
static double canonnan;
static Node arena[256];
static int used;
static Node *efold(Node*), *foldc(Node*), *foldr(Node*), *foldvc(Node*);
static int hasside(Node*, int);

/* LeakSanitizer cannot inspect processes here; the fixture uses static storage. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Unexpected compiler fatal errors must terminate the fixture. */
static void
fatal(char *format, ...)
{
	(void)format;
	abort();
}

/* No successful case should require a source-position diagnostic. */
static void
error(int start, char *format, ...)
{
	(void)start;
	(void)format;
	abort();
}

/* Reject unexpected constant-expression diagnostics. */
static void
nerror(Node *n, char *format, ...)
{
	(void)n;
	(void)format;
	abort();
}

/* Tested shift amounts never need compiler warnings. */
static void
nwarn(Node *n, char *format, ...)
{
	(void)n;
	(void)format;
	abort();
}

/* Bounded type models use canonical identity. */
static int
tequal(Type *a, Type *b)
{
	return a == b;
}

/* String-length AST paths are outside this arithmetic fixture. */
static int
utflen(char *p)
{
	(void)p;
	abort();
}

/* Reject string concatenation paths instead of modelling their allocation. */
static Decl*
stringcat(Sym *a, Sym *b)
{
	(void)a;
	(void)b;
	abort();
}

/* No string constant is built by the tested rewrites. */
static Node*
mksconst(Src *src, Decl *d)
{
	(void)src;
	(void)d;
	abort();
}

/* Aggregate constructor rewrites are outside this fixture. */
static Node*
rewrite(Node *n)
{
	(void)n;
	abort();
}

/* No tested expression selects an aggregate member. */
static Node*
tuplemem(Node *n, Decl *d)
{
	(void)n;
	(void)d;
	abort();
}

/* Cast folding has a separate production-path fixture. */
static Node*
foldcast(Node *n, Node *left)
{
	(void)n;
	(void)left;
	abort();
}

/* Allocate bounded nodes for production boolean rewrites. */
static Node*
mkconst(Src *src, Long value)
{
	Node *n;

	assert(used < 256);
	n = &arena[used++];
	*n = (Node){Oconst, tint, nil, nil, nil, *src, value, 0};
	return n;
}

/* Build a binary expression without replacing production recursive folding. */
static Node*
mkbin(int op, Node *left, Node *right)
{
	Node *n;

	assert(used < 256);
	n = &arena[used++];
	*n = (Node){op, left->ty, left, right, nil, left->src, 0, 0};
	return n;
}

IMPLEMENTATION

/* Evaluate the supported expression subset independently of folding helpers. */
static double
evaluate(Node *n)
{
	double a, b;

	if(n->op == Oname || n->op == Ocall) { visits++; return n->rval; }
	if(n->op == Oconst) return n->ty == treal ? n->rval : (double)n->val;
	a = evaluate(n->left);
	if(n->op == Oandand) return a != 0 && evaluate(n->right) != 0;
	if(n->op == Ooror) return a != 0 || evaluate(n->right) != 0;
	b = evaluate(n->right);
	switch(n->op) {
	case Oadd: return a+b;
	case Osub: return a-b;
	case Omul: return a*b;
	case Oeq: return a == b;
	case Oneq: return a != b;
	case Olt: return a < b;
	case Ogt: return a > b;
	case Oleq: return a <= b;
	case Ogeq: return a >= b;
	default: abort();
	}
}

/* Compare exceptional values and signed zeros without equating NaNs to numbers. */
static void
samevalue(double actual, double expected)
{
	if(isnan(expected)) assert(isnan(actual));
	else { assert(actual == expected); if(expected == 0) assert(!!signbit(actual) == !!signbit(expected)); }
}

/* Same-tree real identities must retain NaN/infinity and zero-sign behavior. */
static void
realidentities(void)
{
	double values[] = {NAN, INFINITY, -INFINITY, 0, -0.0, 1, -1, 1e300, 1e-300};
	int ops[] = {Oeq, Oneq, Olt, Ogt, Oleq, Ogeq, Osub};
	Decl d = {nil, Dlocal, 0, nil, nil};
	Node left, right, n;
	double expected;
	unsigned int i, j;

	for(i = 0; i < sizeof(values)/sizeof(values[0]); i++)
		for(j = 0; j < sizeof(ops)/sizeof(ops[0]); j++) {
			left = (Node){Oname, treal, nil, nil, &d, {1}, 0, values[i]};
			right = left;
			n = (Node){ops[j], ops[j] == Osub ? treal : tint, &left, &right, nil, {1}, 0, 0};
			expected = evaluate(&n);
			samevalue(evaluate(efold(&n)), expected);
			checks++;
		}
}

/* Logical identity results are booleans even when an integer operand is not. */
static void
booleans(void)
{
	Decl d = {nil, Dlocal, 0, nil, nil};
	Node left, right, n, *result;
	int value, op, effect, before;
	double expected;

	for(value = -128; value <= 128; value++)
		for(op = 0; op < 2; op++)
			for(effect = 0; effect < 2; effect++) {
				left = (Node){effect ? Ocall : Oname, tint, nil, nil, &d, {1}, 0, value};
				right = left;
				n = (Node){op ? Ooror : Oandand, tint, &left, &right, nil, {1}, 0, 0};

				visits = 0; expected = evaluate(&n); before = visits;
				used = 0; result = efold(&n);
				visits = 0; samevalue(evaluate(result), expected);
				if(effect) assert(visits == before);
				checks++;
			}

}

/* Already-normalized comparisons may still collapse when they have no effects. */
static void
normalizedbooleans(void)
{
	Decl d = {nil, Dlocal, 0, nil, nil};
	Node variable, constant, left, right, n, *result;
	int value, op, effect, before;
	double expected;

	for(value = -128; value <= 128; value++)
		for(op = 0; op < 2; op++)
			for(effect = 0; effect < 2; effect++) {
				variable = (Node){effect ? Ocall : Oname, tint, nil, nil, &d, {1}, 0, value};
				constant = (Node){Oconst, tint, nil, nil, nil, {1}, 5, 0};
				left = (Node){Olt, tint, &variable, &constant, nil, {1}, 0, 0};
				right = left;
				n = (Node){op ? Ooror : Oandand, tint, &left, &right, nil, {1}, 0, 0};
				visits = 0; expected = evaluate(&n); before = visits;
				used = 0; result = efold(&n);
				visits = 0; samevalue(evaluate(result), expected);
				if(effect) assert(visits == before);
				else assert(result == &left);
				checks++;
			}
}

/* Retain exact integer identities while restricting inexact real ones. */
static void
integeridentities(void)
{
	Decl d = {nil, Dlocal, 0, nil, nil};
	Node left, right, n;
	Type *type;
	int value, kind;
	int ops[] = {Oeq, Oneq, Olt, Ogt, Oleq, Ogeq, Osub};
	unsigned int op;
	double expected;

	for(kind = 0; kind < 2; kind++)
		for(value = -32; value <= 32; value++)
			for(op = 0; op < sizeof(ops)/sizeof(ops[0]); op++) {
				type = kind ? tbig : tint;
				left = (Node){Oname, type, nil, nil, &d, {1}, 0, value};
				right = left;
				n = (Node){ops[op], ops[op] == Osub ? type : tint, &left, &right, nil, {1}, 0, 0};
				expected = evaluate(&n);
				samevalue(evaluate(efold(&n)), expected);
				checks++;
			}
}

/* Keep left-associated real arithmetic while retaining exact integer combining. */
static void
association(void)
{
	double triples[][3] = {{1e16, -1e16, 1}, {1e308, 1e-308, 1e-308},
		{-0.0, -0.0, 0.0}, {INFINITY, -INFINITY, 1}, {NAN, 1, 2},
		{1e308, 1e308, -1e308}};
	Decl d = {nil, Dlocal, 0, nil, nil};
	Node variable, a, b, inner, outer;
	double expected;
	unsigned int i;
	int op;

	for(i = 0; i < sizeof(triples)/sizeof(triples[0]); i++)
		for(op = 0; op < 2; op++) {
			variable = (Node){Oname, treal, nil, nil, &d, {1}, 0, triples[i][0]};
			a = (Node){Oconst, treal, nil, nil, nil, {1}, 0, triples[i][1]};
			b = (Node){Oconst, treal, nil, nil, nil, {1}, 0, triples[i][2]};
			inner = (Node){op ? Omul : Oadd, treal, &variable, &a, nil, {1}, 0, 0};
			outer = (Node){op ? Omul : Oadd, treal, &inner, &b, nil, {1}, 0, 0};
			expected = evaluate(&outer);
			samevalue(evaluate(efold(&outer)), expected);
			checks++;
		}
	variable = (Node){Oname, tint, nil, nil, &d, {1}, 0, 10};
	a = (Node){Oconst, tint, nil, nil, nil, {1}, 2, 0};
	b = (Node){Oconst, tint, nil, nil, nil, {1}, 3, 0};
	inner = (Node){Oadd, tint, &variable, &a, nil, {1}, 0, 0};
	outer = (Node){Oadd, tint, &inner, &b, nil, {1}, 0, 0};
	assert(efold(&outer) == &outer && outer.left == &variable && outer.right->op == Oconst && outer.right->val == 5);
	checks++;
	/* Fixed-point addition/multiplication must not enter the integer reassociator. */
	{
		Type fixed = {Tfix, 4};
		variable.ty = a.ty = b.ty = inner.ty = outer.ty = &fixed;
		inner.left = &variable; inner.right = &a;
		outer.left = &inner; outer.right = &b;
		assert(efold(&outer) == &outer && outer.left == &inner && outer.right == &b);
		checks++;
	}

}

/* Exercise real and boolean identities plus recursive constant reassociation. */
int
main(void)
{
	canonnan = NAN;
	sideeffect[Ocall] = 1;
	isrelop[Oeq] = isrelop[Oneq] = isrelop[Olt] = isrelop[Ogt] = isrelop[Oleq] = isrelop[Ogeq] = 1;
	opcommute[Oadd] = Oadd; opcommute[Omul] = Omul;
	realidentities();
	booleans();
	normalizedbooleans();
	integeridentities();
	association();
	printf("PASS compiler identities: %d IEEE/boolean/effect/reassociation cases\n", checks);
	return 0;
}
