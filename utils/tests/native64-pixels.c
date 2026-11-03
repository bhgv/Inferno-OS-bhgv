#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "draw.h"
#include "kernel.h"
#include "draw-native64.h"
#include <stdio.h>
#include <setjmp.h>

char exNilref[] = "nil reference"; // Exact VM exception identity.
char exBounds[] = "bounds"; // Exact VM bounds exception identity.
char exType[] = "type"; // Exact VM type exception identity.
Type Tbyte, Tarray; // Canonical element and array owner identities.
static jmp_buf recovery; // Bounded VM error transport, no scheduler emulation.
static int recovering; // An error must never jump into an expired fixture invocation.
static char *expectederror; // Expected argument-refusal identity.
static unsigned long checks; // Active independent predicates, including NDEBUG builds.
static Image image; // Native image passed across the bounded lookup boundary.
static Display display; // Native protocol transport fixture.
static uchar storage[32768], record[32768]; // Actual allocated data and outgoing message storage.
static int pending, requests, flushes, reads, position, balance, locked, lookups; // Observable protocol state.
static vlong start, width, height; // Wide independent offsets, including refused extreme extents.
static int kind, row, stride, linebytes, limit, expectedx, expectedy; // Independent current-case geometry.
static int diagnostic; // Distinguish a retained I/O reason from a replacement diagnostic.
static int readmode, allocfail, flushfail, remote; // Explicit bounded transport failures.


/* Stop at an active semantic predicate before a bad native request can touch storage. */
static void
checkat(int condition, int line)
{
	if(!condition){
		fprintf(stderr, "Pixel transfer mismatch at %lu line %d pos %d limit %d\n", checks, line, position, limit);
		exit(10);
	}
	checks++;
}


#define check(condition) checkat((condition), __LINE__) // Active predicates retain their source location.


/* Require the expected VM error without treating a crash as successful refusal. */
void
error(char *message)
{
	check(recovering && expectederror != nil && strcmp(message, expectederror) == 0);
	longjmp(recovery, 1);
}


/* Capture diagnostics without substituting transport success for a library refusal. */
void
kwerrstr(char *format, ...)
{
	USED(format);
	diagnostic = 1;
}


/* Decode protocol fields independently of the production packing macros. */
static int
field(int offset)
{
	u32int value;

	value = record[offset]|((u32int)record[offset+1]<<8)|
		((u32int)record[offset+2]<<16)|((u32int)record[offset+3]<<24);
	return (int)value;
}


/* Inspect the preceding actual message before its storage can be reused. */
static void
capture(void)
{
	int rows, y, x;

	if(pending == 0)
		return;
	check(record[0] == kind && field(1) == 7);
	check(field(5) == expectedx && field(9) == expectedy+row);
	check(field(13) == expectedx+width);
	rows = field(17)-(expectedy+row);
	check(rows > 0 && rows <= height-row);
	if(kind == 'y'){
		check(pending == 21+rows*linebytes);
		for(y=0; y<rows; y++)
			for(x=0; x<linebytes; x++)
				check(record[21+y*linebytes+x] == (uchar)(((start+(row+y)*stride+x)*17+3)^((start+(row+y)*stride+x)>>8)));
	}else
		check(pending == 21);
	row += rows;
	pending = 0;
}


/* Allocate only bounded messages; the production code fills their actual protocol bytes. */
uchar*
bufimage(Display *d, int bytes)
{
	check(d == &display && bytes >= 21 && bytes <= sizeof(record));
	capture();
	requests++;
	if(allocfail)
		return nil;
	pending = bytes;
	memset(record, 0xa5, sizeof(record));
	return record;
}


/* Commit complete production messages while exposing real flush failures. */
int
flushimage(Display *d, int visible)
{
	check(d == &display && visible == 0);
	capture();
	flushes++;
	return flushfail ? -1 : 1;
}


/* Return bounded short reads, EOF or errors from a synthetic byte stream. */
long
kchanio(void *channel, void *buffer, int bytes, int mode)
{
	int n, i;

	USED(channel);
	check(mode == OREAD && bytes > 0 && position+bytes <= limit);
	check(buffer == storage+position);
	check(balance == (remote ? -1 : 0));
	reads++;
	if(readmode == 1 || (readmode == 3 && position > 0))
		return 0;
	if(readmode == 2 || (readmode == 4 && position > 0)){
		diagnostic = 2;
		return -1;
	}
	n = bytes < 7 ? bytes : 7;
	for(i=0; i<n; i++)
		((uchar*)buffer)[i] = ((position+i)*17+3)^((position+i)>>8);
	position += n;
	return n;
}


/* Record the native remote-I/O exclusion boundary without claiming a real scheduler. */
void
release(void)
{
	check(balance == 0);
	balance--;
}


/* Restore the same bounded exclusion state after every short read and failure. */
void
acquire(void)
{
	check(balance == -1);
	balance++;
}


/* Preserve the production VM consumer's lookup boundary as an observable test transport. */
Image*
checkimage(Draw_Image *object)
{
	check(object != nil && object != H);
	lookups++;
	return &image;
}


/* Record exact display locking around the actual production pixel call. */
int
lockdisplay(Display *d)
{
	check(d == &display && locked == 0);
	locked++;
	return 1;
}


/* Require the consumer to release its acquired display lock before publishing a result. */
void
unlockdisplay(Display *d)
{
	check(d == &display && locked == 1);
	locked--;
}


/* Observe the flush boundary without exercising GUI refresh or ownership. */
void
checkflush(Draw_Image *object)
{
	USED(object);
	check(locked == 1);
}

PIXEL_CONSUMERS
PIXEL_ORACLES


/* Reset bounded transport state and retain every source and destination guard byte. */
static void
setup(void)
{
	int i;

	memset(&image, 0, sizeof(image));
	memset(&display, 0, sizeof(display));
	display.bufsize = 512;
	display.local = !remote;
	image.display = &display;
	image.id = 7;
	pending = requests = flushes = reads = position = balance = locked = lookups = 0;
	readmode = allocfail = flushfail = row = diagnostic = 0;
	for(i=0; i<sizeof(storage); i++)
		storage[i] = (i*17+3)^(i>>8);
}


/* Isolate setjmp from all state modified by a rejected production consumer. */
static int
invoke(void (*call)(void*), void *frame)
{
	if(setjmp(recovery)){
		recovering = 0;
		return 0;
	}
	recovering = 1;
	call(frame);
	recovering = 0;
	return 1;
}


/* Exercise real frame declarations, byte-array validation and unaligned scalar results. */
static void
vmcases(void)
{
	struct {
		Heap owner; // Actual VM owner header checked by the byte view.
		Array value; // Canonical array representation following its header.
	} array;
	Type compatible, bad;
	F_Draw_Image_readpixels frame;
	uchar result[16], wanted[16], wantedframe[sizeof(frame)];
	WORD value;
	int method, offset, refusal, alias, expectedrow;
	void (*call)(void*);

	check((uchar*)&array.value-(uchar*)&array.owner == sizeof(Heap));
	check(sizeof(F_Draw_Image_readpixels) == sizeof(F_Draw_Image_writepixels));
	check(offsetof(F_Draw_Image_readpixels, src) == offsetof(F_Draw_Image_writepixels, dst));
	check(offsetof(F_Draw_Image_readpixels, data) == offsetof(F_Draw_Image_writepixels, data));
	check(offsetof(F_Draw_Image_readpixels, r) == offsetof(F_Draw_Image_writepixels, r));
	memset(&compatible, 0, sizeof(compatible));
	memset(&bad, 0, sizeof(bad));
	compatible.size = 1;
	bad.size = 2;
	for(method=0; method<2; method++)
	for(alias=0; alias<2; alias++)
	for(offset=0; offset<8; offset++)
	for(refusal=0; refusal<17; refusal++){
		setup();
		image.r = image.clipr = Rect(0, 0, 10, 2);
		image.depth = 8;
		kind = method ? 'y' : 'r';
		stride = linebytes = width = 10;
		height = 2;
		start = expectedx = expectedy = 0;
		limit = 20;
		memset(&array, 0, sizeof(array));
		array.owner.t = &Tarray;
		array.value.t = &Tbyte;
		array.value.data = storage;
		array.value.len = 20;
		memset(&frame, 0, sizeof(frame));
		frame.r.min.x = frame.r.min.y = 0;
		frame.r.max.x = 10; frame.r.max.y = 2;
		frame.src = (Draw_Image*)&image;
		frame.data = &array.value;
		frame.ret = (WORD*)(alias ? (uchar*)&frame.r+offset : result+offset);
		expectederror = nil;
		value = 20;
		expectedrow = 2;
		switch(refusal){
		case 1: frame.data = H; expectederror = exNilref; break;
		case 2: frame.data = nil; expectederror = exNilref; break;
		case 3: array.owner.t = &Tbyte; expectederror = exType; break;
		case 4: array.value.len = -1; expectederror = exBounds; break;
		case 5: array.value.data = H; expectederror = exBounds; break;
		case 6: array.value.data = nil; expectederror = exBounds; break;
		case 7: array.value.t = nil; expectederror = exType; break;
		case 8: array.value.data = (uchar*)((uintptr)-1-3); expectederror = exBounds; break;
		case 9: array.value.t = &compatible; break;
		case 10: bad.size = 1; bad.np = 1; array.value.t = &bad; expectederror = exType; break;
		case 11: bad.size = 2; bad.np = 0; array.value.t = &bad; expectederror = exType; break;
		case 12: array.value.len = 19; value = -1; expectedrow = 0; break;
		case 13: allocfail = 1; value = -1; expectedrow = 0; break;
		case 14: flushfail = 1; value = -1; expectedrow = method ? 2 : 0; break;
		case 15: readmode = 1; value = method ? 20 : -1; break;
		case 16: readmode = 4; value = method ? 20 : -1; break;
		}
		memset(result, 0x5a, sizeof(result));
		memmove(wanted, result, sizeof(result));
		memmove(wantedframe, &frame, sizeof(frame));
		call = method ? Draw_Image_writepixels : Draw_Image_readpixels;
		if(expectederror != nil){
			check(!invoke(call, &frame));
			check(lookups == 0 && requests == 0 && flushes == 0 && locked == 0);
		}else{
			check(invoke(call, &frame));
			if(alias)
				memmove(wantedframe+offsetof(F_Draw_Image_readpixels,r)+offset, &value, sizeof(value));
			else
				memmove(wanted+offset, &value, sizeof(value));
			check(lookups == 1 && row == expectedrow && locked == 0 && balance == 0);
		}
		check(memcmp(result, wanted, sizeof(result)) == 0);
		check(memcmp(&frame, wantedframe, sizeof(frame)) == 0);
	}
}


/* Run independent native protocol oracles and the actual VM consumer argument boundary. */
int
main(void)
{
	int i;

	check(limbo_native64_Draw_layout());
	check(sizeof(WORD) == 4);
	for(remote=0; remote<2; remote++){
		for(i=0; i<nelem(cases); i++)
			exercise(&cases[i]);
		vmcases();
	}
	printf("PASS native64 pixels: %lu predicates; real load/unload protocol, short reads, failures and VM frames\n", checks);
	return 0;
}
