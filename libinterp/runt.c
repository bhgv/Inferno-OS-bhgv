#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "sysmod.h"
#include "raise.h"
#include "kernel.h"


static	int		utfnleng(char*, int, int*);

void
sysmodinit(void)
{
	sysinit();
	builtinmod("$Sys", Sysmodtab, Sysmodlen);
}

/* A full destination consumes no bytes, even when vsnprint returns -1. */
static int
xsnprint(char *buf, int n, char *format, ...)
{
	va_list args;
	int written;

	if(n <= 0)
		return 0;
	va_start(args, format);
	written = vsnprint(buf, n, format, args);
	va_end(args);
	return written < 0 ? 0 : written;
}

/* Consume arguments even after output fills, bounding each format specification. */
int
xprint(Prog *xp, void *vfp, void *vva, String *s1, char *buf, int n)
{
	int i;
	void *p;
	LONG bg;
	Type *t;
	double d;
	String *ss;
	ulong *ptr;
	uchar *fp, *va;
	int nc, c, isbig, isr, sip;
	char *b, *eb, *f, fmt[32];
	Rune r;

	fp = vfp;
	va = vva;

	sip = 0;
	isr = 0;
	if(s1 == H || n <= 0)
		return 0;
	nc = s1->len;
	if(nc < 0) {
		nc = -nc;
		isr = 1;
	}

	b = buf;
	eb = buf+n-1;
	while(nc > 0) {
		nc--;
		c = isr ? s1->Srune[sip] : s1->Sascii[sip];
		sip++;
		if(c != '%') {
			if(b < eb) {
				if(c < Runeself)
					*b++ = c;
				else
					b += xsnprint(b, eb-b+1, "%C", c);
			}
			continue;
		}
		f = fmt;
		*f++ = c;
		isbig = 0;
		while(nc > 0) {
			nc--;
			if(f-fmt > sizeof(fmt)-UTFmax-1)
				error(exInval);
			c = isr ? s1->Srune[sip] : s1->Sascii[sip];
			sip++;
			*f++ = c;
			*f = '\0';
			switch(c) {
			default:
				continue;
			case '*':
				/* Dis integer arguments occupy four bytes, even on LP64 hosts. */
				memmove(&i, va, IBY2WD);
				f--;
				/* Widths need at most eleven decimal bytes and a terminator. */
				if(sizeof(fmt)-(f-fmt) < 12+UTFmax)
					error(exInval);
				f += snprint(f, sizeof(fmt)-(f-fmt), "%d", (int)i);
				va += IBY2WD;
				continue;
			case 'b':
				f[-1] = 'l';
				*f++ = 'l';
				*f = '\0';
				isbig = 1;
				continue;
			case '%':
				if(b < eb)
					*b++ = '%';
				break;
			case 'q':
			case 's':
				ss = *(String**)va;
				va += IBY2WD;
				if(ss == H)
					p = "";
				else
				if(ss->len < 0) {
					f[-1] += 'A'-'a';
					ss->Srune[-ss->len] = L'\0';
					p = ss->Srune;
				}
				else {
					ss->Sascii[ss->len] = '\0';
					p = ss->Sascii;
				}
				b += xsnprint(b, eb-b+1, fmt, p);
				break;
			case 'E':
				f--;
				r = 0x00c9;	/* L'É' */
				f += runetochar(f, &r);	/* avoid clash with ether address */
				*f = '\0';
				/* fall through */
			case 'e':
			case 'f':
			case 'g':
			case 'G':
				while((va - fp) & (sizeof(REAL)-1))
					va++;
				d = *(REAL*)va;
				b += xsnprint(b, eb-b+1, fmt, d);
				va += sizeof(REAL);
				break;
			case 'd':
			case 'o':
			case 'x':
			case 'X':
			case 'c':
				if(isbig) {
					while((va - fp) & (IBY2LG-1))
						va++;
					bg = *(LONG*)va;
					b += xsnprint(b, eb-b+1, fmt, bg);
					va += IBY2LG;
				}
				else {
					/* Dis integer arguments occupy four bytes, even on LP64 hosts. */
					memmove(&i, va, IBY2WD);
					/* always a unicode character */
					if(c == 'c')
						f[-1] = 'C';
					b += xsnprint(b, eb-b+1, fmt, (int)i);
					va += IBY2WD;
				}
				break;
			case 'r':
				if(b < eb)
					b = syserr(b, eb, xp);
				break;
/* Debugging formats - may disappear */
			case 'H':
				ptr = *(ulong**)va;
				c = -1;
				t = nil;
				if(ptr != H) {
					c = D2H(ptr)->ref;
					t = D2H(ptr)->t;
				}
				b += xsnprint(b, eb-b+1, "%d.%.8lux", c, (ulong)t);
				va += IBY2WD;
				break;
			}
			break;
		}
	}
	return b - buf;
}

typedef struct XprintCall XprintCall;
struct XprintCall
{
	Prog *prog;
	void *frame;
	void *args;
	String *format;
	char *buf;
	int size;
	int written;
};

/* Keep the allocated output visible to cleanup if formatting raises a VM error. */
static int
xprintcall(void *arg)
{
	XprintCall *call;

	call = arg;
	call->written = xprint(call->prog, call->frame, call->args,
		call->format, call->buf, call->size);
	return 1;
}

/* Grow without signed overflow and free temporary output before propagating errors. */
int
bigxprint(Prog *xp, void *vfp, void *vva, String *s1, char **buf, int s)
{
	XprintCall call;
	int m;

	if(s <= 0)
		error(exInval);
	call.prog = xp;
	call.frame = vfp;
	call.args = vva;
	call.format = s1;
	m = s;
	for(;;){
		if(m > 0x7fffffff/2)
			error(exNomem);
		m *= 2;
		call.size = m;
		call.buf = malloc(m);
		if(call.buf == nil)
			error(exNomem);
		if(!kprotect(xprintcall, &call)){
			free(call.buf);
			krethrow();
		}
		if(call.written < m-UTFmax-2)
			break;
		free(call.buf);
	}
	*buf = call.buf;
	return call.written;
}

typedef struct XprintResult XprintResult;
struct XprintResult
{
	char *text;
	int length;
	int array;
	void *value;
};

/* Build a new VM owner without touching the caller's previous result. */
static int
buildprintresult(void *arg)
{
	XprintResult *result;

	result = arg;
	if(result->array)
		result->value = mem2array(result->text, result->length);
	else
		result->value = c2string(result->text, strlen(result->text));
	return 1;
}

/* Release dynamic formatting storage on both conversion success and VM error. */
static void*
printresult(char *text, int length, int array, int allocated)
{
	XprintResult result;
	int ok;

	result.text = text;
	result.length = length;
	result.array = array;
	result.value = H;
	ok = kprotect(buildprintresult, &result);
	if(allocated)
		free(text);
	if(!ok)
		krethrow();
	return result.value;
}

/* Publish a formatted string only after conversion, retaining old results on error. */
void
Sys_sprint(void *fp)
{
	int n;
	char buf[256], *b = buf;
	String *result, *old;
	F_Sys_sprint *f;

	f = fp;
	n = xprint(currun(), f, &f->vargs, f->s, buf, sizeof(buf));
	if(n >= sizeof(buf)-UTFmax-2)
		n = bigxprint(currun(), f, &f->vargs, f->s, &b, sizeof(buf));
	b[n] = '\0';
	result = printresult(b, n, 0, b != buf);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* Publish a formatted byte array after allocation and temporary-buffer cleanup. */
void
Sys_aprint(void *fp)
{
	int n;
	char buf[256], *b = buf;
	Array *result, *old;
	F_Sys_aprint *f;

	f = fp;
	n = xprint(currun(), f, &f->vargs, f->s, buf, sizeof(buf));
	if(n >= sizeof(buf)-UTFmax-2)
		n = bigxprint(currun(), f, &f->vargs, f->s, &b, sizeof(buf));
	result = printresult(b, n, 1, b != buf);
	old = *f->ret;
	*f->ret = result;
	destroy(old);
}

/* A nil delimiter string denotes an empty set of separator characters. */
static int
tokdelim(int c, String *d)
{
	int l;
	char *p;
	Rune *r;

	if(d == H)
		return 0;
	l = d->len;
	if(l < 0) {
		l = -l;
		for(r = d->Srune; l != 0; l--)
			if(*r++ == c)
				return 1;
		return 0;
	}
	for(p = d->Sascii; l != 0; l--)
		if(*p++ == c)
			return 1;
	return 0;
}

typedef struct TokenizeResult TokenizeResult;
struct TokenizeResult
{
	String *source;
	String *delimiters;
	List *words;
	int count;
};

/* Keep partial words under one owner until every allocation has succeeded. */
static int
buildtokens(void *arg)
{
	TokenizeResult *result;
	String *s, *d;
	List **h, *nl;
	int c, nc, first, last, srune;

	result = arg;
	s = result->source;
	d = result->delimiters;
	if(s == H)
		return 1;
	h = &result->words;
	first = 0;
	srune = 0;
	nc = s->len;
	if(nc < 0){
		nc = -nc;
		srune = 1;
	}
	while(first < nc){
		while(first < nc){
			c = srune ? s->Srune[first] : s->Sascii[first];
			if(!tokdelim(c, d))
				break;
			first++;
		}
		last = first;
		while(last < nc){
			c = srune ? s->Srune[last] : s->Sascii[last];
			if(tokdelim(c, d))
				break;
			last++;
		}
		if(first == last)
			break;
		nl = cons(sizeof(String*), h);
		nl->tail = H;
		nl->t = &Tptr;
		Tptr.ref++;
		/* slicer can raise an error after the list node has been published. */
		*(String**)nl->data = H;
		*(String**)nl->data = slicer(first, last, s);
		h = &nl->tail;
		first = last;
		result->count++;
	}
	return 1;
}

/* Publish complete token lists, releasing all partial words on a VM error. */
void
Sys_tokenize(void *fp)
{
	F_Sys_tokenize *f;
	TokenizeResult result;
	List *old;

	f = fp;
	result.source = f->s;
	result.delimiters = f->delim;
	result.words = H;
	result.count = 0;
	if(!kprotect(buildtokens, &result)){
		destroy(result.words);
		krethrow();
	}
	old = f->ret->t1;
	f->ret->t0 = result.count;
	f->ret->t1 = result.words;
	destroy(old);
}

/* Validate byte-array metadata and full VM lengths before native UTF routines. */
static int
sysutfarray(Array *a)
{
	if(a == H || a == nil)
		error(exBounds);
	if(D2H(a)->t != &Tarray || a->t != &Tbyte)
		error(exType);
	if(a->len < 0 || (uvlong)a->len > 0x7fffffff || (a->len != 0 && a->data == nil))
		error(exBounds);
	return (int)a->len;
}

/* Count only complete UTF sequences within the validated byte prefix. */
void
Sys_utfbytes(void *fp)
{
	Array *a;
	int nbyte, length;
	F_Sys_utfbytes *f;

	f = fp;
	a = f->buf;
	length = sysutfarray(a);
	if(f->n < 0 || f->n > length)
		error(exBounds);

	utfnleng((char*)a->data, (int)f->n, &nbyte);
	*f->ret = nbyte;
}

/* Validate the full VM offset before decoding one possibly incomplete rune. */
void
Sys_byte2char(void *fp)
{
	Rune r;
	char *p;
	int n, w, length;
	Array *a;
	F_Sys_byte2char *f;

	f = fp;
	a = f->buf;
	length = sysutfarray(a);
	if(f->n < 0 || f->n >= length)
		error(exBounds);
	n = (int)f->n;
	r = a->data[n];
	if(r < Runeself){
		f->ret->t0 = r;
		f->ret->t1 = 1;
		f->ret->t2 = 1;
		return;
	}
	p = (char*)a->data+n;
	if(length-n >= UTFmax || fullrune(p, length-n))
		w = chartorune(&r, p);
	else {
		/* insufficient data */
		f->ret->t0 = Runeerror;
		f->ret->t1 = 0;
		f->ret->t2 = 0;
		return;
	}
	if(r == Runeerror && w==1){	/* encoding error */
		f->ret->t0 = Runeerror;
		f->ret->t1 = 1;
		f->ret->t2 = 0;
		return;
	}
	f->ret->t0 = r;
	f->ret->t1 = w;
	f->ret->t2 = 1;
}

/* Preserve the full rune and offset until validation, including U+10FFFF. */
void
Sys_char2byte(void *fp)
{
	F_Sys_char2byte *f;
	Array *a;
	int n, length;
	WORD c;
	Rune r;

	f = fp;
	a = f->buf;
	length = sysutfarray(a);
	c = f->c;
	if(f->n < 0 || f->n >= length)
		error(exBounds);
	n = (int)f->n;
	if(c < 0 || c > Runemax)
		c = Runeerror;
	if(c < Runeself){
		a->data[n] = c;
		*f->ret = 1;
		return;
	}
	r = c;
	if(length-n >= UTFmax || runelen(c) <= length-n){
		*f->ret = runetochar((char*)a->data+n, &r);
		return;
	}
	*f->ret = 0;
}

typedef struct BuiltinModule BuiltinModule;
struct BuiltinModule
{
	char *name;
	Runtab *table;
	int count;
	Module *module;
};

/* Keep every frame descriptor reachable by cleanup while export names can fail. */
static int
buildbuiltin(void *arg)
{
	BuiltinModule *c;
	Module *m;
	Runtab *r;
	Type *t;
	int i;

	c = arg;
	c->module = m = newmod(c->name);
	m->ntype = c->count;
	if(c->count != 0){
		m->type = mallocz(c->count*sizeof(Type*), 1);
		if(m->type == nil){
			kwerrstr(exNomem);
			return 0;
		}
	}
	m->ext = mallocz((c->count+1)*sizeof(Link), 1);
	if(m->ext == nil){
		kwerrstr(exNomem);
		return 0;
	}
	for(i = 0; i < c->count; i++){
		r = &c->table[i];
		t = dtype(freeheap, r->size, r->map, r->np);
		if(t == nil){
			kwerrstr("bad builtin frame or out of memory");
			return 0;
		}
		m->type[i] = t;
		runtime(m, &m->ext[i], r->name, r->sig, r->fn, t);
	}
	return 1;
}

/* Validate table bounds, build under one error owner, then publish exactly once. */
Module*
builtinmod(char *name, void *vr, int rlen)
{
	BuiltinModule c;
	Runtab *r;
	Module *m;
	int n, limit;

	limit = 0x7fffffff/sizeof(Link)-1;
	if(name == nil || vr == nil || rlen < 0 || rlen > limit)
		goto badtable;
	r = vr;
	for(n = 0; r[n].name != nil; n++){
		if(n >= limit || (rlen != 0 && n >= rlen) ||
		   r[n].fn == nil || r[n].size < 0 ||
		   r[n].np < 0 || r[n].np > sizeof(r[n].map))
			goto badtable;
	}
	if(rlen != 0 && n != rlen)
		goto badtable;
	c.name = name;
	c.table = r;
	c.count = n;
	c.module = nil;
	if(kprotect(buildbuiltin, &c)){
		m = c.module;
		m->link = modules;
		modules = m;
		return m;
	}
	if(c.module != nil){
		destroylinks(c.module);
		freemod(c.module);
	}
	return nil;
badtable:
	kwerrstr("bad builtin table");
	return nil;
}

void
retnstr(char *s, int n, String **d)
{
	String *s1;

	s1 = H;
	if(n != 0)
		s1 = c2string(s, n);
	destroy(*d);
	*d = s1;
}

void
retstr(char *s, String **d)
{
	String *s1;

	s1 = H;
	if(s != nil)
		s1 = c2string(s, strlen(s));
	destroy(*d);
	*d = s1;
}

Array*
mem2array(void *va, int n)
{
	Heap *h;
	Array *a;

	if(n < 0)
		n = 0;
	h = nheap(sizeof(Array)+n);
	h->t = &Tarray;
	h->t->ref++;
	a = H2D(Array*, h);
	a->t = &Tbyte;
	Tbyte.ref++;
	a->len = n;
	a->root = H;
	a->data = (uchar*)a+sizeof(Array);
	if(va != 0)
		memmove(a->data, va, n);

	return a;
}

/* Empty prefixes need no data pointer; compare remaining bytes within the span. */
static int
utfnleng(char *s, int nb, int *ngood)
{
	int c;
	long n;
	Rune rune;
	char *es, *starts;

	if(nb == 0){
		if(ngood)
			*ngood = 0;
		return 0;
	}
	starts = s;
	es = s+nb;
	for(n = 0; s < es; n++) {
		c = *(uchar*)s;
		if(c < Runeself)
			s++;
		else {
			if(es-s >= UTFmax || fullrune(s, es-s))
				s += chartorune(&rune, s);
			else
				break;
		}
	}
	if(ngood)
		*ngood = s-starts;
	return n;
}
