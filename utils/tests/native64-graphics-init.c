#include "runt.h"
#include "draw.h"
#include "nativeptrmap.h"
GRAPHICS_HEADERS

char exType[] = "type"; // Existing VM identity for invalid native field maps.
GRAPHICS_DECLARATIONS

GRAPHICS_CALLBACK

/* Describe only public managed fields; private native aliases are deliberately absent. */
typedef struct GraphicsShape GraphicsShape;

struct GraphicsShape {
	Type **owner; // Creator owner published after module construction.
	int size; // Actual target C record extent, including private wrapper bytes.
	void (*destroy)(Heap*, int); // Expected descriptor callback identity.
	uchar *map; // Public bitmap checked for premature mutation.
	ulong bytes; // Bitmap capacity from the owning source declaration.
	ulong offsets[8]; // Independently enumerated managed field offsets.
	int count; // Number of managed fields in this record.
};
GRAPHICS_SHAPES
static int registration, registrations, cases; // Injected registration refusal and observed counts.


/* Preserve a strict semantic exit while cleaning only fixture storage after failure. */
static void
graphicscheck(int condition)
{
	int i;

	if(!condition){
		fprintf(stderr, "Graphics initialization mismatch\n");
		for(i=0; i<nelem(owned); i++)
			releaseowned(owned[i]);
		exit(10);
	}
}


/* No allocation/registration callback may see staged Types or modified public maps. */
static void
graphicsobserve(void)
{
	ulong j;
	int i;

	for(i=0; i<nelem(shapes); i++){
		graphicscheck(*shapes[i].owner == nil);
		for(j=0; j<shapes[i].bytes; j++)
			graphicscheck(shapes[i].map[j] == 0x5a);
	}
}


/* Real builtin construction is separate; this fixture observes caller staging and refusal. */
Module*
builtinmod(char *name, void *table, int length)
{
	graphicsobserve();
	graphicscheck(strcmp(name, "$" GUI_NAME) == 0 && table == GUI_TABLE && length == GUI_LENGTH);
	graphicscheck(live == nelem(shapes));
	registrations++;
	if(registration){
		strcpy(diagnostic, registration == 1 ? "refused graphics ABI" : "");
		return nil;
	}
	return &baseline;
}

GRAPHICS_INITIALIZER


/* Enter through the actual owning protected VM helper, with no graphical objects allocated. */
static int
initialize(void *argument)
{
	USED(argument);
	GUI_INITIALIZE();
	return 1;
}


/* Each episode starts without descriptor owners and with independent poisoned public maps. */
static void
reset(void)
{
	int i;

	graphicscheck(live == 0 && worker.nerr == 0 && modules == &baseline);
	for(i=0; i<nelem(shapes); i++){
		*shapes[i].owner = nil;
		memset(shapes[i].map, 0x5a, shapes[i].bytes);
	}
	registration = registrations = disturb = 0;
	fail_after = -1;
	strcpy(diagnostic, "before");
}


/* Check every bitmap byte and callback, then require side-effect-free complete reinitialization. */
static void
successful(void)
{
	Type *type, *saved[4];
	uchar expected[64];
	ulong slot;
	int i, j, before;

	graphicscheck(live == nelem(shapes) && registrations == 1 && worker.nerr == 0);
	for(i=0; i<nelem(shapes); i++){
		type = *shapes[i].owner;
		graphicscheck(type != nil && type->ref == 1 && type->size == shapes[i].size);
		graphicscheck(type->free == shapes[i].destroy && type->mark == markheap);
		graphicscheck(type->np == shapes[i].bytes && shapes[i].bytes <= sizeof(expected));
		memset(expected, 0, sizeof(expected));
		for(j=0; j<shapes[i].count; j++){
			graphicscheck(shapes[i].offsets[j]%sizeof(void*) == 0);
			slot = shapes[i].offsets[j]/sizeof(void*);
			graphicscheck(slot/8 < shapes[i].bytes);
			expected[slot/8] |= 0x80 >> (slot%8);
		}
		graphicscheck(memcmp(type->map, expected, shapes[i].bytes) == 0);
		graphicscheck(memcmp(shapes[i].map, expected, shapes[i].bytes) == 0);
		saved[i] = type;
	}
	before = attempts;
	graphicscheck(kprotect(initialize, nil) == 1 && attempts == before && registrations == 1);
	for(i=0; i<nelem(shapes); i++){
		graphicscheck(*shapes[i].owner == saved[i]);
		freetype(*shapes[i].owner);
		*shapes[i].owner = nil;
	}
	graphicscheck(live == 0 && worker.nerr == 0);
}


/* Fail every allocation/registration, retry, and preserve each partial or complete state. */
int
main(void)
{
	Type sentinel;
	int i, failure, mode, noisy, mask, before;

	graphicscheck(nelem(shapes) <= 4);
	for(failure=0; failure<nelem(shapes); failure++){
		reset();
		fail_after = failure;
		graphicscheck(kprotect(initialize, nil) == 0 && live == 0 && worker.nerr == 0);
		graphicscheck(strcmp(diagnostic, exNomem) == 0 && registrations == 0);
		graphicsobserve();
		fail_after = -1;
		graphicscheck(kprotect(initialize, nil) == 1);
		successful();
		cases++;
	}
	for(mode=1; mode<=2; mode++)
	for(noisy=0; noisy<2; noisy++){
		reset();
		registration = mode;
		disturb = noisy;
		graphicscheck(kprotect(initialize, nil) == 0 && live == 0 && worker.nerr == 0 && registrations == 1);
		graphicscheck(strcmp(diagnostic, mode == 1 ? "refused graphics ABI" : GUI_NAME " module registration failed") == 0);
		graphicsobserve();
		registration = registrations = disturb = 0;
		graphicscheck(kprotect(initialize, nil) == 1);
		successful();
		cases++;
	}
	for(mask=1; mask<(1<<nelem(shapes)); mask++){
		reset();
		for(i=0; i<nelem(shapes); i++)
			*shapes[i].owner = mask & (1<<i) ? &sentinel : nil;
		before = attempts;
		graphicscheck(kprotect(initialize, nil) == (mask == (1<<nelem(shapes))-1));
		graphicscheck(attempts == before && live == 0 && registrations == 0 && worker.nerr == 0);
		graphicscheck(strcmp(diagnostic, mask == (1<<nelem(shapes))-1 ? "before" : "partial " GUI_NAME " type initialization") == 0);
		for(i=0; i<nelem(shapes); i++){
			graphicscheck(*shapes[i].owner == (mask & (1<<i) ? &sentinel : nil));
			*shapes[i].owner = nil;
		}
		graphicsobserve();
		cases++;
	}
	printf("PASS %s initialization WORD%d: %d failure/retry/partial cases; native managed-field maps; private aliases excluded\n", GUI_NAME, (int)sizeof(WORD)*8, cases);
	return 0;
}
