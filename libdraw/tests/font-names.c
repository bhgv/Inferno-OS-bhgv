#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long ulong;
#define nil NULL
#define snprint snprintf
static int failalloc;

/* Inject allocation failure while retaining the host allocator for fixtures. */
static void*
fontalloc(size_t n)
{
	return failalloc ? NULL : malloc(n);
}

/* LeakSanitizer cannot enumerate threads in the sandbox. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define malloc fontalloc
IMPLEMENTATION
#undef malloc

/* Verify exact suffix matching and independent full-length cache-key formatting. */
int
main(void)
{
	static const char *names[] = {"", "a", "ab", "abc", ".ttf", "a.TTF",
		"a.TtF", "a.ttfx", "a.ttf/", "ttf", "a.otf", "тест.ttf"};
	static const int sizes[] = {1, 12, 255, INT_MAX, INT_MIN, -1};
	char *name, *key, *expected;
	size_t i, j, n;
	int valid, checks = 0;

	assert(mangle_subfont_ft(NULL, 12) == NULL);
	for(i = 0; i < sizeof names/sizeof names[0]; i++){
		name = strdup(names[i]);
		assert(name != NULL);
		valid = i == 4 || i == 5 || i == 6 || i == 11;
		assert(is_subfont_ft(name) == valid);
		free(name);
		checks++;
	}
	assert(is_subfont_ft(NULL) == 0);
	for(n = 0; n <= 8192; n += n < 270 ? 1 : 257){
		name = malloc(n+1);
		expected = malloc(n+32);
		assert(name != NULL && expected != NULL);
		memset(name, 'x', n);
		name[n] = 0;
		assert(mangle_subfont_ft(name, 0) == NULL);
		for(j = 0; j < sizeof sizes/sizeof sizes[0]; j++){
			snprintf(expected, n+32, "%s[%d]", name, sizes[j]);
			key = mangle_subfont_ft(name, sizes[j]);
			assert(key != NULL && strcmp(key, expected) == 0);
			free(key);
			checks++;
		}
		failalloc = 1;
		assert(mangle_subfont_ft(name, 12) == NULL);
		failalloc = 0;
		free(name);
		free(expected);
	}
	printf("font names: %d checks passed\n", checks);
	return 0;
}
