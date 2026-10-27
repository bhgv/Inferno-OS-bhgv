
/* Require the original VM error with release-mode predicates enabled. */
#define ownererror(expr, why) do { \
	raised = nil; \
	if(setjmp(boundary) == 0) { expr; viewcheck(0); } \
	viewcheck(raised == (why)); \
	checks++; \
} while(0) // Explicit error boundary.

static union {
	void *alignment; // Native Heap baseline.
	uchar data[512]; // Owner and payload image.
} ownerimage;
static uchar before[512], cell[32];
static String *candidate, *result, *original;
static WORD indexvalue, endvalue, value;
static int shift, wide, mode, route, slotshift, savedcalls, savedlive;
static int readcases, splitcases, ownercases;


/* Construct metadata using byte copies so malformed owners need no typed access. */
static void
ownerprepare(int offset, int length, int capacity, ulong references)
{
	char *cache;

	memset(ownerimage.data, 0xa5, sizeof(ownerimage.data));
	candidate = (String*)(ownerimage.data+sizeof(Heap)+offset);
	cache = nil;
	memmove((uchar*)candidate+offsetof(String, len), &length, sizeof(length));
	memmove((uchar*)candidate+offsetof(String, max), &capacity, sizeof(capacity));
	memmove((uchar*)candidate+offsetof(String, tmp), &cache, sizeof(cache));
	memmove((uchar*)candidate-sizeof(Heap)+offsetof(Heap, ref), &references, sizeof(references));
}


/* Reject every packed Heap owner and malformed aligned header before mutation. */
static void
checkownererrors(void)
{
	int length, capacity;
	ulong references;
	char *selection;

	selection = getenv("STRING_OWNER_ROUTE");
	for(route = 0; route < 6; route++)
	for(shift = 0; shift < 16; shift++)
	for(wide = 0; wide < 2; wide++)
	for(mode = 0; mode < 4; mode++)
	for(slotshift = 0; slotshift < 8; slotshift++) {
		if(selection && atoi(selection) != route)
			continue;
		length = wide ? -4 : 4;
		capacity = 4;
		references = 1;
		if(mode == 0 && shift%sizeof(void*) == 0)
			capacity = 3;
		if(mode == 1)
			capacity = -1;
		if(mode == 2)
			length = INT_MIN;
		if(mode == 3)
			references = 0;
		ownerprepare(shift, length, capacity, references);
		memmove(before, ownerimage.data, sizeof(before));
		memmove(cell+slotshift, &candidate, sizeof(candidate));
		indexvalue = 0;
		endvalue = 2;
		value = 'a';
		R.s = &value;
		R.m = &indexvalue;
		R.d = cell+slotshift;
		savedcalls = calls;
		savedlive = live;
		failure = route == 5 ? 0 : -1;
		switch(route) {
		case 0:
			ownererror(insc(), exHeap);
			break;
		case 1:
			R.s = &indexvalue;
			R.m = &endvalue;
			ownererror(slicec(), exHeap);
			break;
		case 2:
			ownererror(slicer(0, 2, candidate), exHeap);
			break;
		case 3:
			ownererror(addstring(candidate, H, 1), exHeap);
			break;
		case 4:
			ownererror(splitc((String**)(cell+slotshift), 0), exHeap);
			break;
		case 5:
			ownererror(splitc((String**)(cell+slotshift), 1), exHeap);
			break;
		}
		failure = -1;
		memmove(&result, cell+slotshift, sizeof(result));
		viewcheck(result == candidate && calls == savedcalls && live == savedlive);
		viewcheck(memcmp(before, ownerimage.data, sizeof(before)) == 0);
		ownercases++;
	}
}


/* Character reads borrow packed strings and copy Rune values without typed loads. */
static void
checkpackedreads(void)
{
	Rune rune;
	WORD expected;
	int length, position;

	for(shift = 0; shift < 16; shift++)
	for(wide = 0; wide < 2; wide++)
	for(length = 1; length <= 17; length++) {
		ownerprepare(shift, wide ? -length : length, length, 1);
		for(position = 0; position < length; position++) {
			rune = position%2 ? 0x80000000U+position : 0x20ac+position;
			if(wide)
				memmove((uchar*)candidate+offsetof(String, data)+position*sizeof(Rune), &rune, sizeof(rune));
			else
				*((uchar*)candidate+offsetof(String, data)+position) = (uchar)rune;
		}
		memmove(before, ownerimage.data, sizeof(before));
		for(position = 0; position < length; position++)
		for(slotshift = 0; slotshift < 8; slotshift++) {
			indexvalue = position;
			value = 77;
			memmove(cell+slotshift, &candidate, sizeof(candidate));
			R.s = cell+slotshift;
			R.m = &indexvalue;
			R.d = &value;
			indc();
			rune = position%2 ? 0x80000000U+position : 0x20ac+position;
			expected = wide ? (WORD)(int)rune : (uchar)rune;
			viewcheck(value == expected);
			viewcheck(memcmp(before, ownerimage.data, sizeof(before)) == 0);
			checks++;
			readcases++;
		}
	}
}


/* Successful splits and allocation failures preserve independent shared owners. */
static void
checksplits(void)
{
	int length, expand, shared, fail, i, sourcebytes;
	Rune rune;

	for(slotshift = 0; slotshift < 8; slotshift++)
	for(wide = 0; wide < 2; wide++)
	for(length = 0; length < 17; length++)
	for(expand = 0; expand < 2; expand++)
	for(shared = 0; shared < 2; shared++)
	for(fail = 0; fail < 2; fail++) {
		original = wide ? newrunes(length) : newstring(length);
		for(i = 0; i < length; i++) {
			if(wide)
				original->Srune[i] = 0x20ac+i;
			else
				original->Sascii[i] = 'a'+i;
		}
		if(shared)
			D2H(original)->ref++;
		sourcebytes = sizeof(Heap)+sizeof(String)+length*(wide ? sizeof(Rune) : 1)+32;
		memmove(before, D2H(original), sourcebytes);
		memmove(cell+slotshift, &original, sizeof(original));
		savedlive = live;
		failure = fail ? 0 : -1;
		if(fail) {
			ownererror(splitc((String**)(cell+slotshift), expand), exNomem);
			memmove(&result, cell+slotshift, sizeof(result));
			viewcheck(result == original && live == savedlive);
			viewcheck(D2H(original)->ref == 1+shared);
			viewcheck(memcmp(before, D2H(original), sourcebytes) == 0);
			failure = -1;
			destroy(original);
		} else {
			result = splitc((String**)(cell+slotshift), expand);
			viewcheck(result != original);
			memmove(&candidate, cell+slotshift, sizeof(candidate));
			viewcheck(candidate == result);
			viewcheck(result->len == ((wide || expand) && length ? -length : length));
			for(i = 0; i < length; i++) {
				rune = wide ? 0x20ac+i : 'a'+i;
				viewcheck((result->len < 0 ? result->Srune[i] : (uchar)result->Sascii[i]) == rune);
			}
			destroy(result);
			checks++;
		}
		if(shared) {
			viewcheck(D2H(original)->ref == 1);
			destroy(original);
		}
		viewcheck(live == 0);
		splitcases++;
	}
}


/* Exercise native pointer-cell bounds before attempting a split. */
static void
checksplitcells(void)
{
	uintptr address;
	int expand;

	for(slotshift = 0; slotshift < 8; slotshift++)
	for(expand = 0; expand < 2; expand++) {
		candidate = H;
		memmove(cell+slotshift, &candidate, sizeof(candidate));
		viewcheck(splitc((String**)(cell+slotshift), expand) == H);
		memmove(&result, cell+slotshift, sizeof(result));
		viewcheck(result == H);
		checks++;
		candidate = nil;
		memmove(cell+slotshift, &candidate, sizeof(candidate));
		ownererror(splitc((String**)(cell+slotshift), expand), exNilref);
		memmove(&result, cell+slotshift, sizeof(result));
		viewcheck(result == nil);
	}
	ownererror(splitc(nil, 1), exHeap);
	ownererror(splitc(H, 1), exHeap);
	for(address = (uintptr)-1-sizeof(void*)+1; address != (uintptr)-1; address++)
		ownererror(splitc((String**)address, 1), exHeap);
}


/* Run VM error frames on an ASan-registered stack. */
static void*
ownerworker(void *unused)
{
	(void)unused;
	if(getenv("STRING_OWNER_FAMILY")) {
		if(strcmp(getenv("STRING_OWNER_FAMILY"), "reads") == 0)
			checkpackedreads();
		else
			checksplitcells();
		return nil;
	}
	if(getenv("STRING_OWNER_ROUTE") == nil) {
		checkpackedreads();
		checksplits();
		checksplitcells();
	}
	checkownererrors();
	if(getenv("STRING_OWNER_ROUTE") == nil)
		viewcheck(readcases == 39168 && splitcases == 2176 && ownercases == 6144 && checks == 47529);
	viewcheck(live == 0 && Tstring.ref == 1);
	printf("families reads=%d splits=%d owners=%d\n", readcases, splitcases, ownercases);
	printf("PASS string owner preflight: %d checks, WORD%zu\n", checks, sizeof(WORD)*8);
	return nil;
}


/* Join the sole VM worker and propagate initialization failures. */
int
main(void)
{
	pthread_t worker;

	if(pthread_create(&worker, nil, ownerworker, nil) != 0 || pthread_join(worker, nil) != 0)
		return 1;
	return 0;
}
