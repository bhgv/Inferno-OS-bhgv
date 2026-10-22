#include "lib9.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>

static int checks;
/* Error formatting has no dependency on a running Inferno process. */
void rerrstr(char *p, uint n) { if(n)snprintf(p, n, "fixture"); }
/* LeakSanitizer cannot inspect processes in the sandbox. */
int __lsan_is_turned_off(void) { return 1; }

/* Build grouped binary text from individual bits, independent of division formatting. */
static void
grouped(uvlong value, char *text)
{
	int bit, first, n;

	first = 63;
	while(first > 0 && !(value & ((uvlong)1<<first)))first--;
	n = 0;
	for(bit = first; bit >= 0; bit--){
		text[n++] = '0'+((value>>bit)&1);
		if(bit && bit%3 == 0)text[n++] = ',';
	}
	text[n] = 0;
}

/* Match independent libc decimal conversions at signed extrema for every size. */
static void
integers(void)
{
	vlong values[] = {(-9223372036854775807LL-1), -9223372036854775807LL,
		-2147483648LL, -1, 0, 1, 2147483647, 9223372036854775807LL};
	char actual[256], expected[256];
	int i, n;
	uvlong value;

	for(i = 0; i < sizeof(values)/sizeof(values[0]); i++){
		snprintf(expected, sizeof(expected), "%lld", (long long)values[i]);
		n = snprint(actual, sizeof(actual), "%lld", values[i]);
		assert(n == strlen(expected) && !strcmp(actual, expected)); checks++;
	}
	snprintf(expected, sizeof(expected), "%ld", LONG_MIN);
	snprint(actual, sizeof(actual), "%ld", LONG_MIN);
	assert(!strcmp(actual, expected)); checks++;
	for(i = 0; i < 64; i++){
		value = (uvlong)1<<i;
		grouped(value, expected);
		snprint(actual, sizeof(actual), "%,ullb", value);
		assert(!strcmp(actual, expected)); checks++;
		value = i == 63 ? ~(uvlong)0 : (value<<1)-1;
		grouped(value, expected);
		snprint(actual, sizeof(actual), "%,ullb", value);
		assert(!strcmp(actual, expected)); checks++;
	}
}

/* Drive the public Rune-format dispatcher with an actual bounded Fmt. */
static int
runeformat(Rune *out, int size, Rune *format, ...)
{
	Fmt f;

	memset(&f, 0, sizeof(f));
	f.runes = 1; f.start = f.to = out; f.stop = out+size-1;
	va_start(f.args, format);
	dorfmt(&f, format);
	va_end(f.args);
	*(Rune*)f.to = 0;
	return (Rune*)f.to-out;
}

/* Oversized decimal fields fail before emitting the conversion or consuming its value. */
static void
fields(void)
{
	char *formats[] = {"ok%2147483648d", "ok%.2147483648d",
		"ok%999999999999999999999999d", "ok%.999999999999999999999999d"};
	char out[256];
	Rune wide[128], result[256];
	int i, j, n;

	for(i = 0; i < sizeof(formats)/sizeof(formats[0]); i++){
		memset(out, 0xa5, sizeof(out));
		n = snprint(out, sizeof(out), formats[i], 7);
		assert(n == 2 && !strcmp(out, "ok") && (uchar)out[3] == 0xa5);
		for(j = 0; formats[i][j]; j++)wide[j] = (uchar)formats[i][j];
		wide[j] = 0;
		n = runeformat(result, 256, wide, 7);
		assert(n == 2 && result[0] == 'o' && result[1] == 'k' && result[2] == 0);
		checks++;
	}
	n = snprint(out, sizeof(out), "ok%*d", INT_MIN, 7);
	assert(n == 2 && !strcmp(out, "ok")); checks++;
	n = snprint(out, sizeof(out), "[%*d:%.*d]", -5, 7, 3, 7);
	assert(n == 11 && !strcmp(out, "[7    :007]")); checks++;
	/* Maximum representable widths stop at the bounded destination. */
	n = snprint(out, sizeof(out), "%2147483647d", 7);
	assert(n >= 0 && n < sizeof(out)); checks++;
}

/* Real native formatting covers both UTF and Rune format dispatch. */
int
main(void)
{
	integers(); fields();
	printf("PASS lib9 format: %d signed/grouped/field cases\n", checks);
	return 0;
}
