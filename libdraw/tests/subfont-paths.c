#include "lib9.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

static char base[20000], expected[20032];
static int depth, hit, attempt, failalloc, live, checks;

/* Disable unsupported thread enumeration while retaining allocator checks. */
int __lsan_is_turned_off(void) { return 1; }

/* Allocate complete path storage, or simulate an immediate allocation failure. */
static void*
talloc(ulong n)
{
	void *p;
	if(failalloc)
		return nil;
	p = malloc(n);
	assert(p != nil);
	live++;
	return p;
}

/* Count the result owner's release as well as unsuccessful search cleanup. */
static void
tfree(void *p)
{
	if(p != nil)
		live--;
	free(p);
}

/* Preserve the special built-in subfont name through allocation failures. */
static char*
tdup(char *s)
{
	char *p = talloc(strlen(s)+1);
	if(p != nil)
		strcpy(p, s);
	return p;
}

/* Match the production formatter when compiling an earlier implementation. */
int
snprint(char *s, int n, char *fmt, ...)
{
	int result;
	va_list ap;
	va_start(ap, fmt);
	result = vsnprintf(s, n, fmt, ap);
	va_end(ap);
	return result;
}

/* Slash is ASCII and cannot occur inside a UTF-8 continuation sequence. */
char* utfrrune(char *s, long c) { assert(c == '/'); return strrchr(s, c); }

/* Reject every altered or truncated candidate, even when another path exists. */
static int
pathaccess(char *s, int mode)
{
	int candidate = depth;
	assert(mode == AREAD);
	if(candidate >= 0){
		snprintf(expected, sizeof expected, "%s.%d", base, candidate);
		depth--;
	}else
		strcpy(expected, base);
	assert(strcmp(s, expected) == 0);
	return attempt++ == hit ? 0 : -1;
}

#define malloc talloc
#define free tfree
#define strdup tdup
#define access pathaccess
IMPLEMENTATION
#undef malloc
#undef free
#undef strdup
#undef access

/* Compare all successful and unsuccessful searches against an independent path. */
static void
check(char *cfname, char *fname, int maxdepth, char *want)
{
	char *result;
	int start, i, count;

	strcpy(base, want);
	start = maxdepth >= 8 ? 3 : maxdepth >= 4 ? 2 : maxdepth >= 2 ? 1 : maxdepth >= 1 ? 0 : -1;
	count = start+2;
	for(i = 0; i <= count; i++){
		depth = start; hit = i; attempt = 0;
		result = subfontname(cfname, fname, maxdepth);
		if(i == count){
			assert(result == nil && attempt == count);
		}else{
			assert(result != nil && strcmp(result, expected) == 0);
			assert(attempt == i+1 && live == 1);
			tfree(result);
		}
		assert(live == 0);
		checks++;
	}
	failalloc = 1;
	attempt = 0;
	assert(subfontname(cfname, fname, maxdepth) == nil && live == 0);
	failalloc = 0;
	checks++;
}

/* Cover relative/root/absolute names, long UTF-8 directories and suffix choices. */
int
main(void)
{
	char cf[9000], font[9000], want[19000], *s;
	int length, d;
	for(length = 0; length <= 8192; length += length < 140 ? 1 : 509){
		memset(cf, 'c', length);
		strcpy(cf+length, "glyph");
		memset(font, 'f', length);
		strcpy(font+length, "/тест/font");
		for(d = -1; d <= 10; d++){
			snprintf(want, sizeof want, "./%s", cf);
			check(cf, "font", d, want);
			snprintf(want, sizeof want, "/%s", cf);
			check(cf, "/font", d, want);
			snprintf(want, sizeof want, "%.*s/тест/%s", length, font, cf);
			check(cf, font, d, want);
			check("/absolute/glyph", font, d, "/absolute/glyph");
		}
	}
	attempt = 0;
	s = subfontname("*default*", "font", 8);
	assert(s != nil && strcmp(s, "*default*") == 0 && attempt == 0);
	tfree(s);
	failalloc = 1;
	assert(subfontname("*default*", "font", 8) == nil && live == 0);
	printf("subfont paths: %d checks passed\n", checks);
	return 0;
}
