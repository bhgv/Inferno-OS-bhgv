#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "draw.h"
#include <assert.h>
#include <stdio.h>

static Screen screen, wrong;
static Draw_Screen handle;
static Display display;
static Draw_Image handles[8];
static Image images[8], *expected[8];
static int live, allocations, failalloc, locked, calls, flushed, expectedcount, checks;

/* Retain ASan allocator checking without unsupported LeakSanitizer enumeration. */
int __lsan_is_turned_off(void) { return 1; }

/* Check the screen handle even when the caller supplies an empty array. */
Screen* checkscreen(Draw_Screen *s) { assert(s == &handle); return &screen; }

/* Resolve known image wrappers; a selected native image may have disappeared. */
Image*
lookupimage(Draw_Image *h)
{
	int index = h-handles;
	assert(index >= 0 && index < 8);
	return images[index].id == -1 ? nil : &images[index];
}

/* Capture native pointer-vector allocation, with deterministic failure injection. */
static void*
windowmalloc(ulong n)
{
	void *p;
	assert(n > 0 && n <= 8*sizeof(Image*));
	allocations++;
	if(failalloc)return nil;
	p = malloc(n); assert(p != nil); live++;
	return p;
}

/* The pointer vector must be released on every early return and successful call. */
static void windowfree(void *p) { if(p != nil)live--; free(p); }
int lockdisplay(Display *d) { assert(d == &display && locked++ == 0); return 1; }
void unlockdisplay(Display *d) { assert(d == &display && --locked == 0); }
int flushimage(Display *d, int visible) { assert(d == &display && visible == 1 && locked == 1); flushed++; return 0; }

/* Check filtered order exactly as the server-facing window operation receives it. */
static void
arrange(Image **list, int count)
{
	assert(locked == 1 && count == expectedcount);
	for(int i = 0; i < count; i++)assert(list[i] == expected[i]);
	calls++;
}

#define malloc windowmalloc
#define free windowfree
IMPLEMENTATION
#undef malloc
#undef free

/* Enumerate nil-element masks and reject mixed-screen/dead-image groups atomically. */
int
main(void)
{
	Array array;
	Draw_Image *data[8];
	int mask, failure, i;
	memset(&array, 0, sizeof array);
	screen.display = &display;
	array.data = (uchar*)data;
	screentopbot(&handle, H, arrange);
	screentopbot(&handle, nil, arrange);
	screentopbot(&handle, &array, arrange);
	array.len = -1; screentopbot(&handle, &array, arrange);
	array.len = 0x7fffffff; screentopbot(&handle, &array, arrange);
	assert(allocations == 0 && live == 0 && locked == 0 && calls == 0);
	checks += 5;
	array.len = 8;
	for(mask = 0; mask < 256; mask++)
		for(failure = 0; failure < 4; failure++){
			expectedcount = 0;
			for(i = 0; i < 8; i++){
				data[i] = mask&(1<<i) ? &handles[i] : H;
				images[i].screen = &screen; images[i].id = i;
				if(data[i] != H)expected[expectedcount++] = &images[i];
			}
			if(failure == 2)images[3].screen = &wrong;
			if(failure == 3)images[3].id = -1;
			failalloc = failure == 1;
			allocations = calls = flushed = 0;
			screentopbot(&handle, &array, arrange);
			assert(live == 0 && locked == 0 && allocations == 1);
			if(failure == 1 || expectedcount == 0 || (failure >= 2 && (mask&8)))
				assert(calls == 0 && flushed == 0);
			else
				assert(calls == 1 && flushed == 1);
			checks++;
		}
	printf("draw windows: %d checks passed\n", checks);
	return 0;
}
