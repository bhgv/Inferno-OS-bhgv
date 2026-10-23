#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef unsigned char uchar;
typedef int64_t Long;
typedef uint64_t uvlong;
#define nil NULL
#define StrSize 256
#define Beof (-1)
CONSTANTS

enum {Lconst = 1, Lrconst};
static unsigned char map[256];
static struct {
	struct {
		union {
			Long ival;
			double rval;
		} v;
	} tok;
} yylval;
static const unsigned char *input;
static int position, errors, checks, lastread;

/* Deliver a byte stream with EOF distinct from the last valid input byte. */
static int
Getc(void)
{
	if(input[position] == 0)
		lastread = Beof;
	else
		lastread = input[position++];
	return lastread;
}

/* Production unGetc ignores EOF; retain the delimiter for the next token. */
static void
unGetc(void)
{
	if(lastread != Beof)
		position--;
}

/* Record scanner diagnostics without depending on compiler source locations. */
static void
yyerror(char *format, ...)
{
	(void)format;
	errors++;
}

/* Supply the legacy parser dependency when testing an older revision. */
static double
rpow(double base, int exponent)
{
	return pow(base, exponent);
}

/* LeakSanitizer process inspection is unavailable in the test sandbox. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Check a token value, diagnostic count and retention of its delimiter. */
static void
check(const char *text, int token, Long value, int diagnostic)
{
	int got;

	input = (const unsigned char*)text;
	position = 1;
	errors = 0;
	got = lexnum(text[0]);
	assert(got == token && errors == diagnostic);
	if(token == Lconst)
		assert(yylval.tok.v.ival == value);
	else
		assert(yylval.tok.v.rval == (double)value);
	if(strchr(text, ';'))
		assert(Getc() == ';');
	else
		assert(Getc() == Beof);
	checks++;
}

/* Keep long radix fractions finite and preserve radix letters as digits. */
static void
checkfractions(void)
{
	char text[256];
	int i;

	memcpy(text, "36r0.", 5);
	memset(text+5, 'z', 245);
	text[250] = ';';
	text[251] = 0;
	input = (const unsigned char*)text;
	position = 1;
	errors = 0;
	assert(lexnum(text[0]) == Lrconst && errors == 0);
	assert(isfinite(yylval.tok.v.rval));
	assert(fabs(yylval.tok.v.rval-1.0) < 1e-15);
	assert(Getc() == ';');
	assert(strtodb("0.fe", 16) == 254.0/256.0);
	assert(strtodb("-0.01", 2) == -0.25);
	for(i = 1; i < 240; i++) {
		text[0] = '0';
		text[1] = '.';
		memset(text+2, '0', i-1);
		text[i+1] = '1';
		text[i+2] = 0;
		assert(strtodb(text, 2) == ldexp(1.0, -i));
	}
}

/* Initialize the actual scanner character classes used by numeric states. */
static void
initmap(void)
{
	int c;

	for(c = '0'; c <= '9'; c++)
		map[c] = Mdigit;
	for(c = 'a'; c <= 'z'; c++)
		map[c] = Mlower;
	for(c = 'A'; c <= 'Z'; c++)
		map[c] = Mupper;
	map['e'] |= Mexp;
	map['E'] |= Mexp;
	map['r'] |= Mradix;
	map['R'] |= Mradix;
	map['+'] |= Msign;
	map['-'] |= Msign;
	map[0xc3] = Munder; /* production lexer treats non-ASCII bytes as identifier bytes */
}

/* Cover EOF, malformed tails, radix pointers and the complete token boundary. */
int
main(void)
{
	char text[600], radix[70];
	int length;

	initmap();
	checkfractions();
	assert(strtoi("-9223372036854775808", 10) == INT64_MIN);
	assert(strtoi("18446744073709551615", 10) == -1);
	assert(strtoi("18446744073709551616", 10) == 0);
	assert(strtoi("ffffffffffffffff", 16) == -1);
	/* Invalid radix digits, including UTF-8 bytes, must not be accumulated. */
	errors = 0;
	assert(strtoi("\xc3", 10) == -1 && errors == 1);
	errors = 0;
	assert(digit('z', 10) == -1 && errors == 1);
	errors = 0;
	assert(digit(0xc3, 10) == -1 && errors == 1);
	memset(radix, '1', 64);
	radix[64] = 0;
	assert(strtoi(radix, 2) == -1);
	check("123", Lconst, 123, 0);
	check("123;", Lconst, 123, 0);
	check("16rff", Lconst, 255, 0);
	check("16rff;", Lconst, 255, 0);
	check("1.5e2", Lrconst, 150, 0);
	check("1e2;", Lrconst, 100, 0);
	check("1e", Lconst, 0, 1);
	check("1e+", Lconst, 0, 1);
	check("16r", Lconst, 0, 1);
	for(length = StrSize-2; length <= StrSize+20; length++) {
		memset(text, '0', length);
		text[length] = 0;
		check(text, Lconst, 0, length >= StrSize);
		text[length] = ';';
		text[length+1] = 0;
		check(text, Lconst, 0, length >= StrSize);
	}
	/* A radix marker at the buffer edge used to leave base past buf. */
	memset(text, '0', StrSize-1);
	text[StrSize-1] = 'r';
	text[StrSize] = '1';
	text[StrSize+1] = ';';
	text[StrSize+2] = 0;
	check(text, Lconst, 0, 1);
	puts("PASS numeric lexer: EOF, radix buffer boundaries, long fractions, long tokens and 64-bit accumulation");
	return 0;
}
