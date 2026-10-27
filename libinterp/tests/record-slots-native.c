static void *recordalternate; // Independent output cell which an allocation callback must not overwrite.
static unsigned long recordcallbacks; // Number of observed real heap allocation callbacks.


/* Retain every ownership and byte-image predicate under NDEBUG. */
static void
recordcheck(int condition)
{
    if(!condition)
    abort();
}


/* A C allocation callback may borrow decoded registers for its own work. */
static void
recordredirect(int event,void *header,ulong size)
{
    (void)event;
    (void)header;
    (void)size;
    recordcallbacks++;
    R.d=&recordalternate;
}


/* Compare all three dispatch routes with allocation, reuse, alias and map oracles. */
static unsigned long
recordpositive(Jit *jit)
{
    Type type,*types[1];
    Module owner;
    Modlink link,*source;
    Heap *oldheader,*header;
    uchar cells[128],before[256],expected[256];
    void *old,*result,*destination,*nilvalue;
    WORD index;
    REG wanted;
    int sizes[]={0,1,7,8,9,15,16,17,31,32,63,64,65,127,128,255,256};
    int ops[]={INEW,INEWZ,IMNEWZ};
    int n,which,mode,sa,da,alias,route,op,size,reuse,beforecalls,beforelive,beforeref,i;
    unsigned long checks=0;

    for(n=0;n<sizeof(sizes)/sizeof(sizes[0]);n++)
    for(which=0;which<3;which++)
    for(mode=0;mode<5;mode++)
    for(sa=0;sa<8;sa++)
    for(da=0;da<8;da++)
    for(alias=0;alias<3;alias++){
        size=sizes[n];
        op=ops[which];
        reuse=mode==0;
        if((mode==2 || mode==3) && alias!=0)
            continue;
        if(mode==3 && size<sizeof(void*))
            continue;
        if(alias==1 && size<sa+sizeof(void*))
            continue;
        if(alias==2 && size<da+2*sizeof(void*))
            continue;
        if(getenv("RECORD_SLOT_MODE") && mode!=atoi(getenv("RECORD_SLOT_MODE")))
            continue;
        for(route=0;route<3;route++){
            memset(&type,0,sizeof(type));
            memset(&owner,0,sizeof(owner));
            memset(&link,0,sizeof(link));
            memset(cells,0xa5,sizeof(cells));
            memset(&R,0xa5,sizeof(R));
            type.ref=1;
            type.size=size;
            type.free=freeheap;
            type.mark=markheap;
            if(mode==3){
                type.np=1;
                type.map[0]=0x80;
            }
            types[0]=&type;
            owner.type=types;
            owner.ntype=1;
            link.m=&owner;
            source=&link;
            index=0;
            heapmonitor=nil;
            failure=0;
            recordcallbacks=0;
            recordalternate=H;
            old=mode==2 ? H : H2D(void*,heapz(&type));
            if(old!=H){
                oldheader=D2H(old);
                oldheader->ref=reuse ? 1 : 2;
                memset(old,0x6d,size);
                if(mode==3){
                    nilvalue=H;
                    memmove(old,&nilvalue,sizeof(nilvalue));
                }
            }
            R.M=&link;
            R.s=cells+sa;
            R.m=cells+32+sa;
            R.d=cells+64+da;
            if(alias==1){
                if(op==IMNEWZ)R.m=(uchar*)old+sa;
                else R.s=(uchar*)old+sa;
            }
            if(alias==2)R.d=(uchar*)old+sizeof(void*)+da;
            if(op==IMNEWZ){
                memmove(R.s,&source,sizeof(source));
                memmove(R.m,&index,sizeof(index));
            }
            else memmove(R.s,&index,sizeof(index));
            memmove(R.d,&old,sizeof(old));
            destination=R.d;
            if(old!=H)memmove(before,old,size);
            memmove(&wanted,&R,sizeof(R));
            beforecalls=calls;
            beforelive=live;
            beforeref=type.ref;
            if(mode==4){
                heapmonitor=recordredirect;
                wanted.d=&recordalternate;
            }
            if(route==0)optab[op]();
            else if(route==1){
                if(!jit->entry[op](&R))optab[op]();
            }
            else recordcheck(jit->dispatch[op](&R)==1);
            heapmonitor=nil;
            memmove(&result,destination,sizeof(result));
            recordcheck(!memcmp(&R,&wanted,sizeof(R)) && recordalternate==H && recordcallbacks==(mode==4));
            recordcheck(calls==beforecalls+!reuse && live==beforelive+!reuse && type.ref==beforeref+!reuse);
            if(reuse){
                recordcheck(result==old && D2H(old)->ref==1);
                if(op==INEW)memmove(expected,before,size);
                else memset(expected,0,size);
                if(alias==2)memmove(expected+sizeof(void*)+da,&old,sizeof(old));
                recordcheck(!memcmp(old,expected,size));
            }
            else{
                recordcheck(result!=old && result!=nil && result!=H);
                header=D2H(result);
                recordcheck(header->t==&type && header->ref==1 && header->color==mutator);
                for(i=0;i<size;i++)recordcheck(((uchar*)result)[i]==(mode==3 && i<sizeof(void*) ? 255 : op==INEW ? 0xa5 : 0));
                if(old!=H){
                    recordcheck(D2H(old)->ref==1);
                    memmove(expected,before,size);
                    if(alias==2)memmove(expected+sizeof(void*)+da,&result,sizeof(result));
                    recordcheck(!memcmp(old,expected,size));
                }
            }
            destroy(result);
            if(old!=H && !reuse)destroy(old);
            recordcheck(live==0 && type.ref==1);
            checks++;
        }
    }
    return checks;
}


/* Reject malformed source/index/output cells before heap work or owner release. */
static unsigned long
recordinvalid(Jit *jit)
{
    Type type,*types[1];
    Module owner;
    Modlink link,*source;
    WORD index=0;
    void *old=H,*bad;
    REG saved;
    int ops[]={INEW,INEWZ,IMNEWZ};
    int which,kind,offset,route,op,failed,beforecalls;
    unsigned long checks=0;

    if(getenv("RECORD_SLOT_NO_ERRORS"))
        return 0;
    for(which=0;which<3;which++)
    for(kind=0;kind<3;kind++)
    for(offset=0;offset<=sizeof(void*);offset++){
        op=ops[which];
        if(kind==2 && op!=IMNEWZ)
            continue;
        if(getenv("RECORD_SLOT_BAD") && kind!=atoi(getenv("RECORD_SLOT_BAD")))
            continue;
        if(offset>(kind==2 || (kind==0 && op!=IMNEWZ) ? sizeof(WORD) : sizeof(void*)))
            continue;
        memset(&type,0,sizeof(type));
        memset(&owner,0,sizeof(owner));
        memset(&link,0,sizeof(link));
        type.ref=1;
        type.size=16;
        type.free=freeheap;
        type.mark=markheap;
        types[0]=&type;
        owner.type=types;
        owner.ntype=1;
        link.m=&owner;
        source=&link;
        memset(&R,0xa5,sizeof(R));
        R.M=&link;
        R.s=op==IMNEWZ ? (void*)&source : (void*)&index;
        R.m=&index;
        R.d=&old;
        bad=offset==0 ? nil : (void*)((uintptr)-offset);
        if(kind==0)
            R.s=bad;
        if(kind==1)
            R.d=bad;
        if(kind==2)
            R.m=bad;
        memmove(&saved,&R,sizeof(R));
        beforecalls=calls;
        recordcheck(jit->entry[op](&R)==0 && !memcmp(&saved,&R,sizeof(R)) && old==H && calls==beforecalls && type.ref==1);
        for(route=0;route<2;route++){
            memmove(&R,&saved,sizeof(R));
            raised=nil;
            failed=0;
            if(setjmp(boundary)==0){
                if(route==0)optab[op]();
                else jit->dispatch[op](&R);
            }else failed=1;
            recordcheck(failed && raised==exBounds && !memcmp(&saved,&R,sizeof(R)) && old==H && calls==beforecalls && type.ref==1 && live==0);
            checks++;
        }
    }
    return checks;
}


/* Verify real constructor entries and both native WORD layouts. */
static int
recordmain(void)
{
    Module module={0};
    Inst program[3]={{0}};
    unsigned long positive=0,negative=0;
    char *section=getenv("RECORD_SLOT_SECTION");

    program[0].op=INEW;
    program[1].op=INEWZ;
    program[2].op=IMNEWZ;
    module.prog=program;
    module.nprog=3;
    optab[INEW]=new;
    optab[INEWZ]=newz;
    optab[IMNEWZ]=mnewz;
    recordcheck(compile(&module,3,nil) && module.jit);
    if(section==nil || !strcmp(section,"positive"))positive=recordpositive(module.jit);
    if(section==nil || !strcmp(section,"invalid"))negative=recordinvalid(module.jit);
    module.jit->freecode(module.jit->code,module.jit->size);
    free(module.jit);
    printf("PASS record slots C/RX WORD%zu: %lu ownership/map/callback routes, %lu rejected routes\n",sizeof(WORD)*8,positive,negative);
    return 0;
}


/* Use a tracked pthread stack so ASan can restore shadow state after VM longjmp. */
static void*
recordworker(void *argument)
{
    int *status;

    status=argument;
    *status=recordmain();
    return nil;
}


/* Join before observing status; the fixture still executes on exactly one VM thread. */
int
main(void)
{
    pthread_t worker;
    int status=-1;

    recordcheck(pthread_create(&worker,nil,recordworker,&status)==0);
    recordcheck(pthread_join(worker,nil)==0);
    return status;
}
