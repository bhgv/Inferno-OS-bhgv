#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef int64_t vlong;
typedef uint64_t uvlong;
IMPLEMENTATION

static unsigned int checks;
static uint64_t seed = 0x737472746f6c6c;

/* Generate full-width values without signed arithmetic in the test oracle. */
static uint64_t
nextvalue(void)
{
	seed ^= seed << 13;
	seed ^= seed >> 7;
	seed ^= seed << 17;
	return seed;
}

/* Format any unsigned bit pattern in a base accepted by the library parser. */
static void
format(char *out, uint64_t value, unsigned int base)
{
	char reversed[65];
	const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	int n;

	n = 0;
	do {
		reversed[n++] = digits[value%base];
		value /= base;
	} while(value);
	while(n)
		*out++ = reversed[--n];
	*out = 0;
}

/* Compare all result bits and the exact end pointer, including negative input. */
static void
checkvalue(uint64_t value, int base)
{
	char text[80], negative[80], *end;
	size_t n;

	format(text, value, base);
	n = strlen(text);
	text[n] = ';';
	text[n+1] = 0;
	assert(test_strtoull(text, &end, base) == value && end == text+n);
	assert((uint64_t)test_strtoll(text, &end, base) == value && end == text+n);
	negative[0] = '-';
	memcpy(negative+1, text, n+2);
	assert(test_strtoull(negative, &end, base) == (uint64_t)0-value);
	assert(end == negative+n+1);
	assert((uint64_t)test_strtoll(negative, &end, base) == (uint64_t)0-value);
	assert(end == negative+n+1);
	checks += 4;
}

/* Exercise upper-half values, overflow saturation and invalid-base recovery. */
int
main(void)
{
	char text[400], *end;
	char *prefix;
	int base, i;
	size_t n;
	uint64_t edges[] = {0, 1, INT64_MAX, (uint64_t)1<<63, UINT64_MAX};

	assert(test_strtoull("18446744073709551615", &end, 10) == UINT64_MAX);
	for(base = 2; base <= 36; base++) {
		for(i = 0; i < (int)(sizeof(edges)/sizeof(edges[0])); i++)
			checkvalue(edges[i], base);
		for(i = 0; i < 128; i++)
			checkvalue(nextvalue(), base);
		format(text, UINT64_MAX, base);
		n = strlen(text);
		text[n] = '0';
		text[n+1] = ';';
		text[n+2] = 0;
		assert(test_strtoull(text, &end, base) == UINT64_MAX);
		assert(end == text+n+1);
		checks++;
	}
	memset(text, '9', 300);
	text[300] = ';';
	text[301] = 0;
	assert(test_strtoull(text, &end, 10) == UINT64_MAX && end == text+300);
	for(base = -2; base <= 1; base++) {
		if(base == 0)
			continue;
		assert(test_strtoull(text, &end, base) == 0 && end == text);
		assert(test_strtoll(text, &end, base) == 0 && end == text);
	}
	assert(test_strtoull(text, &end, 37) == 0 && end == text);
	assert(test_strtoll(text, &end, 37) == 0 && end == text);
	prefix = "  -0xff!";
	assert(test_strtoull(prefix, &end, 0) == UINT64_MAX-254);
	assert(*end == '!');
	prefix = "0x";
	assert(test_strtoull(prefix, &end, 0) == 0 && end == prefix);
	assert(test_strtoll(prefix, &end, 0) == 0 && end == prefix);
	printf("PASS integer conversion: %u base/sign/full-width checks and overflow/end pointers\n",
		checks);
	return 0;
}
