#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned long ulong;
/* Process inspection for LeakSanitizer is unavailable; this fixture allocates no heap blocks. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define nil NULL
static int Xsize, Ysize;
IMPLEMENTATION

/* Check success dimensions and transactional rejection against labelled expectations. */
static void
one(int (*parse)(char*), char *text, int accepted, long x, long y)
{
	int result;

	Xsize = 640;
	Ysize = 480;
	result = parse(text);
	assert(result == accepted);
	assert(Xsize == (accepted && x >= 64 ? x : 640));
	assert(Ysize == (accepted && y >= 48 ? y : 480));
}

/* Preserve established decimal/octal syntax while rejecting overflow and trailing junk. */
int
main(void)
{
	struct Dimension {
		char *text;
		long value;
		int xvalid, yvalid;
	} dimensions[] = {
		{"00", 0, 1, 1}, {"47", 47, 1, 1}, {"48", 48, 1, 1},
		{"63", 63, 1, 1}, {"64", 64, 1, 1}, {"640", 640, 1, 1},
		{"01000", 512, 1, 1}, {"0x280", 640, 1, 0}, {"08", 0, 0, 0},
		{"2147483647", 2147483647, 1, 1}, {"2147483648", 0, 0, 0},
		{"4294967936", 0, 0, 0}, {"18446744073709551616", 0, 0, 0},
		{"9999999999999999999999999999999999999999999", 0, 0, 0},
		{"", 0, 0, 0}, {"-1", 0, 0, 0}, {"+800", 0, 0, 0},
		{" 800", 0, 0, 0}, {"800junk", 0, 0, 0}, {"800 ", 0, 0, 0}
	};
	char input[256];
	int (*parsers[])(char*) = {portable, android};
	int p, x, y, suffix, checks = 0;

	for(p = 0; p < 2; p++){
		one(parsers[p], nil, 0, 0, 0);
		checks++;
		one(parsers[p], "0x480", 0, 0, 0);
		one(parsers[p], "0x0x280", 1, 0, 280);
		one(parsers[p], "4294967936x480", 0, 0, 0);
		one(parsers[p], "800x480junk", 0, 0, 0);
		checks += 4;
		for(x = 0; x < 20; x++)for(y = 0; y < 20; y++)for(suffix = 0; suffix < 2; suffix++){
			snprintf(input, sizeof input, "%sx%s%s", dimensions[x].text,
				dimensions[y].text, suffix ? "junk" : "");
			one(parsers[p], input, !suffix && dimensions[x].xvalid && dimensions[y].yvalid,
				dimensions[x].value, dimensions[y].value);
			checks++;
		}
	}
	printf("Startup geometry: %d spelling/range/state cases\n", checks);
	return 0;
}
