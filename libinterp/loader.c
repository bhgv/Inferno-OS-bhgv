#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "loadermod.h"
#include "raise.h"
#include <kernel.h>

static uchar	Instmap[] = Loader_Inst_map;
static Type*	Tinst;
static uchar	Tdescmap[] = Loader_Typedesc_map;
static Type*	Tdesc;
static uchar	Tlinkmap[] = Loader_Link_map;
static Type*	Tlink;

void
loadermodinit(void)
{
	sysinit();
	builtinmod("$Loader", Loadermodtab, Loadermodlen);
	Tinst = dtype(freeheap, sizeof(Loader_Inst), Instmap, sizeof(Instmap));
	Tdesc = dtype(freeheap, sizeof(Loader_Typedesc), Tdescmap, sizeof(Tdescmap));
	Tlink = dtype(freeheap, sizeof(Loader_Link), Tlinkmap, sizeof(Tlinkmap));
}

static void
brunpatch(Loader_Inst *ip, Module *m)
{
	switch(ip->op) {
	case ICALL:
	case IJMP:
	case IBEQW:
	case IBNEW:
	case IBLTW:
	case IBLEW:
	case IBGTW:
	case IBGEW:
	case IBEQB:
	case IBNEB:
	case IBLTB:
	case IBLEB:
	case IBGTB:
	case IBGEB:
	case IBEQF:
	case IBNEF:
	case IBLTF:
	case IBLEF:
	case IBGTF:
	case IBGEF:
	case IBEQC:
	case IBNEC:
	case IBLTC:
	case IBLEC:
	case IBGTC:
	case IBGEC:
	case IBEQL:
	case IBNEL:
	case IBLTL:
	case IBLEL:
	case IBGTL:
	case IBGEL:
	case ISPAWN:
		ip->dst = (Inst*)ip->dst - m->prog;
		break;
	}
}

void
Loader_ifetch(void *a)
{
	Heap *h;
	Array *ar;
	Module *m;
	Inst *i, *ie;
	Loader_Inst *li;
	F_Loader_ifetch *f;

	f = a;
	destroy(*f->ret);
	*f->ret = H;

	if(f->mp == H)
		return;
	m = f->mp->m;
	if(m == H)
		return;
	if(m->compiled) {
		kwerrstr("compiled module");
		return;
	}

	h = nheap(sizeof(Array)+m->nprog*sizeof(Loader_Inst));
	h->t = &Tarray;
	h->t->ref++;
	ar = H2D(Array*, h);
	ar->t = Tinst;
	Tinst->ref++;
	ar->len = m->nprog;
	ar->root = H;
	ar->data = (uchar*)ar+sizeof(Array);

	li = (Loader_Inst*)ar->data;
	i = m->prog;
	ie = i + m->nprog;
	while(i < ie) {
		li->op = i->op;
		li->addr = i->add;
		li->src = i->s.imm;
		li->dst = i->d.imm;
		li->mid = i->reg;
		if(UDST(i->add) == AIMM)
			brunpatch(li, m);
		li++;
		i++;
	}

	*f->ret = ar;
}

void
Loader_link(void *a)
{
	Link *p;
	Heap *h;
	Type **t;
	int nlink;
	Module *m;
	Array *ar;
	Loader_Link *ll;
	F_Loader_link *f;
	
	f = a;
	destroy(*f->ret);
	*f->ret = H;

	if(f->mp == H)
		return;
	m = f->mp->m;
	if(m == H)
		return;

	nlink = 0;
	for(p = m->ext; p->name; p++)
		nlink++;

	h = nheap(sizeof(Array)+nlink*sizeof(Loader_Link));
	h->t = &Tarray;
	h->t->ref++;
	ar = H2D(Array*, h);
	ar->t = Tlink;
	Tlink->ref++;
	ar->len = nlink;
	ar->root = H;
	ar->data = (uchar*)ar+sizeof(Array);

	ll = (Loader_Link*)ar->data + nlink;
	for(p = m->ext; p->name; p++) {
		ll--;
		ll->name = c2string(p->name, strlen(p->name));
		ll->sig = p->sig;
		if(m->prog == nil) {
			ll->pc = -1;
			ll->tdesc = -1;
		} else {
			ll->pc = p->u.pc - m->prog;
			ll->tdesc = 0;
			for(t = m->type; *t != p->frame; t++)
				ll->tdesc++;
		}
	}

	*f->ret = ar;
}

void
Loader_tdesc(void *a)
{
	int i;
	Heap *h;
	Type *t;
	Array *ar;
	Module *m;
	F_Loader_tdesc *f;
	Loader_Typedesc *lt;

	f = a;
	destroy(*f->ret);
	*f->ret = H;

	if(f->mp == H)
		return;
	m = f->mp->m;
	if(m == H)
		return;

	h = nheap(sizeof(Array)+m->ntype*sizeof(Loader_Typedesc));
	h->t = &Tarray;
	h->t->ref++;
	ar = H2D(Array*, h);
	ar->t = Tdesc;
	Tdesc->ref++;
	ar->len = m->ntype;
	ar->root = H;
	ar->data = (uchar*)ar+sizeof(Array);

	lt = (Loader_Typedesc*)ar->data;
	for(i = 0; i < m->ntype; i++) {
		t = m->type[i];
		lt->size = t->size;
		lt->map = H;
		if(t->np != 0)
			lt->map = mem2array(t->map, t->np);
		lt++;
	}

	*f->ret = ar;
}

typedef struct LoaderModule LoaderModule;
struct LoaderModule {
	F_Loader_newmod *f;
	Module *m;
	Modlink *ml;
};

static int
loadernewmod(void *arg)
{
	LoaderModule *c;
	F_Loader_newmod *f;
	Module *m;
	Heap *h;
	Inst *i;
	Loader_Inst *li;
	int n;

	c=arg; f=c->f;
	c->m=m=mallocz(sizeof(Module),1);
	if(m == nil) {
		kwerrstr(exNomem);
		return 0;
	}
	m->origmp=H;
	m->ref=1;
	m->ss=f->ss;
	m->name=strdup(string2c(f->name));
	if(m->name == nil) {
		kwerrstr(exNomem);
		return 0;
	}
	m->path=strdup(m->name);
	m->type=mallocz(sizeof(Type*),1);
	m->ext=mallocz(sizeof(Link),1);
	if(m->path == nil || m->type == nil || m->ext == nil) {
		kwerrstr(exNomem);
		return 0;
	}
	m->ntype=1;
	m->origmp=(uchar*)f->data;
	h=D2H(f->data);
	h->ref++;
	Setmark(h);
	m->type[0]=h->t;
	h->t->ref++;
	m->nprog=f->inst->len;
	m->prog=mallocz(m->nprog*sizeof(Inst),1);
	if(m->prog == nil) {
		kwerrstr(exNomem);
		return 0;
	}
	li=(Loader_Inst*)f->inst->data;
	for(n=0; n<m->nprog; n++,li++) {
		if(li->op >= MAXDIS || USRC(li->addr) > (AFP|AIND) ||
	     UDST(li->addr) > (AFP|AIND) ||
	     ((li->addr&ARM) != AXNON && (li->mid < -32768 || li->mid > 65535)) ||
	     (((li->addr&ARM) == AXINF || (li->addr&ARM) == AXINM) && li->mid < 0)) {
			kwerrstr("bad instruction");
			return 0;
		}
		i=&m->prog[n];
		i->op=li->op; i->add=li->addr; i->reg=li->mid;
		i->s.imm=li->src; i->d.imm=li->dst;
		if(UDST(i->add) == AIMM && !brpatch(i,m)) {
			kwerrstr("bad branch addr");
			return 0;
		}
	}
	m->entry=m->prog;
	c->ml=mklinkmod(m,f->nlink);
	c->ml->MP=m->origmp;
	m->origmp=H;
	return 1;
}

void
Loader_newmod(void *a)
{
	F_Loader_newmod *f;
	LoaderModule c;

	f=a;
	destroy(*f->ret);
	*f->ret=H;
	if(f->inst == H || f->data == H) {
		kwerrstr("nil parameters");
		return;
	}
	if(f->nlink < 0 || (uvlong)f->nlink >
	   (0x7fffffff-sizeof(Heap)-(sizeof(Modlink)-sizeof(Modl)))/sizeof(Modl) ||
	   f->inst->len <= 0 || (uvlong)f->inst->len > 0x7fffffff/sizeof(Inst) ||
	   f->inst->t == nil || f->inst->t->size != sizeof(Loader_Inst) ||
	   D2H(f->data)->t == nil) {
		kwerrstr("bad module parameters");
		return;
	}
	c.f=f; c.m=nil; c.ml=H;
	if(kprotect(loadernewmod,&c)) {
		*f->ret=c.ml;
		return;
	}
	if(c.m != nil) {
		if(c.m->ext != nil)
			destroylinks(c.m);
		destroy(c.m->origmp);
		freemod(c.m);
	}
}

void
Loader_tnew(void *a)
{
	int mem;
	Module *m;
	Type *t, **nt;
	Array *ar, az;
	F_Loader_tnew *f;

	f = a;
	*f->ret = -1;
	if(f->mp == H)
		return;
	m = f->mp->m;
	if(m == H)
		return;
	if(m->origmp != H){
		kwerrstr("need newmod");
		return;
	}

	ar = f->map;
	if(ar == H) {
		ar = &az;
		ar->len = 0;
		ar->data = nil;
	}

	if(f->size < 0 || (uvlong)f->size > 0x7fffffff ||
	   ar->len < 0 || (uvlong)ar->len > 0x7fffffff) {
		kwerrstr("bad type");
		return;
	}
	t = dtype(freeheap, f->size, ar->data, ar->len);
	if(t == nil)
		return;

	if(m->ntype < 0 || m->ntype >= 0x7fffffff/sizeof(Type*)-1) {
		freetype(t);
		kwerrstr("too many types");
		return;
	}
	mem = (m->ntype+1)*sizeof(Type*);
	if(msize(m->type) > mem) {
		*f->ret = m->ntype;
		m->type[m->ntype++] = t;
		return;
	}
	nt = realloc(m->type, mem);
	if(nt == nil) {
		freetype(t);
		kwerrstr(exNomem);
		return;
	}
	m->type = nt;
	f->mp->type = nt;
	*f->ret = m->ntype;
	m->type[m->ntype++] = t;
}

void
Loader_ext(void *a)
{
	Modl *l;
	Module *m;
	Modlink *ml;
	F_Loader_ext *f;

	f = a;
	*f->ret = -1;
	if(f->mp == H) {
		kwerrstr("nil mp");
		return;
	}
	ml = f->mp;
	m = ml->m;
	if(m->compiled) {
		kwerrstr("compiled module");
		return;
	}
	if(f->tdesc < 0 || f->tdesc >= m->ntype) {
		kwerrstr("bad tdesc");
		return;
	}
	if(f->pc < 0 || f->pc >= m->nprog) {
		kwerrstr("bad pc");
		return;
	}
	if(f->idx < 0 || f->idx >= ml->nlinks) {
		kwerrstr("bad idx");
		return;
	}
	l = &ml->links[f->idx];
	l->u.pc = m->prog + f->pc;
	l->frame = m->type[f->tdesc];
	*f->ret = 0;
}

typedef struct LoaderHeap LoaderHeap;
struct LoaderHeap {
	Type *t;
	Heap *h;
};

static int
loaderheap(void *arg)
{
	LoaderHeap *c;
	c=arg;
	c->h=heapz(c->t);
	return c->h != nil;
}

void
Loader_dnew(void *a)
{
	F_Loader_dnew *f;
	Array *ar, az;
	LoaderHeap c;
	int ok;

	f=a;
	destroy(*f->ret);
	*f->ret=H;
	ar=f->map;
	if(ar == H) {
		ar=&az;
		ar->len=0;
		ar->data=nil;
	}
	if(f->size < 0 || (uvlong)f->size > 0x7fffffff ||
	   ar->len < 0 || (uvlong)ar->len > 0x7fffffff) {
		kwerrstr("bad type");
		return;
	}
	c.t=dtype(freeheap, f->size, ar->data, ar->len);
	if(c.t == nil) {
		kwerrstr("bad type or no memory");
		return;
	}
	c.h=nil;
	ok=kprotect(loaderheap, &c);
	/* heapz takes its own reference; release the creator's on both paths. */
	freetype(c.t);
	if(ok)
		*f->ret=H2D(Loader_Niladt*, c.h);
}

static int
loadercompile(void *a)
{
	F_Loader_compile *f;
	f=a;
	return compile(f->mp->m, f->mp->m->nprog, f->mp);
}

void
Loader_compile(void *a)
{
	Module *m;
	F_Loader_compile *f;
	uchar *savedmp;
	int ok;

	f = a;
	*f->ret = -1;
	if(f->mp == H) {
		kwerrstr("nil mp");
		return;
	}
	m = f->mp->m;
	if(m->compiled) {
		kwerrstr("compiled module");
		return;
	}
	*f->ret=0;
	if(cflag || f->flag) {
		savedmp=m->origmp;
		m->origmp=f->mp->MP;
		ok=kprotect(loadercompile, f);
		m->origmp=savedmp;
		if(ok) {
			f->mp->prog=m->prog;
			f->mp->compiled=m->compiled;
		} else
			*f->ret=-1;
	}
}
