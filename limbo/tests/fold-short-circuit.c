#include <setjmp.h>
#if WORD_BITS == 32
typedef int32_t WORD;
typedef uint32_t UWORD;
#else
typedef int64_t WORD;
typedef uint64_t UWORD;
#endif
typedef int64_t LONG;
typedef uint64_t ULONG;
typedef unsigned char BYTE;
typedef uintptr_t uintptr; // Native decoded-slot arithmetic.
#define H ((void*)-1) // VM nil-owner sentinel.
static struct {void *s, *m, *d;} R;
static char exInval[] = "invalid", exZdiv[] = "zero", exBounds[] = "bounds";
static jmp_buf boundary;
static char *raised;

/* Keep runtime exceptions independent from compile-time expression diagnostics. */
static void
vmerror(char *message)
{
	raised = message;
	longjmp(boundary, 1);
}

#define OP(name) static void name(void)
#define W(r) (*(WORD*)R.r)
#define UW(r) (*(UWORD*)R.r)
#define B(r) (*(BYTE*)R.r)
#define V(r) (*(LONG*)R.r)
#define UV(r) (*(ULONG*)R.r)
SHORT_VM

/* Stable VM-style slots avoid abandoning stack-backed operand storage on error. */
static struct EvalFrame {
	Long a, b, out;
	WORD wa, wb, wo;
	BYTE ba, bb, bo;
} slots[32];
static int depth, phase;
static Long execute(Node*);

/* Execute lazy control flow and actual native arithmetic bodies, rather than folder helpers. */
static Long
executeframe(Node *n, struct EvalFrame *f)
{
	if(n->op == Oconst)
		return n->val;
	if(n->op == Ocall)
		return execute(n->left->decl->init);
	if(n->op == Oname){
		visits++;
		return n->val;
	}
	f->a = execute(n->left);
	if(n->op == Oandand)
		return f->a != 0 && execute(n->right) != 0;
	if(n->op == Ooror)
		return f->a != 0 || execute(n->right) != 0;
	if(n->op == Oif)
		return execute(f->a ? n->right->left : n->right->right);
	f->b = execute(n->right);
	if(n->op == Oeq)return f->a == f->b;
	if(n->op == Oneq)return f->a != f->b;
	f->wa = f->a; f->wb = f->b;
	f->ba = f->a; f->bb = f->b;
	R.s = &f->wb;
	if(n->left->ty == tbig){
		R.m = &f->a; R.d = &f->out;
		switch(n->op){
		case Odiv: R.s = &f->b; divl(); break;
		case Omod: R.s = &f->b; modl(); break;
		case Oexp: iexpl(); break;
		case Olsh: shll(); break;
		case Orsh: shrl(); break;
		default: abort();
		}
		return f->out;
	}
	if(n->left->ty == tbyte){
		R.m = &f->ba; R.d = &f->bo;
		switch(n->op){
		case Odiv: R.s = &f->bb; divb(); break;
		case Omod: R.s = &f->bb; modb(); break;
		case Olsh: shlb(); break;
		case Orsh: shrb(); break;
		default: abort();
		}
		return f->bo;
	}
	R.m = &f->wa; R.d = &f->wo;
	switch(n->op){
	case Odiv: divw(); break;
	case Omod: modw(); break;
	case Oexp: iexpw(); break;
	case Olsh: shlw(); break;
	case Orsh: shrw(); break;
	default: abort();
	}
	return f->wo;
}

/* Allocate a separate stable register context for each recursive AST evaluation. */
static Long
execute(Node *n)
{
	struct EvalFrame *f;
	Long value;

	assert(depth < 32);
	f = &slots[depth++];
	value = executeframe(n, f);
	depth--;
	return value;
}

/* Match the exact integer error or value without relying on a running VM process. */
static Long
observe(Node *n)
{
	static Long value;

	depth = visits = 0;
	raised = nil;
	value = 0;
	if(setjmp(boundary) == 0)
		value = execute(n);
	depth = 0;
	return value;
}

/* Build a bad constant operation inside an int-valued comparison when needed. */
static void
one(int control, Type *type, int badop, Long condition, int arm, int foldedcondition, int dynamic)
{
	Decl declaration = {nil, Dlocal, 0, nil, nil};
	Node numerator, denominator, bad, zero, normalized, conditionnode, conditionzero;
	Node conditionexpression, ordinary, branches, outer, callee, call, *result;
	Decl functiondecl;
	static Type functiontype = {Tfn, 8};
	Node *operand;
	Long expected;
	int selected, errors, beforevisits;

	numerator = (Node){Oconst, type, nil, nil, nil, {31}, badop == Oexp ? 0 : 7, 0};
	denominator = (Node){Oconst, badop == Olsh || badop == Orsh || badop == Oexp ? tint : type,
		nil, nil, nil, {32}, badop == Oexp ? -1 : badop == Olsh ? (type == tint ? 64 : type->size*8) : badop == Orsh ? -1 : 0, 0};
	bad = (Node){badop, type, &numerator, &denominator, nil, {33}, 0, 0};
	zero = (Node){Oconst, type, nil, nil, nil, {34}, 0, 0};
	normalized = (Node){Oneq, tint, &bad, &zero, nil, {35}, 0, 0};
	conditionnode = (Node){dynamic ? Oname : Oconst, tint, nil, nil,
		dynamic ? &declaration : nil, {10}, condition, 0};
	conditionzero = (Node){Oconst, tint, nil, nil, nil, {11}, 0, 0};
	conditionexpression = (Node){Oneq, tint, &conditionnode, &conditionzero, nil, {12}, 0, 0};
	ordinary = (Node){Oconst, tint, nil, nil, nil, {44}, -7, 0};
	functiondecl = (Decl){nil, Dfn, 0, &normalized, nil};
	callee = (Node){Oname, &functiontype, nil, nil, &functiondecl, {36}, 0, 0};
	call = (Node){Ocall, tint, &callee, nil, nil, {37}, 0, 0};
	operand = phase == 2 ? &call : &normalized;
	branches = (Node){Oseq, tint, arm ? &ordinary : operand, arm ? operand : &ordinary,
		nil, {45}, 0, 0};
	outer = (Node){control, tint, foldedcondition ? &conditionexpression : &conditionnode,
		control == Oif ? &branches : operand, nil, {99}, 0, 0};
	selected = control == Oif ? (!!condition == !arm) : control == Oandand ? condition != 0 : condition == 0;
	expected = observe(&outer);
	beforevisits = visits;
	assert((raised != nil) == selected);
	diagnostics = warnings = used = inlined = 0;
	result = phase ? simplify(&outer) : efold(&outer);
	errors = diagnostics+warnings;
	/* Dynamic conditions still require both arms to be folded and checked. */
	if(dynamic || selected){
		assert(errors > 0);
	}else{
		assert(errors == 0 && inlined == 0);
		assert(result->op == Oconst && result->ty == tint);
		assert(observe(result) == expected && raised == nil);
		assert(visits == 0 && beforevisits == 0);
		/* The unselected operation and its constant operands must remain untouched. */
		assert(bad.op == badop && bad.left == &numerator && bad.right == &denominator);
		assert(numerator.op == Oconst && denominator.op == Oconst);
	}
	checks++;
}

/* Normal branches must retain boolean normalization and evaluate only the selected variable. */
static void
valid(void)
{
	Decl d[2] = {{nil, Dlocal}, {nil, Dlocal}};
	Node left, right, n;
	Long values[] = {-2147483648LL, -3, -1, 0, 1, 2, 2147483647};
	Long before;
	int i, j, op, leaf, count;
	Node *result;

	for(op = 0; op < 2; op++)for(leaf = 0; leaf < 2; leaf++)
	for(i = 0; i < 7; i++)for(j = 0; j < 7; j++){
		left = (Node){leaf ? Oname : Oconst, tint, nil, nil, leaf ? &d[0] : nil, {1}, values[i], 0};
		right = (Node){Oname, tint, nil, nil, &d[1], {2}, values[j], 0};
		n = (Node){op ? Ooror : Oandand, tint, &left, &right, nil, {99}, 0, 0};
		before = observe(&n); count = visits;
		used = diagnostics = warnings = inlined = 0;
		result = phase ? simplify(&n) : efold(&n);
		assert(diagnostics == 0 && warnings == 0);
		assert(observe(result) == before && raised == nil && visits == count);
		checks++;
	}
}

/* Cover direct/derived conditions, every lazy control node and selected/dead inline arms. */
int
main(int argc, char **argv)
{
	int controls[] = {Oandand, Ooror, Oif};
	int ops[] = {Odiv, Omod, Oexp, Olsh, Orsh};
	Long conditions[] = {-2147483648LL, -1, 0, 1, 7, 2147483647};
	Type *types[] = {tbyte, tint, tbig};
	int c, t, op, condition, arm, derived, dynamic;

	isrelop[Oeq] = isrelop[Oneq] = 1;
	for(phase = 0; phase < 3; phase++){
		if(argc == 1 || strcmp(argv[1], "valid") == 0)
			valid();
		for(c = 0; c < 3; c++){
			if(argc > 1 && strcmp(argv[1], c == 0 ? "and" : c == 1 ? "or" : "inline") != 0)
				continue;
			for(t = 0; t < 3; t++)for(op = 0; op < 5; op++){
				if(t == 0 && ops[op] == Oexp)continue;
				for(condition = 0; condition < 6; condition++)for(arm = 0; arm < (c == 2 ? 2 : 1); arm++)
				for(derived = 0; derived < 2; derived++)for(dynamic = 0; dynamic < 2; dynamic++)
					one(controls[c], types[t], ops[op], conditions[condition], arm, derived, dynamic);
			}
		}
	}
	assert(checks > 0);
	printf("Compiler lazy folding: %d selected/dead-arm/diagnostic cases, WORD%d\n", checks, WORD_BITS);
	return 0;
}
