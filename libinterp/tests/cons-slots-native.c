static void *alternate; // Callback output must stay untouched by constructor publication.
static uchar poison[512]; // Borrowed source bytes differ from the captured payload.
static WORD otherindex; // Callback middle operand differs from the decoded size.
static unsigned long callbacks; // Real nheap monitor invocations.


/* Keep every oracle active under NDEBUG. */
static void
require(int condition)
{
    if(!condition)
        abort();
}


/* Model runtime work borrowing all three decoded operand registers. */
static void
redirect(int event, void *header, ulong size)
{
    (void)event;
    (void)header;
    (void)size;
    callbacks++;
    R.s = poison;
    R.m = &otherindex;
    R.d = &alternate;
}


/* Compare real C, guarded native+C and complete JIT allocation and publication. */
static unsigned long
positive(Jit *jit)
{
    int ops[] = {ICONSB, ICONSW, ICONSL, ICONSF, ICONSP, ICONSM};
    int sizes[] = {0, 1, 7, 8, 31, 128, 256};
    uchar cells[768], captured[256], expected[768];
    List *old, *result, *sourceowner;
    void *slot;
    REG wanted;
    WORD length;
    int oi, si, sa, da, alias, owner, monitor, route, width, beforecalls;
    unsigned long checks = 0;

    memset(poison, 0x39, sizeof(poison));
    for(oi = 0; oi < 6; oi++)
    for(si = 0; si < (oi == 5 ? 7 : 1); si++)
    for(sa = 0; sa < 8; sa++)
    for(da = 0; da < 8; da++)
    for(alias = 0; alias < 3; alias++)
    for(owner = 0; owner < 2; owner++)
    for(monitor = 0; monitor < 2; monitor++)
    for(route = 0; route < 3; route++){
        if(alias == 2 && !owner)
            continue;
        if(getenv("CONS_SLOT_CALLBACK_ONLY") && (oi != 5 || !monitor))
            continue;
        width = oi == 0 ? 1 : oi == 1 ? sizeof(WORD) : oi == 5 ? sizes[si] : 8;
        if(alias == 2 && width > 32-sa)
            continue;
        heapmonitor = nil;
        old = H;
        if(owner){
            cons(32, &old);
            memset(old->data, 0x71, 32);
            D2H(old)->ref = 2;
        }
        memset(cells, 0x6d, sizeof(cells));
        memset(&R, 0xa5, sizeof(R));
        R.s = cells+sa;
        R.m = &length;
        R.d = cells+384+da;
        if(alias == 1)
            R.s = R.d;
        if(alias == 2)
            R.s = (uchar*)old->data+sa;
        sourceowner = H;
        if(oi == 4)
            memmove(R.s, &sourceowner, sizeof(sourceowner));
        memmove(R.d, &old, sizeof(old));
        memmove(captured, R.s, width);
        memmove(expected, cells, sizeof(cells));
        length = width;
        slot = R.d;
        wanted = R;
        alternate = H;
        callbacks = 0;
        beforecalls = calls;
        if(monitor){
            heapmonitor = redirect;
            wanted.s = poison;
            wanted.m = &otherindex;
            wanted.d = &alternate;
        }
        if(route == 0)
            optab[ops[oi]]();
        else if(route == 1){
            if(!jit->entry[ops[oi]](&R))
                optab[ops[oi]]();
        }else
            require(jit->dispatch[ops[oi]](&R) == 1);
        heapmonitor = nil;
        memmove(&result, slot, sizeof(result));
        require(result != old && result != H && result != nil);
        require(result->tail == old && !memcmp(result->data, captured, width));
        require(result->t == (oi == 4 ? &Tptr : nil));
        require(calls == beforecalls+1 && callbacks == monitor && alternate == H);
        require(!memcmp(&R, &wanted, sizeof(R)));
        memmove(expected+384+da, &result, sizeof(result));
        require(!memcmp(cells, expected, sizeof(cells)));
        destroy(result);
        if(old != H)
            destroy(old);
        require(live == 0 && Tlist.ref == 1 && Tptr.ref == 1);
        checks++;
    }
    return checks;
}


/* Decline bad decoded cells before allocation, then report the real C error. */
static unsigned long
invalid(Jit *jit)
{
    int ops[] = {ICONSB, ICONSW, ICONSL, ICONSF, ICONSP, ICONSM};
    uchar value[16];
    WORD length = 8;
    List *out = H;
    REG saved;
    void *bad;
    int oi, kind, offset, route, extent, beforecalls, failed;
    unsigned long checks = 0;

    if(getenv("CONS_SLOT_NO_ERRORS"))
        return 0;
    for(oi = 0; oi < 6; oi++)
    for(kind = 0; kind < 3; kind++){
        if(kind == 2 && oi != 5)
            continue;
        if(getenv("CONS_SLOT_OP") && oi != atoi(getenv("CONS_SLOT_OP")))
            continue;
        if(getenv("CONS_SLOT_KIND") && kind != atoi(getenv("CONS_SLOT_KIND")))
            continue;
        extent = kind == 2 ? sizeof(WORD) : kind == 1 ? sizeof(void*) :
                 oi == 0 ? 1 : oi == 1 ? sizeof(WORD) : 8;
        for(offset = 0; offset <= extent; offset++){
            memset(&R, 0xa5, sizeof(R));
            memset(value, 0xff, sizeof(value));
            R.s = value;
            R.m = &length;
            R.d = &out;
            bad = offset == 0 ? nil : (void*)((uintptr)-offset);
            if(kind == 0)
                R.s = bad;
            if(kind == 1)
                R.d = bad;
            if(kind == 2)
                R.m = bad;
            saved = R;
            beforecalls = calls;
            require(jit->entry[ops[oi]](&R) == 0);
            require(!memcmp(&R, &saved, sizeof(R)) && calls == beforecalls && out == H);
            for(route = 0; route < 3; route++){
                R = saved;
                raised = nil;
                failed = 0;
                if(setjmp(boundary) == 0){
                    if(route == 0)
                        optab[ops[oi]]();
                    else if(route == 1){
                        if(!jit->entry[ops[oi]](&R))
                            optab[ops[oi]]();
                    }else
                        jit->dispatch[ops[oi]](&R);
                }else
                    failed = 1;
                require(failed && raised == exHeap && calls == beforecalls && out == H);
                require(!memcmp(&R, &saved, sizeof(R)) && live == 0 && Tlist.ref == 1 && Tptr.ref == 1);
                checks++;
            }
        }
    }
    return checks;
}


/* Compile every affected constructor and retain actual complete-dispatch entries. */
int
main(void)
{
    int ops[] = {ICONSB, ICONSW, ICONSL, ICONSF, ICONSP, ICONSM};
    void (*handlers[])(void) = {consb, consw, consl, consf, consp, consm};
    Module module = {0};
    Inst program[6] = {{0}};
    unsigned long successful, rejected;
    int i;

    for(i = 0; i < 6; i++){
        program[i].op = ops[i];
        optab[ops[i]] = handlers[i];
    }
    module.prog = program;
    module.nprog = 6;
    require(compile(&module, 6, nil) && module.jit);
    successful = getenv("CONS_SLOT_INVALID_ONLY") ? 0 : positive(module.jit);
    rejected = invalid(module.jit);
    module.jit->freecode(module.jit->code, module.jit->size);
    free(module.jit);
    printf("PASS constructor cells WORD%zu: %lu captured allocation routes, %lu rejected routes\n",
           sizeof(WORD)*8, successful, rejected);
    return 0;
}
