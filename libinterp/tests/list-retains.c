/* Reject descriptor and tail failures before any allocation or publication. */
int
main(void)
{
	Module owner = {0};
	Modlink module = {0};
	Type *types[1], *selected;
	List *tail, *result;
	WORD index = 0;
	void *source = H;
	Type oldlist, oldptr, oldrecord;
	REG saved;
	int op, mode, before, ok, valid, refs[] = {0, -1, 0x7fffffff};
	ulong total = 0;

	owner.type = types; owner.ntype = 1; module.m = &owner;
	for(op = 0; op < 4; op++)
	for(mode = 0; mode < 9; mode++){
		Tlist.ref = Tptr.ref = record.ref = 1;
		Tlist.size = record.size = 0; Tlist.np = record.np = 0;
		tail = H;
		cons(0, &tail);
		result = tail;
		selected = op == 0 ? &Tlist : op == 1 ? &Tptr : op == 2 ? &record : &Tlist;
		types[0] = selected;
		if(mode < 3)selected->ref = refs[mode];
		if(mode == 3)Tlist.ref = 0;
		if(mode == 4)Tlist.ref = 0x7fffffff;
		if(mode == 5)selected->ref = 0x7ffffffe;
		if(mode == 6)selected->ref = 0x7ffffffd;
		if(mode == 7)D2H(tail)->ref = 0;
		if(mode == 8)selected->ref = 0x7ffffffe;
		R.M = &module; R.s = &source; R.m = &index; R.d = &result;
		operation = op == 0 ? 16 : op == 1 ? 3 : 6;
		memmove(&oldlist, &Tlist, sizeof(Type)); memmove(&oldptr, &Tptr, sizeof(Type));
		memmove(&oldrecord, &record, sizeof(Type)); memmove(&saved, &R, sizeof(R));
		before = allocations; raised = nil;
		valid = mode == 6 || mode == 8 || (mode == 5 && op != 3);
		if(mode == 8 && op == 3)valid = 0;
		ok = kprotect(run, nil);
		if(ok != valid || worker.nerr != 0)abort();
		if(!ok){
			if(raised != exHeap || allocations != before || result != tail || live != 1 ||
			   memcmp(&oldlist, &Tlist, sizeof(Type)) || memcmp(&oldptr, &Tptr, sizeof(Type)) ||
			   memcmp(&oldrecord, &record, sizeof(Type)) || memcmp(&saved, &R, sizeof(R)))abort();
		}else{
			if(result == tail || allocations != before+1 || result->tail != tail ||
			   selected->ref != (op == 3 ? oldlist.ref+2 : op == 0 ? oldlist.ref+1 : op == 1 ? oldptr.ref+1 : oldrecord.ref+1))abort();
		}
		/* Restore independent fixture ownership before testing recursive destruction. */
		D2H(tail)->ref = 1;
		Tlist.ref = ok ? 3 : 2;
		Tptr.ref = 1+(ok && op == 1);
		record.ref = 1+(ok && op == 2);
		if(ok && op == 3)Tlist.ref++;
		destroy(result);
		if(live || Tlist.ref != 1 || Tptr.ref != 1 || record.ref != 1)abort();
		total++;
	}
	printf("PASS list retains: %lu descriptor/tail/alias/publication cases WORD%zu\n", total, sizeof(WORD)*8);
	return 0;
}
