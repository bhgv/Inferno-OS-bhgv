static Type *selectedtype;
static int allocatearray;


/* Invoke malformed descriptor cases without publishing an owned destination. */
static int
copyselected(void *unused)
{
	(void)unused;
	if(allocatearray)
		heaparray(selectedtype, 1);
	else
		newmp(mp_dest, mp_source, selectedtype);
	return 1;
}


/* Reject invalid element maps before allocating a header with fallible cleanup. */
static void
arraymaps(void)
{
	int mode;

	for(mode = 0; mode < 6; mode++){
		setup();
		selectedtype = &mp;
		mp.size = 2*sizeof(void*);
		mp.np = 1;
		mp.map[0] = 0xc0;
		if(mode == 0)mp.np = -1;
		if(mode == 1)mp.np = 2;
		if(mode == 2)mp.map[0] = 0x01;
		if(mode == 3)mp.size = sizeof(void*)+1;
		if(mode == 4)selectedtype = nil;
		if(mode == 5)selectedtype = H;
		raised = nil;
		allocatearray = 1;
		if(kprotect(copyselected, nil) || raised != exHeap || allocations || live ||
		   worker.nerr || Tarray.ref != 1 || mp.ref != 1)abort();
		allocatearray = 0;
	}
}


/* Exercise mapped-owner failure after an earlier field has already been retained. */
int
main(void)
{
	uchar input[64], output[64], before[64], expected[64], savedinput[64];
	Heap *owner;
	void *value, *second, *field;
	Type *descriptor;
	ulong refs, total;
	int mode, sa, da, ok, valid, early, acquired;

	total = 0;
	for(mode = 0; mode < 20; mode++)
	for(sa = 0; sa < 8; sa++)
	for(da = 0; da < 8; da++){
		setup();
		owner = heapz(&atom);
		value = H2D(void*, owner);
		refs = mode == 0 ? 0 : mode == 1 ? (ulong)~0 :
			mode == 2 || mode == 3 ? (ulong)~0-1 : 1;
		owner->ref = refs;
		second = mode == 3 ? value : H;
		mp.size = 2*sizeof(void*);
		mp.np = 1;
		mp.map[0] = 0xc0;
		descriptor = &mp;
		if(mode == 4)value = nil;
		if(mode == 5)owner->t = nil;
		if(mode == 6)owner->t = H;
		if(mode == 7)descriptor = nil;
		if(mode == 8)descriptor = H;
		if(mode == 9)mp.size = -1;
		if(mode == 10)mp.np = -1;
		if(mode == 11)mp.np = 2;
		if(mode == 12)mp.size = sizeof(void*)+1;
		if(mode == 13)second = nil;
		if(mode == 14){mp.map[0] = 0; owner->ref = 0;}
		memset(input, 0x5a, sizeof(input));
		memset(output, 0xa5, sizeof(output));
		memmove(input+sa, &value, sizeof(value));
		memmove(input+sa+sizeof(value), &second, sizeof(second));
		memmove(before, output, sizeof(output));
		memmove(savedinput, input, sizeof(input));
		mp_source = input+sa;
		mp_dest = output+da;
		if(mode == 16)mp_source = nil;
		if(mode == 17)mp_source = H;
		if(mode == 18)mp_dest = nil;
		if(mode == 19)mp_dest = H;
		operation = 1;
		raised = nil;
		/* The generic fixture uses &mp; select descriptor failures explicitly below. */
		if(mode == 7 || mode == 8){
			/* Reuse a protected callback with a selected descriptor. */
			selectedtype = descriptor;
			ok = kprotect(copyselected, nil);
		}else
			ok = kprotect(run, nil);
		valid = mode == 2 || mode == 14 || mode == 15;
		early = (mode >= 7 && mode <= 12) || mode >= 16;
		acquired = mode == 2 || mode == 3 || mode == 13 || mode == 15;
		if(ok != valid || worker.nerr || (!ok && raised != exHeap))abort();
		memmove(expected, before, sizeof(expected));
		if(!early){
			memmove(expected+da, input+sa, mp.size);
			if(mp.map[0]){
				field = acquired ? value : H;
				memmove(expected+da, &field, sizeof(field));
				field = H;
				memmove(expected+da+sizeof(field), &field, sizeof(field));
			}
		}
		if(memcmp(expected, output, sizeof(output)) ||
		   memcmp(savedinput, input, sizeof(input)))abort();
		if(early){
			if(memcmp(output, before, sizeof(output)))abort();
		}else{
			memmove(&field, output+da, sizeof(field));
			if(mp.map[0] && field != (acquired ? value : H))abort();
			memmove(&field, output+da+sizeof(field), sizeof(field));
			if(mp.map[0] && field != H)abort();
			if(owner->ref != (mode == 14 ? 0 : refs+acquired))abort();
			if(mp.map[0])freeptrs(output+da, &mp);
			if(owner->ref != (mode == 14 ? 0 : refs))abort();
		}
		owner->ref = 1;
		owner->t = &atom;
		destroy(H2D(void*, owner));
		if(live || atom.ref != 1)abort();
		total++;
	}
	arraymaps();
	printf("PASS array map preflight: 6 no-allocation cases\n");
	printf("PASS mapped clone owners: %lu lifetime/map/alignment/partial-cleanup cases WORD%zu\n",
		total, sizeof(WORD)*8);
	return 0;
}
