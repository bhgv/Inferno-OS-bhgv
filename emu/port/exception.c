#include "dat.h"
#include "fns.h"
#include "error.h"
#include "interp.h"
#include "isa.h"
#include "runt.h"
#include "kernel.h"
#include "raise.h"


/* Match a literal exception name or a trailing wildcard prefix. */
static int
ematch(char *pat, char *exp)
{
	int l;

	if(strcmp(pat, exp) == 0)
		return 1;

	l = strlen(pat);
	if(l == 0)
		return 0;
	if(pat[l-1] == '*') {
		if(l == 1)
			return 1;
		if(strncmp(pat, exp, l-1) == 0)
			return 1;
	}
	return 0;
}


/* Reuse the emergency ASCII exception buffer without overflowing it. */
static void
setstr(String *s, char *p)
{
	if(s == H)
		return;
	if(s->len < 0 || s->max < 4)
		return;
	kstrcpy(s->Sascii, p, s->max);	/* TO DO: we are assuming they aren't runes */
	s->len = strlen(s->Sascii);
}

static String *exstr;


/* Pin the emergency message buffer before exception handling can allocate. */
void
excinit(void)
{
	exstr = newstring(ERRMAX);
	poolimmutable(D2H(exstr));
}


/* Construct a message, falling back to the pinned emergency buffer. */
static String*
newestring(char *estr)
{
	String *s;

	if(waserror()){
		setstr(exstr, estr);
		D2H(exstr)->ref++;
		return exstr;
	}
	s = c2string(estr, strlen(estr));
	poperror();
	return s;
}

#define NOPC ((ulong)-1) // Match the loader's native-width absent-handler sentinel.


/* Read Dis frame metadata without requiring native C structure alignment. */
static Type*
frametype(uchar *fp)
{
	Type *type;

	memmove(&type, fp+O(Frame, t), sizeof(type));
	if(type == nil)
		type = SEXTYPE((Frame*)fp)->reg.TR;
	return type;
}


/* Validate and discard every prepared frame in this extent's allocated suffix. */
static void
freeframe(uchar *fp, int setsp)
{
	uchar *scan, *end, *last, *previous;
	Frame links;
	Type *type;
	REG resumed;
	int size;

	end = R.SP;
	if((uintptr)fp > (uintptr)end || (uintptr)end > (uintptr)R.TS ||
	   (uintptr)end-(uintptr)fp > 0x7fffffff)
		error(exHeap);
	/* Validate the entire suffix before changing dead frame links or owners. */
	for(scan = fp; scan != end; scan += size){
		if((uintptr)end-(uintptr)scan < sizeof(Frame))
			error(exHeap);
		memmove(&links, scan, sizeof(links));
		if(links.t == nil &&
		   ((uintptr)R.EX > (uintptr)-1-OA(Stkext, reg.tos.fr) ||
		    (uintptr)scan != (uintptr)R.EX+OA(Stkext, reg.tos.fr)))
			error(exHeap);
		type = frametype(scan);
		if(type == nil || type == H ||
		   (uintptr)type > (uintptr)-1-O(Type, size)-sizeof(size))
			error(exHeap);
		memmove(&size, (uchar*)type+O(Type, size), sizeof(size));
		if(size < (int)sizeof(Frame) || (uintptr)size > (uintptr)end-(uintptr)scan ||
		   links.mr != nil)
			error(exHeap);
	}
	/* Only unentered frames are relinked; active caller links stay unchanged. */
	last = nil;
	for(scan = fp; scan != end; scan += size){
		type = frametype(scan);
		memmove(&size, (uchar*)type+O(Type, size), sizeof(size));
		previous = last;
		memmove(scan+O(Frame, fp), &previous, sizeof(previous));
		last = scan;
	}
	while(last != nil){
		memmove(&links, last, sizeof(links));
		type = frametype(last);
		memmove(&resumed, &R, sizeof(resumed));
		/* Lower pending frames remain collector-visible until their own release. */
		resumed.SP = last;
		memmove(&R, &resumed, sizeof(R));
		if(links.t == nil){
			unextend((Frame*)last);
			memmove(&resumed, &R, sizeof(resumed));
		}else if(type->np)
			freeptrs(last, type);
		memmove(&R, &resumed, sizeof(R));
		last = links.fp;
	}
	if(setsp)
		R.SP = fp;
}


/* Check exception storage and cleanup maps before selecting a handler. */
static int
exceptionframe(uchar *fp, Handler *handler)
{
	Type *frame, *cleanup;
	ulong offset, slot, bytes;
	int i, bit;

	frame = frametype(fp);
	if(frame == nil || frame == H || frame->size < (int)sizeof(Frame) ||
	   frame->np < 0 || frame->np > frame->size/(8*sizeof(void*))+
	   (frame->size%(8*sizeof(void*)) != 0))
		return 0;
	offset = handler->eoff;
	if(offset < sizeof(Frame) || offset%sizeof(void*) != 0 ||
	   offset > (ulong)frame->size-sizeof(void*))
		return 0;
	slot = offset/sizeof(void*);
	if(slot/8 >= frame->np || (frame->map[slot/8] & (0x80>>(slot%8))) == 0)
		return 0;

	cleanup = handler->t;
	if(cleanup == nil)
		return 1;
	if(cleanup == H || cleanup->size < 0 || cleanup->size > frame->size ||
	   cleanup->np < 0 || cleanup->np > cleanup->size/(8*sizeof(void*))+
	   (cleanup->size%(8*sizeof(void*)) != 0))
		return 0;
	for(i = 0; i < cleanup->np; i++)
		for(bit = 0; bit < 8; bit++)
			if(cleanup->map[i] & (0x80>>bit)){
				bytes = (i*8+bit)*sizeof(void*);
				if(bytes < sizeof(Frame) || bytes > (ulong)cleanup->size ||
				   sizeof(void*) > (ulong)cleanup->size-bytes)
					return 0;
			}
	return 1;
}

/* Resolve the predecessor PC without subtracting unrelated native pointers.
 * module and nextpc are borrowed. Builtins have no bytecode and return NOPC;
 * interpreted PCs must be aligned, nonzero and within the declared image.
 * Compiled handlers retain their historical byte-offset coordinates. */
static ulong
exceptionpc(Modlink *module, Inst *nextpc)
{
    uintptr base, current, offset;

    if(module == nil || module == H || module->prog == nil || module->prog == H ||
       nextpc == nil || nextpc == H){
        return NOPC;
    }
    base = (uintptr)module->prog;
    current = (uintptr)nextpc;
    if(current <= base){
        return NOPC;
    }
    offset = current-base;
    if(module->compiled){
        if(offset-1 > 0xffffffffUL){
            return NOPC;
        }
        return offset-1;
    }
    if(module->m == nil || module->m == H || module->m->nprog < 0 ||
       offset%sizeof(Inst) != 0 || offset/sizeof(Inst) > (ulong)module->m->nprog){
        return NOPC;
    }
    return offset/sizeof(Inst)-1;
}


/* Validate the selected destination before diagnostics or stack ownership changes.
 * Compiled PCs use byte offsets; their allocation extent is not stored here. */
static int
exceptiontarget(Modlink *module, ulong pc)
{
	uintptr base, offset, limit;

	if(pc == NOPC || module == nil || module == H ||
	   module->prog == nil || module->prog == H)
		return 0;
	base = (uintptr)module->prog;
	limit = (uintptr)-1;
	if(module->compiled){
		if(pc > limit-base)
			return 0;
		offset = pc;
	}else{
		if(module->m == nil || module->m == H || module->m->nprog <= 0 ||
		   pc >= (ulong)module->m->nprog || pc > limit/sizeof(Inst))
			return 0;
		offset = pc*sizeof(Inst);
		if(offset > limit-base || sizeof(Inst) > limit-base-offset)
			return 0;
	}
	return base+offset != (uintptr)H;
}


/* Select usable handlers; release terminal exceptions after clearing their process root.
 * Empty error text still owns exval even though no table is selected. */
int
handler(char *estr)
{
	Prog *p;
	Modlink *m, *mr, *departed;
	REG resumed;
	int str, ne, transfer;
	ulong pc, newpc;
	long eoff;
	uchar *fp, *eadr;
	Frame *f, frame;
	Type *t, *zt;
	Handler *h;
	Except *e;
	void *v, *old;

	p = currun();
	if(p == nil)
		return 0;
	if(*estr == 0)
		goto unhandled;
	str = p->exval == H || D2H(p->exval)->t == &Tstring;
	m = R.M;
	pc = exceptionpc(m, R.PC);
	fp = R.FP;

	while(fp != nil){		/* look for a handler */
		if((h = m->m->htab) != nil){
			for( ; h->etab != nil; h++){
				if(pc < h->pc1 || pc >= h->pc2)
					continue;
				if(!exceptionframe(fp, h))
					continue;
				eoff = h->eoff;
				zt = h->t;
				for(e = h->etab, ne = h->ne; e->s != nil; e++, ne--){
					if(ematch(e->s, estr) && (str && ne <= 0 || !str && ne > 0) &&
					   exceptiontarget(m, e->pc)){
						newpc = e->pc;
						goto found;
					}
				}
				newpc = e->pc;
				if(exceptiontarget(m, newpc))
					goto found;
			}
		}
		if(!str && fp != R.FP){		/* becomes a string exception in immediate caller */
			v = p->exval;
			memmove(&p->exval, v, sizeof(p->exval));
			D2H(p->exval)->ref++;
			memmove(&resumed, &R, sizeof(resumed));
			destroy(v);
			memmove(&R, &resumed, sizeof(R));
			str = 1;
			continue;
		}
		memmove(&frame, fp, sizeof(frame));
		/* The root has no caller PC to reconstruct. */
		if(frame.fp == nil)
			break;
		if(frame.mr != nil)
			m = frame.mr;
		pc = exceptionpc(m, frame.lr);
		fp = frame.fp;
	}
unhandled:
	v = p->exval;
	p->exval = H;
	memmove(&resumed, &R, sizeof(resumed));
	destroy(v);
	memmove(&R, &resumed, sizeof(R));
	return 0;
found:
	{
		int n;
		char name[3*KNAMELEN], *text;

		pc = modstatus(&R, name, sizeof(name));
		n = 10+1+strlen(name)+1+strlen(estr)+1;
		text = realloc(p->exstr, n);
		if(text != nil){
			p->exstr = text;
			snprint(p->exstr, n, "%lud %s %s", pc, name, estr);
		}
	}

	m = R.M;
	for(;;){
		/* Argument evaluation can leave several extents or gaps below helpers. */
		while((uintptr)R.FP < (uintptr)R.EX || (uintptr)R.FP >= (uintptr)R.TS)
			freeframe(R.EX+OA(Stkext, reg.tos.fr), 0);
		t = frametype(R.FP);
		if((uintptr)t->size < (uintptr)R.SP-(uintptr)R.FP)
			freeframe(R.FP+t->size, 1);
		if(R.FP == fp)
			break;
		f = (Frame*)R.FP;
		memmove(&frame, R.FP, sizeof(frame));
		R.PC = frame.lr;
		R.FP = frame.fp;
		R.SP = (uchar*)f;
		mr = frame.mr;
		memmove(&resumed, &R, sizeof(resumed));
		if(frame.t == nil){
			unextend(f);
			memmove(&resumed, &R, sizeof(resumed));
		}else if(frame.t->np)
			freeptrs(f, frame.t);
		memmove(&R, &resumed, sizeof(R));
		if(mr != nil){
			departed = R.M;
			resumed.M = mr;
			resumed.MP = mr->MP;
			memmove(&R, &resumed, sizeof(R));
			destroy(departed);
			memmove(&R, &resumed, sizeof(R));
			m = mr;
		}
	}
	/* Publish the surviving caller before cleanup or message allocation can scan roots. */
	memmove(&p->R, &R, sizeof(R));
	if(zt != nil){
		memmove(&resumed, &R, sizeof(resumed));
		freeptrs(fp, zt);
		memmove(&R, &resumed, sizeof(R));
		initmem(zt, fp);
		memmove(&R, &resumed, sizeof(R));
	}
	eadr = fp+eoff;
	memmove(&old, eadr, sizeof(old));
	v = H;
	memmove(eadr, &v, sizeof(v));
	memmove(&resumed, &R, sizeof(resumed));
	destroy(old);
	memmove(&R, &resumed, sizeof(R));
	transfer = p->exval != H;
	if(!transfer){
		v = newestring(estr);	/* might fail; the result slot already owns nil */
		memmove(&R, &resumed, sizeof(R));
	}else
		v = p->exval;
	/* Publish the held reference before removing its process root. */
	memmove(eadr, &v, sizeof(v));
	if(transfer)
		p->exval = H;
	if(m->compiled)
		R.PC = (Inst*)((ulong)m->prog+newpc);
	else
		R.PC = m->prog+newpc;
	memmove(&p->R, &R, sizeof(R));
	p->kill = nil;
	v = p->exval;
	p->exval = H;
	memmove(&resumed, &R, sizeof(resumed));
	destroy(v);
	memmove(&R, &resumed, sizeof(R));
	return 1;
}
