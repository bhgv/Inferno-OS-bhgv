#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "draw.h"
#include "kernel.h"
#include "draw-native64.h"
#include <stdio.h>
#include <stdlib.h>

static Display display, other; // Bounded native display identities, no device or resource registration.
static Image images[6]; // Destination, source, mask and three different default images.
static Draw_Image objects[3]; // Borrowed VM objects resolved only at the explicit lookup boundary.
static unsigned long checks; // Active semantic predicates under NDEBUG.
static int requests, used, failat, locked, flushes; // Native transport/exclusion observations.
static uchar actual[512]; // Guarded bounded packet trace, never a display buffer substitute beyond this test.

typedef struct PrimitiveCase PrimitiveCase;

struct PrimitiveCase {
	int variant; // Exact native/Frame64 callable index.
	int mode; // Zero native API, one actual generated-frame consumer.
	Rectangle r; // Independent requested rectangle.
	Point p0; // Independent start/center or shared source origin.
	Point p1; // Independent line endpoint or mask origin.
	Point sp; // Independent texture origin.
	int a; // Horizontal ellipse radius.
	int b; // Vertical ellipse radius.
	int width; // Outline radius or signed border thickness.
	int end0; // Line start encoding.
	int end1; // Line end encoding.
	int alpha; // Arc start, encoded with an unsigned bit31 flag.
	int phi; // Arc sweep.
	int op; // Explicit requested composition value; default wrappers ignore it.
	int srcdefault; // Draw/gendraw nil/H source selection.
	int maskdefault; // Draw/gendraw nil/H mask selection.
	int foreign; // One source or mask belongs to another display.
	int count; // Independent number of complete command reservations.
	int bytes; // Independent per-reservation length including optional operator.
	uchar expected[188]; // Complete independent protocol trace (at most four border edges).
};

static PrimitiveCase *current; // Immutable oracle active at the bounded transport boundary.


/* Stop at the first semantic disagreement, before the damaged control can continue. */
static void
checkat(int condition, int line)
{
	if(!condition){
		fprintf(stderr, "primitive semantic mismatch after %lu checks line %d variant=%d mode=%d\n",
			checks, line, current != nil ? current->variant : -1, current != nil ? current->mode : -1);
		exit(10);
	}
	checks++;
}

#define check(condition) checkat((condition), __LINE__) // Keep independent predicates active with source coordinates.


/* Reserve exactly the oracle's complete record or inject refusal without modifying any trace bytes. */
uchar*
bufimage(Display *d, int bytes)
{
	uchar *packet;

	check(d == &display && current != nil && requests < current->count && bytes == current->bytes);
	requests++;
	if(requests == failat)
		return nil;
	check(bytes > 0 && used+bytes <= sizeof(actual)-32);
	packet = actual+used;
	used += bytes;
	return packet;
}


/* Keep native buffer-refusal diagnostics separate from the active semantic oracle. */
int
_drawprint(int fd, char *format, ...)
{
	USED(fd);
	USED(format);
	return 0;
}


/* Resolve only declared test objects; no managed owner or arbitrary pointer provenance is simulated. */
Image*
checkimage(Draw_Image *object)
{
	int i;

	for(i=0; i<3; i++)
		if(object == objects+i)
			return images+i;
	check(0);
	return nil;
}


/* Model balanced display exclusion without replacing the production consumer body. */
int
lockdisplay(Display *d)
{
	check(d == &display && locked == 0);
	locked = 1;
	return 1;
}


/* Observe release after every consumer-side flush, even when native packet reservation refuses. */
void
unlockdisplay(Display *d)
{
	check(d == &display && locked == 1);
	locked = 0;
}


/* Verify the existing consumer flush occurs inside its native display exclusion. */
void
checkflush(Draw_Image *object)
{
	check(object == objects && locked == 1);
	flushes++;
}

PACKET_HELPER
NATIVE_IMPLEMENTATION
VM_IMPLEMENTATION
PRIMITIVE_ORACLES


/* Exercise every native public wrapper against the exact same mathematical inputs. */
static void
nativecall(PrimitiveCase *c)
{
	Image *source, *mask;

	source = c->srcdefault ? nil : images+1;
	mask = c->maskdefault ? nil : images+2;
	switch(c->variant){
	case 0: draw(images, c->r, source, mask, c->p0); break;
	case 1: drawop(images, c->r, source, mask, c->p0, c->op); break;
	case 2: gendraw(images, c->r, source, c->p0, mask, c->p1); break;
	case 3: gendrawop(images, c->r, source, c->p0, mask, c->p1, c->op); break;
	case 4: line(images, c->p0, c->p1, c->end0, c->end1, c->width, images+1, c->sp); break;
	case 5: lineop(images, c->p0, c->p1, c->end0, c->end1, c->width, images+1, c->sp, c->op); break;
	case 6: ellipse(images, c->p0, c->a, c->b, c->width, images+1, c->sp); break;
	case 7: ellipseop(images, c->p0, c->a, c->b, c->width, images+1, c->sp, c->op); break;
	case 8: arc(images, c->p0, c->a, c->b, c->width, images+1, c->sp, c->alpha, c->phi); break;
	case 9: arcop(images, c->p0, c->a, c->b, c->width, images+1, c->sp, c->alpha, c->phi, c->op); break;
	case 10: fillellipse(images, c->p0, c->a, c->b, images+1, c->sp); break;
	case 11: fillellipseop(images, c->p0, c->a, c->b, images+1, c->sp, c->op); break;
	case 12: fillarc(images, c->p0, c->a, c->b, images+1, c->sp, c->alpha, c->phi); break;
	case 13: fillarcop(images, c->p0, c->a, c->b, images+1, c->sp, c->alpha, c->phi, c->op); break;
	case 14: border(images, c->r, c->width, images+1, c->sp); break;
	default: check(0);
	}
}


/* Use fresh compiler-generated Frame64 structs rather than synthetic offset tables. */
static void
framecall(PrimitiveCase *c)
{
	Draw_Image *basic_source, *basic_mask;

	basic_source = c->srcdefault ? H : objects+1;
	basic_mask = c->maskdefault ? H : objects+2;
	switch(c->variant){
	FRAME_DISPATCH
	default: check(0);
	}
}


/* Compare every byte after each possible command refusal; borders retain per-command failure semantics. */
static void
exercise(PrimitiveCase *c)
{
	int failure, i, offset;

	current = c;
	for(failure=0; failure<=c->count; failure++){
		memset(&display, 0, sizeof(display));
		memset(images, 0, sizeof(images));
		for(i=0; i<6; i++)
			images[i].display = &display;
		images[0].id=7; images[1].id=9; images[2].id=11;
		images[3].id=13; images[4].id=17; images[5].id=19;
		display.black=images+3; display.opaque=images+4; display.white=images+5;
		if(c->foreign == 1) images[1].display = &other;
		if(c->foreign == 2) images[2].display = &other;
		memset(actual, 0xa5, sizeof(actual));
		requests = used = locked = flushes = 0;
		failat = failure;
		if(c->mode) framecall(c); else nativecall(c);
		check(requests == c->count && locked == 0 && flushes <= 1);
		offset = 0;
		for(i=0; i<c->count; i++){
			if(i+1 == failure) continue;
			check(memcmp(actual+offset, c->expected+i*c->bytes, c->bytes) == 0);
			offset += c->bytes;
		}
		check(used == offset);
		for(i=used; i<sizeof(actual); i++) check(actual[i] == 0xa5);
	}
}


/* Check zero-side-effect native refusals without passing invalid pointers into managed lookup. */
static void
nativeinvalid(void)
{
	PrimitiveCase empty;
	Point p = {0,0};
	Rectangle r = {{0,0},{1,1}};

	memset(&empty, 0, sizeof(empty));
	current = &empty;
	requests = used = 0;
	draw(nil, r, images+1, images+2, p);
	line(nil, p, p, 0, 0, 1, images+1, p);
	line(images, p, p, 0, 0, 1, nil, p);
	ellipse(nil, p, 1, 1, 0, images+1, p);
	fillellipse(images, p, 1, 1, nil, p);
	border(nil, r, 1, images+1, p);
	display.black = nil;
	draw(images, r, nil, images+2, p);
	display.black = images+3;
	display.opaque = nil;
	draw(images, r, images+1, nil, p);
	display.opaque = images+4;
	images[0].display = nil;
	line(images, p, p, 0, 0, 1, images+1, p);
	ellipse(images, p, 1, 1, 0, images+1, p);
	border(images, r, 1, images+1, p);
	check(requests == 0 && used == 0);
}


/* Run independent full-range packet geometry through actual native and Frame64 consumers. */
int
main(void)
{
	unsigned int i;

	check(sizeof(void*) == 8 && sizeof(WORD) == 4 && sizeof(Frame) == 32);
	for(i=0; i<nelem(cases); i++)
		exercise(cases+i);
	nativeinvalid();
	printf("PASS native primitives: %u independent native/Frame64 cases, %lu predicates, 15 consumers, per-reservation failures; lookup/locks/I-O bounded\n", (unsigned int)nelem(cases), checks);
	return 0;
}
