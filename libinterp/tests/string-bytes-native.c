int mutator, nprop; // Collector cells required by the production native publication adapter.


/* Keep native code publication allocation separate from counted VM heap objects. */
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


/* Identify one owning, publication or encoding case before stopping the process. */
static void
exportfail(int n, int cap, int profile, int sa, int da, int failed, int line)
{
    fprintf(stderr, "CVTCA n=%d cap=%d profile=%d sa=%d da=%d fail=%d line=%d\n",
            n, cap, profile, sa, da, failed, line);
    abort();
}


/* Reject bad source/target metadata before any result or owner counter is changed. */
static void
spanerrors(Module *module)
{
    String *text, *input;
    Array *old, *destination, *backing;
    Heap sourceheap, oldheap;
    Array oldview;
    Type dead;
    REG saved;
    uchar slots[128], before[128], sourcebefore[128], oldbefore[128];
    char *why;
    int mode, sa, da, allocations, objects;
    unsigned long total;

    total = 0;
    for(mode = 0; mode < 29; mode++)
    for(sa = 0; sa < 8; sa++)
    for(da = 0; da < 8; da++){
        text = newstring(16); text->len = 3;
        old = mem2array(nil, 24); backing = mem2array(nil, 0);
        input = text; destination = old; why = exHeap;
        dead = Tbyte; dead.ref = 0;
        sourceheap = *D2H(text); oldheap = *D2H(old); oldview = *old;
        switch(mode){
        case 0: input = nil; why = exNilref; break;
        case 1: input = (String*)8; break;
        case 2: input = (String*)(uintptr)-2; break;
        case 3: input = (String*)((uintptr)-1-8+1); break;
        case 4: D2H(text)->t = &Tarray; why = exType; break;
        case 5: D2H(text)->ref = 0; break;
        case 6: text->len = -0x7fffffff-1; break;
        case 7: text->len = 0x7fffffff; text->max = 0x7fffffff; break;
        case 8: text->len = -(int)((0x7fffffff-sizeof(Heap)-sizeof(Array))/UTFmax)-1; text->max = -text->len; break;
        case 9: text->max = 2; break;
        case 10: destination = nil; why = exNilref; break;
        case 11: destination = (Array*)8; break;
        case 12: D2H(old)->ref = 0; break;
        case 13: old->t = (Type*)(uintptr)-2; why = exType; break;
        case 14: old->root = nil; break;
        case 15: old->root = (Array*)8; break;
        case 16: old->data = nil; why = exNilref; break;
        case 17: old->data = H; why = exNilref; break;
        case 18: old->data = (uchar*)(uintptr)-2; break;
        case 19: input = H; destination = nil; why = exNilref; break;
        case 20: old->root = backing; D2H(backing)->ref = 0; break;
        case 21: input = H; D2H(old)->ref = 0; break;
        case 22: input = H; old->root = nil; break;
        case 23: text->len = -1; old->root = backing; D2H(backing)->ref = 0; break;
        case 24: input = H; old->root = (Array*)8; break;
        case 25: text->len = -1; old->root = nil; break;
        case 26: old->t = &dead; break;
        case 27: old->t = H; why = exType; break;
        case 28: old->t = nil; why = exType; break;
        }
        memset(slots, 0xa5, sizeof(slots));
        memmove(slots+sa, &input, sizeof(input));
        memmove(slots+80+da, &destination, sizeof(destination));
        R.s = slots+sa; R.d = slots+80+da; saved = R;
        memmove(before, slots, sizeof(before));
        memmove(sourcebefore, text, sizeof(String)+16); memmove(oldbefore, old, sizeof(Array)+24);
        allocations = calls; objects = live;
        if(module->jit->entry[ICVTCA](&R) != 0 || memcmp(before, slots, sizeof(slots)) ||
           memcmp(sourcebefore, text, sizeof(String)+16) || memcmp(oldbefore, old, sizeof(Array)+24))
            exportfail(3, 24, mode, sa, da, 0, __LINE__);
        raised = nil;
        if(setjmp(boundary) == 0)
            cvtca();
        if(raised != why || calls != allocations || live != objects || memcmp(before, slots, sizeof(slots)) ||
           memcmp(&saved, &R, sizeof(R)) || memcmp(sourcebefore, text, sizeof(String)+16) ||
           memcmp(oldbefore, old, sizeof(Array)+24))
            exportfail(3, 24, mode, sa, da, 0, __LINE__);
        *D2H(text) = sourceheap; *D2H(old) = oldheap; *old = oldview;
        D2H(backing)->ref = 1;
        destroy(text); destroy(old); destroy(backing);
        total++;
    }
    printf("PASS CVTCA owner/descriptor spans: %lu error/publication cases\n", total);
}


/* Empty exports update metadata without borrowing even a poisoned data pointer. */
static void
emptyguards(Module *module)
{
    String *text, *input;
    Array *old, *result;
    uchar *data;
    int mode, nilinput, allocations;

    for(mode = 0; mode < 3; mode++)
    for(nilinput = 0; nilinput < 2; nilinput++){
        text = newstring(0); old = mem2array(nil, 16); data = old->data;
        old->data = mode == 0 ? nil : mode == 1 ? H : (uchar*)(uintptr)-2;
        input = nilinput ? H : text; result = old; R.s = &input; R.d = &result;
        allocations = calls;
        if(module->jit->entry[ICVTCA](&R) != 0 || old->len != 16 || result != old)
            abort();
        cvtca();
        if(result != old || old->len != 0 || calls != allocations)
            abort();
        old->data = data; destroy(text); destroy(old);
    }
    printf("PASS CVTCA empty spans: 6 metadata-only cases\n");
}


/* Compare native reuse and C export with independent bytes, Rune and ownership oracles. */
int
main(void)
{
    int lengths[] = {0, 1, 2, 7, 31, 128, 255, 256, 257, 511};
    int capacities[] = {0, 7, 256, 512};
    Rune runes[] = {0, 0x7f, 0x80, 0x20ac, 0x1f642, 0xd800, 0x110000, 0xffffffff};
    Module module = {0};
    Inst instruction = {0};
    String *text;
    Array *old, *result, *backing;
    void *input;
    Heap sourceheap, oldheap;
    uchar sourcebefore[4096], sourceexpected[4096], oldbefore[2048], oldexpected[2048];
    uchar slots[128], slotbefore[128], slotexpected[128], captured[2048], encoded[4];
    REG saved;
    WORD count;
    char *why;
    int ni, ci, profile, sa, da, failed, n, cap, i, nb, sourcelen, sourcesize, oldsize;
    int reuse, native, invalid, beforecalls, beforelive, status, shared, flags;
    unsigned long total;

    Tbyte.size = 1;
    instruction.op = ICVTCA; module.prog = &instruction; module.nprog = 1;
    if(!compile(&module, 1, nil) || module.jit == nil || module.jit->entry[ICVTCA] == nil)
        abort();
    if(!getenv("STRING_BYTES_NO_ERRORS"))
        spanerrors(&module);
    emptyguards(&module);
    total = 0;
    for(ni = 0; ni < sizeof(lengths)/sizeof(lengths[0]); ni++)
    for(ci = 0; ci < sizeof(capacities)/sizeof(capacities[0]); ci++)
    for(profile = 0; profile < 16; profile++)
    for(sa = 0; sa < 8; sa++)
    for(da = 0; da < 8; da++)
    for(failed = 0; failed < 2; failed++){
        if(getenv("STRING_BYTES_NO_ERRORS") && (failed || profile == 5 || profile == 6 || profile == 7 || profile == 9))
            continue;
        n = lengths[ni]; cap = capacities[ci];
        if(profile >= 14 && cap < n)
            continue;
        failure = -1;
        text = newstring(3072); text->len = n;
        old = mem2array(nil, 1024); old->len = cap;
        backing = H; input = text; invalid = 0; shared = 0; why = exHeap;
        switch(profile){
        case 1: D2H(old)->ref = 2; shared = 1; break;
        case 2:
            backing = mem2array(nil, 1); D2H(backing)->ref++;
            old->root = backing;
            break;
        case 3: destroy(old); old = (Array*)newstring(1024); break;
        case 4: text->len = -n; break;
        case 5: D2H(text)->ref = 0; invalid = 1; break;
        case 6: old->root = nil; invalid = 1; break;
        case 7: input = nil; invalid = 1; why = exNilref; break;
        case 8: input = H; break;
        case 9: destroy(old); old = nil; invalid = 1; why = exNilref; break;
        case 10: destroy(old); old = H; break;
        case 11: old->data = (uchar*)text->Sascii-3; break;
        case 12: old->data = (uchar*)text->Sascii+3; break;
        case 13: old->data = (uchar*)text->Sascii; break;
        }
        sourcelen = 0;
        for(i = 0; i < n; i++){
            if(profile == 4){
                text->Srune[i] = runes[i%(sizeof(runes)/sizeof(runes[0]))];
                nb = reference(text->Srune[i], encoded);
                memmove(captured+sourcelen, encoded, nb); sourcelen += nb;
            }else{
                text->Sascii[i] = profile == 11 && i < 3 ? 0 : (i*37+sa+da)%256;
                captured[sourcelen++] = text->Sascii[i];
            }
        }
        memset(slots, 0x5a, sizeof(slots));
        R.s = profile == 15 ? old->data+5 : slots+sa;
        R.d = profile == 14 ? old->data+5 : slots+80+da;
        memmove(R.s, &input, sizeof(input)); memmove(R.d, &old, sizeof(old)); saved = R;
        sourcesize = hmsize(D2H(text))-sizeof(Heap);
        oldsize = old != H && old != nil ? hmsize(D2H(old))-sizeof(Heap) : 0;
        sourceheap = *D2H(text);
        if(oldsize) oldheap = *D2H(old);
        memmove(sourcebefore, text, sourcesize); memmove(sourceexpected, text, sourcesize);
        if(oldsize){memmove(oldbefore, old, oldsize); memmove(oldexpected, old, oldsize);}
        memmove(slotbefore, slots, sizeof(slots)); memmove(slotexpected, slots, sizeof(slots));
        reuse = !invalid && old != H && old != nil && profile != 1 && profile != 2 && profile != 3 &&
                (profile != 4 || n == 0) && (input == H || cap >= n);
        native = reuse && input != H && 1 <= n && n <= 256;
        if(native){
            if(profile == 11 || profile == 12 || profile == 13)
                memmove(sourceexpected+(old->data-(uchar*)text), captured, n);
            else
                memmove(oldexpected+(old->data-(uchar*)old), captured, n);
            count = n; memmove(oldexpected+offsetof(Array, len), &count, sizeof(count));
            if(profile == 14) memmove(oldexpected+(old->data-(uchar*)old)+5, &old, sizeof(old));
            else memmove(slotexpected+80+da, &old, sizeof(old));
        }
        beforecalls = calls; beforelive = live;
        fesetround(FE_DOWNWARD); feclearexcept(FE_ALL_EXCEPT); feraiseexcept(FE_DIVBYZERO);
        flags = fetestexcept(FE_ALL_EXCEPT);
        status = module.jit->entry[ICVTCA](&R);
        if(status != native || calls != beforecalls || live != beforelive || memcmp(&saved, &R, sizeof(R)) ||
           memcmp(slots, slotexpected, sizeof(slots)) || memcmp(text, sourceexpected, sourcesize) ||
           memcmp(D2H(text), &sourceheap, sizeof(sourceheap)) ||
           (oldsize && (memcmp(old, oldexpected, oldsize) || memcmp(D2H(old), &oldheap, sizeof(oldheap)))) ||
           fegetround() != FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT) != flags)
            exportfail(n, cap, profile, sa, da, failed, __LINE__);
        memmove(text, sourcebefore, sourcesize);
        if(oldsize) memmove(old, oldbefore, oldsize);
        memmove(slots, slotbefore, sizeof(slots)); R = saved;
        raised = nil; failure = failed ? 0 : -1;
        if(setjmp(boundary) == 0)
            cvtca();
        failure = -1;
        if(invalid || (failed && !reuse)){
            if(raised != (invalid ? why : exNomem) || calls != beforecalls || live != beforelive ||
               memcmp(slots, slotbefore, sizeof(slots)) || memcmp(text, sourcebefore, sourcesize) ||
               memcmp(D2H(text), &sourceheap, sizeof(sourceheap)) ||
               (oldsize && (memcmp(old, oldbefore, oldsize) || memcmp(D2H(old), &oldheap, sizeof(oldheap)))))
                exportfail(n, cap, profile, sa, da, failed, __LINE__);
            result = old;
        }else{
            if(raised != nil) exportfail(n, cap, profile, sa, da, failed, __LINE__);
            memmove(&result, R.d, sizeof(result));
            if(reuse){
                int exported = input == H ? 0 : n;
                memmove(sourceexpected, sourcebefore, sourcesize); memmove(oldexpected, oldbefore, oldsize);
                if(exported){
                    if(profile == 11 || profile == 12 || profile == 13)
                        memmove(sourceexpected+(old->data-(uchar*)text), captured, exported);
                    else memmove(oldexpected+(old->data-(uchar*)old), captured, exported);
                }
                count = exported; memmove(oldexpected+offsetof(Array, len), &count, sizeof(count));
                if(profile == 14) memmove(oldexpected+(old->data-(uchar*)old)+5, &old, sizeof(old));
                if(result != old || calls != beforecalls || live != beforelive || memcmp(old, oldexpected, oldsize) ||
                   memcmp(text, sourceexpected, sourcesize))
                    exportfail(n, cap, profile, sa, da, failed, __LINE__);
            }else if(result == old || result == H || calls != beforecalls+1 ||
                     result->len != (input == H ? 0 : sourcelen) ||
                     (input != H && sourcelen && memcmp(result->data, captured, sourcelen)))
                exportfail(n, cap, profile, sa, da, failed, __LINE__);
        }
        if(memcmp(&saved, &R, sizeof(R)) || fegetround() != FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT) != flags)
            exportfail(n, cap, profile, sa, da, failed, __LINE__);
        D2H(text)->ref = 1;
        if(invalid && profile == 6) old->root = H;
        destroy(text);
        if(shared && result != old) destroy(old);
        if(shared && result == old) D2H(old)->ref = 1;
        if(result != nil) destroy(result);
        destroy(backing);
        if(live != 0 || Tstring.ref != 1 || Tarray.ref != 1 || Tbyte.ref != 1)
            exportfail(n, cap, profile, sa, da, failed, __LINE__);
        total++;
    }
    module.jit->freecode(module.jit->code, module.jit->size); free(module.jit);
    printf("PASS CVTCA C/RX: %lu owning/reuse/Rune/overlap/failure/FP cases WORD%zu\n", total, sizeof(WORD)*8);
    return 0;
}
