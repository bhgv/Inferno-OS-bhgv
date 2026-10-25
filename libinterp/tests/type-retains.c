static Type *retainedtype;
static int retainoperation;


/* Exercise allocation entry points through the real VM error protector. */
static int
retainrun(void *unused)
{
	(void)unused;
	switch(retainoperation){
	case 2: allocated = heap(retainedtype); break;
	case 3: allocated = heapz(retainedtype); break;
	case 4: allocated = heaparray(retainedtype, 1); break;
	default: return run(nil);
	}
	return 1;
}


/* Invalid descriptors must neither allocate nor alter operand, owner or Type images. */
int
main(void)
{
	Type *types[1], *selected;
	Module owner = {0};
	Modlink link = {0};
	Array *array, *original;
	WORD count = 1, index = 0, start = 0, end = 0;
	Type beforetype, beforearray;
	REG saved;
	uchar object[sizeof(Heap)+sizeof(Array)];
	int operations[] = {2, 3, 4, 8, 10, 11}; // heap, heapz, heaparray, slicea, newa and newaz.
	int oi, mode, ok, expected, callsbefore, livebefore, originalref, arrayref, alias;
	unsigned long total = 0;

	owner.type = types; owner.ntype = 1;
	link.m = &owner; link.type = types;
	for(oi = 0; oi < nelem(operations); oi++)
	for(mode = 0; mode < 8; mode++){
		retainoperation = which = operations[oi];
		if(getenv("TYPE_RETAIN_MODE") && atoi(getenv("TYPE_RETAIN_MODE")) != mode)continue;
		if(getenv("TYPE_RETAIN_OPERATION") && atoi(getenv("TYPE_RETAIN_OPERATION")) != retainoperation)continue;
		if(retainoperation < 4 && mode >= 3 && mode <= 5)continue;
		alias = retainoperation >= 4 && (mode == 5 || mode == 6);
		type.ref = Tarray.ref = 1;
		type.size = sizeof(WORD); type.np = 0;
		selected = alias ? &Tarray : &type;
		types[0] = retainedtype = selected;
		array = H;
		if(retainoperation == 8)
			array = H2D(Array*, heaparray(selected, 1));
		original = array;
		originalref = selected->ref; arrayref = Tarray.ref;
		if(mode == 0)selected->ref = 0;
		if(mode == 1)selected->ref = -1;
		if(mode == 2)selected->ref = INT_MAX;
		if(mode == 3)Tarray.ref = 0;
		if(mode == 4)Tarray.ref = INT_MAX;
		if(mode == 5)selected->ref = INT_MAX-1;
		if(mode == 6)selected->ref = INT_MAX-2;
		if(mode == 7)selected->ref = INT_MAX-1;
		R.M = &link; R.d = &array;
		R.s = retainoperation == 8 ? (void*)&start : (void*)&count;
		R.m = retainoperation == 8 ? (void*)&end : (void*)&index;
		memmove(&beforetype, selected, sizeof(beforetype));
		memmove(&beforearray, &Tarray, sizeof(beforearray));
		memmove(&saved, &R, sizeof(saved));
		if(original != H)memmove(object, D2H(original), sizeof(object));
		callsbefore = poolcalls; livebefore = live;
		allocated = nil; last_error = nil;
		expected = mode >= 6;
		ok = kprotect(retainrun, nil);
		if(ok != expected || worker.nerr != 0)abort();
		if(!ok){
			if(last_error != exHeap || poolcalls != callsbefore || live != livebefore || array != original ||
			   memcmp(selected, &beforetype, sizeof(Type)) || memcmp(&Tarray, &beforearray, sizeof(Type)) ||
			   memcmp(&R, &saved, sizeof(R)))
				abort();
			if(original != H && memcmp(object, D2H(original), sizeof(object)))abort();
		}else if(selected->ref != beforetype.ref+(alias ? 2 : 1))
			abort();
		/* Restore ordinary counts before invoking the independent fixture destructor. */
		selected->ref = originalref;
		Tarray.ref = arrayref;
		if(ok){
			if(retainoperation == 2 || retainoperation == 3){
				free(allocated); live--;
			}else{
				selected->ref++;
				Tarray.ref++;
				destroy(retainoperation == 4 ? H2D(void*, allocated) : array);
				original = H;
			}
		}
		if(original != H)destroy(original);
		if(live || type.ref != 1 || Tarray.ref != 1)abort();
		total++;
	}
	for(retainoperation = 2; retainoperation <= 4; retainoperation++)
	for(mode = 0; mode < 2; mode++){
		if(getenv("TYPE_RETAIN_OPERATION") && atoi(getenv("TYPE_RETAIN_OPERATION")) != retainoperation)continue;
		retainedtype = mode ? H : nil;
		callsbefore = poolcalls; livebefore = live; last_error = nil;
		if(kprotect(retainrun, nil) || last_error != exHeap || callsbefore != poolcalls || livebefore != live)
			abort();
		total++;
	}
	printf("PASS Type retain preflight: %lu constructor/error/boundary cases WORD%zu\n", total, sizeof(WORD)*8);
	return 0;
}
