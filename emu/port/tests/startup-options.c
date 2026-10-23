#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
/* Process inspection for LeakSanitizer is unavailable; this fixture allocates no heap blocks. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define nil NULL
#define nelem(a) (sizeof(a)/sizeof((a)[0]))
typedef unsigned char uchar;
static char qsep[] = " \t\r\n";
static int calls, expected, style, checks;
static int realtokenize(char*, char**, int);

/* Record the real tokenizer's vector contract before it can write any entries. */
static int
tokenize(char *s, char **argv, int capacity)
{
	assert(capacity > 0 && capacity <= 19);
	return realtokenize(s, argv, capacity);
}

/* Inspect the complete argument vector at the actual option-dispatch boundary. */
static void
option(int argc, char **argv, void (*usage)(void))
{
	int i;
	char token[64];

	(void)usage;
	calls++;
	assert(argc == expected+1 && argc < 20);
	assert(strcmp(argv[0], "emu") == 0 && argv[argc] == NULL);
	for(i = 1; i < argc; i++){
		snprintf(token, sizeof token, style == 0 ? "-v%d" : style == 1 ? "arg %d" : "it's %d", i-1);
		assert(strcmp(argv[i], token) == 0);
	}
}

/* Usage exits in production; expose that boundary without invoking the emulator. */
static void
envusage(void)
{
	abort();
}
#define envusage() return -1
IMPLEMENTATION

/* Exercise empty input, quoted words, exact capacity and excess words on both launchers. */
int
main(void)
{
	char input[16384], token[80];
	int n, i, launcher, result;
	int (*launch[])(char*) = {portable, android};

	for(launcher = 0; launcher < 2; launcher++)
	for(style = 0; style < 3; style++)for(n = 0; n <= 256; n++){
		input[0] = 0;
		for(i = 0; i < n; i++){
			snprintf(token, sizeof token, style == 0 ? " -v%d\t" : style == 1 ? " 'arg %d'\n" : " 'it''s %d'\r", i);
			assert(strlen(input)+strlen(token) < sizeof input);
			strcat(input, token);
		}
		expected = n;
		calls = 0;
		result = launch[launcher](input);
		if(n <= 18)
			assert(result == n+1 && calls == 1);
		else
			assert(result == -1 && calls == 0);
		checks++;
	}
	printf("Startup options: %d capacity/quote/dispatch cases\n", checks);
	return 0;
}
