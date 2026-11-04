#include "lib9.h"
#include "draw.h"
#include <stdio.h>
#include <stdarg.h>

#define Blocks 16 // Maximum owned native table allocations in a scenario.

struct Block {
	void *pointer; // Actual allocated table storage.
	ulong bytes; // Exact source capacity, excluding any extra readable memory.
};

static struct Block blocks[Blocks]; // Table ownership ledger, never fabricated VM Heap metadata.
static int checks, allocations, failallocation; // Active predicates and injected table allocation refusal.
static int flushes, failflush, reservations, failreservation; // Controlled native transport boundaries.
static int oldlive, newlive, imagefailure, oldfrees, newfrees; // Bounded native image service ownership.
static uchar packet[64]; // One complete observed initialization or glyph packet.
static int packetsize; // Length of the latest successful reservation.
static Display display; // Independent native display identity.
static Image oldimage, newimage, glyphimage; // Bounded native image service handles.
static Subfont incoming, retained; // Independently held glyph sources.
static Fontchar metrics[257]; // Complete adjacent-metric storage.


/* Free only actual fixture-owned tables when stopping before an unsafe omission. */
static void
check(int ok, char *stage)
{
	int i;

	if(!ok){
		fprintf(stderr, "Font cache mismatch: %s\n", stage);
		for(i = 0; i < Blocks; i++)
			free(blocks[i].pointer);
		exit(10);
	}
	checks++;
}


/* Poison each exact table extent to expose missing initialization after growth. */
static void*
tableallocate(ulong bytes)
{
	int i;
	void *p;

	if(++allocations == failallocation)
		return nil;
	p = malloc(bytes);
	check(p != nil, "fixture allocation");
	memset(p, 0xa5, bytes);
	for(i = 0; i < Blocks; i++)
		if(blocks[i].pointer == nil){
			blocks[i].pointer = p;
			blocks[i].bytes = bytes;
			return p;
		}
	free(p);
	check(0, "fixture ledger");
	return nil;
}


/* Observe exact retirement and refuse accidental stack/static table release. */
static void
tableretire(void *p)
{
	int i;

	if(p == nil)
		return;
	for(i = 0; i < Blocks; i++)
		if(blocks[i].pointer == p){
			blocks[i].pointer = nil;
			free(p);
			return;
		}
	check(0, "table retirement");
}


/* Catch an oversized source copy before executing it, including omission controls. */
static void*
tablecopy(void *dst, void *src, ulong bytes)
{
	int i;

	for(i = 0; i < Blocks; i++)
		if(blocks[i].pointer == src){
			check(bytes <= blocks[i].bytes, "source extent");
			return memmove(dst, src, bytes);
		}
	check(0, "copy source");
	return nil;
}


/* Supply a separately owned native subfont, leaving its cache algorithm out of scope. */
static Subfont*
cf2subfont(Cachefont *cf, Font *f)
{
	USED(cf); USED(f);
	incoming.ref++;
	return &incoming;
}


/* Track retained source ownership independently from table bytes. */
void
freesubfont(Subfont *sf)
{
	check((sf == &incoming || sf == &retained) && sf->ref > 0, "subfont retirement");
	sf->ref--;
}


/* Refuse selected flushes without pretending that a remote server rolled back. */
int
flushimage(Display *d, int visible)
{
	USED(visible);
	check(d == &display, "flush display");
	return ++flushes == failflush ? -1 : 0;
}


/* Reserve a complete bounded packet, never an image renderer. */
uchar*
bufimage(Display *d, int bytes)
{
	check(d == &display && bytes > 0 && bytes <= sizeof packet, "packet extent");
	if(++reservations == failreservation)
		return nil;
	packetsize = bytes;
	memset(packet, 0xcc, sizeof packet);
	return packet;
}


/* Expose native image creation as an explicit service with an independent owner bit. */
Image*
allocimage(Display *d, Rectangle r, ulong chan, int repl, ulong color)
{
	USED(chan); USED(repl); USED(color);
	check(d == &display && !newlive && r.min.x == 0 && r.min.y == 0, "image request");
	if(imagefailure)
		return nil;
	newimage.r = r;
	newlive = 1;
	return &newimage;
}


/* A native image release consumes its local owner even when device status is separate. */
int
freeimage(Image *image)
{
	if(image == &oldimage){
		check(oldlive, "old image ownership");
		oldlive = 0; oldfrees++;
	}else{
		check(image == &newimage && newlive, "new image ownership");
		newlive = 0; newfrees++;
	}
	return 0;
}


/* The ascent-adjustment drawing path is not exercised by these cache scenarios. */
void
draw(Image *dst, Rectangle r, Image *src, Image *mask, Point p)
{
	USED(dst); USED(&r); USED(src); USED(mask); USED(&p);
	check(0, "unexpected ascent adjustment");
}


/* Keep native diagnostics distinct from acceptance of the attempted operation. */
int
_drawprint(int fd, char *format, ...)
{
	USED(fd); USED(format);
	return 0;
}

static int fontresize(Font*, int, int, int); // Actual private implementation follows.
#define malloc tableallocate // Inject only native table allocation.
#define free tableretire // Observe table retirement.
#define memmove tablecopy // Enforce the source allocation's exact extent.
IMPLEMENTATION
#undef malloc
#undef free
#undef memmove


/* Construct independently owned cache tables with one live young source entry. */
static void
setup(Font *f, Cachefont *cf, Cachefont **catalog, int width)
{
	memset(f, 0, sizeof *f);
	memset(cf, 0, sizeof *cf);
	memset(&incoming, 0, sizeof incoming);
	memset(&retained, 0, sizeof retained);
	memset(metrics, 0, sizeof metrics);
	memset(&display, 0, sizeof display);
	memset(&oldimage, 0, sizeof oldimage);
	memset(&newimage, 0, sizeof newimage);
	memset(&glyphimage, 0, sizeof glyphimage);
	allocations = failallocation = flushes = failflush = reservations = failreservation = 0;
	imagefailure = oldfrees = newfrees = newlive = packetsize = 0;
	oldlive = 1;
	oldimage.display = newimage.display = glyphimage.display = &display;
	oldimage.id = 7; newimage.id = 8; glyphimage.id = 9; glyphimage.depth = 2;
	incoming.ref = retained.ref = 1;
	incoming.n = 256; incoming.ascent = 5; incoming.height = 15;
	incoming.info = metrics; incoming.bits = &glyphimage;
	metrics['A'].x = 100; metrics['A'+1].x = 110;
	metrics['A'].top = 2; metrics['A'].bottom = 14;
	metrics['A'].left = -3; metrics['A'].width = 7;
	cf->min = 0; cf->max = 255; cf->offset = 0;
	catalog[0] = cf;
	f->sub = catalog; f->nsub = 1; f->height = 15; f->ascent = 5;
	f->width = width; f->maxdepth = width >= 10 ? 2 : 1;
	f->display = &display; f->cacheimage = &oldimage; f->age = 2;
	f->ncache = NFCACHE+NFLOOK;
	f->cache = tableallocate(f->ncache*sizeof *f->cache);
	memset(f->cache, 0, f->ncache*sizeof *f->cache);
	f->nsubf = 1;
	f->subf = tableallocate(sizeof *f->subf);
	memset(f->subf, 0, sizeof *f->subf);
	f->subf[0].age = 1; f->subf[0].f = &retained;
}


/* Retire every successful or refused scenario back to an empty table ledger. */
static void
finish(Font *f)
{
	int i;

	for(i = 0; i < f->nsubf; i++)
		if(f->subf[i].f != nil)
			freesubfont(f->subf[i].f);
	tableretire(f->subf);
	tableretire(f->cache);
	check(incoming.ref == 1 && retained.ref == 0, "source references");
	for(i = 0; i < Blocks; i++)
		check(blocks[i].pointer == nil, "table baseline");
	check(oldlive+newlive == 1, "image baseline");
}


/* Check populated growth, initialization refusals and glyph publication before reuse. */
int
main(void)
{
	Font f;
	Cachefont cf, *catalog[1];
	Cachesubf *old;
	Cacheinfo saved;
	char *name;
	int status, failure, i, width, before, population;
	char *text;
	ushort output[2];
	Cacheinfo *oldcache;

	/* Grow a populated table without reading beyond the old allocation. */
	setup(&f, &cf, catalog, 16);
	old = f.subf;
	status = loadchar(&f, 'A', f.cache, 0, 0, &name);
	check(status == 1 && f.nsubf == 1+DSUBF && f.subf != old, "populated growth");
	check(f.subf[0].f == &retained && f.subf[0].age == 1, "retained entry");
	check(f.subf[1].f == &incoming && f.subf[1].cf == &cf, "new source");
	for(i = 2; i < f.nsubf; i++)
		check(f.subf[i].f == nil && f.subf[i].cf == nil && f.subf[i].age == 0, "zero grown entries");
	check(packetsize == 37 && packet[0] == 'l' && packet[35] == 253 && packet[36] == 7, "glyph packet");
	check(f.cache[0].value == 'A' && f.cache[0].width == 7 && f.cache[0].left == -3, "glyph fields");
	finish(&f);
	/* Neither a failed flush nor a failed reservation may publish a glyph. */
	for(failure = 1; failure <= 2; failure++){
		setup(&f, &cf, catalog, 16);
		f.cache[0].value = 123; f.cache[0].width = 99; f.cache[0].left = 2;
		saved = f.cache[0];
		if(failure == 1) failflush = 1;
		else failreservation = 1;
		status = loadchar(&f, 'A', f.cache, 0, 0, &name);
		check(status == 0 && memcmp(f.cache, &saved, sizeof saved) == 0, "glyph publication");
		check(failure != 1 || reservations == 0, "glyph flush order");
		finish(&f);
	}
	/* Every failed image-initialization stage retains the old geometry and owner. */
	for(failure = 1; failure <= 4; failure++){
		setup(&f, &cf, catalog, 8);
		if(failure == 1) imagefailure = 1;
		if(failure == 2) failflush = 1;
		if(failure == 3) failreservation = 1;
		if(failure == 4) failflush = 2;
		status = loadchar(&f, 'A', f.cache, 0, 0, &name);
		check(status == 0 && f.width == 8 && f.maxdepth == 1 && f.cacheimage == &oldimage, "resize rollback");
		check(oldlive && !newlive && oldfrees == 0 && newfrees == (failure != 1), "resize owners");
		check(failure != 2 || reservations == 0, "resize flush order");
		finish(&f);
	}
	setup(&f, &cf, catalog, 8);
	status = loadchar(&f, 'A', f.cache, 0, 0, &name);
	check(status == 1 && f.width == 10 && f.maxdepth == 2 && f.cacheimage == &newimage, "resize commit");
	check(!oldlive && newlive && oldfrees == 1 && newfrees == 0, "resize retirement");
	check(newimage.r.max.x == (NFCACHE+NFLOOK)*10 && newimage.r.max.y == 15, "resize geometry");
	finish(&f);
	setup(&f, &cf, catalog, 16);
	old = f.subf;
	failallocation = allocations+1;
	status = loadchar(&f, 'A', f.cache, 0, 0, &name);
	check(status == 1 && f.subf == old && f.nsubf == 1 && retained.ref == 0, "growth allocation fallback");
	finish(&f);
	setup(&f, &cf, catalog, 8);
	status = loadchar(&f, 'A', f.cache, 0, 1, &name);
	check(status == -1 && f.width == 8 && f.maxdepth == 1 && flushes == 0 && reservations == 0, "pending batch retry");
	finish(&f);
	for(failure = 1; failure <= 2; failure++){
		setup(&f, &cf, catalog, 8);
		if(failure == 1) failflush = 3;
		else failreservation = 2;
		status = loadchar(&f, 'A', f.cache, 0, 0, &name);
		check(status == 0 && f.cacheimage == &newimage && f.width == 10 && f.maxdepth == 2, "post-resize refusal");
		check(f.cache[0].value == 0 && f.cache[0].width == 0 && f.cache[0].age == 0, "post-resize publication");
		finish(&f);
	}
	for(failure = 0; failure <= 1; failure++){
		setup(&f, &cf, catalog, 8);
		oldcache = f.cache;
		if(failure) failallocation = allocations+1;
		status = fontresize(&f, 10, 2*NFCACHE+NFLOOK, 2);
		check(f.cacheimage == &newimage && f.width == 10 && f.maxdepth == 2, "cache-table image commit");
		check(failure ? (status == 1 && f.cache == oldcache && f.ncache == NFCACHE+NFLOOK) :
			(status == 0 && f.cache != oldcache && f.ncache == 2*NFCACHE+NFLOOK), "cache-table retry");
		for(i = 0; i < f.ncache; i++)
			check(f.cache[i].value == 0 && f.cache[i].age == 0 && f.cache[i].width == 0, "cache-table initialization");
		finish(&f);
	}
	setup(&f, &cf, catalog, 16);
	text = "AA";
	status = cachechars(&f, &text, nil, output, 2, &width, &name);
	check(status == 2 && width == 14 && *text == 0, "actual cache batch");
	check(output[0] == output[1] && output[0] == ((17*'A') & (NFCACHE-1)), "cache hash reuse");
	check(f.cache[output[0]].age == f.age && f.cache[output[0]].left == -3, "cache batch publication");
	before = reservations;
	text = "AA";
	status = cachechars(&f, &text, nil, output, 2, &width, &name);
	check(status == 2 && width == 14 && reservations == before, "warm batch reuse");
	finish(&f);
	/* Check growth at the final young-table boundary and eviction above it. */
	for(population = 5; population <= 53; population += (population == 5 ? 44 : 4)){
		setup(&f, &cf, catalog, 16);
		tableretire(f.subf);
		f.nsubf = population;
		f.subf = tableallocate(population*sizeof *f.subf);
		memset(f.subf, 0, population*sizeof *f.subf);
		retained.ref = population;
		for(i = 0; i < population; i++){
			f.subf[i].age = 1;
			f.subf[i].f = &retained;
		}
		old = f.subf;
		status = loadchar(&f, 'A', f.cache, 0, 0, &name);
		check(status == 1, "populated boundary load");
		check(population <= MAXSUBF ? (f.nsubf == population+DSUBF && f.subf != old && retained.ref == population) :
			(f.nsubf == population && f.subf == old && retained.ref == population-1), "bounded growth or eviction");
		finish(&f);
	}
	/* Packed cache fields keep their short/ushort limits on every native64 target. */
	for(failure = 0; failure < 5; failure++){
		setup(&f, &cf, catalog, 8);
		width = failure == 0 ? 32768 : failure == 1 ? -1 : failure == 4 ? 64 : 10;
		population = failure == 2 ? MAXFCACHE+1 : failure == 3 ? NFLOOK : failure == 4 ? MAXFCACHE : f.ncache;
		before = allocations;
		status = fontresize(&f, width, population, 2);
		check(status == 0 && allocations == before && flushes == 0 && reservations == 0, "cache capacity");
		check(f.width == 8 && f.maxdepth == 1 && oldlive && !newlive && f.cacheimage == &oldimage, "capacity geometry");
		finish(&f);
	}
	setup(&f, &cf, catalog, 8);
	status = fontresize(&f, 65535/(f.ncache-1), f.ncache, 2);
	check(status == 1 && f.width == 65535/(f.ncache-1) && f.cacheimage == &newimage, "cache capacity endpoint");
	finish(&f);
	printf("PASS populated font cache: %d active checks\n", checks);
	return 0;
}
