#include "limbo.h"
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>

Decl *impmods;
Dlist *impdecls;
Type *tnone;
int scope;
char debug[256];
static int attempts, failat, live, checks, rootmode;
static ulong virtualsize;
static char virtualroot, *installed;
static char *roottext;
static Sym iface;
static Decl ifacedecl;
#define SLASHMOD "/module"
#define INCPATH "/default/module"

/* Check compiler fatal exits without recovering into partially constructed state. */
void
fatal(char *fmt, ...)
{
	assert(fmt != nil);
	_Exit(73);
}

/* Track allocations and inject each failure below the production checked allocator. */
static void*
startupmalloc(ulong n)
{
	void *p;

	if(++attempts == failat)
		return nil;
	assert(n > 0 && n < 1000000);
	p = malloc(n);
	assert(p != nil);
	live++;
	return p;
}

/* Returned include paths and implementation list nodes have independent owners. */
static void
startupfree(void *p)
{
	assert(p != nil && p != &virtualroot && live > 0);
	live--;
	free(p);
}

/* Supply an owned root, an absent environment root, or a synthetic huge length. */
static char*
getroot(void)
{
	char *p;

	if(rootmode == 0)
		return nil;
	if(rootmode == 2)
		return &virtualroot;
	p = allocmem(strlen(roottext)+1);
	strcpy(p, roottext);
	return p;
}

/* Simulate unallocatable root lengths without materializing gigabytes of text. */
static ulong
startuplen(char *s)
{
	return s == &virtualroot ? virtualsize : strlen(s);
}

/* The real addinclude retains its argument rather than copying or freeing it. */
void addinclude(char *s) { assert(installed == nil); installed = s; }

/* Bound module declaration dependencies; actual list construction remains production code. */
void installids(int store, Decl *ids) { assert(store == Dtype || store == Dglobal); (void)ids; }
void pushscope(Node *n, int kind) { assert(n == nil && kind == Sother); }
Decl* popscope(void) { return nil; }
void fielddecled(Node *n) { assert(n == nil); }
Sym* enter(char *s, int token) { assert(strncmp(s, ".m.", 3) == 0 && token == 0); return &iface; }
Decl* mkids(Src *src, Sym *s, Type *t, Decl *next) { (void)src; (void)t; assert(s == &iface && next == nil); return &ifacedecl; }
int isimpmod(Sym *s) { (void)s; return 0; }
int dequal(Decl *a, Decl *b, int mode) { (void)a; (void)b; (void)mode; return 0; }
void redecl(Decl *d) { (void)d; assert(0); }

/* Preserve bounded interface-name formatting for the tiny module fixtures. */
char*
seprint(char *buf, char *end, char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, end-buf, fmt, ap);
	va_end(ap);
	assert(n >= 0 && n < end-buf);
	return buf+n;
}

/* Debug output is disabled, but keep the production symbol linkable. */
int print(char *fmt, ...) { (void)fmt; return 0; }
#define malloc startupmalloc
#define free startupfree
#define strlen startuplen
IMPLEMENTATION
#undef malloc
#undef free
#undef strlen

/* Check successful root joining, including empty roots and existing trailing slashes. */
static void
paths(void)
{
	char *inputs[] = {"", "/", "/inferno", "/inferno/", "C:/Inferno", "/with spaces/тест"};
	char expected[256];
	int i;

	rootmode = 0;
	rootincludes();
	assert(strcmp(installed, INCPATH) == 0 && live == 0);
	installed = nil;
	checks++;
	rootmode = 1;
	for(i = 0; i < nelem(inputs); i++){
		roottext = inputs[i];
		snprintf(expected, sizeof expected, "%s/module", roottext);
		rootincludes();
		assert(strcmp(installed, expected) == 0 && live == 1);
		startupfree(installed);
		installed = nil;
		checks++;
	}
}

/* Build implementation-module matches using actual compiler declaration layouts. */
static void
modules(void)
{
	Sym names[4];
	Decl ids[4], imports[4];
	Node n;
	Type type;
	Dlist *p, *next;
	int count, i;

	for(count = 1; count <= 4; count++){
		memset(names, 0, sizeof names);
		memset(ids, 0, sizeof ids);
		memset(imports, 0, sizeof imports);
		memset(&type, 0, sizeof type);
		memset(&n, 0, sizeof n);
		for(i = 0; i < count; i++){
			names[i].name = "module";
			ids[i].sym = imports[i].sym = &names[i];
			ids[i].ty = &type;
			if(i+1 < count){
				ids[i].next = &ids[i+1];
				imports[i].next = &imports[i+1];
			}
		}
		impmods = imports;
		n.decl = ids;
		n.ty = &type;
		moddecled(&n);
		p = impdecls;
		for(i = 0; i < count; i++){
			assert(p != nil && p->d == &ids[i]);
			next = p->next;
			startupfree(p);
			p = next;
		}
		assert(p == nil && live == 0);
		impdecls = nil;
		checks++;
	}
}

/* Every startup allocation failure must diagnose, including the joined-path allocation. */
static void
failures(void)
{
	pid_t child;
	int i, status;

	for(i = 1; i <= 3; i++){
		child = fork();
		assert(child >= 0);
		if(child == 0){
			attempts = 0;
			failat = i <= 2 ? i : 1;
			if(i <= 2){
				rootmode = 1;
				roottext = "/root";
				rootincludes();
			}else
				modules();
			_Exit(74);
		}
		assert(waitpid(child, &status, 0) == child);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
		checks++;
	}
	child = fork();
	assert(child >= 0);
	if(child == 0){
		rootmode = 2;
		virtualsize = 0x7fffffff;
		rootincludes();
		_Exit(74);
	}
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 73);
	checks++;
}

/* Compare both parser copies through normal and failure paths. */
int
main(int argc, char **argv)
{
	if(argc == 1 || strcmp(argv[1], "paths") == 0)
		paths();
	if(argc == 1 || strcmp(argv[1], "modules") == 0)
		modules();
	if(argc == 1 || strcmp(argv[1], "failure") == 0)
		failures();
	assert(live == 0);
	printf("compiler startup: %d path/module/allocation cases passed\n", checks);
	return 0;
}
