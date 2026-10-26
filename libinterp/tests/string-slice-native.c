/* Allocate ordinary publication metadata without changing the VM heap fixture. */
void*
mallocz(ulong size, int clear)
{
	void *p;

	p = malloc(size);
	if(p != nil && clear)
		memset(p, 0, size);
	return p;
}


/* Exercise the production compiler's normal executable publication boundary. */
int
kprotect(int (*fn)(void*), void *arg)
{
	return fn(arg);
}

int mutator, nprop;


/* Require malformed operands to preserve slots, owners and allocation counters. */
static void
checkrejections(Module *module)
{
	String *owner, *slot;
	WORD start, end;
	uchar saved[128];
	char *expected;
	int mode, allocations;

	for(mode = 0; mode < 11; mode++){
		owner = newstring(16);
		owner->len = 8;
		slot = owner; start = 1; end = 4; expected = exHeap;
		switch(mode){
		case 0: slot = nil; expected = exNilref; break;
		case 1: slot = H; expected = exBounds; break;
		case 2: owner->len = (-0x7fffffff-1); break;
		case 3: owner->max = 7; break;
		case 4: owner->max = -1; break;
		case 5: D2H(owner)->ref = 0; break;
		case 6: start = -1; expected = exBounds; break;
		case 7: end = 9; expected = exBounds; break;
		case 8: end = 0; expected = exBounds; break;
		case 9: end = (WORD)((UWORD)1 << (sizeof(WORD)*8-2)); expected = exBounds; break;
		case 10: start = end = -1; expected = exBounds; break;
		}
		R.s = &start; R.m = &end; R.d = &slot;
		memmove(saved, owner, sizeof(String)+16);
		allocations = calls;
		if(module->jit->entry[ISLICEC](&R) != 0 || calls != allocations ||
		   memcmp(owner, saved, sizeof(String)+16))abort();
		raised = nil;
		if(setjmp(boundary) == 0){slicec(); abort();}
		if(raised != expected || calls != allocations ||
		   memcmp(owner, saved, sizeof(String)+16) ||
		   slot != (mode == 0 ? nil : mode == 1 ? H : owner))abort();
		owner->len = 8; owner->max = 16; D2H(owner)->ref = 1;
		destroy(owner);
	}
}


/* Large prefixes must change only signed length, without accessing untouched pages. */
static void
checkprefixes(Module *module)
{
	Heap *header;
	String *owner, *slot;
	WORD start = 0, end;
	ulong bytes;
	int wide, pass, variant, before;

	bytes = (ulong)0x66666668+sizeof(Heap)+sizeof(String)+4096;
	header = mmap(nil, bytes, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(header == MAP_FAILED || mprotect(header, 4096, PROT_READ|PROT_WRITE) != 0)abort();
	header->ref = 1; header->t = &Tstring;
	owner = H2D(String*, header);
	for(wide = 0; wide < 2; wide++)
	for(pass = 0; pass < 2; pass++)
	for(variant = 0; variant < 2; variant++){
		owner->max = wide ? 0x1999999a : 0x66666668;
		owner->len = wide ? -owner->max : owner->max;
		end = variant ? owner->max : 129;
		slot = owner; R.s = &start; R.m = &end; R.d = &slot; before = calls;
		if(pass == 0){
			if(module->jit->entry[ISLICEC](&R) != 1)abort();
		}else slicec();
		if(slot != owner || owner->len != (wide ? -end : end) || calls != before ||
		   header->ref != 1 || owner->max != (wide ? 0x1999999a : 0x66666668))abort();
	}
	if(munmap(header, bytes) != 0)abort();
}


/* Compare moved slices around the expanded copy limit with published RX code. */
static void
checkcopies(Module *module)
{
	String *owner, *slot;
	uchar original[2304], expected[2304], slots[128], before[128];
	WORD start, end;
	int starts[] = {1, 7, 127};
	int counts[] = {127, 128, 129, 255, 256, 257, 384};
	int wide, first, count, alignment, pass, size, stride, i, allocations, native;
	int total = 0;

	for(wide = 0; wide < 2; wide++)
	for(first = 0; first < nelem(starts); first++)
	for(count = 0; count < nelem(counts); count++)
	for(alignment = 0; alignment < 8; alignment++){
		owner = wide ? newrunes(512) : newstring(512);
		owner->len = wide ? -512 : 512;
		stride = wide ? sizeof(Rune) : 1;
		size = sizeof(String)+512*stride;
		if(size > sizeof(original))
			abort();
		for(i = 0; i < 512; i++){
			if(wide)
				owner->Srune[i] = 0x80000100U+i;
			else
				owner->Sascii[i] = 33+i;
		}
		memmove(original, owner, size);
		memmove(expected, original, size);
		start = starts[first];
		end = start+counts[count];
		memmove(expected+offsetof(String, data), original+offsetof(String, data)+start*stride,
			counts[count]*stride);
		((String*)expected)->len = wide ? -counts[count] : counts[count];
		for(pass = 0; pass < 2; pass++){
			memmove(owner, original, size);
			slot = owner;
			memset(slots, 0xa5, sizeof(slots));
			R.s = slots+alignment; R.m = slots+32+alignment; R.d = slots+64+alignment;
			memmove(R.s, &start, sizeof(start));
			memmove(R.m, &end, sizeof(end));
			memmove(R.d, &slot, sizeof(slot));
			memmove(before, slots, sizeof(before));
			allocations = calls;
			native = counts[count] <= 256;
			if(pass == 0){
				if(module->jit->entry[ISLICEC](&R) != native)
					abort();
			}else
				slicec();
			if(memcmp(owner, pass == 0 && !native ? original : expected, size) ||
			   memcmp(slots, before, sizeof(slots)) || calls != allocations ||
			   D2H(owner)->ref != 1)
				abort();
			total++;
		}
		destroy(owner);
		if(live || Tstring.ref != 1)
			abort();
	}
	printf("PASS moved string slices: %d C/RX/copy-limit/image cases WORD%zu\n", total, sizeof(WORD)*8);
}


/* Compare production C and RX code against independent slice/ownership expectations. */
int
main(void)
{
	Module module = {0};
	Inst instruction = {0};
	String *owner, *result;
	uchar slots[128], before[128], original[800];
	WORD start, end;
	int wide, shared, first, last, alignment, pass, i, allocations, native, length;
	int refs[] = {0, -1, 0x7fffffff};
	ulong total = 0;

	instruction.op = ISLICEC;
	module.prog = &instruction;
	module.nprog = 1;
	if(!compile(&module, 1, nil) || !module.jit || !module.jit->entry[ISLICEC])
		abort();
	checkrejections(&module);
	checkprefixes(&module);
	checkcopies(&module);
	for(wide = 0; wide < 2; wide++)
	for(shared = 0; shared < 2; shared++)
	for(first = 0; first < 10; first++)
	for(last = first; last < 10; last++)
	for(alignment = 0; alignment < 8; alignment++)
	for(pass = 0; pass < 2; pass++){
		owner = wide ? newrunes(160) : newstring(160);
		owner->len = wide ? -9 : 9;
		for(i = 0; i < 9; i++){
			if(wide)owner->Srune[i] = 0x80000100U+i;
			else owner->Sascii[i] = 33+i;
		}
		D2H(owner)->ref = shared ? 2 : 1;
		memmove(original, owner, sizeof(String)+160*(wide ? sizeof(Rune) : 1));
		memset(slots, 0xa5, sizeof(slots));
		start = first; end = last;
		R.s = slots+alignment; R.m = slots+32+alignment; R.d = slots+64+alignment;
		memmove(R.s, &start, sizeof(start)); memmove(R.m, &end, sizeof(end));
		memmove(R.d, &owner, sizeof(owner)); memmove(before, slots, sizeof(slots));
		allocations = calls;
		native = !shared && first != last;
		if(pass == 0){
			if(module.jit->entry[ISLICEC](&R) != native)
				abort();
			if(!native && (memcmp(slots, before, sizeof(slots)) || calls != allocations ||
			   memcmp(owner, original, sizeof(String)+160*(wide ? sizeof(Rune) : 1))))
				abort();
			if(!native)slicec();
		}else
			slicec();
		memmove(&result, R.d, sizeof(result));
		length = last-first;
		if(length == 0){
			if(result != H)abort();
		}else{
			if(result->len != (wide ? -length : length))abort();
			for(i = 0; i < length; i++)
				if(wide ? result->Srune[i] != 0x80000100U+first+i : result->Sascii[i] != 33+first+i)
					abort();
			if(!shared && (result != owner || calls != allocations))abort();
		}
		if(shared){
			if(D2H(owner)->ref != 1 || memcmp(owner, original, sizeof(String)+160*(wide ? sizeof(Rune) : 1)))
				abort();
			destroy(owner);
		}
		if(memcmp(slots, before, 64+alignment) ||
		   memcmp(slots+64+alignment+sizeof(result), before+64+alignment+sizeof(result),
		   sizeof(slots)-64-alignment-sizeof(result)))abort();
		destroy(result);
		total++;
	}
	/* Dead owners must fail before allocation even when an empty result is requested. */
	owner = newstring(8); D2H(owner)->ref = 0;
	start = end = 0; R.s = &start; R.m = &end; R.d = &owner;
	allocations = calls; raised = nil;
	if(setjmp(boundary) == 0){slicec(); abort();}
	if(raised != exHeap || calls != allocations || D2H(owner)->ref != 0)abort();
	raised = nil;
	if(setjmp(boundary) == 0){slicer(0, 0, owner); abort();}
	if(raised != exHeap || calls != allocations)abort();
	raised = nil;
	if(setjmp(boundary) == 0){stringdup(owner); abort();}
	if(raised != exHeap || calls != allocations)abort();
	D2H(owner)->ref = 1; destroy(owner);
	/* Scalar string constructors must reserve the descriptor before allocating. */
	for(i = 0; i < nelem(refs); i++)
	for(wide = 0; wide < 3; wide++){
		Tstring.ref = refs[i]; allocations = calls; raised = nil;
		if(setjmp(boundary) == 0){
			if(wide == 0)newstring(8);
			else newrunes(wide == 1 ? 8 : 0);
			abort();
		}
		if(raised != exHeap || calls != allocations || Tstring.ref != refs[i])abort();
	}
	for(wide = 0; wide < 3; wide++){
		Tstring.ref = 0x7ffffffe;
		owner = wide == 0 ? newstring(1) : newrunes(wide == 1 ? -1 : 0);
		if(Tstring.ref != 0x7fffffff)abort();
		destroy(owner);
		if(Tstring.ref != 0x7ffffffe)abort();
	}
	Tstring.ref = 1;

	module.jit->freecode(module.jit->code, module.jit->size);
	free(module.jit);
	if(live != 0 || Tstring.ref != 1)abort();
	printf("PASS string slices: %lu C/RX cases WORD%zu, dead-owner rejection\n", total, sizeof(WORD)*8);
	return 0;
}
