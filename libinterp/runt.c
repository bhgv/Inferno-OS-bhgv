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

typedef struct XprintString XprintString;

struct XprintString
{
	uchar *data; // Validated inline ASCII or Rune storage.
	int length; // Nonnegative count of logical characters.
	int wide; // Whether characters occupy native Rune cells.
};


/* Validate borrowed string metadata and exactly the payload the formatter uses. */
static void
xprintstring(String *string, int terminate, XprintString *view)
{
	int length, capacity;
	uintptr address, extent;
	uvlong bytes;

	if(string == nil)
		error(exNilref);
	view->data = (uchar*)"";
	view->length = view->wide = 0;
	if(string == H)
		return;
	address = (uintptr)string;
	extent = O(String, len) > O(String, max) ? O(String, len) : O(String, max);
	if(address > (uintptr)-1-extent-sizeof(length))
		error(exHeap);
	memmove(&length, (uchar*)string+O(String, len), sizeof(length));
	memmove(&capacity, (uchar*)string+O(String, max), sizeof(capacity));
	if(length == (-0x7fffffff-1))
		error(exInval);
	view->wide = length < 0;
	view->length = length < 0 ? -length : length;
	if(capacity < view->length)
		error(exHeap);
	/* An empty format reads no payload; string arguments still need a terminator. */
	if(view->length == 0 && !terminate)
		return;
	bytes = ((uvlong)view->length+terminate)*(view->wide ? sizeof(Rune) : 1);
	if(address > (uintptr)-1-O(String, data) ||
	   bytes > (uintptr)-1-(address+O(String, data)))
		error(exHeap);
	view->data = (uchar*)(address+O(String, data));
	/* The lib9 Rune formatter consumes Rune*, whereas pattern reads use byte copies. */
	if(terminate && view->wide && (uintptr)view->data%sizeof(Rune) != 0)
		error(exHeap);
}


/* Align by native coordinates and bound an argument before copying its bytes. */
static uchar*
xprintargument(void *frame, uchar *argument, int bytes, int alignment)
{
	uintptr address, padding;

	if(argument == nil || argument == H)
		error(exNilref);
	address = (uintptr)argument;
	padding = ((uintptr)frame-address)&(alignment-1);
	if(padding > (uintptr)-1-address ||
	   (uintptr)bytes > (uintptr)-1-address-padding)
		error(exBounds);
	return (uchar*)(address+padding);
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
	Rune r, zero = 0;
	XprintString pattern, text;
	ulong references;

	fp = vfp;
	va = vva;

	sip = 0;
	isr = 0;
	if(s1 == H || n <= 0)
		return 0;
	if(s1 == nil || buf == nil || buf == H)
		error(exNilref);
	if((uintptr)buf > (uintptr)-1-(uintptr)n)
		error(exBounds);
	xprintstring(s1, 0, &pattern);
	nc = pattern.length;
	isr = pattern.wide;

	b = buf;
	eb = buf+n-1;
	while(nc > 0) {
		nc--;
		if(isr){
			memmove(&r, pattern.data+(uintptr)sip*sizeof(r), sizeof(r));
			c = r;
		}else
			c = pattern.data[sip];
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
			if(isr){
				memmove(&r, pattern.data+(uintptr)sip*sizeof(r), sizeof(r));
				c = r;
			}else
				c = pattern.data[sip];
			sip++;
			*f++ = c;
			*f = '\0';
			switch(c) {
			default:
				continue;
			case '*':
				/* Dis integer arguments occupy four bytes, even on LP64 hosts. */
				va = xprintargument(fp, va, IBY2WD, 1);
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
				va = xprintargument(fp, va, sizeof(ss), 1);
				memmove(&ss, va, sizeof(ss));
				xprintstring(ss, 1, &text);
				va += IBY2WD;
				if(ss == H)
					p = "";
				else
				if(text.wide) {
					f[-1] += 'A'-'a';
					memmove(text.data+(uintptr)text.length*sizeof(zero), &zero, sizeof(zero));
					p = text.data;
				}
				else {
					text.data[text.length] = '\0';
					p = text.data;
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
				va = xprintargument(fp, va, sizeof(d), sizeof(REAL));
				memmove(&d, va, sizeof(d));
				b += xsnprint(b, eb-b+1, fmt, d);
				va += sizeof(REAL);
				break;
			case 'd':
			case 'o':
			case 'x':
			case 'X':
			case 'c':
				if(isbig) {
					va = xprintargument(fp, va, sizeof(bg), IBY2LG);
					memmove(&bg, va, sizeof(bg));
					b += xsnprint(b, eb-b+1, fmt, bg);
					va += IBY2LG;
				}
				else {
					/* Dis integer arguments occupy four bytes, even on LP64 hosts. */
					va = xprintargument(fp, va, IBY2WD, 1);
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
				va = xprintargument(fp, va, sizeof(ptr), 1);
				memmove(&ptr, va, sizeof(ptr));
				if(ptr == nil)
					error(exNilref);
				t = nil;
				if(ptr != H) {
					if((uintptr)ptr <= sizeof(Heap) || (uintptr)ptr%sizeof(void*) != 0)
						error(exHeap);
					references = D2H(ptr)->ref;
					t = D2H(ptr)->t;
					b += xsnprint(b, eb-b+1, "%lud.%.8lux", references, (ulong)t);
				}else
					b += xsnprint(b, eb-b+1, "%d.%.8lux", -1, (ulong)t);
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
	Prog *prog; // Borrowed process used for error-string formatting.
	void *frame; // Borrowed frame-relative argument alignment anchor.
	void *args; // Initial packed argument cursor for each formatting attempt.
	String *format; // Borrowed format retained by the calling VM frame.
	char *buf; // Owned temporary output released when protected formatting fails.
	int size; // Current output capacity in bytes.
	int written; // Successful byte count, including embedded NUL characters.
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
	if(buf == nil || buf == H)
		error(exNilref);
	if((uintptr)buf > (uintptr)-1-sizeof(*buf))
		error(exBounds);
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
	memmove(buf, &call.buf, sizeof(call.buf));
	return call.written;
}

typedef struct XprintResult XprintResult;

struct XprintResult
{
	char *text; // Borrowed formatted bytes used while constructing the result.
	int length; // Exact byte count, independent of embedded NULs.
	int array; // Whether the VM result is a byte array instead of a String.
	void *value; // Newly constructed result published only after success.
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
		result->value = c2string(result->text, result->length);
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
	memmove(&old, f->ret, sizeof(old));
	memmove(f->ret, &result, sizeof(result));
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
	memmove(&old, f->ret, sizeof(old));
	memmove(f->ret, &result, sizeof(result));
	destroy(old);
}


/* Compare unsigned byte characters with runes; nil denotes no separators. */
static int
tokdelim(int c, String *d)
{
	int l;
	uchar *p;
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
	for(p = (uchar*)d->Sascii; l != 0; l--)
		if(*p++ == c)
			return 1;
	return 0;
}

typedef struct TokenizeResult TokenizeResult;

struct TokenizeResult
{
	String *source; // Input string retained by the caller throughout construction.
	String *delimiters; // Delimiter set in either string representation.
	List *words; // Partial word list owned by protected cleanup.
	int count; // Number of complete words built before publication.
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
	if(s == nil || d == nil)
		error(exNilref);
	/* Validate before negating rune counts or allocating partial words. */
	if(s->len == (-0x7fffffff-1) || s->max < 0 ||
	   (s->len < 0 ? -s->len : s->len) > s->max ||
	   (d != H && (d->len == (-0x7fffffff-1) || d->max < 0 ||
	   (d->len < 0 ? -d->len : d->len) > d->max)))
		error(exInval);
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
			c = srune ? s->Srune[first] : (uchar)s->Sascii[first];
			if(!tokdelim(c, d))
				break;
			first++;
		}
		last = first;
		while(last < nc){
			c = srune ? s->Srune[last] : (uchar)s->Sascii[last];
			if(tokdelim(c, d))
				break;
			last++;
		}
		if(first == last)
			break;
		if(Tptr.ref <= 0 || Tptr.ref == 0x7fffffff)
			error(exHeap);
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


/* Publish tuple fields through byte copies after every allocation succeeds. */
void
Sys_tokenize(void *fp)
{
	typedef struct { WORD count; List *words; } Value;
	F_Sys_tokenize *f;
	TokenizeResult result;
	List *old;
	WORD count;
	uchar *ret;

	f = fp;
	ret = (uchar*)f->ret;
	if(ret == nil || ret == H)
		error(exNilref);
	result.source = f->s;
	result.delimiters = f->delim;
	result.words = H;
	result.count = 0;
	if(!kprotect(buildtokens, &result)){
		destroy(result.words);
		krethrow();
	}
	count = result.count;
	memmove(&old, ret+O(Value, words), sizeof(old));
	memmove(ret+O(Value, count), &count, sizeof(count));
	memmove(ret+O(Value, words), &result.words, sizeof(result.words));
	destroy(old);
}


/* Validate byte-array metadata and full VM lengths before native UTF routines. */
static int
sysutfarray(Array *a)
{
	if(a == H || a == nil)
		error(exBounds);
	/* A compiler may emit its own pointer-free byte descriptor. */
	if(D2H(a)->t != &Tarray ||
	   (a->t != &Tbyte &&
	    (a->t == nil || a->t == H || a->t->size != 1 || a->t->np != 0)))
		error(exType);
	if(a->len < 0 || (uvlong)a->len > 0x7fffffff || (a->len != 0 && (a->data == nil || a->data == H)))
		error(exBounds);
	return (int)a->len;
}


/* Validate result slots before UTF operations can modify an aliased byte array. */
static uchar*
sysutfresult(void *slot)
{
	if(slot == nil || slot == H)
		error(exNilref);
	return slot;
}


/* Count complete UTF sequences and publish WORD without requiring alignment. */
void
Sys_utfbytes(void *fp)
{
	Array *a;
	int nbyte, length;
	WORD result;
	uchar *ret;
	F_Sys_utfbytes *f;

	f = fp;
	ret = sysutfresult(f->ret);
	a = f->buf;
	length = sysutfarray(a);
	if(f->n < 0 || f->n > length)
		error(exBounds);

	utfnleng((char*)a->data, (int)f->n, &nbyte);
	result = nbyte;
	memmove(ret, &result, sizeof(result));
}


/* Decode the complete tuple before publishing into a possibly unaligned slot. */
void
Sys_byte2char(void *fp)
{
	Rune r;
	char *p;
	int n, w, valid, length;
	WORD result[3];
	uchar *ret;
	Array *a;
	F_Sys_byte2char *f;

	f = fp;
	ret = sysutfresult(f->ret);
	a = f->buf;
	length = sysutfarray(a);
	if(f->n < 0 || f->n >= length)
		error(exBounds);
	n = (int)f->n;
	r = a->data[n];
	w = valid = 1;
	if(r >= Runeself){
		p = (char*)a->data+n;
		if(length-n >= UTFmax || fullrune(p, length-n)){
			w = chartorune(&r, p);
			valid = r != Runeerror || w != 1;
		}else{
			r = Runeerror;
			w = valid = 0;
		}
	}
	result[0] = r;
	result[1] = w;
	result[2] = valid;
	memmove(ret, result, sizeof(result));
}


/* Encode before publishing the count, preserving the full rune and offset. */
void
Sys_char2byte(void *fp)
{
	F_Sys_char2byte *f;
	Array *a;
	int n, length;
	WORD c, result;
	Rune r;
	uchar *ret;

	f = fp;
	ret = sysutfresult(f->ret);
	a = f->buf;
	length = sysutfarray(a);
	c = f->c;
	if(f->n < 0 || f->n >= length)
		error(exBounds);
	n = (int)f->n;
	if(c < 0 || c > Runemax)
		c = Runeerror;
	result = 0;
	if(c < Runeself){
		a->data[n] = c;
		result = 1;
	}else{
		r = c;
		if(length-n >= UTFmax || runelen(c) <= length-n)
			result = runetochar((char*)a->data+n, &r);
	}
	memmove(ret, &result, sizeof(result));
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


/* Prepare the string before publishing through a potentially unaligned return slot. */
void
retnstr(char *s, int n, void *d)
{
	String *s1, *old;

	s1 = H;
	if(n != 0)
		s1 = c2string(s, n);
	memmove(&old, d, sizeof(old));
	memmove(d, &s1, sizeof(s1));
	destroy(old);
}


/* Retain the original owner until conversion and byte-addressed slot publication succeed. */
void
retstr(char *s, void *d)
{
	String *s1, *old;
	uvlong length;

	s1 = H;
	if(s != nil){
		length = strlen(s);
		if(length > 0x7fffffff)
			error(exHeap);
		s1 = c2string(s, (int)length);
	}
	memmove(&old, d, sizeof(old));
	memmove(d, &s1, sizeof(s1));
	destroy(old);
}


/* Reserve both descriptors and bound the total heap size before allocating bytes. */
Array*
mem2array(void *va, int n)
{
	Heap *h;
	Array *a;

	if(n < 0)
		n = 0;
	if(Tarray.ref <= 0 || Tarray.ref == 0x7fffffff ||
	   Tbyte.ref <= 0 || Tbyte.ref == 0x7fffffff ||
	   (ulong)n > 0x7fffffff-sizeof(Heap)-sizeof(Array))
		error(exHeap);
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
