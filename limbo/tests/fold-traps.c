#include <setjmp.h>
#include <stddef.h>

typedef unsigned char uchar; // Byte view used by the extracted VM field reads.

typedef uintptr_t uintptr;
#if WORD_BITS == 32
typedef int32_t WORD;
typedef uint32_t UWORD;
#else
typedef int64_t WORD;
typedef uint64_t UWORD;
#endif
typedef int64_t LONG;
typedef uint64_t ULONG;
typedef unsigned long ulong;
typedef unsigned char BYTE;
typedef uint32_t Rune;
#define H ((void*)1) // Fixture nil-owner sentinel.
#define D2H(s) (s) // Static fixture owners store their reference count inline.

static struct {
	void *s; // Captured source slot for the production VM operators.
	void *m; // Captured middle operand slot.
	void *d; // Captured destination slot.
} R;
typedef struct List List;

struct List {
	List *tail; // Next list owner consumed by head/tail VM operators.
	WORD data[1]; // Scalar payload for the list-head cases.
};
typedef struct String String;

/* Bounded storage model for the production slice function. */
struct String {
	unsigned long ref; // Live ownership required by the production slice helper.
	int len; // Signed logical length and representation.
	int max; // Character capacity of either fixture representation.

	union {
		char ascii[16]; // ASCII payload for narrow slice cases.
		Rune runes[16]; // Rune payload for wide slice cases.
	} data; // Shared inline payload, matching the production slice-address contract.
};
#define Sascii data.ascii // ASCII view of the inline payload.
#define Srune data.runes // Rune view of the inline payload.

static String stringpool[64];
static int stringused;
static char exInval[] = "invalid", exZdiv[] = "zero divide", exNilref[] = "nil", exBounds[] = "bounds", exHeap[] = "heap";
static char *raised;
static jmp_buf escape;


/* Preserve the native exception identity while leaving compiler diagnostics independent. */
static void
vmerror(char *message)
{
	raised = message;
	longjmp(escape, 1);
}


/* Successful list tail movement needs no GC in this static ownership fixture. */
static void
movp(void)
{
	*(List**)R.d = *(List**)R.s;
}


/* Supply bounded result storage to the actual ASCII and Rune slice implementation. */
static String*
newstring(int n)
{
	String *s;

	if(n < 0 || n > 16 || stringused >= 64)
		abort();
	s = &stringpool[stringused++];
	memset(s, 0, sizeof(*s));
	s->ref = 1;
	s->len = n;
	s->max = 16;
	return s;
}


/* Rune contents are irrelevant to same-slice comparisons, but lengths remain real. */
static String*
newrunes(int n)
{
	String *s;

	s = newstring(n);
	s->len = -n;
	return s;
}

#define OP(name) static void name(void) // Local names for extracted production VM operators.
#define W(r) (*(WORD*)R.r) // Access a VM WORD operand.
#define B(r) (*(BYTE*)R.r) // Access a byte operand.
#define UW(r) (*(UWORD*)R.r) // Access an unsigned VM WORD operand.
#define V(r) (*(LONG*)R.r) // Access a wide signed operand.
#define UV(r) (*(ULONG*)R.r) // Access a wide unsigned operand.
#define L(r) (*(List**)R.r) // Access a captured list-owner slot.
VM_IMPLEMENTATION
EFFECT_TABLE

/* Model VM operand slots explicitly, so numeric errors abandon no C stack-backed slots. */
typedef struct EvalFrame EvalFrame;

struct EvalFrame
{
	Long a, b, out; // Captured wide arithmetic operands and result.
	WORD worda, wordb, wordout; // VM WORD operand slots and result.
	BYTE bytea, byteout; // Byte shift operand and result.
	List list, *lp, *tailout; // Stable list owner and pointer slots.
	String original, *slice; // Source string owner and selected result.
};
static EvalFrame evalframes[32];
static int evaldepth;
static Long run(Node*);


/* Evaluate trap-producing operators through their actual native VM bodies. */
static Long
runframe(Node *n, EvalFrame *frame)
{
	if(n->op == Oconst || n->op == Oname)
		return n->val;
	frame->a = run(n->left);
	if(n->op == Oandand)
		return frame->a != 0 && run(n->right) != 0;
	if(n->op == Ooror)
		return frame->a != 0 || run(n->right) != 0;
	if(n->op == Ohd || n->op == Otl){
		frame->list.tail = H;
		frame->list.data[0] = 7;
		frame->lp = frame->a ? &frame->list : H;
		R.s = &frame->lp;
		if(n->op == Ohd){
			R.d = &frame->wordout;
			headw();
			return frame->wordout;
		}
		R.d = &frame->tailout;
		tail();
		return frame->tailout != H;
	}
	if(n->op == Oslice){
		memset(&frame->original, 0, sizeof frame->original);
		frame->original.ref = 1;
		frame->original.len = frame->a;
		frame->original.max = 16;
		frame->slice = slicer(run(n->right->left), run(n->right->right), &frame->original);
		return frame->slice == H ? 0 : frame->slice->len;
	}
	frame->b = run(n->right);
	if(n->op == Olsh || n->op == Orsh || n->op == Oexp){
		frame->worda = frame->a; frame->wordb = frame->b; frame->bytea = frame->a;
		R.s = &frame->wordb;
		if(n->left->ty == tbig){
			R.m = &frame->a; R.d = &frame->out;
			if(n->op == Olsh)shll();
			else if(n->op == Orsh)shrl();
			else iexpl();
			return frame->out;
		}
		if(n->left->ty == tbyte){
			R.m = &frame->bytea; R.d = &frame->byteout;
			if(n->op == Olsh)shlb();else shrb();
			return frame->byteout;
		}
		R.m = &frame->worda; R.d = &frame->wordout;
		if(n->op == Olsh)shlw();
		else if(n->op == Orsh)shrw();
		else iexpw();
		return frame->wordout;
	}
	switch(n->op){
	case Omul: return (uvlong)frame->a*(uvlong)frame->b;
	case Osub: return (uvlong)frame->a-(uvlong)frame->b;
	case Oand: return frame->a&frame->b;
	case Omod:
		if(frame->b != 1)
			abort();
		return 0;
	case Oeq: return frame->a == frame->b;
	case Oneq: return frame->a != frame->b;
	default: abort();
	}
}


/* Allocate one stable operand context per recursive expression evaluation. */
static Long
run(Node *n)
{
	EvalFrame *frame;
	Long value;

	if(evaldepth >= 32)
		abort();
	frame = &evalframes[evaldepth++];
	value = runframe(n, frame);
	evaldepth--;
	return value;
}

typedef struct Outcome Outcome;

struct Outcome {
	char *exception; // Exact native error identity, or nil on success.
	Long value; // Observable expression result when no error is raised.
};

/* Capture value or exact native exception before and after compiler AST mutation. */
static Outcome
capture(Node *n)
{
	Outcome result;

	raised = nil;
	stringused = 0;
	evaldepth = 0;
	result.value = 0;
	if(setjmp(escape) == 0)
		result.value = run(n);
	evaldepth = 0;
	result.exception = raised;
	return result;
}


/* Observable errors must survive result-absorbing and same-tree rewrites. */
static void
compare(Node *n)
{
	Outcome before, after;
	Node *folded;

	before = capture(n);
	used = 0;
	folded = efold(n);
	after = capture(folded);
	if(before.exception != after.exception)
		fprintf(stderr, "exception changed by folding: before=%s after=%s\n",
			before.exception ? before.exception : "none", after.exception ? after.exception : "none");
	if(before.exception != after.exception)
		abort();
	if(before.exception == nil && before.value != after.value)
		abort();
	checks++;
}


/* Rebuild each wrapper because production folding deliberately mutates its operands. */
static void
wrappers(int innerop, Type *type, Long a, Long b, Long end)
{
	Decl d[4] = {{nil, Dlocal}, {nil, Dlocal}, {nil, Dlocal}, {nil, Dlocal}};
	Node left, right, inner, outer, constant, bounds, stop, normalized, reference;
	static Type listtype = {Tlist, 8};
	int ops[] = {Omul, Oand, Omod, Oexp, Oandand, Ooror, Osub, Oeq, Oneq};
	int i;

	for(i = 0; i < sizeof(ops)/sizeof(ops[0]); i++){
		left = (Node){Oname, type, nil, nil, &d[0], {1}, a, 0};
		right = (Node){Oname, tint, nil, nil, &d[1], {1}, b, 0};
		inner = (Node){innerop, type, &left, &right, nil, {1}, 0, 0};
		if(innerop == Ohd || innerop == Otl){
			left.ty = &listtype;
			inner.ty = innerop == Ohd ? tint : &listtype;
			inner.right = nil;
		}
		if(innerop == Oslice){
			left.ty = inner.ty = tstring;
			stop = (Node){Oname, tint, nil, nil, &d[2], {1}, end, 0};
			bounds = (Node){Oseq, tint, &right, &stop, nil, {1}, 0, 0};
			inner.right = &bounds;
		}
		/* Normalize list/string expressions before applying integer absorbers. */
		reference = (Node){Oname, inner.ty, nil, nil, &d[3], {1}, 0, 0};
		normalized = (Node){Oeq, tint, &inner, &reference, nil, {1}, 0, 0};
		constant = (Node){Oconst, tint, nil, nil, nil, {1}, 0, 0};
		constant.val = ops[i] == Omod || ops[i] == Ooror ? 1 : 0;
		outer = (Node){ops[i], tint, inner.ty == tint ? &inner : &normalized,
			&constant, nil, {1}, 0, 0};
		if(ops[i] == Osub || ops[i] == Oeq || ops[i] == Oneq)
			outer.right = outer.left;
		compare(&outer);
	}
}


/* Zero-valued constant bases still require a dynamic shift-count check. */
static void
zeroshifts(void)
{
	Decl d = {nil, Dlocal, 0, nil, nil};
	Node left, right, n;
	Long amounts[] = {-2147483648LL, -1, 0, 1, 7, 8, 31, 32, 63, 64, 2147483647};
	Type *types[] = {tbyte, tint, tbig};
	int op, i, t;

	for(t = 0; t < 3; t++)
	for(op = 0; op < 2; op++)
	for(i = 0; i < sizeof(amounts)/sizeof(amounts[0]); i++){
		left = (Node){Oconst, types[t], nil, nil, nil, {1}, 0, 0};
		right = (Node){Oname, tint, nil, nil, &d, {1}, amounts[i], 0};
		n = (Node){op ? Orsh : Olsh, types[t], &left, &right, nil, {1}, 0, 0};
		compare(&n);
	}
}


/* Cover each newly classified operator with successful and exceptional executions. */
int
main(int argc, char **argv)
{
	Long amounts[] = {-2147483648LL, -1, 0, 1, 7, 8, 31, 32, 63, 64, 2147483647};
	Long powers[] = {-5, -1, 0, 1, 2, 5};
	Type *types[] = {tbyte, tint, tbig};
	int i, a, t, op;

	for(i = 0; effectops[i] >= 0; i++)
		sideeffect[effectops[i]] = 1;
	isrelop[Oeq] = isrelop[Oneq] = 1;
	if(argc == 1 || strcmp(argv[1], "zero") == 0)
		zeroshifts();
	if(argc == 1 || strcmp(argv[1], "shifts") == 0)
		for(t = 0; t < 3; t++)for(op = 0; op < 2; op++)
		for(a = 0; a <= 3; a++)for(i = 0; i < sizeof(amounts)/sizeof(amounts[0]); i++)
			wrappers(op ? Orsh : Olsh, types[t], a, amounts[i], 0);
	if(argc == 1 || strcmp(argv[1], "powers") == 0)
		for(t = 0; t < 2; t++)for(a = -1; a <= 3; a++)
		for(i = 0; i < sizeof(powers)/sizeof(powers[0]); i++)
			wrappers(Oexp, t ? tbig : tint, a, powers[i], 0);
	if(argc == 1 || strcmp(argv[1], "lists") == 0 || strcmp(argv[1], "heads") == 0)
		for(a = 0; a < 2; a++)
			wrappers(Ohd, tint, a, 0, 0);
	if(argc == 1 || strcmp(argv[1], "lists") == 0 || strcmp(argv[1], "tails") == 0)
		for(a = 0; a < 2; a++)
			wrappers(Otl, tint, a, 0, 0);
	if(argc == 1 || strcmp(argv[1], "slices") == 0)
		for(a = 0; a <= 4; a++)for(i = -1; i <= 5; i++)for(op = -1; op <= 5; op++)
			wrappers(Oslice, tstring, a, i, op);
	if(checks <= 0)
		abort();
	printf("Compiler exception folding: %d value/error cases, WORD%d\n", checks, WORD_BITS);
	return 0;
}
