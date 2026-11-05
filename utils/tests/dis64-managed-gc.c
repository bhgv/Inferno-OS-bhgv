/* Symbolic String/List ownership fixture. No file kind or VM call is admitted here. */
static Type *stringelement;

static void
managedput(void **slot, void *owned)
{
	void *old;

	old = *slot;
	*slot = owned;
	if(owned != H)
		Setmark(D2H(owned));
	destroy(old);
}

static String*
managedstring(void)
{
	String *s;

	s = H2D(String*, heapz(&Tstring));
	s->len = 1;
	s->max = 1;
	s->Sascii[0] = 'x';
	return s;
}

static List*
managedlist(String *value)
{
	List *list = H;
	int i;

	D2H(value)->ref++;
	for(i=0; i<2; i++){
		cons(sizeof(value), &list);
		list->t = stringelement;
		stringelement->ref++;
		memmove(list->data, &value, sizeof(value));
		Setmark(D2H(value));
	}
	return list;
}

static String*
managedhead(List *list)
{
	String *value;

	memmove(&value, list->data, sizeof(value));
	return value;
}

/* Trace only actual mapped storage. Neither the transient address nor C locals root values. */
static void
managedcase(Type *slots, int reverse, int empty, int unwind)
{
	Heap *caller, *callee;
	void **outer, **inner, *result, *argument;
	String *text;
	int slot;

	caller = heapz(slots);
	ptradd(caller);
	callee = heapz(slots);
	ptradd(callee);
	outer = H2D(void**, caller);
	inner = H2D(void**, callee);
	require(outer[0] == H && outer[1] == H && inner[0] == H && inner[1] == H);

	text = empty ? H : managedstring();
	argument = reverse && !empty ? (void*)managedlist(text) : text;
	managedput(outer, argument);
	if(argument != H)
		D2H(argument)->ref++;
	managedput(inner, argument);
	managedput(outer, H);
	numericcollect(nil);
	if(argument != H){
		require(numericowner(D2H(argument))->magic == MAGIC_A);
		require(numericowner(D2H(text))->magic == MAGIC_A);
		if(reverse)
			require(managedhead(argument) == text &&
				managedhead(((List*)argument)->tail) == text);
		D2H(text)->ref++;
	}
	result = empty ? H : reverse ? (void*)text : managedlist(text);
	managedput(inner+1, result);
	numericcollect(nil);
	require(outer[1] == H);
	if(result != H)
		require(numericowner(D2H(result))->magic == MAGIC_A);
	if(!unwind){
		inner[1] = H;
		managedput(outer+1, result);
	}
	ptrdel(callee);
	destroy(H2D(void*, callee));
	numericcollect(nil);
	require(outer[1] == (unwind ? H : result));
	if(result != H)
		require(numericowner(D2H(result))->magic == (unwind ? MAGIC_F : MAGIC_A));
	if(!unwind && !empty)
		require((reverse ? (String*)outer[1] : managedhead(outer[1]))->Sascii[0] == 'x');
	ptrdel(caller);
	destroy(H2D(void*, caller));
	numericcollect(nil);
	for(slot=1; slot<GcSlots; slot++)
		require(numericblock(slot)->magic == MAGIC_F);
	require(Tstring.ref == 1 && Tlist.ref == 1 && stringelement->ref == 1);
}

int
main(void)
{
	static uchar two[] = {0xc0}, one[] = {0x80};
	Type *slots;
	int reverse, empty, unwind;

	numericpoolinit();
	Tstring.size = sizeof(String);
	Tstring.free = freestring;
	Tstring.mark = noptrs;
	stringelement = dtype(freeheap, sizeof(String*), one, sizeof(one));
	slots = dtype(freeheap, 2*sizeof(void*), two, sizeof(two));
	require(stringelement != nil && slots != nil);
	for(reverse=0; reverse<2; reverse++)
		for(empty=0; empty<2; empty++)
			for(unwind=0; unwind<2; unwind++)
				managedcase(slots, reverse, empty, unwind);
	freetype(slots);
	freetype(stringelement);
	numericdrain();
	require(live == 0);
	printf("PASS symbolic String/List GC: 8 ownership cases, %lu epochs, %d checks\n", gcepochs, checks);
	return 0;
}
