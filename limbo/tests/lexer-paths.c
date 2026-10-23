#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long Long;
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef struct { char *name; int len; } Sym;
typedef struct { int line, pos; } Line;
typedef struct File File;
struct File { char *name, *act; int abs, off, in, actoff, sbl; };
typedef struct { File *file; int line; } Fline;
typedef struct { int unused; } Biobuf;
enum { MaxPath = 4096, MaxInclude = 16, MaxIncPath = 8, StrSize = 256,
       Linestart = 0, OREAD = 0, Beof = -1, Mdigit = 1 };
#define nil NULL
static File *files[128];
static int nfiles, lastfile, lineno, linepos, bstack;
static Line linestack[MaxInclude];
static Biobuf inputbuf, *bins[MaxInclude], *bin;
static char srcdir[MaxPath], *incpath[MaxIncPath], opened[MaxPath];
static int diagnostics, opens, checks, failfirst;
static const char *input;
static int position;

/* Source bytes arrive through the same callback convention as production Getc. */
static int
Getc(void)
{
	return input[position] ? (uchar)input[position++] : Beof;
}

/* Only decimal classification is needed for line-number directives. */
static int
cmap(int c)
{
	return c >= '0' && c <= '9' ? Mdigit : 0;
}

/* Count diagnostics without replacing directive or include decisions. */
static void
yyerror(char *fmt, ...)
{
	assert(fmt != nil);
	diagnostics++;
}

/* Observe overlong-path errors without adding a native filesystem dependency. */
static void
werrstr(char *fmt, ...)
{
	assert(fmt != nil);
}

/* Exceeding include nesting is outside the fixtures and must not be reached. */
static void
fatal(char *fmt, ...)
{
	(void)fmt;
	abort();
}

/* Preserve the production formatting contract for bounded include paths. */
static char*
seprint(char *p, char *end, char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(p, end-p, fmt, ap);
	va_end(ap);
	return p + (n >= end-p ? end-p-1 : n);
}

/* Every attempted open is observable, including an incorrectly truncated name. */
static Biobuf*
Bopen(char *name, int mode)
{
	assert(mode == OREAD && strlen(name) < sizeof(opened));
	opens++;
	strcpy(opened, name);
	return failfirst && opens == 1 ? nil : &inputbuf;
}

/* Store real directive metadata so subsequent lookups exercise production fline. */
static File*
mkfile(char *name, int abs, int off, int in, char *act, int actoff, int sbl)
{
	File *f = calloc(1, sizeof(*f));

	assert(f);
	f->name = name; f->abs = abs; f->off = off; f->in = in;
	f->act = act; f->actoff = actoff; f->sbl = sbl;
	return f;
}

/* Keep ownership of every mapping until the next independent fixture. */
static int
addfile(File *f)
{
	assert(nfiles < 128);
	files[nfiles] = f;
	return nfiles++;
}

/* Supply source position only for the unreachable include-depth diagnostic. */
static Line
curline(void)
{
	Line l = {lineno, linepos};
	return l;
}

/* LeakSanitizer cannot inspect host threads in this sandbox. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Reset mappings and include state while balancing all fixture allocations. */
static void
setup(void)
{
	int i;

	for(i = 0; i < nfiles; i++){
		free(files[i]->name);
		free(files[i]);
	}
	nfiles = lastfile = bstack = diagnostics = opens = failfirst = 0;
	lineno = 1;
	linepos = 0;
	bin = bins[0] = &inputbuf;
	memset(srcdir, 0, sizeof(srcdir));
	memset(incpath, 0, sizeof(incpath));
	addfile(mkfile(strdup("original"), 1, 0, -1, nil, 0, -1));
}

/* Exact paths work; embedded NULs and truncated paths never reach Bopen. */
static void
includes(void)
{
	char path[MaxPath+1], nulname[] = "first\0second";
	Sym name;
	int n;

	for(n = 0; n < 5; n++){
		setup();
		name.name = nulname;
		name.len = n == 0 ? 0 : sizeof(nulname)-1;
		if(n >= 2){
			memset(path, 'a', MaxPath);
			path[MaxPath] = 0;
			name.name = path;
			name.len = n == 2 ? MaxPath : MaxPath-1;
			path[name.len] = 0;
			if(n == 4)strcpy(srcdir, "prefix/");
		}
		includef(&name);
		if(n == 3)
			assert(opens == 1 && bstack == 1 && strlen(opened) == MaxPath-1);
		else
			assert(opens == 0 && bstack == 0 && diagnostics == 1);
		checks++;
	}
	setup();
	strcpy(srcdir, "source/");
	name.name = "module.m"; name.len = 8;
	includef(&name);
	assert(opens == 1 && !strcmp(opened, "source/module.m") && bstack == 1);
	checks++;
	setup();
	memset(path, 'p', MaxPath-1); path[MaxPath-1] = 0;
	incpath[0] = path; incpath[1] = "search"; failfirst = 1;
	includef(&name);
	assert(opens == 2 && !strcmp(opened, "search/module.m") && bstack == 1);
	checks++;
	setup();
	name.name = "/absolute.m"; name.len = 11;
	strcpy(srcdir, "ignored/"); incpath[0] = "ignored";
	includef(&name);
	assert(opens == 1 && !strcmp(opened, "/absolute.m"));
	checks++;
}

/* Full decimal limits and long comments cannot overflow or forge a directive. */
static void
directives(void)
{
	const char *cases[] = {"line 2147483648 \"bad\"\n", "line 999999999999999999999 \"bad\"\n",
		"line 2147483647 \"max\"\n", "line 0 \"zero\"\n", "line 42 \"normal\"\n"};
	int expected[] = {0, 0, INT_MAX, 0, 42};
	char large[1024];
	int i;
	Fline fl;

	for(i = 0; i < 5; i++){
		setup(); input = cases[i]; position = 0;
		lexcom();
		assert(position == strlen(input) && lineno == 2);
		if(i < 2)
			assert(diagnostics == 1 && nfiles == 1);
		else{
			assert(diagnostics == 0 && nfiles == 2);
			fl = fline(2);
			assert(fl.line == expected[i]);
			fl = fline(3);
			assert(fl.line == (i == 2 ? INT_MAX : expected[i]+1));
		}
		checks++;
	}
	setup();
	memset(large, 'a', sizeof(large));
	memcpy(large, "line 1 \"", 8);
	large[StrSize-2] = '"';
	large[sizeof(large)-2] = '\n'; large[sizeof(large)-1] = 0;
	input = large; position = 0; lexcom();
	assert(nfiles == 1 && diagnostics == 1 && position == strlen(input));
	checks++;
	setup();
	large[0] = 'x'; input = large; position = 0; lexcom();
	assert(nfiles == 1 && diagnostics == 0 && position == strlen(input));
	checks++;
}

/* Keep include lookup and directive arithmetic reproducible without a full compiler. */
int
main(void)
{
	const char *only = getenv("LEX_PATH_CASES");

	if(only == nil || !strcmp(only, "includes"))includes();
	if(only == nil || !strcmp(only, "directives"))directives();
	setup();
	free(files[0]->name); free(files[0]); nfiles = 0;
	printf("lexer paths/lines: %d scenarios\n", checks);
	return 0;
}
