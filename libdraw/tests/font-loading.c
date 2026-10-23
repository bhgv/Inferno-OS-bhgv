#include "lib9.h"
#include "draw.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>

static int live, calls, failat, checks, closes, reads;
static vlong filelength;

/* Disable unsupported process inspection; ownership is counted explicitly. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Count successful allocations and fail exactly one requested allocation. */
static void*
talloc(ulong n)
{
	void *p;
	if(++calls == failat)
		return nil;
	p = malloc(n);
	assert(p != nil);
	live++;
	return p;
}

/* Preserve the original owner on failed growth, as realloc requires. */
static void*
trealloc(void *p, ulong n)
{
	void *q;
	if(++calls == failat)
		return nil;
	q = realloc(p, n);
	assert(q != nil);
	if(p == nil)
		live++;
	return q;
}

/* Account for NULL-safe release of every parser-owned allocation. */
static void
tfree(void *p)
{
	if(p != nil)
		live--;
	free(p);
}

/* Route copied names through the same failure schedule. */
static char*
tdup(char *s)
{
	char *p = talloc(strlen(s)+1);
	if(p != nil)
		strcpy(p, s);
	return p;
}

/* Boundary stubs keep display and native file services outside this fixture. */
void kwerrstr(char *fmt, ...) { USED(fmt); }
void freesubfont(Subfont *s) { assert(s == nil); }
int freeimage(Image *i) { assert(i == nil); return 0; }
static int libopen(char *s, int mode) { USED(s); assert(mode == OREAD); return 7; }
static int libclose(int fd) { assert(fd == 7); closes++; return 0; }
static Dir* libdirfstat(int fd) { Dir *d; assert(fd == 7); d = talloc(sizeof *d); d->length = filelength; return d; }
static int libreadn(int fd, void *p, long n) { USED(p); assert(fd == 7); reads++; return n; }
int is_subfont_ft(char *s) { USED(s); return 0; }

#define malloc talloc
#define realloc trealloc
#define free tfree
#define strdup tdup
IMPLEMENTATION
#undef malloc
#undef realloc
#undef free
#undef strdup

/* Retry every allocation boundary and cover short-field and file-size limits. */
int
main(void)
{
	char text[200], *many, *p;
	Font *f;
	int i, total, count;
	static const char *bad[] = {"32768 1 0 1 a", "65537 1 0 1 a",
		"1 32768 0 1 a", "1 65537 0 1 a", "0 1 0 1 a"};
	static const vlong lengths[] = {-1, 2147483647LL, 2147483648LL,
		4294967296LL, 4294967312LL};

	strcpy(text, "12 9 0 127 one 128 255 two 256 300 three");
	f = buildfont(nil, text, "font");
	assert(f != nil && f->nsub == 3);
	total = calls;
	freefont(f);
	assert(live == 0);
	for(i = 1; i <= total; i++){
		calls = 0; failat = i;
		strcpy(text, "12 9 0 127 one 128 255 two 256 300 three");
		f = buildfont(nil, text, "font");
		assert(f == nil && live == 0);
		checks++;
	}
	failat = 0;
	for(i = 0; i < sizeof bad/sizeof bad[0]; i++){
		strcpy(text, bad[i]);
		f = buildfont(nil, text, "font");
		assert(f == nil && live == 0);
		checks++;
	}
	many = malloc(32768*6+20);
	assert(many != nil);
	for(count = 32767; count <= 32768; count++){
		p = many;
		memcpy(p, "32767 32767 ", 12); p += 12;
		for(i = 0; i < count; i++){
			memcpy(p, "0 1 a ", 6); p += 6;
		}
		*p = 0;
		f = buildfont(nil, many, "font");
		if(count == 32767){
			assert(f != nil && f->nsub == count);
			assert(f->height == 32767 && f->ascent == 32767);
			freefont(f);
		}else
			assert(f == nil);
		assert(live == 0);
		checks++;
	}
	free(many);
	for(i = 0; i < sizeof lengths/sizeof lengths[0]; i++){
		filelength = lengths[i];
		closes = reads = 0;
		assert(openfont(nil, "font") == nil);
		assert(closes == 1 && reads == 0 && live == 0);
		checks++;
	}
	printf("font loading: %d checks passed\n", checks);
	return 0;
}
