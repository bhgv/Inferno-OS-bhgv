#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "raise.h"
#include <pthread.h>
#include <stdio.h>

#define streamtest(test) streampredicate(!!(test), __LINE__) // Preserve every check under NDEBUG.
char exNilref[] = "nil", exBounds[] = "bounds";
static char *expectederror;
static int frameoffset, fdoffset, phase, mutated, misread, miswrite, misat;
static int wantsrc = 17, wantdst = 18, srcsentinel, dstsentinel;
static WORD srcnumber = 17, dstnumber = 18;
static uchar *originalframe, *originalsrc, *originaldst;
static uchar frameimage[sizeof(F_Sys_stream)+32];
static uchar errorimage[32], errorslot[32];


/* Report an exact predicate location without assertion configuration dependence. */
static void
streampredicate(int valid, int line)
{
	if(!valid) {
		fprintf(stderr, "Sys stream predicate failed at %d phase=%d\n", line, phase);
		abort();
	}
}


/* Replace borrowed frame and descriptor fields at one observable callback boundary. */
static void
streamcallback(int point)
{
	WORD number = 99;

	if(phase != point || mutated || originalframe == nil)
		return;
	mutated = 1;
	memset(originalframe, 0xa7, sizeof(F_Sys_stream));
	memmove(originalsrc+O(Sys_FD, fd), &number, sizeof(number));
	memmove(originaldst+O(Sys_FD, fd), &number, sizeof(number));
}


char Ebadarg[] = "bad argument", Enomem[] = "out of memory";
static Prog process;
static int released, allocations, live, failalloc, errors, checks;
static int length, position, used, readchunk, writechunk, failread, failwrite, zerowrite;
static int reads, writes, killed, virtualio;
static uvlong virtualleft;
static uchar input[128], output[128];
static void *owner;
#define fdchk(x) ((x) == (Sys_FD*)H ? -1 : (x)->fd)


/* Explicit ownership checks replace unavailable LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Reject invalid VM spans before allocation, release or native I/O. */
void
error(char *message)
{
	if(message == expectederror) {
		streamtest(!released && !allocations && !live && !errors && !reads && !writes);
		streamtest(!memcmp(errorslot, errorimage, sizeof(errorslot)));
		printf("PASS Sys stream preflight %s\n", message);
		exit(0);
	}
	fprintf(stderr, "Unexpected stream VM error: %s\n", message);
	abort();
}


/* Native operations must observe a released interpreter. */
void
release(void)
{
	streamtest(!released);
	released = 1;
	streamcallback(2);
}


/* Callback changes must remain visible after reacquiring the interpreter. */
void
acquire(void)
{
	streamtest(released);
	streamcallback(5);
	released = 0;
}


/* Capture the running program while the interpreter is acquired. */
Prog*
currun(void)
{
	streamtest(!released);
	streamcallback(7);
	return &process;
}


/* Record ordinary argument errors without native side effects. */
void
seterror(char *s, ...)
{
	streamtest(!released && s == Ebadarg);
	errors++;
}


/* Preserve the native out-of-memory result for a failed temporary allocation. */
void
kwerrstr(char *s, ...)
{
	streamtest(!released && s == Enomem);
	errors++;
}


/* Allocate guarded production storage, including a virtual large-count mode. */
static void*
streamalloc(ulong size)
{
	streamcallback(1);
	allocations++;
	if(failalloc)
		return nil;
	streamtest(live == 0 && size > 0 && size <= 0x7fffffff);
	owner = malloc(virtualio ? 1 : size);
	streamtest(owner != nil);
	live++;
	return owner;
}


/* Every allocation must be released after reacquiring the VM lock. */
static void
streamfree(void *p)
{
	streamcallback(6);
	streamtest(!released && p == owner && live == 1);
	free(p);
	owner = nil;
	live--;
}


/* Fill exact requested spans, with deterministic fragmentation and read errors. */
long
kread(int fd, void *p, long n)
{
	int count;

	streamtest(released && fd == wantsrc && n > 0);
	reads++;
	streamcallback(3);
	if(misread && reads == misat)
		return misread == 1 ? n+1 : misread == 2 ? ((vlong)1<<32)+n : 9223372036854775807LL;
	if(virtualio){
		streamtest(p == owner);
		if(virtualleft == 0)
			return 0;
		count = n;
		if((uvlong)count > virtualleft)
			count = virtualleft;
		virtualleft -= count;
		return count;
	}
	if(failread >= 0 && position >= failread)
		return -1;
	count = length-position;
	if(count > n)
		count = n;
	if(readchunk && count > readchunk)
		count = readchunk;
	if(failread >= 0 && position+count > failread)
		count = failread-position;
	memmove(p, input+position, count);
	position += count;
	if(killed && reads == killed)
		process.kill = "stop";
	return count;
}


/* Acknowledge only a bounded prefix; errors must never replay earlier bytes. */
long
kwrite(int fd, void *p, long n)
{
	int count;

	streamtest(released && fd == wantdst && n > 0);
	writes++;
	streamcallback(4);
	if(miswrite && writes == misat)
		return miswrite == 1 ? n+1 : miswrite == 2 ? ((vlong)1<<32)+n : 9223372036854775807LL;
	if(virtualio){
		streamtest(p == owner);
		return n;
	}
	if(failwrite >= 0 && used >= failwrite)
		return zerowrite ? 0 : -1;
	count = n;
	if(writechunk && count > writechunk)
		count = writechunk;
	if(failwrite >= 0 && used+count > failwrite)
		count = failwrite-used;
	streamtest(used+count <= sizeof(output));
	memmove(output+used, p, count);
	used += count;
	return count;
}

#define malloc streamalloc
#define free streamfree
IMPLEMENTATION
#undef malloc
#undef free


/* Reset only the fixture state; production owns the temporary stream buffer. */
static void
setup(void)
{
	int i;

	streamtest(!released && live == 0);
	memset(&process, 0, sizeof(process));
	memset(output, 0xa5, sizeof(output));
	for(i = 0; i < sizeof(input); i++)
		input[i] = i*37+11;
	originalframe = nil;
	misread = miswrite = 0;
	misat = 1;
	expectederror = nil;
	allocations = failalloc = errors = reads = writes = position = used = 0;
	readchunk = writechunk = killed = virtualio = zerowrite = 0;
	failread = failwrite = -1;
}

static int returnoffset;


/* Enter through the actual generated builtin frame and native pointer ABI. */
static WORD
invoke(WORD size)
{
	F_Sys_stream f = {0};
	Sys_FD src, dst;
	uchar srcbytes[sizeof(Sys_FD)+8], dstbytes[sizeof(Sys_FD)+8];
	uchar packed[sizeof(F_Sys_stream)+32];
	WORD result = -77;
	union {
		uvlong align; // Align the backing store before applying result byte shifts.
		uchar bytes[32]; // Preserve canaries around the complete VM result cell.
	} slot;
	uchar expected[32];
	WORD *destination=(void*)(slot.bytes+8+returnoffset);

	memset(slot.bytes,0xa5,sizeof(slot.bytes));
	memmove(slot.bytes+8+returnoffset,&result,sizeof(result));
	memmove(expected,slot.bytes,sizeof(expected));
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	src.fd = srcnumber;
	dst.fd = dstnumber;
	originalsrc = srcbytes+fdoffset;
	originaldst = dstbytes+fdoffset;
	memmove(originalsrc, &src, sizeof(src));
	memmove(originaldst, &dst, sizeof(dst));
	f.src = srcsentinel ? (Sys_FD*)H : (Sys_FD*)originalsrc;
	f.dst = dstsentinel ? (Sys_FD*)H : (Sys_FD*)originaldst;
	f.bufsiz = size;
	f.ret = destination;
	memset(packed, 0x3c, sizeof(packed));
	originalframe = packed+8+frameoffset;
	memmove(originalframe, &f, sizeof(f));
	memmove(frameimage, packed, sizeof(packed));
	mutated = 0;
	Sys_stream(originalframe);
	if(mutated)
		memset(frameimage+8+frameoffset, 0xa7, sizeof(f));
	streamtest(!memcmp(packed, frameimage, sizeof(packed)));
	originalframe = nil;
	streamtest(!released && live == 0);
	memmove(&result,slot.bytes+8+returnoffset,sizeof(result));
	memmove(expected+8+returnoffset,&result,sizeof(result));
	streamtest(!memcmp(slot.bytes,expected,sizeof(expected)));
	return result;
}


/* Check all combinations of buffered read/write fragmentation against a byte oracle. */
static void
transfers(void)
{
	int size, len, r, w;

	for(size = 1; size <= 16; size++)
	for(len = 0; len <= 40; len++)
	for(r = 0; r <= 3; r++)
	for(w = 0; w <= 3; w++){
		setup();
		length = len;
		readchunk = r;
		writechunk = w;
		streamtest(invoke(size) == len && used == len);
		streamtest(memcmp(input, output, len) == 0);
		streamtest(output[len] == 0xa5 && allocations == 1);
		checks++;
	}
}


/* Fail writes in full and final buffers, retaining only acknowledged prefixes. */
static void
failures(void)
{
	int size, stop, zero;

	for(size = 1; size <= 16; size++)
	for(stop = 0; stop <= 32; stop++)
	for(zero = 0; zero <= 1; zero++){
		setup();
		length = 32;
		writechunk = 3;
		failwrite = stop;
		zerowrite = zero;
		streamtest(invoke(size) == stop && used == stop);
		streamtest(memcmp(input, output, stop) == 0 && output[stop] == 0xa5);
		checks++;
	}
	for(stop = 0; stop <= 32; stop++){
		setup();
		length = 32;
		failread = stop;
		readchunk = writechunk = 3;
		streamtest(invoke(8) == stop && used == stop);
		checks++;
	}
	setup();
	failalloc = 1;
	streamtest(invoke(8) == -1 && errors == 1 && reads == 0 && writes == 0);
	checks++;
	setup();
	length = 32;
	readchunk = writechunk = 3;
	killed = 2;
	streamtest(invoke(8) == 6 && used == 6 && reads == 2);
	checks++;
}


/* Reject invalid full-width sizes before allocation or native I/O. */
static void
bounds(void)
{
	WORD sizes[6];
	int i, count;

	sizes[0] = 0;
	sizes[1] = -1;
	sizes[2] = (WORD)(-0x7fffffff-1);
	count = 3;
	if(sizeof(WORD) == 8){
		sizes[count++] = (WORD)((uvlong)1<<32);
		sizes[count++] = (WORD)(((uvlong)1<<32)+8);
		sizes[count++] = (WORD)(~(UWORD)0>>1);
	}
	for(i = 0; i < count; i++){
		setup();
		streamtest(invoke(sizes[i]) == -1 && errors == 1 && allocations == 0 && reads == 0);
		checks++;
	}
}


/* Model multi-gigabyte acknowledgments without allocating or accessing a huge buffer. */
static void
counts(void)
{
	WORD result, expected;
	int blocks;

	for(blocks = 2; blocks <= 4; blocks++){
		setup();
		virtualio = 1;
		virtualleft = (uvlong)blocks*0x40000000;
		if(sizeof(WORD) == 8)
			expected = virtualleft;
		else
			expected = blocks == 2 ? -0x7fffffff-1 : blocks == 3 ? -0x40000000 : 0;
		result = invoke(0x40000000);
		streamtest(result == expected);
		streamtest(reads == blocks+1 && writes == blocks);
		checks++;
	}
}


/* Keep isolated regressions reproducible without allowing an empty selected matrix. */
static int
streamselected(char *name, int value)
{
	char *s = getenv(name);

	return s == nil || atoi(s) == value;
}


/* Check packed operands, byte-exact progress and oversized native transfer reports. */
static void
capturetransfers(void)
{
	int mode, expected, kind;
	WORD result;

	for(phase=0; phase<8; phase++)
	for(frameoffset=0; frameoffset<8; frameoffset++)
	for(fdoffset=0; fdoffset<8; fdoffset++)
	for(returnoffset=0; returnoffset<8; returnoffset++)
	for(mode=0; mode<20; mode++) {
		if(!streamselected("STREAM_PHASE", phase) || !streamselected("STREAM_FRAME", frameoffset) ||
		   !streamselected("STREAM_FD_SHIFT", fdoffset) || !streamselected("STREAM_MODE", mode))
			continue;
		setup();
		length = 32;
		readchunk = 2;
		writechunk = 3;
		expected = 32;
		if(mode == 1)length = expected = 5;
		if(mode == 2){ failread = 0; expected = 0; }
		if(mode == 3){ failread = 5; expected = 5; }
		if(mode == 4){ failwrite = 0; expected = 0; }
		if(mode == 5){ failwrite = 5; expected = 5; }
		if(mode == 6){ readchunk = 3; killed = 2; expected = 6; }
		if(mode == 7){ failalloc = 1; expected = -1; }
		if(mode >= 8) {
			kind = mode < 14 ? mode-8 : mode-14;
			misat = kind%2+1;
			if(mode < 14)misread = kind/2+1;
			else miswrite = kind/2+1;
			writechunk = 2;
			expected = misat == 1 ? 0 : 2;
		}
		result = invoke(8);
		streamtest(result == expected && used == (expected < 0 ? 0 : expected));
		streamtest(!memcmp(input, output, used) && output[used] == 0xa5);
		streamtest(errors == (mode == 7) && allocations == 1);
		checks++;
	}
}


/* Forward full signed native descriptors and reject high bits before allocation. */
static void
capturedescriptors(void)
{
	vlong numbers[] = {17, 0, -1, 0x7fffffff, -0x7fffffff-1, -1,
		2147483648LL, -2147483649LL, 4294967313LL, -1099511627776LL,
		9223372036854775807LL, -9223372036854775807LL-1};
	int target, index, valid;
	WORD result;

	for(target=0; target<2; target++)
	for(index=0; index<(sizeof(WORD)>4 ? 12 : 6); index++)
	for(frameoffset=0; frameoffset<8; frameoffset++)
	for(fdoffset=0; fdoffset<8; fdoffset++)
	for(returnoffset=0; returnoffset<8; returnoffset++) {
		if(!streamselected("STREAM_TARGET", target) || !streamselected("STREAM_NUMBER", index))
			continue;
		setup();
		phase = getenv("STREAM_PHASE") == nil ? 2 : atoi(getenv("STREAM_PHASE"));
		srcnumber = wantsrc = 17;
		dstnumber = wantdst = 18;
		srcsentinel = dstsentinel = 0;
		valid = index < 6;
		if(target == 0) {
			srcnumber = numbers[index];
			srcsentinel = index == 5;
			if(valid)wantsrc = numbers[index];
		} else {
			dstnumber = numbers[index];
			dstsentinel = index == 5;
			if(valid)wantdst = numbers[index];
		}
		length = 17;
		readchunk = 2;
		writechunk = 3;
		result = invoke(8);
		streamtest(result == (valid ? 17 : -1) && used == (valid ? 17 : 0));
		streamtest(errors == !valid && allocations == valid);
		if(!valid)streamtest(!reads && !writes);
		streamtest(!memcmp(input, output, used) && output[used] == 0xa5);
		checks++;
	}
}


/* Isolate VM preflight so expected errors can terminate without native side effects. */
static void
streampreflight(int mode)
{
	F_Sys_stream f = {0};
	Sys_FD src = {17}, dst = {18};
	void *fp = &f;

	setup();
	memset(errorslot, 0xa5, sizeof(errorslot));
	memmove(errorimage, errorslot, sizeof(errorimage));
	f.ret = (void*)(errorslot+8);
	f.src = &src;
	f.dst = &dst;
	f.bufsiz = 8;
	if(mode == 0)fp = nil;
	if(mode == 1)fp = H;
	if(mode == 2)fp = (void*)((uintptr)-1-1);
	if(mode == 3)f.ret = nil;
	if(mode == 4)f.ret = H;
	if(mode == 5)f.ret = (void*)((uintptr)-1-sizeof(WORD)+1);
	if(mode == 6)f.src = nil;
	if(mode == 7)f.src = (Sys_FD*)((uintptr)-1-1);
	if(mode == 8)f.dst = nil;
	if(mode == 9)f.dst = (Sys_FD*)((uintptr)-1-1);
	expectederror = mode == 2 || mode == 5 || mode == 7 || mode == 9 ? exBounds : exNilref;
	Sys_stream(fp);
	streamtest(0);
}


/* Keep baseline selectors independent so each old behavior is demonstrable. */
static int
streammatrix(int argc, char **argv)
{
	if(argc > 1 && !strcmp(argv[1], "preflight"))
		streampreflight(atoi(argv[2]));
	if(argc > 1 && !strcmp(argv[1], "capture"))
		capturetransfers();
	else if(argc > 1 && !strcmp(argv[1], "nativefd"))
		capturedescriptors();
	else for(returnoffset=0;returnoffset<8;returnoffset++){
		if(argc == 1 || strcmp(argv[1], "transfer") == 0)
			transfers();
		if(argc == 1 || strcmp(argv[1], "failure") == 0)
			failures();
		if(argc == 1 || strcmp(argv[1], "bounds") == 0)
			bounds();
		if(argc == 1 || strcmp(argv[1], "count") == 0)
			counts();
	}
	streamtest(checks != 0);
	printf("Sys.stream WORD%u: %d transfer/count/ownership cases passed\n", (unsigned)(8*sizeof(WORD)), checks);
	return 0;
}

/* Keep command-line selectors stable while moving sanitizer work to a tracked stack. */
struct StreamArguments {
	int argc; // Number of fixture arguments.
	char **argv; // Selected baseline or extended stream family.
};


/* Run stream tests without relying on unavailable process-stack discovery. */
static void*
streamworker(void *argument)
{
	struct StreamArguments *args = argument;

	streamtest(streammatrix(args->argc, args->argv) == 0);
	return nil;
}


/* Join before reporting success so all buffers and owner checks complete. */
int
main(int argc, char **argv)
{
	struct StreamArguments args;
	pthread_t thread;

	args.argc = argc;
	args.argv = argv;
	streamtest(pthread_create(&thread, nil, streamworker, &args) == 0);
	streamtest(pthread_join(thread, nil) == 0);
	return 0;
}
