#include "lib9.h"
#include "draw.h"
#include <stdio.h>
#include <limits.h>

static int live, calls, failat, checks, closes, reads; // Allocation/I-O counts and active predicates.
static vlong filelength; // Bounded native file length.


/* Identify semantic stages so allocation failures cannot masquerade as omission controls. */
static void
semantic(int condition, char *stage)
{
	if(!condition){
		fprintf(stderr, "Font input semantic mismatch: %s after %d predicates\n", stage, checks);
		exit(10);
	}
	checks++;
}


/* Keep resource checks active under NDEBUG, distinct from the targeted semantic stages. */
static void
require(int condition)
{
	semantic(condition, "resource");
}


/* Count successful allocations and fail exactly one requested allocation. */
static void*
talloc(ulong n)
{
	void *p;
	if(++calls == failat)
		return nil;
	p = malloc(n);
	require(p != nil);
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
	require(q != nil);
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


/* Error formatting is a bounded native diagnostic service in this parser-only fixture. */
void
kwerrstr(char *format, ...)
{
	USED(format);
}


/* Fresh parser objects must not own an opened subfont. */
void
freesubfont(Subfont *subfont)
{
	require(subfont == nil);
}


/* No image allocation or rendering is modeled by the font text loader. */
int
freeimage(Image *image)
{
	require(image == nil);
	return 0;
}


/* Open only the controlled description file. */
static int
libopen(char *name, int mode)
{
	USED(name);
	require(mode == OREAD);
	return 7;
}


/* Every rejected native file length must still close its descriptor. */
static int
libclose(int fd)
{
	require(fd == 7);
	closes++;
	return 0;
}


/* Supply a separately owned directory record with a controlled signed file length. */
static Dir*
libdirfstat(int fd)
{
	Dir *directory;

	require(fd == 7);
	directory = talloc(sizeof(*directory));
	if(directory != nil)
		directory->length = filelength;
	return directory;
}


/* Observe whether the loader refuses an invalid file length before payload I/O. */
static int
libreadn(int fd, void *data, long bytes)
{
	USED(data);
	require(fd == 7);
	reads++;
	return bytes;
}


/* TrueType opening is outside this bitmap-font parser fixture. */
int
is_subfont_ft(char *name)
{
	USED(name);
	return 0;
}


#define malloc talloc // Route parser ownership through failure injection.
#define realloc trealloc // Preserve ownership when growth refuses.
#define free tfree // Count every parser-owned release.
#define strdup tdup // Include copied names in allocation failures.
IMPLEMENTATION
#undef malloc
#undef realloc
#undef free
#undef strdup

FONT_ORACLES


/* Check numeric lexing independently, including refusal without publishing partial values. */
static void
numericcases(void)
{
	unsigned int row;
	char *end;
	int result, status;

	for(row = 0; row < nelem(numbers); row++){
		end = numbers[row].token;
		result = 77;
		status = fontinteger(numbers[row].token, &end, &result);
		semantic(status == numbers[row].status, "number status");
		require(result == (status == 1 ? numbers[row].value : 77));
		require(end == (status == 0 ? numbers[row].token : numbers[row].token+strlen(numbers[row].token)));
	}
}


/* Check complete field boundaries, optional offsets, Unicode endpoints and numeric filenames. */
static void
parsercases(void)
{
	Font *font;
	char buffer[512];
	unsigned int row;
	int admitted;

	for(row = 0; row < nelem(fontcases); row++){
		strcpy(buffer, fontcases[row].text);
		font = buildfont(nil, buffer, "oracle");
		admitted = (font != nil) == fontcases[row].valid;
		if(!admitted){
			freefont(font);
			font = nil;
		}
		semantic(admitted, "font admission");
		if(font != nil){
			require(font->nsub == 1 && font->height == fontcases[row].height && font->ascent == fontcases[row].ascent);
			require(font->sub[0]->min == fontcases[row].low && font->sub[0]->max == fontcases[row].high);
			require(font->sub[0]->offset == fontcases[row].offset && strcmp(font->sub[0]->name, fontcases[row].name) == 0);
			freefont(font);
		}
		require(live == 0);
	}
	require(buildfont(nil, nil, "name") == nil && buildfont(nil, buffer, nil) == nil && live == 0);
}


/* Replay committed descriptions against independent full range/name records, without opening subfont images. */
static void
corpuscases(void)
{
	Font *font;
	FontCorpusCase *entry;
	FontRangeCase *range;
	char *buffer;
	unsigned int row;
	int index, admitted;

	for(row = 0; row < nelem(corpus_fonts); row++){
		entry = corpus_fonts+row;
		buffer = talloc(strlen(entry->text)+1);
		require(buffer != nil);
		strcpy(buffer, entry->text);
		font = buildfont(nil, buffer, entry->path);
		admitted = (font != nil) == entry->valid;
		if(!admitted){
			fprintf(stderr, "font corpus admission: %s\n", entry->path);
			freefont(font);
			font = nil;
		}
		semantic(admitted, "corpus admission");
		if(font != nil){
			require(font->height == entry->height && font->ascent == entry->ascent && font->nsub == entry->count);
			for(index = 0; index < font->nsub; index++){
				range = corpus_ranges+entry->first+index;
				require(font->sub[index]->min == range->low && font->sub[index]->max == range->high);
				require(font->sub[index]->offset == range->offset && strcmp(font->sub[index]->name, range->name) == 0);
			}
			freefont(font);
		}
		tfree(buffer);
		require(live == 0);
	}
}


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
	require(f != nil && f->nsub == 3);
	total = calls;
	freefont(f);
	require(live == 0);
	for(i = 1; i <= total; i++){
		calls = 0; failat = i;
		strcpy(text, "12 9 0 127 one 128 255 two 256 300 three");
		f = buildfont(nil, text, "font");
		require(f == nil && live == 0);
		checks++;
	}
	failat = 0;
	numericcases();
	parsercases();
	corpuscases();
	for(i = 0; i < sizeof bad/sizeof bad[0]; i++){
		strcpy(text, bad[i]);
		f = buildfont(nil, text, "font");
		require(f == nil && live == 0);
		checks++;
	}
	many = malloc(32768*6+20);
	require(many != nil);
	for(count = 32767; count <= 32768; count++){
		p = many;
		memcpy(p, "32767 32767 ", 12); p += 12;
		for(i = 0; i < count; i++){
			memcpy(p, "0 1 a ", 6); p += 6;
		}
		*p = 0;
		f = buildfont(nil, many, "font");
		if(count == 32767){
			require(f != nil && f->nsub == count);
			require(f->height == 32767 && f->ascent == 32767);
			freefont(f);
		}else
			require(f == nil);
		require(live == 0);
		checks++;
	}
	free(many);
	for(i = 0; i < sizeof lengths/sizeof lengths[0]; i++){
		filelength = lengths[i];
		closes = reads = 0;
		require(openfont(nil, "font") == nil);
		require(closes == 1 && reads == 0 && live == 0);
		checks++;
	}
	printf("font loading: %d checks passed\n", checks);
	return 0;
}
