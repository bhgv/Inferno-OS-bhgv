/* Require impossible byte-array requests to fail before either descriptor is retained. */
int
main(void)
{
	Array *array;
	int refs[] = {0, -1, 0x7fffffff, 0x7ffffffe};
	int selected, mode, size, before, ok;
	Type savedarray, savedbyte;
	ulong total = 0;

	for(selected = 0; selected < 2; selected++)
	for(mode = 0; mode < 5; mode++){
		Tarray.ref = Tbyte.ref = 1;
		if(mode < 4){
			if(selected)Tbyte.ref = refs[mode];
			else Tarray.ref = refs[mode];
		}
		size = mode == 4 ? 0x7fffffff : 4;
		memmove(&savedarray, &Tarray, sizeof(Type)); memmove(&savedbyte, &Tbyte, sizeof(Type));
		before = calls; raised = nil; array = H; ok = 0;
		if(setjmp(boundary) == 0){array = mem2array("data", size); ok = 1;}
		if(ok != (mode == 3))abort();
		if(!ok){
			if(raised != exHeap || array != H || live || calls != before ||
			   memcmp(&savedarray, &Tarray, sizeof(Type)) || memcmp(&savedbyte, &Tbyte, sizeof(Type)))abort();
		}else{
			if(calls != before+1 || array->len != 4 || memcmp(array->data, "data", 4) ||
			   Tarray.ref != savedarray.ref+1 || Tbyte.ref != savedbyte.ref+1)abort();
			destroy(array);
			if(Tarray.ref != savedarray.ref || Tbyte.ref != savedbyte.ref)abort();
		}
		total++;
	}
	Tarray.ref = Tbyte.ref = 1;
	/* Preserve the historical negative-length-to-empty convention. */
	array = mem2array(nil, -1);
	if(array->len != 0)abort();
	destroy(array);
	if(live || Tarray.ref != 1 || Tbyte.ref != 1)abort();
	printf("PASS byte-array retains: %lu descriptor/size/allocation cases WORD%zu\n", total, sizeof(WORD)*8);
	return 0;
}
