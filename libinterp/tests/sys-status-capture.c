#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "raise.h"
#include <stdio.h>
#include <pthread.h>

#define check(test) predicate(!!(test), __LINE__) // Keep every check active with NDEBUG.
#define fdchk(handle) ((handle) == (Sys_FD*)H ? -1 : (handle)->fd) // Replay the parent implementation.
#define TK2MS(ticks) osmillisec() // Observe the kernel clock through the same controlled boundary.

char exNilref[] = "nil", exBounds[] = "bounds", exHeap[] = "heap";
char Ebadarg[] = "argument";
static int operation, phase, mutated, released, entries, calls, bad, conversions;
static int sleepentered;
static WORD wantperiod;
static Proc sleepworker;
static int wantfd, wantafd, wantold, wantnew, wantflags, wantstart, nativeint, failallocation, allocations, frees;
static vlong nativebig, wantoff;
static uchar *originalframe;
static uintptr framesize;
static Sys_FD *originalfd, *originalafd;
static String strings[3];
static Dir *packed;
static char *expectederror;
static uchar resultslot[40], originalresult[40];


/* Report the exact failing predicate independently of assertion build options. */
static void
predicate(int valid, int line)
{
	if(!valid) {
		fprintf(stderr, "Sys capture predicate failed at %d op=%d phase=%d\n", line, operation, phase);
		abort();
	}
}


/* Keep sanitizer instrumentation when leak-process discovery is unavailable. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Preserve callback changes instead of restoring the caller's original frame. */
static void
callback(int point)
{
	WORD number = 99;

	if(phase != point || mutated)
		return;
	mutated = 1;
	memset(originalframe, 0xa7, framesize);
	memmove((uchar*)originalfd+O(Sys_FD, fd), &number, sizeof(number));
	memmove((uchar*)originalafd+O(Sys_FD, fd), &number, sizeof(number));
}


/* VM preflight errors must occur before native entry or result publication. */
void
error(char *message)
{
	if(message == expectederror) {
		check(!released && !entries && !calls && !bad && !allocations && !frees);
		check(!memcmp(resultslot, originalresult, sizeof(resultslot)));
		check(conversions == (failallocation ? 4 : 0));
		printf("PASS Sys capture preflight %s op=%d\n", message, operation);
		exit(0);
	}
	fprintf(stderr, "Unexpected VM error: %s\n", message);
	abort();
}


/* Record normal native-argument rejection while retaining the interpreter lock. */
void
seterror(char *message, ...)
{
	check(message == Ebadarg && !released);
	bad++;
}


/* Mutate borrowed operands exactly at the scheduler release boundary. */
void
release(void)
{
	check(!released && !entries);
	released = 1;
	entries++;
	callback(1);
}


/* Observe callbacks after result publication and require a matching native entry. */
void
acquire(void)
{
	check(released && calls == (operation == 11 && wantperiod <= 0 ? 0 : 1) && !sleepentered);
	callback(5);
	released = 0;
}


/* Model allocation-capable string conversion with stable borrowed native strings. */
char*
string2c(String *s)
{
	check(released || operation == 8 || operation == 9);
	conversions++;
	callback(2);
	if(s == &strings[0])
		return "first";
	if(s == &strings[1])
		return "second";
	check(s == &strings[2]);
	return "spec";
}


/* Exercise the real packdir allocation boundary, including its failure path. */
void*
smalloc(size_t bytes)
{
	check(!released && (operation == 8 || operation == 9) && !allocations);
	callback(4);
	if(failallocation)
		error(exHeap);
	packed = malloc(bytes);
	check(packed != nil);
	allocations++;
	return packed;
}


/* Require the one packed directory allocation to be released after native use. */
static void
freepacked(void *p)
{
	check(!released && p == packed && allocations == 1 && frees == 0);
	frees++;
	free(p);
	packed = nil;
}


/* Check directory values produced by the real packer, including wide lengths. */
static void
checkdirectory(Dir *d)
{
	check(d == packed && allocations == 1 && frees == 0);
	check(!strcmp(d->name, "first") && !strcmp(d->uid, "second"));
	check(!strcmp(d->gid, "spec") && !strcmp(d->muid, "first"));
	check(d->qid.path == ((uvlong)1<<40)+19 && d->qid.vers == 123 && d->qid.type == 7);
	check(d->mode == 511 && d->atime == 17 && d->mtime == 29);
	check(d->length == 9223372036854775798LL && d->type == 5 && d->dev == 6);
}


/* Require the intended native operation and preserve its callbacks visibly. */
static void
nativecall(int op)
{
	check(operation == op && !calls && released == (op != 10));
	calls++;
	callback(3);
}

/* Route the kernel wait queue through a stable worker without starting a scheduler. */
Proc*
getup(void)
{
	return &sleepworker;
}


/* Observe entry to the emulator's native sleep adapter. */
void
osenter(void)
{
	check(released && !sleepentered);
	sleepentered = 1;
}


/* Leave the adapter before the wrapper reacquires its interpreter. */
void
osleave(void)
{
	check(released && sleepentered);
	sleepentered = 0;
}


/* Supply the existing kernel wait predicate without actually delaying execution. */
int
return0(void *arg)
{
	USED(arg);
	return 0;
}


/* Verify the complete kernel sleep operand at its native boundary. */
void
tsleep(Rendez *queue, int (*waitpredicate)(void*), void *arg, int period)
{
	nativecall(11);
	check(queue == &sleepworker.sleep && waitpredicate == return0 && arg == nil && period == wantperiod);
}

#undef waserror
#define waserror() 0 // Native wait errors are covered separately by scheduler tests.
#undef poperror
#define poperror() ((void)0) // The controlled wait does not install a runtime frame.

NATIVE_MOCKS

#define free(p) freepacked(p) // Observe only production directory cleanup.
IMPLEMENTATION
#undef free

/* Keep all generated frames aligned while preparing their packed byte copies. */
union StatusFrame {
	F_Sys_dup dup; // Native descriptor duplication arguments.
	F_Sys_remove remove; // Single path argument.
	F_Sys_seek seek; // Full 64-bit seek offset and result.
	F_Sys_unmount unmount; // Two path arguments.
	F_Sys_chdir chdir; // Working-directory path.
	F_Sys_mount mount; // Two descriptors, flags and two strings.
	F_Sys_bind bind; // Source, destination and flags.
	F_Sys_iounit iounit; // Descriptor query.
	F_Sys_wstat wstat; // Path and complete directory value.
	F_Sys_fwstat fwstat; // Descriptor and complete directory value.
	F_Sys_millisec millisec; // Scalar clock result.
	F_Sys_sleep sleep; // Signed native sleep period.
	F_Sys_export exportframe; // Export descriptor, directory and masked flags.
};

static union StatusFrame frame;
static uchar framebytes[sizeof(frame)+32], frameimage[sizeof(frame)+32];
static uchar fdbytes[sizeof(Sys_FD)+8], afdbytes[sizeof(Sys_FD)+8];
static uintptr retoffset, fdoffset, afdoffset, resultwidth;
static void (*invoke)(void*);

#define selectframe(member, name) do { \
	framesize = sizeof(frame.member); \
	retoffset = O(F_Sys_##name, ret); \
	resultwidth = sizeof(*frame.member.ret); \
	invoke = Sys_##name; \
} while(0) // Derive each layout from its own generated declaration.


/* Populate every generated frame and directory with independent expected operands. */
static void
prepare(int frameshift, int resultshift, int fdshift)
{
	Sys_Dir directory;
	Sys_FD handle;
	void *ret;

	memset(&frame, 0, sizeof(frame));
	memset(framebytes, 0x3c, sizeof(framebytes));
	memset(fdbytes, 0x5b, sizeof(fdbytes));
	memset(afdbytes, 0x6d, sizeof(afdbytes));
	memset(resultslot, 0xa5, sizeof(resultslot));
	memmove(originalresult, resultslot, sizeof(resultslot));
	originalfd = (Sys_FD*)(fdbytes+fdshift);
	originalafd = (Sys_FD*)(afdbytes+fdshift);
	memset(&handle, 0, sizeof(handle));
	handle.fd = 17;
	memmove(originalfd, &handle, sizeof(handle));
	handle.fd = -1;
	memmove(originalafd, &handle, sizeof(handle));
	memset(&directory, 0, sizeof(directory));
	directory.name = &strings[0];
	directory.uid = &strings[1];
	directory.gid = &strings[2];
	directory.muid = &strings[0];
	directory.qid.path = ((uvlong)1<<40)+19;
	directory.qid.vers = 123;
	directory.qid.qtype = 7;
	directory.mode = 511;
	directory.atime = 17;
	directory.mtime = 29;
	directory.length = 9223372036854775798LL;
	directory.dtype = 5;
	directory.dev = 6;
	frame.dup.old = 17;
	frame.dup.new = 18;
	switch(operation) {
	case 0:
		selectframe(dup, dup);
		break;
	case 1:
		frame.remove.s = &strings[0];
		selectframe(remove, remove);
		break;
	case 2:
		frame.seek.fd = originalfd;
		frame.seek.off = wantoff;
		frame.seek.start = 1;
		selectframe(seek, seek);
		break;
	case 3:
		frame.unmount.s1 = &strings[0];
		frame.unmount.s2 = &strings[1];
		selectframe(unmount, unmount);
		break;
	case 4:
		frame.chdir.path = &strings[0];
		selectframe(chdir, chdir);
		break;
	case 5:
		frame.mount.fd = originalfd;
		frame.mount.afd = originalafd;
		frame.mount.on = &strings[1];
		frame.mount.spec = &strings[2];
		frame.mount.flags = 3;
		selectframe(mount, mount);
		break;
	case 6:
		frame.bind.s = &strings[0];
		frame.bind.on = &strings[1];
		frame.bind.flags = 3;
		selectframe(bind, bind);
		break;
	case 7:
		frame.iounit.fd = originalfd;
		selectframe(iounit, iounit);
		break;
	case 8:
		frame.wstat.s = &strings[0];
		frame.wstat.d = directory;
		selectframe(wstat, wstat);
		break;
	case 9:
		frame.fwstat.fd = originalfd;
		frame.fwstat.d = directory;
		selectframe(fwstat, fwstat);
		break;
	case 10:
		selectframe(millisec, millisec);
		break;
	case 11:
		frame.sleep.period = 3;
		selectframe(sleep, sleep);
		break;
	case 12:
		frame.exportframe.c = originalfd;
		frame.exportframe.dir = &strings[0];
		frame.exportframe.flag = 3;
		selectframe(exportframe, export);
		break;
	}
	fdoffset = 0;
	afdoffset = 0;
	if(operation == 2)fdoffset = O(F_Sys_seek, fd);
	if(operation == 5) {
		fdoffset = O(F_Sys_mount, fd);
		afdoffset = O(F_Sys_mount, afd);
	}
	if(operation == 7)fdoffset = O(F_Sys_iounit, fd);
	if(operation == 12)fdoffset = O(F_Sys_export, c);
	if(operation == 9)fdoffset = O(F_Sys_fwstat, fd);
	originalframe = framebytes+8+frameshift;
	ret = resultslot+8+resultshift;
	memmove(originalframe, &frame, framesize);
	memmove(originalframe+retoffset, &ret, sizeof(ret));
	memmove(frameimage, framebytes, sizeof(framebytes));
	released = entries = calls = bad = conversions = 0;
	mutated = allocations = frees = failallocation = 0;
	wantold = 17;
	wantnew = 18;
	wantflags = 3;
	wantstart = 1;
	wantperiod = 3;
	sleepentered = 0;
	wantfd = 17;
	wantafd = -1;
	packed = nil;
	expectederror = nil;
}


/* Compare the whole result and frame images, including preserved callback changes. */
static void
run(int resultshift, int valid)
{
	uchar expected[40];
	WORD word;
	LONG big;

	invoke(originalframe);
	check(!released && !sleepentered && calls == (valid && (operation != 11 || wantperiod > 0)) && entries == (valid && operation != 10));
	check(bad == !valid && packed == nil);
	check(allocations == (valid && (operation == 8 || operation == 9)) && frees == allocations);
	memmove(expected, originalresult, sizeof(expected));
	word = valid ? (operation == 11 && (TEST_OS || wantperiod <= 0) ? 0 : nativeint) : -1;
	big = valid ? nativebig : -1;
	memmove(expected+8+resultshift, operation == 2 ? (void*)&big : (void*)&word, resultwidth);
	check(!memcmp(expected, resultslot, sizeof(expected)));
	if(mutated)
		memset(frameimage+(originalframe-framebytes), 0xa7, framesize);
	check(!memcmp(frameimage, framebytes, sizeof(framebytes)));
	if(!valid)
		check(!mutated && !conversions);
}


/* Select a bounded ordinary scenario without weakening checks in full runs. */
static int
selected(char *name, int value)
{
	char *s = getenv(name);

	return s == nil || atoi(s) == value;
}


/* Check frame/result preflight and allocation failure in an isolated process. */
static void
preflight(int key)
{
	int mode;
	void *fp, *value;

	operation = key/8;
	mode = key%8;
	if(key >= 104) {
		operation = key < 106 ? 5 : key == 106 ? 8 : 9;
		mode = key < 106 ? key-96 : 10;
	}
	phase = 0;
	prepare(0, 0, 0);
	fp = originalframe;
	value = nil;
	if(mode == 0)fp = nil;
	if(mode == 1)fp = H;
	if(mode == 2)fp = (void*)((uintptr)-1-7);
	if(mode == 3 || mode == 4 || mode == 5) {
		if(mode == 4)value = H;
		if(mode == 5)value = (void*)((uintptr)-1-resultwidth+1);
		memmove(originalframe+retoffset, &value, sizeof(value));
	}
	if(mode >= 6 && mode <= 9) {
		if(mode == 7 || mode == 9)value = (void*)((uintptr)-1-1);
		memmove(originalframe+(mode >= 8 ? afdoffset : fdoffset), &value, sizeof(value));
	}
	expectederror = mode == 2 || mode == 5 || mode == 7 || mode == 9 ? exBounds : exNilref;
	if(mode == 10) {
		failallocation = 1;
		expectederror = exHeap;
	}
	invoke(fp);
	check(0);
}


/* Exercise all fixed builtins with packed frames, FD views, results and callbacks. */
static void*
worker(void *unused)
{
	int results[] = {0, 1, -1, 0x7fffffff, -0x7fffffff-1};
	vlong bigresults[] = {0, 1, -1, 9223372036854775807LL, -9223372036854775807LL-1};
	vlong numbers[] = {17, 0, -1, 0x7fffffff, -0x7fffffff-1, -1,
		2147483648LL, -2147483649LL, 4294967313LL, -1099511627776LL,
		9223372036854775807LL, -9223372036854775807LL-1};
	int targets[] = {2, 5, 5, 7, 9, 12};
	int scalarops[] = {0, 0, 2, 5, 6, 11};
	vlong scalars[] = {0, 1, -1, 17, 0x7fffffff, -0x7fffffff-1,
		4294967313LL, -1099511627776LL, 9223372036854775807LL, -9223372036854775807LL-1};
	int fs, rs, ds, r, target, index, valid;
	uintptr offset;
	WORD number;
	Sys_FD *handle;
	unsigned long checks = 0;
	char *family = getenv("SYS_CAPTURE_FAMILY");

	USED(unused);
	if(getenv("SYS_CAPTURE_ERROR") != nil)
		preflight(atoi(getenv("SYS_CAPTURE_ERROR")));
	check(family != nil);
	if(!strcmp(family, "ordinary")) {
		for(operation=0; operation<13; operation++)
		for(phase=0; phase<6; phase++)
		for(fs=0; fs<8; fs++)
		for(rs=0; rs<8; rs++)
		for(ds=0; ds<8; ds++)
		for(r=0; r<5; r++) {
			if(!selected("SYS_CAPTURE_OP", operation) || !selected("SYS_CAPTURE_PHASE", phase) ||
			   !selected("SYS_CAPTURE_FRAME", fs) || !selected("SYS_CAPTURE_FD", ds) ||
			   !selected("SYS_CAPTURE_RESULT", rs))
				continue;
			nativeint = results[r];
			nativebig = wantoff = bigresults[r];
			prepare(fs, rs, ds);
			run(rs, 1);
			checks++;
		}
	} else if(!strcmp(family, "nativeint")) {
		for(target=0; target<6; target++)
		for(index=0; index<(sizeof(WORD)>4 ? 10 : 6); index++)
		for(fs=0; fs<8; fs++)
		for(rs=0; rs<8; rs++)
		for(ds=0; ds<8; ds++)
		for(r=0; r<5; r++) {
			operation = scalarops[target];
			if(!selected("SYS_CAPTURE_OP", operation) || !selected("SYS_CAPTURE_TARGET", target) ||
			   !selected("SYS_CAPTURE_NUMBER", index))
				continue;
			phase = 1;
			nativeint = results[r];
			nativebig = wantoff = bigresults[r];
			prepare(fs, rs, ds);
			number = scalars[index];
			switch(target) {
			case 0: offset = O(F_Sys_dup, old); break;
			case 1: offset = O(F_Sys_dup, new); break;
			case 2: offset = O(F_Sys_seek, start); break;
			case 3: offset = O(F_Sys_mount, flags); break;
			case 4: offset = O(F_Sys_bind, flags); break;
			default: offset = O(F_Sys_sleep, period); break;
			}
			memmove(originalframe+offset, &number, sizeof(number));
			valid = index < 6;
			if(valid) {
				if(target == 0)wantold = number;
				if(target == 1)wantnew = number;
				if(target == 2)wantstart = number;
				if(target == 3 || target == 4)wantflags = number;
				if(target == 5)wantperiod = number;
			} else if(target == 2 || target == 3) {
				/* Invalid flags/start must reject before inspecting the unused FD. */
				handle = nil;
				memmove(originalframe+fdoffset, &handle, sizeof(handle));
			}
			memmove(frameimage, framebytes, sizeof(framebytes));
			run(rs, valid);
			checks++;
		}
	} else {
		check(!strcmp(family, "nativefd"));
		for(target=0; target<6; target++)
		for(index=0; index<(sizeof(WORD)>4 ? 12 : 6); index++)
		for(fs=0; fs<8; fs++)
		for(rs=0; rs<8; rs++)
		for(ds=0; ds<8; ds++)
		for(r=0; r<5; r++) {
			operation = targets[target];
			if(!selected("SYS_CAPTURE_OP", operation) || !selected("SYS_CAPTURE_TARGET", target) ||
			   !selected("SYS_CAPTURE_NUMBER", index))
				continue;
			phase = getenv("SYS_CAPTURE_PHASE") == nil ? 1 : atoi(getenv("SYS_CAPTURE_PHASE"));
			nativeint = results[r];
			nativebig = wantoff = bigresults[r];
			prepare(fs, rs, ds);
			number = numbers[index];
			handle = target == 2 ? originalafd : originalfd;
			memmove((uchar*)handle+O(Sys_FD, fd), &number, sizeof(number));
			offset = target == 2 ? afdoffset : fdoffset;
			if(index == 5) {
				handle = H;
				memmove(originalframe+offset, &handle, sizeof(handle));
			}
			valid = index < 6;
			if(valid) {
				if(target == 2)wantafd = numbers[index];
				else wantfd = numbers[index];
			}
			memmove(frameimage, framebytes, sizeof(framebytes));
			run(rs, valid);
			checks++;
		}
	}
	check(checks != 0);
	printf("PASS Sys capture %s: %lu scenarios WORD%zu\n", family, checks, sizeof(WORD)*8);
	return nil;
}


/* Supply ASan a tracked stack while keeping every preflight predicate enabled. */
int
main(void)
{
	pthread_t thread;

	check(pthread_create(&thread, nil, worker, nil) == 0);
	check(pthread_join(thread, nil) == 0);
	return 0;
}
