#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "draw.h"
#include "prefab.h"
#include "pool.h"
#include "raise.h"
#include "nativeptrmap.h"
#include <assert.h>
#include <stdio.h>

WRAPPERS
static struct { void *pointer; ulong size; } blocks[256];
static int live, checks, registrations;
static Type leaf = {1, 0, 0, sizeof(WORD)};
Type *TFont, *TImage, *TScreen, *TDisplay, *TElement, *TCompound, *TLayout;
uchar fontmap[] = Draw_Font_map, imagemap[] = Draw_Image_map;
uchar screenmap[] = Draw_Screen_map, displaymap[] = Draw_Display_map;
uchar elementmap[] = Prefab_Element_map, compoundmap[] = Prefab_Compound_map;
uchar layoutmap[] = Prefab_Layout_map;
static int Drawmodtab[1], Prefabmodtab[1];
enum {Drawmodlen = 1, Prefabmodlen = 1};
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
char exType[] = "type", exHeap[] = "heap";

/* Any unexpected layout or allocation error must fail the fixture. */
void
error(char *message)
{
	(void)message;
	abort();
}

/* All fixture allocations have explicit ownership accounting; LSan inspection is unavailable. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Poison each exact-sized allocation and guard its end independently of the pointer map. */
static void*
allocate(ulong size, int zero)
{
	void *pointer;
	int i;

	assert(size <= 65536);
	pointer = malloc(size+32);
	assert(pointer);
	memset(pointer, zero ? 0 : 0xa5, size);
	memset((uchar*)pointer+size, 0xa5, 32);
	for(i = 0; i < 256 && blocks[i].pointer; i++);
	assert(i < 256);
	blocks[i].pointer = pointer;
	blocks[i].size = size;
	live++;
	return pointer;
}

/* Check exact guards before releasing types, wrapper payloads or shared children. */
static void
deallocate(void *pointer)
{
	int i, j;

	if(pointer == nil)
		return;
	for(i = 0; i < 256 && blocks[i].pointer != pointer; i++);
	assert(i < 256);
	for(j = 0; j < 32; j++)
		assert(((uchar*)pointer)[blocks[i].size+j] == 0xa5);
	blocks[i].pointer = nil;
	free(pointer);
	live--;
}

/* Bind real heap construction to exact guarded native allocations. */
void*
poolalloc(Pool *pool, ulong size)
{
	(void)pool;
	return allocate(size, 0);
}

/* Descriptor mark callbacks are retained without starting a collector. */
void
markheap(Type *type, void *value)
{
	(void)type;
	(void)value;
}

/* Registration is outside this map fixture; verify only the requested builtin modules. */
Module*
builtinmod(char *name, void *table, int length)
{
	assert((strcmp(name, "$Draw") == 0 || strcmp(name, "$Prefab") == 0) && table && length == 1);
	registrations++;
	return nil;
}

/* Font finalization needs a live native font and is outside the map fixture. */
void
freedrawfont(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}

/* Image finalization needs a live native image and is outside the map fixture. */
void
freedrawimage(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}

/* Screen finalization needs a live native screen and is outside the map fixture. */
void
freedrawscreen(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}

/* Display finalization needs a live native display and is outside the map fixture. */
void
freedrawdisplay(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}

/* Compound finalization needs a live native display and is outside the map fixture. */
void
freeprefabcompound(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}

/* Map tests call production freeptrs directly instead of an object callback. */
void
freeheap(Heap *heap, int swept)
{
	(void)heap;
	(void)swept;
	abort();
}

/* Exercise real map release while keeping unrelated native graphics finalization bounded. */
void
destroy(void *value)
{
	Heap *h;

	if(value == H)
		return;
	h = D2H(value);
	assert(h->ref > 0);
	if(--h->ref)
		return;
	freeptrs(value, h->t);
	freetype(h->t);
	deallocate(h);
}

#define mallocz(n,z) allocate(n,z)
#define free(p) deallocate(p)
#define BIT(bt, nb) ((bt)&(1<<(nb)))
IMPLEMENTATION
#undef mallocz
#undef free

/* Verify all managed fields, every excluded byte and copy/release reference ownership. */
static void
one(Type *type, ulong size, ulong *offsets, int count, int alias, int populated, int order)
{
	uchar expected[256], *first, *copy;
	void *children[32];
	int i, j, baseline;

	assert(type && type->size == size && size <= sizeof(expected));
	memset(expected, 0, sizeof(expected));
	for(i = 0; i < count; i++){
		assert(offsets[i]+sizeof(void*) <= size);
		for(j = 0; j < sizeof(void*); j++)
			expected[offsets[i]+j] = 1;
	}
	for(i = 0; i < type->np*8; i++)
		assert(!!(type->map[i/8] & (0x80>>(i%8))) == !!expected[i*sizeof(void*)]);
	baseline = live;
	first = H2D(uchar*, heap(type));
	for(i = 0; i < size; i++)
		if(!expected[i])
			assert(first[i] == 0xa5);
	for(i = 0; i < count; i++)
		assert(*(void**)(first+offsets[i]) == H);
	for(i = 0; i < count; i++){
		children[i] = H;
		if(!(populated&(1<<i)))
			continue;
		if(alias && i > 0){
			for(j = 0; j < i && children[j] == H; j++);
			if(j < i){
				children[i] = children[j];
				D2H(children[i])->ref++;
			}
		}
		if(children[i] == H)
			children[i] = H2D(void*, heap(&leaf));
		*(void**)(first+offsets[i]) = children[i];
	}
	copy = H2D(uchar*, heap(type));
	memmove(copy, first, size);
	incmem(copy, type);
	for(i = 0; i < count; i++)
		assert(*(void**)(copy+offsets[i]) == children[i]);
	if(order){ destroy(first); destroy(copy); }
	else{ destroy(copy); destroy(first); }
	assert(live == baseline && type->ref == 1 && leaf.ref == 1);
	checks++;
}

/* Use actual C field addresses as an independent declaration of intended ownership. */
int
main(int argc, char **argv)
{
	ulong font[] = {offsetof(DFont, drawfont.name), offsetof(DFont, drawfont.display)};
	ulong image[] = {offsetof(DImage, drawimage.display), offsetof(DImage, drawimage.screen), offsetof(DImage, drawimage.iname)};
	ulong screen[] = {offsetof(DScreen, drawscreen.image), offsetof(DScreen, drawscreen.fill), offsetof(DScreen, drawscreen.display)};
	ulong display[] = {offsetof(DDisplay, drawdisplay.image), offsetof(DDisplay, drawdisplay.white), offsetof(DDisplay, drawdisplay.black), offsetof(DDisplay, drawdisplay.opaque), offsetof(DDisplay, drawdisplay.transparent)};
	ulong element[] = {offsetof(PElement, e.environ), offsetof(PElement, e.tag), offsetof(PElement, e.kids), offsetof(PElement, e.str), offsetof(PElement, e.mask), offsetof(PElement, e.image), offsetof(PElement, e.font)};
	ulong compound[] = {offsetof(PCompound, c.image), offsetof(PCompound, c.environ), offsetof(PCompound, c.title), offsetof(PCompound, c.contents)};
	ulong layout[] = {offsetof(Prefab_Layout, font), offsetof(Prefab_Layout, color), offsetof(Prefab_Layout, text), offsetof(Prefab_Layout, icon), offsetof(Prefab_Layout, mask), offsetof(Prefab_Layout, tag)};
	struct Record { Type *type; ulong size, *offsets; int count; } records[7];
	int k, alias, mask, order;

	drawmodinit();
	prefabmodinit();
	assert(registrations == 2);
	records[0] = (struct Record){TFont, sizeof(DFont), font, 2};
	records[1] = (struct Record){TImage, sizeof(DImage), image, 3};
	records[2] = (struct Record){TScreen, sizeof(DScreen), screen, 3};
	records[3] = (struct Record){TDisplay, sizeof(DDisplay), display, 5};
	records[4] = (struct Record){TElement, sizeof(PElement), element, 7};
	records[5] = (struct Record){TCompound, sizeof(PCompound), compound, 4};
	records[6] = (struct Record){TLayout, sizeof(Prefab_Layout), layout, 6};
	for(k = 0; k < 7; k++){
		if(argc > 1 && strcmp(argv[1], k < 4 ? "draw" : "prefab") != 0)
			continue;
		for(alias = 0; alias < 2; alias++)for(order = 0; order < 2; order++)
		for(mask = 0; mask < (1<<records[k].count); mask++)
			one(records[k].type, records[k].size, records[k].offsets, records[k].count, alias, mask, order);
	}
	for(k = 0; k < 7; k++){
		assert(records[k].type && records[k].type->ref == 1);
		freetype(records[k].type);
	}
	assert(live == 0 && checks > 0);
	printf("GUI type maps: %d initialization/copy/alias/release cases\n", checks);
	return 0;
}
