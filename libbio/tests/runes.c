#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char uchar;
typedef int64_t vlong;
typedef uint32_t Rune;
enum {
	UTFmax = 4,
	Runeself = 0x80,
	Runeerror = 0xfffd,
	Runemax = 0x10ffff
};
#include "bio.h"

static const uchar *input;
static int inputsize, inputpos, fragmentsize;
static int outputfailure, outputsize, checks;
static char output[UTFmax];

/* Feed short reads through the real Bgetc refill and unget implementation. */
static int
testread(int fd, void *buffer, int capacity)
{
	int n;

	(void)fd;
	n = inputsize-inputpos;
	if(n > fragmentsize)
		n = fragmentsize;
	if(n > capacity)
		n = capacity;
	memmove(buffer, input+inputpos, n);
	inputpos += n;
	return n;
}
#define read testread

/* Model Bputc's zero success result and EOF failure without a real file. */
int
Bputc(Biobuf *bp, int c)
{
	(void)bp;
	if(outputfailure)
		return Beof;
	output[outputsize++] = c;
	return 0;
}

/* Capture multibyte writes or fail them in the same way as the ASCII path. */
long
Bwrite(Biobuf *bp, void *buffer, long n)
{
	(void)bp;
	if(outputfailure)
		return Beof;
	assert(n <= UTFmax);
	memmove(output, buffer, n);
	outputsize = n;
	return n;
}

/* LeakSanitizer process inspection is unavailable in the test sandbox. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Check every rune twice through Bungetrune, including the replacement rune. */
static void
checkinput(const uchar *bytes, int size, const Rune *runes, int count)
{
	Biobuf buffer;
	int fragment, i;
	long got;

	for(fragment = 1; fragment <= 7; fragment++) {
		memset(&buffer, 0, sizeof(buffer));
		buffer.bbuf = buffer.b+Bungetsize;
		buffer.ebuf = buffer.bbuf+16;
		buffer.bsize = 16;
		buffer.state = Bractive;
		input = bytes;
		inputsize = size;
		inputpos = 0;
		fragmentsize = fragment;
		for(i = 0; i < count; i++) {
			got = Bgetrune(&buffer);
			assert(got == runes[i]);
			assert(buffer.runesize >= 1 && buffer.runesize <= UTFmax);
			assert(Bungetrune(&buffer) == 1);
			assert(Bgetrune(&buffer) == got);
			checks++;
		}
		assert(Bgetrune(&buffer) == Beof);
		assert(buffer.runesize == 0);
		Bungetrune(&buffer);
		assert(Bgetrune(&buffer) == Beof);
	}
}

/* Write failures must be observable for both ASCII and multibyte characters. */
static void
checkoutput(long rune)
{
	Biobuf buffer;
	Rune value;
	char expected[UTFmax];
	int n;

	memset(&buffer, 0, sizeof(buffer));
	value = rune;
	n = runetochar(expected, &value);
	outputsize = 0;
	outputfailure = 0;
	assert(Bputrune(&buffer, rune) == n);
	assert(outputsize == n && memcmp(output, expected, n) == 0);
	outputsize = 0;
	outputfailure = 1;
	assert(Bputrune(&buffer, rune) == Beof);
	assert(outputsize == 0);
	checks++;
}

/* Cover legal replacement encoding, malformed prefixes and truncated input. */
int
main(void)
{
	static const uchar valid[] = {'A', 0xef, 0xbf, 0xbd, 0xc2, 0xa2,
		0xf0, 0x9f, 0x98, 0x80, 'Z'};
	static const Rune decoded[] = {'A', Runeerror, 0xa2, 0x1f600, 'Z'};
	static const uchar bad[] = {0xc2, 'A', 0xe1, 0x80, 'B', 0xff, 'C'};
	static const Rune replacement[] = {Runeerror, 'A', Runeerror,
		Runeerror, 'B', Runeerror, 'C'};
	static const uchar tail[] = {'Q', 0xf0, 0x9f};
	static const Rune truncated[] = {'Q', Runeerror, Runeerror};

	checkinput(valid, sizeof(valid), decoded, sizeof(decoded)/sizeof(decoded[0]));
	checkinput(bad, sizeof(bad), replacement, sizeof(replacement)/sizeof(replacement[0]));
	checkinput(tail, sizeof(tail), truncated, sizeof(truncated)/sizeof(truncated[0]));
	checkinput(valid, 0, decoded, 0);
	checkoutput('A');
	checkoutput(0);
	checkoutput(0xa2);
	checkoutput(0x1f600);
	checkoutput(Runeerror);
	printf("PASS buffered runes: %d refill/unget/error cases across seven fragment sizes\n",
		checks);
	return 0;
}
