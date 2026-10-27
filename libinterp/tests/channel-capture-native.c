static Channel *alternate; // Callback cell which must never receive the new channel.
static WORD borrowed; // Temporary decoded input used only by the callback.
static unsigned long callbacks; // Real channel and array allocation notifications.


/* Borrow decoded registers without changing the captured constructor arguments. */
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


/* Compare all seven constructors through C, native+C and complete JIT dispatch. */
int
main(void)
{
    int ops[] = {INEWCB, INEWCW, INEWCL, INEWCF, INEWCP, INEWCM, INEWCMP};
    void (*handlers[])(void) = {newcb, newcw, newcl, newcf, newcp, newcm, newcmp};
    Type mapped = {1, 0, 0, 16, 1, 0, 0, {0x80}};
    Type *table[] = {&mapped};
    Type *types[] = {&Tbyte, &Tword, &Tlong, &Treal, &Tptr};
    void (*movers[])(void) = {movb, movw, movl, movf, movp, movm, movtmp};
    Module module = {0};
    Modlink link = {0};
    Inst program[7] = {{0}};
    uchar cells[128], expected[128];
    REG wanted;
    Channel *old, *result;
    void *destination;
    WORD source, capacity;
    int oi, len, sa, ma, da, alias, owner, monitor, route, i, count;
    unsigned long checks = 0;

    for(i = 0; i < 7; i++){
        program[i].op = ops[i];
        optab[ops[i]] = handlers[i];
    }
    module.prog = program;
    module.nprog = 7;
    module.type = table;
    module.ntype = 1;
    link.m = &module;
    check(compile(&module, 7, nil) && module.jit);
    for(oi = 0; oi < 7; oi++)
    for(len = 0; len < 4; len++)
    for(sa = 0; sa < 8; sa++)
    for(ma = 0; ma < 8; ma++)
    for(da = 0; da < 8; da++)
    for(alias = 0; alias < 2; alias++)
    for(owner = 0; owner < 2; owner++)
    for(monitor = 0; monitor < 2; monitor++)
    for(route = 0; route < 3; route++){
        if(getenv("CHANNEL_CALLBACK_ONLY") && !monitor)
            continue;
        heapmonitor = nil;
        old = owner ? cnewc(&Tbyte, movb, 0) : H;
        source = oi == 5 ? 13 : 0;
        capacity = len;
        memset(cells, 0xa5, sizeof(cells));
        memset(&R, 0xa5, sizeof(R));
        R.M = &link;
        R.s = cells+sa;
        R.m = cells+32+ma;
        R.d = cells+80+da;
        memmove(R.s, &source, sizeof(source));
        memmove(R.m, &capacity, sizeof(capacity));
        memmove(R.d, &old, sizeof(old));
        if(alias)
            R.m = R.d;
        count = alias ? 0 : len;
        destination = R.d;
        wanted = R;
        alternate = H;
        callbacks = 0;
        memmove(expected, cells, sizeof(cells));
        if(monitor){
            heapmonitor = redirect;
            wanted.s = &borrowed;
            wanted.m = &borrowed;
            wanted.d = &alternate;
        }
        if(route == 0)
            optab[ops[oi]]();
        else if(route == 1){
            if(!module.jit->entry[ops[oi]](&R))
                optab[ops[oi]]();
        }else
            check(module.jit->dispatch[ops[oi]](&R) == 1);
        heapmonitor = nil;
        memmove(&result, destination, sizeof(result));
        check(result != old && result != nil && result != H);
        check(result->mover == movers[oi] && result->front == 0 && result->size == 0);
        check(result->send != nil && result->recv != nil);
        check(count == 0 ? result->buf == H : result->buf != H && result->buf->len == count);
        if(oi == 5)
            check(result->mid.w == source && (count == 0 || result->buf->t->size == source));
        else if(oi == 6)
            check(result->mid.t == &mapped && (count == 0 || result->buf->t == &mapped));
        else
            check(count == 0 || result->buf->t == types[oi]);
        check(alternate == H && callbacks == (monitor ? 1+(count != 0) : 0));
        check(!memcmp(&wanted, &R, sizeof(R)));
        memmove(expected+80+da, &result, sizeof(result));
        check(!memcmp(cells, expected, sizeof(cells)));
        destroy(result);
        check(live == 0 && worker.nerr == 0 && Tchannel.ref == 1 && Tarray.ref == 1 && mapped.ref == 1);
        for(i = 0; i < 5; i++)
            check(types[i]->ref == 1);
        checks++;
    }
    module.jit->freecode(module.jit->code, module.jit->size);
    (free)(module.jit);
    printf("PASS captured channel WORD%zu: %lu callback/ownership/buffer/alias routes\n",
           sizeof(WORD)*8, checks);
    return 0;
}
