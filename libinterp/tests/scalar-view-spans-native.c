

/* Compare actual C instructions with published x64 code, including native-span errors. */
int
main(void)
{
    void (*ops[])(void) = {headb, headw, headl, headf, lenl, lenc};
    int opcodes[] = {IHEADB, IHEADW, IHEADL, IHEADF, ILENL, ILENC};
    int widths[] = {sizeof(BYTE), sizeof(WORD), sizeof(LONG), sizeof(REAL), sizeof(void*), sizeof(int)};
    int lengths[] = {0, 1, -7, -0x7fffffff-1, 0x7fffffff};
    Module module = {0};
    Inst instructions[6] = {0};
    union {
        uvlong alignment; // Align ordinary owner storage while varying operand alignment.
        uchar bytes[512]; // Two independent list/string owner images.
    } nodes;
    uchar nodebefore[512], nodeexpected[512], slots[128], slotexpected[128];
    void *source, *owner, *next;
    uintptr offset, extent;
    REG saved;
    WORD result;
    int kind, profile, value, sa, da, alias, chain, pass, valid, status;
    unsigned long checks;

    for(kind = 0; kind < 6; kind++)
        instructions[kind].op = opcodes[kind];
    module.prog = instructions;
    module.nprog = 6;
    if(!compile(&module, 6, nil) || module.jit == nil)
        abort();
    for(kind = 0; kind < 6; kind++)
        if(module.jit->entry[opcodes[kind]] == nil)
            abort();
    checks = 0;
    for(kind = 0; kind < 6; kind++)
    for(profile = 0; profile < 5; profile++)
    for(value = 0; value < 5; value++)
    for(sa = 0; sa < 8; sa++)
    for(da = 0; da < 8; da++)
    for(alias = 0; alias < 3; alias++)
    for(chain = 0; chain <= (kind == 4); chain++){
        memset(nodes.bytes, 0xa5, sizeof(nodes.bytes));
        memset(slots, 0x5a, sizeof(slots));
        offset = kind < 4 ? offsetof(List, data) : kind == 4 ? offsetof(List, tail) : offsetof(String, len);
        extent = offset+widths[kind];
        owner = profile == 0 ? nil : profile == 1 ? H : profile == 2 ? nodes.bytes :
                (void*)((uintptr)-1-extent+(profile == 3 ? 1 : 2));
        if(kind == 4){
            next = H;
            memmove(nodes.bytes+offset, &next, sizeof(next));
        }else if(kind == 5)
            memmove(nodes.bytes+offset, &lengths[value], sizeof(int));
        else if(value == 1)
            memset(nodes.bytes+offset, 0, widths[kind]);
        else if(value == 2)
            memset(nodes.bytes+offset, 0xff, widths[kind]);
        source = owner;
        if(chain){
            memmove(nodes.bytes+256+offset, &owner, sizeof(owner));
            source = nodes.bytes+256;
        }
        memmove(slots+sa, &source, sizeof(source));
        R.s = slots+sa;
        R.d = alias == 0 ? slots+80+da : alias == 1 ? slots+sa : nodes.bytes+offset+da;
        saved = R;
        memmove(nodebefore, nodes.bytes, sizeof(nodebefore));
        memmove(nodeexpected, nodes.bytes, sizeof(nodeexpected));
        memmove(slotexpected, slots, sizeof(slotexpected));
        valid = (profile == 1 && kind >= 4) || profile == 2;
        if(kind == 5 && profile == 2 && value == 3)
            valid = 0;
        if(valid){
            if(kind < 4){
                if(alias == 2)
                    memmove(nodeexpected+offset+da, nodes.bytes+offset, widths[kind]);
                else
                    memmove(slotexpected+(alias == 0 ? 80+da : sa), nodes.bytes+offset, widths[kind]);
            }else{
                result = kind == 4 ? (profile == 1 ? chain : chain+1) : profile == 1 ? 0 :
                         lengths[value] < 0 ? -lengths[value] : lengths[value];
                if(alias == 2)
                    memmove(nodeexpected+offset+da, &result, sizeof(result));
                else
                    memmove(slotexpected+(alias == 0 ? 80+da : sa), &result, sizeof(result));
            }
        }
        for(pass = 0; pass < 2; pass++){
            memmove(nodes.bytes, nodebefore, sizeof(nodebefore));
            memset(slots, 0x5a, sizeof(slots));
            memmove(slots+sa, &source, sizeof(source));
            R = saved;
            raised = nil;
            if(pass == 0){
                status = module.jit->entry[opcodes[kind]](&R);
                if(status != valid)
                    abort();
            }else{
                if(setjmp(boundary) == 0)
                    ops[kind]();
                if(valid ? raised != nil : raised == nil)
                    abort();
                if(!valid && raised != (profile == 0 || (profile == 1 && kind < 4) ? exNilref : kind == 5 ? exHeap : exBounds))
                    abort();
            }
            if(memcmp(&saved, &R, sizeof(R)) || memcmp(nodes.bytes, nodeexpected, sizeof(nodeexpected)) ||
               memcmp(slots, slotexpected, sizeof(slots)))
                abort();
        }
        checks++;
    }
    module.jit->freecode(module.jit->code, module.jit->size);
    free(module.jit);
    printf("PASS C/RX scalar view spans: %lu checks WORD%lu\n", checks, (ulong)(sizeof(WORD)*8));
    return 0;
}
