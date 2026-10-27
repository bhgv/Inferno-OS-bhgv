static void *alternate; // Borrowed result cell must never receive a VM owner.
static WORD borrowed; // Temporary decoded inputs owned by the allocation callback.
static unsigned long callbacks; // Number of bounded nheap callback invocations.
int mutator, nprop; // Collector identities used by actual native publication.


/* Keep all ownership and text predicates active in release builds. */
static void
require(int condition)
{
    if(!condition)
        abort();
}


/* Supply bounded storage to the real nheap without duplicating its monitor behavior. */
void*
poolalloc(Pool *pool, ulong size)
{
    (void)pool;
    require(size >= sizeof(Heap));
    return codecstorage(size-sizeof(Heap));
}


/* Invoke the borrowing hook at the actual production nheap monitor boundary. */
static void
codecmonitor(int event, void *header, ulong size)
{
    (void)event;
    (void)header;
    (void)size;
    if(allocationhook != nil)
        allocationhook();
}


/* Allocate native publication metadata outside the observed VM heap. */
void*
mallocz(ulong size, int clear)
{
    void *p;

    p = malloc(size);
    if(p != nil && clear)
        memset(p, 0, size);
    return p;
}


/* Real compiled entries are invoked below; publication itself does not throw. */
int
kprotect(int (*fn)(void*), void *arg)
{
    return fn(arg);
}


/* Borrow decoded registers while preserving the original operand storage. */
static void
redirect(void)
{
    callbacks++;
    R.s = &borrowed;
    R.m = &borrowed;
    R.d = &alternate;
}


/* Compare the actual C path, decoder fallback and complete JIT bridge. */
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


/* Create aligned byte or Rune sources without sharing conversion logic. */
static String*
text(int wide, const char *ascii)
{
    String *s;
    int i, n;

    n = strlen(ascii);
    s = wide ? newrunes(n+8) : newstring(n+8);
    s->len = wide ? -n : n;
    for(i = 0; i < n; i++){
        if(wide)
            s->Srune[i] = ascii[i];
        else
            s->Sascii[i] = ascii[i];
    }
    return s;
}


/* Complete every allocating operation before publishing to its original cell. */
static unsigned long
allocations(Jit *jit)
{
    int ops[] = {ICVTWC, ICVTLC, ICVTFC, ICVTCA, ICVTAC, IADDC, ISLICEC};
    String *left, *right, *oldtext, *resulttext;
    Array *input, *oldarray, *resultarray;
    void *old, *result, *slot;
    WORD word, first, last;
    LONG big;
    REAL real;
    uchar cells[128], expected[128];
    REG wanted;
    int oi, wide, shared, sa, ma, da, monitor, route, beforecalls, i;
    unsigned long total = 0;

    for(oi = 0; oi < 7; oi++)
    for(wide = 0; wide < 2; wide++)
    for(shared = 0; shared < 2; shared++)
    for(sa = 0; sa < 8; sa++)
    for(ma = 0; ma < 8; ma++)
    for(da = 0; da < 8; da++)
    for(monitor = 0; monitor < 2; monitor++)
    for(route = 0; route < 3; route++){
        if(oi == 6 && !shared)
            continue;
        if(getenv("STRING_CAPTURE_OP") && oi != atoi(getenv("STRING_CAPTURE_OP")))
            continue;
        if(getenv("STRING_CAPTURE_CALLBACK_ONLY") && !monitor)
            continue;
        allocationhook = nil;
        left = right = oldtext = H;
        input = oldarray = H;
        old = H;
        word = -123;
        big = -9223372036854775807LL-1;
        real = 2.0;
        first = 1;
        last = 4;
        if(oi == 3){
            left = text(wide, "abcdef");
            if(shared){
                oldarray = mem2array(nil, 2);
                old = oldarray;
            }
        }else if(oi == 4)
            input = mem2array("abcdef", 6);
        else if(oi == 5){
            left = text(wide, "foo");
            right = text(wide, "bar");
        }
        if(shared && oi != 3){
            oldtext = text(wide, "abcdef");
            old = oldtext;
        }
        if(old != H)
            D2H(old)->ref++;
        memset(cells, 0xa5, sizeof(cells));
        memset(&R, 0xa5, sizeof(R));
        R.s = cells+sa;
        R.m = cells+40+ma;
        R.d = cells+80+da;
        if(oi == 0)
            memmove(R.s, &word, sizeof(word));
        else if(oi == 1)
            memmove(R.s, &big, sizeof(big));
        else if(oi == 2)
            memmove(R.s, &real, sizeof(real));
        else if(oi == 3)
            memmove(R.s, &left, sizeof(left));
        else if(oi == 4)
            memmove(R.s, &input, sizeof(input));
        else if(oi == 5){
            memmove(R.s, &right, sizeof(right));
            memmove(R.m, &left, sizeof(left));
        }else{
            memmove(R.s, &first, sizeof(first));
            memmove(R.m, &last, sizeof(last));
        }
        memmove(R.d, &old, sizeof(old));
        memmove(expected, cells, sizeof(cells));
        slot = R.d;
        wanted = R;
        callbacks = 0;
        alternate = H;
        beforecalls = calls;
        if(monitor){
            allocationhook = redirect;
            wanted.s = &borrowed;
            wanted.m = &borrowed;
            wanted.d = &alternate;
        }
        execute(jit, ops[oi], route);
        allocationhook = nil;
        memmove(&result, slot, sizeof(result));
        require(result != old && result != H && result != nil);
        require(calls == beforecalls+1 && callbacks == monitor && alternate == H);
        require(!memcmp(&R, &wanted, sizeof(R)));
        if(oi == 3){
            resultarray = result;
            require(resultarray->len == 6 && !memcmp(resultarray->data, "abcdef", 6));
        }else{
            const char *expectedtext;
            int length, runes;

            expectedtext = oi == 0 ? "-123" : oi == 1 ? "-9223372036854775808" :
                           oi == 2 ? "2" : oi == 6 ? "bcd" : "abcdef";
            if(oi == 5)
                expectedtext = "foobar";
            length = strlen(expectedtext);
            runes = wide && (oi == 5 || oi == 6);
            resulttext = result;
            require(resulttext->len == (runes ? -length : length));
            for(i = 0; i < length; i++)
                require((runes ? resulttext->Srune[i] : (uchar)resulttext->Sascii[i]) == expectedtext[i]);
        }
        memmove(expected+80+da, &result, sizeof(result));
        require(!memcmp(cells, expected, sizeof(cells)));
        destroy(result);
        if(old != H)
            destroy(old);
        if(left != H)
            destroy(left);
        if(right != H)
            destroy(right);
        if(input != H)
            destroy(input);
        require(live == 0 && Tstring.ref == 1 && Tarray.ref == 1 && Tbyte.ref == 1);
        total++;
    }
    return total;
}


/* Restore output cells which overlap integer/real formatted payload bytes. */
static unsigned long
aliases(Jit *jit)
{
    int ops[] = {ICVTWC, ICVTLC, ICVTFC};
    String *old, *result;
    WORD word = -123;
    LONG big = -9223372036854775807LL-1;
    REAL real = 2.0;
    uchar expected[128];
    void *slot;
    int oi, wide, offset, route, beforecalls;
    unsigned long total = 0;

    if(getenv("STRING_CAPTURE_OP"))
        return 0;
    for(oi = 0; oi < 3; oi++)
    for(wide = 0; wide < 2; wide++)
    for(offset = 0; offset < 24; offset++)
    for(route = 0; route < 3; route++){
        old = wide ? newrunes(32) : newstring(32);
        old->len = wide ? -7 : 7;
        memset(old->Sascii, 0x71, wide ? 128 : 32);
        memset(&R, 0xa5, sizeof(R));
        R.s = oi == 0 ? (void*)&word : oi == 1 ? (void*)&big : (void*)&real;
        slot = old->Sascii+offset;
        R.d = slot;
        memmove(slot, &old, sizeof(old));
        memmove(expected, old->Sascii, wide ? 128 : 32);
        memcpy(expected, oi == 0 ? "-123" : oi == 1 ? "-9223372036854775808" : "2", oi == 0 ? 5 : oi == 1 ? 21 : 2);
        memmove(expected+offset, &old, sizeof(old));
        beforecalls = calls;
        execute(jit, ops[oi], route);
        memmove(&result, slot, sizeof(result));
        require(result == old && calls == beforecalls);
        require(!memcmp(expected, old->Sascii, wide ? 128 : 32));
        require(old->len == (oi == 0 ? 4 : oi == 1 ? 20 : 1));
        destroy(old);
        require(live == 0 && Tstring.ref == 1);
        total++;
    }
    return total;
}


/* Reject decoded cells and formatter Heap/header origins before any mutation. */
static unsigned long
invalid(Jit *jit)
{
    int ops[] = {ICVTWC, ICVTLC, ICVTFC, ICVTCA, ICVTAC};
    String *source, *old, *savedold;
    Array *array;
    WORD word = -123;
    LONG big = 123;
    REAL real = 2.0;
    void *slot, *input, *bad, *poison;
    REG saved;
    uchar before[96], after[96];
    int oi, kind, offset, route, width, failed, beforecalls, beforelive;
    char *expected;
    unsigned long total = 0;

    if(getenv("STRING_CAPTURE_NO_ERRORS"))
        return 0;
    source = text(0, "source");
    array = mem2array("source", 6);
    savedold = text(0, "old");
    for(oi = 0; oi < 5; oi++)
    for(kind = 0; kind < 5; kind++){
        if(kind == 2 && oi >= 3)
            continue;
        if(getenv("STRING_CAPTURE_OP") && oi != atoi(getenv("STRING_CAPTURE_OP")))
            continue;
        if(getenv("STRING_CAPTURE_KIND") && kind != atoi(getenv("STRING_CAPTURE_KIND")))
            continue;
        width = (kind == 0 || kind == 4) && oi == 0 ? sizeof(WORD) : 8;
        for(offset = 0; offset <= width; offset++){
            old = savedold;
            memset(&R, 0xa5, sizeof(R));
            input = oi == 0 ? (void*)&word : oi == 1 ? (void*)&big : oi == 2 ?
                    (void*)&real : oi == 3 ? (void*)&source : (void*)&array;
            slot = &old;
            bad = offset == 0 ? nil : (void*)((uintptr)-offset);
            expected = exHeap;
            if(kind == 0)
                input = bad;
            if(kind == 1)
                slot = bad;
            if(kind == 2){
                old = offset == 0 ? nil : offset == 1 ? (String*)8 : offset == 2 ?
                      (String*)sizeof(Heap) : offset == 3 ? (String*)((uchar*)savedold+1) :
                      (String*)((uintptr)-8);
                if(offset == 0)
                    expected = exNilref;
            }
            poison = (void*)(uintptr)0x123456780000ULL;
            if(kind == 3){
                input = oi >= 3 ? (void*)&poison : poison;
                slot = bad;
            }
            if(kind == 4){
                input = bad;
                old = poison;
            }
            R.s = input;
            R.d = slot;
            saved = R;
            beforecalls = calls;
            beforelive = live;
            memmove(before, savedold, sizeof(String)+4);
            require(jit->entry[ops[oi]](&R) == 0);
            for(route = 0; route < 3; route++){
                R = saved;
                raised = nil;
                failed = 0;
                if(setjmp(boundary) == 0)
                    execute(jit, ops[oi], route);
                else
                    failed = 1;
                memmove(after, savedold, sizeof(String)+4);
                require(failed && raised == expected && calls == beforecalls && live == beforelive);
                require(!memcmp(&R, &saved, sizeof(R)) && !memcmp(before, after, sizeof(String)+4));
                require(D2H(savedold)->ref == 1 && D2H(source)->ref == 1 && D2H(array)->ref == 1);
                total++;
            }
        }
    }
    destroy(source);
    destroy(array);
    destroy(savedold);
    require(live == 0 && Tstring.ref == 1 && Tarray.ref == 1 && Tbyte.ref == 1);
    return total;
}


/* Publish the actual seven complete entries and exercise both allocation domains. */
int
main(void)
{
    int ops[] = {ICVTWC, ICVTLC, ICVTFC, ICVTCA, ICVTAC, IADDC, ISLICEC};
    void (*handlers[])(void) = {cvtwc, cvtlc, cvtfc, cvtca, cvtac, addc, slicec};
    Module module = {0};
    Inst program[7] = {{0}};
    unsigned long created, reused, rejected;
    int i;
    char *section = getenv("STRING_CAPTURE_SECTION");

    heapmonitor = codecmonitor;
    for(i = 0; i < 7; i++){
        program[i].op = ops[i];
        optab[ops[i]] = handlers[i];
    }
    module.prog = program;
    module.nprog = 7;
    require(compile(&module, 7, nil) && module.jit);
    created = section == nil || !strcmp(section, "allocations") ? allocations(module.jit) : 0;
    reused = section == nil || !strcmp(section, "aliases") ? aliases(module.jit) : 0;
    rejected = section == nil || !strcmp(section, "invalid") ? invalid(module.jit) : 0;
    module.jit->freecode(module.jit->code, module.jit->size);
    free(module.jit);
    printf("PASS string captures WORD%zu: %lu allocating, %lu alias reuse, %lu rejected routes\n",
           sizeof(WORD)*8, created, reused, rejected);
    return 0;
}
