
/* Require VM errors while keeping rejection predicates active in release builds. */
#define codecerror(expr, why) do { \
	raised = nil; \
	if(setjmp(boundary) == 0) { expr; viewcheck(0); } \
	viewcheck(raised == (why)); \
	checks++; \
} while(0) // Explicit error boundary.

static union {
	void *alignment; // Heap baseline alignment.
	uchar data[1024]; // Borrowed source owner and inline data.
} sourceimage, targetimage, rootimage, typeimage;
static uchar sourcebefore[1024], targetbefore[1024], slot[32];
static uchar typebefore[1024], rootbefore[1024];
static uchar expectedbytes[256];
static Rune expectedrunes[32];
static void *input, *old, *output;
static int direction, shift, wide, count, offset, profile, fail;
static int bytecount, savedcalls, savedlive, reused, cases;


/* Report the active conversion geometry before aborting a failed predicate. */
static void
codecdiag(int line)
{
	fprintf(stderr, "codec line=%d dir=%d shift=%d wide=%d count=%d slot=%d profile=%d fail=%d reused=%d\n",
		line, direction, shift, wide, count, offset, profile, fail, reused);
}


/* Encode an independent UTF oracle, including replacement of invalid Rune keys. */
static int
encodekey(Rune rune, uchar *bytes)
{
	if(rune > 0x10ffff || (rune >= 0xd800 && rune <= 0xdfff))
		rune = 0xfffd;
	if(rune < 0x80) {
		bytes[0] = rune;
		return 1;
	}
	if(rune < 0x800) {
		bytes[0] = 0xc0 | (rune>>6);
		bytes[1] = 0x80 | (rune&63);
		return 2;
	}
	if(rune < 0x10000) {
		bytes[0] = 0xe0 | (rune>>12);
		bytes[1] = 0x80 | ((rune>>6)&63);
		bytes[2] = 0x80 | (rune&63);
		return 3;
	}
	bytes[0] = 0xf0 | (rune>>18);
	bytes[1] = 0x80 | ((rune>>12)&63);
	bytes[2] = 0x80 | ((rune>>6)&63);
	bytes[3] = 0x80 | (rune&63);
	return 4;
}


/* Build packed borrowed sources without dereferencing their native structures. */
static void
preparesource(void)
{
	Heap heap;
	Array array;
	String *string;
	Rune keys[] = {0, 'A', 0x7f, 0x80, 0x7ff, 0x800, 0xd800, 0x20ac, 0x10000, 0x10ffff, 0x110000, 0xffffffff};
	char *cache;
	int i, length, capacity;

	memset(sourceimage.data, 0xa5, sizeof(sourceimage.data));
	memset(&heap, 0, sizeof(heap));
	heap.ref = 1;
	heap.t = direction ? &Tarray : &Tstring;
	input = sourceimage.data+sizeof(Heap)+shift;
	memmove((uchar*)input-sizeof(Heap), &heap, sizeof(heap));
	bytecount = 0;
	for(i = 0; i < count; i++) {
		expectedrunes[i] = wide ? keys[i%nelem(keys)] : 'a'+i;
		bytecount += encodekey(expectedrunes[i], expectedbytes+bytecount);
	}
	if(direction) {
		memset(&array, 0, sizeof(array));
		array.t = (Type*)(typeimage.data+shift);
		memmove(array.t, &Tbyte, sizeof(Tbyte));
		array.root = H;
		array.len = bytecount;
		array.data = sourceimage.data+512;
		memmove(input, &array, sizeof(array));
		memmove(array.data, expectedbytes, bytecount);
	} else {
		string = input;
		length = wide ? -count : count;
		capacity = count;
		cache = nil;
		memmove((uchar*)string+offsetof(String, len), &length, sizeof(length));
		memmove((uchar*)string+offsetof(String, max), &capacity, sizeof(capacity));
		memmove((uchar*)string+offsetof(String, tmp), &cache, sizeof(cache));
		if(wide)
			memmove((uchar*)string+offsetof(String, data), expectedrunes, count*sizeof(Rune));
		else
			memmove((uchar*)string+offsetof(String, data), expectedbytes, count);
	}
	memmove(sourcebefore, sourceimage.data, sizeof(sourcebefore));
}


/* Borrow packed sources while reusing, replacing or retaining independent old owners. */
static void
checkconversions(void)
{
	String *text;
	Array *array;
	Rune key;
	int i;

	for(direction = 0; direction < 2; direction++)
	for(shift = 0; shift < 16; shift++)
	for(wide = 0; wide < 2; wide++)
	for(count = 0; count < 18; count++)
	for(offset = 0; offset < 8; offset++)
	for(profile = 0; profile < 3; profile++)
	for(fail = 0; fail < 2; fail++) {
		if(getenv("CODEC_OWNER_DIRECTION") && direction != atoi(getenv("CODEC_OWNER_DIRECTION")))
			continue;
		preparesource();
		memmove(typebefore, typeimage.data, sizeof(typebefore));
		old = H;
		if(profile) {
			old = direction ? (void*)newstring(128) : (void*)mem2array(nil, 128);
			if(profile == 2)
				D2H(old)->ref++;
		}
		memmove(slot+offset, &old, sizeof(old));
		R.s = &input;
		R.d = slot+offset;
		savedcalls = calls;
		savedlive = live;
		reused = profile == 1 && (!wide || count == 0 || (direction && count <= 3));
		failure = fail ? 0 : -1;
		if(fail && !reused) {
			if(direction) {
				codecerror(cvtac(), exNomem);
			} else {
				codecerror(cvtca(), exNomem);
			}
			memmove(&output, slot+offset, sizeof(output));
			viewcheck(output == old && calls == savedcalls && live == savedlive);
			if(old != H)
				viewcheck(D2H(old)->ref == (profile == 2 ? 2 : 1));
			failure = -1;
			destroy(old);
		} else {
			if(direction)
				cvtac();
			else
				cvtca();
			memmove(&output, slot+offset, sizeof(output));
			viewcheck((output == old) == reused);
			viewcheck(calls-savedcalls == (reused ? 0 : 1));
			if(direction) {
				text = output;
				viewcheck(text->len == ((wide && count > 3) ? -count : count));
				for(i = 0; i < count; i++) {
					key = expectedrunes[i];
					if(key > 0x10ffff || (key >= 0xd800 && key <= 0xdfff))
						key = 0xfffd;
					viewcheck((text->len < 0 ? text->Srune[i] : (uchar)text->Sascii[i]) == key);
				}
			} else {
				array = output;
				viewcheck(array->len == bytecount && memcmp(array->data, expectedbytes, bytecount) == 0);
			}
			failure = -1;
			destroy(output);
			checks++;
		}
		if(profile == 2) {
			viewcheck(D2H(old)->ref == 1);
			destroy(old);
		}
		viewcheck(memcmp(sourcebefore, sourceimage.data, sizeof(sourcebefore)) == 0 && live == 0);
		viewcheck(memcmp(typebefore, typeimage.data, sizeof(typebefore)) == 0);
		cases++;
	}
}


/* Reject packed result owners, backing owners and releasable array descriptors. */
static void
checktargets(void)
{
	Heap heap;
	Array array;
	Type type;
	int route, length, capacity;
	char *cache, *why;

	for(route = 0; route < 5; route++)
	for(shift = 1; shift < 16; shift++)
	for(offset = 0; offset < 8; offset++) {
		if(getenv("CODEC_OWNER_TARGET_ROUTE") && route != atoi(getenv("CODEC_OWNER_TARGET_ROUTE")))
			continue;
		if(shift%sizeof(void*) == 0)
			continue;
		direction = route == 1 || route == 2;
		wide = 0;
		count = 4;
		preparesource();
		memset(targetimage.data, 0xa5, sizeof(targetimage.data));
		memset(&heap, 0, sizeof(heap));
		heap.ref = 1;
		heap.t = direction ? &Tstring : &Tarray;
		old = targetimage.data+sizeof(Heap)+(route < 3 ? shift : 0);
		memmove((uchar*)old-sizeof(Heap), &heap, sizeof(heap));
		why = exHeap;
		if(direction) {
			length = 0;
			capacity = 128;
			cache = nil;
			memmove((uchar*)old+offsetof(String, len), &length, sizeof(length));
			memmove((uchar*)old+offsetof(String, max), &capacity, sizeof(capacity));
			memmove((uchar*)old+offsetof(String, tmp), &cache, sizeof(cache));
		} else {
			memset(&array, 0, sizeof(array));
			array.len = 128;
			array.data = targetimage.data+512;
			array.root = H;
			array.t = &Tbyte;
			if(route == 3) {
				array.root = (Array*)(rootimage.data+sizeof(Heap)+shift);
				memmove((uchar*)array.root-sizeof(Heap), &heap, sizeof(heap));
			}
			if(route == 4) {
				type = Tbyte;
				array.t = (Type*)(typeimage.data+shift);
				memmove(array.t, &type, sizeof(type));
				why = exType;
			}
			memmove(old, &array, sizeof(array));
		}
		if(route == 2)
			input = H;
		memmove(targetbefore, targetimage.data, sizeof(targetbefore));
		memmove(rootbefore, rootimage.data, sizeof(rootbefore));
		memmove(typebefore, typeimage.data, sizeof(typebefore));
		memmove(slot+offset, &old, sizeof(old));
		R.s = &input;
		R.d = slot+offset;
		savedcalls = calls;
		failure = 0;
		if(direction) {
			codecerror(cvtac(), why);
		} else {
			codecerror(cvtca(), why);
		}
		failure = -1;
		memmove(&output, slot+offset, sizeof(output));
		viewcheck(output == old && calls == savedcalls && live == 0);
		viewcheck(memcmp(targetbefore, targetimage.data, sizeof(targetbefore)) == 0);
		viewcheck(memcmp(rootbefore, rootimage.data, sizeof(rootbefore)) == 0);
		viewcheck(memcmp(typebefore, typeimage.data, sizeof(typebefore)) == 0);
	}
}


/* Run the VM fixture on a registered stack and check all allocated references. */
static void*
codecworker(void *unused)
{
	(void)unused;
	Tbyte.size = 1;
	if(getenv("CODEC_OWNER_TARGET_ROUTE") == nil)
		checkconversions();
	if(getenv("CODEC_OWNER_DIRECTION") == nil)
		checktargets();
	if(getenv("CODEC_OWNER_DIRECTION") == nil && getenv("CODEC_OWNER_TARGET_ROUTE") == nil)
		viewcheck(cases == 55296 && checks == 55856);
	viewcheck(live == 0 && Tarray.ref == 1 && Tbyte.ref == 1 && Tstring.ref == 1);
	printf("PASS codec owners: %d conversions, %d total checks WORD%zu\n", cases, checks, sizeof(WORD)*8);
	return nil;
}


/* Join the single worker before returning its fixture status. */
int
main(void)
{
	pthread_t worker;

	if(pthread_create(&worker, nil, codecworker, nil) != 0 || pthread_join(worker, nil) != 0)
		return 1;
	return 0;
}
