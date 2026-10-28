#include "limbo.h"
#include <stdio.h>
#include <setjmp.h>
#include <sys/resource.h>

#define check(c) predicate(!!(c), __LINE__) // Keep scope/order/capacity checks under NDEBUG.
#define Depth 100000 // Global AST depth must not be a C-stack limit.

int scope, labdep, maxlabdep; // Production lexical and label-stack indices.
Node **labstack; // Production dynamically allocated label storage.
char debug[256]; // Disable unrelated debug output.
char *kindname[Tend]; // Debug kind names are unused in this fixture.
Type *tnone; // Type identity used by the actual undefined-declaration constructor.
static Decl *scopes[MaxScope], *tails[MaxScope]; // Actual scope declaration heads/tails.
static Node *scopenode[MaxScope]; // Actual owning AST scope cells.
static uchar scopekind[MaxScope]; // Actual lexical scope categories.
static Node arena[32768]; // Borrowed shallow global declaration ASTs.
static Type types[16384], notype; // Borrowed Types supplied to the binding callback.
static Decl declarations[32768], outer[8]; // Borrowed original identifiers and outer bindings.
static Sym symbols[8]; // Shared names exercise lexical restoration and undefined propagation.
static char *names[] = { "a", "b", "c", "d", "u", "v", "w", "x" }; // Stable symbol spellings.
static int nnodes, ntypes, ndecls; // Shallow graph allocation cursors.
static Decl *owned[1024]; // Actual undefined declarations owned until attempt reset.
static int nowned; // Number of constructor results awaiting fixture cleanup.
static void *privateblocks[64]; // Private gbind continuation blocks.
static int nprivate; // Live work ownership, independent of installed declarations.
static void *labelowner; // Storage owned by the label realloc provider.
static ulong labelbytes, request; // Existing label allocation size and last requested size.
static int allocations, trapallocation, expecting; // Allocation/fatal probe controls.
static jmp_buf rejection; // Return from expected compiler rejection or observed large allocation.
static Type *mutationtrigger; // Callback Type that changes one deferred ancestor cell.
static Node *mutationparent; // Borrowed sequence/pick parent changed by that callback.
static int mutationkind; // Delayed sibling insertion or pick-declaration replacement.
static uint randomstate; // Reproducible shallow global graph generator.
static uvlong trace; // Ordered binding/error source and scope trace.
static ulong scenarios; // Completed independent scenarios.
static void freeloc(Decl*); // Scope cleanup helper defined by production support.


/* Fail explicitly on incorrect lexical state, callback order or ownership. */
static void
predicate(int valid, int line)
{
	if(!valid){
		fprintf(stderr, "scope binding check at %d\n", line);
		abort();
	}
}


/* Preserve access instrumentation without process-wide leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Recover only from deliberate scope/depth/capacity diagnostics. */
void
fatal(char *message, ...)
{
	if(expecting){
		check(strcmp(message, "scope too deep") == 0
			|| strcmp(message, "invalid label stack depth") == 0
			|| strcmp(message, "label stack too large") == 0);
		longjmp(rejection, 1);
	}
	fprintf(stderr, "unexpected fatal: %s\n", message);
	abort();
}


/* Fingerprint ordered source identities and lexical visibility. */
static void
event(int kind, int identity)
{
	trace = (trace^(uvlong)kind)*1099511628211ULL;
	trace = (trace^(uvlong)identity)*1099511628211ULL;
	trace = (trace^(uvlong)scope)*1099511628211ULL;
}


/* Observe actual undefined/redeclaration diagnostics without invoking format converters. */
void
error(Line line, char *message, ...)
{
	USED(message);
	event(2, line.line);
}


/* Scope cleanup should not emit warnings in these bound graphs. */
void
warn(Line line, char *message, ...)
{
	USED(message);
	event(3, line.line);
}


/* Record duplicate-label diagnostics from actual pushlabel scans. */
void
nerror(Node *n, char *message, ...)
{
	USED(message);
	event(4, n->src.start.line);
}


/* No debug path should require streamed output. */
int
print(char *message, ...)
{
	fatal(message);
	return -1;
}


/* The fixture names describe explicit modules, not implementation-module aliases. */
int
isimpmod(Sym *s)
{
	USED(s);
	return 0;
}


/* Own real mkdecl results separately from private traversal continuations. */
void*
allocmem(ulong bytes)
{
	void *p;

	check(bytes == sizeof(Decl) && nowned < nelem(owned));
	p = malloc(bytes);
	check(p != nil);
	owned[nowned++] = p;
	return p;
}


/* Allocate only bounded gbind work and retain independent ownership accounting. */
static void*
workalloc(ulong bytes)
{
	void *p;

	check(nprivate < nelem(privateblocks) && bytes < 128*1024*1024);
	p = malloc(bytes);
	check(p != nil);
	privateblocks[nprivate++] = p;
	return p;
}


/* Require every normal binding completion to release its private storage. */
static void
workfree(void *p)
{
	int i;

	for(i = 0; i < nprivate; i++)
		if(privateblocks[i] == p){
			privateblocks[i] = privateblocks[--nprivate];
			free(p);
			return;
		}
	check(0);
}


/* Move label storage on every growth; large-boundary probes stop before actual allocation. */
void*
reallocmem(void *old, ulong bytes)
{
	void *next;

	request = bytes;
	allocations++;
	if(trapallocation)
		longjmp(rejection, 2);
	check(old == labelowner && bytes > 0 && bytes <= 65536);
	next = malloc(bytes);
	check(next != nil);
	if(old != nil)
		memmove(next, old, labelbytes < bytes ? labelbytes : bytes);
	free(old);
	labelowner = next;
	labelbytes = bytes;
	return next;
}

SUPPORT


/* Supply lexical Type-identifier binding while the real scope helpers manage visibility. */
void
bindtypes(Type *t)
{
	Sym *s;
	Decl *d;

	if(t == nil)
		return;
	event(1, t->src.start.line);
	if(t == mutationtrigger){
		if(mutationkind == 1)
			mutationparent->right = &arena[2];
		else
			mutationparent->right->left->decl = &declarations[3];
		mutationtrigger = nil;
	}
	if(t->ok & OKbind)
		return;
	t->ok |= OKbind;
	if(t->kind == Tid){
		s = t->decl->sym;
		d = s->decl;
		if(d == nil)
			d = undefed(&t->src, s);
		s->unbound = nil;
		t->decl = d;
		event(5, d->src.start.line);
	}
}

#define allocmem workalloc // Observe only binding work, not declaration construction.
#define free workfree // Source ASTs and installed identifiers are borrowed.
IMPLEMENTATION
#undef free
#undef allocmem

REFERENCE


/* Reset independent attempt ownership, actual scope arrays and all borrowed bindings. */
static void
reset(void)
{
	int i;

	check(nprivate == 0);
	for(i = 0; i < nowned; i++)
		free(owned[i]);
	nowned = 0;
	free(labelowner);
	labelowner = nil;
	labelbytes = request = 0;
	allocations = trapallocation = expecting = 0;
	labstack = nil;
	labdep = maxlabdep = 0;
	memset(scopes, 0, sizeof(scopes));
	memset(tails, 0, sizeof(tails));
	memset(scopenode, 0, sizeof(scopenode));
	memset(scopekind, 0, sizeof(scopekind));
	memset(arena, 0, sizeof(arena));
	memset(types, 0, sizeof(types));
	memset(declarations, 0, sizeof(declarations));
	memset(symbols, 0, sizeof(symbols));
	memset(outer, 0, sizeof(outer));
	for(i = 0; i < nelem(symbols); i++){
		symbols[i].name = names[i];
		symbols[i].decl = i < 4 ? &outer[i] : nil;
		symbols[i].unbound = &outer[i];
		outer[i].src.start.line = -i-1;
		outer[i].sym = &symbols[i];
		outer[i].scope = ScopeBuiltin;
	}
	scope = ScopeGlobal;
	tnone = &notype;
	nnodes = ntypes = ndecls = 0;
	mutationtrigger = nil;
	trace = 1469598103934665603ULL;
}


/* Create a borrowed declaration with a stable identity and expected lexical depth. */
static Decl*
newdecl(int lexical, int store)
{
	Decl *d;

	check(ndecls < nelem(declarations));
	d = &declarations[ndecls++];
	d->src.start.line = 20000+ndecls;
	d->scope = lexical;
	d->store = store;
	d->sym = &symbols[(randomstate>>20)%8];
	d->ty = &notype;
	return d;
}


/* Create a callback Type, optionally referring to a symbol needing lexical lookup. */
static Type*
newtype(int kind)
{
	Type *t;

	check(ntypes < nelem(types));
	t = &types[ntypes++];
	t->src.start.line = 10000+ntypes;
	t->kind = kind;
	if(kind == Tid)
		t->decl = newdecl(0, Dtype);
	return t;
}


/* Allocate an AST node without changing any lexical binding. */
static Node*
newnode(int op)
{
	Node *n;

	check(nnodes < nelem(arena));
	n = &arena[nnodes++];
	n->op = op;
	n->src.start.line = nnodes;
	return n;
}


/* Build mixed global declarations with generic, aggregate and pick scopes. */
static Node*
make(int depth, int lexical)
{
	Node *n, *right, *field;
	Type *t;
	Decl *d;
	int choice, generic;

	randomstate = randomstate*1664525+1013904223;
	choice = (randomstate>>16)%6;
	if(depth == 0) choice = 4+choice%2;
	if(choice == 0){
		n = newnode(Oseq);
		n->left = make(depth-1, lexical);
		n->right = make(depth-1, lexical);
	}else if(choice == 1){
		n = newnode(Otypedecl);
		n->decl = newdecl(lexical, Dtype);
		n->decl->ty = newtype(Tid);
		n->left = make(depth-1, lexical);
	}else if(choice == 2){
		n = newnode(randomstate&1 ? Oadtdecl : Omoddecl);
		t = n->ty = newtype(Tadt);
		generic = (randomstate>>8)&1;
		if(generic) t->polys = newdecl(lexical+1, Dtype);
		t->ids = newdecl(lexical+generic+1, Dglobal);
		n->left = make(depth-1, lexical+generic+1);
	}else if(choice == 3){
		n = newnode(Opickdecl);
		n->left = make(depth-1, lexical);
		right = n->right = newnode(Oseq);
		field = right->left = newnode(Ofielddecl);
		d = field->decl = newdecl(lexical, Dtype);
		d->ty = newtype(Tadt);
		d->ty->ids = newdecl(lexical+1, Dglobal);
		right->right = make(depth-1, lexical+1);
	}else if(choice == 4){
		n = newnode(Ofielddecl);
		n->decl = newdecl(lexical, Dtype);
		n->decl->ty = newtype(Tid);
	}else{
		n = newnode(Onothing);
		/* Skipped declarations must not bind incidental child metadata. */
		n->left = newnode(Ofielddecl);
		n->left->decl = newdecl(lexical, Dtype);
		n->left->decl->ty = newtype(Tid);
	}
	return n;
}


/* Capture Type bindings, symbol restoration, propagated identifiers and scope tails. */
static uvlong
snapshot(void)
{
	uvlong h;
	int i;

	check(nprivate == 0 && scope == ScopeGlobal);
	h = trace;
	for(i = 0; i < ntypes; i++){
		h = (h^(uvlong)types[i].ok)*1099511628211ULL;
		h = (h^(uvlong)(types[i].decl == nil ? 0 : types[i].decl->src.start.line))*1099511628211ULL;
	}
	for(i = 0; i < nelem(symbols); i++){
		h = (h^(uvlong)(symbols[i].decl == nil ? 0 : symbols[i].decl->src.start.line))*1099511628211ULL;
		h = (h^(uvlong)(symbols[i].unbound == nil))*1099511628211ULL;
	}
	for(i = 0; i < nowned; i++){
		h = (h^(uvlong)owned[i]->scope)*1099511628211ULL;
		h = (h^(uvlong)owned[i]->store)*1099511628211ULL;
		h = (h^(uvlong)(owned[i]->next == nil ? 0 : owned[i]->next->src.start.line))*1099511628211ULL;
	}
	for(i = 0; i < MaxScope; i++){
		h = (h^(uvlong)(scopes[i] == nil ? 0 : scopes[i]->src.start.line))*1099511628211ULL;
		h = (h^(uvlong)(tails[i] == nil ? 0 : tails[i]->src.start.line))*1099511628211ULL;
	}
	return h;
}


/* Compare real lexical scope operations through frozen and iterative global traversals. */
static uvlong
attempt(int seed, int original)
{
	Node *n;

	reset();
	randomstate = seed+1;
	n = make(4, scope);
	if(original) reference_gbind(n); else gbind(n);
	return snapshot();
}


/* Preserve delayed sequence reads and the captured pick declaration across callback mutations. */
static uvlong
mutation(int pick, int original)
{
	reset();
	nnodes = 4; ntypes = 3; ndecls = 5;
	mutationparent = &arena[0]; mutationkind = pick ? 2 : 1;
	arena[1].op = Ofielddecl; arena[1].decl = &declarations[0];
	arena[2].op = Ofielddecl; arena[2].decl = &declarations[1];
	declarations[0].ty = &types[0]; declarations[1].ty = &types[1];
	types[0].kind = types[1].kind = Tint;
	types[0].src.start.line = 1; types[1].src.start.line = 2;
	if(!pick){
		arena[0].op = Oseq; arena[0].left = &arena[1];
		mutationtrigger = &types[0];
	}else{
		arena[0].op = Opickdecl; arena[0].right = &arena[1];
		arena[1].op = Oseq; arena[1].left = &arena[2]; arena[1].right = &arena[3];
		arena[2].decl = &declarations[0];
		types[0].kind = Tadt; types[0].ids = &declarations[1];
		declarations[1].scope = ScopeGlobal+1;
		declarations[1].sym = &symbols[0];
		arena[3].op = Ofielddecl; arena[3].decl = &declarations[2];
		declarations[2].ty = &types[1];
		declarations[3].ty = &types[2]; types[2].ids = &declarations[4];
		declarations[4].sym = &symbols[1];
		mutationtrigger = &types[1];
	}
	if(original) reference_gbind(arena); else gbind(arena);
	check(mutationtrigger == nil && (types[1].ok&OKbind));
	check(symbols[0].decl == &outer[0] && symbols[1].decl == &outer[1]);
	if(pick) check(arena[2].decl == &declarations[3]);
	else check(arena[0].right == &arena[2]);
	return snapshot();
}


/* Independently verify generic shadowing and outward propagation of undefined names. */
static void
edges(void)
{
	Node n, body;
	Type aggregate, lookup;
	Decl field, generic, placeholder;
	int variant;

	for(variant = 0; variant < 5; variant++){
		reset();
		memset(&n, 0, sizeof(n)); memset(&body, 0, sizeof(body));
		memset(&aggregate, 0, sizeof(aggregate)); memset(&lookup, 0, sizeof(lookup));
		memset(&field, 0, sizeof(field)); memset(&generic, 0, sizeof(generic));
		memset(&placeholder, 0, sizeof(placeholder));
		lookup.kind = Tid; lookup.decl = &placeholder;
		body.op = Ofielddecl; body.decl = &declarations[0]; declarations[0].ty = &lookup;
		placeholder.sym = &symbols[variant == 1 || variant == 3 ? 4 : 0];
		if(variant < 2)
			gbind(&body);
		else{
			n.op = Oadtdecl; n.ty = &aggregate; n.left = &body;
			aggregate.kind = Tadt; aggregate.ids = &field;
			field.ty = &notype; field.store = Dglobal; field.sym = &symbols[0];
			field.scope = ScopeGlobal+1;
			if(variant == 4){
				generic.ty = &notype; generic.store = Dtype; generic.sym = &symbols[0];
				generic.scope = ScopeGlobal+1; aggregate.polys = &generic;
				field.scope++; field.sym = &symbols[1];
			}
			gbind(&n);
		}
		check(scope == ScopeGlobal && nprivate == 0);
		check(symbols[0].decl == &outer[0] && symbols[1].decl == &outer[1]);
		if(variant == 0) check(lookup.decl == &outer[0]);
		if(variant == 1 || variant == 3){
			check(nowned == 1 && lookup.decl == owned[0] && symbols[4].decl == owned[0]);
			check(owned[0]->scope == ScopeGlobal && owned[0]->store == Dwundef);
			check(scopes[ScopeGlobal] == owned[0]);
		}
		if(variant == 2) check(lookup.decl == &field && field.old == nil);
		if(variant == 4) check(lookup.decl == &generic && generic.old == nil && field.old == nil);
		scenarios++;
	}
}


/* Respect the fixed lexical-depth limit even though the AST work stack is dynamic. */
static void
nestedscopes(void)
{
	int pass, i, depth, status;

	for(pass = 0; pass < 2; pass++){
		reset();
		depth = MaxScope-ScopeGlobal-1+pass;
		for(i = 0; i < depth; i++){
			arena[i].op = Omoddecl;
			arena[i].ty = &types[i];
			arena[i].left = i+1 == depth ? nil : &arena[i+1];
			types[i].kind = Tmodule; types[i].ids = &declarations[i];
			declarations[i].scope = ScopeGlobal+i+1;
			declarations[i].sym = &symbols[0]; declarations[i].ty = &notype;
		}
		expecting = pass;
		status = setjmp(rejection);
		if(status == 0){
			gbind(arena);
			check(!pass && scope == ScopeGlobal && nprivate == 0 && symbols[0].decl == &outer[0]);
		}else{
			check(pass && scope == MaxScope-1);
			/* The compiler normally exits on fatal; release the abandoned probe frames here. */
			while(nprivate)
				free(privateblocks[--nprivate]);
		}
		expecting = 0;
		scenarios++;
	}
}


/* Check every valid starting index and rejection before the first out-of-range scope. */
static void
scopebounds(int repush)
{
	Node marker;
	int start;

	for(start = ScopeBuiltin; start < MaxScope; start++){
		reset();
		scope = start;
		expecting = start == MaxScope-1;
		if(setjmp(rejection) == 0){
			if(repush) repushids(nil); else pushscope(&marker, Sother);
			check(start < MaxScope-1 && scope == start+1);
			check(scopes[scope] == nil && tails[scope] == nil);
			check(scopenode[scope] == (repush ? nil : &marker));
			check(popscope() == nil && scope == start);
		}else
			check(start == MaxScope-1 && scope == start);
		expecting = 0;
		scenarios++;
	}
}


/* Bind deep sequence/type-declaration chains with no additional lexical scopes. */
static void
deep(int sequence)
{
	Node *chain, leaf;
	Decl d;
	Type t;
	int i;

	reset();
	chain = calloc(Depth, sizeof(*chain));
	check(chain != nil);
	memset(&leaf, 0, sizeof(leaf));
	memset(&d, 0, sizeof(d));
	memset(&t, 0, sizeof(t));
	leaf.op = Ofielddecl;
	leaf.decl = &d;
	d.ty = &t;
	t.kind = Tint;
	for(i = 0; i < Depth; i++){
		chain[i].op = sequence ? Oseq : Otypedecl;
		chain[i].decl = &d;
		chain[i].left = i+1 == Depth ? &leaf : &chain[i+1];
		if(sequence) chain[i].right = &leaf;
	}
	gbind(chain);
	check(nprivate == 0 && scope == ScopeGlobal && (t.ok&OKbind));
	for(i = 0; i < Depth; i++)
		check(chain[i].op == (sequence ? Oseq : Otypedecl));
	free(chain);
	scenarios++;
}


/* Compare duplicate diagnostics and pointer preservation while label storage moves. */
static uvlong
labelrun(int seed, int original)
{
	uvlong h;
	int i, count;

	reset();
	count = 1+seed%511;
	for(i = 0; i < count; i++){
		arena[i].src.start.line = i+1;
		arena[i].decl = i%7 ? &declarations[i] : nil;
		declarations[i].sym = &symbols[(seed+i)%8];
		declarations[i].refs = 91;
		if(original) reference_pushlabel(&arena[i]); else pushlabel(&arena[i]);
	}
	check(labdep == count && maxlabdep >= count);
	h = trace^(uvlong)maxlabdep;
	for(i = 0; i < count; i++){
		check(labstack[i] == &arena[i]);
		h = (h^(uvlong)declarations[i].refs)*1099511628211ULL;
	}
	return h^(uvlong)allocations;
}


/* Reject invalid label positions before writes, and observe huge valid growth without allocating it. */
static void
labelbounds(int isolated)
{
	static int capacities[] = { -1, 0, 1, 63, 64, 127, 0x7fffffff-64, 0x7fffffff-63, 0x7fffffff };
	Node n;
	int c, depth, status, valid, first, last;

	memset(&n, 0, sizeof(n));
	for(c = 0; c < nelem(capacities); c++){
		if(isolated == 1 && c != nelem(capacities)-1) continue;
		if(isolated == 2 && capacities[c] != 1) continue;
		first = capacities[c] > 127 ? capacities[c] : -1;
		last = capacities[c] > 127 ? capacities[c] : capacities[c]+1;
		if(isolated == 2) last = -1;
		for(depth = first; depth <= last; depth++){
			reset();
			maxlabdep = capacities[c];
			labdep = depth;
			if(maxlabdep <= 127 && maxlabdep > 0){
				labelowner = calloc(maxlabdep, sizeof(*labstack));
				check(labelowner != nil);
				labelbytes = maxlabdep*sizeof(*labstack);
				labstack = labelowner;
			}
			valid = depth >= 0 && depth <= maxlabdep
				&& (depth < maxlabdep || maxlabdep <= 0x7fffffff-MaxScope);
			trapallocation = maxlabdep > 127;
			expecting = !valid;
			status = setjmp(rejection);
			if(status == 0){
				pushlabel(&n);
				check(valid && labdep == depth+1 && labstack[depth] == &n);
			}else if(status == 1)
				check(!valid && labdep == depth && maxlabdep == capacities[c] && allocations == 0);
			else{
				check(status == 2 && valid && trapallocation);
				check(request == (ulong)(capacities[c]+MaxScope)*sizeof(*labstack));
				check(maxlabdep == capacities[c] && labdep == depth);
			}
			expecting = trapallocation = 0;
			scenarios++;
			if(depth == 0x7fffffff) break;
		}
	}
}


/* Run lexical/dynamic capacity boundaries and independent deep global binding probes. */
int
main(int argc, char **argv)
{
	struct rlimit limit;
	uvlong expected;
	int i;

	check(argc == 2);
	limit.rlim_cur = limit.rlim_max = 512*1024;
	check(setrlimit(RLIMIT_STACK, &limit) == 0);
	if(strcmp(argv[1], "shallow") == 0){
		for(i = 0; i < 3000; i++){
			expected = attempt(i, 1);
			check(attempt(i, 0) == expected);
			scenarios++;
		}
	}else if(strcmp(argv[1], "mutation") == 0){
		for(i = 0; i < 2; i++){
			expected = mutation(i, 1);
			check(mutation(i, 0) == expected);
			scenarios++;
		}
	}else if(strcmp(argv[1], "edges") == 0) edges();
	else if(strcmp(argv[1], "nested") == 0) nestedscopes();
	else if(strcmp(argv[1], "scope") == 0) scopebounds(0);
	else if(strcmp(argv[1], "repush") == 0) scopebounds(1);
	else if(strcmp(argv[1], "sequence") == 0) deep(1);
	else if(strcmp(argv[1], "type") == 0) deep(0);
	else if(strcmp(argv[1], "labels") == 0){
		for(i = 0; i < 1000; i++){
			expected = labelrun(i, 1);
			check(labelrun(i, 0) == expected);
			scenarios++;
		}
	}else if(strcmp(argv[1], "labelbounds") == 0) labelbounds(0);
	else if(strcmp(argv[1], "label-overflow") == 0) labelbounds(1);
	else if(strcmp(argv[1], "label-index") == 0) labelbounds(2);
	else check(0);
	reset();
	printf("scope binding WORD%d %s: %lu scenarios\n", IBY2WD*8, argv[1], scenarios);
	return 0;
}
