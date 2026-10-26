

/* Report the independent address-domain case before stopping a failed oracle. */
static void
spanfail(int offset, int n, int wide, int profile, int line)
{
    fprintf(stderr, "CVTCA span offset=%x n=%d wide=%d profile=%d line=%d\n", offset, n, wide, profile, line);
    abort();
}


/* Exercise exact header/payload boundaries without allocating near the real address limit. */
int
main(void)
{
    int offsets[] = {0, 8, 16, 24, 32, 0x100, 0xff00, 0xffc0, 0xffd0, 0xffe0, 0xffee, 0xffef, 0xfff0, 0xfff7, 0xfff8};
    int lengths[] = {0, 1, 2, 7, 31, 128, 257};
    Rune runes[] = {0, 0x7f, 0x80, 0x20ac, 0x1f642, 0xd800, 0x110000, 0xffffffff};
    uchar *raw, *base, *before, payload[2048], encoded[4], slots[128], expected[128];
    String *text, *input;
    Array *output;
    Heap heap;
    REG saved;
    uintptr first, headerend, dataend;
    int oi, ni, wide, profile, sa, da, n, length, capacity, valid, i, nb, bytes, callsbefore;
    char *why;
    unsigned long total;

    raw = malloc(0x50000); before = malloc(0x20040);
    if(raw == nil || before == nil)
        abort();
    first = ((uintptr)raw+0x1ffff)&~(uintptr)0xffff;
    base = (uchar*)first;
    total = 0;
    for(oi = 0; oi < sizeof(offsets)/sizeof(offsets[0]); oi++)
    for(ni = 0; ni < sizeof(lengths)/sizeof(lengths[0]); ni++)
    for(wide = 0; wide < 2; wide++)
    for(profile = 0; profile < 8; profile++)
    for(sa = 0; sa < 4; sa++)
    for(da = 0; da < 4; da++){
        if(getenv("STRING_BYTES_SPAN_HIGH_ONLY") && offsets[oi] < 0xff00)
            continue;
        memset(base-64, 0x5a, 0x20040); memset(slots, 0xa5, sizeof(slots));
        n = lengths[ni]; length = wide ? -n : n; capacity = n;
        text = (String*)(base+offsets[oi]); input = text; output = H;
        memset(&heap, 0, sizeof(heap)); heap.ref = 1; heap.t = &Tstring;
        why = exHeap;
        switch(profile){
        case 1: input = nil; why = exNilref; break;
        case 2: input = H; break;
        case 3: heap.t = &Tarray; break;
        case 4: heap.ref = 0; break;
        case 5: length = -0x7fffffff-1; capacity = 0x7fffffff; break;
        case 6: capacity = n-1; break;
        case 7: length = wide ? -0x7fffffff : 0x7fffffff; capacity = 0x7fffffff; break;
        }
        memmove((uchar*)text-sizeof(Heap), &heap, sizeof(heap));
        memmove((uchar*)text+offsetof(String, len), &length, sizeof(length));
        memmove((uchar*)text+offsetof(String, max), &capacity, sizeof(capacity));
        bytes = 0;
        for(i = 0; i < n; i++){
            if(wide){
                Rune rune = runes[i%(sizeof(runes)/sizeof(runes[0]))];
                memmove((uchar*)text+offsetof(String, data)+i*sizeof(Rune), &rune, sizeof(rune));
                nb = reference(rune, encoded); memmove(payload+bytes, encoded, nb); bytes += nb;
            }else{
                ((uchar*)text)[offsetof(String, data)+i] = (i*37+sa+da)%256;
                payload[bytes++] = (i*37+sa+da)%256;
            }
        }
        headerend = offsets[oi]+(offsetof(String, len) > offsetof(String, max) ? offsetof(String, len) : offsetof(String, max))+sizeof(int);
        dataend = offsets[oi]+offsetof(String, data)+(uvlong)n*(wide ? sizeof(Rune) : 1);
        valid = offsets[oi] >= sizeof(Heap) && headerend <= 65535;
        if(valid && profile == 3) why = exType;
        valid = valid && profile == 0 && (n == 0 || dataend <= 65535);
        if(profile == 1) valid = 0;
        if(profile == 2){valid = 1; bytes = 0;}
        memmove(slots+sa, &input, sizeof(input)); memmove(slots+80+da, &output, sizeof(output));
        memmove(expected, slots, sizeof(expected)); memmove(before, base-64, 0x20040);
        R.s = slots+sa; R.d = slots+80+da; saved = R;
        callsbefore = calls; raised = nil;
        if(setjmp(boundary) == 0)
            cvtca();
        if(valid){
            memmove(&output, R.d, sizeof(output));
            if(raised != nil || output == H || output->len != bytes || calls != callsbefore+1 ||
               (bytes && memcmp(output->data, payload, bytes)))
                spanfail(offsets[oi], n, wide, profile, __LINE__);
            memmove(expected+80+da, &output, sizeof(output));
            destroy(output);
        }else if(raised != why || calls != callsbefore)
            spanfail(offsets[oi], n, wide, profile, __LINE__);
        if(memcmp(slots, expected, sizeof(slots)) || memcmp(before, base-64, 0x20040) ||
           memcmp(&saved, &R, sizeof(R)) || live != 0 || Tstring.ref != 1 || Tarray.ref != 1 || Tbyte.ref != 1)
            spanfail(offsets[oi], n, wide, profile, __LINE__);
        total++;
    }
    free(raw); free(before);
    printf("PASS CVTCA reduced spans: %lu header/ASCII/Rune/error/image cases WORD%zu\n", total, sizeof(WORD)*8);
    return 0;
}
