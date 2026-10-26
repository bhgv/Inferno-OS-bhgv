#include <limits.h>
#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef int64_t LONG;
typedef struct String String;
struct String {int len; char Sascii[1024];};
#define H ((void*)1)
static struct {void *s, *d;} R;

/* Model the native strtol result width while leaving its decimal syntax to host libc. */
static WORD
modelstrtol(char *text, char **end, int base)
{
	long long value;

	value = strtoll(text, end, base);
	if(sizeof(WORD) == 4){
		if(value > INT32_MAX)
			return INT32_MAX;
		if(value < INT32_MIN)
			return INT32_MIN;
	}
	return value;
}

/* Numeric fixtures use actual ASCII storage; Unicode-to-UTF conversion has its own suite. */
static char*
string2c(String *s)
{
	return s->Sascii;
}

#define OP(name) static void name(void)
#define S(r) (*(String**)R.r)
#define W(r) (*(WORD*)R.r)
#undef V
#define V(r) (*(LONG*)R.r)
STRING_VM

static struct {
	char *text;
	int len;
	uint64_t big;
	int small;
	int64_t value, word32, word64;
} vectors[] = {
STRING_VECTORS
};

/* Require both portable constant folding and agreement with actual VM conversion bodies. */
static void
checkstring(int i)
{
	Type source = {Tstring}, target;
	Sym symbol = {vectors[i].text};
	Decl decl = {&symbol};
	Node left, original, cast, *folded;
	String text, *input;
	union {String *pointer; WORD word; LONG big;} output;
	WORD word;
	LONG big;
	int kind, alias;
	Long expected;

	check(vectors[i].len < sizeof(text.Sascii));
	memset(&text, 0, sizeof text);
	memcpy(text.Sascii, vectors[i].text, vectors[i].len);
	text.len = vectors[i].len;
	for(kind = Tint; kind <= Tbig; kind++){
		target = (Type){kind};
		left = (Node){&source, {7}, 123, 4.5, &decl};
		original = left;
		cast = (Node){&target, {99}, 0, 0, nil};
		lexerrors = 0;
		folded = foldcast(&cast, &left);
		check(lexerrors == 0);
		if(kind != Tbig && !vectors[i].small){
			check(folded == &cast && memcmp(&left, &original, sizeof left) == 0);
		}else{
			expected = kind == Tbig ? (Long)vectors[i].big :
				kind == Tbyte ? (vectors[i].value&255) : vectors[i].value;
			check(folded == &left && left.ty == &target && left.src.line == 99);
			check(left.val == expected);
		}
		checks++;
	}
	for(alias = 0; alias < 2; alias++){
		input = &text;
		output.pointer = &text;
		R.s = alias ? &output.pointer : &input;
		R.d = alias ? &output.word : &word;
		cvtcw();
		if(alias)word = output.word;
		check(word == (sizeof(WORD) == 4 ? vectors[i].word32 : vectors[i].word64));
		output.pointer = &text;
		R.s = alias ? &output.pointer : &input;
		R.d = alias ? &output.big : &big;
		cvtcl();
		if(alias)big = output.big;
		check((uint64_t)big == vectors[i].big);
		checks += 2;
	}
}

/* Initialize actual lexer character metadata, then cover whitespace, suffixes and long prefixes. */
int
main(void)
{
	int c, i;

	for(c = 0; c < 256; c++){
		if(c >= '0' && c <= '9')map[c] |= Mdigit;
		if(c >= 'a' && c <= 'z')map[c] |= Mlower;
		if(c >= 'A' && c <= 'Z')map[c] |= Mupper;
	}
	for(i = 0; i < sizeof(vectors)/sizeof(vectors[0]); i++)
		checkstring(i);
	printf("Compiler string casts: %d prefix/range/deferred/VM cases, WORD%d\n", checks, WORD_BITS);
	return 0;
}
