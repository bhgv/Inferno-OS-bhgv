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


/* Model address overflow while keeping every actual fixture allocation accessible. */
int
main(void)
{
    uchar *allocation, *base, leftbefore[4096], rightbefore[4096], expected[4096];
    String *left, *right, *result;
    uintptr aligned;
    int offsets[] = {0xfff8, 0xfff0, 0xffe8, 0xffe0, 0xffc0, 0xff00};
    int lengths[] = {1, 7, 127, 256};
    int side, wide, offset, li, ri, l, r, stride, invalid, count = 0, allocations;
    uvlong leftstart, rightstart, dataoffset;
    Heap *header;

    allocation = malloc(0x40000);
    if(allocation == nil)
        abort();
    aligned = ((uintptr)allocation+0xffff)&~(uintptr)0xffff;
    base = (uchar*)aligned;
    dataoffset = offsetof(String, data);
    for(side = 0; side < 2; side++)
    for(wide = 0; wide < 2; wide++)
    for(offset = 0; offset < nelem(offsets); offset++)
    for(li = 0; li < nelem(lengths); li++)
    for(ri = 0; ri < nelem(lengths); ri++){
        left = (String*)(base+(side ? 0x800 : offsets[offset]));
        right = (String*)(base+(side ? offsets[offset] : 0x800));
        stride = wide ? sizeof(Rune) : 1;
        l = lengths[li];
        r = lengths[ri];
        memset(D2H(left), 0, sizeof(Heap)+4096);
        memset(D2H(right), 0, sizeof(Heap)+4096);
        header = D2H(left);
        header->ref = 1;
        header->t = &Tstring;
        header = D2H(right);
        header->ref = 1;
        header->t = &Tstring;
        left->len = wide ? -l : l;
        right->len = wide ? -r : r;
        left->max = right->max = 1024;
        memset(left->Sascii, 0xa5, 3072);
        memset(right->Sascii, 0x5a, 3072);
        memmove(leftbefore, left, sizeof(leftbefore));
        memmove(rightbefore, right, sizeof(rightbefore));
        memmove(expected, leftbefore, sizeof(expected));
        leftstart = ((uintptr)left&0xffff)+dataoffset;
        rightstart = ((uintptr)right&0xffff)+dataoffset;
        invalid = leftstart+(uvlong)(l+r)*stride > 0xffff ||
            rightstart+(uvlong)r*stride > 0xffff;
        raised = nil;
        result = H;
        allocations = calls;
        if(setjmp(boundary) == 0){
            result = addstring(left, right, 1);
            if(invalid || result != left)
                abort();
            memmove(expected+dataoffset+l*stride, right->Sascii, r*stride);
            ((String*)expected)->len = wide ? -(l+r) : l+r;
        }else if(!invalid || raised != exHeap)
            abort();
        if(calls != allocations || memcmp(left, expected, sizeof(expected)) ||
           memcmp(right, rightbefore, sizeof(rightbefore)) ||
           D2H(left)->ref != 1 || D2H(right)->ref != 1)
            abort();
        count++;
    }
    free(allocation);
    printf("PASS append C reduced address spans: %d overflow/image/preparation cases WORD%zu\n", count, sizeof(WORD)*8);
    return 0;
}
