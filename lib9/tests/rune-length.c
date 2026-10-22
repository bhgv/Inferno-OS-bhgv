#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef unsigned char uchar;
typedef uint32_t Rune;
enum {
	UTFmax = 4,
	Runeerror = 0xfffd,
	Runemax = 0x10ffff
};
IMPLEMENTATION

static unsigned int checks;

/* Check encoded length, replacement decoding and untouched trailing bytes. */
static void
check(Rune rune)
{
	char bytes[UTFmax+2];
	Rune decoded, want;
	int n;

	memset(bytes, 0x5a, sizeof(bytes));
	n = runetochar(bytes, &rune);
	assert(n >= 1 && n <= UTFmax);
	assert(runenlen(&rune, 1) == n);
	assert(bytes[n] == 0x5a && bytes[n+1] == 0x5a);
	assert(chartorune(&decoded, bytes) == n);
	want = rune;
	if(rune > Runemax || (rune >= 0xd800 && rune <= 0xdfff))
		want = Runeerror;
	assert(decoded == want);
	checks++;
}

/* Exhaust valid code points and invalid ranges used by VM string storage. */
int
main(void)
{
	Rune rune;
	Rune mixed[] = {'A', 0x7ff, 0xffff, 0x10000, 0x110000, UINT32_MAX};
	char buffer[sizeof(mixed)*UTFmax];
	int i, written;

	for(rune = 0; rune <= Runemax+4096; rune++)
		check(rune);
	check(UINT32_MAX);
	check(0x80000000);
	written = 0;
	for(i = 0; i < (int)(sizeof(mixed)/sizeof(mixed[0])); i++)
		written += runetochar(buffer+written, &mixed[i]);
	assert(runenlen(mixed, sizeof(mixed)/sizeof(mixed[0])) == written);
	assert(runenlen(NULL, 0) == 0);
	assert(runenlen(NULL, -1) == 0);
	assert(runenlen(NULL, INT_MIN) == 0);
	printf("PASS UTF lengths: %u scalar/replacement checks and mixed buffer sizing\n",
		checks);
	return 0;
}
