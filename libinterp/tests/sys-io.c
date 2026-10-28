#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "raise.h"
#include <stdio.h>
#include <pthread.h>


/* Keep every check active when the production unit is built with NDEBUG. */
static void
iofail(int test, int line)
{
	if(!test) {
		fprintf(stderr, "Sys I/O predicate failed at %d\n", line);
		abort();
	}
}
#define iocheck(test) iofail(!!(test), __LINE__) // Preserve the failing predicate location.
char exNilref[] = "nil", exBounds[] = "bounds";



Type Tarray = {1}, Tbyte = {1}, other = {1};
char Ebadarg[] = "bad argument";
static int released, entered, calls, total, nativecount, failafter, chunk, checks, bad;
static vlong nativeoff;
static uchar *base;
static uchar *borrowedframe;
static uintptr borrowedsize;
static Array *borrowedarray;
static Sys_FD *borrowedfd;
static int borrowphase, mutated;
static WORD fdnumber;
static int wantfd, sentinel;
static char *expectederror;
static Heap *errorowner;
static WORD errorresult;


/* Unexpected VM errors must fail the ordinary I/O matrix. */
void
error(char *message)
{
	if(expectederror == message) {
		iocheck(!released && !entered && !calls && !bad);
		iocheck(errorresult == -77);
		free(errorowner);
		errorowner = nil;
		printf("PASS Sys I/O rejected %s before native side effects\n", message);
		exit(0);
	}
	fprintf(stderr, "Unexpected VM error: %s\n", message);
	abort();
}


/* Model callbacks replacing borrowed frame, array and FD fields without rollback. */
static void
borrow(void)
{
	uchar *replacement = (uchar*)H;
	WORD number = 99;

	if(mutated)
		return;
	mutated = 1;
	memset(borrowedframe, 0xa7, borrowedsize);
	memmove((uchar*)borrowedarray+O(Array, data), &replacement, sizeof(replacement));
	memmove((uchar*)borrowedfd+O(Sys_FD, fd), &number, sizeof(number));
}
#define fdchk(x) ((x) == (Sys_FD*)H ? -1 : (x)->fd) // Retain the old source oracle macro.


/* The sandbox cannot provide LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Model the scheduler boundary and mutate only at the requested callback phase. */
void
release(void)
{
	iocheck(!released);
	released = 1;
	entered++;
	if(borrowphase == 1)
		borrow();
}


/* Require exactly one matching acquire after native I/O completes. */
void
acquire(void)
{
	iocheck(released);
	released = 0;
}


/* Record normal Sys error returns without throwing a VM exception. */
void
seterror(char *message, ...)
{
	iocheck(message == Ebadarg);
	bad++;
}


/* Simulate short reads and errors while checking exact native spans and offsets. */
static long
transfer(int fd, void *data, long count, vlong offset)
{
	int n;

	iocheck(released && fd == wantfd && count >= 0 && count <= 16);
	if(base == nil)
		iocheck(data == nil && count == 0);
	else
		iocheck((uchar*)data == base+total);
	if(borrowphase == 2)borrow();
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


/* Observe the exact kread arguments without performing filesystem I/O. */
long
kread(int fd, void *p, long n)
{
	return transfer(fd, p, n, 0);
}


/* Observe the exact kwrite arguments without performing filesystem I/O. */
long
kwrite(int fd, void *p, long n)
{
	return transfer(fd, p, n, 0);
}


/* Observe the exact kpread arguments without performing filesystem I/O. */
long
kpread(int fd, void *p, long n, vlong off)
{
	return transfer(fd, p, n, off);
}


/* Observe the exact kpwrite arguments without performing filesystem I/O. */
long
kpwrite(int fd, void *p, long n, vlong off)
{
	return transfer(fd, p, n, off);
}

IMPLEMENTATION

static int returnoffset, frameoffset;


/* Invoke all five production builtins using actual generated frame declarations. */
static WORD
invoke(int op, Array *a, WORD count)
{
	Sys_FD fd;
	uchar fdbytes[sizeof(Sys_FD)+8];
	Sys_FD *packedfd = (Sys_FD*)(fdbytes+frameoffset);
	union {
		uvlong align; // Align the frame backing store before applying byte shifts.
		uchar bytes[sizeof(F_Sys_pread)+32]; // Leave room for the largest fixed frame.
	} packed;
	void *original = nil;
	uintptr size = 0;
	WORD result = -77;
	union {
		uvlong align; // Align the result backing store before applying byte shifts.
		uchar bytes[32]; // Keep canaries around every WORD result cell.
	} slot;
	uchar expected[32];
	WORD *destination=(void*)(slot.bytes+8+returnoffset);
	F_Sys_read r = {0}; F_Sys_readn rn = {0}; F_Sys_write w = {0};
	F_Sys_pread pr = {0}; F_Sys_pwrite pw = {0};
	vlong offset = ((vlong)1<<40)+123;

	fd.fd = fdnumber;
	memmove(packedfd, &fd, sizeof(fd));
	memset(slot.bytes,0xa5,sizeof(slot.bytes));
	memmove(slot.bytes+8+returnoffset,&result,sizeof(result));
	memmove(expected,slot.bytes,sizeof(expected));
	r.fd = rn.fd = w.fd = pr.fd = pw.fd = sentinel ? (Sys_FD*)H : packedfd;
	r.ret = rn.ret = w.ret = pr.ret = pw.ret = destination;
	r.buf = rn.buf = w.buf = pr.buf = pw.buf = a;
	r.n = rn.n = w.n = pr.n = pw.n = count;
	pr.off = pw.off = offset;
	switch(op){
	case 0: original=&r; size=sizeof(r); break;
	case 1: original=&rn; size=sizeof(rn); break;
	case 2: original=&w; size=sizeof(w); break;
	case 3: original=&pr; size=sizeof(pr); break;
	case 4: original=&pw; size=sizeof(pw); break;
	}
	borrowedframe = packed.bytes+frameoffset;
	borrowedsize = size;
	borrowedarray = a;
	borrowedfd = packedfd;
	mutated = 0;
	memmove(borrowedframe, original, size);
	switch(op) {
	case 0: Sys_read(borrowedframe); break;
	case 1: Sys_readn(borrowedframe); break;
	case 2: Sys_write(borrowedframe); break;
	case 3: Sys_pread(borrowedframe); break;
	case 4: Sys_pwrite(borrowedframe); break;
	}
	if(mutated) {
		uintptr i;
		for(i=0; i<size; i++)
			iocheck(borrowedframe[i] == 0xa7);
	}
	iocheck(!released);
	if(calls && op >= 3)iocheck(nativeoff == offset);
	memmove(&result,slot.bytes+8+returnoffset,sizeof(result));
	memmove(expected+8+returnoffset,&result,sizeof(result));
	iocheck(!memcmp(slot.bytes,expected,sizeof(expected)));
	return result;
}


/* Run one deliberately invalid operand in a fresh process, with no native entry. */
static void
preflight(int key)
{
	union {
		F_Sys_read read; // Provide native alignment and the largest fixed frame.
		F_Sys_readn readn; // Retain the actual readn declaration.
		F_Sys_write write; // Retain the actual write declaration.
		F_Sys_pread pread; // Include the full positional offset.
		F_Sys_pwrite pwrite; // Include the full positional offset.
	} frame;
	void *fp, *ret;
	Sys_FD *fd;
	int op, mode;

	op = key/8;
	mode = key%8;
	memset(&frame, 0, sizeof(frame));
	fp = &frame;
	errorresult = -77;
	ret = &errorresult;
	fd = nil;
	if(mode == 0)fp = nil;
	if(mode == 1)fp = H;
	if(mode == 2)fp = (void*)((uintptr)-1-7);
	if(mode == 3)ret = nil;
	if(mode == 4)ret = H;
	if(mode == 5)ret = (void*)((uintptr)-1-1);
	if(mode == 7)fd = (Sys_FD*)((uintptr)-1-1);
	frame.read.ret = ret;
	frame.read.fd = fd;
	/* An empty valid array reaches FD validation without touching payload bytes. */
	{
		Heap *h = calloc(1, sizeof(Heap)+sizeof(Array));
		Array *a;

		iocheck(h != nil);
		errorowner = h;
		h->t = &Tarray;
		a = H2D(Array*, h);
		a->t = &Tbyte;
		frame.read.buf = a;
	}
	expectederror = (mode == 2 || mode == 5 || mode == 7) ? exBounds : exNilref;
	switch(op) {
	case 0: Sys_read(fp); break;
	case 1: Sys_readn(fp); break;
	case 2: Sys_write(fp); break;
	case 3: Sys_pread(fp); break;
	case 4: Sys_pwrite(fp); break;
	}
	iocheck(0);
}


/* Check full-width clamping, no-op conventions, and short-read accumulation. */
static void*
runmatrix(void *unused)
{
	Heap *h;
	Array *a, *arg;
	uchar data[16];
	WORD count, result;
	int op, mode, expected, arrayshift, phase;
	Heap owner;
	Array view;

	USED(unused);
	if(getenv("SYS_IO_ERROR") != nil)
		preflight(atoi(getenv("SYS_IO_ERROR")));
	h = calloc(1, sizeof(Heap)+sizeof(Array)+8);
	iocheck(h != nil);
	memset(&owner, 0, sizeof(owner));
	owner.t = &Tarray;
	base = data;
	for(arrayshift=0; arrayshift<8; arrayshift++)
	for(frameoffset=0; frameoffset<8; frameoffset++)
	for(phase=0; phase<3; phase++)
	for(returnoffset=0; returnoffset<8; returnoffset++)
	for(op=0; op<5; op++)
	for(mode=0; mode<30; mode++){
		if(getenv("SYS_IO_ARRAY_SHIFT") != nil && arrayshift != atoi(getenv("SYS_IO_ARRAY_SHIFT")))
			continue;
		if(getenv("SYS_IO_FRAME_SHIFT") != nil && frameoffset != atoi(getenv("SYS_IO_FRAME_SHIFT")))
			continue;
		if(getenv("SYS_IO_PHASE") != nil && phase != atoi(getenv("SYS_IO_PHASE")))
			continue;
		if(getenv("SYS_IO_RESULT_SHIFT") != nil && returnoffset != atoi(getenv("SYS_IO_RESULT_SHIFT")))
			continue;
		if(getenv("SYS_IO_OP") != nil && op != atoi(getenv("SYS_IO_OP")))
			continue;
		if(getenv("SYS_IO_MODE") != nil && mode != atoi(getenv("SYS_IO_MODE")))
			continue;
		a = (Array*)((uchar*)h+sizeof(Heap)+arrayshift);
		memmove((uchar*)a-sizeof(Heap), &owner, sizeof(owner));
		memset(&view, 0, sizeof(view));
		view.t = &Tbyte; view.len = 16; view.data = data;
		borrowphase = phase;
		fdnumber = 17;
		wantfd = 17;
		sentinel = 0;
		arg = a; count = 8; expected = 8;
		calls = total = nativecount = entered = bad = 0;
		failafter = -1; chunk = 0;
		switch(mode) {
		case 0:
			count = -1;
			expected = 0;
			break;
		case 1:
			count = sizeof(WORD)>4 ? (WORD)((uvlong)1<<32) : 0x7fffffff;
			expected = 16;
			break;
		case 2:
			count = sizeof(WORD)>4 ? (WORD)(((uvlong)1<<32)+3) : 0x7fffffff;
			expected = 16;
			break;
		case 3:
			arg = H;
			expected = 0;
			break;
		case 4:
			arg = nil;
			expected = -1;
			break;
		case 5:
			view.len = -1;
			expected = -1;
			break;
		case 6:
			view.len = sizeof(WORD)>4 ? (WORD)((uvlong)1<<32) : -1;
			expected = -1;
			break;
		case 7:
			view.t = &other;
			expected = -1;
			break;
		case 8:
			owner.t = &other;
			memmove((uchar*)a-sizeof(Heap), &owner, sizeof(owner));
			owner.t = &Tarray;
			expected = -1;
			break;
		case 9:
			view.data = nil;
			expected = -1;
			break;
		case 10:
			count = 0;
			expected = 0;
			break;
		case 11:
			view.len = 0;
			view.data = nil;
			base = nil;
			expected = 0;
			break;
		case 12:
			chunk = 3;
			expected = op == 1 ? 8 : 3;
			break;
		case 13:
			failafter = 0;
			expected = -1;
			break;
		case 14:
			chunk = 3;
			failafter = 3;
			expected = 3;
			break;
		case 15:
			count = 17;
			expected = 16;
			break;
		case 16:
			count = sizeof(WORD)>4 ? -(WORD)((uvlong)1<<32) : -2;
			expected = 0;
			break;
		case 17:
			count = sizeof(WORD)>4 ? -(WORD)(((uvlong)1<<32)-3) : -3;
			expected = 0;
			break;
		case 18:
			view.data = H;
			expected = -1;
			break;
		case 19:
			view.data = (uchar*)((uintptr)-1-7);
			expected = -1;
			break;
		case 20:
			arg = (Array*)1;
			expected = -1;
			break;
		case 21:
			arg = (Array*)((uintptr)-1-7);
			expected = -1;
			break;
		case 22:
			if(sizeof(WORD)>4) {
				fdnumber = (WORD)(((uvlong)1<<32)+17);
				expected = -1;
			}
			break;
		case 23:
			if(sizeof(WORD)>4) {
				fdnumber = -(WORD)((uvlong)1<<40);
				expected = -1;
			}
			break;
		case 24:
			fdnumber = wantfd = 0;
			break;
		case 25:
			fdnumber = wantfd = -1;
			break;
		case 26:
			fdnumber = wantfd = 0x7fffffff;
			break;
		case 27:
			fdnumber = wantfd = -0x7fffffff-1;
			break;
		case 28:
			sentinel = 1;
			wantfd = -1;
			break;
		case 29:
			arg = (Array*)sizeof(Heap);
			expected = -1;
			break;
		}
		memmove(a, &view, sizeof(view));
		result = invoke(op, arg, count);
		iocheck(result == expected);
		if(mode == 0 || mode == 3 || mode == 16 || mode == 17)iocheck(!calls && !entered && !bad);
		if(mode >= 4 && mode <= 9)iocheck(!calls && !entered && bad == 1);
		if(mode == 29)iocheck(!calls && !entered && bad == 1);
		if(mode >= 18 && mode <= 23 && (mode <= 21 || sizeof(WORD)>4))iocheck(!calls && !entered && bad == 1);
		if(mode == 12 && op == 1)iocheck(calls == 3 && total == 8);
		if(mode == 14 && op == 1)iocheck(calls == 2 && total == 3);
		base = data;
		checks++;
	}
	iocheck(checks != 0);
	free(h);
	printf("PASS Sys I/O: %d count/metadata/short-read cases, WORD%zu\n", checks, sizeof(WORD)*8);
	return nil;
}


/* Give ASan a tracked thread stack even when process-stack discovery is unavailable. */
int
main(void)
{
	pthread_t thread;

	iocheck(pthread_create(&thread, nil, runmatrix, nil) == 0);
	iocheck(pthread_join(thread, nil) == 0);
	return 0;
}
