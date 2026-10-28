
static union {
	void *alignment; // Native Heap baseline.
	uchar bytes[512]; // Borrowed Array header and Heap prefix.
} arrayimage;
static uchar typeimage[sizeof(Type)+16], frameimage[512], resultimage[64], dataimage[64];
static uchar expectedresult[64], expecteddata[64], arraybefore[512];
static union {
	void *alignment; // Valid generic Heap origin.
	uchar bytes[sizeof(Heap)+1]; // A wrong-type object has no complete Array payload.
} genericimage;
static uchar utf[8];
static Array *input;
static void *resultslot;
static WORD argument, runevalue;
static int op, fs, as, ts, rs, sample, length;
static unsigned long scenarios;


/* Keep byte-image and error predicates active under NDEBUG. */
static void
utfcheck(int test)
{
	if(!test) {
		fprintf(stderr, "Sys UTF span op=%d frame=%d array=%d type=%d result=%d sample=%d\n", op, fs, as, ts, rs, sample);
		abort();
	}
}


/* Invoke each real public entry using a packed generated builtin frame. */
static void
invoke(void)
{
	F_Sys_utfbytes u = {0};
	F_Sys_byte2char b = {0};
	F_Sys_char2byte c = {0};

	if(op == 0) {
		u.ret = resultslot;
		u.buf = input;
		u.n = argument;
		memmove(frameimage+fs, &u, sizeof(u));
		Sys_utfbytes(frameimage+fs);
	} else if(op == 1) {
		b.ret = resultslot;
		b.buf = input;
		b.n = argument;
		memmove(frameimage+fs, &b, sizeof(b));
		Sys_byte2char(frameimage+fs);
	} else {
		c.ret = resultslot;
		c.buf = input;
		c.n = argument;
		c.c = runevalue;
		memmove(frameimage+fs, &c, sizeof(c));
		Sys_char2byte(frameimage+fs);
	}
}


/* Build metadata through byte copies so the fixture itself supports every shift. */
static void
prepare(void)
{
	Heap heap = {0};
	Array array = {0};
	Type type = {0};

	memset(arrayimage.bytes, 0xa5, sizeof(arrayimage.bytes));
	memset(dataimage, 0xa5, sizeof(dataimage));
	memset(resultimage, 0xa5, sizeof(resultimage));
	heap.t = &Tarray;
	heap.ref = 1;
	type.size = 1;
	type.ref = 1;
	memmove(typeimage+ts, &type, sizeof(type));
	input = (Array*)(arrayimage.bytes+sizeof(Heap)+as);
	array.t = (Type*)(typeimage+ts);
	array.root = H;
	array.data = dataimage+8;
	array.len = op == 2 ? 8 : length;
	memmove((uchar*)input-sizeof(Heap), &heap, sizeof(heap));
	memmove(input, &array, sizeof(array));
	memmove(dataimage+8, utf, length);
	resultslot = rs < 8 ? (void*)(resultimage+rs) : (void*)(dataimage+rs);
	memmove(expectedresult, resultimage, sizeof(resultimage));
	memmove(expecteddata, dataimage, sizeof(dataimage));
	memmove(arraybefore, arrayimage.bytes, sizeof(arraybefore));
}


/* Result publication follows encoding even when the result overlaps byte storage. */
static void
expectedpublication(void *value, int size)
{
	memmove(rs < 8 ? expectedresult+rs : expecteddata+rs, value, size);
}


/* Compare every frame/header/descriptor/result shift against independent UTF bytes. */
static void
checkpacked(void)
{
	unsigned keys[] = {0, 'A', 0x7f, 0x80, 0x7ff, 0x800, 0xd800, 0x20ac, 0x10000, 0x10ffff, 0x110000, 0xffffffff};
	WORD tuple[3], answer;
	unsigned key;

	for(op = 0; op < 3; op++)
	for(fs = 0; fs < 8; fs++)
	for(as = 0; as < 8; as++)
	for(ts = 0; ts < 8; ts++)
	for(rs = 0; rs < 16; rs++)
	for(sample = 0; sample < nelem(keys); sample++) {
		if(getenv("SYS_UTF_OP") && op != atoi(getenv("SYS_UTF_OP")))
			continue;
		key = keys[sample];
		length = encode(key, utf);
		prepare();
		argument = op == 0 ? sample%(length+1) : op == 1 ? 0 : 1;
		runevalue = (WORD)key;
		if(op == 0) {
			answer = argument == length ? length : 0;
			expectedpublication(&answer, sizeof(answer));
		} else if(op == 1) {
			tuple[0] = key > 0x10ffff || (key >= 0xd800 && key <= 0xdfff) ? 0xfffd : key;
			tuple[1] = length;
			tuple[2] = 1;
			expectedpublication(tuple, sizeof(tuple));
		} else {
			answer = length;
			memmove(expecteddata+9, utf, length);
			expectedpublication(&answer, sizeof(answer));
		}
		invoke();
		utfcheck(memcmp(resultimage, expectedresult, sizeof(resultimage)) == 0);
		utfcheck(memcmp(dataimage, expecteddata, sizeof(dataimage)) == 0);
		utfcheck(memcmp(arrayimage.bytes, arraybefore, sizeof(arraybefore)) == 0);
		scenarios++;
	}
}


/* Require a precise public error before any output or payload mutation. */
static void
expecterror(char *why)
{
	raised = nil;
	if(setjmp(boundary) == 0) {
		invoke();
		utfcheck(0);
	}
	utfcheck(raised == why);
	utfcheck(memcmp(dataimage, expecteddata, sizeof(dataimage)) == 0);
	utfcheck(memcmp(resultimage, expectedresult, sizeof(resultimage)) == 0);
	scenarios++;
}


/* Check wrapped result/header/descriptor/payload spans without touching poison bytes. */
static void
checkerrors(void)
{
	Array array;
	int mode;
	char *why;

	fs = as = ts = 0;
	sample = 0;
	length = 1;
	utf[0] = 'A';
	for(op = 0; op < 3; op++)
	for(rs = 0; rs < 8; rs++)
	for(mode = 0; mode < 7; mode++) {
		if(getenv("SYS_UTF_OP") && op != atoi(getenv("SYS_UTF_OP")))
			continue;
		if(getenv("SYS_UTF_ERROR") && mode != atoi(getenv("SYS_UTF_ERROR")))
			continue;
		prepare();
		argument = 0;
		runevalue = 0x20ac;
		why = exBounds;
		memmove(&array, input, sizeof(array));
		if(mode == 0)
			input = (Array*)(uintptr)(sizeof(Heap)-1);
		if(mode == 1)
			input = (Array*)((uintptr)-1-sizeof(Array)+1);
		if(mode == 2) {
			array.t = (Type*)((uintptr)-1-(offsetof(Type, np)+sizeof(int))+1);
			why = exType;
		}
		if(mode == 3)
			array.data = (uchar*)((uintptr)-1-1);
		if(mode == 4)
			array.len = -1;
		if(mode == 5) {
			array.data = nil;
			array.len = 1;
		}
		if(mode == 3)
			array.len = 4;
		if(mode >= 2 && mode < 6)
			memmove(input, &array, sizeof(array));
		if(mode == 6) {
			Heap generic = {0};
			generic.t = &other;
			memmove(genericimage.bytes, &generic, sizeof(generic));
			input = (Array*)(genericimage.bytes+sizeof(Heap));
			why = exType;
		}
		expecterror(why);
	}

}


/* Reject incomplete WORD/tuple results before even a poisoned array is inspected. */
static void
checkresults(void)
{
	uintptr address;

	for(op = 0; op < 3; op++)
	for(address = (uintptr)-1-3*sizeof(WORD); address != (uintptr)-1; address++) {
		if(getenv("SYS_UTF_OP") && op != atoi(getenv("SYS_UTF_OP")))
			continue;
		prepare();
		input = (Array*)1;
		argument = 0;
		resultslot = (void*)address;
		if(address <= (uintptr)-1-(op == 1 ? 3*sizeof(WORD) : sizeof(WORD)))
			continue;
		expecterror(exBounds);
	}
}


/* Check builtin-frame spans before any fields are read. */
static void
checkframes(void)
{
	uintptr pointer;
	int size, bad;

	for(op = 0; op < 3; op++) {
		if(getenv("SYS_UTF_OP") && op != atoi(getenv("SYS_UTF_OP")))
			continue;
		size = op == 0 ? sizeof(F_Sys_utfbytes) : op == 1 ? sizeof(F_Sys_byte2char) : sizeof(F_Sys_char2byte);
		for(bad = 0; bad < size; bad++) {
			pointer = (uintptr)-1-bad;
			raised = nil;
			if(setjmp(boundary) == 0) {
				if(op == 0)
					Sys_utfbytes((void*)pointer);
				else if(op == 1)
					Sys_byte2char((void*)pointer);
				else
					Sys_char2byte((void*)pointer);
				utfcheck(0);
			}
			utfcheck(raised == (bad == 0 ? exNilref : exBounds));
			scenarios++;
		}
	}
}


/* Use a registered pthread stack for sanitizer-aware VM error unwinding. */
static void*
utfworker(void *unused)
{
	(void)unused;
	if(getenv("SYS_UTF_FAMILY") == nil || strcmp(getenv("SYS_UTF_FAMILY"), "packed") == 0)
		checkpacked();
	if(getenv("SYS_UTF_FAMILY") == nil || strcmp(getenv("SYS_UTF_FAMILY"), "errors") == 0)
		checkerrors();
	if(getenv("SYS_UTF_FAMILY") == nil || strcmp(getenv("SYS_UTF_FAMILY"), "results") == 0)
		checkresults();
	if(getenv("SYS_UTF_FAMILY") == nil || strcmp(getenv("SYS_UTF_FAMILY"), "frames") == 0)
		checkframes();
	if(getenv("SYS_UTF_FAMILY") == nil && getenv("SYS_UTF_OP") == nil && getenv("SYS_UTF_ERROR") == nil)
		utfcheck(scenarios == (sizeof(WORD) == 8 ? 295341 : 295265));
	printf("PASS Sys UTF spans: %lu cases WORD%zu\n", scenarios, sizeof(WORD)*8);
	return nil;
}


/* Join the sole worker before returning fixture completion. */
int
main(void)
{
	pthread_t worker;

	if(pthread_create(&worker, nil, utfworker, nil) != 0 || pthread_join(worker, nil) != 0)
		return 1;
	return 0;
}
