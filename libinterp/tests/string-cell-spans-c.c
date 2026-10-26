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


/* Compare narrowed production address checks with independent mathematical extents. */
int
main(void)
{
	uchar *allocation, *base, before[4096], expected[4096];
	String *owner, *slot, *result;
	uintptr aligned;
	uvlong dataoffset, begin, bytes;
	WORD index, end, value, scalar, wanted;
	REG saved;
	Rune rune;
	char *exception;
	int offsets[] = {0xfff8, 0xfff0, 0xffe8, 0xffe0, 0xffc0, 0xff00};
	int lengths[] = {1, 7, 128, 512, 0x7fffffff};
	int indexes[] = {-1, 0, 1, 7, 127, 128, 255, 256, 512, 0x7ffffffe, 0x7fffffff};
	int counts[] = {1, 128, 256, 257};
	int op, wide, offset, li, ii, ci, length, stride, copied, allocations;
	int reused, count = 0;

	allocation = malloc(0x40000);
	if(allocation == nil)
		abort();
	aligned = ((uintptr)allocation+0xffff)&~(uintptr)0xffff;
	base = (uchar*)aligned;
	dataoffset = offsetof(String, data);
	for(op = 0; op < 5; op++)
	for(wide = 0; wide < 2; wide++)
	for(offset = 0; offset < nelem(offsets); offset++)
	for(li = 0; li < nelem(lengths); li++)
	for(ii = 0; ii < nelem(indexes); ii++)
	for(ci = 0; ci < (op == 2 || op == 3 ? nelem(counts) : 1); ci++){
		if(getenv("CELL_MODE") && op != atoi(getenv("CELL_MODE")))
			continue;
		owner = (String*)(base+offsets[offset]);
		memset(D2H(owner), 0, sizeof(Heap)+4096);
		D2H(owner)->ref = op == 4 ? 2 : 1;
		D2H(owner)->t = &Tstring;
		length = lengths[li];
		stride = wide ? sizeof(Rune) : 1;
		owner->len = wide ? -length : length;
		owner->max = 0x7fffffff;
		memset(owner->Sascii, 0xa5, 3072);
		memmove(before, owner, sizeof(before));
		memmove(expected, before, sizeof(expected));
		index = indexes[ii];
		end = (WORD)((vlong)index+counts[ci]);
		value = 0x41;
		scalar = 0x13579;
		wanted = scalar;
		slot = owner;
		result = H;
		exception = nil;
		reused = 0;
		begin = offsets[offset]+dataoffset;
		copied = counts[ci];
		if((op == 3 || (op == 2 && index >= 0 && (uvlong)index+copied <= 0x7fffffff)) &&
		   offsets[offset]+offsetof(String, max)+sizeof(owner->max) > 0xffff)
			exception = exHeap;
		else if((op == 0 || op == 1 || op == 4) && begin > 0xffff)
			exception = exHeap;
		else if(op == 0 || op == 1 || op == 4){
			if(index < 0 || index > length || (op == 0 && index == length))
				exception = exBounds;
			else if(op == 0 || op == 1){
				bytes = ((uvlong)index+1)*stride;
				if(begin+bytes > 0xffff || (op == 1 && index == 0x7fffffff))
					exception = exHeap;
				else if(op == 0){
					if(wide){
						memmove(&rune, before+dataoffset+index*stride, sizeof(rune));
						wanted = (int)rune;
					}else
						wanted = before[dataoffset+index];
				}else{
					reused = 1;
					rune = value;
					memmove(expected+dataoffset+index*stride, &rune, stride);
					if(index == length)
						((String*)expected)->len = wide ? -(length+1) : length+1;
				}
			}else if(begin+(uvlong)length*stride > 0xffff || index == 0x7fffffff)
				exception = exHeap;
		}else{
			if(index < 0 || (vlong)index+copied > 0x7fffffff || end > length)
				exception = exBounds;
			else if((op == 3 || index != 0) && begin+(uvlong)end*stride > 0xffff)
				exception = exHeap;
			else if(op == 2){
				reused = 1;
				if(index != 0)
					memmove(expected+dataoffset, before+dataoffset+index*stride, copied*stride);
				((String*)expected)->len = wide ? -copied : copied;
			}
		}
		R.s = op == 0 ? (void*)&owner : op == 2 ? (void*)&index : (void*)&value;
		R.m = op == 2 ? &end : &index;
		R.d = op == 0 ? (void*)&scalar : (void*)&slot;
		saved = R;
		allocations = calls;
		raised = nil;
		if(setjmp(boundary) == 0){
			if(op == 0)
				indc();
			else if(op == 1 || op == 4)
				insc();
			else if(op == 2)
				slicec();
			else
				result = slicer(index, end, owner);
			if(exception != nil)
				abort();
		}else if(exception == nil || raised != exception)
			abort();
		if(exception != nil){
			if(calls != allocations || slot != owner || scalar != 0x13579 ||
			   memcmp(owner, before, sizeof(before)) || D2H(owner)->ref != (op == 4 ? 2 : 1))
				abort();
		}else{
			if(op == 0){
				if(scalar != wanted || calls != allocations || memcmp(owner, before, sizeof(before)))
					abort();
			}else if(reused){
				if(slot != owner || calls != allocations || memcmp(owner, expected, sizeof(expected)))
					abort();
			}else{
				if(op == 4)
					result = slot;
				if(result == H || result == nil || result == owner || calls != allocations+1 ||
				   memcmp(owner, before, sizeof(before)) || D2H(owner)->ref != 1)
					abort();
				if(op == 3){
					if(result->len != (wide ? -copied : copied) ||
					   memcmp(result->Sascii, before+dataoffset+index*stride, copied*stride))
						abort();
				}else{
					if(result->len != (wide ? -(length+(index == length)) : length+(index == length)))
						abort();
					rune = value;
					if(memcmp(result->Sascii+index*stride, &rune, stride))
						abort();
				}
				destroy(result);
			}
		}
		if(memcmp(&R, &saved, sizeof(R)) || live || Tstring.ref != 1)
			abort();
		count++;
	}
	free(allocation);
	printf("PASS string cell C spans: %d extent/image/ownership/preparation cases WORD%zu\n", count, sizeof(WORD)*8);
	return 0;
}
