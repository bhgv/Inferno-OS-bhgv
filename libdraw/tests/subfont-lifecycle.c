#include "lib9.h"
#include "draw.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

static int live, calls, failat, checks, readcalls, failread, writecalls, failwrite;
static int locks, keyfail;
static char header[36];
static Display display;

/* Keep sanitizers active without unsupported process enumeration. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Fail one allocation and count every remaining owner independently of ASan. */
static void*
talloc(ulong n)
{
	void *p;
	if(++calls == failat)
		return nil;
	assert(n <= 1024*1024);
	p = malloc(n);
	assert(p != nil);
	live++;
	return p;
}

/* Release only successful allocations; callers may pass NULL during cleanup. */
static void
tfree(void *p)
{
	if(p != nil)
		live--;
	free(p);
}

/* Copy names through the same allocation fault schedule as metric buffers. */
static char*
tdup(char *s)
{
	char *p = talloc(strlen(s)+1);
	if(p != nil)
		strcpy(p, s);
	return p;
}

/* Serve exact header and packed metrics spans, or truncate either operation. */
static int
libreadn(int fd, void *p, long n)
{
	assert(fd == 7 && n > 0);
	readcalls++;
	if(readcalls == failread)
		return n-1;
	if(readcalls == 1){
		assert(n == 36);
		memmove(p, header, n);
	}else{
		memset(p, 0, n);
		((uchar*)p)[n-6] = 1;
	}
	return n;
}

/* Reject NULL data and retain the native short-write behavior under test. */
static int
libwrite(int fd, void *p, int n)
{
	assert(fd == 7 && p != nil && n > 0);
	writecalls++;
	return writecalls == failwrite ? n-1 : n;
}

/* Model image ownership without contacting a display server. */
Image*
readimage(Display *d, int fd, int dolock)
{
	Image *i;
	assert(d == &display && fd == 7);
	USED(dolock);
	i = talloc(sizeof *i);
	if(i != nil){
		memset(i, 0, sizeof *i);
		i->display = d;
	}
	return i;
}

/* Count display locking while allowing normal unlocked destruction by callers. */
int lockdisplay(Display *d) { assert(d == &display); assert(locks++ == 0); return 1; }
void unlockdisplay(Display *d) { assert(d == &display); assert(--locks == 0); }
int freeimage(Image *i) { assert(i != nil); tfree(i); return 0; }
void kwerrstr(char *fmt, ...) { USED(fmt); }
int sprint(char *s, char *fmt, ...) { int n; va_list ap; va_start(ap, fmt); n = vsprintf(s, fmt, ap); va_end(ap); return n; }

/* Restrict this fixture's cache-name resolution to a predictable TrueType path. */
int is_subfont_ft(char *s) { return strstr(s, ".ttf") != nil; }
char* mangle_subfont_ft(char *s, int size) { USED(s); USED(size); return keyfail ? nil : tdup("font.ttf[12]"); }
char* subfontname(char *s, char *font, int depth) { USED(s); USED(font); USED(depth); return tdup("font.ttf"); }

#define malloc talloc
#define free tfree
#define strdup tdup
IMPLEMENTATION
#undef malloc
#undef free
#undef strdup

/* Construct an exact fixed-width header with no extra byte available to atoi. */
static void
setheader(int n, int height, int ascent)
{
	char text[100];
	snprintf(text, sizeof text, "%11d %11d %11d ", n, height, ascent);
	memmove(header, text, 36);
}

/* Cover image transfer, every allocation boundary and both truncated reads. */
static void
readcase(int provided, int dolock, int failure, int truncated)
{
	Image *i = nil;
	Subfont *f;

	assert(live == 0 && locks == 0 && lastsubfont == nil);
	failat = 0;
	if(provided)
		i = readimage(&display, 7, 0);
	calls = readcalls = 0;
	failat = failure;
	failread = truncated;
	setheader(1, 12, 9);
	f = readsubfonti(&display, "font", 7, i, dolock);
	if(failure == 0 && truncated == 0)
		assert(f != nil);
	if(truncated != 0)
		assert(f == nil);
	if(f != nil){
		assert(f->name != nil && strcmp(f->name, "font") == 0);
		assert(f->n == 1 && f->height == 12 && f->ascent == 9);
		assert(f->info[1].x == 1);
		assert(!provided || f->bits == i);
		freesubfont(f);
	}else if(provided)
		freeimage(i);
	assert(live == 0 && locks == 0 && lastsubfont == nil && lastname == nil);
	failat = failread = 0;
	checks++;
}

/* Exercise rejected fields while preserving an externally supplied image. */
static void
badheader(void)
{
	Image *i;
	Subfont *f;
	i = readimage(&display, 7, 0);
	readcalls = 0;
	f = readsubfonti(&display, "font", 7, i, 1);
	assert(f == nil && readcalls == 1 && live == 1 && locks == 0);
	freeimage(i);
	assert(live == 0);
	checks++;
}

/* Accept the representable count and metric endpoints on either char model. */
static void
validheaders(void)
{
	int n, height, ascent;
	Subfont *f;

	for(n = 0; n <= 32767; n += 32767)
		for(height = 1; height <= 255; height += 254){
			ascent = (char)-1 < 0 ? -128 : 255;
			setheader(n, height, ascent);
			readcalls = 0;
			f = readsubfont(&display, "font", 7, 1);
			assert(f != nil && f->n == n && f->height == height);
			assert(f->ascent == ascent);
			freesubfont(f);
			assert(live == 0 && locks == 0);
			checks++;
		}
}

/* Reject narrowing at the public constructor before claiming caller resources. */
static void
constructorbounds(void)
{
	static const int values[] = {-2147483647, -129, -1, 0, 128, 256, 32768, 2147483647};
	int field, index, n, height, ascent, v;

	for(field = 0; field < 3; field++)
		for(index = 0; index < sizeof values/sizeof values[0]; index++){
			n = 1; height = 12; ascent = 9;
			v = values[index];
			if(field == 0){ if(v >= 0 && v <= 32767)continue; n = v; }
			if(field == 1){ if(v > 0 && v <= 255)continue; height = v; }
			if(field == 2){ if(v == (char)v)continue; ascent = v; }
			assert(allocsubfont(nil, n, height, ascent, nil, nil) == nil);
			assert(live == 0);
			checks++;
		}
}

/* Verify cache replacement remains atomic when copying its name fails. */
static void
cachecase(void)
{
	Subfont a, b;
	Image image;
	memset(&a, 0, sizeof a);
	memset(&b, 0, sizeof b);
	memset(&image, 0, sizeof image);
	image.display = &display;
	a.bits = b.bits = &image;
	a.ref = b.ref = 1;
	installsubfont("one", &a);
	failat = calls+1;
	installsubfont("two", &b);
	failat = 0;
	assert(lastsubfont == &a && strcmp(lastname, "one") == 0 && live == 1);
	assert(lookupsubfont(&display, "one") == &a && a.ref == 2);
	installsubfont("two", &b);
	assert(lastsubfont == &b && live == 1);
	uninstallsubfont(&a);
	assert(lastsubfont == &b && live == 1);
	uninstallsubfont(&b);
	assert(lastsubfont == nil && lastname == nil && live == 0);
	checks++;
}

/* Drive packed-metric write errors and cache-key allocation failure. */
int
main(void)
{
	int provided, locked, failure, truncated, field, value;
	Fontchar info[2];
	Subfont f;
	Font font;
	Cachefont cf;
	static const int invalid[] = {-2147483647, -129, -1, 0, 128, 256, 32768, 2147483647};

	for(provided = 0; provided <= 1; provided++)
		for(locked = 0; locked <= 1; locked++)
			for(failure = 0; failure <= 7; failure++)
				for(truncated = 0; truncated <= 2; truncated++)
					readcase(provided, locked, failure, truncated);
	for(field = 0; field < 3; field++){
		for(value = 0; value < sizeof invalid/sizeof invalid[0]; value++){
			int n = 1, height = 12, ascent = 9, v = invalid[value];
			if(field == 0){ if(v >= 0 && v <= 32767)continue; n = v; }
			if(field == 1){ if(v > 0 && v <= 255)continue; height = v; }
			if(field == 2){ if(v == (char)v)continue; ascent = v; }
			setheader(n, height, ascent);
			badheader();
		}
		setheader(1, 12, 9);
		memset(header+field*12, '9', 12);
		badheader();
		setheader(1, 12, 9);
		header[field*12+10] = 'x';
		badheader();
	}
	validheaders();
	constructorbounds();
	cachecase();
	memset(&f, 0, sizeof f);
	memset(info, 0, sizeof info);
	f.n = 1; f.height = 12; f.ascent = 9; f.info = info;
	for(failure = 0; failure <= 2; failure++){
		writecalls = 0; failwrite = failure;
		assert(writesubfont(7, &f) == (failure ? -1 : 0));
		assert(live == 0);
		checks++;
	}
	memset(&font, 0, sizeof font);
	memset(&cf, 0, sizeof cf);
	font.display = &display; font.height = 12;
	cf.subfontname = "font.ttf";
	keyfail = 1;
	assert(cf2subfont(&cf, &font) == nil && live == 0);
	checks++;
	printf("subfont lifecycle: %d checks passed\n", checks);
	return 0;
}
