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



/* Exercise representation changes through real append/slice/UTF conversion helpers. */
static void
runeroundtrip(Module *module)
{
    int sizes[] = {1, 2, 3, 5, 8, 16, 32};
    int size, op, sample, rx, length, fits, i;
    WORD word, index, value, start, end;
    LONG big;
    String *owner, *slot, *decoded;
    Array *bytes;
    char expected[32];
    char *cache;
    unsigned long cases = 0;

    for(size = 0; size < sizeof(sizes)/sizeof(sizes[0]); size++) for(op = 0; op < 2; op++)
    for(sample = 0; sample < 3; sample++) for(rx = 0; rx < 2; rx++){
        owner = newrunes(sizes[size]); slot = owner;
        owner->tmp = malloc(16);
        if(owner->tmp == nil) abort();
        memmove(owner->tmp, "old-rune-cache", 15); cache = owner->tmp;
        word = sample == 0 ? 0 : sample == 1 ? -1 : (WORD)((UWORD)1 << (sizeof(WORD)*8-1));
        big = sample == 0 ? 0 : sample == 1 ? -1 : (-9223372036854775807LL-1);
        length = snprintf(expected, sizeof(expected), "%lld", (long long)(op ? big : (LONG)word));
        fits = length <= sizes[size]*(int)sizeof(Rune);
        R.s = op ? (void*)&big : (void*)&word; R.d = &slot;
        failure = fits ? 0 : -1;
        if(rx){
            if(module->jit->entry[op ? ICVTLC : ICVTWC](&R) != fits) abort();
        }
        if(!rx || !fits){if(op) cvtlc(); else cvtwc();}
        failure = -1;
        if(slot == H || slot->len != length || memcmp(slot->Sascii, expected, length+1)) abort();
        if(fits && (slot != owner || slot->max != sizes[size]*(int)sizeof(Rune) || slot->tmp != cache)) abort();
        if(!fits && slot == owner) abort();

        index = length; value = '!'; R.s = &value; R.m = &index; R.d = &slot;
        insc();
        if(slot->len != length+1 || memcmp(slot->Sascii, expected, length) || slot->Sascii[length] != '!') abort();
        index++; value = 0x20ac; insc();
        if(slot->len != -(length+2) || slot->Srune[length] != '!' || slot->Srune[length+1] != 0x20ac) abort();
        for(i = 0; i < length; i++) if(slot->Srune[i] != (uchar)expected[i]) abort();

        start = 1; end = length+2; R.s = &start; R.m = &end; R.d = &slot; slicec();
        if(slot->len != -(length+1) || slot->Srune[length] != 0x20ac) abort();
        bytes = H; R.s = &slot; R.d = &bytes; cvtca();
        decoded = H; R.s = &bytes; R.d = &decoded; cvtac();
        if(decoded->len != slot->len || memcmp(decoded->Srune, slot->Srune, (length+1)*sizeof(Rune))) abort();
        destroy(decoded); destroy(bytes); destroy(slot);
        if(live || Tstring.ref != 1 || Tbyte.ref != 1 || Tarray.ref != 1) abort();
        cases++;
    }
    printf("PASS Rune format/append/slice/UTF round trips: %lu cases WORD%zu\n", cases, sizeof(WORD)*8);
}


/* Exercise a reused pointer temporary whose old owner is an array, never a String.
 * module borrows published leaves. Inputs are captured from the retained owner's payload.
 * Verify write-free decline/failure, then publication before the last owner release. */
static void
foreignowners(Module *module)
{
    int shared, rx, failed, op;
    Array *owner;
    String *slot;
    void *source;
    REG saved;
    unsigned char before[64];
    int size;
    WORD word = -123;
    LONG big = (-9223372036854775807LL-1);

    for (shared = 0; shared < 2; shared++) {
        for (rx = 0; rx < 2; rx++) {
            for (failed = 0; failed < 2; failed++) {
                for (op = 0; op < 2; op++) {
                    owner = mem2array(nil, 64);
                    if (shared) {
                        D2H(owner)->ref++;
                    }
                    source = owner->data;
                    size = op ? sizeof(big) : sizeof(word);
                    memmove(source, op ? (void *)&big : (void *)&word, size);
                    memmove(before, source, size);
                    slot = (String *)owner;
                    R.s = source;
                    R.d = &slot;
                    saved = R;
                    if (rx && module->jit->entry[op ? ICVTLC : ICVTWC](&R) != 0) {
                        fprintf(stderr, "foreign format line %d op=%d shared=%d rx=%d failed=%d raised=%s\n", __LINE__, op, shared, rx, failed, raised ? raised : "none");
                        abort();
                    }
                    if (slot != (String *)owner || memcmp(source, before, size)
                        || D2H(owner)->ref != 1+shared || memcmp(&R, &saved, sizeof(R))) {
                        fprintf(stderr, "foreign format line %d op=%d shared=%d rx=%d failed=%d raised=%s\n", __LINE__, op, shared, rx, failed, raised ? raised : "none");
                        abort();
                    }
                    raised = nil;
                    failure = failed ? 0 : -1;
                    if (setjmp(boundary) == 0) {
                        if (op) {
                            cvtlc();
                        } else {
                            cvtwc();
                        }
                    }
                    failure = -1;
                    if (failed) {
                        if (raised != exNomem || slot != (String *)owner
                            || memcmp(owner->data, before, size) || D2H(owner)->ref != 1+shared) {
                            fprintf(stderr, "foreign format line %d op=%d shared=%d rx=%d failed=%d raised=%s\n", __LINE__, op, shared, rx, failed, raised ? raised : "none");
                            abort();
                        }
                        D2H(owner)->ref = 1;
                        destroy(owner);
                    } else {
                        if (raised != nil || slot == (String *)owner || slot == H
                            || strcmp(slot->Sascii, op ? "-9223372036854775808" : "-123")) {
                            fprintf(stderr, "foreign format line %d op=%d shared=%d rx=%d failed=%d raised=%s\n", __LINE__, op, shared, rx, failed, raised ? raised : "none");
                            abort();
                        }
                        if (shared) {
                            if (D2H(owner)->ref != 1 || memcmp(owner->data, before, size)) {
                                fprintf(stderr, "foreign format line %d op=%d shared=%d rx=%d failed=%d raised=%s\n", __LINE__, op, shared, rx, failed, raised ? raised : "none");
                                abort();
                            }
                            destroy(owner);
                        }
                        destroy(slot);
                    }
                    if (live != 0 || Tstring.ref != 1 || Tarray.ref != 1 || Tbyte.ref != 1) {
                        fprintf(stderr, "foreign format line %d op=%d shared=%d rx=%d failed=%d raised=%s\n", __LINE__, op, shared, rx, failed, raised ? raised : "none");
                        abort();
                    }
                }
            }
        }
    }
    printf("PASS non-string temporary formatting: C/RX decline, allocation failure and source lifetime\n");
}


/* Check actual C/RX formatting, failure preflight and ownership in both WORD models. */
int
main(void)
{
    Module module = {0};
    Inst code[2] = {{0}, {0}};
    LONG values[] = {
        0LL, 1LL, -1LL, 9LL, -9LL, 10LL,
        -10LL, 99LL, -99LL, 100LL, -100LL, 2147483647LL,
        -2147483648LL, 9223372036854775807LL, (-9223372036854775807LL-1), 9LL, -9LL, 10LL,
        -10LL, 11LL, -11LL, 99LL, -99LL, 100LL,
        -100LL, 101LL, -101LL, 999LL, -999LL, 1000LL,
        -1000LL, 1001LL, -1001LL, 9999LL, -9999LL, 10000LL,
        -10000LL, 10001LL, -10001LL, 99999LL, -99999LL, 100000LL,
        -100000LL, 100001LL, -100001LL, 999999LL, -999999LL, 1000000LL,
        -1000000LL, 1000001LL, -1000001LL, 9999999LL, -9999999LL, 10000000LL,
        -10000000LL, 10000001LL, -10000001LL, 99999999LL, -99999999LL, 100000000LL,
        -100000000LL, 100000001LL, -100000001LL, 999999999LL, -999999999LL, 1000000000LL,
        -1000000000LL, 1000000001LL, -1000000001LL, 9999999999LL, -9999999999LL, 10000000000LL,
        -10000000000LL, 10000000001LL, -10000000001LL, 99999999999LL, -99999999999LL, 100000000000LL,
        -100000000000LL, 100000000001LL, -100000000001LL, 999999999999LL, -999999999999LL, 1000000000000LL,
        -1000000000000LL, 1000000000001LL, -1000000000001LL, 9999999999999LL, -9999999999999LL, 10000000000000LL,
        -10000000000000LL, 10000000000001LL, -10000000000001LL, 99999999999999LL, -99999999999999LL, 100000000000000LL,
        -100000000000000LL, 100000000000001LL, -100000000000001LL, 999999999999999LL, -999999999999999LL, 1000000000000000LL,
        -1000000000000000LL, 1000000000000001LL, -1000000000000001LL, 9999999999999999LL, -9999999999999999LL, 10000000000000000LL,
        -10000000000000000LL, 10000000000000001LL, -10000000000000001LL, 99999999999999999LL, -99999999999999999LL, 100000000000000000LL,
        -100000000000000000LL, 100000000000000001LL, -100000000000000001LL, 999999999999999999LL, -999999999999999999LL, 1000000000000000000LL,
        -1000000000000000000LL, 1000000000000000001LL, -1000000000000000001LL
    };

    union {
        uvlong alignment; // Native alignment for the independent operand image.
        uchar bytes[128]; // Unaligned scalar and owner slots with surrounding canaries.
    } slots;
    uchar before[128], expected[128], ownerbefore[256], ownerexpected[256];
    String *owner, *result, *published;
    REG saved;
    WORD word;
    LONG big;
    char decimal[32];
    int op, value, sa, da, mode, alias, pass, length, allocations, accepted, invalid;
    ulong refs;
    uvlong capacity;
    int imagebytes, runes;
    unsigned long checks = 0;

    code[0].op = ICVTWC; code[1].op = ICVTLC;
    module.prog = code; module.nprog = 2;
    if(!compile(&module, 2, nil) || !module.jit ||
       !module.jit->entry[ICVTWC] || !module.jit->entry[ICVTLC])
        abort();
    for(op = 0; op < 2; op++) for(value = 0; value < sizeof(values)/sizeof(values[0]); value++)
    for(sa = 0; sa < 8; sa++) for(da = 0; da < 8; da++)
    for(mode = 0; mode < 55; mode++) for(alias = 0; alias < 3; alias++){
        if(value >= 15 && (sa || da)) continue;
        if(getenv("FORMAT_MODE") && mode != atoi(getenv("FORMAT_MODE"))) continue;
        runes = mode >= 36 && mode <= 41 ? mode-35 : 32;
        owner = mode == 2 || mode >= 36 ? newrunes(32) : newstring(32);
        imagebytes = sizeof(String)+(mode == 2 || mode >= 36 ? 32*sizeof(Rune) : 32)+1;
        memset(owner->Sascii, 0xa5, imagebytes-sizeof(String));
        if(mode == 2 || mode >= 36){
            owner->tmp = malloc(16);
            if(owner->tmp == nil) abort();
            memmove(owner->tmp, "old-rune-cache", 15);
        }
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
        if(mode >= 14 && mode < 36){owner->len = 0; owner->max = mode-14;}
        if(mode >= 36){owner->len = -(mode <= 41 ? runes : 7); if(mode <= 41) owner->max = runes;}
        if(mode == 43){owner->len = -1; owner->max = 0x1fffffff;}
        if(mode == 44){owner->len = -1; owner->max = 0x20000000;}
        if(mode == 45){owner->len = -1; owner->max = 0x7fffffff;}
        if(mode == 46) owner->len = (-0x7fffffff-1);
        if(mode == 47) owner->len = -33;
        if(mode == 48) owner->max = -1;
        if(mode == 49) D2H(owner)->ref = 2;
        if(mode == 50) D2H(owner)->ref = (ulong)-1;
        if(mode == 51) D2H(owner)->ref = 0;
        if(mode == 52) Tstring.ref = 0x7fffffff;
        if(mode == 53) Tstring.ref = 0;
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
        memmove(ownerbefore, owner, imagebytes);
        refs = D2H(owner)->ref;
        capacity = owner->len < 0 ? (uvlong)(uint)owner->max*sizeof(Rune) : (uvlong)(uint)owner->max;
        invalid = (mode >= 4 && mode <= 7) || mode == 12 || mode == 13 ||
            (mode >= 44 && mode <= 48) || mode == 51;
        accepted = !invalid && published == owner && refs == 1 && capacity <= 0x7fffffff && capacity >= length;
        for(pass = 0; pass < 2; pass++){
            R = saved;
            memmove(slots.bytes, before, sizeof(before));
            memmove(owner, ownerbefore, imagebytes);
            memmove(expected, before, sizeof(expected));
            memmove(ownerexpected, ownerbefore, imagebytes);
            allocations = calls;
            if(pass == 0 && getenv("FORMAT_C_ONLY")) continue;
            if(pass == 0){
                if(module.jit->entry[code[op].op](&R) != accepted) abort();
                if(accepted){
                    ((String*)ownerexpected)->len = length;
                    ((String*)ownerexpected)->max = capacity;
                    memmove(((String*)ownerexpected)->Sascii, decimal, length+1);
                }
                if(calls != allocations || memcmp(owner, ownerexpected, imagebytes) ||
                   memcmp(slots.bytes, expected, sizeof(expected))) abort();
            }else{
                /* A pending allocation failure cannot affect unique-storage formatting. */
                failure = mode == 9 || mode == 12 || (mode >= 14 && accepted) ? 0 : -1;
                raised = nil;
                if(setjmp(boundary) == 0){
                    if(op) cvtlc(); else cvtwc();
                    if(invalid) abort();
                }else if(!invalid || raised != (mode == 7 ? exNilref : mode == 12 ? exNomem : exHeap)) abort();
                failure = -1;
                memmove(&result, R.d, sizeof(result));
                if(invalid){
                    if(calls != allocations || memcmp(owner, ownerbefore, imagebytes) ||
                       memcmp(slots.bytes, before, sizeof(before))) abort();
                }else{
                    if(result == H || result == nil || result->len != length ||
                       memcmp(result->Sascii, decimal, length+1)) abort();
                    if(accepted){
                        if(result != owner || calls != allocations || strcmp(string2c(result), decimal)) abort();
                        if((mode == 2 || mode >= 36) && strcmp(result->tmp, "old-rune-cache")) abort();
                        ((String*)ownerexpected)->len = length;
                        ((String*)ownerexpected)->max = capacity;
                        memmove(((String*)ownerexpected)->Sascii, decimal, length+1);
                        if(memcmp(owner, ownerexpected, imagebytes)) abort();
                    }else{
                        if(result == owner || calls != allocations+1) abort();
                        destroy(result);
                        /* Shared input retains one external owner; other replaced owners are freed. */
                        if(published == owner && refs > 1 && (D2H(owner)->ref != refs-1 || memcmp(owner, ownerbefore, imagebytes))) abort();
                        if(published == owner && refs == 1) owner = H;
                    }
                    memmove(expected+80+da, &result, sizeof(result));
                    if(memcmp(slots.bytes, expected, sizeof(expected))) abort();
                }
            }
            if(memcmp(&R, &saved, sizeof(R))) abort();
        }
        if(mode == 10 || mode == 11 || mode == 52 || mode == 53) Tstring.ref = 2;
        if(owner != H){
            owner->len = 7; owner->max = 32; D2H(owner)->ref = 1;
            destroy(owner);
        }
        if(live != 0 || Tstring.ref != 1) abort();
        checks++;
    }
    runeroundtrip(&module);
    foreignowners(&module);
    module.jit->freecode(module.jit->code, module.jit->size); free(module.jit);
    printf("PASS integer formatting C/RX: %lu cases WORD%zu\n", checks, sizeof(WORD)*8);
    return 0;
}
