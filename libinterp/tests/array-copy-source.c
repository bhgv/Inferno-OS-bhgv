

/* Reject malformed source owners/spans before allocating or changing either owner. */
int
main(void)
{
	static Array *owner, *backing;
	static Array before;
	static Heap beforeheap, beforeroot;
	static Type beforetype;
	uchar *data;
	int mode, ok, valid, expectederror;
	char *selection = getenv("ARRAY_COPY_MODE");
	ulong total = 0;

	for(mode = 0; mode < 17; mode++){
		if(selection && mode != atoi(selection))continue;
		setup();
		owner = H2D(Array*, heaparray(&leaf, 3));
		backing = H2D(Array*, heaparray(&leaf, 3));
		memset(owner->data, 0x6b, 3*leaf.size);
		memset(backing->data, 0x6b, 3*leaf.size);
		data = owner->data;
		source = owner;
		result = H;
		operation = 0;
		if(mode == 0)source = nil;
		if(mode == 1)D2H(owner)->ref = 0;
		if(mode == 2)owner->root = nil;
		if(mode == 3){owner->root = backing; D2H(backing)->ref = 0;}
		if(mode == 4)owner->data = nil;
		if(mode == 5)owner->data = H;
		if(mode == 6)owner->data = (uchar*)(uintptr)-4;
		if(mode == 7){owner->len = 0; owner->data = nil;}
		if(mode == 8){leaf.size = 0; owner->data = H;}
		if(mode == 9)D2H(owner)->ref = (ulong)~0;
		if(mode == 10){owner->root = backing; owner->data = backing->data; D2H(backing)->ref = (ulong)~0;}
		if(mode == 11)owner->t = nil;
		if(mode == 12)owner->t = H;
		if(mode == 13)owner->len = -1;
		if(mode == 14)owner->len = (WORD)(((UWORD)~0)>>1);
		if(mode == 15){owner->len = 0; owner->data = H;}
		if(mode == 16){leaf.size = 0; owner->data = nil;}
		memmove(&before, owner, sizeof(before));
		memmove(&beforeheap, D2H(owner), sizeof(beforeheap));
		memmove(&beforeroot, D2H(backing), sizeof(beforeroot));
		memmove(&beforetype, &leaf, sizeof(beforetype));
		allocations = 0;
		raised = nil;
		valid = mode == 7 || mode == 8 || mode == 9 || mode == 10 || mode == 15 || mode == 16;
		expectederror = mode == 13;
		ok = kprotect(run, nil);
		if(ok != valid || worker.nerr || memcmp(&before, owner, sizeof(before)) ||
		   memcmp(&beforeheap, D2H(owner), sizeof(beforeheap)) ||
		   memcmp(&beforeroot, D2H(backing), sizeof(beforeroot)))abort();
		if(!valid){
			if(raised != (expectederror ? exNegsize : exHeap) || allocations || live != 2 ||
			   result != H || memcmp(&beforetype, &leaf, sizeof(leaf)))abort();
		}else{
			if(allocations != 1 || live != 3 || result == H || result == owner ||
			   result->root != H || result->len != owner->len || result->t != &leaf ||
			   leaf.ref != beforetype.ref+1)abort();
			if(leaf.size && result->len && memcmp(result->data, owner->data, result->len*leaf.size))abort();
			destroy(result);
			result = H;
			if(leaf.ref != beforetype.ref)abort();
		}
		/* Restore deliberate corruptions/saturated counters before fixture teardown. */
		owner->t = &leaf;
		owner->root = H;
		owner->data = data;
		owner->len = 3;
		D2H(owner)->ref = D2H(backing)->ref = 1;
		leaf.size = sizeof(WORD);
		destroy(owner);
		destroy(backing);
		source = H;
		if(live || leaf.ref != 1 || Tarray.ref != 1)abort();
		total++;
	}
	if(arraycpy(H) != H)abort();
	printf("PASS array copy sources: %lu lifetime/span/no-allocation/empty cases WORD%zu\n", total, sizeof(WORD)*8);
	return 0;
}
