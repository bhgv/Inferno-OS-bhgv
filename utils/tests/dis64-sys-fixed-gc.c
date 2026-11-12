/* Actual mixed handlers with argument observations and mutable borrowed native frames. */
#include "sys-fixed-images.h"
static const SysFixedEvent *fixedexpected;
static int fixedinside, fixedos;

/* Native scalar argument maps are empty; changing their fields cannot destroy a pointer. */
static void
grouprelease(void)
{
    WORD poison=99;
    void *wrong=&groupwrong;
    if(groupkind < 3){groupstringrelease();return;}
    require(groupkind == 4 || groupkind == 5);
    memmove(R.FP+64,&poison,sizeof(poison));
    if(groupkind == 5)memmove(R.FP+68,&poison,sizeof(poison));
    memmove(R.FP+32,&wrong,sizeof(wrong));groupreleases++;groupcollect();
}

long
osmillisec(void)
{
    require(groupkind == 3 && cachereleases == cacheacquires);
    fixedos++;groupcollect();return 23;
}
int
kdup(int old, int new)
{
    require(groupkind == 5 && cachereleases == cacheacquires+1);
    if(old != fixedexpected->first || new != fixedexpected->second){fprintf(stderr,"Sys fixed argument predicate\n");exit(18);}
    fixedos++;groupcollect();return old == 7 && new == 9 ? 9 : -1;
}
void osenter(void){require(!fixedinside && groupkind == 4 && cachereleases == cacheacquires+1);fixedinside=1;}
void osleave(void){require(fixedinside);fixedinside=0;}
int
limbosleep(ulong period)
{
    require(fixedinside && groupkind == 4 && cachereleases == cacheacquires+1);
    if(period != (ulong)fixedexpected->first){fprintf(stderr,"Sys fixed argument predicate\n");exit(18);}
    fixedos++;groupcollect();return 0;
}
void seterror(char *text,...){USED(text);require(0);}

static void
fixedexecute(const SysFixedCase *test, int jit)
{
    Module *sys;
    Modlink *caller;
    Prog process={0};
    Frame *entry,*pending;
    Type *frame;
    Inst *pc;
    const SysFixedEvent *event;
    uvlong result,guard=0x1122334455667788ULL,expected;
    WORD first=0,second=0;
    unsigned int step,calls=0,ordinal;
    int before,osbefore,pathbefore;
    numericdrain();require(modules == nil);before=live;sys=groupmodule();
    dis64bootmanaged=1;dis64bootprovider="$Sys";
    caller=prepare64sysstring("sys-fixed",test->image,test->bytes,jit);require(caller != nil);
    for(ordinal=0;ordinal<test->imports;ordinal++)require(strcmp(caller->m->ldt[0][ordinal].name,test->names[ordinal]) == 0);
    require(caller->m->ldt[0][test->imports].name == nil);
    frame=caller->m->entryt;require(frame->size == 112);caller->m->ss=4096;
    if(jit && caller->m->jit != nil){rxbegin=(uintptr)caller->m->jit->code;rxend=rxbegin+caller->m->jit->size;}
    process.exval=H;process.state=Pready;process.quanta=1;process.R.M=caller;process.R.PC=caller->prog;
    entry=mallocz(frame->size,1);require(entry != nil);entry->t=frame;R.s=&entry;
    newstack(&process);free(entry);initmem(frame,process.R.FP);ptrdel(D2H(caller));managedprocess=&process;
    fixedos=fixedinside=groupcalls=groupreleases=0;groupvictim=1;cachereleases=cacheacquires=0;
    for(step=0;step<2048;step++){
        pc=process.R.PC;if(pc == caller->prog+caller->m->nprog-1)break;
        osbefore=fixedos;pathbefore=groupcalls;
        if(pc->op == IMCALL){
            require(calls<test->calls);event=test->events+calls;ordinal=pc->reg;
            require(ordinal<test->imports && strcmp(test->names[ordinal],event->method) == 0);
            groupkind=dis64sysfixedkind(event->method);fixedexpected=event;
            memmove(&pending,process.R.FP+40,sizeof(pending));
            if(groupkind >= 4){
                memmove(&first,(uchar*)pending+64,sizeof(first));
                if(groupkind == 5)memmove(&second,(uchar*)pending+68,sizeof(second));
                if(first != event->first || (groupkind == 5 && second != event->second)){fprintf(stderr,"Sys fixed argument predicate\n");exit(18);}
            }
            groupwrong=0xdeadbeefU;memmove(process.R.FP+104,&guard,sizeof(guard));
        }
        require(kprotect(execute,&process));
        if(pc->op == IMCALL){
            event=test->events+calls;memmove(&result,process.R.FP+104,sizeof(result));expected=0x1122334400000000ULL|(UWORD)event->status;
            if(result != expected){fprintf(stderr,"Sys fixed result predicate\n");exit(17);}
            require(groupwrong == 0xdeadbeefU && cachereleases == cacheacquires && !fixedinside);
            if(groupkind == 0)require(strcmp(worker.env->errstr,event->text) == 0 && fixedos == osbefore && groupcalls == pathbefore);
            else if(groupkind < 3)require(groupcalls == pathbefore+1 && strcmp(grouppath,event->text) == 0 && groupresult == event->status && fixedos == osbefore);
            else require(groupcalls == pathbefore && fixedos == osbefore+(groupkind == 4 && event->first <= 0 ? 0 : 1));
            calls++;
        }
        numericcollect(&process);
    }
    require(step<2048 && calls == test->calls && !fixedinside);
    require(!kprotect(execute,&process) && message[0] == 0);
    destroystack(&R);managedprocess=nil;unload(sys);numericdrain();
    require(modules == nil && live == before && worker.nerr == 0 && Tstring.ref == 1 && Tmodlink.ref == 1 && cachereleases == cacheacquires);
    dis64bootmanaged=0;dis64bootprovider=nil;
}

static void
fixedconstructors(int *native,int *heap)
{
    Module *sys;
    Modlink *caller;
    unsigned int i;
    int before,start,poolstart,attempts,pools,failure;
    numericdrain();sys=groupmodule();numericdrain();before=live;dis64bootmanaged=1;dis64bootprovider="$Sys";
    for(i=0;i<nelem(sysfixed_bad);i++){
        start=allocations;poolstart=literalpoolcalls;
        caller=prepare64sysstring("bad-fixed",sysfixed_bad[i].image,sysfixed_bad[i].bytes,0);
        if(caller != nil){fprintf(stderr,"Sys fixed admission predicate\n");exit(14);}
        require(allocations == start && literalpoolcalls == poolstart && live == before && sys->ref == 1);
    }
    /* Generated callable identity and root geometry are independent obligations. */
    start=allocations;poolstart=literalpoolcalls;
    sys->type[3]->size=72;
    require(prepare64sysstring("bad-clock-frame",sysfixed_cases[0].image,sysfixed_cases[0].bytes,0) == nil);
    sys->type[3]->size=64;
    sys->type[4]->map[1]=0x80;
    require(prepare64sysstring("bad-scalar-map",sysfixed_cases[0].image,sysfixed_cases[0].bytes,0) == nil);
    sys->type[4]->map[1]=0;
    sys->ext[4].u.runt=Sys_werrstr;
    require(prepare64sysstring("bad-scalar-function",sysfixed_cases[0].image,sysfixed_cases[0].bytes,0) == nil);
    sys->ext[4].u.runt=Sys_sleep;
    require(allocations == start && literalpoolcalls == poolstart && live == before && sys->ref == 1);
    for(i=0;i<nelem(sysfixed_cases);i++){
        start=allocations;poolstart=literalpoolcalls;
        caller=prepare64sysstring("fixed",sysfixed_cases[i].image,sysfixed_cases[i].bytes,0);require(caller != nil);
        attempts=allocations-start;pools=literalpoolcalls-poolstart;ptrdel(D2H(caller));destroy(caller);numericdrain();require(live == before);
        for(failure=1;failure<=attempts;failure++){
            failat=allocations+failure;require(prepare64sysstring("fixed",sysfixed_cases[i].image,sysfixed_cases[i].bytes,0) == nil);
            failat=0;numericdrain();require(live == before && modules == sys && sys->ref == 1 && worker.nerr == 0 && Tstring.ref == 1);
        }
        for(failure=1;failure<=pools;failure++){
            literalpoolfail=literalpoolcalls+failure;require(prepare64sysstring("fixed",sysfixed_cases[i].image,sysfixed_cases[i].bytes,0) == nil);
            literalpoolfail=0;numericdrain();require(live == before && modules == sys && sys->ref == 1 && worker.nerr == 0 && Tstring.ref == 1);
        }
        *native+=attempts;*heap+=pools;
    }
    unload(sys);numericdrain();dis64bootmanaged=0;dis64bootprovider=nil;
}

int
main(void)
{
    unsigned int i;
    int native=0,heap=0;
    numericpoolinit();Tstring.size=sizeof(String);Tstring.free=freestring;Tstring.mark=noptrs;
    worker.env=&worker.defenv;worker.env->errstr=worker.defenv.errbuf0;
    optab[ILOAD]=iload;optab[IMOVP]=movp;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;optab[IEXIT]=iexit;
    optab[IMOVW]=movw;optab[IADDW]=addw;optab[ISUBW]=subw;optab[ISHLW]=shlw;optab[IBGEW]=bgew;optab[IBNEW]=bnew;optab[IBEQW]=beqw;optab[IBLEW]=blew;optab[IJMP]=jmp;
    fixedconstructors(&native,&heap);
    for(i=0;i<nelem(sysfixed_cases);i++){fixedexecute(sysfixed_cases+i,0);fixedexecute(sysfixed_cases+i,1);}
    require(live == 0);
    printf("PASS mixed fixed Sys owning runtime: %u C/JIT/GC cases, 64/72-byte callee maps, exact scalar args/results/ordinals, release snapshots, %u wire plus 3 generated-entry pre-allocation refusals, %d native/%d heap rollback refusals\n",2*(unsigned int)nelem(sysfixed_cases),(unsigned int)nelem(sysfixed_bad),native,heap);
    return 0;
}
