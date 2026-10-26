#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <setjmp.h>
#include <string.h>
#include <limits.h>

/* LeakSanitizer cannot inspect processes here; keep address and stack checks active. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

typedef uintptr_t uintptr; // Native arithmetic used by extracted slot guards.
#define H ((void *)(uintptr)-1) // Dis sentinel slot.
typedef int64_t vlong;
typedef uint64_t uvlong;
typedef unsigned int uint;
typedef int64_t LONG;
typedef uint64_t ULONG;
typedef double REAL;
#if WORD_BITS == 32
typedef int32_t WORD;
typedef uint32_t UWORD;
#define WORD_MIN INT32_MIN
#else
typedef int64_t WORD;
typedef uint64_t UWORD;
#define WORD_MIN INT64_MIN
#endif
#define nil NULL
OPS

typedef struct Node Node;
struct Node {
	Node *left, *right;
	int op, side;
	vlong val;
	void *decl;
};
static int isrelop[64];
static int visits, checks;
static struct {
	void *s, *m, *d;
} R;
static jmp_buf boundary;
static char exZdiv[] = "divide by zero";
static char exBounds[] = "bounds"; // Malformed decoded numeric slot.
static char *raised;

/* Model strict side-effect detection for variable and relational operands. */
int
hasside(Node *node, int strict)
{
	assert(strict == 1);
	return node->side;
}

/* Catch VM arithmetic errors and verify that the destination is retained. */
void
error(char *message)
{
	raised = message;
	longjmp(boundary, 1);
}

#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define V(r) (*(LONG*)R.r)
#define F(r) (*(REAL*)R.r)
IMPLEMENTATION

/* Evaluate only the operators under test, counting observable leaf calls. */
static vlong
evaluate(Node *node)
{
	vlong left, right;

	if(node->left == nil) {
		visits += node->side;
		return node->val;
	}
	left = evaluate(node->left);
	if(node->op == Onot)
		return !left;
	right = evaluate(node->right);
	switch(node->op) {
	case Ooror:
		return left || right;
	case Oandand:
		return left && right;
	case Oeq:
		return left == right;
	case Oneq:
		return left != right;
	case Oexp:
		return ipow(left, right);
	default:
		assert(0);
		return 0;
	}
}

/* Compare both the value and effects before and after one folding step. */
static void
checkfold(int op, int leftop, vlong a, vlong b, int side)
{
	Node left = {0}, right = {0}, expression = {0};
	vlong want, got;
	int effects;

	left.op = leftop;
	left.val = a;
	left.side = side;
	right.op = Oconst;
	right.val = b;
	expression.op = op;
	expression.left = &left;
	expression.right = &right;
	visits = 0;
	want = evaluate(&expression);
	effects = visits;
	visits = 0;
	got = evaluate(foldvc(&expression));
	assert(got == want && visits == effects);
	checks++;
}

/* Integer powers must agree across constant folding and both VM widths. */
static void
checkinteger(vlong base, int exponent)
{
	WORD wordbase, wordexponent, wordresult;
	union { uint64_t align; unsigned char bytes[64]; } slots;
	unsigned char expected[64];
	int kind, alias, offset, width;
	LONG bigresult;
	uvlong want, factor;
	unsigned int count;

	if(exponent < 0)
		want = base == 1 ? 1 : base == -1 ? (exponent&1 ? -1 : 1) : 0;
	else {
		want = 1;
		factor = base;
		count = exponent;
		while(count--)
			want *= factor;
	}
	assert((uvlong)ipow(base, exponent) == want);
	wordbase = base;
	wordexponent = exponent;
	R.s = &wordexponent;
	R.m = &base;
	R.d = &bigresult;
	iexpl();
	assert((uvlong)bigresult == want);
	R.m = &wordbase;
	R.d = &wordresult;
	iexpw();
	assert((UWORD)wordresult == (UWORD)want);
	for(kind = 0; kind < 2; kind++)
	for(alias = 0; alias < 5; alias++){
		memset(slots.bytes, 0xa5, sizeof(slots.bytes));
		memcpy(slots.bytes+4, &wordexponent, sizeof(wordexponent));
		width = kind ? 8 : sizeof(WORD);
		memcpy(slots.bytes+20, kind ? (void*)&base : (void*)&wordbase, width);
		memcpy(expected, slots.bytes, sizeof(expected));
		offset = alias == 0 ? 36 : alias == 1 ? 4 : alias == 2 ? 20 : alias == 3 ? 8 : 24;
		memcpy(expected+offset, &want, width);
		R.s = slots.bytes+4;
		R.m = slots.bytes+20;
		R.d = slots.bytes+offset;
		if(kind)iexpl();
		else iexpw();
		assert(!memcmp(slots.bytes, expected, sizeof(expected)));
		checks++;
	}
	checks++;
}

/* Real folding and execution must retain finite subnormal reciprocal powers. */
static void
checkreal(double base, int exponent, double want)
{
	WORD wordexponent;
	double result;
	union { uint64_t align; unsigned char bytes[64]; } slots;
	unsigned char expected[64];
	int alias, offset;

	result = rpow(base, exponent);
	assert(result == want);
	if(want == 0.0 || isinf(want))
		assert(!!signbit(result) == !!signbit(want));
	wordexponent = exponent;
	R.s = &wordexponent;
	R.m = &base;
	R.d = &result;
	iexpf();
	assert(result == want);
	if(want == 0.0 || isinf(want))
		assert(!!signbit(result) == !!signbit(want));
	for(alias = 0; alias < 5; alias++){
		memset(slots.bytes, 0xa5, sizeof(slots.bytes));
		memcpy(slots.bytes+4, &wordexponent, sizeof(wordexponent));
		memcpy(slots.bytes+20, &base, sizeof(base));
		memcpy(expected, slots.bytes, sizeof(expected));
		offset = alias == 0 ? 36 : alias == 1 ? 4 : alias == 2 ? 20 : alias == 3 ? 8 : 24;
		memcpy(expected+offset, &want, sizeof(want));
		R.s = slots.bytes+4;
		R.m = slots.bytes+20;
		R.d = slots.bytes+offset;
		iexpf();
		assert(!memcmp(slots.bytes, expected, sizeof(expected)));
		checks++;
	}
	checks++;
}

/* Cover boolean identities, full-width exponents, wraparound and VM errors. */
int
main(void)
{
	int a, b, side, exponent;
	int ops[] = {Ooror, Oandand, Oeq, Oneq};
	unsigned int op;
	WORD minimum, wordbase, wordresult;
	LONG bigbase, bigresult;
	double realbase, realresult;

	isrelop[Olt] = 1;
	for(op = 0; op < sizeof(ops)/sizeof(ops[0]); op++)
		for(a = -3; a <= 3; a++)
			for(b = -3; b <= 3; b++)
				for(side = 0; side <= 1; side++) {
					checkfold(ops[op], Oname, a, b, side);
					if(a == 0 || a == 1)
						checkfold(ops[op], Olt, a, b, side);
				}
	for(a = -3; a <= 3; a++)
		for(side = 0; side <= 1; side++) {
			checkfold(Oexp, Oname, a, 0, side);
			checkfold(Oexp, Oname, a, 1, side);
		}
	for(a = -4; a <= 4; a++)
		for(exponent = -8; exponent <= 80; exponent++)
			if(a != 0 || exponent >= 0)
				checkinteger(a, exponent);
	checkinteger(-1, INT_MIN);
	checkinteger(1, INT_MIN);
	checkinteger(2, INT_MIN);
	checkinteger(INT64_MIN, 3);
	checkreal(2.0, -1024, scalbn(1.0, -1024));
	checkreal(2.0, -1074, scalbn(1.0, -1074));
	checkreal(-1.0, INT_MIN, 1.0);
	checkreal(2.0, INT_MIN, 0.0);
	checkreal(-0.0, -3, -INFINITY);
	checkreal(0.0, -1, INFINITY);
	checkreal(-2.0, -1073, -scalbn(1.0, -1073));

	minimum = WORD_MIN;
	wordbase = -1;
	R.s = &minimum;
	R.m = &wordbase;
	R.d = &wordresult;
	iexpw();
	assert(wordresult == 1);
	bigbase = -1;
	R.m = &bigbase;
	R.d = &bigresult;
	iexpl();
	assert(bigresult == 1);
	realbase = -1;
	R.m = &realbase;
	R.d = &realresult;
	iexpf();
	assert(realresult == 1);

	wordbase = 0;
	wordresult = 123;
	R.m = &wordbase;
	R.d = &wordresult;
	if(setjmp(boundary) == 0) {
		iexpw();
		assert(0);
	}
	assert(raised == exZdiv && wordresult == 123);
	bigbase = 0;
	bigresult = 456;
	R.m = &bigbase;
	R.d = &bigresult;
	if(setjmp(boundary) == 0) {
		iexpl();
		assert(0);
	}
	assert(raised == exZdiv && bigresult == 456);
	printf("PASS folding effects and powers: %d checks, WORD%d and minimum exponents\n",
		checks, WORD_BITS);
	return 0;
}
