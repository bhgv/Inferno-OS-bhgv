#include "limbo.h"
#include <stdio.h>
#include <setjmp.h>
#include <sys/resource.h>

#define check(c) predicate(!!(c), __LINE__) // Keep every ownership and arithmetic check under NDEBUG.

Node znode; // Actual zero template used by node and temporary constructors.
Src nosrc; // Source identity for temporary declarations.
Tattr tattr[Tend]; // Actual temporary size/pointer classification inputs.
Type *tstring; // Builtin string Type identity.
static Decl *wtemp, *bigtemp; // Production temporary reuse lists.
static Node *ntoz; // Production deferred-zeroing queue.
static int ntemp; // Production temporary-name sequence.
static Type smalltype, bigtype, moduletype, interfacetype; // Borrowed fixture Types.
static Decl declaration, second; // Borrowed reference destinations and linked imports.
static Desc descriptor; // Stable descriptor identity returned by the fixture provider.
static Sym symbol; // Interned temporary or string symbol.
static char symbolname[64]; // Owned stable name for the fixture symbol.
static void *allocations[128]; // Node/Decl results owned until scenario reset.
static int nallocations, zeroes, descriptorcalls; // Result and callback accounting.
static int expectfatal; // Only explicit exhausted-count probes expect rejection.
static jmp_buf rejection; // Return from an expected compiler fatal.
static ulong scenarios; // Completed retention/release scenarios.


/* Report the first arithmetic, owner, queue or callback discrepancy. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "reference use check at %d\n", line);
		abort();
	}
}


/* Keep ASan checks without process-wide leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* All deliberate failures must be the checked-count diagnostic. */
void
fatal(char *message, ...)
{
	if(expectfatal){
		check(strcmp(message, "declaration reference count overflow") == 0);
		longjmp(rejection, 1);
	}
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Own actual node/declaration constructor results separately from borrowed inputs. */
void*
allocmem(ulong bytes)
{
	void *p;

	check(nallocations < nelem(allocations));
	p = malloc(bytes);
	check(p != nil);
	allocations[nallocations++] = p;
	return p;
}


/* Supply deterministic temporary names using ordinary formats used by talloc. */
char*
seprint(char *start, char *end, char *format, ...)
{
	va_list args;
	int count;

	check(start < end);
	va_start(args, format);
	count = vsnprintf(start, end-start, format, args);
	va_end(args);
	check(count >= 0 && count < end-start);
	return start+count;
}


/* Intern one temporary symbol; string tests start with that same borrowed symbol. */
Sym*
enter(char *name, int token)
{
	check(token == 0 && strlen(name) < sizeof(symbolname));
	strcpy(symbolname, name);
	symbol.name = symbolname;
	return &symbol;
}


/* Observe global string installation without importing unrelated scope machinery. */
void
installids(int store, Decl *d)
{
	check(store == Dglobal && d->sym == &symbol);
	d->store = store;
	symbol.decl = d;
}


/* Descriptor identity is controlled; production code decides whether it can reuse it. */
Desc*
mktdesc(Type *t)
{
	check(t == &bigtype);
	descriptorcalls++;
	return &descriptor;
}


/* Observe real temporary zeroing requests without emitting unrelated Dis instructions. */
void
zcom1(Node *n, Node **list)
{
	check(n != nil && n->decl != nil && list == nil && n->decl->tref == 1);
	zeroes++;
}

IMPLEMENTATION


/* Reset borrowed metadata and release every real constructor result. */
static void
reset(void)
{
	int i;

	for(i = 0; i < nallocations; i++)
		free(allocations[i]);
	nallocations = 0;
	memset(&declaration, 0, sizeof(declaration));
	memset(&second, 0, sizeof(second));
	memset(&smalltype, 0, sizeof(smalltype));
	memset(&bigtype, 0, sizeof(bigtype));
	memset(&moduletype, 0, sizeof(moduletype));
	memset(&interfacetype, 0, sizeof(interfacetype));
	memset(&symbol, 0, sizeof(symbol));
	memset(tattr, 0, sizeof(tattr));
	smalltype.kind = Tint; smalltype.size = smalltype.align = 4;
	bigtype.kind = Ttuple; bigtype.size = 32; bigtype.align = 8;
	tattr[Ttuple].big = 1;
	moduletype.kind = Tmodule; moduletype.ok = OKverify;
	moduletype.tof = &interfacetype; moduletype.decl = &declaration;
	interfacetype.kind = Tiface;
	declaration.ty = &smalltype; second.ty = &smalltype;
	declaration.sym = second.sym = &symbol;
	declaration.desc = second.desc = &descriptor;
	symbol.name = symbolname; strcpy(symbolname, "fixture");
	tstring = &smalltype;
	wtemp = bigtemp = nil; ntoz = nil; ntemp = 0;
	zeroes = descriptorcalls = 0;
}


/* Check actual entry points with independently computed int-boundary expectations. */
static void
usecase(char *family, int count, int big, int reuse)
{
	Node input, output, *result;
	Decl *d;
	int valid, originalrefs, originaltref;
	vlong expected;

	reset();
	memset(&input, 0, sizeof(input)); memset(&output, 0, sizeof(output));
	declaration.refs = second.refs = count;
	declaration.tref = count;
	declaration.ty = big ? &bigtype : &smalltype;
	input.decl = &declaration; input.ty = declaration.ty;
	originalrefs = originaltref = count;
	expected = (vlong)count+1;
	valid = expected <= 0x7fffffff;
	if(strcmp(family, "release") == 0 || strcmp(family, "free") == 0 || strcmp(family, "later") == 0){
		expected = (vlong)count-1;
		valid = expected >= (-0x7fffffff-1);
	}
	if((strcmp(family, "acquire") == 0 || strcmp(family, "release") == 0
	|| strcmp(family, "free") == 0 || strcmp(family, "later") == 0) && count == 0){
		valid = 1; expected = 0;
	}
	expectfatal = !valid;
	if(setjmp(rejection) == 0){
		if(strcmp(family, "name") == 0){
			result = mkdeclname(&nosrc, &declaration);
			check(valid && declaration.refs == expected);
			check(result->decl == &declaration && result->ty == declaration.ty && result->op == Oname);
		}else if(strcmp(family, "imports") == 0){
			declaration.link = &second; increfs(&declaration);
			check(valid && declaration.refs == expected && second.refs == expected);
		}else if(strcmp(family, "module") == 0){
			modrefable(&moduletype);
			check(valid && declaration.refs == expected && interfacetype.linkall == 1);
			modrefable(&moduletype);
			check(declaration.refs == expected);
		}else if(strcmp(family, "string") == 0){
			symbol.decl = &declaration;
			check(globalsconst(&input) == &declaration);
			check(valid && declaration.refs == expected && nallocations == 0);
		}else if(strcmp(family, "acquire") == 0){
			check(tacquire(&input) == &input && valid && declaration.tref == expected);
		}else if(strcmp(family, "release") == 0){
			trelease(&input); check(valid && declaration.tref == expected);
		}else if(strcmp(family, "free") == 0){
			tfree(&input); check(valid && declaration.tref == expected);
			check(zeroes == (count == 2));
		}else if(strcmp(family, "later") == 0){
			tfreelater(&input);
			check(valid && declaration.tref == (count == 2 ? 2 : expected));
			check((ntoz != nil) == (count == 2) && zeroes == 0);
			if(ntoz != nil){
				tfreenow(); check(ntoz == nil && declaration.tref == 1 && zeroes == 1);
			}
		}else if(strcmp(family, "allocate") == 0){
			/* Separate ordinary reuse coverage; overflow allocation cases run below. */
			check(valid);
			declaration.tref = 2; declaration.refs = 7;
			if(reuse == 1){
				declaration.tref = 1;
				if(big) bigtemp = &declaration; else wtemp = &declaration;
			}
			result = talloc(&output, declaration.ty, reuse == 0 ? &input : nil);
			check(result == &output && output.op == Oname && output.ty == declaration.ty);
			d = output.decl;
			check(d != nil && d->tref == (reuse == 0 ? 3 : 2));
			if(reuse < 2)
				check(d == &declaration && d->refs == (big ? 8 : 7));
			else
				check(d->refs == 1 && d->store == Dlocal);
			check(descriptorcalls == big);
		}else
			check(0);
		check(valid);
	}else{
		check(!valid && declaration.refs == originalrefs && declaration.tref == originaltref);
	}
	expectfatal = 0;
	scenarios++;
}


/* Verify saturated temporary reuse fails at the actual selected counter update. */
static void
allocateoverflow(int big, int counter, int list)
{
	Node input, output;

	reset(); memset(&input, 0, sizeof(input));
	declaration.ty = big ? &bigtype : &smalltype;
	declaration.tref = list ? 1 : counter == 0 ? 0x7fffffff : 2;
	declaration.refs = counter == 1 ? 0x7fffffff : 3;
	input.decl = &declaration;
	if(list){
		check(big && counter == 1); bigtemp = &declaration;
	}
	expectfatal = 1;
	if(setjmp(rejection) == 0){
		talloc(&output, declaration.ty, list ? nil : &input);
		check(0);
	}
	expectfatal = 0;
	if(counter == 0)
		check(declaration.tref == 0x7fffffff);
	else
		check(declaration.refs == 0x7fffffff);
	scenarios++;
}


/* Preserve null/zero guards, fresh-string construction and non-counting fast returns. */
static void
edges(void)
{
	Node n, result;
	Decl *d;

	reset(); memset(&n, 0, sizeof(n));
	check(tacquire(nil) == nil && tacquire(&n) == &n);
	trelease(nil); tfree(nil); tfreelater(nil); tfreenow(); increfs(nil); modrefable(nil);
	check(zeroes == 0 && ntoz == nil && nallocations == 0); scenarios++;
	n.decl = &declaration;
	check(tacquire(&n) == &n); trelease(&n); tfree(&n); tfreelater(&n);
	check(declaration.tref == 0 && zeroes == 0 && ntoz == nil); scenarios++;
	d = globalsconst(&n);
	check(d != &declaration && d == symbol.decl && d->refs == 1);
	check(d->store == Dglobal && d->init == &n && d->sym == &symbol); scenarios++;
	reset(); memset(&n, 0, sizeof(n)); n.decl = &declaration;
	declaration.refs = 0x7fffffff; declaration.tref = 2;
	talloc(&result, &smalltype, &n);
	check(result.decl == &declaration && declaration.refs == 0x7fffffff && declaration.tref == 3); scenarios++;
	reset(); declaration.refs = 0x7fffffff; moduletype.ok |= OKmodref;
	modrefable(&moduletype);
	check(declaration.refs == 0x7fffffff && interfacetype.linkall == 0); scenarios++;
}


/* Drive a long import chain and verify the exact updated prefix on rejection. */
static void
chaincase(void)
{
	Decl *chain;
	int i, pass, stop;

	chain = calloc(100000, sizeof(*chain)); check(chain != nil);
	for(pass = 0; pass < 2; pass++){
		for(i = 0; i < 100000; i++){
			chain[i].link = i == 99999 ? nil : &chain[i+1]; chain[i].refs = 7;
		}
		stop = pass ? 50000 : 100000;
		if(pass) chain[stop].refs = 0x7fffffff;
		expectfatal = pass;
		if(setjmp(rejection) == 0){
			increfs(chain); check(!pass);
		}else
			check(pass);
		expectfatal = 0;
		for(i = 0; i < 100000; i++)
			check(chain[i].refs == (i < stop ? 8 : i == stop ? 0x7fffffff : 7));
		scenarios++;
	}
	free(chain);
}


/* Preserve deferred-owner holds while draining more than one queued temporary. */
static void
deferredcase(void)
{
	Node one, two;

	reset(); memset(&one, 0, sizeof(one)); memset(&two, 0, sizeof(two));
	declaration.tref = second.tref = 2; one.decl = &declaration; two.decl = &second;
	tfreelater(&one); tfreelater(&two);
	check(ntoz != nil && ntoz->left != nil && declaration.tref == 2 && second.tref == 2);
	tfreenow(); check(ntoz == nil && declaration.tref == 1 && second.tref == 1 && zeroes == 2);
	tfreenow(); check(zeroes == 2);
	scenarios++;
}


/* Drain queued owners only after all additional live aliases have been released. */
static void
drain(Node *expressions, int *holds, int *pending, int *expectedzeroes)
{
	int i;

	for(i = 0; i < 8; i++)
		if(pending[i])
			while(holds[i] > 2){
				trelease(&expressions[i]); holds[i]--;
			}
	tfreenow();
	for(i = 0; i < 8; i++)
		if(pending[i]){
			holds[i] = 1; pending[i] = 0; (*expectedzeroes)++;
		}
}


/* Compare shared-owner lifetime sequences against an independent hold/queue model. */
static void
sequences(void)
{
	Decl declarations[8];
	Node expressions[8], *queued;
	int holds[8], pending[8], seed, step, i, action, selected, expectedzeroes, count;
	uint state;

	for(seed = 0; seed < 10000; seed++){
		reset(); memset(declarations, 0, sizeof(declarations));
		memset(expressions, 0, sizeof(expressions)); memset(pending, 0, sizeof(pending));
		for(i = 0; i < 8; i++){
			declarations[i].tref = holds[i] = 1; declarations[i].refs = seed%31;
			expressions[i].decl = &declarations[i];
		}
		state = seed+1; expectedzeroes = 0;
		for(step = 0; step < 100; step++){
			state = state*1664525+1013904223;
			selected = (state>>16)%8; action = (state>>24)%5;
			if(action == 0){
				tacquire(&expressions[selected]); holds[selected]++;
			}else if(action == 4)
				drain(expressions, holds, pending, &expectedzeroes);
			else if(holds[selected] > 1+pending[selected]){
				holds[selected]--;
				if(action == 1)
					trelease(&expressions[selected]);
				else if(action == 2){
					tfree(&expressions[selected]);
					if(holds[selected] == 1) expectedzeroes++;
				}else{
					tfreelater(&expressions[selected]);
					if(holds[selected] == 1){ holds[selected] = 2; pending[selected] = 1; }
				}
			}
			count = 0;
			for(queued = ntoz; queued != nil; queued = queued->left){
				check(++count <= 8);
				check(queued->decl >= declarations && queued->decl < declarations+8);
				check(pending[queued->decl-declarations]);
			}
			for(i = 0; i < 8; i++){
				check(declarations[i].tref == holds[i] && declarations[i].refs == seed%31);
				check(expressions[i].left == nil && expressions[i].right == nil);
				count -= pending[i];
			}
			check(count == 0 && zeroes == expectedzeroes);
		}
		drain(expressions, holds, pending, &expectedzeroes);
		for(i = 0; i < 8; i++){
			while(holds[i] > 1){
				tfree(&expressions[i]); holds[i]--;
				if(holds[i] == 1) expectedzeroes++;
			}
			check(declarations[i].tref == 1);
		}
		check(ntoz == nil && zeroes == expectedzeroes);
		scenarios++;
	}
}


/* Run real producer/lifetime paths over ordinary and exhausted counts. */
int
main(int argc, char **argv)
{
	static int counts[] = { -0x7fffffff-1, -0x7fffffff, -1, 0, 2, 3, 31, 255, 65535, 0x7ffffffe, 0x7fffffff };
	struct rlimit limit;
	int i, big, reuse;

	check(argc == 2); limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	if(strcmp(argv[1], "edges") == 0)
		edges();
	else if(strcmp(argv[1], "sequences") == 0)
		sequences();
	else if(strcmp(argv[1], "chain") == 0)
		chaincase();
	else if(strcmp(argv[1], "deferred") == 0)
		deferredcase();
	else if(strcmp(argv[1], "allocate-small-hold") == 0)
		allocateoverflow(0, 0, 0);
	else if(strcmp(argv[1], "allocate-big-hold") == 0)
		allocateoverflow(1, 0, 0);
	else if(strcmp(argv[1], "allocate-big-refs") == 0)
		allocateoverflow(1, 1, 0);
	else if(strcmp(argv[1], "allocate-list-refs") == 0)
		allocateoverflow(1, 1, 1);
	else if(strcmp(argv[1], "allocate") == 0){
		for(big = 0; big < 2; big++)
			for(reuse = 0; reuse < 3; reuse++)
				usecase(argv[1], 7, big, reuse);
		allocateoverflow(0, 0, 0); allocateoverflow(1, 0, 0);
		allocateoverflow(1, 1, 0); allocateoverflow(1, 1, 1);
	}else
		for(i = 0; i < nelem(counts); i++)
			usecase(argv[1], counts[i], 0, 0);
	reset();
	printf("reference uses WORD%d %s: %lu scenarios\n", IBY2WD*8, argv[1], scenarios);
	return 0;
}
