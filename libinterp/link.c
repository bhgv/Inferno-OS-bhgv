#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <kernel.h>

static void
newlink(Link *l, char *fn, int sig, Type *t)
{
	l->name = malloc(strlen(fn)+1);
	if(l->name == nil)
		error(exNomem);
	strcpy(l->name, fn);
	l->sig = sig;
	l->frame = t;
}

void
runtime(Module *m, Link *l, char *fn, int sig, void (*runt)(void*), Type *t)
{
	USED(m);
	newlink(l, fn, sig, t);
	l->u.runt = runt;
}

void
mlink(Module *m, Link* l, uchar *fn, int sig, int pc, Type *t)
{
	newlink(l, (char*)fn, sig, t);
	l->u.pc = pc < 0 ? nil : m->prog+pc;
}

/* Resolve exact exports and report names as text rather than error format strings. */
static int
linkm(Module *m, Modlink *ml, int i, Import *ldt)
{
	Link *l;
	int sig;
	char e[ERRMAX];

	sig = ldt->sig;
	for(l = m->ext; l != nil && l->name != nil; l++)
		if(strcmp(ldt->name, l->name) == 0)
			break;

	if(l == nil || l->name == nil) {
		snprint(e, sizeof(e), "link failed fn %s->%s() not implemented", m->name, ldt->name);
		goto bad;
	}
	if(l->sig != sig) {
		snprint(e, sizeof(e), "link typecheck %s->%s() %ux/%ux",
							m->name, ldt->name, l->sig, sig);
		goto bad;
	}

	ml->links[i].u = l->u;
	ml->links[i].frame = l->frame;
	return 0;
bad:
	kwerrstr("%s", e);
	print("%s\n", e);
	return -1;
}


/* Validate link storage and descriptor retention before allocating a module owner. */
Modlink*
mklinkmod(Module *m, int n)
{
	Heap *h;
	Modlink *ml;

	if(Tmodlink.ref <= 0 || Tmodlink.ref == 0x7fffffff || n < 0 || (ulong)n >
	   (0x7fffffff-sizeof(Heap)-(sizeof(Modlink)-sizeof(ml->links[0])))/sizeof(ml->links[0]))
		error(exHeap);
	h = nheap(sizeof(Modlink)-sizeof(ml->links[0])+n*sizeof(ml->links[0]));
	h->t = &Tmodlink;
	Tmodlink.ref++;
	ml = H2D(Modlink*, h);
	ml->nlinks = n;
	ml->m = m;
	ml->prog = m->prog;
	ml->type = m->type;
	ml->compiled = m->compiled;
	ml->MP = H;
	ml->data = nil;

	return ml;
}

typedef struct LinkModule LinkModule;
struct LinkModule {
	Module *m;
	Import *imports;
	Modlink *result;
	int mkmp;
	int linkfailed;
};

/* Keep the instance reachable by its owner throughout fallible MP cloning. */
static int
linkmodule(void *arg)
{
	LinkModule *c;
	Module *m;
	Import *l;
	Type *t;
	Heap *h;
	Modlink *ml;
	int i;

	c = arg;
	m = c->m;
	for(i = 0, l = c->imports; l->name != nil; i++, l++)
		if(i >= (0x7fffffff-sizeof(Heap)-(sizeof(Modlink)-sizeof(Modl)))/sizeof(Modl))
			error(exHeap);
	c->result = ml = mklinkmod(m, i);
	if(c->mkmp){
		if(m->rt == DYNMOD)
			newdyndata(ml);
		else if(m->origmp != H && m->ntype > 0){
			t = m->type[0];
			h = nheap(t->size);
			h->t = t;
			t->ref++;
			ml->MP = H2D(uchar*, h);
			newmp(ml->MP, m->origmp, t);
		}
	}
	for(i = 0, l = c->imports; l->name != nil; i++, l++){
		if(linkm(m, ml, i, l) < 0){
			c->linkfailed = 1;
			return 0;
		}
	}
	return 1;
}

/* Consume the module reference on all paths, including a failed first allocation. */
Modlink*
linkmod(Module *m, Import *ldt, int mkmp)
{
	LinkModule c;

	if(m == nil || m == H)
		return H;
	if(ldt == nil || ldt == H){
		kwerrstr("nil import table");
		unload(m);
		return H;
	}
	c.m = m;
	c.imports = ldt;
	c.result = H;
	c.mkmp = mkmp;
	c.linkfailed = 0;
	if(kprotect(linkmodule, &c))
		return c.result;
	if(c.result != H)
		destroy(c.result);
	else
		unload(m);
	if(!c.linkfailed)
		krethrow();
	return H;
}

/* Release partial or complete export tables, leaving repeated cleanup harmless. */
void
destroylinks(Module *m)
{
	Link *l;

	for(l = m->ext; l != nil && l->name != nil; l++)
		free(l->name);
	free(m->ext);
	m->ext = nil;
}
