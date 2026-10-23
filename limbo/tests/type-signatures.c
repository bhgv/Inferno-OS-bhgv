#include "limbo.h"
#include "libsec.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

char debug[256];
char *signdump;
int storespace[Dend];
static int eqrec;
static int rtsign(Type*, uchar*, int, int);
static int idsign(Decl*, int, uchar*, int, int);
static int idsign1(Decl*, int, uchar*, int, int);
static int raisessign(Node*, uchar*, int, int);
static int clearrec(Type*);

/* Stop immediately if a valid fixture reaches a compiler diagnostic. */
void
fatal(char *message, ...)
{
	fprintf(stderr, "%s\n", message);
	abort();
}

/* Retain normal allocator ownership while bounding fixture growth. */
void*
reallocmem(void *p, ulong n)
{
	assert(n < 1048576);
	p = realloc(p, n);
	assert(p != nil);
	return p;
}

/* These fixtures contain no polymorphic function-pointer rewriting. */
int
ispoly(Decl *d)
{
	(void)d;
	return 0;
}

/* Rewriting would indicate an unexpected fixture dependency. */
void
rmfnptrs(Decl *d)
{
	(void)d;
	abort();
}

/* Supply only the decimal and real formatting used by signature serialization. */
char*
seprint(char *p, char *end, char *format, ...)
{
	va_list ap;
	int n;

	va_start(ap, format);
	n = vsnprintf(p, end-p, format, ap);
	va_end(ap);
	assert(n >= 0 && n < end-p);
	return p+n;
}

/* Debug output is disabled; production diagnostics must not be reached. */
int
print(char *format, ...)
{
	(void)format;
	abort();
}

IMPLEMENTATION

/* Sweep every small capacity and verify both guards and recursive cleanup. */
static void
check(Type *t, char *expected)
{
	uchar buffer[512];
	int cap, n, i, length;
	Decl d = {.ty=t};

	length = strlen(expected);
	for(cap = 0; cap < length+24; cap++){
		memset(buffer, 0xa5, sizeof(buffer));
		eqrec = 0;
		n = rtsign(t, buffer+16, cap, 0);
		assert(clearrec(t) == eqrec);
		eqrec = 0;
		for(i = 0; i < 16; i++)
			assert(buffer[i] == 0xa5);
		for(i = 16+cap; i < sizeof(buffer); i++)
			assert(buffer[i] == 0xa5);
		if(n >= 0){
			assert(n == length && n < cap);
			assert(memcmp(buffer+16, expected, n) == 0);
		}else
			assert(cap < length+16);
	}
	t->sig = 0;
	printf("%s %08lx\n", expected, sign(&d));
	assert(sign(&d) == (u32int)t->sig);
}

/* Exercise shared nodes, recursion, exception names and failed-position inputs. */
int
main(void)
{
	Type integer = {.kind=Tint}, string = {.kind=Tstring};
	Type tuple = {.kind=Ttuple}, fn = {.kind=Tfn,.varargs=1,.tof=&integer};
	Teq eq = {0};
	Type adt = {.kind=Tadt,.eq=&eq}, ref = {.kind=Tref,.tof=&adt};
	Sym self = {.name="self",.len=4}, value = {.name="value",.len=5};
	Decl a = {.ty=&integer,.store=Darg}, b = {.ty=&string,.store=Darg};
	Decl field = {.sym=&self,.ty=&ref,.store=Dfield};
	Decl field2 = {.sym=&value,.ty=&integer,.store=Dfield};
	Sym error = {.name="SomeException",.len=13};
	Decl ed = {.sym=&error};
	Node en = {.decl=&ed}, list = {.left=&en}, raises = {.left=&list};
	uchar buf[64], before[64];
	char longname[8193];
	ulong first;
	int positions[] = {-1, INT_MIN, INT_MAX}, i;
	Node real = {.rval=-1.23456e200};
	Type fixed = {.kind=Tfix,.val=&real};

	storespace[Darg] = storespace[Dfield] = 1;
	a.next = &b;
	tuple.ids = &a;
	fn.ids = &a;
	fn.u.eraises = &raises;
	field.next = &field2;
	adt.ids = &field;
	check(&integer, "i");
	check(&tuple, "t(i,s)");
	check(&fn, "f*(i,s)(SomeException)i");
	check(&adt, "a(self:R@1,value:i)");
	check(&fixed, "x-1.23456e+200");
	fn.polys = &a;
	check(&fn, "f*(i,s)(i,s)(SomeException)i");
	memset(buf, 0xa5, sizeof(buf));
	memmove(before, buf, sizeof(buf));
	for(i = 0; i < 3; i++){
		assert(idsign(&a, 1, buf, sizeof(buf), positions[i]) == -1);
		assert(idsign1(&a, 0, buf, sizeof(buf), positions[i]) == -1);
		assert(raisessign(&raises, buf, sizeof(buf), positions[i]) == -1);
		assert(rtsign(&integer, buf, sizeof(buf), positions[i]) == -1);
	}
	assert(memcmp(buf, before, sizeof(buf)) == 0);
	eq.id = eqrec = 1234567890;
	assert(rtsign(&adt, buf, 11, 0) == -1);
	assert(buf[11] == 0xa5);
	eq.id = eqrec = 0;
	/* Force several real sign() growth retries through a recursive graph. */
	memset(longname, 'q', sizeof(longname)-1);
	longname[sizeof(longname)-1] = 0;
	self.name = longname;
	self.len = sizeof(longname)-1;
	adt.sig = 0;
	{ Decl d = {.ty=&adt}; first = sign(&d); }
	assert(adt.rec == 0 && eq.id == 0);
	adt.sig = 0;
	{ Decl d = {.ty=&adt}; assert(sign(&d) == first); }
	integer.sig = -1;
	{ Decl d = {.ty=&integer}; assert(sign(&d) == 0xffffffffUL); }
	return 0;
}
