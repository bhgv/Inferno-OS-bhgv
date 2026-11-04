#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include <stdio.h>
#include <stdarg.h>

#define Blocks 128 // Maximum simultaneously owned native allocations in this fixture.

struct Block {
	void *pointer; // Actual malloc allocation identity.
	ulong bytes; // Payload size followed by guard bytes.
};

static struct Block blocks[Blocks]; // Native allocation ledger, not fabricated Heap metadata.
static int live, calls, failat, checks; // Owned blocks, fault schedule and active predicates.
static int preflight, packets, failpacket, failflush, pending, loaded; // Native transport boundaries.
static uchar wire[1024]; // One complete controlled image-data packet.
static Display displays[2]; // Independently live native displays.
static char diagnostic[ERRMAX]; // Preserved native error text.
BUILTIN_BYTES
static uchar storage[sizeof(original)+16]; // Every source alignment plus checked surrounding bytes.
static uchar *candidate; // Controlled immutable source view.
static int candidate_length; // Exact exposed source capacity.


/* Only a specific semantic stage satisfies its corresponding omission control. */
static void
check(int condition, char *stage)
{
	int index;

	if(!condition){
		fprintf(stderr, "Default font semantic mismatch: %s after %d checks\n", stage, checks);
		for(index = 0; index < Blocks; index++){
			if(blocks[index].pointer != nil){
				free(blocks[index].pointer);
				blocks[index].pointer = nil;
			}
		}
		exit(10);
	}
	checks++;
}


/* Allocate real native objects while exposing every constructor refusal boundary. */
static void*
talloc(ulong bytes)
{
	uchar *pointer;
	int index;

	calls++;
	if(calls == failat){
		kwerrstr("allocation refused");
		return nil;
	}
	check(bytes <= 1024*1024, "allocation size");
	pointer = malloc(bytes+32);
	check(pointer != nil, "allocation");
	memset(pointer, 0, bytes);
	memset(pointer+bytes, 0xa5, 32);
	for(index = 0; index < Blocks && blocks[index].pointer != nil; index++)
		;
	check(index < Blocks, "ledger capacity");
	blocks[index].pointer = pointer;
	blocks[index].bytes = bytes;
	live++;
	return pointer;
}


/* Check each exact allocation extent before release. */
static void
tfree(void *pointer)
{
	int index, guard;

	if(pointer == nil)
		return;
	for(index = 0; index < Blocks && blocks[index].pointer != pointer; index++)
		;
	check(index < Blocks, "allocation identity");
	for(guard = 0; guard < 32; guard++)
		check(((uchar*)pointer)[blocks[index].bytes+guard] == 0xa5, "allocation guard");
	free(pointer);
	blocks[index].pointer = nil;
	live--;
}


/* Native names participate in the same failure schedule and ownership ledger. */
static char*
tdup(char *text)
{
	char *copy;

	copy = talloc(strlen(text)+1);
	if(copy != nil)
		strcpy(copy, text);
	return copy;
}


/* Preserve real constructor diagnostics across a deliberately destructive image-release service. */
void
kwerrstr(char *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(diagnostic, sizeof(diagnostic), format, arguments);
	va_end(arguments);
}


/* Copy the isolated native error text without allocating a managed String. */
void
kgerrstr(char *text, unsigned int bytes)
{
	if(bytes)
		snprintf(text, bytes, "%s", diagnostic);
}


/* Decode every actual loadimage packet before reusing its bounded transport storage. */
static void
observe(void)
{
	int bytes;

	if(pending == 0)
		return;
	bytes = pending-21;
	check(wire[0] == 'y' && bytes > 0 && bytes <= 864, "pixel packet");
	check(BGLONG(wire+5) == 0 && BGLONG(wire+13) == 2304, "pixel rectangle");
	check(BGLONG(wire+9) == loaded/288 && BGLONG(wire+17) == (loaded+bytes)/288, "pixel rows");
	check(loaded+bytes <= 4320, "pixel extent");
	check(memcmp(wire+21, original+60+loaded, bytes) == 0, "pixel bytes");
	loaded += bytes;
	pending = 0;
}


/* Refuse a selected packet before accepting its payload; prior complete packets stay observed. */
uchar*
bufimage(Display *display, int bytes)
{
	check(display == displays || display == displays+1, "display identity");
	check(bytes > 21 && bytes <= sizeof(wire), "packet capacity");
	observe();
	packets++;
	if(packets == failpacket){
		kwerrstr("packet refused");
		return nil;
	}
	pending = bytes;
	memset(wire, 0xa5, sizeof(wire));
	return wire;
}


/* Run real byte upload code against a controlled final flush refusal. */
int
flushimage(Display *display, int visible)
{
	USED(display);
	USED(visible);
	observe();
	if(failflush){
		kwerrstr("flush refused");
		return -1;
	}
	return 1;
}


/* Supply only native Image allocation; parsing, byte uploads and subfont ownership stay production code. */
Image*
allocimage(Display *display, Rectangle rectangle, ulong channel, int repl, ulong color)
{
	Image *image;

	USED(color);
	check(!preflight, "preflight");
	image = talloc(sizeof(*image));
	if(image != nil){
		check(pending == 0, "load boundary");
		loaded = 0;
		image->display = display;
		image->r = image->clipr = rectangle;
		image->chan = channel;
		image->depth = chantodepth(channel);
		image->repl = repl;
		image->id = ++display->imageid;
	}
	return image;
}


/* Native image retirement consumes its header and can overwrite the diagnostic. */
int
freeimage(Image *image)
{
	tfree(image);
	kwerrstr("image cleanup diagnostic");
	return 0;
}

#define malloc talloc // Observe native constructor allocations.
#define free tfree // Observe actual destructor ownership.
#define strdup tdup // Observe copied native names.
#define defontdata candidate // Exercise arbitrary byte alignment without changing the production blob.
#define sizeofdefont candidate_length // Expose exact source capacity.
IMPLEMENTATION
#undef sizeofdefont
#undef defontdata
#undef strdup
#undef free
#undef malloc


/* Signed bearings must survive both host plain-char models without changing the native layout. */
static void
signedmetrics(void)
{
	uchar packed[18] = {0,0,0,1,128,1, 1,0,0,1,255,1, 2,0,0,1,127,1};
	Fontchar metrics[3];

	_unpackinfo(metrics, packed, 2);
	check(sizeof(Fontchar) == 8, "metric layout");
	check(metrics[0].left == -128 && metrics[1].left == -1 && metrics[2].left == 127, "signed metric");
	check(15+metrics[1].left == 14 && metrics[2].x == 2, "signed metric");
}


/* Restore an identical byte image at every alignment and reset only bounded transport state. */
static void
reset(int offset)
{
	check(live == 0 && lastsubfont == nil && lastname == nil, "owners");
	memset(storage, 0xa5, sizeof(storage));
	candidate = storage+offset;
	memmove(candidate, original, sizeof(original));
	candidate_length = sizeof(original);
	calls = failat = preflight = packets = failpacket = failflush = pending = loaded = 0;
	memset(displays, 0, sizeof(displays));
	displays[0].bufsize = displays[1].bufsize = 1024;
}


/* Write exactly one fixed-width decimal field in the controlled candidate. */
static void
field(int offset, int value)
{
	char text[13];

	snprintf(text, sizeof(text), "%11d ", value);
	memmove(candidate+offset, text, 12);
}


/* Prove immutable bytes, complete uploads and every partial-construction allocation refusal. */
static void
loads(void)
{
	Subfont *font;
	int offset, failure;

	for(offset = 0; offset < 8; offset++){
		for(failure = 0; failure <= 5; failure++){
			reset(offset);
			failat = failure;
			font = getdefont(displays+offset%2);
			failat = 0;
			if(memcmp(candidate, original, sizeof(original)) != 0){
				freesubfont(font);
				font = nil;
				check(0, "blob identity");
			}
			check((font != nil) == (failure == 0 || failure == 5), "load success");
			if(font != nil){
				check(font->n == 256 && font->height == 15 && font->ascent == 13 && font->ref == 1, "subfont fields");
				check(loaded == 4320 && font->bits->display == displays+offset%2, "upload complete");
				freesubfont(font);
			}
			check(live == 0 && lastsubfont == nil && lastname == nil, "owners");
		}
	}
}


/* Reuse identical unaligned bytes across simultaneously live Displays and an existing default alias. */
static void
sharedloads(void)
{
	Subfont *first, *second, *third;

	reset(3);
	first = getdefont(displays);
	second = getdefont(displays+1);
	check(first != nil && second != nil && first != second, "shared displays");
	check(first->bits->display == displays && second->bits->display == displays+1, "shared displays");
	displays[0].defaultsubfont = first;
	third = getdefont(displays);
	check(third != nil && third != first && first->ref == 1, "default alias");
	check(memcmp(candidate, original, sizeof(original)) == 0, "blob identity");
	displays[0].defaultsubfont = nil;
	freesubfont(third);
	freesubfont(first);
	check(second->ref == 1 && second->bits->display == displays+1, "remaining display");
	freesubfont(second);
	check(live == 0 && lastsubfont == nil && lastname == nil, "owners");
}


/* Reject malformed metadata before any allocation or transport reservation. */
static void
badblobs(void)
{
	Subfont *font;
	int mutation;

	for(mutation = 0; mutation < 12; mutation++){
		reset(3);
		switch(mutation){
		case 0: candidate_length = 95; break;
		case 1: candidate_length--; break;
		case 2: candidate_length++; break;
		case 3: field(0, -1); break;
		case 4: field(0, 4); break;
		case 5: field(36, (-2147483647-1)); break;
		case 6: field(4380, 32768); break;
		case 7: field(4392, 256); break;
		case 8: field(4404, 256); break;
		case 9: candidate[0] = 'x'; break;
		case 10: candidate[4416+2] = 20; break;
		case 11: candidate[4416] = 255; candidate[4416+1] = 255; break;
		}
		preflight = 1;
		font = getdefont(displays);
		check(font == nil && calls == 0 && packets == 0 && live == 0, "preflight");
	}
	reset(0);
	check(getdefont(nil) == nil && live == 0, "nil display");
}


/* Refused packet/flush must not publish an image or metrics owner, and must keep the original error. */
static void
transport(void)
{
	Subfont *font;
	int failure;

	for(failure = 1; failure <= 6; failure++){
		reset(1);
		if(failure <= 5)
			failpacket = failure;
		else
			failflush = 1;
		font = getdefont(displays);
		if(font != nil){
			freesubfont(font);
			check(0, "load refusal");
		}
		check(live == 0 && lastsubfont == nil, "owners");
		check(strcmp(diagnostic, failure <= 5 ? "bufimage failed" : "flush refused") == 0, "diagnostic");
	}
}


/* Transfer exactly one Subfont reference into a complete synthetic Font, including every allocation refusal. */
static void
synthetic(void)
{
	Subfont *subfont;
	Font *font;
	int failure, before, callsbefore;
	Rune first;

	for(failure = 0; failure <= 6; failure++){
		reset(0);
		subfont = getdefont(displays);
		check(subfont != nil, "synthetic input");
		before = live;
		callsbefore = calls;
		if(failure)
			failat = calls+failure;
		first = Runemax-subfont->n+1;
		font = mkfont(subfont, first);
		failat = 0;
		if(failure){
			check(font == nil && live == before && subfont->ref == 1, "synthetic refusal");
			freesubfont(subfont);
		}else{
			check(font != nil && calls == callsbefore+6, "synthetic success");
			check(font->sub[0]->min == first && font->sub[0]->max == Runemax, "synthetic range");
			check(font->subf[0].f == subfont && subfont->ref == 1, "transfer");
			freefont(font);
		}
		check(live == 0 && lastsubfont == nil, "owners");
	}
	reset(0);
	subfont = getdefont(displays);
	subfont->ref++;
	font = mkfont(subfont, 0);
	check(font != nil && subfont->ref == 2, "transfer");
	freefont(font);
	check(subfont->ref == 1 && subfont->bits != nil, "shared subfont");
	freesubfont(subfont);
	check(live == 0, "owners");
	reset(0);
	subfont = getdefont(displays);
	for(failure = 0; failure < 2; failure++){
		subfont->ref++;
		font = mkfont(subfont, failure == 0 ? Runemax : (Rune)-1);
		if(font != nil)
			freefont(font);
		check(font == nil, "synthetic admission");
		freesubfont(subfont);
		check(subfont->ref == 1, "synthetic input");
	}
	freesubfont(subfont);
	check(mkfont(nil, 0) == nil && live == 0, "nil subfont");
}


/* Exercise actual byte parsing, upload and owned native graphs without a real draw server or VM collector. */
int
main(void)
{
	signedmetrics();
	loads();
	sharedloads();
	badblobs();
	transport();
	synthetic();
	check(live == 0, "owners");
	printf("PASS default font: %d checks; immutable5958 bytes,8 alignments, allocation/transport refusal, synthetic reference transfer; native device/GC not executed\n", checks);
	return 0;
}
