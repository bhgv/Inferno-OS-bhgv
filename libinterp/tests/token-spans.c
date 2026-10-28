
typedef struct TokenValue TokenValue;

struct TokenValue {
	WORD count; // Published complete token count.
	List *words; // Owned result list.
};

static uchar frameimage[sizeof(F_Sys_tokenize)+16], slots[sizeof(TokenValue)+16];
static uchar before[sizeof(slots)], delimiterimage[sizeof(String)+32];
static String *source, *delimiter;
static List *old;
static void *ret;
static int fs, rs, ds, wide, dwide, fail, previous;
static unsigned long cases;


/* Identify the full active geometry before stopping an explicit failed predicate. */
static void
tokenpredicate(int test)
{
	if(!test) {
		fprintf(stderr, "token spans frame=%d result=%d delim=%d wide=%d dwide=%d fail=%d old=%d\n",
			fs, rs, ds, wide, dwide, fail, previous);
		abort();
	}
}


/* Call the production public entry under actual emu/os VM error frames. */
static int
spancall(void *frame)
{
	Sys_tokenize(frame);
	return 1;
}


/* Construct an owning source and a packed borrowed separator view independently. */
static void
prepare(void)
{
	Rune rune;
	char *cache;
	int length, capacity, i;

	source = wide ? newrunes(5) : newstring(5);
	for(i = 0; i < 5; i++) {
		rune = i == 0 ? 'a' : i == 2 ? 0 : i == 4 ? 'z' : ' ';
		if(wide)
			memmove((uchar*)source+offsetof(String, data)+i*sizeof(rune), &rune, sizeof(rune));
		else
			source->Sascii[i] = rune;
	}
	memset(delimiterimage, 0xa5, sizeof(delimiterimage));
	delimiter = (String*)(delimiterimage+ds);
	length = dwide ? -1 : 1;
	capacity = 1;
	cache = nil;
	memmove((uchar*)delimiter+offsetof(String, len), &length, sizeof(length));
	memmove((uchar*)delimiter+offsetof(String, max), &capacity, sizeof(capacity));
	memmove((uchar*)delimiter+offsetof(String, tmp), &cache, sizeof(cache));
	rune = ' ';
	if(dwide)
		memmove((uchar*)delimiter+offsetof(String, data), &rune, sizeof(rune));
	else
		*((uchar*)delimiter+offsetof(String, data)) = ' ';
	old = H;
	if(previous) {
		List *node;
		node = cons(sizeof(source), &old);
		node->t = &Tptr;
		Tptr.ref++;
		memmove(node->data, &source, sizeof(source));
		D2H(source)->ref++;
	}
	memset(slots, 0xa5, sizeof(slots));
	length = -77;
	{
		WORD count = length;
		memmove(slots+rs+offsetof(TokenValue, count), &count, sizeof(count));
	}
	memmove(slots+rs+offsetof(TokenValue, words), &old, sizeof(old));
	ret = slots+rs;
	memmove(before, slots, sizeof(before));
}


/* Populate generated builtin frames using aligned locals and byte-safe publication. */
static void
frame(void)
{
	F_Sys_tokenize value = {0};

	value.s = source;
	value.delim = delimiter;
	value.ret = ret;
	memmove(frameimage+fs, &value, sizeof(value));
}


/* Verify every packed delimiter/frame/result and allocation-failure boundary. */
static void
checkpacked(void)
{
	WORD count;
	List *words, *node;
	String *word;
	int ok, baseline, i, j;
	Rune rune;

	for(fs = 0; fs < 8; fs++)
	for(rs = 0; rs < 8; rs++)
	for(ds = 0; ds < 8; ds++)
	for(wide = 0; wide < 2; wide++)
	for(dwide = 0; dwide < 2; dwide++)
	for(previous = 0; previous < 2; previous++)
	for(fail = 0; fail < 8; fail++) {
		failure = 0;
		prepare();
		frame();
		baseline = live;
		allocations = 0;
		failure = fail;
		raised = nil;
		ok = kprotect(spancall, frameimage+fs);
		failure = 0;
		tokencheck(worker.nerr == 0);
		if(fail > 0 && fail <= 6) {
			tokencheck(!ok && raised == exHeap && live == baseline);
			tokencheck(memcmp(slots, before, sizeof(before)) == 0);
			tokencheck(D2H(source)->ref == 1+previous);
			destroy(old);
		} else {
			tokencheck(ok && allocations == 6);
			memmove(&count, slots+rs+offsetof(TokenValue, count), sizeof(count));
			memmove(&words, slots+rs+offsetof(TokenValue, words), sizeof(words));
			tokencheck(count == 3 && D2H(source)->ref == 1);
			node = words;
			for(i = 0; i < 3; i++) {
				tokencheck(node != H);
				memmove(&word, node->data, sizeof(word));
				tokencheck(word->len == (wide ? -1 : 1));
				rune = i == 0 ? 'a' : i == 1 ? 0 : 'z';
				tokencheck((wide ? word->Srune[0] : (uchar)word->Sascii[0]) == rune);
				node = node->tail;
			}
			tokencheck(node == H);
			for(j = 0; j < sizeof(slots); j++)
				if(!((j >= rs && j < rs+sizeof(WORD)) ||
				   (j >= rs+offsetof(TokenValue, words) && j < rs+offsetof(TokenValue, words)+sizeof(words))))
					tokencheck(slots[j] == before[j]);
			destroy(words);
		}
		destroy(source);
		tokencheck(live == 0 && Tstring.ref == 1 && Tlist.ref == 1 && Tptr.ref == 1);
		cases++;
	}
}


/* Reject malformed result/frame/header and old-owner spans before any allocation. */
static void
checkerrors(void)
{
	uintptr address;
	void *callframe;
	char *why;
	int mode, ok;
	Heap dead = {0};

	fs = ds = wide = dwide = previous = fail = 0;
	for(rs = 0; rs < 8; rs++)
	for(mode = 0; mode < 8; mode++) {
		if(getenv("TOKEN_SPAN_ERROR") && mode != atoi(getenv("TOKEN_SPAN_ERROR")))
			continue;
		failure = 0;
		prepare();
		why = exHeap;
		if(mode == 0)
			ret = (void*)((uintptr)-1-sizeof(TokenValue)+1);
		if(mode == 1)
			delimiter = (String*)((uintptr)-1-(offsetof(String, max)+sizeof(int))+1);
		if(mode == 2)
			source->max = 0;
		if(mode == 3) {
			int length = (-0x7fffffff-1);
			memmove((uchar*)delimiter+offsetof(String, len), &length, sizeof(length));
		}
		if(mode == 2 || mode == 3)
			why = exInval;
		if(mode == 4 || mode == 5) {
			List *invalid = mode == 4 ? nil : (List*)1;
			memmove(slots+rs+offsetof(TokenValue, words), &invalid, sizeof(invalid));
			if(mode == 4)
				why = exNilref;
		}
		if(mode == 6) {
			List *invalid = (List*)(&dead+1);
			memmove(slots+rs+offsetof(TokenValue, words), &invalid, sizeof(invalid));
		}
		memmove(before, slots, sizeof(before));
		frame();
		callframe = frameimage+fs;
		if(mode == 7)
			callframe = (void*)((uintptr)-1-sizeof(F_Sys_tokenize)+1);
		allocations = 0;
		raised = nil;
		ok = kprotect(spancall, callframe);
		tokencheck(!ok && raised == why && allocations == 0 && worker.nerr == 0);
		tokencheck(memcmp(before, slots, sizeof(before)) == 0 && live == 1);
		source->max = 5;
		destroy(source);
		tokencheck(live == 0);
		cases++;
	}
	if(getenv("TOKEN_SPAN_ERROR"))
		return;
	for(address = (uintptr)-1-sizeof(TokenValue)+1; address != (uintptr)-1; address++) {
		failure = 0;
		prepare();
		ret = (void*)address;
		frame();
		allocations = 0;
		ok = kprotect(spancall, frameimage);
		tokencheck(!ok && raised == exHeap && allocations == 0);
		destroy(source);
		cases++;
	}
}


/* Use one registered VM worker with real nested emu/os error boundaries. */
static void*
tokenworker(void *unused)
{
	(void)unused;
	if(getenv("TOKEN_SPAN_FAMILY") == nil || strcmp(getenv("TOKEN_SPAN_FAMILY"), "packed") == 0)
		checkpacked();
	if(getenv("TOKEN_SPAN_FAMILY") == nil || strcmp(getenv("TOKEN_SPAN_FAMILY"), "errors") == 0)
		checkerrors();
	if(getenv("TOKEN_SPAN_FAMILY") == nil && getenv("TOKEN_SPAN_ERROR") == nil)
		tokencheck(cases == 32847);
	tokencheck(worker.nerr == 0 && live == 0 && Tptr.ref == 1 && Tlist.ref == 1 && Tstring.ref == 1);
	printf("PASS token spans: %lu cases WORD%zu\n", cases, sizeof(WORD)*8);
	return nil;
}


/* Join the sole worker before returning the complete fixture status. */
int
main(void)
{
	pthread_t thread;

	if(pthread_create(&thread, nil, tokenworker, nil) != 0 || pthread_join(thread, nil) != 0)
		return 1;
	return 0;
}
