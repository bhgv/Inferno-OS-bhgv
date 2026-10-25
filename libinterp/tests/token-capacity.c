

/* Invalid source and delimiter extents must leave the complete result untouched. */
int
main(void)
{
	Rune text[] = {'a', ' ', 'b'}, delimiter[] = {' '};
	F_Sys_tokenize frame = {0};
	uchar slots[64], saved[64];
	int wide, mode, alignment, baseline, oldlen, oldmax, ok;
	String *changed;
	ulong total;

	total = 0;
	for(wide = 0; wide < 2; wide++)
	for(mode = 0; mode < 8; mode++)
	for(alignment = 0; alignment < 8; alignment++){
		frame.s = string(text, 3, wide);
		frame.delim = string(delimiter, 1, wide);
		changed = mode < 4 ? frame.s : frame.delim;
		oldlen = changed->len;
		oldmax = changed->max;
		if(mode%4 == 0)changed->max = -1;
		if(mode%4 == 1)changed->max = 0;
		if(mode%4 == 2)changed->len = (-0x7fffffff-1);
		if(mode%4 == 3)changed->len = wide ? -0x7fffffff : 0x7fffffff;
		memset(slots, 0xa5, sizeof(slots));
		memmove(saved, slots, sizeof(slots));
		frame.ret = (void*)(slots+alignment);
		baseline = live;
		allocations = 0;
		raised = nil;
		ok = kprotect(invoke, &frame);
		if(ok || raised != exInval || allocations || live != baseline || worker.nerr ||
		   memcmp(slots, saved, sizeof(slots)))abort();
		changed->len = oldlen;
		changed->max = oldmax;
		destroy(frame.s);
		destroy(frame.delim);
		if(live || Tstring.ref != 1 || Tptr.ref != 1)abort();
		total++;
	}
	printf("PASS token capacity: %lu source/delimiter/result-alignment cases WORD%zu\n",
		total, sizeof(WORD)*8);
	return 0;
}
