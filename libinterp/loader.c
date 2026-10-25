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

/* Recover a Dis PC from the native pointer before narrowing to a Limbo int. */
static void
brunpatch(Loader_Inst *ip, Inst *inst, Module *m)
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
		ip->dst = inst->d.ins - m->prog;
		break;
	}
}

/* Clear a borrowed pointer result slot before destroying its former owner.
 * The slot may be unaligned; nil and H are rejected before any allocation.
 * No ownership escapes: callers publish a completed snapshot with a byte copy. */
static void
loaderclear(void *slot)
{
    void *old; /* Former owned snapshot pointer, copied before publication. */
    void *empty = H; /* Empty owner published before invoking destruction. */

    if(slot == nil || slot == H) {
        error(exNilref);
    }
    memmove(&old, slot, sizeof(old));
    memmove(slot, &empty, sizeof(empty));
    destroy(old);
}

enum {
	SnapshotInst,
	SnapshotLink,
	SnapshotType,
};

typedef struct LoaderSnapshot LoaderSnapshot;
struct LoaderSnapshot {
	Module *m;
	Array *array;
	Type *type;
	int count;
	int kind;
};

/* Keep partial pointer-bearing arrays valid for destruction after any error. */
static int
loadersnapshot(void *arg)
{
	LoaderSnapshot *c;
	Module *m;
	Array *ar;
	Loader_Inst *li;
	Loader_Link *ll;
	Loader_Typedesc *lt;
	Inst *ip;
	Link *link;
	Type *t;
	int i, j;

	c = arg;
	m = c->m;
	ar = H2D(Array*, heaparray(c->type, c->count));
	c->array = ar;
	switch(c->kind){
	case SnapshotInst:
		li = (Loader_Inst*)ar->data;
		for(i = 0; i < c->count; i++, li++){
			ip = &m->prog[i];
			li->op = ip->op;
			li->addr = ip->add;
			li->src = ip->s.imm;
			li->dst = ip->d.imm;
			li->mid = ip->reg;
			if(UDST(ip->add) == AIMM)
				brunpatch(li, ip, m);
		}
		break;
	case SnapshotLink:
		ll = (Loader_Link*)ar->data + c->count;
		for(i = 0; i < c->count; i++){
			link = &m->ext[i];
			ll--;
			ll->name = c2string(link->name, strlen(link->name));
			ll->sig = link->sig;
			if(m->prog == nil || link->u.pc == nil){
				ll->pc = -1;
				ll->tdesc = -1;
			}else{
				ll->pc = link->u.pc - m->prog;
				if(link->frame == nil){
					ll->tdesc = -1;
					continue;
				}
				for(j = 0; j < m->ntype; j++)
					if(m->type[j] == link->frame)
						break;
				if(j == m->ntype){
					kwerrstr("bad export frame");
					return 0;
				}
				ll->tdesc = j;
			}
		}
		break;
	case SnapshotType:
		lt = (Loader_Typedesc*)ar->data;
		for(i = 0; i < c->count; i++, lt++){
			t = m->type[i];
			lt->size = t->size;
			if(t->np != 0)
				lt->map = mem2array(t->map, t->np);
		}
		break;
	}
	return 1;
}

/* Own the result outside the VM error frame until every element is complete. */
static Array*
loadergetsnapshot(Module *m, Type *type, int count, int kind)
{
	LoaderSnapshot c;

	c.m = m;
	c.array = H;
	c.type = type;
	c.count = count;
	c.kind = kind;
	if(kprotect(loadersnapshot, &c))
		return c.array;
	destroy(c.array);
	return H;
}

/* Return checked instruction storage, preserving Dis branch indices. */
void
Loader_ifetch(void *a)
{
	Module *m;
	Array *snapshot;
	F_Loader_ifetch *f;

	f = a;
	loaderclear(f->ret);
	if(f->mp == H || f->mp == nil || f->mp->m == H || f->mp->m == nil)
		return;
	m = f->mp->m;
	if(m->compiled){
		kwerrstr("compiled module");
		return;
	}
	snapshot = loadergetsnapshot(m, Tinst, m->nprog, SnapshotInst);
	memmove(f->ret, &snapshot, sizeof(snapshot));
}

/* Native-PC modules cannot expose their export addresses as Dis indices. */
void
Loader_link(void *a)
{
	Module *m;
	Array *snapshot;
	int nlink;
	F_Loader_link *f;

	f = a;
	loaderclear(f->ret);
	if(f->mp == H || f->mp == nil || f->mp->m == H || f->mp->m == nil)
		return;
	m = f->mp->m;
	if(m->compiled){
		kwerrstr("compiled module");
		return;
	}
	nlink = 0;
	if(m->ext != nil) {
		int maxlink;

		maxlink = (0x7fffffff-sizeof(Heap)-sizeof(Array))/sizeof(Loader_Link);
		/* Check the limit before reading the next sentinel entry. */
		while(nlink < maxlink && m->ext[nlink].name != nil)
			nlink++;
		if(nlink == maxlink) {
			kwerrstr("too many exports");
			return;
		}
	}
	snapshot = loadergetsnapshot(m, Tlink, nlink, SnapshotLink);
	memmove(f->ret, &snapshot, sizeof(snapshot));
}

/* Build descriptor maps in an initialized, checked and error-owned array. */
void
Loader_tdesc(void *a)
{
	Module *m;
	Array *snapshot;
	F_Loader_tdesc *f;

	f = a;
	loaderclear(f->ret);
	if(f->mp == H || f->mp == nil || f->mp->m == H || f->mp->m == nil)
		return;
	m = f->mp->m;
	snapshot = loadergetsnapshot(m, Tdesc, m->ntype, SnapshotType);
	memmove(f->ret, &snapshot, sizeof(snapshot));
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
	if(f->inst == H || f->inst == nil || f->data == H || f->data == nil ||
	   f->name == H || f->name == nil) {
		kwerrstr("nil parameters");
		return;
	}
	if(f->nlink < 0 || (uvlong)f->nlink >
	   (0x7fffffff-sizeof(Heap)-(sizeof(Modlink)-sizeof(Modl)))/sizeof(Modl) ||
	   f->inst->len <= 0 || (uvlong)f->inst->len > 0x7fffffff/sizeof(Inst) ||
	   f->inst->data == nil || f->inst->t == nil ||
	   f->inst->t == H || f->inst->t->size != sizeof(Loader_Inst) ||
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
	if(f->mp == H || f->mp == nil)
		return;
	m = f->mp->m;
	if(m == H || m == nil)
		return;
	if(m->origmp != H){
		kwerrstr("need newmod");
		return;
	}

	ar = f->map;
	if(ar == nil) {
		kwerrstr("nil type map");
		return;
	}
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
	if(f->mp == H || f->mp == nil) {
		kwerrstr("nil mp");
		return;
	}
	ml = f->mp;
	m = ml->m;
	if(m == H || m == nil) {
		kwerrstr("nil module");
		return;
	}
	if(m->compiled) {
		kwerrstr("compiled module");
		return;
	}
	if(f->tdesc < 0 || f->tdesc >= m->ntype ||
	   m->type == nil || m->type[f->tdesc] == nil || m->type[f->tdesc] == H) {
		kwerrstr("bad tdesc");
		return;
	}
	if(f->pc < 0 || f->pc >= m->nprog ||
	   m->prog == nil) {
		kwerrstr("bad pc");
		return;
	}
	if(f->idx < 0 || f->idx >= ml->nlinks ||
	   ml->links == nil) {
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
	if(ar == nil) {
		kwerrstr("nil type map");
		return;
	}
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
	if(f->mp == H || f->mp == nil) {
		kwerrstr("nil mp");
		return;
	}
	m = f->mp->m;
	if(m == H || m == nil) {
		kwerrstr("nil module");
		return;
	}
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
