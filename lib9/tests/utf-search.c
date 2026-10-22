#include "lib9.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static int checks;
/* LeakSanitizer cannot inspect processes in this sandbox. */
int __lsan_is_turned_off(void) { return 1; }

/* Count independently generated Unicode sequences at every byte boundary. */
static void
prefixes(void)
{
	char *texts[] = {"", "ascii", "A\xc2\xa2\xe2\x82\xac\xf0\x9f\x98\x80Z",
		"\xff" "A", "\xe2\x82", "\xc2"};
	int ends[][8] = {{0}, {1,2,3,4,5,0}, {1,3,6,10,11,0}, {1,2,0}, {0}, {0}};
	int t, limit, expected, i, length;

	assert(utfnlen(nil, 0) == 0 && utfnlen(nil, -1) == 0);
	assert(utfnlen(nil, LONG_MIN) == 0);
	for(t = 0; t < sizeof(texts)/sizeof(texts[0]); t++){
		length = strlen(texts[t]);
		for(limit = 0; limit <= length+8; limit++){
			expected = 0;
			for(i = 0; ends[t][i] && ends[t][i] <= limit; i++)expected++;
			if(t >= 4 && limit > length)expected = length;
			if(t == 3)expected = limit >= UTFmax ? 2 : 0;
			assert(utfnlen(texts[t], limit) == expected); checks++;
		}
		/* A very large maximum is valid for a short NUL-terminated string. */
		expected = 0;
		for(i = 0; ends[t][i]; i++)expected++;
		if(t >= 4)expected = length;
		assert(utfnlen(texts[t], LONG_MAX) == expected); checks++;
	}
}

/* Match exact first/last positions, malformed-byte error runes and invalid values. */
static void
searches(void)
{
	char text[] = "a\xc2\xa2" "b\xe2\x82\xac" "a\xc2\xa2";
	char malformed[] = {'a', (char)0xff, 'b', (char)0xff, 0};
	long invalid[] = {-1, -128, -256, -255, LONG_MIN, 0x110000, LONG_MAX};
	int i;

	assert(utfrune(text, 'a') == text && utfrrune(text, 'a') == text+7);
	assert(utfrune(text, 0xa2) == text+1 && utfrrune(text, 0xa2) == text+8);
	assert(utfrune(text, 0x20ac) == text+4 && utfrrune(text, 0x20ac) == text+4);
	assert(utfrune(text, 0) == text+strlen(text) && utfrrune(text, 0) == text+strlen(text));
	assert(utfrune(text, 'z') == nil && utfrrune(text, 'z') == nil);
	assert(utfrune(malformed, Runeerror) == malformed+1);
	assert(utfrrune(malformed, Runeerror) == malformed+3);
	for(i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++){
		assert(utfrune(malformed, invalid[i]) == nil);
		assert(utfrrune(malformed, invalid[i]) == nil); checks++;
	}
}

/* Native UTF helpers must preserve their search and prefix compatibility. */
int
main(void)
{
	prefixes(); searches();
	printf("PASS native UTF: %d prefix/search/invalid-value cases\n", checks);
	return 0;
}
