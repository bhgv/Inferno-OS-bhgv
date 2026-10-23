#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char uchar;
typedef unsigned int Rune;
typedef struct Sym Sym;
struct Sym { int token; char *name; int len, hash; Sym *next; };
enum { HashSize = 1024, Lid = 100, Ltid = 101, StrSize = 256, Runeself = 128, Malpha = 1, Mdigit = 2, UTFmax = 4, Beof = -1, Linestart = 0 };
#define nil NULL
static Sym *symbols[HashSize], *strings[HashSize];
static struct { struct { struct { Sym *idval; } v; } tok; } yylval;
static int lineno, linepos, errors, isfatal, diagnostics, growths;
static void *bout, *bsym;
static char *outfile, *symfile;
static const char *input;
static int at, exit_failure, eof;
static jmp_buf failed;
static void fatal(char*, ...);

/* Poison new storage so empty strings cannot depend on allocator contents. */
static void*
allocmem(size_t n)
{
	void *p = malloc(n);
	assert(p);
	memset(p, 0xa5, n);
	return p;
}

/* Count growth operations to catch quadratic string buffer expansion. */
static void*
reallocmem(void *p, size_t n)
{
	growths++;
	p = realloc(p, n);
	assert(p);
	return p;
}

/* Feed ASCII fixtures; byte interning below separately covers all high bytes. */
static int
getrune(void)
{
	eof = input[at] == 0;
	return eof ? Beof : (uchar)input[at++];
}
/* Return identifier classification and preserve the first nonidentifier byte. */
static int
cmap(int c)
{
	return c >= 'a' && c <= 'z' ? Malpha : 0;
}
/* Mirror production EOF handling when returning the delimiter to the input. */
static void
ungetrune(void)
{
	if(!eof)
		at--;
}
/* These fixtures use single-byte escaped characters. */
static int
escchar(void)
{
	return getrune();
}
/* Encode the ASCII fixture without replacing production buffer management. */
static int
runetochar(char *p, Rune *r)
{
	*p = *r;
	return 1;
}
/* Count lexer diagnostics independently of their human-readable text. */
static void
yyerror(char *fmt, ...)
{
	(void)fmt;
	diagnostics++;
}
/* Supply the formatting boundary used by the production fatal diagnostic. */
static char*
vseprint(char *p, char *e, char *fmt, va_list ap)
{
	vsnprintf(p, e-p, fmt, ap);
	return p + strlen(p);
}
/* Suppress expected fatal diagnostics in the test output. */
static void
fprint(int fd, char *fmt, ...)
{
	(void)fd;
	(void)fmt;
}

/* Verify fatal status without exiting the harness. */
static void
exits(char *status)
{
	exit_failure = status && *status;
	longjmp(failed, 1);
}

/* LeakSanitizer cannot enumerate threads here; the intern tables own live symbols. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Check both empty literal forms, error exits, and long buffer growth. */
static void
literals(void)
{
	char *longtext;
	Sym *empty = enter("", 0);
	input = "\""; at = 0; lexstring(0);
	assert(yylval.tok.v.idval == empty);
	input = "`"; at = 0; lexstring(1);
	assert(yylval.tok.v.idval == empty);
	input = ""; at = 0; lexstring(0);
	assert(yylval.tok.v.idval == empty && diagnostics == 1);
	longtext = malloc(100002);
	memset(longtext, 'a', 100000);
	longtext[100000] = '"'; longtext[100001] = 0;
	input = longtext; at = 0; growths = 0; lexstring(0);
	assert(yylval.tok.v.idval->len == 100000 && growths < 20);
	memset(longtext, 'a', 256); longtext[256] = 0;
	input = longtext; at = 1; diagnostics = 0;
	assert(lexid('a') == Lid && diagnostics == 0);
	longtext[256] = 'b'; longtext[257] = ';'; longtext[258] = 0;
	at = 1;
	assert(lexid('a') == Lid && diagnostics == 1 && input[at] == ';');
	free(longtext);
}

/* Exercise interning, embedded NULs, concat bounds, and zero-sized diagnostics. */
int
main(void)
{
	int i;
	char name[2], bytes[256], guard[2] = {'x', 'y'};
	Sym *s, *t, huge = {0};
	literals();
	for(i = 1; i < 256; i++){
		name[0] = i; name[1] = 0;
		s = enter(name, 0);
		assert(s == enter(name, 0) && s->len == 1);
		bytes[i] = i;
	}
	bytes[0] = 0;
	s = enterstring(memcpy(allocmem(256), bytes, 256), 256);
	t = enterstring(memcpy(allocmem(256), bytes, 256), 256);
	assert(s == t);
	t = stringcat(s, s);
	assert(t->len == 512 && !memcmp(t->name, bytes, 256));
	assert(secpy(guard, guard, "abc") == guard);
	assert(guard[0] == 'x' && guard[1] == 'y');
	assert(secpy(guard, guard+1, "abc") == guard+1 && guard[0] == 0);
	huge.len = INT_MAX;
	errors = 1;
	if(setjmp(failed) == 0){ stringcat(&huge, s); abort(); }
	assert(exit_failure);
	puts("lexer strings: ok");
	return 0;
}
