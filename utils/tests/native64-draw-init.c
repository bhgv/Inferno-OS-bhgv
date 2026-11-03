#include "runt.h"
#include "draw.h"
#include "nativeptrmap.h"

char exType[] = "bad type"; // Existing VM error identity used by native pointer-map validation.

#define Drawmodlen 0 // No export-table ABI is asserted by this descriptor fixture.
static Runtab Drawmodtab[1]; // Observe only the module-registration boundary.
static int registration, registrations, observations, cases; // Failure mode and active boundary counters.

typedef struct DRef DRef;
typedef struct DDisplay DDisplay;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct DFont DFont;
DRAW_WRAPPERS
DRAW_GLOBALS


/* Distinguish a failed ownership predicate from a compiler error or fatal signal. */
static void
drawcheck(int condition)
{
	int i;

	if(!condition){
		fprintf(stderr, "Draw type initialization mismatch\n");
		/* The predicate has failed. Retire only fixture allocations before fatal exit,
		 * so deliberate source leaks do not turn a semantic control into LSan failure. */
		for(i=0; i<nelem(owned); i++)
			releaseowned(owned[i]);
		exit(10);
	}
}


/* Check that staging neither publishes a descriptor nor changes a public bitmap. */
static void
drawobserve(void)
{
	uchar *maps[] = {fontmap, imagemap, screenmap, displaymap};
	ulong sizes[] = {sizeof(fontmap), sizeof(imagemap), sizeof(screenmap), sizeof(displaymap)};
	int i;
	ulong j;

	drawcheck(TFont == nil && TImage == nil && TScreen == nil && TDisplay == nil);
	for(i=0; i<4; i++)
		for(j=0; j<sizes[i]; j++)
			drawcheck(maps[i][j] == 0x5a);
	observations++;
}


/* Keep distinct real descriptor callback identities without creating GUI objects. */
static void
freedrawfont(Heap *heap, int swept)
{
	USED(heap); USED(swept);
}


/* Descriptor construction must retain the image-specific destructor identity. */
static void
freedrawimage(Heap *heap, int swept)
{
	USED(heap); USED(swept);
}


/* Descriptor construction must retain the screen-specific destructor identity. */
static void
freedrawscreen(Heap *heap, int swept)
{
	USED(heap); USED(swept);
}


/* Descriptor construction must retain the display-specific destructor identity. */
static void
freedrawdisplay(Heap *heap, int swept)
{
	USED(heap); USED(swept);
}


/* Registration is injected; the owning builtin constructor has independent gates. */
Module*
builtinmod(char *name, void *table, int length)
{
	drawobserve();
	drawcheck(strcmp(name, "$Draw") == 0 && table == Drawmodtab && length == Drawmodlen && live == 4);
	registrations++;
	if(registration){
		strcpy(diagnostic, registration == 1 ? "refused Draw ABI" : "");
		return nil;
	}
	return &baseline;
}

DRAW_INITIALIZER


/* Exercise production initialization inside the actual protected VM error body. */
static int
initialize(void *argument)
{
	USED(argument);
	drawmodinit();
	return 1;
}


/* Poison every public map so a premature publication is observable independently. */
static void
reset(void)
{
	drawcheck(live == 0 && worker.nerr == 0 && modules == &baseline);
	TFont = TImage = TScreen = TDisplay = nil;
	memset(fontmap, 0x5a, sizeof(fontmap));
	memset(imagemap, 0x5a, sizeof(imagemap));
	memset(screenmap, 0x5a, sizeof(screenmap));
	memset(displaymap, 0x5a, sizeof(displaymap));
	registration = registrations = observations = disturb = 0;
	fail_after = -1;
	strcpy(diagnostic, "before");
}


/* Compute expected map bytes from independent field offsets, checking all other bits. */
static void
descriptor(Type *type, int size, void (*destroy)(Heap*, int), ulong *offsets, int count,
	uchar *map, ulong bytes)
{
	uchar expected[64];
	ulong slot;
	int i;

	drawcheck(type != nil && type->ref == 1 && type->free == destroy && type->mark == markheap);
	drawcheck(type->size == size && type->np == bytes && bytes <= sizeof(expected));
	memset(expected, 0, sizeof(expected));
	for(i=0; i<count; i++){
		drawcheck(offsets[i]%sizeof(void*) == 0);
		slot = offsets[i]/sizeof(void*);
		drawcheck(slot/8 < bytes);
		expected[slot/8] |= 0x80 >> (slot%8);
	}
	drawcheck(memcmp(type->map, expected, bytes) == 0 && memcmp(map, expected, bytes) == 0);
}


/* Check all four real native wrapper layouts, then prove repeated init has no side effects. */
static void
successful(void)
{
	ulong fonts[] = {offsetof(DFont, drawfont.name), offsetof(DFont, drawfont.display)};
	ulong images[] = {offsetof(DImage, drawimage.display), offsetof(DImage, drawimage.screen), offsetof(DImage, drawimage.iname)};
	ulong screens[] = {offsetof(DScreen, drawscreen.image), offsetof(DScreen, drawscreen.fill), offsetof(DScreen, drawscreen.display)};
	ulong displays[] = {offsetof(DDisplay, drawdisplay.image), offsetof(DDisplay, drawdisplay.white), offsetof(DDisplay, drawdisplay.black), offsetof(DDisplay, drawdisplay.opaque), offsetof(DDisplay, drawdisplay.transparent)};
	Type *saved[4];
	int before;

	drawcheck(live == 4 && registrations == 1 && worker.nerr == 0);
	descriptor(TFont, sizeof(DFont), freedrawfont, fonts, nelem(fonts), fontmap, sizeof(fontmap));
	descriptor(TImage, sizeof(DImage), freedrawimage, images, nelem(images), imagemap, sizeof(imagemap));
	descriptor(TScreen, sizeof(DScreen), freedrawscreen, screens, nelem(screens), screenmap, sizeof(screenmap));
	descriptor(TDisplay, sizeof(DDisplay), freedrawdisplay, displays, nelem(displays), displaymap, sizeof(displaymap));
	saved[0] = TFont; saved[1] = TImage; saved[2] = TScreen; saved[3] = TDisplay;
	before = attempts;
	drawcheck(kprotect(initialize, nil) == 1 && attempts == before && registrations == 1);
	drawcheck(TFont == saved[0] && TImage == saved[1] && TScreen == saved[2] && TDisplay == saved[3]);
	freetype(TFont); freetype(TImage); freetype(TScreen); freetype(TDisplay);
	TFont = TImage = TScreen = TDisplay = nil;
	drawcheck(live == 0 && worker.nerr == 0);
}


/* Fail every allocation, registration and partial state, then retry through real dtype/freetype. */
int
main(void)
{
	Type sentinel;
	int failure, mode, noisy, mask, before;

	for(failure=0; failure<4; failure++){
		reset();
		fail_after = failure;
		drawcheck(kprotect(initialize, nil) == 0 && live == 0 && worker.nerr == 0);
		drawcheck(strcmp(diagnostic, exNomem) == 0 && registrations == 0);
		drawobserve();
		fail_after = -1;
		drawcheck(kprotect(initialize, nil) == 1);
		successful();
		cases++;
	}
	for(mode=1; mode<=2; mode++)
	for(noisy=0; noisy<2; noisy++){
		reset();
		registration = mode;
		disturb = noisy;
		drawcheck(kprotect(initialize, nil) == 0 && live == 0 && worker.nerr == 0 && registrations == 1);
		drawcheck(strcmp(diagnostic, mode == 1 ? "refused Draw ABI" : "Draw module registration failed") == 0);
		drawobserve();
		registration = registrations = disturb = 0;
		drawcheck(kprotect(initialize, nil) == 1);
		successful();
		cases++;
	}
	for(mask=1; mask<16; mask++){
		reset();
		TFont = mask & 1 ? &sentinel : nil;
		TImage = mask & 2 ? &sentinel : nil;
		TScreen = mask & 4 ? &sentinel : nil;
		TDisplay = mask & 8 ? &sentinel : nil;
		before = attempts;
		drawcheck(kprotect(initialize, nil) == (mask == 15) && attempts == before && live == 0 && registrations == 0 && worker.nerr == 0);
		drawcheck(strcmp(diagnostic, mask == 15 ? "before" : "partial Draw type initialization") == 0);
		drawcheck(TFont == (mask & 1 ? &sentinel : nil) && TImage == (mask & 2 ? &sentinel : nil));
		drawcheck(TScreen == (mask & 4 ? &sentinel : nil) && TDisplay == (mask & 8 ? &sentinel : nil));
		TFont = TImage = TScreen = TDisplay = nil;
		drawobserve();
		cases++;
	}
	printf("PASS Draw initialization WORD%d: %d failure/retry/partial cases; four native maps; idempotent ownership; preserved registration diagnostics\n", (int)sizeof(WORD)*8, cases);
	return 0;
}
