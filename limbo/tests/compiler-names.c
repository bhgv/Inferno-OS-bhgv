#include "limbo.h"
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>

int scope;
Decl *impmods;
static Decl *fndec;
typedef struct Idlist Idlist;
struct Idlist { int id; Idlist *next; };
static Idlist *frelist;
static int live, failalloc, checks;
static char diagnostic[20000], virtualtext;
static ulong virtualsize;

/* Disable unsupported process inspection; allocation accounting remains explicit. */
int __lsan_is_turned_off(void) { return 1; }

/* Count real allocations and inject native allocation failure below allocmem. */
static void*
namemalloc(ulong n)
{
	void *p;
	assert(n > 0 && n < 1000000);
	if(failalloc)
		return nil;
	p = malloc(n);
	assert(p != nil);
	live++;
	return p;
}

/* Ensure temporary diagnostic buffers and returned names have single owners. */
static void
namefree(void *p)
{
	if(p != nil)
		live--;
	free(p);
}

/* Simulate huge components to test bounds before any allocation or text copy. */
static ulong
namelen(char *s)
{
	return s == &virtualtext ? virtualsize : strlen(s);
}

/* Expected compiler errors terminate a child without sanitizer-unsafe longjmp. */
void
fatal(char *fmt, ...)
{
	assert(fmt != nil);
	_Exit(73);
}

/* Preserve the compiler's ordinary decimal scope-name formatting. */
int
sprint(char *s, char *fmt, ...)
{
	int n;
	va_list ap;
	va_start(ap, fmt);
	n = vsprintf(s, fmt, ap);
	va_end(ap);
	return n;
}

/* Capture an independently checked diagnostic rather than parsing compiler output. */
void
warn(Line line, char *fmt, ...)
{
	va_list ap;
	(void)line;
	va_start(ap, fmt);
	vsnprintf(diagnostic, sizeof diagnostic, fmt, ap);
	va_end(ap);
}

#define malloc namemalloc
#define free namefree
#define strlen namelen
IMPLEMENTATION
#undef malloc
#undef free
#undef strlen

/* Test all exception-name prefixes and exact module-union delimiters. */
static void
names(void)
{
	Sym symbols[4];
	Decl declarations[4];
	char expected[20000], *name;
	char module[4000], fn[4000], exception[4000];
	int length, dot, imported, inside;

	memset(declarations, 0, sizeof declarations);
	memset(symbols, 0, sizeof symbols);
	for(dot = 0; dot < 4; dot++)declarations[dot].sym = &symbols[dot];
	for(length = 0; length <= 3500; length += length < 6 ? 1 : 199){
		memset(module, 'm', length); module[length] = 0;
		memset(fn, 'f', length); fn[length] = 0;
		memset(exception, 'e', length); exception[length] = 0;
		symbols[0].name = exception; symbols[1].name = module;
		symbols[2].name = fn; symbols[3].name = "import";
		for(dot = 0; dot <= 1; dot++)
			for(imported = 0; imported <= 1; imported++)
				for(inside = 0; inside <= 1; inside++){
					declarations[0].dot = dot ? &declarations[1] : nil;
					impmods = imported ? &declarations[3] : nil;
					fndec = inside ? &declarations[2] : nil;
					scope = ScopeGlobal+5;
					expected[0] = 0;
					if(dot || imported){ strcat(expected, dot ? module : "import"); strcat(expected, "."); }
					if(inside){ strcat(expected, fn); strcat(expected, "."); }
					strcat(expected, "5."); strcat(expected, exception);
					name = exname(&declarations[0]);
					assert(strcmp(name, expected) == 0 && live == 1);
					namefree(name); checks++;
				}
		declarations[1].next = &declarations[2];
		declarations[2].next = &declarations[3];
		snprintf(expected, sizeof expected, "%s+%s+import", module, fn);
		name = uname(&declarations[1]);
		assert(strcmp(name, expected) == 0 && live == 1);
		namefree(name); checks++;
	}
	name = uname(nil);
	assert(strcmp(name, "") == 0 && live == 1);
	namefree(name); checks++;
}

/* Verify suffix matching is exact, including long paths and empty suffixes. */
static void
filenames(void)
{
	static char *files[] = {"", "x", "a.b", ".b", "a.b.b", "тест.b", "/dir.b/file"};
	static char *old[] = {"", ".b", ".dis", "longer-than-file"};
	static char *ext[] = {"", ".dis", ".sbl", ".b"};
	char expected[20000], *name;
	ulong i, j, k, n, suffix;
	for(i = 0; i < sizeof files/sizeof files[0]; i++)
		for(j = 0; j < sizeof old/sizeof old[0]; j++)
			for(k = 0; k < sizeof ext/sizeof ext[0]; k++){
				n = strlen(files[i]); suffix = strlen(old[j]);
				if(n >= suffix && strcmp(files[i]+n-suffix, old[j]) == 0)n -= suffix;
				memcpy(expected, files[i], n); strcpy(expected+n, ext[k]);
				name = mkfileext(files[i], old[j], ext[k]);
				assert(strcmp(name, expected) == 0 && live == 1);
				namefree(name); checks++;
			}
}

/* Exercise aliases, non-matching declarations and recycled optimizer list cells. */
static void
optimizer(void)
{
	Decl d[4]; Sym s[4]; Inst instruction;
	Idlist *list = nil, *saved;
	memset(d, 0, sizeof d); memset(s, 0, sizeof s); memset(&instruction, 0, sizeof instruction);
	s[0].name = "value"; s[1].name = "alias"; s[2].name = "ignored"; s[3].name = "last";
	for(int i = 0; i < 4; i++)d[i].sym = &s[i];
	d[1].next = &d[2]; d[2].next = &d[3];
	d[1].link = d[3].link = &d[0];
	warning(&instruction, "unused", &d[0], &d[1]);
	assert(strcmp(diagnostic, "value/alias/last: unused") == 0 && live == 0);
	failalloc = 1;
	d[2].next = nil;
	warning(&instruction, "unused", &d[0], &d[2]);
	assert(strcmp(diagnostic, "value: unused") == 0 && live == 0);
	failalloc = 0;
	addlist(&list, 10); assert(list->id == 10 && list->next == nil);
	saved = list; frelist = list; list = nil;
	failalloc = 1;
	addlist(&list, 20);
	assert(list == saved && list->id == 20 && frelist == nil);
	failalloc = 0;
	namefree(list);
	checks += 3;
}

/* Require controlled failure for each allocator path and virtual length overflow. */
static void
failure(int which)
{
	pid_t pid;
	int status;
	pid = fork(); assert(pid >= 0);
	if(pid == 0){
		Decl d[2]; Sym s[2]; Inst i; Idlist *list = nil;
		memset(d, 0, sizeof d); memset(s, 0, sizeof s); memset(&i, 0, sizeof i);
		s[0].name = "name"; s[1].name = "alias";
		d[0].sym = &s[0]; d[1].sym = &s[1];
		d[0].next = &d[1]; d[1].link = &d[0];
		fndec = impmods = nil; scope = ScopeGlobal;
		failalloc = which < 5;
		virtualsize = which == 10 ? 0x7ffffffeUL : 0x7fffffffUL;
		if(which == 0)exname(&d[0]);
		if(which == 1)uname(&d[0]);
		if(which == 2)mkfileext("file.b", ".b", ".dis");
		if(which == 3)warning(&i, "unused", &d[0], &d[1]);
		if(which == 4){ frelist = nil; addlist(&list, 1); }
		if(which == 5 || which == 10){ s[0].name = &virtualtext; uname(&d[0]); }
		if(which == 6){ s[0].name = &virtualtext; exname(&d[0]); }
		if(which == 7)mkfileext("file.b", ".b", &virtualtext);
		if(which == 8){ s[1].name = &virtualtext; warning(&i, "unused", &d[0], &d[1]); }
		if(which == 9)mkfileext(&virtualtext, ".b", ".dis");
		_Exit(0);
	}
	assert(waitpid(pid, &status, 0) == pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
	checks++;
}

/* Run value oracles and child-process failure probes against both driver copies. */
int
main(void)
{
	names(); filenames(); optimizer();
	assert(live == 0);
	for(int i = 0; i <= 10; i++)failure(i);
	printf("compiler names: %d checks passed\n", checks);
	return 0;
}
