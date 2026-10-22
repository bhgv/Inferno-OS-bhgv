#include "lib9.h"
#include <assert.h>
#include <stdio.h>

static int checks;
struct Input { const char *text; int length, position, last; };
/* Return an EOF sentinel without ever reading beyond the test input. */
static int
next(void *arg)
{
	struct Input *input = arg;
	int c;

	c = input->position < input->length ? (uchar)input->text[input->position] : -1;
	input->position++; input->last = c;
	assert(input->position <= input->length+1);
	return c;
}
/* Provide the native invalid-number value without pulling in another parser. */
double NaN(void) { return __builtin_nan(""); }
/* LeakSanitizer cannot inspect processes in this sandbox. */
int __lsan_is_turned_off(void) { return 1; }
IMPLEMENTATION

/* Match independently parsed values and exact callback termination on ordinary input. */
static void
ordinary(void)
{
	char *cases[] = {"0", "-0", "+1.25", "\t -2.5e+3", ".125", "1.",
		"1e-10", "1E+10", "Infinity", "-INFINITY", "nan", "-NaN", "+NAN"};
	char text[128], *end;
	struct Input input;
	double actual, expected;
	int i, eof, n;

	for(i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) for(eof = 0; eof < 2; eof++){
		n = strlen(cases[i]); memmove(text, cases[i], n);
		if(!eof)text[n++] = ';'; text[n] = 0;
		input.text = text; input.length = n; input.position = 0;
		expected = strtod(cases[i], &end); actual = charstod(next, &input);
		if(expected != expected)assert(actual != actual);
		else assert(actual == expected && (actual != 0 || signbit(actual) == signbit(expected)));
		assert(input.position == n+eof && input.last == (eof ? -1 : ';')); checks++;
	}
}

/* Long mantissas and exponents still consume exactly one complete token. */
static void
longtokens(void)
{
	char text[2100], *p;
	struct Input input;
	double value;
	int length, form, eof, n;

	for(length = 390; length <= 2000; length += 7)
	for(form = 0; form < 4; form++) for(eof = 0; eof < 2; eof++){
		p = text;
		if(form == 1)*p++ = '-';
		if(form == 2){ *p++ = '1'; *p++ = 'e'; *p++ = '+'; }
		if(form == 3){ *p++ = '1'; *p++ = '.'; }
		memset(p, '1', length); p += length;
		n = p-text;
		if(!eof)*p++ = ';'; *p = 0;
		input.text = text; input.length = p-text; input.position = 0;
		value = charstod(next, &input);
		assert(input.position == input.length+eof);
		assert(input.last == (eof ? -1 : ';'));
		if(n > 399)assert(value != value);
		else assert(value == strtod(text, nil));
		checks++;
	}
	/* The exact storage boundary has room for 399 bytes plus its NUL. */
	memset(text, '0', 399); text[399] = ';';
	input.text = text; input.length = 400; input.position = 0;
	assert(charstod(next, &input) == 0 && input.position == 400); checks++;
}

/* Exercise only scanner behavior; conversion delegates to host strtod in this fixture. */
int
main(void)
{
	if(getenv("CHARSTOD_LONG_ONLY") == nil)ordinary();
	longtokens();
	printf("PASS charstod: %d value/sign/overflow/termination cases\n", checks);
	return 0;
}
