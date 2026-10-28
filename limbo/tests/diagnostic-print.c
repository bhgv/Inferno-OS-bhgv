#include "limbo.h"
#include <stdio.h>
#include <limits.h>
#include <sys/resource.h>

#define check(test) predicate(!!(test), __LINE__) // Keep diagnostic checks active with NDEBUG.

char *opname[Oend]; // Stable opcode spellings used by both printers.
char unescmap[256]; // String escape spelling used by the real diagnostic helper.
Type *tint, *tbyte, *tbig; // Builtin identities used by the debug printer.
static Type types[Tend]; // Borrowed test Types with a stable converter spelling.
static Decl declaration; // Named leaf and tagged-type declaration.
static Sym symbol; // Shared short identifier outside the quote-boundary test.
static Node pool[32768]; // Shallow trees for the recursive differential oracle.
static int used; // Number of shallow nodes allocated in the current tree.
static ulong checks, allocations, outstanding; // Verified scenarios and live private work blocks.
static uint randomstate = 0x28419; // Reproducible shallow tree generator state.


/* Report the first failed oracle, cursor, canary or ownership condition. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "diagnostic print failure at %d\n", line);
		abort();
	}
}


/* Keep address instrumentation without unavailable process-wide LSan inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* No valid test tree should reach a compiler fatal diagnostic. */
void
fatal(char *message, ...)
{
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Track each private deferred-action allocation. */
void*
allocmem(ulong bytes)
{
	void *p;

	check(bytes != 0 && bytes < 128*1024*1024);
	p = malloc(bytes);
	check(p != nil);
	allocations++;
	outstanding++;
	return p;
}


/* Release precisely the deferred-action storage owned by production printing. */
static void
printfree(void *p)
{
	check(p != nil && outstanding != 0);
	outstanding--;
	free(p);
}


/* Supply the real error-format engine with a host-only error message. */
void
rerrstr(char *p, uint n)
{
	if(n)
		snprintf(p, n, "fixture");
}


/* Type recursion has its own tests; isolate AST order with a stable Type spelling. */
static int
typeformat(Fmt *f)
{
	Type *ty;

	ty = va_arg(f->args, Type*);
	return fmtstrcpy(f, ty == nil ? "niltype" : "type");
}


/* Preserve debug opcode placement without linking unrelated compiler globals. */
static int
opformat(Fmt *f)
{
	int op;

	op = va_arg(f->args, int);
	check(op >= 0 && op < Oend);
	return fmtstrcpy(f, opname[op]);
}

#define free printfree // Observe only production diagnostic work ownership.
IMPLEMENTATION
#undef free

REFERENCE


/* Advance the deterministic generator without depending on libc random state. */
static uint
nextvalue(void)
{
	randomstate = randomstate*1664525U+1013904223U;
	return randomstate;
}


/* Create a valid shallow expression with every explicit printing category. */
static Node*
maketree(int depth)
{
	int ops[] = {Oadr, Oused, Ocast, Otuple, Ochan, Oarray, Oelem, Olabel,
	             Orange, Ospawn, Oraise, Ocall, Oinc, Odec, Oindex, Oindx, Oinds,
	             Oslice, Oload, Oref, Olen, Ohd, Otl, Otagof, Oadd, Osub, Onot};
	Node *n, *list;
	uint choice;

	check(used < nelem(pool));
	n = &pool[used++];
	memset(n, 0, sizeof(*n));
	n->ty = &types[Tint];
	n->flags = nextvalue()&PARENS;
	choice = nextvalue();
	if(depth == 0 || choice%5 == 0){
		n->op = Oname;
		if(choice%7)
			n->decl = &declaration;
		if(choice%3 == 0){
			n->op = Oconst;
			n->val = (int)nextvalue();
			n->rval = (int)nextvalue()/17.0;
			n->ty = &types[(int[]){Tint, Tbyte, Tbig, Treal, Tfix, Tstring}[choice%6]];
			if(n->ty->kind == Tstring)
				n->decl = &declaration;
		}else if(choice%11 == 0){
			n->op = (int[]){Obreak, Ocont, Oexit, Owild, Onothing}[choice%5];
		}
		return n;
	}
	n->op = ops[choice%nelem(ops)];
	if(n->op == Otuple && (choice&8))
		n->ty = &types[Tadt];
	n->left = maketree(depth-1);
	n->right = maketree(depth-1);
	switch(n->op){
	case Otuple: case Oelem: case Olabel: case Ocall: case Oarray:
		check(used < nelem(pool));
		list = &pool[used++];
		memset(list, 0, sizeof(*list));
		list->op = Oseq;
		list->left = n->right;
		if(n->op == Otuple || n->op == Oelem || n->op == Olabel)
			n->left = list;
		else
			n->right = list;
		break;
	case Oslice:
		check(used < nelem(pool));
		list = &pool[used++];
		memset(list, 0, sizeof(*list));
		list->left = n->left;
		list->right = n->right;
		n->right = list;
		break;
	default:
		if(choice&16)
			n->right = nil;
		break;
	}
	return n;
}


/* Compare exact bytes, live NUL cursors and canaries for bounded shallow output. */
static void
compare(Node *n, int capacity, int dump, int list)
{
	char actual[8194], expected[8194], *a, *e;

	memset(actual, 0xa5, sizeof(actual));
	memmove(expected, actual, sizeof(actual));
	actual[1] = expected[1] = 0;
	if(dump){
		a = nprint(actual+1, actual+1+capacity, n, 0);
		e = reference_nprint(expected+1, expected+1+capacity, n, 0);
	}else if(list){
		a = eprintlist(actual+1, actual+1+capacity, n, " or ");
		e = reference_eprintlist(expected+1, expected+1+capacity, n, " or ");
	}else{
		a = eprint(actual+1, actual+1+capacity, n);
		e = reference_eprint(expected+1, expected+1+capacity, n);
	}
	check(!strcmp(actual+1, expected+1));
	check(a >= actual+1 && a < actual+1+capacity && *a == 0);
	check(e >= expected+1 && e <= expected+1+capacity);
	check(actual[0] == (char)0xa5 && expected[0] == (char)0xa5);
	check(actual[capacity+1] == (char)0xa5 && expected[capacity+1] == (char)0xa5);
	check(outstanding == 0);
	checks++;
}


/* Exercise all printing categories, truncation boundaries and sequence stop rules. */
static void
shallow(void)
{
	int capacities[] = {1, 2, 3, 7, 31, 127, 4096};
	Node *n, cells[4], leaf[4], saved[32768];
	int i, j, kind;

	for(i = 0; i < 500; i++){
		used = 0;
		n = maketree(6);
		memmove(saved, pool, used*sizeof(*pool));
		for(j = 0; j < nelem(capacities); j++){
			compare(n, capacities[j], 0, 0);
			compare(n, capacities[j], 1, 0);
		}
		check(!memcmp(saved, pool, used*sizeof(*pool)));
	}
	for(kind = 0; kind < 4; kind++){
		memset(cells, 0, sizeof(cells));
		memset(leaf, 0, sizeof(leaf));
		for(i = 0; i < nelem(cells); i++){
			leaf[i].op = Oname;
			leaf[i].decl = &declaration;
			cells[i].op = Oseq;
			cells[i].left = &leaf[i];
			if(i+1 < nelem(cells))
				cells[i].right = &cells[i+1];
		}
		if(kind == 1)
			cells[1].op = Onothing;
		if(kind >= 2)
			leaf[kind-2].op = Ofnptr;
		for(j = 0; j < nelem(capacities); j++)
			compare(cells, capacities[j], 0, 1);
	}
}


/* Hit the historical quoted-conversion stack overflow with long identifiers. */
static void
quotes(void)
{
	char name[8200], output[9000];
	Node n;
	Sym sym;
	Decl decl;
	int length;

	memset(&n, 0, sizeof(n));
	memset(&sym, 0, sizeof(sym));
	memset(&decl, 0, sizeof(decl));
	n.op = Oname;
	n.flags = PARENS;
	n.decl = &decl;
	decl.sym = &sym;
	sym.name = name;
	memset(name, 'x', sizeof(name));
	for(length = 4088; length < sizeof(name); length++){
		name[length] = 0;
		check(seprint(output, output+sizeof(output), "%V", &n) != nil);
		check(output[0] == '\'' && strlen(output) <= 4095);
		check(output[strlen(output)-1] == '\'');
		name[length] = 'x';
		checks++;
	}
}


/* Traverse hundred-thousand-edge trees under a small native stack, including silent edges. */
static void
deep(int dump)
{
	Node *nodes, leaf;
	struct rlimit limit;
	char output[8192], before[16];
	int i, kind;

	limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	nodes = calloc(100000, sizeof(*nodes));
	check(nodes != nil);
	memset(&leaf, 0, sizeof(leaf));
	leaf.op = Oname;
	leaf.decl = &declaration;
	for(kind = 0; kind < 4; kind++){
		for(i = 0; i < 100000; i++){
			nodes[i].op = (int[]){Oused, Ocall, Oadd, Onothing}[kind];
			nodes[i].ty = tint;
			nodes[i].left = i+1 < 100000 ? &nodes[i+1] : &leaf;
			nodes[i].right = kind == 2 ? &leaf : nil;
		}
		output[0] = 0;
		if(dump)
			check(nprint(output, output+sizeof(output), nodes, 0) != nil);
		else{
			check(eprint(output, output+sizeof(output), nodes) != nil);
			if(kind == 0)
				check(!strcmp(output, "name"));
			if(kind == 1)
				check(!strncmp(output, "name()()", 8) && allocations > 0);
			if(kind == 3)
				check(output[0] == 0);
		}
		check(outstanding == 0 && strlen(output) < sizeof(output));
		checks++;
	}
	memset(before, 0xa5, sizeof(before));
	check(eprint(before, before, nodes) == before);
	check(before[0] == (char)0xa5);
	check(eprint(before, before+sizeof(before), nil) == before && before[0] == (char)0xa5);
	if(dump){
		check(nprint(output, output+sizeof(output), nodes, INT_MAX) == output+sizeof(output)-1);
		check(outstanding == 0);
		checks++;
	}
	free(nodes);
	checks++;
}


/* Install real formatter conversions and isolate parent-source regression probes. */
int
main(int argc, char **argv)
{
	Node value;
	int i;

	check(argc == 2);
	for(i = 0; i < Oend; i++)
		opname[i] = "+";
	for(i = 0; i < Tend; i++){
		types[i].kind = i;
		types[i].tof = &types[Tint];
		types[i].decl = &declaration;
	}
	memset(&value, 0, sizeof(value));
	value.rval = 1.25;
	types[Tfix].val = &value;
	tint = &types[Tint]; tbyte = &types[Tbyte]; tbig = &types[Tbig];
	symbol.name = "name"; symbol.len = 4;
	declaration.sym = &symbol;
	for(i = 0; i < 256; i++)
		unescmap[i] = i;
	check(fmtinstall('T', typeformat) == 0 && fmtinstall('O', opformat) == 0);
	check(fmtinstall('V', expconv) == 0 && fmtinstall('Q', nodeconv) == 0);
	if(!strcmp(argv[1], "shallow"))
		shallow();
	else if(!strcmp(argv[1], "quote"))
		quotes();
	else
		deep(!strcmp(argv[1], "dump"));
	printf("PASS diagnostic %s: %lu spelling/bounds/stack scenarios\n", argv[1], checks);
	return 0;
}
