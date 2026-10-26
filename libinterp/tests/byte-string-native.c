int mutator, nprop; // Live collector cells required by the production publication adapter.


/* Keep compiler metadata allocation independent of the observed VM heap. */
void*
mallocz(ulong size, int clear)
{
    void *value;

    value = malloc(size);
    if(value != nil && clear)
        memset(value, 0, size);
    return value;
}


/* Enter the normal executable publication path without changing fixture error frames. */
int
kprotect(int (*fn)(void*), void *arg)
{
    return fn(arg);
}


/* Report one independent owning/oracle case before terminating a failed process. */
static void
bytefail(int length, int capacity, int profile, int sa, int da, int failed, int line)
{
    fprintf(stderr, "CVTAC case n=%d cap=%d profile=%d sa=%d da=%d fail=%d line=%d\n",
            length, capacity, profile, sa, da, failed, line);
    abort();
}


/* Reject owner/descriptor geometry before payload access or output publication. */
static void
spanerrors(Module *module)
{
    Array *a, *input;
    String *old, *destination;
    Array original;
    Heap owner;
    REG saved;
    uchar slots[128], before[128];
    char *why;
    int mode, sa, da, allocations, objects;
    unsigned long total;

    total = 0;
    for(mode = 0; mode < 19; mode++)
    for(sa = 0; sa < 8; sa++)
    for(da = 0; da < 8; da++){
        a = mem2array("abc", 3);
        old = newstring(16); old->len = 0;
        input = a; destination = old; why = exHeap;
        original = *a; owner = *D2H(a);
        switch(mode){
        case 0: input = nil; why = exNilref; break;
        case 1: input = (Array*)8; break;
        case 2: input = (Array*)(uintptr)-2; break;
        case 3: input = (Array*)((uintptr)-1-sizeof(Array)+1); break;
        case 4: a->t = (Type*)(uintptr)-2; why = exType; break;
        case 5: a->t = H; why = exType; break;
        case 6: a->data = nil; why = exNilref; break;
        case 7: a->data = H; why = exNilref; break;
        case 8: a->data = (uchar*)(uintptr)-2; break;
        case 9: a->root = nil; break;
        case 10: a->root = (Array*)8; break;
        case 11: D2H(a)->ref = 0; a->t = (Type*)1; break;
        case 12: destination = nil; why = exNilref; break;
        case 13: destination = (String*)8; break;
        case 14: D2H(old)->ref = 0; break;
        case 15: a->len = 0; a->t = (Type*)(uintptr)-2; why = exType; break;
        case 16: input = H; destination = nil; why = exNilref; break;
        case 17: input = H; destination = (String*)8; break;
        case 18: input = H; D2H(old)->ref = 0; break;
        }
        memset(slots, 0xa5, sizeof(slots));
        memmove(slots+sa, &input, sizeof(input));
        memmove(slots+80+da, &destination, sizeof(destination));
        R.s = slots+sa; R.d = slots+80+da; saved = R;
        memmove(before, slots, sizeof(before));
        allocations = calls; objects = live;
        if(module->jit->entry[ICVTAC](&R) != 0 || memcmp(before, slots, sizeof(slots)))
            bytefail(3, 16, mode, sa, da, 0, __LINE__);
        raised = nil;
        if(setjmp(boundary) == 0)
            cvtac();
        if(raised != why || calls != allocations || live != objects ||
           memcmp(before, slots, sizeof(slots)) || memcmp(&saved, &R, sizeof(R)))
            bytefail(3, 16, mode, sa, da, 0, __LINE__);
        *a = original; *D2H(a) = owner; D2H(old)->ref = 1;
        destroy(a); destroy(old);
        total++;
    }
    printf("PASS CVTAC owner/descriptor spans: %lu error/publication cases\n", total);
}


/* Check actual reuse, allocation failure, Unicode fallback, overlap and temporary owners. */
int
main(void)
{
    int lengths[] = {0, 1, 2, 7, 31, 128, 255, 256, 257, 511};
    int capacities[] = {0, 7, 256, 512};
    Module module = {0};
    Inst instruction = {0};
    Array *a;
    String *old, *result;
    void *input;
    Heap ah, oh;
    uchar abefore[2048], obefore[2048], oexpected[2048], slots[128], slotsbefore[128], slotsexpected[128];
    uchar captured[512];
    REG saved;
    char *why;
    int ni, ci, profile, sa, da, failed, n, cap, asize, osize, i, unicode;
    int reuse, native, invalid, beforecalls, beforelive, status, shared, flags;
    unsigned long total;

    Tbyte.size = 1;
    instruction.op = ICVTAC;
    module.prog = &instruction; module.nprog = 1;
    if(!compile(&module, 1, nil) || module.jit == nil || module.jit->entry[ICVTAC] == nil)
        abort();
    if(!getenv("BYTE_STRING_NO_ERRORS"))
        spanerrors(&module);
    total = 0;
    for(ni = 0; ni < sizeof(lengths)/sizeof(lengths[0]); ni++)
    for(ci = 0; ci < sizeof(capacities)/sizeof(capacities[0]); ci++)
    for(profile = 0; profile < 16; profile++)
    for(sa = 0; sa < 8; sa++)
    for(da = 0; da < 8; da++)
    for(failed = 0; failed < 2; failed++){
        n = lengths[ni]; cap = capacities[ci];
        /* Separate successful-path ASan checks from unavailable stack-unwind inspection. */
        if(getenv("BYTE_STRING_NO_ERRORS") && (failed || profile == 5 || profile == 6 || profile == 7 || profile == 9))
            continue;
        /* Interior-slot profiles exercise only the safe in-place owner lifetime. */
        if(profile >= 14 && cap < n)
            continue;
        failure = -1;
        a = mem2array(nil, n);
        old = newstring(1024); old->len = 0; old->max = cap;
        input = a; invalid = 0; unicode = 0; shared = 0; why = exHeap;
        switch(profile){
        case 1: D2H(old)->ref = 2; shared = 1; break;
        case 2: old->len = -1; break;
        case 3: destroy(old); old = (String*)mem2array(nil, 1024); break;
        case 4: unicode = n != 0; break;
        case 5: D2H(a)->ref = 0; invalid = 1; break;
        case 6: a->root = nil; invalid = 1; break;
        case 7: input = nil; invalid = 1; why = exNilref; break;
        case 8: input = H; break;
        case 9: destroy(old); old = nil; invalid = 1; why = exNilref; break;
        case 10: destroy(old); old = H; break;
        case 11: a->data = (uchar*)old->Sascii-3; break;
        case 12: a->data = (uchar*)old->Sascii+3; break;
        case 13: a->data = (uchar*)old->Sascii; break;
        }
        for(i = 0; i < n; i++)
            a->data[i] = profile == 11 && i < 3 ? 0 : (i*17+sa+da)%128;
        if(unicode){
            a->data[0] = 0xc2;
            if(n > 1) a->data[1] = 0xa2;
        }
        memset(slots, 0x5a, sizeof(slots));
        R.s = profile == 15 ? (uchar*)old->Sascii+5 : slots+sa;
        R.d = profile == 14 ? (uchar*)old->Sascii+5 : slots+80+da;
        memmove(R.s, &input, sizeof(input));
        memmove(R.d, &old, sizeof(old));
        saved = R;
        asize = hmsize(D2H(a))-sizeof(Heap);
        osize = old != nil && old != H ? hmsize(D2H(old))-sizeof(Heap) : 0;
        ah = *D2H(a);
        if(osize) oh = *D2H(old);
        memmove(abefore, a, asize);
        if(osize){memmove(obefore, old, osize); memmove(oexpected, old, osize);}
        memmove(slotsbefore, slots, sizeof(slots));
        memmove(slotsexpected, slots, sizeof(slots));
        if(n) memmove(captured, a->data, n);
        reuse = !invalid && input != H && old != H && old != nil && profile != 1 &&
                profile != 2 && profile != 3 && !unicode && cap >= n;
        native = reuse && 1 <= n && n <= 256;
        if(native){
            memmove(oexpected+offsetof(String, data), captured, n);
            memmove(oexpected+offsetof(String, len), &n, sizeof(n));
            if(profile == 14) memmove(oexpected+offsetof(String, data)+5, &old, sizeof(old));
            else memmove(slotsexpected+80+da, &old, sizeof(old));
        }
        beforecalls = calls; beforelive = live;
        fesetround(FE_DOWNWARD); feclearexcept(FE_ALL_EXCEPT); feraiseexcept(FE_DIVBYZERO);
        flags = fetestexcept(FE_ALL_EXCEPT);
        status = module.jit->entry[ICVTAC](&R);
        if(status != native || calls != beforecalls || live != beforelive ||
           memcmp(&saved, &R, sizeof(R)) || memcmp(slots, slotsexpected, sizeof(slots)) ||
           memcmp(a, abefore, asize) || memcmp(D2H(a), &ah, sizeof(ah)) ||
           (osize && (memcmp(old, oexpected, osize) || memcmp(D2H(old), &oh, sizeof(oh)))) ||
           fegetround() != FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT) != flags)
            bytefail(n, cap, profile, sa, da, failed, __LINE__);
        /* Restore the same complete input for the real C instruction. */
        if(osize) memmove(old, obefore, osize);
        memmove(slots, slotsbefore, sizeof(slots));
        R = saved; raised = nil; failure = failed ? 0 : -1;
        if(setjmp(boundary) == 0)
            cvtac();
        failure = -1;
        if(invalid || (failed && !reuse && input != H)){
            if(raised != (invalid ? why : exNomem) || calls != beforecalls || live != beforelive ||
               memcmp(slots, slotsbefore, sizeof(slots)) || memcmp(a, abefore, asize) ||
               (osize && (memcmp(old, obefore, osize) || memcmp(D2H(old), &oh, sizeof(oh)))))
                bytefail(n, cap, profile, sa, da, failed, __LINE__);
            result = old;
        }else{
            if(raised != nil)
                bytefail(n, cap, profile, sa, da, failed, __LINE__);
            memmove(&result, R.d, sizeof(result));
            if(input == H){
                if(result != H || calls != beforecalls)
                    bytefail(n, cap, profile, sa, da, failed, __LINE__);
            }else if(reuse){
                memmove(oexpected, obefore, osize);
                if(n) memmove(oexpected+offsetof(String, data), captured, n);
                memmove(oexpected+offsetof(String, len), &n, sizeof(n));
                if(profile == 14) memmove(oexpected+offsetof(String, data)+5, &old, sizeof(old));
                if(result != old || calls != beforecalls || live != beforelive || memcmp(old, oexpected, osize))
                    bytefail(n, cap, profile, sa, da, failed, __LINE__);
            }else{
                if(result == old || result == H || calls != beforecalls+1)
                    bytefail(n, cap, profile, sa, da, failed, __LINE__);
                if(!unicode){
                    if(result->len != n || (n && memcmp(result->Sascii, captured, n)))
                        bytefail(n, cap, profile, sa, da, failed, __LINE__);
                }else{
                    if(result->len != (n == 1 ? 0 : -(n-1)) || (n > 1 && result->Srune[0] != 0xa2))
                        bytefail(n, cap, profile, sa, da, failed, __LINE__);
                    for(i = 2; i < n; i++)
                        if(result->Srune[i-1] != captured[i])
                            bytefail(n, cap, profile, sa, da, failed, __LINE__);
                }
            }
        }
        if(memcmp(&saved, &R, sizeof(R)) || fegetround() != FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT) != flags)
            bytefail(n, cap, profile, sa, da, failed, __LINE__);
        D2H(a)->ref = 1; a->root = H;
        destroy(a);
        if(shared && result != old)
            destroy(old);
        if(shared && result == old)
            D2H(old)->ref = 1;
        if(result != nil)
            destroy(result);
        if(live != 0 || Tstring.ref != 1 || Tarray.ref != 1 || Tbyte.ref != 1)
            bytefail(n, cap, profile, sa, da, failed, __LINE__);
        total++;
    }
    module.jit->freecode(module.jit->code, module.jit->size); free(module.jit);
    printf("PASS CVTAC C/RX: %lu owning/reuse/overlap/UTF/failure/FP cases WORD%zu\n", total, sizeof(WORD)*8);
    return 0;
}
