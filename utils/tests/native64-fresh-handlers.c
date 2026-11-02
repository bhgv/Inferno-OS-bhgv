static String *exstr = H; // Per-case emergency buffer, retained independently of the delivered message.
static void *freshparent, *freshchild; // Borrowed identities that must be marked through the caller map.
static char *freshtext; // Original stable message supplied to the actual constructor.
static int freshkind, freshfailure, freshscans, freshenabled, freshfailed; // Message/memory profiles and observations.
char exNegsize[] = "negative size"; // Production string constructor's ordinary validation error.

static void runcatch(int, int, int, int, int, int, int); // Existing actual-call fixture specialized below.


/* Supply exact allocator extent queries without substituting string construction. */
ulong
poolmsize(Pool *pool, void *pointer)
{
	void *block;
	int i;

	(void)pool;
	block = (uchar*)pointer-O(Bhdr, u.data);
	for(i=0; i<RuntimeBlocks && blocks[i].pointer != block; i++)
		;
	require(i < RuntimeBlocks);
	return blocks[i].bytes-O(Bhdr, u.data);
}


/* Preserve the production emergency buffer's immutable pool identity. */
void
poolimmutable(void *pointer)
{
	Bhdr *block;

	D2B(block, pointer);
	block->magic = MAGIC_I;
}


/* Preserve the emergency ASCII buffer's bounded byte-copy contract. */
void
kstrcpy(char *destination, char *source, int bytes)
{
	int count;

	require(bytes > 0);
	count = strlen(source);
	if(count >= bytes)
		count = bytes-1;
	memmove(destination, source, count);
	destination[count] = 0;
}

MESSAGE_IMPLEMENTATION


/* Keep an independently owned graph live only through the caller's declared map. */
static void
freshsetup(Module *module)
{
	Type *pair;
	uchar map[] = {0x80}; // The parent object's first native slot owns its child.

	freshenabled = freshscans = freshfailed = 0;
	pair = dtype(freeheap, sizeof(void*), map, sizeof(map));
	require(pair != nil);
	freshchild = H2D(void*, heapz(module->type[0]));
	freshparent = H2D(void*, heapz(pair));
	freetype(pair);
	memmove(freshparent, &freshchild, sizeof(freshchild));
	memmove(handlerroot+112, &freshparent, sizeof(freshparent));
	excinit();
	require(exstr != H && exstr->max >= ERRMAX);
}


/* Scan actual published process roots before the pool creates a new untyped object. */
static void
allocroots(void)
{
	void *slot;
	REG *published;

	if(!freshenabled)
		return;
	published = &handlerprocess->R;
	if(published->FP != R.FP || published->SP != R.SP ||
	   published->TS != R.TS || published->EX != R.EX ||
	   published->M != R.M || published->MP != R.MP){
		fprintf(stderr, "stale process roots before message\n");
		exit(12);
	}
	require(memcmp(published, &R, sizeof(R)) == 0);
	require(gchalt == 0 && R.FP == handlerroot && worker.nerr == 1);
	memmove(&slot, handlerroot+96, sizeof(slot));
	require(slot == H);
	D2H(freshparent)->color = propagator;
	D2H(freshchild)->color = propagator;
	visit = 1000;
	rootset(handlerprocess);
	if(D2H(freshparent)->color != mutator || D2H(freshchild)->color != mutator){
		fprintf(stderr, "live caller graph was not marked\n");
		exit(13);
	}
	require(D2H(freshparent)->ref == 1 && D2H(freshchild)->ref == 1);
	freshscans++;
	if(freshfailure && !freshfailed){
		failat = allocations+1;
		freshfailed = 1;
	}
	/* The actual message construction must also survive a borrowed global REG. */
	if(borrowregisters & 1)
		memset(&R, 0x44, sizeof(R));
}


/* Invoke the real handler with no pre-existing process exception to transfer. */
static int
freshinvoke(int failure)
{
	static uchar rune[] = {0xd0, 0xb6, 0}; // UTF-8 for U+0436, independently checked below.
	int selected;

	freshtext = freshkind ? (char*)rune : failure == 3 ? exBounds : exZdiv;
	freshenabled = 1;
	selected = handler(freshtext);
	freshenabled = 0;
	failat = 0;
	return selected;
}


/* Check actual normal and emergency payloads, ownership and protected error depth. */
static void
freshresult(void *value)
{
	String *string;
	int bytes;

	string = value;
	bytes = strlen(freshtext);
	require(freshscans == 1 && freshfailed == freshfailure && worker.nerr == 0);
	require(value != H && D2H(value)->t == &Tstring);
	if(freshfailure){
		require(string == exstr && D2H(value)->ref == 2);
		require(string->len == bytes && memcmp(string->Sascii, freshtext, bytes) == 0);
	}else{
		require(string != exstr && D2H(value)->ref == 1);
		if(freshkind)
			require(string->len == -1 && string->Srune[0] == 0x436);
		else
			require(string->len == bytes && memcmp(string->Sascii, freshtext, bytes) == 0);
	}
}


/* Keep each batch readable while retaining the previous complete call/error matrix. */
static int
freshbatch(void)
{
	int jit, grow, failure, q, padding, borrow, route, cases;
	int quanta[] = {1, 2, 3, 7, 16, 64, 1000};

	cases = 0;
	for(jit=0; jit<2; jit++)
		for(grow=0; grow<2; grow++)
			for(failure=1; failure<=3; failure++)
				for(q=0; q<(int)nelem(quanta); q++)
					for(padding=0; padding<=8; padding+=8)
						for(borrow=0; borrow<4; borrow++)
							for(route=0; route<2; route++){
								runcatch(jit, grow, failure, quanta[q], padding, borrow, route);
								cases++;
							}
	return cases;
}


/* Exercise real message allocation/fallback after actual C/RX call-stack recovery. */
int
main(int argc, char **argv)
{
	int cases;

	Tstring.size = sizeof(String);
	Tstring.free = freeheap;
	Tstring.mark = markheap;
	optab[IFRAME] = frame;
	optab[ICALL] = call;
	optab[IRET] = ret;
	optab[IMOVP] = movp;
	optab[IMOVW] = movw;
	optab[IMOVL] = movl;
	optab[IADDW] = addw;
	optab[IDIVW] = divw;
	optab[IEXIT] = iexit;
	if(argc == 2){
		runcatch(atoi(argv[1]), 1, 2, 1, 0, 0, 0);
		return 0;
	}
	require(argc == 1);
	cases = 0;
	for(freshkind=0; freshkind<2; freshkind++)
		for(freshfailure=0; freshfailure<2; freshfailure++)
			cases += freshbatch();
	printf("PASS fresh handler messages: %d cases WORD%d; actual constructors/rootset/markheap; no sweep or parallel GC\n", cases, (int)(8*sizeof(WORD)));
	return 0;
}
