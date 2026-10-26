static Jit *code; // Published native entries remain owned until all cases finish.
static int selected; // Zero selects a raw payload, one selects a mapped descriptor.
static int native; // Enable the actual AMD64 leaf before the normal C fallback.
static int ops[]={INEWCM, INEWCMP}; // Real opcode identities from the VM ISA.


/* Execute the generated entry and preserve the decoder's side-effect-free fallback contract. */
static int
execute(void *unused)
{
    (void)unused;
    if(native && code->entry[ops[selected]](&R)){
        return 1;
    }
    if(selected){
        newcmp();
    }else{
        newcm();
    }
    return 1;
}


/* Reject dead/saturated descriptors before their map bytes on a protected page are read. */
static void
guardedmaps(void)
{
    long page;
    uchar *memory;
    Type *type, *table[1];
    Module module={0};
    Modlink link={0};
    Channel *out;
    WORD index=0, length;
    int profile, before, buffered;

    page=sysconf(_SC_PAGESIZE);
    check(page>0 && (ulong)page>offsetof(Type,map));
    memory=mmap(nil,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    check(memory!=MAP_FAILED);
    type=(Type*)(memory+page-offsetof(Type,map));
    memset(type,0,offsetof(Type,map));
    type->size=8;
    type->np=1;
    check(mprotect(memory+page,page,PROT_NONE)==0);
    table[0]=type;
    module.type=table;
    module.ntype=1;
    link.m=&module;
    selected=1;
    for(native=0;native<2;native++){
        for(buffered=0;buffered<2;buffered++){
            for(profile=0;profile<3;profile++){
                type->ref=profile==0 ? 0 : profile==1 ? -1 : 0x7fffffff;
                length=buffered;
                out=H;
                R.M=&link;
                R.s=&index;
                R.m=&length;
                R.d=&out;
                before=calls;
                last_error=nil;
                check(!kprotect(execute,nil));
                check(last_error==exHeap && out==H && calls==before && live==0);
                check(Tchannel.ref==1 && Tarray.ref==1 && worker.nerr==0);
            }
        }
    }
    check(munmap(memory,2*page)==0);
    puts("PASS guarded mapped descriptors: 12 reference-before-map cases");
}


/* Reject bad live module/type metadata and preserve descriptor-error precedence under C/RX. */
static void
metadata(void)
{
    Module module;
    Modlink link;
    Type type;
    Type *table[1];
    Channel *out;
    WORD index, length;
    int profile, before, references, buffered;

    selected=1;
    for(native=0;native<2;native++){
        for(buffered=0;buffered<2;buffered++){
            for(profile=0;profile<17;profile++){
                memset(&module,0,sizeof(module));
                memset(&link,0,sizeof(link));
                memset(&type,0,sizeof(type));
                type.ref=1;
                type.size=8;
                table[0]=&type;
                module.type=table;
                module.ntype=1;
                link.m=&module;
                index=0;
                length=buffered;
                out=H;
                R.M=&link;
                if(profile==0){
                    R.M=nil;
                }
                if(profile==1){
                    link.m=nil;
                }
                if(profile==2){
                    module.type=nil;
                }
                if(profile==3){
                    module.ntype=0;
                }
                if(profile==4){
                    index=-1;
                }
                if(profile==5){
                    table[0]=nil;
                }
                if(profile==6){
                    type.ref=0;
                }
                if(profile==7){
                    type.ref=0x7fffffff;
                }
                if(profile==8){
                    type.np=-1;
                }
                if(profile==9){
                    type.size=-1;
                }
                if(profile==10){
                    module.type=H;
                }
                if(profile==11){
                    length=-1;
                    R.M=nil;
                }
                if(profile==12){
                    R.M=(Modlink*)(uintptr)-4;
                }
                if(profile==13){
                    type.size=1;
                    type.np=1;
                    type.map[0]=0x80;
                }
                if(profile==14){
                    type.np=1;
                    type.map[0]=0x40;
                }
                if(profile==15){
                    type.size=0;
                    type.np=1;
                    type.map[0]=0x80;
                }
                if(profile==16){
                    type.np=2;
                }
                R.s=&index;
                R.m=&length;
                R.d=&out;
                references=type.ref;
                before=calls;
                last_error=nil;
                check(!kprotect(execute,nil));
                check(out==H && live==0 && type.ref==references);
                check(Tchannel.ref==1 && Tarray.ref==1 && worker.nerr==0);
                check(calls==before);
                if(profile==0 || profile==1 || profile==11 || profile==12){
                    check(last_error==exModule);
                }else if((profile>=6 && profile<=8) || profile>=13){
                    check(last_error==exHeap);
                }else{
                    check(last_error!=nil && strcmp(last_error,"invalid type")==0);
                }
            }
        }
    }
    puts("PASS mapped channels: 68 live-module/map/pre-allocation/error-order cases");
}


/* Sweep raw/mapped source words, zero-size buffers, aliases, old owners and allocation cutoffs. */
int
main(void)
{
    Module module={0};
    Modlink link={0};
    Inst instructions[2]={{0}};
    Type plain={1,0,0,13};
    Type mapped={1,0,0,16,1,0,0,{0x80}};
    Type empty={1};
    Type *table[]={&plain,&mapped,&empty};
    WORD values[]={0,1,13,-1,0x7fffffff,(WORD)0x100000001ULL};
    WORD lengths[]={0,1,3,-1,0x7fffffff};
    WORD value, count;
    Channel *old, *out;
    Type *type;
    REG saved;
    uchar slots[128], expected[128];
    int vi, li, sa, ma, da, alias, cutoff, ok, i, valid, flags;
    int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    fenv_t environment;
    unsigned long checks=0;

    for(i=0;i<2;i++){
        instructions[i].op=ops[i];
    }
    module.prog=instructions;
    module.nprog=2;
    module.type=table;
    module.ntype=3;
    link.m=&module;
    check(compile(&module,2,nil) && module.jit!=nil);
    code=module.jit;
    check(code->entry[INEWCM]!=nil && code->entry[INEWCMP]!=nil);
    if(!getenv("CHANNEL_NO_ERRORS")){
        metadata();
        guardedmaps();
    }
    if(getenv("CHANNEL_METADATA_ONLY")){
        code->freecode(code->code,code->size);
        (free)(code);
        return 0;
    }
    fegetenv(&environment);
    for(selected=0;selected<2;selected++)
    for(vi=0;vi<6;vi++)
    for(li=0;li<5;li++)
    for(sa=0;sa<4;sa++)
    for(ma=0;ma<4;ma++)
    for(da=0;da<4;da++)
    for(alias=0;alias<4;alias++)
    for(cutoff=-1;cutoff<6;cutoff++)
    for(native=0;native<2;native++){
        if(getenv("CHANNEL_NO_ERRORS") && (vi>1 || li>2 || cutoff!=-1)){
            continue;
        }
        fail_after=-1;
        old=H;
        if(alias==3){
            old=cnewc(nil,movm,0);
            old->mid.w=values[vi];
        }
        memset(slots,0xa5,sizeof(slots));
        R.M=&link;
        R.s=slots+8+sa;
        R.m=slots+40+ma;
        R.d=slots+88+da;
        memmove(R.s,&values[vi],sizeof(WORD));
        memmove(R.m,&lengths[li],sizeof(WORD));
        memmove(R.d,&old,sizeof(old));
        if(alias==1){
            R.m=R.d;
        }
        if(alias==2){
            R.s=R.m;
        }
        if(alias==3){
            R.s=&old->mid.w;
        }
        memmove(&value,R.s,sizeof(value));
        count=0;
        if(R.m!=R.d){
            memmove(&count,R.m,sizeof(count));
        }
        type=selected && value>=0 && value<3 ? table[value] : nil;
        valid=value>=0 && (selected ? type!=nil : (uvlong)value<=0x7fffffff) && count>=0;
        if(getenv("CHANNEL_NO_ERRORS") && !valid){
            if(old!=H){
                destroy(old);
            }
            continue;
        }
        memmove(expected,slots,sizeof(expected));
        memmove(&saved,&R,sizeof(saved));
        publication_old=old;
        publication_width=value;
        publication_watch=!selected && old!=H;
        fail_after=cutoff;
        last_error=nil;
        fesetround(rounds[ma]);
        feclearexcept(FE_ALL_EXCEPT);
        feraiseexcept(FE_DIVBYZERO);
        flags=fetestexcept(FE_ALL_EXCEPT);
        ok=kprotect(execute,nil);
        check(fegetround()==rounds[ma] && fetestexcept(FE_ALL_EXCEPT)==flags);
        fail_after=-1;
        check(!memcmp(&saved,&R,sizeof(R)) && worker.nerr==0);
        memmove(&out,R.d,sizeof(out));
        if(ok){
            check(publication_watch==0);
            check(valid && out!=H && out!=old && out->size==0 && out->front==0);
            check(out->mover==(selected ? movtmp : movm));
            check(selected ? out->mid.t==type : out->mid.w==value);
            check(count==0 ? out->buf==H : out->buf!=H && out->buf->len==count);
            if(count!=0){
                check(selected ? out->buf->t==type : out->buf->t->size==value && out->buf->t->np==0);
            }
            memmove(expected+88+da,&out,sizeof(out));
            destroy(out);
        }else{
            publication_watch=0;
            check(out==old && last_error!=nil);
            if(old!=H){
                destroy(old);
            }
        }
        check(!memcmp(slots,expected,sizeof(slots)));
        check(live==0 && plain.ref==1 && mapped.ref==1 && empty.ref==1);
        check(Tchannel.ref==1 && Tarray.ref==1);
        checks++;
    }
    fesetenv(&environment);
    code->freecode(code->code,code->size);
    (free)(code);
    printf("PASS payload channels C/RX: %lu descriptor/width/buffer/alias/failure cases WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
