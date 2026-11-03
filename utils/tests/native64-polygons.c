#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "draw.h"
#include "kernel.h"
#include "draw-native64.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

char exNilref[] = "nil"; // Expected nil-array VM refusal identity.
char exBounds[] = "bounds"; // Expected extent/alignment VM refusal identity.
char exType[] = "type"; // Expected Point/array metadata refusal identity.
Type Tarray; // Canonical managed-array owner identity.
static Display display, foreign; // Explicit native transport display identities.
static Image images[2]; // Native source/destination under the bounded lookup transport.
static Draw_Image objects[2]; // Borrowed VM objects, without allocating GUI resources.
static unsigned long checks; // Active NDEBUG-independent predicates.
static uchar packet[400000]; // Bounded actual native protocol output.
static Point zeros[65536]; // Real accessible storage for the maximum wire point count.
static int requests, operators, lookups, locked, flushes, denyalloc, rejecting; // Observable effect boundaries.
static int wantedcmd, wantedend, wantedend1, wantedradius, wantedop, wantedcount; // Expected message header.
static const uchar *wantedbytes; // Independent Python payload, or nil for all-zero coordinates.
static int wantedlength, packetlength; // Expected compressed payload and actual complete record.
static int calls, failat, live, buffail; // Native allocation/transport failure injection.
static jmp_buf recovery; // Bounded VM error transport, never a scheduler substitute.
static char *expectederror; // Exact expected pre-effect VM error.

typedef struct Block Block;

struct Block {
	void *p; // Owned tracked native allocation.
	size_t bytes; // Guarded accessible payload size.
};

static Block blocks[16]; // Bounded native curve and packet allocation ledger.


/* Reject a semantic failure before unsafe continuation, releasing private control-test allocations. */
static void
checkat(int condition, int line)
{
	int i;

	if(!condition){
		for(i=0; i<16; i++)
			if(blocks[i].p != nil){
				free(blocks[i].p);
				blocks[i].p = nil;
			}
		fprintf(stderr, "polygon semantic mismatch after %lu checks line %d\n", checks, line);
		exit(10);
	}
	checks++;
}


#define check(condition) checkat((condition), __LINE__) // Keep every predicate active with source location.


/* Bound actual native allocation and inject a failed reservation before point reads. */
static void*
pointalloc(size_t bytes)
{
	uchar *p;
	int i;

	check(!denyalloc && bytes <= 1024*1024);
	calls++;
	if(calls == failat)
		return nil;
	p = malloc(bytes+32);
	check(p != nil);
	memset(p+bytes, 0xa5, 32);
	for(i=0; i<16 && blocks[i].p != nil; i++)
		;
	check(i < 16);
	blocks[i].p = p;
	blocks[i].bytes = bytes;
	live++;
	return p;
}


/* Observe exact guards before retiring the native allocation. */
static void
pointfree(void *p)
{
	int i, j;

	if(p == nil)
		return;
	for(i=0; i<16 && blocks[i].p != p; i++)
		;
	check(i < 16);
	for(j=0; j<32; j++)
		check(((uchar*)p)[blocks[i].bytes+j] == 0xa5);
	free(p);
	blocks[i].p = nil;
	live--;
}


/* Preserve the old owner on realloc failure, matching the ordinary native allocator contract. */
static void*
pointrealloc(void *p, size_t bytes)
{
	void *next;
	int i;
	size_t count;

	for(i=0; i<16 && blocks[i].p != p; i++)
		;
	check(i < 16);
	count = blocks[i].bytes < bytes ? blocks[i].bytes : bytes;
	next = pointalloc(bytes);
	if(next != nil){
		memmove(next, p, count);
		pointfree(p);
	}
	return next;
}


/* Reject VM arrays before any image lookup or native effect. */
void
error(char *message)
{
	check(rejecting && expectederror != nil && strcmp(message, expectederror) == 0);
	longjmp(recovery, 1);
}


/* Preserve diagnostics without turning a native protocol refusal into success. */
int
_drawprint(int fd, char *format, ...)
{
	USED(fd);
	USED(format);
	return 0;
}


/* Inspect compositing only after all coordinates have been proven encodable. */
void
_setdrawop(Display *d, Drawop op)
{
	check(d == &display && wantedlength >= 0 && op == wantedop);
	operators++;
}


/* Bound the actual packet; its payload is checked independently after the native call returns. */
uchar*
bufimage(Display *d, int bytes)
{
	check(d == &display && wantedlength >= 0 && bytes == 31+wantedlength && bytes <= sizeof(packet));
	requests++;
	if(buffail)
		return nil;
	memset(packet, 0xa5, sizeof(packet));
	packetlength = bytes;
	return packet;
}


/* Keep lookup observable and forbid any effect after a rejected Point-array shape. */
Image*
checkimage(Draw_Image *object)
{
	check(!rejecting);
	lookups++;
	if(object == &objects[0])
		return &images[0];
	check(object == &objects[1]);
	return &images[1];
}


/* Model one existing display exclusion, without simulating native scheduler ownership. */
int
lockdisplay(Display *d)
{
	check(d == &display && locked == 0);
	locked++;
	return 1;
}


/* Require balanced native exclusion around every VM consumer invocation. */
void
unlockdisplay(Display *d)
{
	check(d == &display && locked == 1);
	locked--;
}


/* Observe the existing VM flush boundary with its lock still held. */
void
checkflush(Draw_Image *object)
{
	check(object == &objects[0] && locked == 1);
	flushes++;
}

NATIVE_IMPLEMENTATION
VM_IMPLEMENTATION
POLYGON_ORACLES


/* Decode message words without the production packing macros. */
static u32int
word(int at)
{
	return packet[at]|((u32int)packet[at+1]<<8)|((u32int)packet[at+2]<<16)|((u32int)packet[at+3]<<24);
}


/* Reset only native test transports; all live allocations must already have retired. */
static void
setup(int count, int length, const uchar *bytes)
{
	check(live == 0 && locked == 0);
	memset(images, 0, sizeof(images));
	images[0].display = images[1].display = &display;
	images[0].id = 7;
	images[1].id = 9;
	requests = operators = lookups = flushes = calls = failat = buffail = denyalloc = rejecting = packetlength = 0;
	wantedcmd = 'p';
	wantedend = 3;
	wantedend1 = 5;
	wantedradius = 2;
	wantedop = SoverD;
	wantedcount = count;
	wantedlength = length;
	wantedbytes = bytes;
}


/* Compare full header and compressed coordinate bytes against the independent oracle. */
static void
inspect(void)
{
	int i;

	check(live == 0);
	if(wantedlength < 0 || wantedcount <= 0){
		check(requests == 0 && operators == 0);
		return;
	}
	check(requests == 1 && operators == 1);
	if(buffail){
		check(packetlength == 0);
		return;
	}
	check(packetlength == 31+wantedlength && packet[0] == wantedcmd && word(1) == 7);
	check((packet[5]|packet[6]<<8)+1 == wantedcount);
	check(word(7) == (u32int)wantedend && word(11) == (u32int)wantedend1 && word(15) == (u32int)wantedradius);
	check(word(19) == 9 && word(23) == (u32int)-7 && word(27) == 11);
	for(i=0; i<wantedlength; i++)
		check(packet[31+i] == (wantedbytes == nil ? 0 : wantedbytes[i]));
}


/* Exercise real native polygon encoders, wire endpoints and allocation/transport refusal. */
static void
nativecases(void)
{
	unsigned int i;
	int fill, op, fault;
	Point sp;

	sp.x = -7;
	sp.y = 11;
	for(i=0; i<nelem(cases); i++)
	for(fill=0; fill<2; fill++)
	for(op=0; op<2; op++)
	for(fault=0; fault<3; fault++){
		setup(cases[i].count, cases[i].bytes, cases[i].encoded);
		wantedcmd = fill ? 'P' : 'p';
		wantedend1 = fill ? 0 : 5;
		wantedradius = fill ? 0 : 2;
		wantedop = op ? 4 : SoverD;
		if(fault == 1){ failat = 1; wantedlength = -1; }
		if(fault == 2) buffail = 1;
		if(fill){
			if(op) fillpolyop(&images[0], cases[i].points, cases[i].count, 3, &images[1], sp, 4);
			else fillpoly(&images[0], cases[i].points, cases[i].count, 3, &images[1], sp);
		}else{
			if(op) polyop(&images[0], cases[i].points, cases[i].count, 3, 5, 2, &images[1], sp, 4);
			else poly(&images[0], cases[i].points, cases[i].count, 3, 5, 2, &images[1], sp);
		}
		inspect();
	}
	setup(65536, 131072, nil);
	poly(&images[0], zeros, 65536, 3, 5, 2, &images[1], sp);
	inspect();
	for(fault=0; fault<4; fault++){
		setup(0, -1, nil);
		denyalloc = 1;
		poly(&images[0], zeros, fault == 0 ? -1 : fault == 1 ? 0 : fault == 2 ? 65537 : 0x7fffffff, 0, 0, 0, &images[1], sp);
		check(calls == 0 && live == 0 && requests == 0);
	}
	for(fault=0; fault<5; fault++){
		setup(0, -1, nil);
		denyalloc = 1;
		if(fault == 3) images[1].display = &foreign;
		poly(fault == 0 ? nil : &images[0], fault == 1 ? nil : fault == 4 ? (Point*)((uintptr)-4) : zeros,
			1, 0, 0, 0, fault == 2 ? nil : &images[1], sp);
		if(fault == 3){
			images[1].display = &foreign;
			poly(&images[0], zeros, 1, 0, 0, 0, &images[1], sp);
		}
		check(calls == 0 && live == 0 && requests == 0);
	}
}


/* Check each real curve-list growth refusal and the retained zero-geometry positive. */
static void
curvecases(void)
{
	Point input[80], *out;
	int count, fault, total, fill, op, result;
	Point zero, sp;

	memset(input, 0, sizeof(input));
	setup(0, -1, nil);
	count = getbezsplinepts(input, 80, &out);
	check(count == 238 && out != nil);
	total = calls;
	pointfree(out);
	check(live == 0 && total > 2);
	for(fault=1; fault<=total; fault++){
		setup(0, -1, nil);
		failat = fault;
		count = getbezsplinepts(input, 80, &out);
		check(count == -1 && out == nil && live == 0 && calls == fault);
	}

	zero.x = zero.y = 0;
	sp.x = -7;
	sp.y = 11;
	for(fill=0; fill<2; fill++)
	for(op=0; op<2; op++)
	for(fault=0; fault<4; fault++){
		setup(4, 8, nil);
		wantedcmd = fill ? 'P' : 'p';
		wantedend1 = fill ? 0 : 5;
		wantedradius = fill ? 0 : 2;
		wantedop = op ? 4 : SoverD;
		if(fault == 1 || fault == 2){
			failat = fault;
			wantedlength = -1;
		}
		if(fault == 3) buffail = 1;
		if(fill){
			if(op) result = fillbezierop(&images[0], zero, zero, zero, zero, 3, &images[1], sp, 4);
			else result = fillbezier(&images[0], zero, zero, zero, zero, 3, &images[1], sp);
		}else{
			if(op) result = bezierop(&images[0], zero, zero, zero, zero, 3, 5, 2, &images[1], sp, 4);
			else result = bezier(&images[0], zero, zero, zero, zero, 3, 5, 2, &images[1], sp);
		}
		check(result == (fault != 1));
		inspect();
	}
}


/* Invoke VM entry points with expected errors isolated from changing frame storage. */
static int
invoke(void (*call)(void*), void *frame)
{
	if(setjmp(recovery))
		return 0;
	call(frame);
	return 1;
}


/* Execute all eight generated Frame64 consumers with independent array-shape and no-effect checks. */
static void
vmcases(void)
{
	struct {
		Heap owner; // Real owner-header geometry, with explicit canonical array identity.
		Array array; // Borrowed VM array metadata exercised by the real consumer.
	} value;
	Type element;
	F_Draw_Image_polyop line;
	F_Draw_Image_fillpolyop fill;
	void (*entries[8])(void*) = {Draw_Image_poly, Draw_Image_polyop, Draw_Image_bezspline, Draw_Image_bezsplineop,
		Draw_Image_fillpoly, Draw_Image_fillpolyop, Draw_Image_fillbezspline, Draw_Image_fillbezsplineop};
	int entry, shape, status, smooth, filled;
	void *frame;

	for(entry=0; entry<8; entry++)
	for(shape=0; shape<15; shape++){
		smooth = (entry&2) != 0;
		filled = entry >= 4;
		setup(smooth ? 7 : 3, smooth ? 14 : 6, nil);
		wantedcmd = filled ? 'P' : 'p';
		wantedend1 = filled ? 0 : 5;
		wantedradius = filled ? 0 : 2;
		wantedop = (entry&1) ? 4 : SoverD;
		memset(&value, 0, sizeof(value));
		memset(&element, 0, sizeof(element));
		value.owner.t = &Tarray;
		value.owner.ref = 1;
		element.ref = 1;
		element.size = sizeof(Point);
		value.array.t = &element;
		value.array.len = 3;
		value.array.data = (uchar*)zeros;
		memset(&line, 0, sizeof(line));
		memset(&fill, 0, sizeof(fill));
		line.dst = fill.dst = &objects[0];
		line.src = fill.src = &objects[1];
		line.p = fill.p = &value.array;
		line.end0 = fill.wind = 3;
		line.end1 = 5;
		line.radius = 2;
		line.sp.x = fill.sp.x = -7;
		line.sp.y = fill.sp.y = 11;
		line.op = fill.op = 4;
		expectederror = exBounds;
		if(shape == 1){line.p = fill.p = H; expectederror = exNilref;}
		if(shape == 2){line.p = fill.p = nil; expectederror = exNilref;}
		if(shape == 3){value.owner.t = nil; expectederror = exType;}
		if(shape == 4){value.array.t = H; expectederror = exType;}
		if(shape == 5){element.size = 4; expectederror = exType;}
		if(shape == 6){element.np = 1; expectederror = exType;}
		if(shape == 7)value.array.len = -1;
		if(shape == 8)value.array.len = 65537;
		if(shape == 9)value.array.data = nil;
		if(shape == 10)value.array.data = (uchar*)zeros+1;
		if(shape == 11)value.array.data = (uchar*)((uintptr)-4);
		if(shape == 12){value.array.len = 0; value.array.data = (uchar*)H; wantedcount = 0; wantedlength = -1;}
		if(shape == 13){images[1].display = &foreign; wantedcount = 0; wantedlength = -1;}
		if(shape == 14 && !filled){line.radius = -1; wantedcount = 0; wantedlength = -1;}
		rejecting = shape >= 1 && shape <= 11;
		frame = filled ? (void*)&fill : (void*)&line;
		status = invoke(entries[entry], frame);
		check(status == !rejecting && locked == 0 && live == 0);
		if(rejecting)
			check(lookups == 0 && flushes == 0 && calls == 0 && requests == 0);
		else if(shape == 13 || (shape == 14 && !filled))
			check(lookups == 2 && flushes == 0 && requests == 0);
		else{
			check(lookups == 2 && flushes == 1);
			inspect();
		}
	}
}


/* Keep protocol, real curve allocation and generated VM-entry evidence separate. */
int
main(void)
{
	nativecases();
	curvecases();
	vmcases();
	printf("PASS native polygons: %lu predicates; four real encoders, eight Frame64 consumers, curve growth refusals; bounded transports\n", checks);
	return 0;
}
