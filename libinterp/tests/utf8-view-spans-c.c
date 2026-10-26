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


/* Compare real view preparation, termination and Rune-cache lifetime with bounded extents. */
int
main(void)
{
	uchar *allocation, *base, before[4096], expected[4096];
	String *owner, *input;
	char *cache, *view, *why;
	uintptr aligned;
	uvlong begin, bytes;
	int offsets[] = {0xfff8, 0xfff0, 0xffe8, 0xffe0, 0xffc0, 0xff00};
	int lengths[] = {0, 1, 7, 31, 127, 512, 0x7fffffff};
	int wide, offset, li, mode, length, i, allocations, total = 0;

	allocation = malloc(0x40000);
	if(allocation == nil)
		abort();
	aligned = ((uintptr)allocation+0xffff)&~(uintptr)0xffff;
	base = (uchar*)aligned;
	for(wide = 0; wide < 2; wide++)
	for(offset = 0; offset < nelem(offsets); offset++)
	for(li = 0; li < nelem(lengths); li++)
	for(mode = 0; mode < 5; mode++){
		if(getenv("UTF8_VIEW_WIDE") && (wide != atoi(getenv("UTF8_VIEW_WIDE")) ||
		   (wide && lengths[li] == 0)))
			continue;
		owner = (String*)(base+offsets[offset]);
		memset(D2H(owner), 0, sizeof(Heap)+4096);
		D2H(owner)->ref = 1;
		D2H(owner)->t = &Tstring;
		length = lengths[li];
		owner->len = wide ? -length : length;
		owner->max = length;
		if(wide){
			for(i = 0; i < 768; i++)
				owner->Srune[i] = 0x20ac;
		}else
			memset(owner->Sascii, 'a', 3072);
		cache = malloc(8);
		if(cache == nil)
			abort();
		memmove(cache, "oldview", 8);
		owner->tmp = cache;
		input = mode == 1 ? nil : mode == 2 ? H : owner;
		if(mode == 3)
			owner->max = length-1;
		memmove(before, owner, sizeof(before));
		memmove(expected, before, sizeof(expected));
		begin = offsets[offset]+offsetof(String, data);
		bytes = owner->len < 0 ? (uvlong)length*sizeof(Rune) : (uvlong)length+1;
		why = nil;
		if(input == nil)
			why = exNilref;
		else if(input != H){
			if(begin > 0xffff)
				why = exHeap;
			else if(owner->len < -(0x7fffffff-UTFmax)/UTFmax)
				why = exNomem;
			else if(owner->max < length || begin+bytes > 0xffff)
				why = exHeap;
			else if(owner->len < 0 && mode == 4)
				why = exNomem;
		}
		cachefailure = mode == 4;
		cachecalls = 0;
		allocations = calls;
		raised = nil;
		view = nil;
		if(setjmp(boundary) == 0){
			view = string2c(input);
			if(why != nil)
				abort();
		}else if(why == nil || raised != why)
			abort();
		cachefailure = 0;
		if(why != nil){
			if(memcmp(owner, before, sizeof(before)) || owner->tmp != cache ||
			   memcmp(cache, "oldview", 8) ||
			   cachecalls != (why == exNomem && owner->len < 0 &&
			       length <= (0x7fffffff-UTFmax)/UTFmax ? 1 : 0))
				abort();
		}else if(input == H){
			if(view == nil || strcmp(view, "") || cachecalls ||
			   memcmp(owner, before, sizeof(before)))
				abort();
		}else if(owner->len >= 0){
			expected[offsetof(String, data)+length] = 0;
			if(view != owner->Sascii || cachecalls || memcmp(owner, expected, sizeof(expected)) ||
			   owner->tmp != cache || memcmp(cache, "oldview", 8))
				abort();
		}else{
			((String*)expected)->tmp = owner->tmp;
			if(view != owner->tmp || view == cache || cachecalls != 1 ||
			   memcmp(owner, expected, sizeof(expected)))
				abort();
			for(i = 0; i < length; i++)
				if(memcmp(view+i*3, "\xe2\x82\xac", 3))
					abort();
			if(view[length*3] != 0)
				abort();
		}
		if(calls != allocations || live || Tstring.ref != 1)
			abort();
		free(owner->tmp);
		total++;
	}
	free(allocation);
	printf("PASS UTF-8 view C spans: %d address/termination/cache/preparation cases WORD%zu\n", total, sizeof(WORD)*8);
	return 0;
}
