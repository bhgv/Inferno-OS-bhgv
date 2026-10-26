static Jit *nativecode; // Executable leaves remain owned until every case completes.
static int usejit; // Exercise generated code before the normal decoder fallback.
static unsigned long accepted; // Count genuine native successes separately from guarded fallback.


/* Drive the production constructor through a real outer VM error frame. */
static int
execute(void *unused)
{
    (void)unused;
    if(usejit && nativecode->entry[ICONSMP](&R)){
        accepted++;
    }else{
        consmp();
    }
    return 1;
}


/* Allocate an independent list root and initialize payload bytes for alias snapshots. */
static List*
rootnode(int bytes)
{
    List *node=H;
    cons(bytes,&node);
    memset(node->data,0x6b,bytes);
    return node;
}


/* Check exact raw copies, descriptor aliases and allocator failures across bounded native paths. */
static void
plaincopies(Modlink *link)
{
    int sizes[]={0,1,7,8,13,64,128,256,257,300};
    uchar slots[1024], expected[1024], payload[320];
    Type *type;
    List *tail, *out;
    WORD index=0;
    int si, sa, da, alias, descriptor, fail, ok, i, flags;
    int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    fenv_t environment;
    unsigned long total=0;

    fegetenv(&environment);
    for(descriptor=0;descriptor<2;descriptor++)
    for(si=0;si<10;si++)
    for(sa=0;sa<8;sa++)
    for(da=0;da<8;da++)
    for(alias=0;alias<3;alias++)
    for(fail=0;fail<2;fail++)
    for(usejit=0;usejit<2;usejit++){
        if(getenv("MAPPED_CONS_NO_ERRORS") && fail){
            continue;
        }
        failure=0;
        tail=rootnode(320);
        D2H(tail)->ref=2;
        type=descriptor ? &Tlist : &record;
        type->size=sizes[si];
        type->np=0;
        link->m->type[0]=type;
        memset(slots,0xa5,sizeof(slots));
        for(i=0;i<320;i++){
            slots[16+sa+i]=(uchar)(i*13+si);
        }
        R.M=link;
        R.s=slots+16+sa;
        R.m=&index;
        R.d=slots+512+da;
        memmove(R.d,&tail,sizeof(tail));
        if(alias==1){
            R.s=R.d;
        }
        if(alias==2){
            R.s=(uchar*)tail->data+sa;
        }
        memmove(payload,R.s,type->size);
        memmove(expected,slots,sizeof(slots));
        failure=fail;
        raised=nil;
        fesetround(rounds[sa%4]);
        feclearexcept(FE_ALL_EXCEPT);
        feraiseexcept(FE_DIVBYZERO);
        flags=fetestexcept(FE_ALL_EXCEPT);
        ok=kprotect(execute,nil);
        check(fegetround()==rounds[sa%4] && fetestexcept(FE_ALL_EXCEPT)==flags);
        failure=0;
        memmove(&out,R.d,sizeof(out));
        if(ok){
            check(!fail && out!=tail && out!=H && out->t==type && out->tail==tail);
            check(!memcmp(out->data,payload,type->size));
            memmove(expected+512+da,&out,sizeof(out));
            destroy(out);
            check(D2H(tail)->ref==1);
        }else{
            check(fail && raised==exHeap && out==tail && D2H(tail)->ref==2);
        }
        check(!memcmp(expected,slots,sizeof(slots)) && worker.nerr==0);
        D2H(tail)->ref=1;
        destroy(tail);
        check(live==0 && record.ref==1 && Tlist.ref==1);
        total++;
    }
    fesetenv(&environment);
    printf("PASS mapped raw C/RX: %lu copy/alias/Type/allocation cases WORD%zu\n",total,sizeof(WORD)*8);
}


/* Require rollback of duplicate/saturated heads without releasing the old destination tail. */
static void
mappedheads(Modlink *link)
{
    uchar slots[128], expected[128], payload[16];
    void *owners[2];
    List *head, *tail, *out;
    ulong headrefs, tailrefs;
    WORD index=0;
    int profile, sa, da, alias, fail, ok, valid, i, heads, tails;
    unsigned long total=0;

    record.size=16;
    record.np=1;
    record.map[0]=0xc0;
    link->m->type[0]=&record;
    for(profile=0;profile<9;profile++)
    for(sa=0;sa<8;sa++)
    for(da=0;da<8;da++)
    for(alias=0;alias<3;alias++)
    for(fail=0;fail<2;fail++)
    for(usejit=0;usejit<2;usejit++){
        if(getenv("MAPPED_CONS_OVERFLOW_ONLY") && profile!=3 && profile!=4){
            continue;
        }
        if(getenv("MAPPED_CONS_NO_ERRORS") && (fail || (profile!=0 && profile!=2 && profile!=7))){
            continue;
        }
        failure=0;
        head=rootnode(32);
        tail=rootnode(64);
        D2H(head)->ref=2;
        D2H(tail)->ref=3;
        owners[0]=head;
        owners[1]=head;
        if(profile==1){
            owners[1]=nil;
        }
        if(profile==2){
            owners[1]=H;
        }
        if(profile==3){
            D2H(head)->ref=(ulong)-1;
        }
        if(profile==4){
            D2H(head)->ref=(ulong)-2;
        }
        if(profile==5){
            D2H(head)->ref=0;
        }
        if(profile==6){
            owners[1]=(void*)(uintptr)sizeof(Heap);
        }
        if(profile==7){
            owners[1]=tail;
        }
        if(profile==8){
            owners[0]=tail;
            owners[1]=nil;
        }
        memset(slots,0xa5,sizeof(slots));
        R.M=link;
        R.s=slots+16+sa;
        R.m=&index;
        R.d=slots+80+da;
        if(alias==1){
            R.s=R.d;
        }
        if(alias==2){
            R.s=(uchar*)tail->data+sa;
        }
        memmove(R.s,owners,sizeof(owners));
        memmove(R.d,&tail,sizeof(tail));
        memmove(owners,R.s,sizeof(owners));
        memmove(payload,R.s,sizeof(payload));
        memmove(expected,slots,sizeof(slots));
        headrefs=D2H(head)->ref;
        tailrefs=D2H(tail)->ref;
        valid=!fail;
        heads=0;
        tails=0;
        for(i=0;i<2;i++){
            if(owners[i]==head){
                heads++;
            }else if(owners[i]==tail){
                tails++;
            }else if(owners[i]!=H){
                valid=0;
            }
        }
        if(heads && (headrefs==0 || headrefs>(ulong)-1-heads)){
            valid=0;
        }
        failure=fail;
        raised=nil;
        ok=kprotect(execute,nil);
        failure=0;
        memmove(&out,R.d,sizeof(out));
        if(ok!=valid || worker.nerr!=0){
            fprintf(stderr,"mapped profile=%d sa=%d da=%d alias=%d failure=%d native=%d ok=%d expected=%d heads=%d ref=%lu error=%s\n",profile,sa,da,alias,fail,usejit,ok,valid,heads,headrefs,raised ? raised : "none");
            abort();
        }
        if(ok){
            check(out!=H && out!=tail && out->t==&record && out->tail==tail);
            check(!memcmp(out->data,payload,sizeof(payload)));
            check(D2H(head)->ref==headrefs+heads && D2H(tail)->ref==tailrefs+tails);
            memmove(expected+80+da,&out,sizeof(out));
            destroy(out);
            check(D2H(tail)->ref==tailrefs-1);
        }else{
            check(raised==exHeap && out==tail && D2H(tail)->ref==tailrefs);
        }
        check(D2H(head)->ref==headrefs && record.ref==1);
        check(!memcmp(expected,slots,sizeof(slots)));
        D2H(head)->ref=1;
        D2H(tail)->ref=1;
        destroy(head);
        destroy(tail);
        check(live==0 && Tlist.ref==1);
        total++;
    }
    printf("PASS mapped retain rollback: %lu head/tail/duplicate/failure cases WORD%zu\n",total,sizeof(WORD)*8);
}


/* Validate complete operand spans and malformed maps before contacting the node allocator. */
static void
preflight(Modlink *link)
{
    Type *type;
    List *out;
    WORD index;
    uchar source[32];
    int profile, before, ok;
    void *bad[]={nil,H,(void*)(uintptr)-4};

    for(usejit=0;usejit<2;usejit++){
        for(profile=0;profile<13;profile++){
            record.ref=1;
            record.size=8;
            record.np=0;
            record.map[0]=0;
            type=&record;
            out=H;
            index=0;
            memset(source,0x6b,sizeof(source));
            R.M=link;
            R.s=source;
            R.m=&index;
            R.d=&out;
            if(profile<3){
                R.m=bad[profile];
            }else if(profile<6){
                R.d=bad[profile-3];
            }else if(profile<9){
                R.s=bad[profile-6];
            }else if(profile==9){
                record.np=-1;
            }else if(profile==10){
                record.size=1;
                record.np=1;
                record.map[0]=0x80;
            }else if(profile==11){
                record.np=1;
                record.map[0]=0x40;
            }else{
                record.ref=0;
                record.np=1;
            }
            link->m->type[0]=type;
            before=allocations;
            raised=nil;
            ok=kprotect(execute,nil);
            check(!ok && raised==exHeap && out==H && allocations==before);
            check(live==0 && Tlist.ref==1 && worker.nerr==0);
        }
    }
    record.ref=1;
    record.np=0;
    puts("PASS mapped cons preflight: 26 slot/map/no-allocation cases");
}


/* Publish the production entry and run raw/native plus mapped/fallback ownership matrices. */
int
main(void)
{
    Module module={0};
    Modlink link={0};
    Inst instruction={0};
    Type *table[1];

    instruction.op=ICONSMP;
    module.prog=&instruction;
    module.nprog=1;
    module.type=table;
    module.ntype=1;
    link.m=&module;
    check(compile(&module,1,nil) && module.jit!=nil);
    nativecode=module.jit;
    check(nativecode->entry[ICONSMP]!=nil);
    if(!getenv("MAPPED_CONS_NO_ERRORS") && !getenv("MAPPED_CONS_OVERFLOW_ONLY")){
        preflight(&link);
    }
    if(!getenv("MAPPED_CONS_OVERFLOW_ONLY")){
        plaincopies(&link);
    }
    mappedheads(&link);
    check(accepted>0 || getenv("MAPPED_CONS_OVERFLOW_ONLY"));
    nativecode->freecode(nativecode->code,nativecode->size);
    free(nativecode);
    printf("PASS mapped cons native successes: %lu\n",accepted);
    return 0;
}
