#include "lib9.h"
#include "interp.h"
#include "isa.h"
#include "runt.h"
#include "freetype.h"
#include <stdio.h>
#include <setjmp.h>

FONT_WRAPPER

static Type face_type, glyph_type, string_type; // Only descriptor identities cross the fixture boundary.
Type *TFace = &face_type, *TGlyph = &glyph_type; // Identities used by the actual entry bodies.
Type Tbyte; // Primitive array identity observed by heaparray.
char exRange[] = "range", exType[] = "type", exNomem[] = "memory"; // VM error identities.
static jmp_buf recovery; // Only expected scalar refusal uses the VM-error boundary.
static int expecting, released, calls, checks, allocations, failalloc; // Active observation state.
static char diagnostic[128]; // Process-error transport observed after ordinary refusal.
static FTglyph supplied; // Borrowed glyph data supplied by the native renderer boundary.
static FTmatrix observed_matrix; // Exact fixed-int transform received by the native bridge.
static FTvector observed_vector; // Exact fixed-int displacement received by the native bridge.
static int saw_matrix, saw_vector; // Null-transform semantics are observed independently.
static Heap *baseface; // Native face wrapper freed even on a strict semantic-control exit.
static Heap *owners[8]; // Small fixture allocation registry, distinct from runtime heap semantics.
static int failpath, failface, nonames, nativefaces, converted; // Face construction failure and cleanup observations.
static Freetype_Face **constructing; // Published managed owner observed during fallible string work.
static WORD *character_result; // Return slot must remain unchanged until VM exclusion is reacquired.


/* Fail on a semantic predicate; signals and sanitizer failures are never controls. */
static void
check(int truth)
{
	if(!truth){
		fprintf(stderr, "Freetype bridge mismatch at check %d\n", checks);
		{
			int i;
			for(i=0; i<8; i++)
				free(owners[i]);
			free(baseface);
		}
		exit(10);
	}
	checks++;
}


/* Catch only explicitly expected checked-scalar refusals. */
void
error(char *message)
{
	check(expecting && message == exRange);
	longjmp(recovery, 1);
}


/* Observe managed diagnostics without replacing the glyph copying implementation. */
void
kwerrstr(char *message, ...)
{
	snprintf(diagnostic, sizeof(diagnostic), "%s", message);
}


/* Record native-call exclusion and prevent nesting in this serialized fixture. */
void
release(void)
{
	check(!released);
	released = 1;
}


/* Return to the VM before any managed result publication. */
void
acquire(void)
{
	check(released);
	if(character_result != nil){
		check(*character_result == 0);
		character_result = nil;
	}
	released = 0;
}


/* Allocate exact native payloads with a descriptor header and explicit ownership. */
static Heap*
allocate(int bytes, Type *type)
{
	Heap *h;
	int i;

	check(!released && bytes >= 0 && bytes < 65536);
	allocations++;
	if(failalloc == allocations)
		return H;
	h = calloc(1, sizeof(Heap)+bytes);
	check(h != nil);
	h->t = type;
	for(i=0; i<8 && owners[i] != nil; i++);
	check(i < 8);
	owners[i] = h;
	return h;
}


/* Model only the heap boundary; the actual entry owns allocation and result order. */
Heap*
heap(Type *type)
{
	Heap *h;

	check(type == TGlyph);
	h = allocate(sizeof(Freetype_Glyph), type);
	if(h != H)
		H2D(Freetype_Glyph*, h)->bitmap = H;
	return h;
}


/* Return a tightly sized byte array so sanitizer bounds expose bad row copies. */
Heap*
heaparray(Type *type, int length)
{
	Heap *h;
	Array *a;

	check(type == &Tbyte);
	h = allocate(sizeof(Array)+length, type);
	if(h != H){
		a = H2D(Array*, h);
		a->len = length;
		a->data = (uchar*)(a+1);
	}
	return h;
}


/* Initialize managed names as nil and leave native handles unpublished. */
Heap*
heapz(Type *type)
{
	Heap *h;
	Face *face;

	check(type == TFace);
	h = allocate(sizeof(Face), type);
	if(h != H){
		face = H2D(Face*, h);
		face->freetypeface.familyname = H;
		face->freetypeface.stylename = H;
	}
	return h;
}


/* Observe that the face is rooted before the actual entry converts its path. */
char*
string2c(String *string)
{
	(void)string;
	check(constructing != nil && *constructing != H);
	return "font";
}


/* Inject only native path duplication failure, leaving the production cleanup intact. */
static char*
fixturestrdup(char *string)
{
	char *copy;

	if(failpath)
		return nil;
	copy = malloc(strlen(string)+1);
	check(copy != nil);
	strcpy(copy, string);
	return copy;
}


/* Check both nullable font-name conversions and create distinct managed string owners. */
String*
c2string(char *text, int length)
{
	Heap *h;

	check(text != nil && length == strlen(text) && *constructing != H);
	check(nonames ? length == 0 : length > 0);
	converted++;
	h = allocate(sizeof(String), &string_type);
	check(h != H);
	return H2D(String*, h);
}


/* Model only native handle creation and nullable names, without loading a font. */
char*
ftnewface(char *path, int index, FTface *face, FTfaceinfo *info)
{
	check(released && path != nil && index == 0);
	calls++;
	if(failface)
		return "face failure";
	memset(info, 0, sizeof(*info));
	info->familyname = nonames ? nil : "family";
	info->stylename = nonames ? nil : "style";
	face->ft_face = &face_type;
	nativefaces++;
	return nil;
}


/* Retire only owners created by this fixture, recursively releasing glyph bytes. */
void
destroy(void *data)
{
	Heap *h;
	int i;

	check(!released);
	if(data == H)
		return;
	h = D2H(data);
	for(i=0; i<8 && owners[i] != h; i++);
	check(i < 8);
	if(h->t == TGlyph)
		destroy(((Freetype_Glyph*)data)->bitmap);
	if(h->t == TFace){
		destroy(((Face*)data)->freetypeface.familyname);
		destroy(((Face*)data)->freetypeface.stylename);
		if(((Face*)data)->ftface.ft_face != nil)
			nativefaces--;
	}
	owners[i] = nil;
	free(h);
}


/* Supply renderer bytes without executing a second implementation of row copying. */
char*
ftloadglyph(FTface face, int c, FTglyph *glyph)
{
	(void)face; USED(c);
	check(released);
	calls++;
	*glyph = supplied;
	return nil;
}


/* Observe the character-query call while its managed return slot is protected. */
int
fthaschar(FTface face, int c)
{
	(void)face;
	check(released && c == 65 && character_result != nil && *character_result == 0);
	calls++;
	return 1;
}


/* Observe both optional fixed-int records while the managed entry is excluded. */
void
ftsettransform(FTface face, FTmatrix *matrix, FTvector *vector)
{
	(void)face;
	check(released);
	calls++;
	saw_matrix = matrix != nil;
	saw_vector = vector != nil;
	if(matrix != nil)
		observed_matrix = *matrix;
	if(vector != nil)
		observed_vector = *vector;
}

#define strdup fixturestrdup // Bound only path duplication in the isolated VM fixture.
FONT_ENTRIES
#undef strdup


/* Exercise padding, signed pitch, empty rasters, malformed extents and both allocation failures. */
int
main(void)
{
	Heap *h;
	Face *face;
	Freetype_Glyph *result;
	Freetype_Matrix matrix;
	Freetype_Vector vector;
	String *status = H;
	F_Face_loadglyph load;
	F_Face_settransform transform;
	F_Freetype_newface create;
	Freetype_Face *created;
	F_Face_haschar character;
	WORD present;
	uchar bytes[256];
	int w, rows, pad, negative, i, j, before, cases = 0;

	h = calloc(1, sizeof(Heap)+sizeof(Face));
	baseface = h;
	check(h != nil);
	h->t = TFace;
	face = H2D(Face*, h);
	memset(&load, 0, sizeof(load));
	load.face = &face->freetypeface;
	load.ret = &result;
	memset(&character, 0, sizeof(character));
	character.face = &face->freetypeface;
	character.c = 65;
	character.ret = &present;
	character_result = &present;
	Face_haschar(&character);
	check(present == 1 && character_result == nil && !released);
	cases++;
	for(w=0; w<=8; w++)
	for(rows=0; rows<=7; rows++)
	for(pad=0; pad<=2; pad++)
	for(negative=0; negative<=1; negative++){
		for(i=0; i<256; i++)
			bytes[i] = (uchar)(i*37+11);
		memset(&supplied, 0, sizeof(supplied));
		supplied.width = w;
		supplied.height = rows;
		supplied.bpr = negative ? -w-pad : w+pad;
		supplied.bitmap = bytes+(negative && rows ? (rows-1)*(w+pad) : 0);
		result = H;
		allocations = 0;
		Face_loadglyph(&load);
		check(result != H && result->bitmap->len == w*rows);
		for(i=0; i<rows; i++)
		for(j=0; j<w; j++)
			check(result->bitmap->data[i*w+j] == supplied.bitmap[i*supplied.bpr+j]);
		destroy(result);
		cases++;
	}
	for(i=0; i<8; i++){
		memset(&supplied, 0, sizeof(supplied));
		supplied.width = 2; supplied.height = 2; supplied.bpr = 2; supplied.bitmap = bytes;
		switch(i){
		case 0: supplied.width = -1; break;
		case 1: supplied.height = -1; break;
		case 2: supplied.width = 0x7fffffff; break;
		case 3: supplied.bpr = 1; break;
		case 4: supplied.bpr = -1; break;
		case 5: supplied.bpr = -0x7fffffff-1; break;
		case 6: supplied.bpr = 0x7fffffff; break;
		case 7: supplied.bitmap = nil; break;
		}
		allocations = 0; result = H; diagnostic[0] = 0;
		Face_loadglyph(&load);
		check(result == H && allocations == 0 && diagnostic[0] != 0);
		cases++;
	}
	memset(&supplied, 0, sizeof(supplied));
	supplied.width = 2; supplied.height = 2; supplied.bpr = 2; supplied.bitmap = bytes;
	for(i=1; i<=2; i++){
		allocations = 0; failalloc = i; result = H;
		Face_loadglyph(&load);
		check(result == H && !strcmp(diagnostic, exNomem));
		for(j=0; j<8; j++)
			check(owners[j] == nil);
		cases++;
	}
	failalloc = 0;
	memset(&create, 0, sizeof(create));
	create.ret = &created;
	constructing = &created;
	for(i=0; i<5; i++){
		allocations = 0; converted = 0; created = H;
		failalloc = i == 0 ? 1 : 0;
		failpath = i == 1;
		failface = i == 2;
		nonames = i == 3;
		Freetype_newface(&create);
		if(i < 3)
			check(created == H && nativefaces == 0 && converted == 0);
		else{
			check(created != H && nativefaces == 1 && converted == 2);
			destroy(created);
			check(nativefaces == 0);
		}
		for(j=0; j<8; j++)
			check(owners[j] == nil);
		cases++;
	}
	failalloc = failpath = failface = nonames = 0;
	constructing = nil;
	matrix.a = 65536; matrix.b = -32768; matrix.c = 12345; matrix.d = -65536;
	vector.dx = -129; vector.dy = 321;
	memset(&transform, 0, sizeof(transform));
	transform.face = &face->freetypeface;
	transform.ret = &status;
	for(i=0; i<4; i++){
		transform.m = i&1 ? &matrix : H;
		transform.v = i&2 ? &vector : H;
		Face_settransform(&transform);
		check(saw_matrix == (i&1) && saw_vector == ((i>>1)&1));
		if(i&1)
			check(observed_matrix.a == matrix.a && observed_matrix.b == matrix.b && observed_matrix.c == matrix.c && observed_matrix.d == matrix.d);
		if(i&2)
			check(observed_vector.dx == vector.dx && observed_vector.dy == vector.dy);
		cases++;
	}
	if(sizeof(WORD) > 4){
		before = calls;
		matrix.a = (WORD)((uvlong)1<<32);
		transform.m = &matrix;
		expecting = 1;
		if(setjmp(recovery) == 0){
			Face_settransform(&transform);
			check(0);
		}
		expecting = 0;
		check(calls == before && !released);
		cases++;
	}
	free(h);
	baseface = nil;
	printf("PASS Freetype VM bridge WORD%u: %d raster/transform/refusal cases\n", (unsigned)(8*sizeof(WORD)), cases);
	return 0;
}
