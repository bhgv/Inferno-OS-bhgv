#include "lib9.h"
#include <bio.h>
#include <assert.h>
#include <stdio.h>

static int seeks, writes, seekfailure, writefailure, checks;
static vlong nativeposition;
/* Simulate native seeks without allowing invalid absolute positions. */
static vlong
inputseek(int fd, vlong offset, int base)
{
	assert(fd == 17); seeks++;
	if(seekfailure || (base == 0 && offset < 0))return Beof;
	if(base == 0)nativeposition = offset;
	else if(base == 1)nativeposition += offset;
	else if(base == 2)nativeposition = 1000+offset;
	else return Beof;
	return nativeposition;
}
/* Output flushes advance the native position only when accepted in full. */
static long
outputwrite(int fd, void *p, long n)
{
	(void)p; assert(fd == 17 && n > 0); writes++;
	if(writefailure)return -1;
	nativeposition += n;
	return n;
}
/* Unexpected inactive-state diagnostics remain observable without linking stdio fmt. */
int fprint(int fd, char *text, ...) { (void)fd; (void)text; return 0; }
/* LeakSanitizer cannot inspect processes in the sandbox. */
int __lsan_is_turned_off(void) { return 1; }
#define seek inputseek
#define write outputwrite
IMPLEMENTATION
#undef seek
#undef write

/* Model a 16-byte read window ending at offset 100, with logical position 90. */
static void
setup(Biobuf *b, int state)
{
	memset(b, 0, sizeof(*b));
	b->bbuf = b->b; b->ebuf = b->b+16; b->gbuf = b->bbuf;
	b->bsize = 16; b->fid = 17; b->state = state;
	b->offset = nativeposition = 100; b->icount = -10; b->ocount = -10;
	seeks = writes = seekfailure = writefailure = 0;
}

/* Every position inside the valid read window has an exact in-buffer count. */
static void
windows(void)
{
	Biobuf b;
	int state, relative, target;

	for(state = Bractive; state <= Bracteof; state++){
		if(state == Bwactive)continue;
		for(relative = 0; relative < 2; relative++) for(target = 84; target <= 100; target++){
			setup(&b, state);
			assert(Bseek(&b, relative ? target-90 : target, relative) == target);
			assert(b.offset == 100 && b.icount == target-100 && b.state == Bractive);
			assert(Boffset(&b) == target && seeks == 0); checks++;
		}
	}
}

/* Failed seeks preserve read-window metadata even for full-width distant targets. */
static void
failures(void)
{
	Biobuf b, saved;
	vlong maximum = (vlong)(~(uvlong)0 >> 1);
	vlong targets[] = {83, 101, 0x100000001LL, -1, (-9223372036854775807LL-1)};
	int state, i;

	for(state = Bractive; state <= Bracteof; state++){
		if(state == Bwactive)continue;
		for(i = 0; i < sizeof(targets)/sizeof(targets[0]); i++){
			setup(&b, state); saved = b; seekfailure = 1;
			assert(Bseek(&b, targets[i], 0) == Beof);
			assert(!memcmp(&b, &saved, sizeof(b)) && seeks == 1); checks++;
		}
	}
	setup(&b, Bractive); b.offset = maximum; b.icount = -1; saved = b;
	assert(Bseek(&b, 2, 1) == Beof && seeks == 0);
	assert(!memcmp(&b, &saved, sizeof(b))); checks++;
	/* The lowest signed offset is valid input arithmetic, but an invalid file position. */
	setup(&b, Bractive); saved = b;
	assert(Bseek(&b, -maximum-1, 1) == Beof && seeks == 1);
	assert(!memcmp(&b, &saved, sizeof(b))); checks++;
}

/* Native movement invalidates old read data only after successful completion. */
static void
native(void)
{
	Biobuf b;

	setup(&b, Bractive);
	assert(Bseek(&b, 80, 0) == 80 && seeks == 1 && b.offset == 80);
	assert(b.icount == 0 && b.gbuf == b.ebuf);
	setup(&b, Bracteof);
	assert(Bseek(&b, -20, 2) == 980 && seeks == 1 && b.state == Bractive);
	assert(b.offset == 980 && b.icount == 0 && b.gbuf == b.ebuf); checks += 2;
}

/* Output seeks must stop at flush errors and retain post-flush offsets on seek errors. */
static void
output(void)
{
	Biobuf b;

	setup(&b, Bwactive); writefailure = 1;
	assert(Bseek(&b, 200, 0) == Beof && writes == 1 && seeks == 0);
	assert(b.offset == 100); checks++;
	setup(&b, Bwactive); seekfailure = 1;
	assert(Bseek(&b, 200, 0) == Beof && writes == 1 && seeks == 1);
	assert(b.offset == 106 && b.ocount == -16); checks++;
	setup(&b, Bwactive);
	assert(Bseek(&b, 3, 1) == 109 && writes == 1 && seeks == 1);
	assert(b.offset == 109 && b.ocount == -16); checks++;
}

/* Exercise production seek/offset/flush with deterministic native boundaries. */
int
main(void)
{
	windows(); failures(); native(); output();
	printf("PASS Bio seek: %d window/overflow/failure/flush cases\n", checks);
	return 0;
}
