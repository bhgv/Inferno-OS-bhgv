/* Supply the builtin identity used by numeric leaf publication in this linked fixture. */
Type Tstring = {1}, Tptr = {1};


/* Keep executable-code descriptor allocation outside the bounded VM pool. */
void*
mallocz(ulong size, int clear)
{
	void *p;

	p = malloc(size);
	if(p && clear)memset(p, 0, size);
	return p;
}


/* Shift real root storage twice, then copy, share and release the original headers. */
static void
rootcopies(Jit *jit)
{
	int widths[] = {0, 1, 3, 8, 17};
	uchar original[512], *base;
	Array *root, *view, *copy, *identity;
	WORD first, last;
	int wi, pass, step, i, before;
	ulong total = 0;

	for(wi = 0; wi < 5; wi++)for(pass = 0; pass < 2; pass++){
		type.size = widths[wi];
		type.np = 0;
		type.map[0] = 0;
		root = H2D(Array*, heaparray(&type, 16));
		identity = root;
		base = root->data;
		for(i = 0; i < 16*type.size; i++)original[i] = base[i] = (uchar)(i*7+19);
		R.s = &first;
		R.m = &last;
		R.d = &root;
		which = 8;
		before = poolcalls;
		force_failure = 1;
		for(step = 0; step < 2; step++){
			first = step ? 1 : 2;
			last = step ? 10 : 14;
			if(pass){if(!kprotect(run, nil))abort();}
			else if(!jit->entry[ISLICEA](&R))abort();
			if(root != identity || root->root != H || poolcalls != before ||
			   root->len != (step ? 9 : 12) || root->data != base+(step ? 3 : 2)*type.size)abort();
		}
		force_failure = 0;
		copy = arraycpy(root);
		if(copy == root || copy->root != H || copy->len != 9 ||
		   (type.size && memcmp(copy->data, original+3*type.size, 9*type.size)))abort();
		destroy(copy);
		/* A held alias forces the original allocation/owner-transfer path. */
		view = root;
		D2H(root)->ref++;
		R.d = &view;
		first = 1;
		last = 9;
		if(jit->entry[ISLICEA](&R) || view != root)abort();
		if(!kprotect(run, nil) || view == root || view->root != root ||
		   view->len != 8 || view->data != base+4*type.size || root->len != 9 || D2H(root)->ref != 2)abort();
		copy = arraycpy(view);
		if(copy == view || copy->root != H || copy->len != 8 ||
		   (type.size && memcmp(copy->data, original+4*type.size, 8*type.size)))abort();
		destroy(copy);
		destroy(view);
		destroy(root);
		if(live || type.ref != 1 || Tarray.ref != 1)abort();
		total++;
	}
	printf("PASS root shrink/copy/share/release: %lu cases WORD%zu\n", total, sizeof(WORD)*8);
}


/* Compare actual C/RX slices, reuse, shared fallback and mapped backing lifetimes. */
int
main(void)
{
	Module compiled = {0};
	Inst instruction = {0};
	Type scalar = {1};
	Array *root, *view, *old, wanted, savedarray;
	Heap savedheap, savedroot;
	REG savedreg;
	uchar slots[128], savedslots[128];
	WORD first, last, value;
	int widths[] = {0, 1, sizeof(void*)};
	int wi, mode, alignment, alias, pass, i, baseline, calls, tref, aref;
	int valid, native, completed, held, fast;
	ulong total = 0;

	instruction.op = ISLICEA;
	compiled.prog = &instruction;
	compiled.nprog = 1;
	if(!compile(&compiled, 1, nil) || !compiled.jit || !compiled.jit->entry[ISLICEA])abort();
	scalar.size = sizeof(WORD);
	for(wi = 0; wi < 3; wi++)for(mode = 0; mode < 28; mode++)
	for(alignment = 0; alignment < 8; alignment++)for(alias = 0; alias < 3; alias++)
	for(pass = 0; pass < 2; pass++){
		if(((mode == 12 || mode == 18) && wi != 2) || ((mode == 13 || mode == 25) && wi != 0) || (mode == 14 && alias))continue;
		type.size = widths[wi];
		type.np = mode == 12 || mode == 18;
		type.map[0] = type.np ? 0x80 : 0;
		root = H2D(Array*, heaparray(&type, 16));
		if(mode == 12 || mode == 18){
			for(i = 0; i < 16; i++){
				void *child = H2D(void*, heapz(&scalar));
				value = i+1;
				memmove(child, &value, sizeof(value));
				memmove(root->data+i*sizeof(child), &child, sizeof(child));
			}
		}
		view = root;
		first = 2;
		last = 14;
		R.s = &first;
		R.m = &last;
		R.d = &view;
		which = 8;
		if(mode != 6 && mode < 18){
			/* These cases intentionally start with a view, not a unique root. */
			D2H(root)->ref++;
			if(!kprotect(run, nil))abort();
			destroy(root);
		}
		old = view;
		held = mode == 5 || mode == 11 || mode == 17 || mode == 19 || mode == 20;
		if(held)D2H(old)->ref = mode == 17 ? (ulong)~0 : 2;
		tref = type.ref;
		aref = Tarray.ref;
		if(mode == 7 || mode == 22)type.ref = INT_MAX;
		if(mode == 8)D2H(root)->ref = (ulong)~0;
		if(mode == 9 || mode == 23)Tarray.ref = INT_MAX;
		if(mode == 13 || mode == 25)old->data = root->data = nil;
		if(mode == 15 || mode == 24)type.ref = 0;
		if(mode == 16 || mode == 27)D2H(root)->ref = 0;
		if(mode == 26)old->data = H;
		first = mode == 1 ? 3 : mode == 2 || mode == 3 ? 0 : 2;
		last = mode == 1 ? 3 : mode == 2 ? old->len : mode == 3 ? 0 : 8;
		if(mode == 4)first = last = old->len;
		if(mode == 14){first = 13; last = 12;}
		memset(slots, 0xa5, sizeof(slots));
		memset(&R, 0, sizeof(R));
		R.s = slots+alignment;
		R.m = slots+32+alignment;
		R.d = slots+64+alignment;
		memmove(R.s, &first, sizeof(first));
		memmove(R.m, &last, sizeof(last));
		memmove(R.d, &old, sizeof(old));
		if(alias == 1){R.s = &old->len; last = old->len; memmove(R.m, &last, sizeof(last));}
		if(alias == 2)R.m = &old->len;
		memmove(&first, R.s, sizeof(first));
		memmove(&last, R.m, sizeof(last));
		memmove(savedslots, slots, sizeof(slots));
		memmove(&savedarray, old, sizeof(savedarray));
		memmove(&savedheap, D2H(old), sizeof(savedheap));
		memmove(&savedroot, D2H(root), sizeof(savedroot));
		memmove(&savedreg, &R, sizeof(R));
		baseline = live;
		calls = poolcalls;
		nprop = 7;
		force_failure = mode == 10 || mode == 11 || mode == 20 || mode == 21;
		last_error = nil;
		valid = mode != 11 && mode != 14 && mode != 15 && mode != 16 && mode != 20 && mode != 24 && mode != 27 && (mode != 26 || wi == 0);
		fast = valid && !held && (old != root || type.np == 0);
		native = fast && (type.size == 0 || last != 0);
		if(pass == 0){
			completed = compiled.jit->entry[ISLICEA](&R);
			if(completed != native)abort();
			if(!completed && (memcmp(&savedreg, &R, sizeof(R)) ||
			   memcmp(old, &savedarray, sizeof(savedarray)) || memcmp(slots, savedslots, sizeof(slots))))abort();
		}else
			completed = 0;
		if(!completed)completed = kprotect(run, nil);
		force_failure = 0;
		if(completed != valid || worker.nerr)abort();
		memmove(&view, R.d, sizeof(view));
		if(!valid){
			if(last_error != (mode == 14 || mode == 26 ? exBounds : exHeap) || view != old || live != baseline ||
			   memcmp(&R, &savedreg, sizeof(R)) || memcmp(old, &savedarray, sizeof(savedarray)) ||
			   memcmp(slots, savedslots, sizeof(slots)))abort();
		}else if(fast){
			memmove(&wanted, &savedarray, sizeof(wanted));
			wanted.len = last-first;
			wanted.data = type.size ? savedarray.data+first*type.size : savedarray.data;
			if(view != old || poolcalls != calls || live != baseline || nprop != 7 ||
			   memcmp(&wanted, old, sizeof(wanted)) || memcmp(&savedheap, D2H(old), sizeof(savedheap)) ||
			   (root != old && memcmp(&savedroot, D2H(root), sizeof(savedroot))) || memcmp(&R, &savedreg, sizeof(R)) ||
			   memcmp(slots, savedslots, sizeof(slots)))abort();
		}else if(view == old || view->root != root || view->len != last-first ||
		   view->data != savedarray.data+first*type.size || poolcalls != calls+1)abort();
		if(mode == 12 || mode == 18){
			for(i = 0; i < 16; i++){
				void *child;
				memmove(&child, root->data+i*sizeof(child), sizeof(child));
				if(D2H(child)->ref != 1)abort();
			}
		}
		if(mode == 7 || mode == 15 || mode == 22 || mode == 24)type.ref = tref;
		if(mode == 9 || mode == 23)Tarray.ref = aref;
		if(mode == 8 || mode == 16 || mode == 27)D2H(root)->ref = 1;
		if(held && old != root)D2H(old)->ref = 1;
		destroy(view);
		if(held && (view != old || old == root))destroy(old);
		if(live || type.ref != 1 || Tarray.ref != 1 || scalar.ref != 1)abort();
		total++;
	}
	rootcopies(compiled.jit);
	compiled.jit->freecode(compiled.jit->code, compiled.jit->size);
	free(compiled.jit);
	printf("PASS SLICEA C/RX: %lu reuse/shared/failure/mapped-owner/alias cases WORD%zu\n", total, sizeof(WORD)*8);
	return 0;
}
