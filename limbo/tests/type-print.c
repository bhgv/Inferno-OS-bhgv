#include "limbo.h"
#include <stdio.h>
#include <sys/resource.h>

#define check(c) predicate(!!(c), __LINE__) // Keep all checks active under NDEBUG.

static int reference; // Select the shallow oracle for nested formatter callbacks.
static ulong live; // Private diagnostic continuation allocations.
static Type pool[8192]; // Immutable shallow random Type trees.
static Decl fields[16384]; // Tuple fields and function arguments.
static Typelist arguments[16384]; // Instantiation argument chains.
static int nargs; // Current instantiated argument allocation cursor.
static int used, nfields; // Current shallow tree allocation cursors.
static uint rng = 71823; // Reproducible random generator state.
static Sym name = { .name = "item" }; // Shared short nominal spelling.
static Decl named = { .sym = &name }; // Named Type declaration.
static ulong scenarios; // Completed comparisons and deep traversals.


/* Fail explicitly on spelling, ownership or output-span violations. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "type print check at %d\n", line);
		abort();
	}
}


/* Retain address instrumentation without process-wide leak scanning. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Valid fixture types must never require a compiler error. */
void
fatal(char *message, ...)
{
	fprintf(stderr, "fatal: %s\n", message);
	abort();
}


/* Reject unexpected compiler errors rather than silently dropping diagnostics. */
void
yyerror(char *message, ...)
{
	fatal(message);
}


/* Track private continuation ownership independently of compiler-owned types. */
void*
allocmem(ulong size)
{
	void *p;

	p = malloc(size);
	check(p != nil);
	live++;
	return p;
}


/* Balance every continuation allocation, including truncated traversals. */
static void
workfree(void *p)
{
	check(p != nil && live > 0);
	live--;
	free(p);
}


/* The fixture's nominal module is an explicitly named import. */
int
isimpmod(Sym *s)
{
	USED(s);
	return 0;
}


/* Supply the formatter's error provider without unrelated runtime linkage. */
void
rerrstr(char *p, uint n)
{
	if(n)
		snprintf(p, n, "fixture");
}

#define free workfree // Observe production continuation ownership.
IMPLEMENTATION
#undef free

REFERENCE


/* Use the actual formatter callback path in the shallow recursive oracle. */
static int
typeformat(Fmt *f)
{
	char buf[1024];
	Type *t;

	t = va_arg(f->args, Type*);
	buf[0] = 0;
	if(reference)
		reference_tprint(buf, buf+sizeof(buf), t);
	else
		tprint(buf, buf+sizeof(buf), t);
	return fmtstrcpy(f, buf);
}


/* Generate mixed shallow Types with repeated tuple and function continuations. */
static Type*
make(int depth)
{
	Type *t;
	Decl *a, *b;
	int kind;

	check(used < nelem(pool));
	t = &pool[used++];
	rng = rng*1664525+1013904223;
	kind = depth ? (int)(rng%9) : 0;
	t->decl = &named;
	switch(kind){
	case 0: t->kind = Tint; break;
	case 1: t->kind = Tref; t->tof = make(depth-1); break;
	case 2: t->kind = Tarray; t->tof = make(depth-1); break;
	case 3: t->kind = Tarrow; t->tof = make(depth-1); break;
	case 4: t->kind = Tdot; t->tof = make(depth-1); break;
	case 5:
		t->kind = Tinst;
		t->tof = make(depth-1);
		check(nargs+2 <= nelem(arguments));
		t->u.tlist = &arguments[nargs];
		nargs += 2;
		t->u.tlist->nxt = t->u.tlist+1;
		t->u.tlist->t = make(depth-1);
		t->u.tlist->nxt->t = make(depth-1);
		break;
	case 6: t->kind = Tlist; t->tof = make(depth-1); break;
	default:
		check(nfields+2 <= nelem(fields));
		a = &fields[nfields++];
		b = &fields[nfields++];
		a->next = b;
		a->sym = &name;
		b->sym = &name;
		a->implicit = rng&1;
		a->ty = make(depth-1);
		b->ty = make(depth-1);
		t->ids = a;
		t->kind = kind == 7 ? Ttuple : Tfn;
		if(kind == 8){
			t->tof = make(depth-1);
			t->varargs = rng&1;
			t->polys = &named;
		}
		break;
	}
	return t;
}


/* Compare exact shallow text and protect both ends of every output capacity. */
static void
compare(Type *t, int capacity)
{
	char actual[2050], expected[2050], *cursor;

	memset(actual, 0x5a, sizeof(actual));
	memset(expected, 0x5a, sizeof(expected));
	if(capacity)
		actual[1] = expected[1] = 0;
	reference = 1;
	reference_tprint(expected+1, expected+1+capacity, t);
	reference = 0;
	cursor = tprint(actual+1, actual+1+capacity, t);
	check(cursor >= actual+1 && cursor <= actual+1+capacity);
	check(actual[0] == 0x5a && actual[capacity+1] == 0x5a);
	if(capacity){
		if(strcmp(actual+1, expected+1) != 0)
			fprintf(stderr, "capacity %d actual <%s> expected <%s> kind %d\n", capacity, actual+1, expected+1, t->kind);
		check(strcmp(actual+1, expected+1) == 0);
	}
	check(live == 0);
	scenarios++;
}


/* Cover nominal declarations, mapped parameters, invalid kinds and empty spans. */
static void
special(void)
{
	Type t, leaf, mapped;
	Decl declaration, module, parameter;
	Sym modname;
	Tpair pair;
	char out[32];
	int kind, capacity;

	memset(&t, 0, sizeof(t));
	memset(&leaf, 0, sizeof(leaf));
	memset(&mapped, 0, sizeof(mapped));
	memset(&declaration, 0, sizeof(declaration));
	memset(&module, 0, sizeof(module));
	memset(&parameter, 0, sizeof(parameter));
	memset(&modname, 0, sizeof(modname));
	memset(&pair, 0, sizeof(pair));
	modname.name = "module";
	module.sym = &modname;
	declaration.sym = &name;
	declaration.dot = &module;
	leaf.kind = Tint;
	mapped.kind = Tstring;
	parameter.sym = &name;
	parameter.ty = &leaf;
	pair.t1 = &leaf;
	pair.t2 = &mapped;
	t.decl = &declaration;
	for(kind = 0; kind < Tend; kind++){
		if(kind == Tfix)
			continue;
		t.kind = kind;
		t.tof = &leaf;
		for(capacity = 0; capacity <= 64; capacity++)
			compare(&t, capacity);
	}
	t.kind = Tadt;
	t.polys = &parameter;
	for(kind = 0; kind < 2; kind++){
		t.u.tmap = kind ? &pair : nil;
		for(capacity = 0; capacity <= 64; capacity++)
			compare(&t, capacity);
	}
	t.kind = -1;
	tprint(out, out+sizeof(out), &t);
	check(strcmp(out, "kind 255") == 0 && live == 0);
	scenarios++;
}


/* Verify full large output and deeply nested input with a constrained C stack. */
static void
deep(int kind, int capacity)
{
	Type *types;
	Decl *decls;
	char *out, *cursor;
	int i, depth;

	depth = 100000;
	types = calloc(depth+1, sizeof(*types));
	decls = calloc(depth, sizeof(*decls));
	out = malloc(capacity+2);
	check(types != nil && decls != nil && out != nil);
	for(i = 0; i < depth; i++){
		types[i].kind = kind;
		types[i].tof = &types[i+1];
		types[i].decl = &named;
		if(kind == Ttuple || kind == Tfn){
			types[i].ids = &decls[i];
			decls[i].ty = &types[i+1];
			decls[i].sym = &name;
			types[i].tof = nil;
		}
	}
	types[depth].kind = Tint;
	memset(out, 0x5a, capacity+2);
	out[1] = 0;
	cursor = tprint(out+1, out+1+capacity, types);
	check(cursor >= out+1 && cursor <= out+1+capacity);
	check(out[0] == 0x5a && out[capacity+1] == 0x5a);
	check(strlen(out+1) < capacity && live == 0);
	if(kind == Tinst && capacity > 2*depth+3)
		check(strlen(out+1) == 2*depth+3 && memcmp(out+1, "int[][]", 7) == 0);
	if(kind == Tarrow && capacity > 6*depth+3)
		check(strlen(out+1) == 6*depth+3 && memcmp(out+1, "int->item", 9) == 0);
	for(i = 0; i < depth; i++){
		check(types[i].kind == kind && types[i].decl == &named);
		if(kind == Ttuple || kind == Tfn)
			check(types[i].ids == &decls[i] && decls[i].ty == &types[i+1]);
		else
			check(types[i].tof == &types[i+1]);
	}
	if(kind == Tref && capacity > 4*depth+3)
		check(strlen(out+1) == 4*depth+3 && strcmp(out+1+4*depth, "int") == 0);
	if(kind == Ttuple && capacity > 2*depth+3)
		check(strlen(out+1) == 2*depth+3 && memcmp(out+1+depth, "int", 3) == 0);
	if(kind == Tfn && capacity > 10*depth+3)
		check(strlen(out+1) == 10*depth+3 && memcmp(out+1+9*depth, "int", 3) == 0);
	free(decls);
	free(types);
	free(out);
	scenarios++;
}


/* Run differential and independent deep checks under a small native stack. */
int
main(int argc, char **argv)
{
	struct rlimit limit;
	Type *t;
	int i, n;

	check(argc == 2);
	fmtinstall('T', typeformat);
	if(strcmp(argv[1], "shallow") == 0){
		special();
		for(i = 0; i < 1000; i++){
			memset(pool, 0, sizeof(pool));
			memset(fields, 0, sizeof(fields));
			memset(arguments, 0, sizeof(arguments));
			used = nfields = nargs = 0;
			t = make(5);
			for(n = 0; n <= 64; n++)
				compare(t, n);
			compare(t, 1024);
		}
	}else{
		limit.rlim_cur = limit.rlim_max = 512*1024;
		check(setrlimit(RLIMIT_STACK, &limit) == 0);
		i = strcmp(argv[1], "arrow") == 0 ? Tarrow : Tinst;
		deep(i, 1024);
		deep(i, 700004);
		if(strcmp(argv[1], "deep") == 0){
			deep(Tref, 1024);
			deep(Tref, 700004);
			deep(Tarray, 1024);
			deep(Tdot, 1024);
			deep(Ttuple, 1024);
			deep(Ttuple, 700004);
			deep(Tfn, 1024);
			deep(Tfn, 1400004);
		}
	}
	printf("type print %s: %lu scenarios\n", argv[1], scenarios);
	return 0;
}
