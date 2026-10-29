#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <pool.h>


/* Bound the complete extent before narrowing its size or growing a frame. */
static int
stacksize(Type *t, ulong hint, int grow)
{
	ulong header, need, slot;
	uintptr address;
	int i, bit, size, pointers;

	header = OA(Stkext, reg.tos.fu);
	address = (uintptr)t;
	if(t == nil || t == H || address%sizeof(void*) != 0 ||
	   address > (uintptr)-1-(O(Type, np)+sizeof(t->np)))
		error(exHeap);
	size = t->size;
	pointers = t->np;
	if(size < (int)sizeof(Frame) || pointers < 0 ||
	   (ulong)size > 0x7fffffff-header || hint > 0x7fffffff ||
	   pointers > size/(8*sizeof(void*))+(size%(8*sizeof(void*)) != 0))
		error(exHeap);
	if(pointers != 0 && (address > (uintptr)-1-O(Type, map) ||
	   address+O(Type, map) > (uintptr)-1-(uintptr)pointers))
		error(exHeap);
	for(i = 0; i < pointers; i++)
		for(bit = 0; bit < 8; bit++){
			if(!(t->map[i] & (0x80>>bit)))
				continue;
			slot = (ulong)i*8+bit;
			if(slot < sizeof(Frame)/sizeof(void*) || slot >= (ulong)size/sizeof(void*))
				error(exHeap);
		}
	need = header+size;
	if(hint < need){
		if(grow){
			if((ulong)size > (0x7fffffff-header)/2)
				error(exHeap);
			need = header+2*(ulong)size;
		}
		hint = need;
	}
	return hint;
}


/* Recover only a naturally aligned root extent without backward pointer underflow. */
static Stkext*
stackextent(Frame *f)
{
	Type *declared;
	uintptr address, header;

	address = (uintptr)f;
	header = OA(Stkext, reg.tos.fu);
	if(f == nil || f == H)
		error(exNilref);
	if(address > (uintptr)-1-sizeof(*f) || address < header)
		error(exHeap);
	memmove(&declared, (uchar*)f+O(Frame, t), sizeof(declared));
	if(declared != nil || (address-header)%sizeof(void*) != 0)
		error(exHeap);
	return (Stkext*)(address-header);
}


/* Read ordinary or root frame metadata without assuming input Frame alignment. */
static Type*
stackframe(Frame *f, Type **declared)
{
	Type *t;
	Stkext *sx;

	if(f == nil || f == H)
		error(exNilref);
	if((uintptr)f > (uintptr)-1-sizeof(*f))
		error(exHeap);
	memmove(&t, (uchar*)f+O(Frame, t), sizeof(t));
	if(declared != nil)
		*declared = t;
	if(t == nil){
		sx = stackextent(f);
		t = sx->reg.TR;
	}
	return t;
}


/* Validate both runtime owners before reading a module's stack-size hint. */
static ulong
stackhint(Modlink *link)
{
	if(link == nil || link == H || (uintptr)link%sizeof(void*) != 0 ||
	   (uintptr)link > (uintptr)-1-sizeof(*link) ||
	   link->m == nil || link->m == H || (uintptr)link->m%sizeof(void*) != 0 ||
	   (uintptr)link->m > (uintptr)-1-sizeof(*link->m))
		error(exModule);
	return link->m->ss;
}


/* Copy a complete source frame only after the new extent has been allocated. */
void
newstack(Prog *p)
{
	int l, size;
	Type *t;
	Frame *f;
	Stkext *ns;

	if(p == nil || p == H || (uintptr)p%sizeof(void*) != 0 ||
	   (uintptr)p > (uintptr)-1-sizeof(*p))
		error(exHeap);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(f))
		error(exBounds);
	memmove(&f, R.s, sizeof(f));
	t = stackframe(f, nil);
	l = stacksize(t, stackhint(p->R.M), 0);
	size = t->size;
	if((uintptr)f > (uintptr)-1-(uintptr)size)
		error(exHeap);
	ns = mallocz(l, 0);
	if(ns == nil)
		error(exNomem);

	ns->reg.TR = t;
	ns->reg.SP = nil;
	ns->reg.TS = nil;
	ns->reg.EX = nil;
	memmove(ns->reg.tos.fu, f, size);
	f = ns->reg.tos.fr;
	f->lr = nil;
	f->mr = nil;
	f->fp = nil;
	f->t = nil;
	p->R.EX = ns->stack;
	p->R.TS = ns->stack + l;
	p->R.SP = ns->reg.tos.fu + size;
	p->R.FP = ns->reg.tos.fu;
}


/* Allocate enough space for the native extent header and requested frame. */
void
extend(void)
{
	int l, size;
	Type *t;
	Frame *f;
	Stkext *ns;
	uchar *savedsp, *savedts, *savedex;

	t = R.s;
	l = stacksize(t, stackhint(R.M), 1);
	size = t->size;
	savedsp = R.SP;
	savedts = R.TS;
	savedex = R.EX;
	ns = mallocz(l, 0);
	if(ns == nil)
		error(exNomem);

	ns->reg.TR = t;
	ns->reg.SP = savedsp;
	ns->reg.TS = savedts;
	ns->reg.EX = savedex;
	f = ns->reg.tos.fr;
	f->t  = nil;
	f->mr = nil;
	R.s = f;
	R.EX = ns->stack;
	R.TS = ns->stack + l;
	R.SP = ns->reg.tos.fu + size;

	if (t->np)
		initmem(t, f);
}


/* Preserve restored registers across mapped cleanup and extent-release callbacks. */
void
unextend(Frame *f)
{
	Stkext *sx;
	Type *t;
	REG resumed;

	sx = stackextent(f);
	t = sx->reg.TR;
	stacksize(t, 0, 0);
	if((uintptr)f > (uintptr)-1-(uintptr)t->size)
		error(exHeap);
	memmove(&resumed, &R, sizeof(resumed));
	resumed.SP = sx->reg.SP;
	resumed.TS = sx->reg.TS;
	resumed.EX = sx->reg.EX;
	memmove(&R, &resumed, sizeof(R));
	if(t->np)
		freeptrs(f, t);
	free(sx);
	memmove(&R, &resumed, sizeof(R));
}


/* Capture both frame descriptors before changing the stack or freeing an extent. */
void
unframe(void)
{
	Type *t, *source;
	Frame *f;
	Stkext *sx;
	uchar *nextsp;

	f = (Frame*)R.FP;
	t = stackframe(f, nil);
	stacksize(t, 0, 0);
	if((uintptr)R.FP > (uintptr)-1-(uintptr)t->size)
		error(exHeap);
	nextsp = (uchar*)((uintptr)R.FP+(uintptr)t->size);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(f))
		error(exBounds);
	memmove(&f, R.s, sizeof(f));
	t = stackframe(f, &source);
	stacksize(t, 0, 0);
	if((uintptr)f > (uintptr)-1-(uintptr)t->size)
		error(exHeap);
	R.SP = nextsp;
	if(source == nil){
		sx = stackextent(f);
		R.TS = sx->reg.TS;
		R.EX = sx->reg.EX;
		free(sx);
	}
}
