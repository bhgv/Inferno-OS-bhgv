#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <kernel.h>

#define	A(r)	*((Array**)(r)) // Array owner stored in a checked module-data pointer slot.

Module*	modules;
int	dontcompile;


/* Decode signed Dis operands without narrowing an out-of-range unsigned value. */
static int
operand(uchar **p)
{
	int c;
	uchar *cp;

	cp = *p;
	c = cp[0];
	switch(c & 0xC0) {
	case 0x00:
		*p = cp+1;
		return c;
	case 0x40:
		*p = cp+1;
		return c|~0x7F;
	case 0x80:
		*p = cp+2;
		if(c & 0x20)
			c |= ~0x3F;
		else
			c &= 0x3F;
		return c*256+cp[1];
	case 0xC0:
		*p = cp+4;
		if(c & 0x20)
			c |= ~0x3F;
		else
			c &= 0x3F;
		return c*16777216+(cp[1]<<16)+(cp[2]<<8)+cp[3];
	}
	return 0;	
}


/* Read one canonical unsigned 32-bit word independently of native ulong width. */
static ulong
disw(uchar **p)
{
	ulong v;
	uchar *c;

	c = *p;
	v  = (ulong)c[0] << 24;
	v |= c[1] << 16;
	v |= c[2] << 8;
	v |= c[3];
	*p = c + 4;
	return v;
}


/* Sign-extend a canonical int into either supported VM WORD without a narrowing cast. */
static WORD
dissignedword(ulong bits)
{
	if(bits & 0x80000000UL)
		return -1-(WORD)(0xffffffffUL-bits);
	return bits;
}


/* Decode canonical IEEE words without depending on the width of native ulong. */
double
canontod(ulong v[2])
{
	union { double d; unsigned int ul[2]; } a;
	a.d = 1.;
	if(a.ul[0]) {
		a.ul[0] = v[0];
		a.ul[1] = v[1];
	}
	else {
		a.ul[1] = v[0];
		a.ul[0] = v[1];
	}
	return a.d;
}

Module*
load(char *path)
{
	return readmod(path, nil, 0);
}

int
brpatch(Inst *ip, Module *m)
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
		if(ip->d.imm < 0 || ip->d.imm >= m->nprog)
			return 0;
		ip->d.ins = &m->prog[ip->d.imm];
		break;
	}
	return 1;
}


/* Check the complete encoded operand before the legacy decoder reads it. */
static int
disneed(uchar *p, uchar *end, ulong n)
{
	return (uintptr)p <= (uintptr)end && n <= (uintptr)end-(uintptr)p;
}

static int
disopvalid(uchar *p, uchar *end)
{
	if(!disneed(p,end,1))
		return 0;
	return disneed(p,end,(*p&0xc0) == 0xc0 ? 4 : (*p&0x80) ? 2 : 1);
}

static int
disname(uchar *p, uchar *end)
{
	return p < end && memchr(p,0,end-p) != nil;
}

static int
disrange(int size, int offset, int count, int width)
{
	return offset >= 0 && count >= 0 && offset <= size &&
		count <= (size-offset)/width && offset%width == 0;
}

/* Data initializers must respect the same pointer slots as GC. */
static int
disptr(Type *t, int offset)
{
	int slot;
	if(t == nil || t->size < sizeof(void*))
		return 0;
	offset %= t->size;
	if(offset%sizeof(void*) != 0 || offset > t->size-sizeof(void*))
		return 0;
	slot=offset/sizeof(void*);
	return slot/8 < t->np && (t->map[slot/8] & (0x80>>(slot%8))) != 0;
}

static int
displain(Type *t, int offset, int n)
{
	int at, count, slot, last;
	if(n == 0 || (t != nil && t->np == 0))
		return 1;
	if(t == nil || t->size <= 0)
		return 0;
	while(n != 0) {
		at=offset%t->size;
		count=t->size-at;
		if(count > n)
			count=n;
		last=(at+count-1)/sizeof(void*);
		for(slot=at/sizeof(void*); slot<=last; slot++)
			if(slot/8 < t->np && (t->map[slot/8] & (0x80>>(slot%8))))
				return 0;
		offset+=count;
		n-=count;
	}
	return 1;
}

typedef struct DisLoad DisLoad;

struct DisLoad {
	char *path; // Borrowed source path copied only after parsing succeeds.
	uchar *code; // Borrowed complete canonical image; never retained by the module.
	ulong length; // Checked byte extent of the image.
	Dir *dir; // Borrowed aligned metadata used during construction.
	Module *m; // Partial owner retained for cleanup across protected VM errors.
};


/* Parse and validate module data before publishing any owned runtime objects. */
static int
disparse(void *arg)
{
	DisLoad *c;
	char *path;
	uchar *code;
	ulong length;
	Dir *dir;
	Heap *h;
	Inst *ip;
	Type *pt, *addrtype, *typestack[DADEPTH];
	String *s;
	Module *m;
	Array *ary;
	ulong ul[2];
	WORD lo, hi;
	int lsize, id, v, entry, entryt, tnp, tsz, siglen;
	int de, pc, i, n, isize, dsize, hsize, dasp, addrsize, sizestack[DADEPTH];
	uchar *mod, sm, *istream, **isp, *si, *addr, *end, *dastack[DADEPTH];
	Link *l;

	c=arg; path=c->path; code=c->code; length=c->length; dir=c->dir;
	if(code == nil || code == H || length > 0x7fffffff ||
	   (uintptr)code > (uintptr)-1-(uintptr)length) {
		kwerrstr("bad Dis length");
		return 0;
	}
	if(path == nil || path == H || dir == nil || dir == H ||
	   (uintptr)dir > (uintptr)-1-sizeof(*dir) || (uintptr)dir%sizeof(void*) != 0){
		kwerrstr("bad Dis storage");
		return 0;
	}
	end=code+length;
	istream = code;
	isp = &istream;

	c->m = m = mallocz(sizeof(Module),1);
	if(m == nil)
		return 0;

	m->jit = nil;
	m->dev = dir->dev;
	m->dtype = dir->type;
	m->qid = dir->qid;
	m->mtime = dir->mtime;
	m->origmp = H;
	m->pctab = nil;

	if(!disopvalid(istream,end))
		goto truncated;
	switch(operand(isp)) {
	default:
		kwerrstr("bad magic");
		goto bad;
	case SMAGIC:
		if(!disopvalid(istream,end))
			goto truncated;
		siglen = operand(isp);
		n = length-(*isp-code);
		if(n < 0 || siglen < 0 || siglen > n){
			kwerrstr("corrupt signature");
			goto bad;
		}
		if(verifysigner(*isp, siglen, *isp+siglen, n-siglen) == 0) {
			kwerrstr("security violation");
			goto bad;
		}
		*isp += siglen;
		break;		
	case XMAGIC:
		if(mustbesigned(path, code, length, dir)){
			kwerrstr("security violation: not signed");
			goto bad;
		}
		break;
	}

	if(!disopvalid(istream,end))
		goto truncated;
	m->rt = operand(isp);
	if(!disopvalid(istream,end))
		goto truncated;
	m->ss = operand(isp);
	if(!disopvalid(istream,end))
		goto truncated;
	isize = operand(isp);
	if(!disopvalid(istream,end))
		goto truncated;
	dsize = operand(isp);
	if(!disopvalid(istream,end))
		goto truncated;
	hsize = operand(isp);
	if(!disopvalid(istream,end))
		goto truncated;
	lsize = operand(isp);
	if(!disopvalid(istream,end))
		goto truncated;
	entry = operand(isp);
	if(!disopvalid(istream,end))
		goto truncated;
	entryt = operand(isp);

	if(isize < 0 || dsize < 0 || hsize < 0 || lsize < 0 ||
	   isize > 0x7fffffff/sizeof(Inst) ||
	   hsize > 0x7fffffff/sizeof(Type*) ||
	   lsize >= 0x7fffffff/sizeof(Link) ||
	   isize > (end-istream)/2 || hsize > (end-istream)/3 ||
	   lsize > (end-istream)/7) {
		kwerrstr("implausible Dis file");
		goto bad;
	}

	m->nprog = isize;
	m->prog = mallocz(isize*sizeof(Inst), 0);
	if(m->prog == nil) {
		kwerrstr(exNomem);
		goto bad;
	}

	m->ref = 1;

	ip = m->prog;
	for(i = 0; i < isize; i++) {
		if(!disneed(istream,end,2))
			goto truncated;
		ip->op = *istream++;
		if(ip->op >= MAXDIS) {
			kwerrstr("bad Dis opcode");
			goto bad;
		}
		ip->add = *istream++;
		if(USRC(ip->add) > (AFP|AIND) || UDST(ip->add) > (AFP|AIND))
			goto badinstruction;
		ip->reg = 0;
		ip->s.imm = 0;
		ip->d.imm = 0;
		switch(ip->add & ARM) {
		case AXIMM:
		case AXINF:
		case AXINM:
			if(!disopvalid(istream,end))
				goto truncated;
			v=operand(isp);
			if(v < -32768 || v > 65535 ||
			   ((ip->add&ARM) != AXIMM && v < 0))
				goto badinstruction;
			ip->reg=v;
		 	break;
		}
		switch(UXSRC(ip->add)) {
		case SRC(AFP):
		case SRC(AMP):	
		case SRC(AIMM):
			if(!disopvalid(istream,end))
				goto truncated;
			ip->s.ind = operand(isp);
			break;
		case SRC(AIND|AFP):
		case SRC(AIND|AMP):
			if(!disopvalid(istream,end))
				goto truncated;
			v=operand(isp);
			if(v < 0 || v > 65535)
				goto badinstruction;
			ip->s.i.f=v;
			if(!disopvalid(istream,end))
				goto truncated;
			v=operand(isp);
			if(v < 0 || v > 65535)
				goto badinstruction;
			ip->s.i.s=v;
			break;
		}
		switch(UXDST(ip->add)) {
		case DST(AFP):
		case DST(AMP):	
			if(!disopvalid(istream,end))
				goto truncated;
			ip->d.ind = operand(isp);
			break;
		case DST(AIMM):
			if(!disopvalid(istream,end))
				goto truncated;
			ip->d.ind = operand(isp);
			if(brpatch(ip, m) == 0) {
				kwerrstr("bad branch addr");
				goto bad;
			}
			break;
		case DST(AIND|AFP):
		case DST(AIND|AMP):
			if(!disopvalid(istream,end))
				goto truncated;
			v=operand(isp);
			if(v < 0 || v > 65535)
				goto badinstruction;
			ip->d.i.f=v;
			if(!disopvalid(istream,end))
				goto truncated;
			v=operand(isp);
			if(v < 0 || v > 65535)
				goto badinstruction;
			ip->d.i.s=v;
			break;
		}
		ip++;		
	}

	m->ntype = hsize;
	m->type = mallocz(hsize*sizeof(Type*), 1);
	if(m->type == nil) {
		kwerrstr(exNomem);
		goto bad;
	}
	for(i = 0; i < hsize; i++) {
		if(!disopvalid(istream,end))
			goto truncated;
		id = operand(isp);
		if(id < 0 || id >= hsize || m->type[id] != nil) {
			kwerrstr("heap id range");
			goto bad;
		}
		if(!disopvalid(istream,end))
			goto truncated;
		tsz = operand(isp);
		if(!disopvalid(istream,end))
			goto truncated;
		tnp = operand(isp);
		if(tsz < 0 || tnp < 0 || tnp > 128*1024){
			kwerrstr("implausible Dis file");
			goto bad;
		}
		if(!disneed(istream,end,tnp))
			goto truncated;
		pt = dtype(freeheap, tsz, istream, tnp);
		if(pt == nil) {
			kwerrstr(exNomem);
			goto bad;
		}
		istream += tnp;
		m->type[id] = pt;
	}

	if(dsize != 0) {
		if(hsize == 0) {
			kwerrstr("missing desc for mp");
			goto bad;
		}
		pt = m->type[0];
		if(pt == 0 || pt->size != dsize) {
			kwerrstr("bad desc for mp");
			goto bad;
		}
		h = heapz(pt);
		m->origmp = H2D(uchar*, h);
	}
	addr = m->origmp;
	addrsize=dsize;
	addrtype=dsize == 0 ? nil : m->type[0];
	dasp = 0;
	for(;;) {
		if(!disneed(istream,end,1))
			goto truncated;
		sm = *istream++;
		if(sm == 0)
			break;
		n = DLEN(sm);
		if(n == 0) {
			if(!disopvalid(istream,end))
				goto truncated;
			n = operand(isp);
		}
		if(!disopvalid(istream,end))
			goto truncated;
		v = operand(isp);
		if(v < 0 || v > addrsize || n < 0)
			goto datarange;
		si = addr == H ? H : addr + v;
		switch(DTYPE(sm)) {
		default:
			kwerrstr("bad data item");
			goto bad;
		case DEFS:
			if(!disrange(addrsize,v,1,sizeof(String*)) || !disptr(addrtype,v))
				goto datarange;
			if(!disneed(istream,end,n))
				goto truncated;
			s = c2string((char*)istream, n);
			istream += n;
			destroy(*(String**)si);
			*(String**)si = s;
			break;
		case DEFB:
			if(!disrange(addrsize,v,n,1) ||
			   !displain(addrtype,v,n*1))
				goto datarange;
			if(n > (end-istream)/1)
				goto truncated;
			for(i = 0; i < n; i++)
				*si++ = *istream++;
			break;
		case DEFW:
			if(!disrange(addrsize,v,n,sizeof(WORD)) ||
			   !displain(addrtype,v,n*sizeof(WORD)))
				goto datarange;
			if(n > (end-istream)/4)
				goto truncated;
			for(i = 0; i < n; i++) {
				if(!disneed(istream,end,4))
					goto truncated;
				*(WORD*)si = dissignedword(disw(isp));
				si += sizeof(WORD);
			}
			break;
		case DEFL:
			if(!disrange(addrsize,v,n,sizeof(LONG)) ||
			   !displain(addrtype,v,n*sizeof(LONG)))
				goto datarange;
			if(n > (end-istream)/8)
				goto truncated;
			for(i = 0; i < n; i++) {
				if(!disneed(istream,end,4))
					goto truncated;
				hi = disw(isp);
				if(!disneed(istream,end,4))
					goto truncated;
				lo = disw(isp);
				*(LONG*)si = (LONG)((uvlong)(unsigned int)hi << 32 | (unsigned int)lo);
				si += sizeof(LONG);
			}
			break;
		case DEFF:
			if(!disrange(addrsize,v,n,sizeof(REAL)) ||
			   !displain(addrtype,v,n*sizeof(REAL)))
				goto datarange;
			if(n > (end-istream)/8)
				goto truncated;
			for(i = 0; i < n; i++) {
				if(!disneed(istream,end,4))
					goto truncated;
				ul[0] = disw(isp);
				if(!disneed(istream,end,4))
					goto truncated;
				ul[1] = disw(isp);
				*(REAL*)si = canontod(ul);
				si += sizeof(REAL);
			}
			break;
		case DEFA:			/* Array */
			if(!disrange(addrsize,v,1,sizeof(Array*)) || !disptr(addrtype,v))
				goto datarange;
			if(!disneed(istream,end,4))
				goto truncated;
			v = disw(isp);
			if(v < 0 || v >= m->ntype) {
				kwerrstr("bad array type");
				goto bad;
			}
			pt = m->type[v];
			if(pt == nil || pt == H || pt->ref <= 0 ||
			   pt->ref > 0x7ffffffe - (pt == &Tarray) || Tarray.ref <= 0 || Tarray.ref == 0x7fffffff){
				kwerrstr("invalid array type reference");
				goto bad;
			}
			if(!disneed(istream,end,4))
				goto truncated;
			v = disw(isp);
			if(v < 0 || pt->size < 0 ||
			   (pt->size != 0 && v > (0x7fffffff-sizeof(Array))/pt->size))
				goto datarange;
			h = nheap(sizeof(Array)+(pt->size*v));
			h->t = &Tarray;
			h->t->ref++;
			ary = H2D(Array*, h);
			ary->t = pt;
			ary->len = v;
			ary->root = H;
			ary->data = (uchar*)ary+sizeof(Array);
			memset((void*)ary->data, 0, pt->size*v);
			initarray(pt, ary);
			destroy(A(si));
			A(si) = ary;
			break;			
		case DIND:			/* Set index */
			if(!disrange(addrsize,v,1,sizeof(Array*)) || !disptr(addrtype,v))
				goto datarange;
			ary = A(si);
			if(ary == H || D2H(ary)->t != &Tarray) {
				kwerrstr("ind not array");
				goto bad;
			}
			if(!disneed(istream,end,4))
				goto truncated;
			v = disw(isp);
			if(v > ary->len || v < 0 || dasp >= DADEPTH) {
				kwerrstr("array init range");
				goto bad;
			}
			typestack[dasp]=addrtype;
			sizestack[dasp]=addrsize;
			dastack[dasp++] = addr;
			addrtype=ary->t;
			addrsize=(ary->len-v)*ary->t->size;
			addr = ary->data+v*ary->t->size;
			break;
		case DAPOP:
			if(dasp == 0) {
				kwerrstr("pop range");
				goto bad;
			}
			addr = dastack[--dasp];
			addrsize=sizestack[dasp];
			addrtype=typestack[dasp];
			break;
		}
	}
	mod = istream;
	if(!disname(mod,end) || strlen((char*)mod) >= 128) {
		kwerrstr("bad module name");
		goto bad;
	}
	m->name = strdup((char*)mod);
	if(m->name == nil) {
		kwerrstr(exNomem);
		goto bad;
	}
	while(*istream++)
		;

	l = m->ext = (Link*)mallocz((lsize+1)*sizeof(Link),1);
	if(l == nil){
		kwerrstr(exNomem);
		goto bad;
	}
	for(i = 0; i < lsize; i++, l++) {
		if(!disopvalid(istream,end))
			goto truncated;
		pc = operand(isp);
		if(!disopvalid(istream,end))
			goto truncated;
		de = operand(isp);
		if(!disneed(istream,end,4))
			goto truncated;
		v  = disw(isp);
		if(pc < -1 || pc >= isize || de < -1 || de >= hsize) {
			kwerrstr("bad export index");
			goto bad;
		}
		if(!disname(istream,end))
			goto truncated;
		/* Public module data uses the compiler's (-1, -1, ".mp") sentinel. */
		if(pc == -1 && (de != -1 || strcmp((char*)istream, ".mp") != 0)){
			kwerrstr("bad global-data export");
			goto bad;
		}
		pt = nil;
		if(de != -1)
			pt = m->type[de];
		mlink(m, l, istream, v, pc, pt);
		while(*istream++)
			;
	}
	l->name = nil;

	if(m->rt & HASLDT0){
		kwerrstr("obsolete dis");
		goto bad;
	}

	if(m->rt & HASLDT){
		int j, nl;
		Import *i1, **i2;

		if(!disopvalid(istream,end))
			goto truncated;
		nl = operand(isp);
		if(nl < 0 || nl >= 0x7fffffff/sizeof(Import*) ||
		   nl > end-istream)
			goto datarange;
		i2 = m->ldt = (Import**)mallocz((nl+1)*sizeof(Import*),1);
		if(i2 == nil){
			kwerrstr(exNomem);
			goto bad;
		}
		for(i = 0; i < nl; i++, i2++){
			if(!disopvalid(istream,end))
				goto truncated;
			n = operand(isp);
			if(n < 0 || n >= 0x7fffffff/sizeof(Import) ||
			   n > end-istream)
				goto datarange;
			i1 = *i2 = (Import*)mallocz((n+1)*sizeof(Import),1);
			if(i1 == nil){
				kwerrstr(exNomem);
				goto bad;
			}
			for(j = 0; j < n; j++, i1++){
				if(!disneed(istream,end,4))
					goto truncated;
				i1->sig = disw(isp);
				if(!disname(istream,end))
					goto truncated;
				i1->name = strdup((char*)istream);
				if(i1->name == nil){
					kwerrstr(exNomem);
					goto bad;
				}
				while(*istream++)
					;
			}
		}
		if(!disneed(istream,end,1))
			goto truncated;
		istream++;
	}

	if(m->rt & HASEXCEPT){
		int j, nh;
		Handler *h;
		Except *e;

		if(!disopvalid(istream,end))
			goto truncated;
		nh = operand(isp);
		if(nh < 0 || nh >= 0x7fffffff/sizeof(Handler) ||
		   nh > end-istream)
			goto datarange;
		m->htab = mallocz((nh+1)*sizeof(Handler),1);
		if(m->htab == nil){
			kwerrstr(exNomem);
			goto bad;
		}
		h = m->htab;
		for(i = 0; i < nh; i++, h++){
			if(!disopvalid(istream,end))
				goto truncated;
			h->eoff = operand(isp);
			if(!disopvalid(istream,end))
				goto truncated;
			h->pc1 = operand(isp);
			if(!disopvalid(istream,end))
				goto truncated;
			h->pc2 = operand(isp);
			if(!disopvalid(istream,end))
				goto truncated;
			n = operand(isp);
			if(n < -1 || n >= hsize ||
			   h->pc1 > h->pc2 || h->pc2 > isize) {
				kwerrstr("bad exception index");
				goto bad;
			}
			if(n != -1)
				h->t = m->type[n];
			if(!disopvalid(istream,end))
				goto truncated;
			n = operand(isp);
			if(n < 0 || (n>>16) > (n&0xffff))
				goto datarange;
			h->ne = n>>16;
			n &= 0xffff;
			h->etab = mallocz((n+1)*sizeof(Except),1);
			if(h->etab == nil){
				kwerrstr(exNomem);
				goto bad;
			}
			e = h->etab;
			for(j = 0; j < n; j++, e++){
				if(!disname(istream,end))
					goto truncated;
				e->s = strdup((char*)istream);
				if(e->s == nil){
					kwerrstr(exNomem);
					goto bad;
				}
				while(*istream++)
					;
				if(!disopvalid(istream,end))
					goto truncated;
				e->pc = operand(isp);
				if(e->pc >= (ulong)isize)
					goto datarange;
			}
			e->s = nil;
			if(!disopvalid(istream,end))
				goto truncated;
			e->pc = operand(isp);
			if(e->pc != (ulong)-1 && e->pc >= (ulong)isize)
				goto datarange;
		}
		if(!disneed(istream,end,1))
			goto truncated;
		istream++;
	}

	m->entryt = nil;
	m->entry = m->prog;
	if((ulong)entry < isize && (ulong)entryt < hsize) {
		m->entry = &m->prog[entry];
		m->entryt = m->type[entryt];
	}

	/* Match only fully checked images; C code does not relocate PCs or tables. */
	disc_attach(m, code, length);
	if(m->cstep == nil){
		if(cflag) {
			if((m->rt&DONTCOMPILE) == 0 && !dontcompile)
				compile(m, isize, nil);
		}
		else
		if(m->rt & MUSTCOMPILE && !dontcompile) {
			if(compile(m, isize, nil) == 0) {
				kwerrstr("compiler required");
				goto bad;
			}
		}
	}

	m->path = strdup(path);
	if(m->path == nil) {
		kwerrstr(exNomem);
		goto bad;
	}
	m->link = modules;
	modules = m;

	return 1;
badinstruction:
	kwerrstr("bad instruction operand");
	goto bad;
truncated:
	kwerrstr("truncated Dis file");
	goto bad;
datarange:
	kwerrstr("bad Dis data range");
bad:
	return 0;
}

Module*
parsemod(char *path, uchar *code, ulong length, Dir *dir)
{
	DisLoad c;
	c.path=path; c.code=code; c.length=length; c.dir=dir; c.m=nil;
	if(kprotect(disparse,&c))
		return c.m;
	if(c.m != nil) {
		if(c.m->ext != nil)
			destroylinks(c.m);
		destroy(c.m->origmp);
		freemod(c.m);
	}
	return nil;
}

/* Allocate an unpublished module with owned names and initialized cleanup state. */
Module*
newmod(char *s)
{
	Module *m;

	m = mallocz(sizeof(Module), 1);
	if(m == nil)
		error(exNomem);
	m->ref = 1;
	m->origmp = H;
	m->name = strdup(s);
	m->path = strdup(s);
	if(m->name == nil || m->path == nil){
		free(m->name);
		free(m->path);
		free(m);
		error(exNomem);
	}
	return m;
}

Module*
lookmod(char *s)
{
	Module *m;

	for(m = modules; m != nil; m = m->link)
		if(strcmp(s, m->path) == 0) {
			m->ref++;
			return m;
		}
	return nil;
}

/* Release metadata and let each leaf backend release its own code allocation. */
void
freemod(Module *m)
{
	int i;
	Handler *h;
	Except *e;
	Import *i1, **i2;

	if(m->type != nil) {
		for(i = 0; i < m->ntype; i++)
			freetype(m->type[i]);
		free(m->type);
	}
	if(m->jit != nil) {
		if(m->jit->freecode != nil)
			m->jit->freecode(m->jit->code, m->jit->size);
		else
			free(m->jit->code);
		free(m->jit);
	}
	free(m->name);
	free(m->prog);
	free(m->path);
	free(m->pctab);
	if(m->ldt != nil){
		for(i2 = m->ldt; *i2 != nil; i2++){
			for(i1 = *i2; i1->name != nil; i1++)
				free(i1->name);
			free(*i2);
		}
		free(m->ldt);
	}
	if(m->htab != nil){
		for(h = m->htab; h->etab != nil; h++){
			for(e = h->etab; e->s != nil; e++)
				free(e->s);
			free(h->etab);
		}
		free(m->htab);
	}
	free(m);
}

void
unload(Module *m)
{
	Module **last, *mm;

	m->ref--;
	if(m->ref > 0)
		return;
	if(m->ref == -1)
		abort();

	last = &modules;
	for(mm = modules; mm != nil; mm = mm->link) {
		if(mm == m) {
			*last = m->link;
			break;
		}
		last = &mm->link;
	}

	if(m->rt == DYNMOD)
		freedyncode(m);
	else
		destroy(m->origmp);

	destroylinks(m);

	freemod(m);
}
