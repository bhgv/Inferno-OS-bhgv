#include "limbo.h"
#include <stdio.h>
#include <setjmp.h>
#include <sys/resource.h>

#define check(c) predicate(!!(c), __LINE__) // Keep checks active under NDEBUG.

static Sym anonymous = { .name = "anonymous" }; // Identity of anonymous tuples.
Sym *anontupsym = &anonymous; // Production anonymous-tuple identity.
static Sym symbol = { .name = "field" }; // Field and nominal Type spelling.
static Sym firstsymbol = { .name = "first" }; // Unique first aggregate field.
static Sym secondsymbol = { .name = "second" }; // Unique second aggregate field.
static Sym modulesymbol = { .name = "Module" }; // Qualified module spelling.
static Decl parent = { .sym = &modulesymbol }; // Root of shallow qualified declarations.
static Decl named = { .sym = &symbol, .dot = &parent }; // Named aggregate declaration.
static Decl anon = { .sym = &anonymous }; // Anonymous aggregate declaration.
static Type types[8192]; // Shallow generated Type trees.
static Decl fields[16384]; // Shallow anonymous aggregate fields.
static int used, nfields, reference, wordbytes; // Current tree and formatter model.
static uint randomstate = 94817; // Reproducible random Type generator.
static ulong live, scenarios; // Private continuation ownership and scenario count.
static int expectfatal; // Layout rejection is expected only in explicit negative tests.
static char streamed[128]; // Captured emitted padding declaration.
static jmp_buf rejection; // Return from an expected compiler fatal diagnostic.


/* Reject spelling, bounds, ownership or validation regressions explicitly. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "stub print failure at %d\n", line);
		abort();
	}
}


/* Retain address instrumentation without process-wide leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Catch expected layout diagnostics and fail all unexpected errors. */
void
fatal(char *format, ...)
{
	if(expectfatal)
		longjmp(rejection, 1);
	fprintf(stderr, "unexpected fatal: %s\n", format);
	abort();
}


/* No supported fixture Type should reach a C-equivalent diagnostic. */
void
yyerror(char *format, ...)
{
	fatal(format);
}


/* Count private continuation allocations without owning any borrowed Types. */
void*
allocmem(ulong bytes)
{
	void *p;

	p = malloc(bytes);
	check(p != nil);
	live++;
	return p;
}


/* Balance every work allocation at normal completion, including truncation. */
static void
workfree(void *p)
{
	check(p != nil && live != 0);
	live--;
	free(p);
}


/* Provide the real formatter with an isolated host error provider. */
void
rerrstr(char *p, uint count)
{
	if(count)
		snprintf(p, count, "fixture");
}

/* Capture actual streamed padding with the same production formatting engine. */
int
print(char *format, ...)
{
	va_list args;
	char *end;

	va_start(args, format);
	end = vseprint(streamed, streamed+sizeof(streamed), format, args);
	va_end(args);
	return end-streamed;
}


#define free workfree // Observe only production continuation ownership.
IMPLEMENTATION
#undef free

REFERENCE


/* Exercise the genuine nested %R path in the frozen shallow oracle. */
static int
ctypeformat(Fmt *f)
{
	Type *t;
	char buf[1024];

	t = va_arg(f->args, Type*);
	buf[0] = 0;
	if(reference)
		reference_ctprint(buf, buf+sizeof(buf), t);
	else
		ctprint(buf, buf+sizeof(buf), t);
	return fmtstrcpy(f, buf);
}


/* Generate sized and aligned shallow aggregates, references and scalar Types. */
static Type*
make(int depth)
{
	Type *t;
	Decl *first, *second;
	int kind, offset;

	check(used < nelem(types));
	t = &types[used++];
	randomstate = randomstate*1664525+1013904223;
	kind = depth ? randomstate%8 : 0;
	t->size = 4;
	t->align = 4;
	t->decl = &named;
	switch(kind){
	case 0: t->kind = Tint; break;
	case 1:
		t->kind = Tref;
		t->tof = make(depth-1);
		t->size = t->align = wordbytes;
		break;
	case 2: t->kind = Tarray; t->size = t->align = wordbytes; break;
	case 3: t->kind = Tbig; t->size = t->align = 8; break;
	case 4: t->kind = Tadt; break;
	case 5: t->kind = Tbyte; t->size = t->align = 1; break;
	default:
		t->kind = kind == 6 ? Ttuple : Tadt;
		t->decl = &anon;
		check(nfields+2 <= nelem(fields));
		first = &fields[nfields++];
		second = &fields[nfields++];
		first->sym = &firstsymbol;
		second->sym = &secondsymbol;
		first->ty = make(depth-1);
		second->ty = make(depth-1);
		first->next = second;
		t->ids = first;
		t->align = first->ty->align > second->ty->align ? first->ty->align : second->ty->align;
		offset = first->ty->size;
		offset = (offset+second->ty->align-1)&~(second->ty->align-1);
		second->offset = offset;
		offset += second->ty->size;
		t->size = (offset+t->align-1)&~(t->align-1);
		break;
	}
	return t;
}


/* Compare exact shallow text and canaries over empty, short and full spans. */
static void
compare(Type *t, int capacity)
{
	char actual[2050], expected[2050], *cursor;

	memset(actual, 0x5a, sizeof(actual));
	memset(expected, 0x5a, sizeof(expected));
	if(capacity)
		actual[1] = expected[1] = 0;
	reference = 1;
	reference_ctprint(expected+1, expected+sizeof(expected)-1, t);
	if(capacity && strlen(expected+1) >= capacity)
		expected[capacity] = 0;
	reference = 0;
	cursor = ctprint(actual+1, actual+1+capacity, t);
	check(cursor >= actual+1 && cursor <= actual+1+capacity);
	check(actual[0] == 0x5a && actual[capacity+1] == 0x5a);
	if(capacity){
		check(memchr(actual+1, 0, capacity) != nil);
		if(strcmp(actual+1, expected+1) != 0)
			fprintf(stderr, "capacity %d actual <%s> expected <%s>\n", capacity, actual+1, expected+1);
		check(strcmp(actual+1, expected+1) == 0);
	}
	check(live == 0);
	scenarios++;
}


/* Cover all nonrecursive C spellings, including nil and pointer-like builtins. */
static void
literals(void)
{
	static int kinds[] = { Tarray, Tlist, Tint, Tbig, Tstring, Treal, Tbyte,
		Tnone, Tany, Tchan, Tmodule, Tfix, Tpoly };
	Type t;
	int i, capacity;

	memset(&t, 0, sizeof(t));
	for(i = 0; i < nelem(kinds)+1; i++){
		if(i < nelem(kinds))
			t.kind = kinds[i];
		for(capacity = 0; capacity <= 64; capacity++)
			compare(i < nelem(kinds) ? &t : nil, capacity);
		compare(i < nelem(kinds) ? &t : nil, 1024);
	}
}


/* Check a declaration chain independently, including a nameless final component. */
static void
dots(int shortonly)
{
	Decl *chain;
	char output[64], *cursor;
	int i, capacity;

	chain = calloc(100000, sizeof(*chain));
	check(chain != nil);
	for(i = 0; i < 100000; i++){
		chain[i].sym = &symbol;
		if(i)
			chain[i].dot = &chain[i-1];
	}
	for(capacity = 0; !shortonly && capacity <= 32; capacity++){
		memset(output, 0x5a, sizeof(output));
		cursor = dotprint(output+1, output+1+capacity, &chain[99999], '_');
		check(cursor >= output+1 && cursor <= output+1+capacity);
		check(output[0] == 0x5a && output[capacity+1] == 0x5a);
		if(capacity)
			check(memchr(output+1, 0, capacity) != nil);
		check(live == 0);
		scenarios++;
	}
	/* One real name followed by a nameless leaf exposed the missing terminator. */
	chain[0].sym = &symbol;
	chain[1].sym = nil;
	for(capacity = 1; capacity <= 16; capacity++){
		memset(output, 0x5a, sizeof(output));
		cursor = dotprint(output+1, output+1+capacity, &chain[1], '_');
		check(memchr(output+1, 0, capacity) != nil);
		check(cursor < output+1+capacity && *cursor == 0);
		check(output[0] == 0x5a && output[capacity+1] == 0x5a);
		if(capacity >= 7)
			check(strcmp(output+1, "field_") == 0);
		scenarios++;
	}
	free(chain);
}


/* Verify full spelling and immutable inputs through 100,000 nested C Types. */
static void
deep(int kind)
{
	Type *chain;
	Decl *decls;
	char *output, *cursor;
	int i, capacity, pass;
	ulong length;

	chain = calloc(100001, sizeof(*chain));
	decls = calloc(100000, sizeof(*decls));
	check(chain != nil && decls != nil);
	for(i = 0; i < 100000; i++){
		chain[i].kind = kind;
		chain[i].size = 4;
		chain[i].align = 4;
		chain[i].tof = &chain[i+1];
		chain[i].decl = &anon;
		if(kind == Ttuple){
			chain[i].ids = &decls[i];
			decls[i].ty = &chain[i+1];
			decls[i].sym = &symbol;
		}else if(kind == Tadtpick){
			chain[i].decl = &decls[i];
			decls[i].dot = &decls[i];
			decls[i].ty = &chain[i+1];
		}
	}
	chain[100000].kind = Tint;
	chain[100000].size = chain[100000].align = 4;
	for(pass = 0; pass < 2; pass++){
		capacity = pass ? 2000004 : 1024;
		output = malloc(capacity+2);
		check(output != nil);
		memset(output, 0x5a, capacity+2);
		output[1] = 0;
		cursor = ctprint(output+1, output+1+capacity, chain);
		check(cursor >= output+1 && cursor <= output+1+capacity);
		check(output[0] == 0x5a && output[capacity+1] == 0x5a);
		check(memchr(output+1, 0, capacity) != nil && live == 0);
		if(pass){
			length = kind == Tref ? 100004 : kind == Ttuple ? 1700004 : 4;
			check(strlen(output+1) == length);
			if(kind == Tref)
				check(memcmp(output+1, "WORD***", 7) == 0);
			if(kind == Tadtpick)
				check(strcmp(output+1, "WORD") == 0);
			if(kind == Ttuple)
				check(memcmp(output+1+8*100000, "WORD field; }", 13) == 0);
		}
		free(output);
		scenarios++;
	}
	for(i = 0; i < 100000; i++)
		check(chain[i].kind == kind && chain[i].tof == &chain[i+1]);
	free(chain);
	free(decls);
}


/* Check bounded padding and reject invalid layouts before publishing any padding. */
static void
alignment(void)
{
	static long offsets[] = { 0, 1, 2, 3, 7, 15, 31, 255, 65535, 0x7ffffff0, 0x7ffffffe, 0x7fffffff, -1, 0x80000000L };
	static int aligns[] = { 0, 1, 2, 3, 4, 8, 16, 32, -1, 0x7fffffff };
	char output[128], *cursor, expected[128];
	long value, next, pad;
	int i, j, valid;

	for(i = 0; i < nelem(offsets); i++)
		for(j = 0; j < nelem(aligns); j++){
			value = offsets[i];
			valid = value >= 0 && value <= 0x7fffffff && aligns[j] > 0 && (aligns[j]&(aligns[j]-1)) == 0;
			next = valid ? (value+aligns[j]-1)&~((long)aligns[j]-1) : 0;
			valid = valid && next <= 0x7fffffff;
			memset(output, 0x5a, sizeof(output));
			cursor = output+1;
			expectfatal = !valid;
			if(setjmp(rejection) == 0){
				check(stubalign(value, aligns[j], &cursor, output+127) == next);
				check(valid);
				pad = next-value;
				if(pad){
					snprintf(expected, sizeof(expected), "uchar\t_pad%ld[%ld]; ", value, pad);
					check(strcmp(output+1, expected) == 0);
				}else
					check(cursor == output+1 && output[1] == 0x5a);
			}else{
				check(!valid && cursor == output+1 && output[1] == 0x5a);
			}
			expectfatal = 0;
			check(output[0] == 0x5a && output[127] == 0x5a);
			scenarios++;
			memset(streamed, 0x5a, sizeof(streamed));
			expectfatal = !valid;
			if(setjmp(rejection) == 0){
				check(stubalign(value, aligns[j], nil, nil) == next && valid);
				pad = next-value;
				if(pad){
					snprintf(expected, sizeof(expected), "\tuchar\t_pad%ld[%ld];\n", value, pad);
					check(strcmp(streamed, expected) == 0);
				}else
					check(streamed[0] == 0x5a);
			}else
				check(!valid && streamed[0] == 0x5a);
			expectfatal = 0;
			scenarios++;
		}
}


/* Reject invalid layouts even when the destination has already filled. */
static void
badlayouts(void)
{
	Type aggregate, field;
	Decl declaration;
	char out[1026];
	int variant, capacity;

	for(variant = 0; variant < 7; variant++)
		for(capacity = 0; capacity <= 3; capacity++){
			memset(&aggregate, 0, sizeof(aggregate));
			memset(&field, 0, sizeof(field));
			memset(&declaration, 0, sizeof(declaration));
			aggregate.kind = Ttuple;
			aggregate.decl = &anon;
			aggregate.ids = &declaration;
			aggregate.size = aggregate.align = 4;
			field.kind = Tint;
			field.size = field.align = 4;
			declaration.ty = &field;
			declaration.sym = &symbol;
			switch(variant){
			case 0: field.align = 0; break;
			case 1: field.align = 3; break;
			case 2: declaration.offset = 1; break;
			case 3: field.size = -1; break;
			case 4: field.size = 0x80000000L; break;
			case 5: aggregate.size = 5; break;
			case 6:
				field.size = aggregate.size = 0x7fffffff;
				field.align = 1;
				aggregate.align = 8;
				break;
			}
			memset(out, 0x5a, sizeof(out));
			expectfatal = 1;
			if(setjmp(rejection) == 0){
				ctprint(out+1, out+1+capacity, &aggregate);
				check(0);
			}
			expectfatal = 0;
			check(live == 0 && out[0] == 0x5a && out[capacity+1] == 0x5a);
			scenarios++;
		}
}


/* Emit complete declarations for a separate strict host-C compilation check. */
static void
emittypes(void)
{
	Type *t;
	char output[32768];
	int i;

	printf("typedef int WORD; typedef long long LONG; typedef unsigned char BYTE; typedef unsigned char uchar;\n");
	printf("typedef struct Array Array; typedef struct Module_field { WORD value; } Module_field;\n");
	for(i = 0; i < 1000; i++){
		memset(types, 0, sizeof(types));
		memset(fields, 0, sizeof(fields));
		used = nfields = 0;
		t = make(4);
		output[0] = 0;
		ctprint(output, output+sizeof(output), t);
		check(live == 0 && strlen(output) < sizeof(output)-1);
		printf("%s value_%d;\n", output, i);
		if(wordbytes == sizeof(void*))
			printf("typedef char size_%d[sizeof(value_%d) == %ld ? 1 : -1];\n", i, i, t->size);
	}
}


/* Run independent families with a small C stack and both target pointer widths. */
int
main(int argc, char **argv)
{
	struct rlimit limit;
	Type *t;
	int i, capacity;

	check(argc == 3);
	wordbytes = atoi(argv[2]);
	check(wordbytes == 4 || wordbytes == 8);
	fmtinstall('R', ctypeformat);
	if(strcmp(argv[1], "emit") == 0){
		emittypes();
		return 0;
	}
	limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	if(strcmp(argv[1], "shallow") == 0){
		literals();
		for(i = 0; i < 1000; i++){
			memset(types, 0, sizeof(types));
			memset(fields, 0, sizeof(fields));
			used = nfields = 0;
			t = make(4);
			for(capacity = 0; capacity <= 64; capacity++)
				compare(t, capacity);
			compare(t, 1024);
		}
	}else if(strcmp(argv[1], "dots") == 0)
		dots(0);
	else if(strcmp(argv[1], "terminator") == 0)
		dots(1);
	else if(strcmp(argv[1], "refs") == 0)
		deep(Tref);
	else if(strcmp(argv[1], "picks") == 0)
		deep(Tadtpick);
	else if(strcmp(argv[1], "tuples") == 0)
		deep(Ttuple);
	else if(strcmp(argv[1], "alignment") == 0)
		alignment();
	else if(strcmp(argv[1], "layouts") == 0)
		badlayouts();
	else
		check(0);
	printf("stub print %s width %d: %lu scenarios\n", argv[1], wordbytes, scenarios);
	return 0;
}
