

/* Check C exception identity or RX decline before any visible ownership effects. */
static void
mappedobserve(Module *module, int op, char *failure, int size, uchar *bytes,
              uchar *storage, uchar *expected, void *wanted)
{
	void (*handlers[])(void) = {movmp, headmp, movtmp};
	int ops[] = {IMOVMP, IHEADMP};
	uchar before[512], payload[1024];
	REG saved;
	int pass, result;

	memmove(before, bytes, sizeof(before));
	memmove(payload, storage, sizeof(payload));
	memmove(&saved, &R, sizeof(saved));
	for(pass = 0; pass < 2; pass++){
		if(pass == 0 && op == 2)
			continue;
		memmove(bytes, before, sizeof(before));
		memmove(storage, payload, sizeof(payload));
		memmove(&R, &saved, sizeof(saved));
		retained = released = 0;
		raised = nil;
		nprop = 1;
		feclearexcept(FE_ALL_EXCEPT);
		feraiseexcept(FE_DIVBYZERO);
		if(pass == 0){
			result = module->jit->entry[ops[op]](&R);
			mappedcheck(result == (failure == nil));
		}else{
			if(setjmp(boundary) == 0){
				handlers[op]();
				mappedcheck(failure == nil);
			}else
				mappedcheck(failure != nil && !strcmp(failure, raised));
		}
		if(failure != nil){
			mappedcheck(!memcmp(&R, &saved, sizeof(R)));
			mappedcheck(!memcmp(bytes, before, sizeof(before)));
			mappedcheck(!memcmp(storage, payload, sizeof(payload)));
			mappedcheck(retained == 0 && released == 0);
		}else{
			if(op == 1 && size != 0)
				saved.s = wanted;
			mappedcheck(!memcmp(&R, &saved, sizeof(R)));
			mappedcheck(!memcmp(bytes, before, sizeof(before)));
			mappedcheck(!memcmp(storage, expected, sizeof(payload)));
			mappedcheck(retained == (pass && size != 0));
			mappedcheck(released == (pass && op == 1 && size != 0));
			saved.s = op == 1 && size != 0 ? (void*)(bytes+8) : saved.s;
		}
		mappedcheck(nprop == 1 && fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
	}
}


/* Exercise malformed complete spans, lazy empty records and descriptor maps.
 * All owned images have fixed canaries; errors must precede refcount mutation.
 * Both WORD models use the real native-pointer compiler adapter and VM layouts.
 */
int
main(void)
{
	Module code = {0}, owner = {0};
	Modlink link = {0};
	Inst instructions[2] = {{0}};
	Type *table[2], *type;
	List *list, *head;
	Heap *heap;

	union {
		uvlong alignment; // Native alignment for borrowed operand and descriptor images.
		uchar bytes[512]; // Owned slots or full Type metadata and map canaries.
	} slots, descriptor;

	union {
		uvlong alignment; // Native Heap/List alignment.
		uchar bytes[1024]; // Source, destination and surrounding ownership canaries.
	} storage;
	uchar expected[1024];
	void *source, *destination, *wanted;
	char *failure, *selection;
	uintptr pointer;
	WORD index;
	int op, profile, shift, size, variant, i, which;
	unsigned long checks = 0;

	instructions[0].op = IMOVMP;
	instructions[1].op = IHEADMP;
	code.prog = instructions;
	code.nprog = 2;
	mappedcheck(compile(&code, 2, nil));
	selection = getenv("MAPPED_PREFLIGHT_PROFILE");
	for(op = 0; op < 3; op++)
	for(profile = 0; profile < 18; profile++)
	for(shift = 0; shift < 8; shift++)
	for(variant = 0; variant < 20; variant++){
		if(selection && atoi(selection) != profile)
			continue;
		if(getenv("MAPPED_PREFLIGHT_OP") && atoi(getenv("MAPPED_PREFLIGHT_OP")) != op)
			continue;
		memset(&R, 0, sizeof(R));
		memset(slots.bytes, 0xa5, sizeof(slots.bytes));
		memset(descriptor.bytes, 0, sizeof(descriptor.bytes));
		for(i = 0; i < sizeof(storage.bytes); i++)
			storage.bytes[i] = (i*37+11)&255;
		type = (Type*)descriptor.bytes;
		type->size = size = (variant&1) ? 256 : 8;
		type->np = 0;
		heap = (Heap*)storage.bytes;
		heap->ref = 2;
		list = H2D(List*, heap);
		head = list;
		owner.type = table;
		owner.ntype = 2;
		link.m = &owner;
		table[0] = table[1] = type;
		index = 1;
		R.M = &link;
		R.PC = instructions;
		R.s = op == 1 ? (void*)(slots.bytes+8) : storage.bytes+256+shift;
		R.m = slots.bytes+128+shift;
		R.d = storage.bytes+640+shift;
		failure = nil;
		pointer = variant == 0 ? 0 : variant == 1 ? (uintptr)-1 : (uintptr)-1-(variant-1);
		which = variant%2;
		if(profile == 0){
			/* Index/type slots must fit before any metadata is read. */
			if(variant >= (op == 2 ? sizeof(type) : sizeof(index))+1)
				continue;
			R.m = (void*)pointer;
			failure = exBounds;
		}
		if(profile == 1){
			if(op != 1 || variant >= sizeof(head)+1)
				continue;
			R.s = (void*)pointer;
			failure = exHeap;
		}
		if(profile == 2){
			if(variant > 3)
				continue;
			if(which)
				R.d = variant == 1 ? nil : H;
			else if(op == 1)
				head = variant == 0 ? nil : H;
			else
				R.s = variant == 0 ? nil : H;
			failure = exNilref;
		}
		if(profile == 3){
			/* Even a nonwrapping base cannot justify a wrapping complete copy. */
			pointer = (uintptr)-1-(variant%8);
			if(which)
				R.d = (void*)pointer;
			else if(op == 1)
				head = (List*)(pointer - offsetof(List, data));
			else
				R.s = (void*)pointer;
			failure = pointer == (uintptr)-1 && (which || op != 1) ? exNilref : exBounds;
		}
		if(profile == 4){ type->np = -1; failure = exHeap; }
		if(profile == 5){ type->np = 0x7fffffff; failure = exHeap; }
		if(profile == 6){
			type->size = size = 1+variant%8;
			type->np = 1;
			type->map[0] = size < 8 ? 0x80 : 0x40;
			failure = exHeap;
		}
		if(profile == 7){
			if(shift == 0)
				continue;
			type = (Type*)(descriptor.bytes+shift);
			memmove((uchar*)type+offsetof(Type, size), &size, sizeof(size));
			table[0] = table[1] = type;
			failure = exHeap;
		}
		if(profile == 8){
			if(op != 1)
				continue;
			head = (List*)(uintptr)(variant%5 == 0 ? 1 : (variant%5)*8);
			failure = exHeap;
		}
		if(profile == 9){
			/* A zero size skips invalid maps, arbitrary data and heap identities. */
			type->size = size = 0;
			type->np = -1;
			R.d = (void*)pointer;
			if(op == 1)
				head = (List*)(pointer == 0 || pointer == (uintptr)-1 ? 1 : pointer);
			else
				R.s = (void*)pointer;
		}
		if(profile == 10){
			if(op == 2)
				continue;
			R.M = variant&1 ? nil : H;
			failure = exModule;
		}
		if(profile == 11){
			if(op == 2)
				continue;
			owner.type = (Type**)((uintptr)-1-(variant%8));
			failure = "invalid type";
		}
		if(profile == 12){
			if(op != 2)
				continue;
			type = variant&1 ? nil : H;
			failure = exType;
		}
		if(profile == 13){
			/* Valid overlap checks the full staged record after capturing metadata. */
			R.d = op == 1 ? (uchar*)list+offsetof(List, data)+(variant%17)-8 : (uchar*)R.s+(variant%17)-8;
		}
		if(profile == 14){
			if(shift == 0)
				continue;
			type = (Type*)(descriptor.bytes+shift);
			size = 0;
			memmove((uchar*)type+offsetof(Type, size), &size, sizeof(size));
			table[0] = table[1] = type;
			R.d = (void*)pointer;
			if(op == 1)
				head = (List*)1;
			else
				R.s = (void*)pointer;
		}
		if(profile == 15){
			type = (Type*)pointer;
			table[0] = table[1] = type;
			failure = op == 2 ? exType : "invalid type";
		}
		if(profile == 16 || profile == 17){
			if(op == 2)
				continue;
			if(profile == 16 && variant >= offsetof(Modlink, m)+sizeof(link.m)+1)
				continue;
			if(profile == 16)
				R.M = (Modlink*)pointer;
			else
				link.m = (Module*)pointer;
			failure = exModule;
		}
		if(op == 1 && profile != 1)
			memmove(R.s, &head, sizeof(head));
		if(profile != 0){
			if(op == 2)
				memmove(R.m, &type, sizeof(type));
			else
				memmove(R.m, &index, sizeof(index));
		}
		source = op == 1 ? (void*)((uintptr)head+offsetof(List, data)) : R.s;
		destination = R.d;
		wanted = op == 1 && size ? source : R.s;
		memmove(expected, storage.bytes, sizeof(expected));
		if(failure == nil && size != 0)
			memmove(expected+((uchar*)destination-storage.bytes), source, size);
		if(getenv("MAPPED_PREFLIGHT_TRACE"))
			fprintf(stderr, "case op=%d profile=%d shift=%d variant=%d\n", op, profile, shift, variant);
		mappedobserve(&code, op, failure, size, slots.bytes, storage.bytes, expected, wanted);
		checks++;
	}
	code.jit->freecode(code.jit->code, code.jit->size);
	free(code.jit);
	printf("PASS mapped preflight C/RX WORD%zu: %lu cases\n", sizeof(WORD)*8, checks);
	return 0;
}
