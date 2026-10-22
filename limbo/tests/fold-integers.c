#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef int64_t Long;
typedef uint64_t uvlong;
#define nil NULL
OPS

struct Type {
	int size;
};
static struct Type wi = {4}, bi = {8}, by = {1};
static struct Type *tint = &wi, *tbig = &bi, *tbyte = &by;
typedef struct Node Node;
struct Node {
	Node *left, *right;
	struct Type *ty;
	void *decl;
	int op;
	Long val;
};
static int warnings, errors, checks;

/* Record diagnostics without requiring the compiler front end. */
void
nerror(Node *n, char *message)
{
	(void)n;
	(void)message;
	errors++;
}

/* Count invalid-shift warnings while allowing the historical recovery. */
void
nwarn(Node *n, char *message, int value)
{
	(void)n;
	(void)message;
	(void)value;
	warnings++;
}

/* Exponentiation is outside this fixture's arithmetic and shift checks. */
Long
ipow(Long value, int exponent)
{
	(void)value;
	(void)exponent;
	assert(0);
	return 0;
}

BODY

/* Check both the folded value and replacement of the expression tree. */
static void
check(int op, struct Type *ty, Long a, Long b, Long want)
{
	Node left = {0}, right = {0}, node = {0};

	left.val = a;
	right.val = b;
	node.left = &left;
	node.right = &right;
	node.ty = ty;
	node.op = op;
	assert(foldc(&node) == &node);
	assert(node.op == Oconst);
	if(node.val != want) {
		fprintf(stderr, "op=%d a=%lld b=%lld result=%lld expected=%lld\n",
			op, (long long)a, (long long)b,
			(long long)node.val, (long long)want);
		assert(0);
	}
	assert(node.left == nil && node.right == nil);
	checks++;
}

/* Cover every valid shift count and the signed wraparound boundaries. */
int
main(void)
{
	int shift;
	Long value;

	for(shift = 0; shift < 64; shift++) {
		check(Orsh, tbig, INT64_MIN, shift, INT64_MIN >> shift);
		check(Orsh, tbig, 0x80000000LL, shift, 0x80000000LL >> shift);
		check(Orsh, tbig, -1, shift, -1);
		value = (Long)((uvlong)1 << shift);
		check(Olsh, tbig, 1, shift, value);
		check(Olsh, tbig, -1, shift, (Long)(UINT64_MAX << shift));
	}
	for(shift = 0; shift < 32; shift++) {
		check(Orsh, tint, INT32_MIN, shift, INT32_MIN >> shift);
		check(Olsh, tint, 1, shift, (int32_t)(1U << shift));
	}
	check(Oadd, tbig, INT64_MAX, 1, INT64_MIN);
	check(Osub, tbig, INT64_MIN, 1, INT64_MAX);
	check(Omul, tbig, INT64_MAX, 2, -2);
	check(Oneg, tbig, INT64_MIN, 0, INT64_MIN);
	check(Odiv, tbig, INT64_MIN, -1, INT64_MIN);
	check(Omod, tbig, INT64_MIN, -1, 0);
	check(Odiv, tint, INT32_MIN, -1, INT32_MIN);
	check(Omod, tint, INT32_MIN, -1, 0);
	check(Odiv, tbig, -17, 3, -5);
	check(Omod, tbig, -17, 3, -2);
	check(Oadd, tint, INT32_MAX, 1, INT32_MIN);
	check(Oadd, tbyte, 255, 1, 0);
	check(Olsh, tbig, 3, 64, 3);
	check(Orsh, tint, -3, -1, -3);
	assert(warnings == 2 && errors == 0);
	printf("PASS compiler integer folding: %d width, sign, shift and wraparound cases\n",
		checks);
	return 0;
}
