#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef unsigned char uchar;
typedef unsigned long ulong;
typedef struct PEMChain PEMChain;
struct PEMChain {
	PEMChain *next;
	uchar *pem;
	int pemlen;
};
#define nil NULL
static int attempts, failat, live, checks;

/* Inject each allocation failure while checking that all owners are released. */
static void*
checkedmalloc(size_t size)
{
	void *p;

	if(++attempts == failat)
		return nil;
	p = malloc(size);
	assert(p != nil);
	live++;
	return p;
}

/* Count payload and chain-node releases. */
static void
checkedfree(void *p)
{
	assert(p != nil && live > 0);
	live--;
	free(p);
}

/* Error text is not interpreted by these parsing and ownership tests. */
static int
werrstr(char *fmt, ...)
{
	(void)fmt;
	return 0;
}

/* Disable unsupported process inspection; the fixture counts every live allocation. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
#define malloc checkedmalloc
#define free checkedfree
IMPLEMENTATION
#undef malloc
#undef free

/* Free the exact valid prefix returned by the chain API. */
static int
releasechain(PEMChain *p)
{
	PEMChain *next;
	int n;

	n = 0;
	while(p != nil){
		next = p->next;
		assert(p->pemlen == 3 && memcmp(p->pem, "abc", 3) == 0);
		checkedfree(p->pem);
		checkedfree(p);
		p = next;
		n++;
	}
	return n;
}

/* Check exact content and continuation for both newline conventions and EOF trailers. */
static void
boundaries(void)
{
	char input[512], *next, *end, *nl[] = {"\n", "\r\n"};
	uchar *d;
	int i, final, n;

	for(i = 0; i < 2; i++)
	for(final = 0; final < 2; final++){
		snprintf(input, sizeof(input), "ignored%s-----BEGIN CERTIFICATE-----%sYWJj%s-----END CERTIFICATE-----%s",
			nl[i], nl[i], nl[i], final ? nl[i] : "");
		end = input+strlen(input);
		next = nil;
		d = decodePEM(input, "CERTIFICATE", &n, &next);
		assert(d != nil && n == 3 && memcmp(d, "abc", 3) == 0 && next == end);
		checkedfree(d);
		checks++;
	}
}

/* Truncate at every byte, using exact allocations so ASan catches boundary overreads. */
static void
truncations(void)
{
	char full[] = "-----BEGIN X-----\nYWJj\n-----END X-----\n";
	char *input, *next;
	uchar *d;
	int i, n;

	for(i = 0; i <= strlen(full); i++){
		input = malloc(i+1);
		memcpy(input, full, i);
		input[i] = 0;
		next = input;
		d = decodePEM(input, "X", &n, &next);
		if(i >= strlen(full)-1){
			assert(d != nil && n == 3 && next == input+i);
			checkedfree(d);
		}else
			assert(d == nil && n == 0 && next == input);
		free(input);
		checks++;
	}
}

/* Verify empty payloads and exact type/line matching without publishing failure cursors. */
static void
formats(void)
{
	char *cases[] = {
		"-----BEGIN X-----\n-----END X-----",
		"-----BEGIN XY-----\nYWJj\n-----END X-----\n",
		"-----BEGIN X-----suffix\nYWJj\n-----END X-----\n",
		"-----BEGIN X-----\nYWJj\n-----END XY-----\n",
		"-----BEGIN X-----\nYWJj\nprefix-----END X-----\n",
		"-----END X-----\n"
	};
	char *next;
	uchar *d;
	int i, n;

	for(i = 0; i < sizeof(cases)/sizeof(cases[0]); i++){
		next = cases[i];
		d = decodePEM(cases[i], "X", &n, &next);
		if(i == 0){
			assert(d != nil && n == 0 && *next == 0);
			checkedfree(d);
		}else
			assert(d == nil && n == 0 && next == cases[i]);
		checks++;
	}
}

/* Fail all payload and node allocations in a two-block chain. */
static void
failures(void)
{
	char input[] = "-----BEGIN X-----\nYWJj\n-----END X-----\n"
		"-----BEGIN X-----\r\nYWJj\r\n-----END X-----";
	PEMChain *p;
	int i, expected;

	for(i = 0; i <= 4; i++){
		attempts = 0;
		failat = i;
		p = decodepemchain(input, "X");
		expected = i == 0 ? 2 : (i > 2 ? 1 : 0);
		assert(releasechain(p) == expected && live == 0);
		checks++;
	}
	failat = 0;
}

/* Exercise the production parser, decoder, continuation, and allocation ownership. */
int
main(void)
{
	boundaries();
	truncations();
	formats();
	failures();
	assert(live == 0);
	printf("PEM: %d format, truncation, cursor and allocation cases passed\n", checks);
	return 0;
}
