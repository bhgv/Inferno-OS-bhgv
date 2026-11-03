#define LIMBO_NATIVE64_TABLES // Inspect the exact generated specifications without registering dependencies.
#include "freetype-native64.h"
#include "freetype.h"

extern Type *TMatrix, *TVector, *TFace, *TGlyph; // Creator owners published by the actual initializer.
extern void freetypemodinit(void); // Complete production entry, linked as its own consumer object.
Type Tbyte = {1, 0, 0, 1}; // Static primitive owner retained by byte arrays.
Type Tarray = {1, 0, 0, sizeof(Array)}; // Bounded array header descriptor.
Type Tstring = {1, 0, 0, sizeof(String)}; // ASCII transport descriptor, without moving-GC claims.
char exType[] = "type", exRange[] = "range", exHeap[] = "heap"; // VM diagnostics used by the linked consumer.
static int building, excluded = 1, fontchecks, observed; // Construction/exclusion and active invocation observations.


/* Fail semantically before a bad descriptor can be called or an owner reused. */
static void
fontcheck(int truth)
{
	int i;

	if(!truth){
		fprintf(stderr, "Freetype registration mismatch at %d\n", fontchecks);
		/* Strict controls fail during construction/metadata validation, before FT handles exist. */
		for(i=0; i<nelem(owned); i++)
			if(owned[i] != nil)
				releaseowned(owned[i]);
		exit(10);
	}
	fontchecks++;
}


/* Observe actual constructor allocation without replacing registration or publication. */
static void
fontobserve(void)
{
	if(building)
		fontcheck(TMatrix == nil && TVector == nil && TFace == nil && TGlyph == nil && modules == &baseline);
}


/* Keep the native path copy visible to the constructor fixture's allocation registry. */
char*
fontduplicate(char *text)
{
	return duplicate(text);
}


/* Retire only an owned native path copy from the linked production consumer. */
void
fontfree(void *pointer)
{
	releaseowned(pointer);
}


/* Allocate bounded native payloads; initmem and all descriptor maps are production code. */
static Heap*
fontobject(Type *type, ulong bytes)
{
	Heap *heap;

	fontcheck(excluded && type != nil && type->ref > 0 && bytes >= type->size);
	heap = zeroallocate(sizeof(Heap)+bytes, 1);
	if(heap == nil)
		return H;
	heap->t = type;
	heap->ref = 1;
	type->ref++;
	initmem(type, H2D(void*, heap));
	return heap;
}


/* Use the actual mapped initialization with a bounded allocation transport. */
Heap*
heap(Type *type)
{
	return fontobject(type, type->size);
}


/* Zero allocation and mapped initialization share the same fixture transport. */
Heap*
heapz(Type *type)
{
	return fontobject(type, type->size);
}


/* Release the byte-array element owner; header retirement is handled by destroy. */
static void
fontarrayfree(Heap *heap, int swept)
{
	Array *array = H2D(Array*, heap);

	fontcheck(!swept && array->t == &Tbyte && array->root == H);
	if(array->t->ref <= 1){
		fontcheck(0);
		return;
	}
	freetype(array->t);
}


/* Construct the actual Array shape with separate header and element descriptor owners. */
Heap*
heaparray(Type *type, int length)
{
	Heap *heap;
	Array *array;

	fontcheck(type == &Tbyte && length >= 0 && length < 1048576);
	heap = fontobject(&Tarray, sizeof(Array)+length);
	if(heap == H)
		return H;
	array = H2D(Array*, heap);
	array->len = length;
	array->t = type;
	type->ref++;
	array->root = H;
	array->data = (uchar*)(array+1);
	return heap;
}


/* Retire bounded fixture payloads through their actual published destructor and Type owner. */
void
destroy(void *payload)
{
	Heap *heap;
	Type *type;
	void (*retire)(Heap*, int);

	fontcheck(excluded);
	if(payload == H)
		return;
	fontcheck(payload != nil);
	heap = D2H(payload);
	fontcheck(heap->ref > 0);
	if(--heap->ref != 0)
		return;
	type = heap->t;
	retire = type->free;
	if(retire != nil)
		retire(heap, 0);
	freetype(type);
	releaseowned(heap);
}


/* Limit this process-string boundary to the actual ASCII font paths and names used here. */
String*
c2string(char *text, int length)
{
	Heap *heap;
	String *string;
	int i;

	fontcheck(text != nil && length >= 0 && length < 4096);
	for(i=0; i<length; i++)
		fontcheck((uchar)text[i] < 128);
	heap = fontobject(&Tstring, sizeof(String)+length+1);
	if(heap == H)
		error(exNomem);
	string = H2D(String*, heap);
	string->len = length;
	string->max = length+1;
	memmove(string->Sascii, text, length);
	string->Sascii[length] = 0;
	return string;
}


/* Borrow the ASCII transport string while the caller retains its ordinary owner. */
char*
string2c(String *string)
{
	fontcheck(string != nil && string != H && string->len >= 0);
	return string->Sascii;
}


/* Implement only the bounded managed diagnostic-result transport used by setcharsize. */
void
retstr(char *message, void *result)
{
	String **slot = result;

	destroy(*slot);
	*slot = message == nil ? H : c2string(message, strlen(message));
}


/* Observe the production consumer's native exclusion transitions. */
void
release(void)
{
	fontcheck(excluded);
	excluded = 0;
}


/* Reacquire the VM before managed stores or native-handle retirement completes. */
void
acquire(void)
{
	fontcheck(!excluded);
	excluded = 1;
}


/* Exercise the real protected initializer with the constructor's error-frame transport. */
static int
fontinitialize(void *unused)
{
	USED(unused);
	freetypemodinit();
	return 1;
}


/* Retire the four creator owners only after every diagnostic object has gone. */
static void
fonttypesclear(void)
{
	fontcheck(TMatrix->ref == 1 && TVector->ref == 1 && TFace->ref == 1 && TGlyph->ref == 1);
	freetype(TMatrix); freetype(TVector); freetype(TFace); freetype(TGlyph);
	TMatrix = TVector = TFace = TGlyph = nil;
}


/* Validate independent names/hashes/Frame64 sizes/root maps before any callable is used. */
static Link*
fontentry(Module *module, int index)
{
	static char *names[] = {
		"Face.haschar", "Face.loadglyph", "newface", "newmemface", "Face.setcharsize", "Face.settransform"
	}; // Existing Limbo module-relative export names.
	static uint signatures[] = {
		0x6a7da190U, 0xdd275b67U, 0x018a90beU, 0xc56f82ddU, 0xb282ce87U, 0xcf26b85eU
	}; // Existing legacy hashes, not new shared structural signatures.
	Link *link;
	int i;

	fontcheck(module != nil && module->ntype == 6);
	for(i=0; i<6; i++){
		link = &module->ext[i];
		if(strcmp(link->name, names[index]) != 0)
			continue;
		fontcheck((uint)link->sig == signatures[index] && link->u.runt != nil);
		fontcheck(link->frame->size == (index < 4 ? 80 : 88));
		fontcheck(link->frame->np == 2 && link->frame->map[0] == 0);
		fontcheck(link->frame->map[1] == (index == 5 ? 0xe0 : 0x80));
		observed |= 1<<index;
		return link;
	}
	fontcheck(0);
	return nil;
}


/* Invoke all six published exports with freshly generated frames and real native font work. */
static void
fontcalls(Module *module, char *path)
{
	F_Freetype_newface create = {0};
	F_Freetype_newmemface memory = {0};
	F_Freetype_Face_haschar character = {0};
	F_Freetype_Face_setcharsize size = {0};
	F_Freetype_Face_settransform transform = {0};
	F_Freetype_Face_loadglyph load = {0};
	Freetype_Face *face = H, *unsupported = H;
	Freetype_Glyph *glyph = H;
	Freetype_Matrix *matrix;
	Freetype_Vector *vector;
	String *name, *status = H;
	WORD present = -1;
	int before = live;

	name = c2string(path, strlen(path));
	create.path = name; create.ret = &face;
	fontentry(module, 2)->u.runt(&create);
	fontcheck(face != H && face->familyname != H && face->stylename != H);
	character.face = face; character.c = 'A'; character.ret = &present;
	fontentry(module, 0)->u.runt(&character);
	fontcheck(present == 1);
	size.face = face; size.pts = 16*64; size.hdpi = size.vdpi = 72; size.ret = &status;
	fontentry(module, 4)->u.runt(&size);
	fontcheck(status == H && face->height > 0);
	matrix = H2D(Freetype_Matrix*, heapz(TMatrix));
	vector = H2D(Freetype_Vector*, heapz(TVector));
	matrix->a = matrix->d = 65536; vector->dx = 128; vector->dy = 192;
	transform.face = face; transform.m = matrix; transform.v = vector; transform.ret = &status;
	fontentry(module, 5)->u.runt(&transform);
	fontcheck(status == H);
	load.face = face; load.c = 'A'; load.ret = &glyph;
	fontentry(module, 1)->u.runt(&load);
	fontcheck(glyph != H && glyph->width > 0 && glyph->height > 0);
	fontcheck(glyph->bitmap->len == glyph->width*glyph->height && glyph->advance.x > 0);
	memory.data = H; memory.ret = &unsupported;
	fontentry(module, 3)->u.runt(&memory);
	fontcheck(unsupported == H && !strcmp(diagnostic, "not implemented"));
	destroy(glyph); destroy(matrix); destroy(vector); destroy(face); destroy(name);
	fontcheck(live == before && Tbyte.ref == 1 && Tarray.ref == 1 && Tstring.ref == 1 && excluded);
}


/* Cover every constructor allocation refusal, complete publication, idempotence and retirement. */
int
main(int argc, char **argv)
{
	Module *module;
	int limit, total, i;

	fontcheck(argc == 3);
	Tarray.free = fontarrayfree;
	Tstring.free = freeheap;
	building = 1;
#if defined(FONT_HEADER_ONLY) || defined(FONT_PACKED)
	fontcheck(kprotect(fontinitialize, nil) == 0);
	fontcheck(attempts == 0 && live == 0 && modules == &baseline && TFace == nil);
	printf("PASS native64 Freetype early registration refusal\n");
	return 0;
#endif
	fontcheck(kprotect(fontinitialize, nil) == 1);
	building = 0;
	total = attempts;
	module = modules;
	fontcheck(module != &baseline && !strcmp(module->name, "$Freetype"));
	for(i=0; i<6; i++)
		fontentry(module, i);
	unload(module);
	fonttypesclear();
	fontcheck(live == 0 && modules == &baseline);
	for(limit=0; limit<total; limit++){
		fail_after = limit; attempts = 0; building = 1; diagnostic[0] = 0;
		fontcheck(kprotect(fontinitialize, nil) == 0);
		building = 0;
		fontcheck(modules == &baseline && live == 0 && worker.nerr == 0 && diagnostic[0] != 0);
		fontcheck(TMatrix == nil && TVector == nil && TFace == nil && TGlyph == nil);
	}
	fail_after = -1; building = 1; attempts = 0;
	fontcheck(kprotect(fontinitialize, nil) == 1);
	building = 0;
	module = modules; i = attempts;
	fontcheck(kprotect(fontinitialize, nil) == 1 && modules == module && attempts == i);
	fontcalls(module, argv[1]);
	fontcalls(module, argv[2]);
	fontcheck(observed == 63 && worker.nerr == 0);
	unload(module);
	fonttypesclear();
	fontcheck(live == 0 && modules == &baseline);
	printf("PASS native64 Freetype registration: 6 published exports, %d allocation refusals, 2 real fonts; %d predicates\n", total, fontchecks);
	return 0;
}
