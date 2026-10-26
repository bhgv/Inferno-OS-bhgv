#include "limbo.h"
#include <limits.h>
#include <stdio.h>
#include <sys/wait.h>

char debug[256];
int storespace[Dend];
static int checks;

/* Keep layout oracles enabled when the host compiler defines NDEBUG. */
static void
layoutcheck(int condition)
{
	if(!condition)
		abort();
}


/* Reject invalid compiler layouts through the ordinary fatal diagnostic path. */
void
fatal(char *fmt, ...)
{
	layoutcheck(fmt != nil);
	_Exit(73);
}


/* Require the type identity supplied to the standalone layout fixture. */
void
usedty(Type *t)
{
	layoutcheck(t != nil);
}
IMPLEMENTATION

/* Use a separate bounded arithmetic expression for ordinary layout expectations. */
static long
roundoffset(long offset, int alignment)
{
	return ((offset+alignment-1)/alignment)*alignment;
}


/* Compare thousands of declaration lists with independent offsets and final sizes. */
static void
ordinary(void)
{
	Type types[8];
	Decl ids[8];
	Szal result;
	long offset, expected;
	int seed, i, maxalign;

	for(seed = 0; seed < 2000; seed++){
		memset(types, 0, sizeof types);
		memset(ids, 0, sizeof ids);
		expected = offset = seed%13;
		maxalign = 1;
		for(i = 0; i < 8; i++){
			types[i].ok = OKsized;
			types[i].align = 1<<((seed+i)%4);
			types[i].size = (seed*7+i*11)%65;
			ids[i].store = Darg;
			ids[i].nid = 1;
			ids[i].ty = &types[i];
			if(i+1 < 8)
				ids[i].next = &ids[i+1];
			if(types[i].align > maxalign)
				maxalign = types[i].align;
		}
		result = sizeids(ids, offset);
		for(i = 0; i < 8; i++){
			expected = roundoffset(expected, types[i].align);
			layoutcheck(ids[i].offset == expected);
			expected += types[i].size;
		}
		layoutcheck(result.size == expected && result.align == maxalign);
		layoutcheck(idoffsets(ids, offset, 8) == roundoffset(expected, 8));
		checks++;
	}
	/* Reused locals retain their preassigned slots and consume no new extent. */
	memset(ids, 0, sizeof ids);
	memset(types, 0, sizeof types);
	types[0].ok = OKsized;
	types[0].align = 8;
	types[0].size = 16;
	ids[0].store = Dlocal;
	ids[0].ty = types;
	ids[0].link = &ids[1];
	ids[1].offset = 24;
	layoutcheck(idoffsets(ids, 48, 8) == 48 && ids[0].offset == 24);
	checks++;
}


/* Cover table multiplication limits without allocating the tables themselves. */
static void
tables(void)
{
	Type type;
	Case table;
	int kinds[] = {Talt, Tcase, Tcasec, Tcasel, Tgoto};
	int strides[] = {2, 3, 3, 6, 1};
	int headers[] = {2, 2, 2, 3, 1};
	int i, n, count, wild;
	long overhead, stride;

	for(i = 0; i < nelem(kinds); i++)
	for(wild = 0; wild <= 1; wild++){
		stride = kinds[i] == Tcasel ? 2*IBY2LG+2*IBY2WD : strides[i]*IBY2WD;
		overhead = (headers[i]+(kinds[i] == Tgoto && wild))*IBY2WD;
		for(n = 0; n < 4; n++){
			count = n == 0 ? 0 : n == 1 ? 1 : n == 2 ? 12345 : (0x7fffffff-overhead)/stride;
			memset(&type, 0, sizeof type);
			memset(&table, 0, sizeof table);
			type.kind = kinds[i];
			type.ok = OKverify;
			type.cse = &table;
			table.nlab = count;
			table.iwild = wild ? (Inst*)1 : nil;
			sizetype(&type);
			if(type.size != (long)count*stride+overhead ||
			   type.align != (kinds[i] == Tcasel ? IBY2LG : IBY2WD))
				abort();
			checks++;
		}
	}
}


/* Exercise every checked failure in a child to keep parent compiler fixtures intact. */
static void
badlayout(int mode)
{
	Type type = {0}, scalar = {0}, parent = {0};
	Decl decl = {0}, dot = {0};
	Sym symbol = {0};
	Case table = {0};

	scalar.ok = OKsized;
	scalar.size = 16;
	scalar.align = 8;
	decl.store = Darg;
	decl.nid = 1;
	decl.ty = &scalar;
	switch(mode){
	case 0: (void)align(0x7fffffff, 8); break;
	case 1: (void)align(-1, 8); break;
	case 2: (void)align(0, 0); break;
	case 3: (void)align(0, -1); break;
	case 4: (void)sizeids(&decl, 0x7ffffff8); break;
	case 5: (void)idoffsets(&decl, 0x7ffffff8, 8); break;
	case 6: scalar.size = -1; (void)sizeids(&decl, 0); break;
	case 7: scalar.size = -1; (void)idoffsets(&decl, 0, 8); break;
	case 8: (void)sizeids(nil, -1); break;
	case 9:
		type.kind = Tiface;
		type.ok = OKverify;
		type.ids = &decl;
		decl.sym = &symbol;
		symbol.len = 0x7fffffff;
		decl.dot = &dot;
		dot.ty = &parent;
		parent.kind = Tmodule;
		sizetype(&type);
		break;
	default:
		type.kind = mode%2 ? Tcasel : Tcase;
		type.ok = OKverify;
		type.cse = &table;
		table.nlab = mode < 12 ? -1 : 0x7fffffff;
		sizetype(&type);
		break;
	}
	_Exit(74);
}


/* Require range rejection before any narrowing or signed arithmetic overflow. */
static void
failures(void)
{
	pid_t child;
	int mode, status;

	for(mode = 0; mode < 14; mode++){
		child = fork();
		layoutcheck(child >= 0);
		if(child == 0)
			badlayout(mode);
		layoutcheck(waitpid(child, &status, 0) == child);
		layoutcheck(WIFEXITED(status) && WEXITSTATUS(status) == 73);
		checks++;
	}
}


/* Test finite storage extents at ordinary, table and invalid arithmetic boundaries. */
int
main(int argc, char **argv)
{
	storespace[Darg] = storespace[Dlocal] = storespace[Dfield] = storespace[Dglobal] = 1;
	if(argc == 1 || strcmp(argv[1], "ordinary") == 0)
		ordinary();
	if(argc == 1 || strcmp(argv[1], "tables") == 0)
		tables();
	if(argc == 1 || strcmp(argv[1], "failure") == 0)
		failures();
	printf("compiler layout word=%d: %d offset/table/range cases passed\n", IBY2WD, checks);
	return 0;
}
