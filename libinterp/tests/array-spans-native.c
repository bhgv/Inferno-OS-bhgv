static void *alternate; // Callback result cell must stay unpublished.
static WORD borrowed; // Runtime work may replace both decoded scalar operands.
static unsigned long callbacks; // Number of real allocation monitor invocations.


/* Keep allocation, ownership and byte-image checks active with NDEBUG. */
static void
arraycheck(int condition, int line)
{
    if(!condition){
        fprintf(stderr, "array span check failed at %d\n", line);
        abort();
    }
}


#define require(condition) arraycheck((condition), __LINE__) // Keep every predicate and its source location.


/* Borrow decoded operands without changing their original pointed-to storage. */
static void
redirect(int event, void *header, ulong size)
{
    (void)event;
    (void)header;
    (void)size;
    callbacks++;
    R.s = &borrowed;
    R.m = &borrowed;
    R.d = &alternate;
}


/* Exercise actual C, fast+C and complete JIT through the same decoded operands. */
static void
execute(Jit *jit, int op, int route)
{
    if(route == 0)
        optab[op]();
    else if(route == 1){
        if(!jit->entry[op](&R))
            optab[op]();
    }else
        require(jit->dispatch[op](&R) == 1);
}


/* Compare reuse, maps and captured result cells through the real array allocator. */
static unsigned long
constructors(Jit *jit)
{
    int sizes[] = {0, 1, 7, 8, 15, 16, 31, 64};
    Type type, *table[1];
    Module module;
    Modlink link;
    Array *old, *result;
    uchar cells[128], expected[128], before[256], payload[256];
    void *slot, *nilvalue;
    REG wanted;
    WORD count, index;
    int si, n, mode, clear, sa, da, alias, monitor, route, bytes, reuse, i, op;
    int beforecalls;
    unsigned long checks = 0;

    for(si = 0; si < 8; si++)
    for(n = 0; n < 5; n++)
    for(mode = 0; mode < 4; mode++)
    for(clear = 0; clear < 2; clear++)
    for(sa = 0; sa < 8; sa++)
    for(da = 0; da < 8; da++)
    for(alias = 0; alias < 3; alias++)
    for(monitor = 0; monitor < 2; monitor++)
    for(route = 0; route < 3; route++){
        bytes = sizes[si]*n;
        if(bytes > 256 || (mode == 3 && sizes[si] < sizeof(void*)))
            continue;
        if(alias != 0 && (mode == 2 || mode == 3 || bytes < 16+sa+da))
            continue;
        if(getenv("ARRAY_SPAN_CALLBACK_ONLY") && (!monitor || mode == 0))
            continue;
        memset(&type, 0, sizeof(type));
        memset(&module, 0, sizeof(module));
        memset(&link, 0, sizeof(link));
        type.ref = 1;
        type.size = sizes[si];
        if(mode == 3){
            type.np = 1;
            type.map[0] = 0x80;
        }
        table[0] = &type;
        module.type = table;
        module.ntype = 1;
        link.m = &module;
        heapmonitor = nil;
        old = mode == 2 ? H : H2D(Array*, heaparray(&type, n));
        reuse = mode == 0;
        if(old != H){
            D2H(old)->ref = reuse ? 1 : 2;
            memset(old->data, 0x71, bytes);
            if(mode == 3){
                nilvalue = H;
                for(i = 0; i < n; i++)
                    memmove(old->data+i*type.size, &nilvalue, sizeof(nilvalue));
            }
        }
        memset(cells, 0xa5, sizeof(cells));
        memset(&R, 0xa5, sizeof(R));
        count = n;
        index = 0;
        R.M = &link;
        R.s = cells+sa;
        R.m = cells+32+sa;
        R.d = cells+64+da;
        if(alias == 1)
            R.s = old->data+sa;
        if(alias == 2)
            R.d = old->data+8+da;
        memmove(R.s, &count, sizeof(count));
        memmove(R.m, &index, sizeof(index));
        memmove(R.d, &old, sizeof(old));
        if(old != H)
            memmove(before, old->data, bytes);
        slot = R.d;
        wanted = R;
        alternate = H;
        callbacks = 0;
        beforecalls = calls;
        memmove(expected, cells, sizeof(cells));
        if(monitor){
            heapmonitor = redirect;
            if(!reuse){
                wanted.s = &borrowed;
                wanted.m = &borrowed;
                wanted.d = &alternate;
            }
        }
        op = clear ? INEWAZ : INEWA;
        execute(jit, op, route);
        heapmonitor = nil;
        memmove(&result, slot, sizeof(result));
        require(result != nil && result != H && result->t == &type && result->len == n && result->root == H);
        require(result == old ? reuse : !reuse);
        require(calls == beforecalls+!reuse && callbacks == (monitor && !reuse));
        require(alternate == H && !memcmp(&R, &wanted, sizeof(R)));
        if(reuse){
            if(clear)
                memset(payload, 0, bytes);
            else
                memmove(payload, before, bytes);
            if(alias == 2)
                memmove(payload+8+da, &old, sizeof(old));
            require(!memcmp(result->data, payload, bytes));
        }else{
            for(i = 0; i < bytes; i++)
                require(result->data[i] == (mode == 3 && i%type.size < sizeof(void*) ? 255 : clear ? 0 : 0xa5));
            if(old != H){
                require(D2H(old)->ref == 1);
                if(alias == 2)
                    memmove(before+8+da, &result, sizeof(result));
                require(!memcmp(old->data, before, bytes));
            }
        }
        if(alias != 2)
            memmove(expected+64+da, &result, sizeof(result));
        require(!memcmp(cells, expected, sizeof(cells)));
        destroy(result);
        if(old != H && !reuse)
            destroy(old);
        require(live == 0 && type.ref == 1 && Tarray.ref == 1);
        checks++;
    }
    return checks;
}


/* Publish new shared-root/view slices through the original slot after a C callback. */
static unsigned long
views(Jit *jit)
{
    int widths[] = {0, 1, 7, 8, 17};
    Type type;
    Array *root, *old, *result;
    WORD first, last;
    uchar cells[96], expected[96], *base;
    void *slot;
    REG wanted;
    int wi, view, sa, ma, da, monitor, route, beforecalls;
    unsigned long checks = 0;

    for(wi = 0; wi < 5; wi++)
    for(view = 0; view < 2; view++)
    for(sa = 0; sa < 8; sa++)
    for(ma = 0; ma < 8; ma++)
    for(da = 0; da < 8; da++)
    for(monitor = 0; monitor < 2; monitor++)
    for(route = 0; route < 3; route++){
        heapmonitor = nil;
        memset(&type, 0, sizeof(type));
        type.ref = 1;
        type.size = widths[wi];
        root = H2D(Array*, heaparray(&type, 16));
        base = root->data;
        memset(base, 0x71, 16*type.size);
        D2H(root)->ref++;
        old = root;
        if(view){
            first = 0;
            last = 16;
            R.s = &first;
            R.m = &last;
            R.d = &old;
            slicea();
            D2H(old)->ref++;
        }
        first = 2;
        last = 14;
        memset(cells, 0xa5, sizeof(cells));
        memset(&R, 0xa5, sizeof(R));
        R.s = cells+sa;
        R.m = cells+32+ma;
        R.d = cells+64+da;
        memmove(R.s, &first, sizeof(first));
        memmove(R.m, &last, sizeof(last));
        memmove(R.d, &old, sizeof(old));
        memmove(expected, cells, sizeof(cells));
        slot = R.d;
        wanted = R;
        callbacks = 0;
        alternate = H;
        beforecalls = calls;
        if(monitor){
            heapmonitor = redirect;
            wanted.s = &borrowed;
            wanted.m = &borrowed;
            wanted.d = &alternate;
        }
        execute(jit, ISLICEA, route);
        heapmonitor = nil;
        memmove(&result, slot, sizeof(result));
        require(result != old && result->t == &type && result->root == root);
        require(result->len == 12 && result->data == base+2*type.size);
        require(calls == beforecalls+1 && callbacks == monitor && alternate == H);
        require(!memcmp(&R, &wanted, sizeof(R)));
        memmove(expected+64+da, &result, sizeof(result));
        require(!memcmp(cells, expected, sizeof(cells)));
        if(view)
            destroy(old);
        destroy(result);
        destroy(root);
        require(live == 0 && type.ref == 1 && Tarray.ref == 1);
        checks++;
    }
    return checks;
}


/* Reject each decoded slot family without allocation or header/payload mutation. */
static unsigned long
invalid(Jit *jit)
{
    int ops[] = {INEWA, INEWAZ, ISLICEA, ISLICELA};
    Type type, *table[1];
    Module module;
    Modlink link;
    Array *source, *target;
    WORD left, middle;
    REG saved;
    void *bad;
    uchar before[96], after[96];
    int oi, kind, offset, route, extent, failed, beforecalls;
    unsigned long checks = 0;

    if(getenv("ARRAY_SPAN_NO_ERRORS"))
        return 0;
    memset(&type, 0, sizeof(type));
    memset(&module, 0, sizeof(module));
    memset(&link, 0, sizeof(link));
    type.ref = 1;
    type.size = 1;
    table[0] = &type;
    module.type = table;
    module.ntype = 1;
    link.m = &module;
    source = H2D(Array*, heaparray(&type, 4));
    target = H2D(Array*, heaparray(&type, 8));
    for(oi = 0; oi < 4; oi++)
    for(kind = 0; kind < 3; kind++){
        if(getenv("ARRAY_SPAN_OP") && oi != atoi(getenv("ARRAY_SPAN_OP")))
            continue;
        if(getenv("ARRAY_SPAN_KIND") && kind != atoi(getenv("ARRAY_SPAN_KIND")))
            continue;
        extent = kind == 1 || (kind == 0 && oi == 3) ? sizeof(void*) : sizeof(WORD);
        for(offset = 0; offset <= extent; offset++){
            memset(&R, 0xa5, sizeof(R));
            left = oi == 2 ? 0 : 4;
            middle = oi == 2 ? 4 : 0;
            R.M = &link;
            R.s = oi == 3 ? (void*)&source : (void*)&left;
            R.m = &middle;
            R.d = &target;
            bad = offset == 0 ? nil : (void*)((uintptr)-offset);
            if(kind == 0)
                R.s = bad;
            if(kind == 1)
                R.d = bad;
            if(kind == 2)
                R.m = bad;
            saved = R;
            beforecalls = calls;
            memmove(before, source, sizeof(Array));
            memmove(before+sizeof(Array), target, sizeof(Array));
            require(jit->entry[ops[oi]](&R) == 0);
            for(route = 0; route < 3; route++){
                R = saved;
                raised = nil;
                failed = 0;
                if(setjmp(boundary) == 0)
                    execute(jit, ops[oi], route);
                else
                    failed = 1;
                memmove(after, source, sizeof(Array));
                memmove(after+sizeof(Array), target, sizeof(Array));
                require(failed && raised == exHeap && calls == beforecalls && !memcmp(before, after, 2*sizeof(Array)));
                require(!memcmp(&R, &saved, sizeof(R)) && live == 2 && Tarray.ref == 3 && type.ref == 3);
                checks++;
            }
        }
    }
    destroy(source);
    destroy(target);
    require(live == 0 && type.ref == 1 && Tarray.ref == 1);
    return checks;
}


/* Validate exact array/type spans and backing origins before metadata or writes. */
static unsigned long
headers(Jit *jit)
{

    struct Owner {
        Heap heap; // Real backward reference cell used by view preflight.
        Array array; // Complete metadata used by both slice instructions.
    } source, target, backing;
    Type type;
    Array *input, *output;
    WORD first, last;
    REG saved;
    uchar bytes[128], before[128];
    struct Owner oldsource, oldtarget, oldbacking;
    int copy, profile, route, failed, beforecalls;
    char *expected;
    unsigned long checks = 0;

    if(getenv("ARRAY_SPAN_NO_ERRORS"))
        return 0;
    for(copy = 0; copy < 2; copy++)
    for(profile = 0; profile < 14; profile++){
        if(copy && profile >= 10)
            continue;
        if(getenv("ARRAY_SPAN_COPY") && copy != atoi(getenv("ARRAY_SPAN_COPY")))
            continue;
        if(getenv("ARRAY_SPAN_HEADER") && profile != atoi(getenv("ARRAY_SPAN_HEADER")))
            continue;
        memset(&type, 0, sizeof(type));
        memset(&source, 0, sizeof(source));
        memset(&target, 0, sizeof(target));
        memset(&backing, 0, sizeof(backing));
        memset(bytes, 0x71, sizeof(bytes));
        memset(&R, 0xa5, sizeof(R));
        type.ref = 1;
        type.size = 1;
        source.heap.ref = target.heap.ref = backing.heap.ref = 1;
        source.array.len = 4;
        target.array.len = 8;
        source.array.t = target.array.t = &type;
        source.array.root = target.array.root = H;
        source.array.data = bytes;
        target.array.data = bytes+64;
        input = &source.array;
        output = copy ? &target.array : input;
        first = copy ? 1 : 0;
        last = 4;
        expected = exHeap;
        if(profile == 0)
            output = (Array*)((uintptr)-8);
        if(profile == 1)
            output = (Array*)((uchar*)&target.array+1);
        if(profile == 2)
            input = (Array*)((uintptr)-8);
        if(profile == 3)
            input = (Array*)((uchar*)&source.array+1);
        if(profile >= 4 && profile <= 7){
            target.array.t = source.array.t = profile == 4 ? nil : profile == 5 ? H :
                profile == 6 ? (Type*)((uchar*)&type+1) : (Type*)((uintptr)-8);
            expected = exBounds;
        }
        if(profile == 8 || profile == 9){
            output = profile == 8 ? nil : H;
            expected = exNilref;
        }
        if(profile >= 10){
            source.array.root = profile == 10 ? (Array*)8 : profile == 11 ? (Array*)sizeof(Heap) :
                profile == 12 ? (Array*)((uchar*)&backing.array+1) : nil;
            if(profile == 13)
                expected = exNilref;
        }
        if(!copy && (profile == 2 || profile == 3))
            output = input;
        R.s = copy ? (void*)&input : (void*)&first;
        R.m = copy ? (void*)&first : (void*)&last;
        R.d = &output;
        saved = R;
        beforecalls = calls;
        oldsource = source;
        oldtarget = target;
        oldbacking = backing;
        memmove(before, bytes, sizeof(bytes));
        require(jit->entry[copy ? ISLICELA : ISLICEA](&R) == 0);
        for(route = 0; route < 3; route++){
            R = saved;
            failed = 0;
            raised = nil;
            if(setjmp(boundary) == 0)
                execute(jit, copy ? ISLICELA : ISLICEA, route);
            else
                failed = 1;
            require(failed && raised == expected && calls == beforecalls);
            require(!memcmp(&R, &saved, sizeof(R)) && !memcmp(bytes, before, sizeof(bytes)));
            require(!memcmp(&source, &oldsource, sizeof(source)) && !memcmp(&target, &oldtarget, sizeof(target)));
            require(!memcmp(&backing, &oldbacking, sizeof(backing)) && type.ref == 1 && live == 0);
            checks++;
        }
    }
    return checks;
}


/* A nil source consumes neither destination nor index, including their cell addresses. */
static unsigned long
absent(Jit *jit)
{
    Array *source;
    REG saved;
    int sentinel, offset, route;
    unsigned long checks = 0;

    for(sentinel = 0; sentinel < 2; sentinel++)
    for(offset = 0; offset <= sizeof(void*); offset++)
    for(route = 0; route < 3; route++){
        memset(&R, 0xa5, sizeof(R));
        source = sentinel ? H : nil;
        R.s = &source;
        R.d = offset ? (void*)((uintptr)-offset) : nil;
        R.m = R.d;
        saved = R;
        execute(jit, ISLICELA, route);
        require(!memcmp(&R, &saved, sizeof(R)) && live == 0 && Tarray.ref == 1);
        checks++;
    }
    return checks;
}


/* Keep real native entries, heap ownership and both VM WORD widths observable. */
int
main(void)
{
    int ops[] = {INEWA, INEWAZ, ISLICEA, ISLICELA};
    void (*handlers[])(void) = {newa, newaz, slicea, slicela};
    Module module = {0};
    Inst program[4] = {{0}};
    unsigned long constructed = 0, sliced = 0, rejected, bounded, empty;
    int i;
    char *section = getenv("ARRAY_SPAN_SECTION");

    Tarray.free = freearray;
    Tarray.size = sizeof(Array);
    for(i = 0; i < 4; i++){
        program[i].op = ops[i];
        optab[ops[i]] = handlers[i];
    }
    module.prog = program;
    module.nprog = 4;
    require(compile(&module, 4, nil) && module.jit);
    if(!getenv("ARRAY_SPAN_INVALID_ONLY")){
        if(!getenv("ARRAY_SPAN_VIEW_ONLY") && (section == nil || !strcmp(section, "constructors")))
            constructed = constructors(module.jit);
        if(!getenv("ARRAY_SPAN_CONSTRUCTOR_ONLY") && (section == nil || !strcmp(section, "views")))
            sliced = views(module.jit);
    }
    rejected = section == nil || !strcmp(section, "slots") ? invalid(module.jit) : 0;
    bounded = section == nil || !strcmp(section, "headers") ? headers(module.jit) : 0;
    empty = section == nil || !strcmp(section, "absent") ? absent(module.jit) : 0;
    module.jit->freecode(module.jit->code, module.jit->size);
    free(module.jit);
    printf("PASS array spans WORD%zu: %lu constructor, %lu view, %lu slot-error, %lu header-error, %lu nil-source routes\n",
           sizeof(WORD)*8, constructed, sliced, rejected, bounded, empty);
    return 0;
}
