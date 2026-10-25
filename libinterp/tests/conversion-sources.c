

/* Identify the source profile and failed check before stopping a regression process. */
static void
conversionfail(int op, int mode, int line)
{
    fprintf(stderr, "conversion source failure: op=%d mode=%d line=%d\n", op, mode, line);
    abort();
}


/* Exercise the public byte-string constructor independently of VM array metadata. */
static unsigned long
directspans(void)
{
    static String *converted;
    static char *bytes;
    int mode, count, valid, allocations;
    char *why;

    for(mode = 0; mode < 11; mode++){
        bytes = "abc"; count = 3; valid = 0; why = exHeap; converted = H;
        switch(mode){
        case 0: bytes = (char*)(uintptr)-2; break;
        case 1: bytes = (char*)(uintptr)-3; count = 4; break;
        case 2: bytes = (char*)(uintptr)-2; count = 0; valid = 1; break;
        case 3: bytes = nil; why = exNilref; break;
        case 4: bytes = H; why = exNilref; break;
        case 5: count = -1; why = exNegsize; break;
        case 6: count = 0x7fffffff; break;
        case 7: valid = 1; break;
        case 8: bytes = H; count = 0; valid = 1; break;
        case 9: bytes = nil; count = 0; valid = 1; break;
        case 10: count = 0; valid = 1; break;
        }
        allocations = calls; raised = nil;
        if(setjmp(boundary) == 0){
            converted = c2string(bytes, count);
            if(!valid) conversionfail(2, mode, __LINE__);
        }else if(valid || raised != why) conversionfail(2, mode, __LINE__);
        if(valid){
            if(converted == H || converted->len != count || calls != allocations+1 ||
               (count && memcmp(converted->Sascii, "abc", count))) conversionfail(2, mode, __LINE__);
            destroy(converted);
        }else if(converted != H || calls != allocations) conversionfail(2, mode, __LINE__);
        if(live || Tstring.ref != 1) conversionfail(2, mode, __LINE__);
    }
    return 11;
}


/* Check real conversion source ownership, spans and write-free allocation failure. */
int
main(void)
{
    static Array *array, *backing, *oldarray, *arrayresult;
    static String *text, *oldtext, *textresult;
    static Array arraysaved, backsaved;
    static Heap arrayheap, backheap, textheap;
    static uchar textsaved[64], slotsbefore[128], expected[128];
    static Type custom;

    union {
        uvlong alignment; // Native alignment for the independent slot buffer.
        uchar bytes[128]; // All unaligned input/output addresses and surrounding canaries.
    } slots;
    REG saved;
    void *input, *output;
    uchar *data;
    char *why;
    int op, mode, sa, da, invalid, allocations, beforelive, length, totalt, totala, totalb;
    unsigned long total = 0;
    char *selection = getenv("CONVERSION_MODE");

    if(selection && !((atoi(selection) >= 0 && atoi(selection) < 24) ||
       (atoi(selection) >= 100 && atoi(selection) < 118) || atoi(selection) == 200))
        conversionfail(3, atoi(selection), __LINE__);
    for(op = 0; op < 2; op++) for(mode = 0; mode < (op ? 18 : 24); mode++)
    for(sa = 0; sa < 8; sa++) for(da = 0; da < 8; da++){
        if(selection && op*100+mode != atoi(selection)) continue;
        array = mem2array("abc", 3); backing = mem2array("xyz", 3);
        oldarray = mem2array("keep", 4);
        text = c2string("abc", 3); oldtext = c2string("keep", 4);
        data = array->data;
        custom = Tbyte; custom.size = 1; custom.np = 0;
        why = exHeap; invalid = 0; length = 3;
        if(op == 0){
            input = array; output = oldtext;
            switch(mode){
            case 1: D2H(array)->ref = 0; invalid = 1; break;
            case 2: array->root = nil; invalid = 1; break;
            case 3: array->root = backing; D2H(backing)->ref = 0; invalid = 1; break;
            case 4: array->data = (uchar*)(uintptr)-2; invalid = 1; break;
            case 5: array->data = nil; invalid = 1; why = exNilref; break;
            case 6: array->data = H; invalid = 1; why = exNilref; break;
            case 7: array->len = length = 0; array->data = nil; break;
            case 8: array->len = length = 0; array->data = H; break;
            case 9: D2H(array)->ref = (ulong)~0; break;
            case 10: array->root = backing; array->data = backing->data; D2H(backing)->ref = (ulong)~0; break;
            case 11: array->len = -1; invalid = 1; break;
            case 12: array->len = (WORD)(((UWORD)~0)>>1); invalid = 1; break;
            case 13: array->t = &custom; break;
            case 14: array->t = &custom; custom.size = 2; invalid = 1; why = exType; break;
            case 15: array->t = &custom; custom.np = 1; invalid = 1; why = exType; break;
            case 16: input = H; length = 0; break;
            case 17: input = nil; invalid = 1; why = exNilref; break;
            case 18: array->len = 0; D2H(array)->ref = 0; invalid = 1; break;
            case 19: array->len = 0; array->root = nil; invalid = 1; break;
            case 20: array->len = length = 0; array->data = (uchar*)(uintptr)-2; break;
            case 21: array->root = backing; array->data = backing->data; break;
            case 22: array->t = (Type*)(uintptr)1; D2H(array)->ref = 0; invalid = 1; break;
            case 23: array->t = (Type*)(uintptr)1; array->len = 0; D2H(array)->ref = 0; invalid = 1; break;
            }
        }else{
            input = text; output = oldarray;
            switch(mode){
            case 1: D2H(text)->ref = 0; invalid = 1; break;
            case 2: text->len = 0; D2H(text)->ref = 0; invalid = 1; break;
            case 3: text->len = -1; D2H(text)->ref = 0; invalid = 1; break;
            case 4: D2H(text)->t = &Tarray; invalid = 1; why = exType; break;
            case 5: text->len = (-0x7fffffff-1); invalid = 1; break;
            case 6: text->len = 4; invalid = 1; break;
            case 7: input = H; length = 0; break;
            case 8: input = nil; invalid = 1; why = exNilref; break;
            case 9: D2H(text)->ref = (ulong)~0; break;
            case 10: text->len = length = 0; break;
            case 11: text->len = -1; text->max = 0; invalid = 1; break;
            case 12: Tstring.ref = 0x7fffffff; break;
            case 13: Tarray.ref = 0x7fffffff; invalid = 1; break;
            case 14: Tbyte.ref = 0x7fffffff; invalid = 1; break;
            case 15: failure = 0; invalid = 1; why = exNomem; break;
            case 16: text->len = -1; text->Srune[0] = 0x20ac; length = 3; break;
            case 17: text->len = -1; text->Srune[0] = 0x20ac; D2H(text)->ref = (ulong)~0; length = 3; break;
            }
        }
        memset(slots.bytes, 0xa5, sizeof(slots.bytes));
        R.s = slots.bytes+sa; R.d = slots.bytes+80+da;
        memmove(R.s, &input, sizeof(input)); memmove(R.d, &output, sizeof(output)); saved = R;
        arraysaved = *array; backsaved = *backing;
        arrayheap = *D2H(array); backheap = *D2H(backing); textheap = *D2H(text);
        memmove(textsaved, text, sizeof(String)+4);
        memmove(slotsbefore, slots.bytes, sizeof(slotsbefore));
        memmove(expected, slots.bytes, sizeof(expected));
        allocations = calls; beforelive = live;
        totalt = Tstring.ref; totala = Tarray.ref; totalb = Tbyte.ref;
        raised = nil;
        if(setjmp(boundary) == 0){
            if(op) cvtca(); else cvtac();
            if(invalid) conversionfail(op, mode, __LINE__);
        }else if(!invalid || raised != why) conversionfail(op, mode, __LINE__);
        failure = -1;
        if(memcmp(&R, &saved, sizeof(R)) || memcmp(array, &arraysaved, sizeof(Array)) ||
           memcmp(backing, &backsaved, sizeof(Array)) || memcmp(D2H(array), &arrayheap, sizeof(Heap)) ||
           memcmp(D2H(backing), &backheap, sizeof(Heap)) || memcmp(D2H(text), &textheap, sizeof(Heap)) ||
           memcmp(text, textsaved, sizeof(String)+4)) conversionfail(op, mode, __LINE__);
        if(invalid){
            if(calls != allocations || live != beforelive || Tstring.ref != totalt ||
               Tarray.ref != totala || Tbyte.ref != totalb ||
               memcmp(slots.bytes, slotsbefore, sizeof(slots.bytes))) conversionfail(op, mode, __LINE__);
        }else{
            memmove(&output, R.d, sizeof(output));
            memmove(expected+80+da, &output, sizeof(output));
            if(memcmp(slots.bytes, expected, sizeof(expected))) conversionfail(op, mode, __LINE__);
            if(op){
                arrayresult = output;
                if(arrayresult == H || arrayresult->len != length || calls != allocations+1) conversionfail(op, mode, __LINE__);
                if(length && memcmp(arrayresult->data, mode >= 16 ? "\xe2\x82\xac" : "abc", length)) conversionfail(op, mode, __LINE__);
                oldarray = H;
                destroy(arrayresult);
            }else{
                textresult = output;
                if(mode == 16){
                    if(textresult != H || calls != allocations || live != beforelive-1) conversionfail(op, mode, __LINE__);
                }else{
                    if(textresult == H || textresult->len != length || calls != allocations+1) conversionfail(op, mode, __LINE__);
                    if(length && memcmp(textresult->Sascii, mode == 10 || mode == 21 ? "xyz" : "abc", length)) conversionfail(op, mode, __LINE__);
                    destroy(textresult);
                }
                oldtext = H;
            }
        }
        /* Restore deliberate corruptions before releasing the independent fixture owners. */
        array->root = H; array->data = data; array->t = &Tbyte; array->len = 3;
        D2H(array)->ref = D2H(backing)->ref = D2H(text)->ref = 1;
        D2H(text)->t = &Tstring; text->len = 3; text->max = 3;
        if(op && mode == 12) Tstring.ref = 3;
        if(op && mode == 13) Tarray.ref = 4;
        if(op && mode == 14) Tbyte.ref = 4;
        destroy(array); destroy(backing); destroy(text); destroy(oldarray); destroy(oldtext);
        if(live != 0 || Tarray.ref != 1 || Tbyte.ref != 1 || Tstring.ref != 1) conversionfail(op, mode, __LINE__);
        total++;
    }
    if(!selection || atoi(selection) == 200) total += directspans();
    printf("PASS conversion sources: %lu owner/span/publication/cleanup cases WORD%zu\n", total, sizeof(WORD)*8);
    return 0;
}
