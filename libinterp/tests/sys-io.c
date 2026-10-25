#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include <assert.h>
#include <stdio.h>

Type Tarray = {1}, Tbyte = {1}, other = {1};
char Ebadarg[] = "bad argument";
static int released, entered, calls, total, nativecount, failafter, chunk, checks, bad;
static vlong nativeoff;
static uchar *base;
#define fdchk(x) ((x) == (Sys_FD*)H ? -1 : (x)->fd)

/* The sandbox cannot provide LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Model the scheduler boundary and require every native call to run released. */
void release(void) { assert(!released); released = 1; entered++; }
void acquire(void) { assert(released); released = 0; }
/* Record normal Sys error returns without throwing a VM exception. */
void seterror(char *message, ...) { assert(message == Ebadarg); bad++; }

/* Simulate short reads and errors while checking exact native spans and offsets. */
static long
transfer(int fd, void *data, long count, vlong offset)
{
	int n;

	assert(released && fd == 17 && count >= 0 && count <= 16);
	if(base == nil)
		assert(data == nil && count == 0);
	else
		assert((uchar*)data == base+total);
	nativecount = count;
	nativeoff = offset;
	calls++;
	if(failafter >= 0 && total >= failafter)
		return -1;
	n = count;
	if(chunk && n > chunk)n = chunk;
	total += n;
	return n;
}
long kread(int fd, void *p, long n) { return transfer(fd, p, n, 0); }
long kwrite(int fd, void *p, long n) { return transfer(fd, p, n, 0); }
long kpread(int fd, void *p, long n, vlong off) { return transfer(fd, p, n, off); }
long kpwrite(int fd, void *p, long n, vlong off) { return transfer(fd, p, n, off); }

IMPLEMENTATION

static int returnoffset;

/* Invoke all five production builtins using actual generated frame declarations. */
static WORD
invoke(int op, Array *a, WORD count)
{
	Sys_FD fd = {17};
	WORD result = -77;
	union { uvlong align; uchar bytes[32]; } slot;
	uchar expected[32];
	WORD *destination=(void*)(slot.bytes+8+returnoffset);
	F_Sys_read r = {0}; F_Sys_readn rn = {0}; F_Sys_write w = {0};
	F_Sys_pread pr = {0}; F_Sys_pwrite pw = {0};
	vlong offset = ((vlong)1<<40)+123;

	memset(slot.bytes,0xa5,sizeof(slot.bytes));
	memmove(slot.bytes+8+returnoffset,&result,sizeof(result));
	memmove(expected,slot.bytes,sizeof(expected));
	r.fd = rn.fd = w.fd = pr.fd = pw.fd = &fd;
	r.ret = rn.ret = w.ret = pr.ret = pw.ret = destination;
	r.buf = rn.buf = w.buf = pr.buf = pw.buf = a;
	r.n = rn.n = w.n = pr.n = pw.n = count;
	pr.off = pw.off = offset;
	switch(op){
	case 0: Sys_read(&r); break;
	case 1: Sys_readn(&rn); break;
	case 2: Sys_write(&w); break;
	case 3: Sys_pread(&pr); break;
	case 4: Sys_pwrite(&pw); break;
	}
	assert(!released);
	if(calls && op >= 3)assert(nativeoff == offset);
	memmove(&result,slot.bytes+8+returnoffset,sizeof(result));
	memmove(expected+8+returnoffset,&result,sizeof(result));
	assert(!memcmp(slot.bytes,expected,sizeof(expected)));
	return result;
}

/* Check full-width clamping, no-op conventions, and short-read accumulation. */
int
main(void)
{
	Heap *h;
	Array *a, *arg;
	uchar data[16];
	WORD count, result;
	int op, mode, expected;

	h = calloc(1, sizeof(Heap)+sizeof(Array)); assert(h);
	a = H2D(Array*, h);
	base = data;
	for(returnoffset=0;returnoffset<8;returnoffset++)for(op = 0; op < 5; op++) for(mode = 0; mode < 18; mode++){
		h->t = &Tarray; a->t = &Tbyte; a->len = 16; a->data = data;
		arg = a; count = 8; expected = 8;
		calls = total = nativecount = entered = bad = 0;
		failafter = -1; chunk = 0;
		if(mode == 0){ count = -1; expected = 0; }
		if(mode == 1){ count = sizeof(WORD)>4 ? (WORD)((uvlong)1<<32) : 0x7fffffff; expected = 16; }
		if(mode == 2){ count = sizeof(WORD)>4 ? (WORD)(((uvlong)1<<32)+3) : 0x7fffffff; expected = 16; }
		if(mode == 3){ arg = H; expected = 0; }
		if(mode == 4){ arg = nil; expected = -1; }
		if(mode == 5){ a->len = -1; expected = -1; }
		if(mode == 6){ a->len = sizeof(WORD)>4 ? (WORD)((uvlong)1<<32) : -1; expected = -1; }
		if(mode == 7){ a->t = &other; expected = -1; }
		if(mode == 8){ h->t = &other; expected = -1; }
		if(mode == 9){ a->data = nil; expected = -1; }
		if(mode == 10){ count = 0; expected = 0; }
		if(mode == 11){ a->len = 0; a->data = nil; base = nil; expected = 0; }
		if(mode == 12){ chunk = 3; expected = op == 1 ? 8 : 3; }
		if(mode == 13){ failafter = 0; expected = -1; }
		if(mode == 14){ chunk = 3; failafter = 3; expected = 3; }
		if(mode == 15){ count = 17; expected = 16; }
		if(mode == 16){ count = sizeof(WORD)>4 ? -(WORD)((uvlong)1<<32) : -2; expected = 0; }
		if(mode == 17){ count = sizeof(WORD)>4 ? -(WORD)(((uvlong)1<<32)-3) : -3; expected = 0; }
		result = invoke(op, arg, count);
		assert(result == expected);
		if(mode == 0 || mode == 3 || mode == 16 || mode == 17)assert(!calls && !entered && !bad);
		if(mode >= 4 && mode <= 9)assert(!calls && !entered && bad == 1);
		if(mode == 12 && op == 1)assert(calls == 3 && total == 8);
		if(mode == 14 && op == 1)assert(calls == 2 && total == 3);
		base = data; checks++;
	}
	free(h);
	printf("PASS Sys I/O: %d count/metadata/short-read cases, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
