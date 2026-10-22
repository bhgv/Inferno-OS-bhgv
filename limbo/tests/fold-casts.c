#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

typedef int64_t Long;
typedef uint64_t uvlong;
typedef double Real;
enum {Tint, Tbyte, Tbig, Treal, Tfix, Tstring, NumSize = 128};
typedef struct { int kind; double factor; } Type;
typedef struct { int line; } Src;
typedef struct { char *name; } Sym;
typedef struct { Sym *sym; } Decl;
typedef struct Node Node;
struct Node { Type *ty; Src src; Long val; double rval; Decl *decl; };
#define nil NULL
static int checks;

/* LeakSanitizer cannot inspect processes here; the fixture allocates no heap. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Parsing is a bounded dependency; the production numeric lexer has its own tests. */
static Long
strtoi(char *text, int base)
{
	return strtoll(text, nil, base);
}

/* Fixed-point and formatted-string targets are outside this fixture. */
static Long real2fix(double value, Type *type) { (void)value; (void)type; abort(); }
/* Bind fixed-point scaling while retaining the production fix2real body. */
static double scale(Type *type) { return type->factor; }
static Long fix2fix(Long value, Type *from, Type *to) { (void)value; (void)from; (void)to; abort(); }
static int tequal(Type *a, Type *b) { return a == b; }
static void *allocmem(int size) { (void)size; abort(); }
static char *seprint(char *p, char *end, char *format, ...) { (void)p; (void)end; (void)format; abort(); }
static Decl *enterstring(char *text, int n) { (void)text; (void)n; abort(); }
static Node *mksconst(Src *src, Decl *decl) { (void)src; (void)decl; abort(); }
static Node *foldcasti(Node*, Node*);
IMPLEMENTATION

/* Canonical signed-32 arithmetic uses a wider intermediate as an independent oracle. */
static Long
int32value(Long value)
{
	uint32_t bits = (uint32_t)(uvlong)value;

	return bits & 0x80000000U ? (Long)bits-INT64_C(4294967296) : bits;
}

/* Check source normalization and every integer destination, including real output. */
static void
integers(void)
{
	Long edges[] = {0, 1, -1, 127, 128, 255, 256, -256, 300, -300,
		INT32_MIN, INT32_MAX, INT64_MIN, INT64_MAX, INT64_C(4294967295)};
	uvlong state = UINT64_C(0x6361737473);
	Long input, expected;
	int source, target, i;
	Type types[] = {{Tint}, {Tbyte}, {Tbig}, {Treal}};
	Node left, cast, *result;

	for(i = 0; i < 2048; i++) {
		state = state*UINT64_C(6364136223846793005)+1;
		memcpy(&input, &state, sizeof(input));
		if(i < sizeof(edges)/sizeof(edges[0])) input = edges[i];
		for(source = Tint; source <= Tbig; source++)
			for(target = Tint; target <= Treal; target++) {
				left = (Node){&types[source], {1}, input, 0, nil};
				cast = (Node){&types[target], {99}, 0, 0, nil};
				expected = source == Tint ? int32value(input) : source == Tbyte ? (Long)((uvlong)input&255) : input;
				if(target == Tint) expected = int32value(expected);
				if(target == Tbyte) expected = (uvlong)expected&255;
				result = foldcast(&cast, &left);
				assert(result == &left && left.ty == &types[target] && left.src.line == 99);
				if(target == Treal) assert(left.rval == (double)expected);
				else assert(left.val == expected);
				checks++;
			}
	}
}

/* Strings must undergo the same integer narrowing as other constant sources. */
static void
strings(void)
{
	Long values[] = {0, 255, 256, 300, -1, -300, INT32_MIN, INT32_MAX,
		INT64_C(4294967295), INT64_MIN, INT64_MAX};
	Type source = {Tstring}, target;
	char text[64];
	Sym symbol = {text};
	Decl decl = {&symbol};
	Node left, cast;
	Long expected;
	unsigned int i;
	int kind;

	for(i = 0; i < sizeof(values)/sizeof(values[0]); i++)
		for(kind = Tint; kind <= Tbig; kind++) {
			snprintf(text, sizeof(text), "%lld", (long long)values[i]);
			target.kind = kind;
			left = (Node){&source, {1}, 0, 0, &decl};
			cast = (Node){&target, {99}, 0, 0, nil};
			expected = kind == Tint ? int32value(values[i]) : kind == Tbyte ? (Long)((uvlong)values[i]&255) : values[i];
			assert(foldcast(&cast, &left) == &left);
			assert(left.val == expected && left.ty == &target && left.src.line == 99);
			checks++;
		}
}

/* Preserve the original AST when a native rounded conversion is not representable. */
static void
realcase(double value, int kind)
{
	Type source = {Treal}, target = {kind};
	Node left = {&source, {1}, 123, value, nil}, original, cast = {&target, {99}, 0, 0, nil};
	Node *result;
	double adjusted;
	long double rounded;
	Long expected;
	int valid;

	memcpy(&original, &left, sizeof(left));
	adjusted = value < 0 ? value-.5 : value+.5;
	rounded = truncl((long double)adjusted);
	valid = isfinite(adjusted) && rounded >= -9223372036854775808.0L && rounded < 9223372036854775808.0L;
	if(kind != Tbig) valid = valid && rounded >= -2147483648.0L && rounded < 2147483648.0L;
	result = foldcast(&cast, &left);
	if(!valid) {
		assert(result == &cast && memcmp(&original, &left, sizeof(left)) == 0);
	}else {
		expected = (Long)rounded;
		if(kind == Tbyte) expected = (uvlong)expected&255;
		assert(result == &left && left.val == expected && left.ty == &target && left.src.line == 99);
	}
	checks++;
}

/* Fixed-point sources truncate as before, then narrow to the destination width. */
static void
fixed(void)
{
	Type source = {Tfix, .25}, target;
	Node left, cast;
	Long expected;
	int kind, value;

	for(kind = Tint; kind <= Tbig; kind++)
		for(value = -4096; value <= 4096; value++) {
			target = (Type){kind, 0};
			left = (Node){&source, {1}, value, 0, nil};
			cast = (Node){&target, {99}, 0, 0, nil};
			expected = value/4;
			if(kind == Tbyte) expected = (uvlong)expected&255;
			assert(foldcast(&cast, &left) == &left);
			assert(left.val == expected && left.ty == &target && left.src.line == 99);
			checks++;
		}
}

/* Fixed-point scaling can round a boundary beyond the native integer domain. */
static void
fixedbounds(void)
{
	Type source = {Tfix, 1}, target;
	Long values[] = {INT64_MIN, INT64_MAX, INT32_MIN, INT32_MAX};
	double factors[] = {.25, 1, 2};
	Node left, original, cast, *result;
	long double converted;
	Long expected;
	int kind, valid;
	unsigned int i, j;

	for(kind = Tint; kind <= Tbig; kind++)
		for(i = 0; i < sizeof(values)/sizeof(values[0]); i++)
			for(j = 0; j < sizeof(factors)/sizeof(factors[0]); j++) {
				source.factor = factors[j];
				target = (Type){kind, 0};
				left = (Node){&source, {1}, values[i], 0, nil};
				memcpy(&original, &left, sizeof(left));
				cast = (Node){&target, {99}, 0, 0, nil};
				converted = truncl((long double)((double)values[i]*factors[j]));
				valid = converted >= -9223372036854775808.0L && converted < 9223372036854775808.0L;
				if(kind != Tbig) valid = valid && converted >= -2147483648.0L && converted < 2147483648.0L;
				result = foldcast(&cast, &left);
				if(valid) {
					expected = (Long)converted;
					if(kind == Tbyte) expected = (uvlong)expected&255;
					assert(result == &left && left.val == expected);
				}else
					assert(result == &cast && memcmp(&original, &left, sizeof(left)) == 0);
				checks++;
			}
}

/* Exercise ties, byte wrapping, signed boundaries, NaNs and infinities. */
int
main(void)
{
	double edges[] = {NAN, INFINITY, -INFINITY, 0, -0.0, 255.5, -255.5,
		2147483647.0, 2147483647.5, -2147483648.0, -2147483648.5,
		-2147483648.49, -2147483649.0, 9223372036854775808.0,
		-9223372036854775808.0, 1e300, -1e300};
	unsigned int i;
	int kind, step;

	integers();
	strings();
	fixed();
	fixedbounds();
	for(kind = Tint; kind <= Tbig; kind++) {
		for(step = -4096; step <= 4096; step++) realcase(step*.25, kind);
		for(i = 0; i < sizeof(edges)/sizeof(edges[0]); i++) realcase(edges[i], kind);
		realcase(nextafter(9223372036854775808.0, 0), kind);
		realcase(nextafter(-9223372036854775808.0, -INFINITY), kind);
	}
	printf("PASS compiler casts: %d source/target/narrowing/rounding/deferred-AST cases\n", checks);
	return 0;
}
