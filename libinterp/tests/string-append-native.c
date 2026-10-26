/* Supply real native code publication with ordinary descriptor allocation. */
void*
mallocz(ulong n, int clear)
{
	void *p;

	p = malloc(n);
	if(p != nil && clear)
		memset(p, 0, n);
	return p;
}


/* Run compilation through its normal publication boundary. */
int
kprotect(int (*fn)(void*), void *arg)
{
	return fn(arg);
}

int mutator, nprop;


/* Reserve a genuine large string span without committing its untouched payload. */
static void
checklarge(Module *module)
{
	Heap *header;
	String *left, *right, *result;
	ulong bytes;
	int pass, allocations, mode;

	bytes = (ulong)0x66666668+sizeof(Heap)+sizeof(String)+16;
	header = mmap(nil, bytes, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(header == MAP_FAILED)
		abort();
	header->ref = 1;
	header->t = &Tstring;
	left = H2D(String*, header);
	left->len = left->max = 0x66666668;
	right = newstring(0);
	for(pass = 0; pass < 2; pass++){
		result = left;
		R.s = &right; R.m = R.d = &result;
		allocations = calls;
		if(pass == 0){
			if(module->jit->entry[IADDC](&R) != 1)
				abort();
		}else{
			raised = nil;
			if(setjmp(boundary) == 0)
				addc();
			else
				abort();
		}
		if(result != left || left->len != 0x66666668 || calls != allocations || header->ref != 1)
			abort();
	}
	destroy(right);
	/* Required growth and representation promotion still reject an impossible allocation. */
	for(mode = 0; mode < 2; mode++){
		right = mode ? newrunes(1) : newstring(1);
		left->max = left->len+(mode ? 4 : 0);
		result = left;
		R.s = &right; R.m = R.d = &result;
		allocations = calls;
		if(module->jit->entry[IADDC](&R) != 0)
			abort();
		raised = nil;
		if(setjmp(boundary) == 0){
			addc();
			abort();
		}
		if(raised != exHeap || result != left || left->len != 0x66666668 || calls != allocations)
			abort();
		destroy(right);
	}
	if(munmap(header, bytes) != 0)
		abort();
}


/* Compare complete heap images and operand slots on success and write-free fallback. */
int
main(void)
{
	Module module = {0};
	Inst instruction = {0}, sentinel;
	String *left, *right, *source, *destination;
	REG saved;
	uchar slots[128], before[128], leftimage[2048], rightimage[2048], expected[2048];
	int lengths[] = {0, 1, 8, 127, 128, 129, 255, 256, 257};
	int li, ri, mode, sa, ma, pass, i, l, r, maximum, valid, allocations, size, wide;
	char *exception;
	unsigned long total = 0;

	instruction.op = IADDC;
	module.prog = &instruction;
	module.nprog = 1;
	if(!compile(&module, 1, nil) || !module.jit || !module.jit->entry[IADDC])
		abort();
	if(getenv("APPEND_LARGE_ONLY")){
		checklarge(&module);
		goto finished;
	}
	for(wide = 0; wide < 2; wide++)
	for(li = 0; li < nelem(lengths); li++)
	for(ri = 0; ri < nelem(lengths); ri++)
	for(mode = 0; mode < 13; mode++)
	for(sa = 0; sa < 8; sa++)
	for(ma = 0; ma < 8; ma++){
		l = lengths[li]; r = lengths[ri];
		left = wide ? newrunes(320) : newstring(320);
		right = wide ? newrunes(320) : newstring(320);
		maximum = left->max;
		left->len = wide ? -l : l; right->len = wide ? -r : r;
		for(i = 0; i < l; i++){
			if(wide)left->Srune[i] = (Rune)i*0x1000101U+0x80000000U;
			else left->Sascii[i] = i*73+1;
		}
		for(i = 0; i < r; i++){
			if(wide)right->Srune[i] = (Rune)i*0x1000101U+0xffffffffU;
			else right->Sascii[i] = i*17+255;
		}
		source = right;
		if(mode == 2)D2H(left)->ref = 2;
		if(mode == 3)D2H(left)->ref = 0;
		if(mode == 4)left->max = l-1;
		if(mode == 5)right->max = r-1;
		if(mode == 6)left->max = l;
		if(mode == 7)right->len = left->len < 0 ? 1 : -1;
		if(mode == 8)left->len = right->len < 0 ? 1 : -1;
		if(mode == 9)source = H;
		if(mode == 10)source = nil;
		if(mode == 11){source = left; r = l;}
		if(mode == 12)D2H(right)->ref = 0;
		memset(slots, 0xa5, sizeof(slots));
		memset(&R, 0, sizeof(R));
		R.s = slots+8+sa; R.m = slots+40+ma;
		R.d = mode == 1 ? slots+80+sa : R.m;
		if(mode == 11)R.s = R.m;
		destination = left;
		memmove(R.s, &source, sizeof(source));
		memmove(R.m, &left, sizeof(left));
		memmove(R.d, &destination, sizeof(destination));
		R.PC = &sentinel;
		saved = R;
		valid = (mode == 0 || mode == 6 || mode == 11) && r <= 256 && l+r <= left->max &&
			(left->len < 0) == (source->len < 0);
		exception = mode == 3 || mode == 4 || mode == 5 || mode == 12 ? exHeap : mode == 10 ? exNilref : nil;
		size = sizeof(Heap)+sizeof(String)+320*(wide ? sizeof(Rune) : 1)+32;
		if(size > sizeof(leftimage))abort();
		memmove(leftimage, D2H(left), size);
		memmove(rightimage, D2H(right), size);
		memmove(expected, leftimage, size);
		memmove(before, slots, sizeof(slots));
		if(valid){
			int offset = (uchar*)left->Sascii-(uchar*)D2H(left);
			int combined = left->len < 0 ? -(l+r) : l+r;
			memmove(expected+offset+l*(wide ? sizeof(Rune) : 1), source->Sascii, r*(wide ? sizeof(Rune) : 1));
			memmove(expected+((uchar*)&left->len-(uchar*)D2H(left)), &combined, sizeof(combined));
		}
		allocations = calls;
		for(pass = 0; pass < 2; pass++){
			memmove(D2H(left), leftimage, size);
			memmove(D2H(right), rightimage, size);
			memmove(slots, before, sizeof(slots));
			R = saved; raised = nil;
			if(pass == 0){
				if(module.jit->entry[IADDC](&R) != valid)abort();
			}else if(valid || exception){
				if(setjmp(boundary) == 0){
					addc();
					if(exception)abort();
				}else if(raised != exception)
					abort();
			}
			if(memcmp(D2H(left), expected, size) || memcmp(D2H(right), rightimage, size) ||
			   memcmp(slots, before, sizeof(slots)) || memcmp(&R, &saved, sizeof(R)) || calls != allocations)
				abort();
		}
		D2H(left)->ref = 1;
		D2H(right)->ref = 1;
		left->max = maximum; right->max = maximum;
		destroy(left); destroy(right);
		if(live)abort();
		total++;
	}
	checklarge(&module);
finished:
	module.jit->freecode(module.jit->code, module.jit->size);
	free(module.jit);
	printf("PASS native string append: %lu C/JIT/image/slot/owner cases WORD%zu\n", total, sizeof(WORD)*8);
	return 0;
}
