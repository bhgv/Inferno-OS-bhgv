/* Allocate ordinary publication metadata without changing the VM heap fixture. */
void*
mallocz(ulong size, int clear)
{
	void *p;

	p = malloc(size);
	if(p != nil && clear)
		memset(p, 0, size);
	return p;
}


/* Exercise the production compiler's normal executable publication boundary. */
int
kprotect(int (*fn)(void*), void *arg)
{
	return fn(arg);
}

int mutator, nprop;



/* Check actual C/RX formatting, failure preflight and ownership in both WORD models. */
int
main(void)
{
    Module module = {0};
    Inst code[2] = {{0}, {0}};
    LONG values[] = {0, 1, -1, 9, -9, 10, -10, 99, -99, 100, -100,
        2147483647LL, -2147483648LL, 9223372036854775807LL, (-9223372036854775807LL-1)};

    union {
        uvlong alignment; // Native alignment for the independent operand image.
        uchar bytes[128]; // Unaligned scalar and owner slots with surrounding canaries.
    } slots;
    uchar before[128], expected[128], ownerbefore[80], ownerexpected[80];
    String *owner, *result, *published;
    REG saved;
    WORD word;
    LONG big;
    char decimal[32];
    int op, value, sa, da, mode, alias, pass, length, allocations, accepted, invalid;
    unsigned long checks = 0;

    code[0].op = ICVTWC; code[1].op = ICVTLC;
    module.prog = code; module.nprog = 2;
    if(!compile(&module, 2, nil) || !module.jit ||
       !module.jit->entry[ICVTWC] || !module.jit->entry[ICVTLC])
        abort();
    for(op = 0; op < 2; op++) for(value = 0; value < sizeof(values)/sizeof(values[0]); value++)
    for(sa = 0; sa < 8; sa++) for(da = 0; da < 8; da++)
    for(mode = 0; mode < 14; mode++) for(alias = 0; alias < 3; alias++){
        if(getenv("FORMAT_MODE") && mode != atoi(getenv("FORMAT_MODE"))) continue;
        owner = newstring(32);
        memset(owner->Sascii, 0xa5, 33);
        owner->len = 7;
        if(mode == 1) D2H(owner)->ref = 2;
        if(mode == 2) owner->len = -7;
        if(mode == 3) owner->max = 23;
        if(mode == 4) D2H(owner)->ref = 0;
        if(mode == 5) owner->len = (-0x7fffffff-1);
        if(mode == 6) owner->max = -1;
        if(mode == 10) Tstring.ref = 0x7fffffff;
        if(mode == 11) Tstring.ref = 0;
        if(mode == 12) D2H(owner)->ref = 2;
        if(mode == 13) owner->len = 33;
        published = mode == 7 ? nil : mode == 8 ? H : owner;
        memset(slots.bytes, 0xa5, sizeof(slots.bytes));
        R.s = alias == 1 ? (void*)(owner->Sascii+sa) : alias == 2 ? (void*)(slots.bytes+80+da) : (void*)(slots.bytes+sa);
        R.d = slots.bytes+80+da;
        word = (WORD)values[value]; big = values[value];
        memmove(R.s, op ? (void*)&big : (void*)&word, op ? sizeof(big) : sizeof(word));
        memmove(R.d, &published, sizeof(published));
        if(op) memmove(&big, R.s, sizeof(big));
        else memmove(&word, R.s, sizeof(word));
        length = snprintf(decimal, sizeof(decimal), "%lld", (long long)(op ? big : (LONG)word));
        saved = R;
        memmove(before, slots.bytes, sizeof(before));
        memmove(ownerbefore, owner, sizeof(String)+33);
        accepted = mode == 0 || mode == 9 || mode == 10 || mode == 11;
        invalid = (mode >= 4 && mode <= 7) || mode == 12 || mode == 13;
        for(pass = 0; pass < 2; pass++){
            R = saved;
            memmove(slots.bytes, before, sizeof(before));
            memmove(owner, ownerbefore, sizeof(String)+33);
            memmove(expected, before, sizeof(expected));
            memmove(ownerexpected, ownerbefore, sizeof(String)+33);
            allocations = calls;
            if(pass == 0 && getenv("FORMAT_C_ONLY")) continue;
            if(pass == 0){
                if(module.jit->entry[code[op].op](&R) != accepted) abort();
                if(accepted){
                    ((String*)ownerexpected)->len = length;
                    memmove(((String*)ownerexpected)->Sascii, decimal, length+1);
                }
                if(calls != allocations || memcmp(owner, ownerexpected, sizeof(String)+33) ||
                   memcmp(slots.bytes, expected, sizeof(expected))) abort();
            }else{
                /* A pending allocation failure cannot affect unique-storage formatting. */
                failure = mode == 9 || mode == 12 ? 0 : -1;
                raised = nil;
                if(setjmp(boundary) == 0){
                    if(op) cvtlc(); else cvtwc();
                    if(invalid) abort();
                }else if(!invalid || raised != (mode == 7 ? exNilref : mode == 12 ? exNomem : exHeap)) abort();
                failure = -1;
                memmove(&result, R.d, sizeof(result));
                if(invalid){
                    if(calls != allocations || memcmp(owner, ownerbefore, sizeof(String)+33) ||
                       memcmp(slots.bytes, before, sizeof(before))) abort();
                }else{
                    if(result == H || result == nil || result->len != length ||
                       memcmp(result->Sascii, decimal, length+1)) abort();
                    if(accepted){
                        if(result != owner || calls != allocations) abort();
                        ((String*)ownerexpected)->len = length;
                        memmove(((String*)ownerexpected)->Sascii, decimal, length+1);
                        if(memcmp(owner, ownerexpected, sizeof(String)+33)) abort();
                    }else{
                        if(result == owner || calls != allocations+1) abort();
                        destroy(result);
                        /* Shared input retains one external owner; other replaced owners are freed. */
                        if(mode != 1 && mode != 8) owner = H;
                    }
                    memmove(expected+80+da, &result, sizeof(result));
                    if(memcmp(slots.bytes, expected, sizeof(expected))) abort();
                }
            }
            if(memcmp(&R, &saved, sizeof(R))) abort();
        }
        if(mode == 10 || mode == 11) Tstring.ref = 2;
        if(owner != H){
            owner->len = 7; owner->max = 32; D2H(owner)->ref = 1;
            destroy(owner);
        }
        if(live != 0 || Tstring.ref != 1) abort();
        checks++;
    }
    module.jit->freecode(module.jit->code, module.jit->size); free(module.jit);
    printf("PASS integer formatting C/RX: %lu cases WORD%zu\n", checks, sizeof(WORD)*8);
    return 0;
}
