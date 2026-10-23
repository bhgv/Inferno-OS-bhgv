#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct Biobuf Biobuf;
typedef struct Decl Decl;
struct Biobuf { int slot, active; };
struct Decl { int unused; };
#define nil NULL
#define OREAD 0
#define OWRITE 1
#define Beof (-1)
static Biobuf streams[3], *bins[1], *bout, *bsym;
static Decl *fns;
static void *descriptors;
static char *infile, *outfile, *symfile;
static int errors, toterrors, emitstub, emittab, emitcode, dosym, nfns;
static int openfault, closefault, parsefault, parsed, opens[3], closes[3];
static int removes[2], diagnostics, checks;

/* Inject input, bytecode and symbol-file open failures independently. */
static Biobuf*
Bopen(char *name, int mode)
{
	int slot;

	slot = mode == OREAD ? 0 : strcmp(name, "output") == 0 ? 1 : 2;
	opens[slot]++;
	if(openfault & (1<<slot))
		return nil;
	assert(!streams[slot].active);
	streams[slot].active = 1;
	streams[slot].slot = slot;
	return streams+slot;
}

/* A close releases ownership even when its buffered flush fails. */
static int
Bterm(Biobuf *b)
{
	assert(b >= streams && b < streams+3 && b->active);
	b->active = 0;
	closes[b->slot]++;
	return closefault & (1<<b->slot) ? Beof : 0;
}

/* Record artifact deletion after all output handles have been released. */
static int
removeoutput(char *name)
{
	assert(!streams[1].active && !streams[2].active);
	removes[strcmp(name, "output") != 0]++;
	return 0;
}
#define remove removeoutput

/* Keep expected diagnostics observable without linking the formatting runtime. */
static void
fprint(int fd, char *fmt, ...)
{
	assert(fd == 2 && fmt != nil);
	diagnostics++;
}

/* Compiler phases are boundaries here; production yyparse normally closes input. */
static void
lexstart(char *in)
{
	assert(strcmp(in, "input") == 0);
}
/* Scope cleanup has no stream ownership in this fixture. */
static void
popscopes(void)
{
}
/* Type initialization is outside the I/O driver under test. */
static void
typestart(void)
{
}
/* Declaration initialization is outside the I/O driver under test. */
static void
declstart(void)
{
}
/* Parsing closes its input and may leave compile diagnostics. */
static void
yyparse(void)
{
	parsed++;
	errors += parsefault;
	assert(Bterm(bins[0]) == 0);
}
/* No entry declaration is required to exercise final output cleanup. */
static Decl*
typecheck(int emit)
{
	(void)emit;
	return nil;
}
/* Emission details are covered separately; this driver owns the stream lifetime. */
static void
modcom(Decl *entry)
{
	assert(entry == nil);
}

/* LeakSanitizer cannot enumerate threads in this environment. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Exhaust independent stream failures and repeatedly reuse the global driver. */
int
main(void)
{
	int emit, symbols, opening, closing, parse, i, previous, expected, haveout, havesym;
	char drive[] = "C:\\ABC\\\xc3\x89.b", relative[] = "ABC\\\xc3\x89.b";
	char *path;

	path = win2inf(drive);
	assert(path == drive+1 && !strcmp(path, "//abc/\xc3\x89.b"));
	assert(win2inf(relative) == relative && !strcmp(relative, "ABC/\xc3\x89.b"));

	for(emit = 0; emit < 2; emit++)
	for(symbols = 0; symbols < 2; symbols++)
	for(opening = 0; opening < 8; opening++)
	for(closing = 0; closing < 4; closing++)
	for(parse = 0; parse < 2; parse++){
		for(i = 0; i < 3; i++)
			assert(!streams[i].active);
		memset(opens, 0, sizeof(opens));
		memset(closes, 0, sizeof(closes));
		memset(removes, 0, sizeof(removes));
		/* Poison stale pointers as if the preceding translation had closed them. */
		bout = streams+1;
		bsym = streams+2;
		emitstub = emit;
		dosym = symbols;
		openfault = opening;
		closefault = closing<<1;
		parsefault = parse;
		parsed = diagnostics = 0;
		previous = toterrors;
		translate("input", "output", "symbols");
		if(opening&1 || (!emit && (opening&2))){
			assert(toterrors == previous+1 && !parsed);
		}else{
			haveout = !emit;
			havesym = !emit && symbols && !(opening&4);
			expected = parse+(!emit && symbols && (opening&4) != 0);
			expected += haveout && (closing&1);
			expected += havesym && (closing&2);
			assert(toterrors == previous+expected && parsed == 1);
			assert(removes[0] == (haveout && expected));
			assert(removes[1] == (havesym && expected));
		}
		assert(bout == nil && bsym == nil);
		for(i = 0; i < 3; i++)
			assert(!streams[i].active && closes[i] <= 1);
		checks++;
	}
	printf("translation I/O: %d scenarios\n", checks);
	return 0;
}
