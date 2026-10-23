#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

typedef unsigned long ulong;
#define nil NULL
#define SLASHMOD "/module"
#define INCPATH "/inferno/module"
static char *infile, *environment_emu, *environment_root;
static const char *cwd = "/inferno";
static int cwd_failed, cwd_calls, checks, live, fail_after = -1;
static void *blocks[32];
static int expected_fatal;

/* Model borrowed environment strings; production code must never alter them. */
static char*
pathgetenv(char *name)
{
	return !strcmp(name, "EMU") ? environment_emu : environment_root;
}

/* Fatal tests run in child processes and require all temporary owners released. */
static void
fatal(char *message, ...)
{
	assert(message && expected_fatal && live == 0);
	_Exit(73);
}

/* Fail before touching storage when the caller cannot obtain a working directory. */
static char*
pathgetwd(char *p, int size)
{
	cwd_calls++;
	if(cwd_failed || strlen(cwd) >= (size_t)size)
		return nil;
	strcpy(p, cwd);
	return p;
}

/* Keep allocation failures deterministic and all successful owners visible. */
static void*
allocmem(ulong n)
{
	void *p;
	int i;

	if(fail_after == 0)
		fatal("out of memory");
	if(fail_after > 0)
		fail_after--;
	p = malloc(n);
	assert(p);
	for(i = 0; i < 32 && blocks[i]; i++)
		;
	assert(i < 32);
	blocks[i] = p;
	live++;
	return p;
}

/* Check every free, including the temporary path on failed cwd lookup. */
static void
pathfree(void *p)
{
	int i;

	if(p == nil)
		return;
	for(i = 0; i < 32 && blocks[i] != p; i++)
		;
	assert(i < 32);
	blocks[i] = nil;
	live--;
	free(p);
}

/* Route legacy malloc/strdup calls through the same allocation boundary for baselines. */
static void*
pathmalloc(size_t n)
{
	return allocmem(n);
}

/* Copy borrowed strings into a tracked owner without changing their contents. */
static char*
pathstrdup(char *s)
{
	char *p = allocmem(strlen(s)+1);

	strcpy(p, s);
	return p;
}

#define malloc pathmalloc
#define strdup pathstrdup
#define getenv pathgetenv
#define getwd pathgetwd
#define free pathfree

/* LeakSanitizer cannot enumerate host threads; fixture owners are counted above. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION
#undef getenv

/* Cover attached/separated root flags, whitespace, fallback and repeatability. */
static void
roots(void)
{
	char *values[] = {"emu -r/inferno -g", "-r/inferno\t-g", "-g\t-r /inferno\t-c",
		"other-r/ignored -r/inferno", "-r", "-g", ""};
	char text[128], saved[128], fallback[] = "/fallback";
	char *result;
	int i, repeat;

	for(i = 0; i < 7; i++){
		strcpy(text, values[i]); strcpy(saved, text);
		environment_emu = text;
		environment_root = fallback;
		for(repeat = 0; repeat < 2; repeat++){
			result = getroot();
			assert(result && !strcmp(result, i < 4 ? "/inferno" : "/fallback"));
			assert(!strcmp(text, saved) && !strcmp(fallback, "/fallback"));
			free(result);
			assert(live == 0);
			checks++;
		}
	}
	environment_emu = environment_root = nil;
	assert(getroot() == nil && live == 0);
	checks++;
}

/* Root stripping must stop at a component boundary and preserve output guards. */
static void
paths(void)
{
	struct { char *root, *source, *expected; } cases[] = {
		{"/inferno", "/inferno/appl/x.b", "/appl/x.b"},
		{"/inferno", "/inferno-other/x.b", "/inferno-other/x.b"},
		{"/inferno", "/inferno", ""},
		{"/", "/appl/x.b", "appl/x.b"},
		{"/inferno", "/inferno/a/../x.b", "/x.b"},
		{"/inferno", "appl/./x.b", "/appl/x.b"},
		{"C:/inferno", "C:/inferno/APPL/X.b", "/appl/x.b"},
		{nil, "/inferno/appl/x.b", "/appl/x.b"},
	};
	char buffer[128], guard[2] = {'x', 'y'};
	int i, n, size;

	environment_emu = nil;
	for(i = 0; i < sizeof(cases)/sizeof(cases[0]); i++){
		environment_root = cases[i].root;
		infile = cases[i].source;
		for(size = 1; size <= 40; size++){
			memset(buffer, 0x5a, sizeof(buffer));
			assert(srcpath(buffer, size) == buffer);
			n = strlen(cases[i].expected);
			if(n >= size)n = size-1;
			assert(!memcmp(buffer, cases[i].expected, n) && buffer[n] == 0);
			assert(buffer[size] == 0x5a && live == 0);
			checks++;
		}
	}
	cwd_calls = 0;
	assert(srcpath(guard, 0) == guard && guard[0] == 'x' && guard[1] == 'y');
	assert(srcpath(guard, -1) == guard && !cwd_calls && !live);
	checks += 2;
}

/* Long working directories work; genuine lookup failures become compiler errors. */
static void
workingdirs(void)
{
	char longcwd[600], *result;
	int i, status;
	pid_t child;

	memset(longcwd, 'a', sizeof(longcwd));
	longcwd[0] = '/'; longcwd[sizeof(longcwd)-1] = 0;
	cwd = longcwd;
	result = cleann("source.b");
	assert(strlen(result) == strlen(cwd)+9 && !strcmp(result+strlen(cwd), "/source.b"));
	free(result); assert(live == 0); checks++;
	cwd = "/inferno";
	child = fork();
	assert(child >= 0);
	if(child == 0){
		cwd_failed = expected_fatal = 1;
		cleann("relative.b");
		_Exit(0);
	}
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
	checks++;
	/* Fatal allocation failure is terminal in production; test it in a child too. */
	child = fork();
	assert(child >= 0);
	if(child == 0){
		environment_emu = "-r/inferno";
		fail_after = 0; expected_fatal = 1;
		getroot();
		_Exit(0);
	}
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
	for(i = 0; i < 32; i++)
		assert(blocks[i] == nil);
	checks++;
}

/* Execute both checked-in compiler drivers with both host char models. */
int
main(void)
{
	const char *only = getenv("SOURCE_PATH_CASES");

	if(only == nil || !strcmp(only, "roots"))roots();
	if(only == nil || !strcmp(only, "paths"))paths();
	if(only == nil || !strcmp(only, "cwd"))workingdirs();
	assert(live == 0);
	printf("source paths: %d scenarios\n", checks);
	return 0;
}
