#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <pool.h>

#define T(r)	*((void**)(R.r))

/* Bound the complete extent before narrowing its size or growing a frame. */
static int
stacksize(Type *t, ulong hint, int grow)
{
	ulong header, need;

	header = OA(Stkext, reg.tos.fu);
	if(t == nil || t->size < sizeof(Frame) ||
	   (ulong)t->size > 0x7fffffff-header || hint > 0x7fffffff)
		error(exHeap);
	need = header+t->size;
	if(hint < need){
		if(grow){
			if((ulong)t->size > (0x7fffffff-header)/2)
				error(exHeap);
			need = header+2*(ulong)t->size;
		}
		hint = need;
	}
	return hint;
}

/* Copy a complete source frame only after the new extent has been allocated. */
void
newstack(Prog *p)
{
	int l;
	Type *t;
	Frame *f;
	Stkext *ns;

	f = T(s);

	t = f->t;
	if(t == nil)
		t = SEXTYPE(f)->reg.TR;

	l = stacksize(t, p->R.M->m->ss, 0);
	ns = mallocz(l, 0);
	if(ns == nil)
		error(exNomem);

	ns->reg.TR = t;
	ns->reg.SP = nil;
	ns->reg.TS = nil;
	ns->reg.EX = nil;
	p->R.EX = ns->stack;
	p->R.TS = ns->stack + l;
	p->R.SP = ns->reg.tos.fu + t->size;
	p->R.FP = ns->reg.tos.fu;

	memmove(p->R.FP, f, t->size);
	f = (Frame*)p->R.FP;
	f->lr = nil;
	f->mr = nil;
	f->fp = nil;
	f->t = nil;
}

/* Allocate enough space for the native extent header and requested frame. */
void
extend(void)
{
	int l;
	Type *t;
	Frame *f;
	Stkext *ns;

	t = R.s;
	l = stacksize(t, R.M->m->ss, 1);
	ns = mallocz(l, 0);
	if(ns == nil)
		error(exNomem);

	ns->reg.TR = t;
	ns->reg.SP = R.SP;
	ns->reg.TS = R.TS;
	ns->reg.EX = R.EX;
	f = ns->reg.tos.fr;
	f->t  = nil;
	f->mr = nil;
	R.s = f;
	R.EX = ns->stack;
	R.TS = ns->stack + l;
	R.SP = ns->reg.tos.fu + t->size;

	if (t->np)
		initmem(t, f);
}

void
unextend(Frame *f)
{
	Stkext *sx;
	Type *t;

	sx = SEXTYPE(f);
	R.SP = sx->reg.SP;
	R.TS = sx->reg.TS;
	R.EX = sx->reg.EX;
	t = sx->reg.TR;
	if (t->np)
		freeptrs(f, t);
	free(sx);
}

void
unframe(void)
{
	Type *t;
	Frame *f;
	Stkext *sx;

	f = (Frame*)R.FP;
	t = f->t;
	if(t == nil)
		t = SEXTYPE(f)->reg.TR;

	R.SP = R.FP+t->size;

	f = T(s);
	if(f->t == nil) {
		sx = SEXTYPE(f);
		R.TS = sx->reg.TS;
		R.EX = sx->reg.EX;
		free(sx);
	}
}
