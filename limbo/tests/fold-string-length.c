#include <stddef.h>

#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef unsigned char uchar;
typedef uintptr_t uintptr; // Native byte-span width remains independent of the VM WORD.
typedef uint32_t Rune;
enum {Runeself = 128, UTFmax = 4};
extern int chartorune(Rune*, char*);
extern int fullrune(char*, int);
typedef struct String String;
struct String {int len; union {char ascii[1024]; Rune runes[1024];} data;};
#define Sascii data.ascii
#define Srune data.runes
/* Only sizeof(Heap) is needed by the decoder; allocation-limit checks have separate tests. */
typedef struct {int unused;} Heap;
static String loaded[4];
static int loadcount;
static char exNegsize[] = "negative", exHeap[] = "heap", exNilref[] = "nil";
#define H ((void*)1)
static struct {void *s, *d;} R;

/* Unexpected decoder errors indicate a failure in these valid constant-string fixtures. */
static void
vmerror(char *message)
{
	(void)message;
	abort();
}

/* Provide string storage while preserving the production loader's decoding path. */
static String*
newstring(int n)
{
	String *s;

	assert(loadcount < 4 && n >= 0 && n <= 1024);
	s = &loaded[loadcount++];
	memset(s, 0, sizeof(*s));
	s->len = n;
	return s;
}

/* Rune strings store the character count with the actual interpreter's negative sign convention. */
static String*
newrunes(int n)
{
	String *s;

	s = newstring(n);
	s->len = -n;
	return s;
}

#define OP(name) static void name(void)
#define S(r) (*(String**)R.r)
#define W(r) (*(WORD*)R.r)
STRING_LENGTH_VM

static struct {char *left; int nleft; char *right; int nright; int count;} vectors[] = {
LENGTH_VECTORS
};

/* Fold literals, named constants and concatenation through the actual recursive folder. */
static void
checklength(int i)
{
	char text[1024];
	Sym symbol, other;
	Decl declaration, second, alias;
	Node left, right, addition, name, length, *result;
	String *decoded;
	WORD runtime;
	int n, mode;

	n = vectors[i].nleft+vectors[i].nright;
	assert(n < sizeof text);
	memcpy(text, vectors[i].left, vectors[i].nleft);
	memcpy(text+vectors[i].nleft, vectors[i].right, vectors[i].nright);
	text[n] = 0;
	loadcount = 0;
	decoded = c2string(text, n);
	R.s = &decoded;
	R.d = &runtime;
	lenc();
	assert(runtime == vectors[i].count);
	checks++;
	for(mode = 0; mode < 3; mode++){
		used = concatlength = concatsymbolcount = 0;
		symbol = (Sym){mode == 2 ? vectors[i].left : text, mode == 2 ? vectors[i].nleft : n};
		other = (Sym){vectors[i].right, vectors[i].nright};
		declaration = (Decl){nil, Dconst, 0, nil, &symbol};
		second = (Decl){nil, Dconst, 0, nil, &other};
		left = (Node){Oconst, tstring, nil, nil, &declaration, {11}, 0, 0};
		right = (Node){Oconst, tstring, nil, nil, &second, {12}, 0, 0};
		alias = (Decl){nil, Dconst, 0, &left, nil};
		name = (Node){Oname, tstring, nil, nil, &alias, {13}, 0, 0};
		addition = (Node){Oadd, tstring, &left, &right, nil, {14}, 0, 0};
		length = (Node){Olen, tint, mode == 0 ? &left : mode == 1 ? &name : &addition,
			nil, nil, {99}, 0, 0};
		result = efold(&length);
		assert(result->op == Oconst && result->ty == tint && result->src.start == 99);
		assert(result->val == vectors[i].count && result->val == runtime);
		checks++;
	}
}

/* Include leading, interior and trailing NULs, plus one- through four-byte Unicode characters. */
int
main(void)
{
	int i;

	for(i = 0; i < sizeof(vectors)/sizeof(vectors[0]); i++)
		checklength(i);
	printf("Compiler string lengths: %d literal/alias/concat/VM cases, WORD%d\n", checks, WORD_BITS);
	return 0;
}
