/* Actual mixed handlers with argument observations and mutable borrowed native frames. */
#include "sys-fixed-images.h"
static const SysFixedEvent *fixedexpected;
static int fixedinside, fixedos, fdthrow;
static Fgrp fdgroup;
static int fdopened,fdclosed,fdlive[256];

/* Native scalar argument maps are empty; changing their fields cannot destroy a pointer. */
static void
grouprelease(void)
{
    WORD poison=99;
    void *wrong=&groupwrong;
    if(fdclosing)return;
    if(groupkind == 8){
        memmove(R.FP+64,&poison,sizeof(poison));memmove(R.FP+32,&wrong,sizeof(wrong));groupreleases++;groupcollect();return;
    }
    if(groupkind == 9){
        void *old,*empty=H;
        memmove(&old,R.FP+64,sizeof(old));memmove(R.FP+64,&empty,sizeof(empty));destroy(old);
        memmove(R.FP+32,&wrong,sizeof(wrong));groupreleases++;groupcollect();return;
    }
    if(groupkind < 3){groupstringrelease();return;}
    if(groupkind >= 6){
        void *old,*empty=H;
        memmove(&old,R.FP+64,sizeof(old));memmove(R.FP+64,&empty,sizeof(empty));destroy(old);
        memmove(&old,R.FP+72,sizeof(old));memmove(R.FP+72,&empty,sizeof(empty));destroy(old);
        if(groupkind == 7)memmove(R.FP+80,&poison,sizeof(poison));
        memmove(R.FP+32,&wrong,sizeof(wrong));groupreleases++;groupcollect();return;
    }
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
    if(groupkind == 8){
        require(cachereleases == cacheacquires+1 && old == fixedexpected->first && new == -1);
        fixedos++;groupcollect();if(old < 0)return -1;
        require(old < 100 && !fdlive[old+100]);fdlive[old+100]=1;fdopened++;return old+100;
    }
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
static int
pairnative(char *first,char *second,int flags)
{
    require(groupkind >= 6 && cachereleases == cacheacquires+1);
    if(strcmp(first,fixedexpected->text) || strcmp(second,fixedexpected->other) ||
       (groupkind == 7 && flags != fixedexpected->first)){
        fprintf(stderr,"Sys paired argument predicate\n");exit(19);
    }
    fixedos++;groupcollect();return -1;
}
int kunmount(char *first,char *second){return pairnative(first,second,0);}
int kbind(char *first,char *second,int flags){return pairnative(first,second,flags);}
int incref(Ref *ref){return ++ref->ref;}
void closefgrp(Fgrp *group){require(group == &fdgroup && group->r.ref > 1);group->r.ref--;}
int kfgrpclose(Fgrp *group,int fd){require(group == &fdgroup && fd >= 100 && fd < 200 && fdlive[fd]);fdlive[fd]=0;fdclosed++;return 0;}
int kiounit(int fd){require(groupkind == 9 && fd == fixedexpected->first && cachereleases == cacheacquires+1);fixedos++;groupcollect();if(fdthrow)error("fd out of range or not open");return fd < 0 ? -1 : 4096;}
void seterror(char *text,...){USED(text);require(0);}

static void
fixedexecute(const SysFixedCase *test, int jit, int conversion)
{
    Module *sys;
    Modlink *caller;
    Prog process={0};
    Frame *entry,*pending;
    Type *frame;
    Inst *pc;
    const SysFixedEvent *event;
    uvlong fdguard=0,result,guard=0x1122334455667788ULL,expected;
    WORD first=0,second=0;
    unsigned int step,calls=0,ordinal;
    int before,osbefore,pathbefore,fault=0,dropping,dropbefore,openbefore=fdopened,closebefore=fdclosed;
    Sys_FD *fdresult;
    uchar *callerfp;
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
        callerfp=process.R.FP;pc=process.R.PC;if(pc == caller->prog+caller->m->nprog-1)break;
        osbefore=fixedos;pathbefore=groupcalls;dropbefore=fdclosed;
        dropping=pc->op == IMOVP && pc->add == (SRC(AMP)|DST(AFP)) && pc->s.ind == 8 && pc->d.ind == 96;
        if(dropping){memmove(&fdresult,process.R.FP+96,sizeof(fdresult));dropping=fdresult != H ? 2 : 1;}
        if(pc->op == IMCALL){
            require(calls<test->calls);event=test->events+calls;ordinal=pc->reg;
            require(ordinal<test->imports && strcmp(test->names[ordinal],event->method) == 0);
            groupkind=dis64sysfixedkind(event->method);fixedexpected=event;
            memmove(&pending,process.R.FP+40,sizeof(pending));
            if(groupkind == 4 || groupkind == 5 || groupkind == 8){
                memmove(&first,(uchar*)pending+64,sizeof(first));
                if(groupkind == 5)memmove(&second,(uchar*)pending+68,sizeof(second));
                if(first != event->first || (groupkind == 5 && second != event->second)){fprintf(stderr,"Sys fixed argument predicate\n");exit(18);}
            }
            groupwrong=0xdeadbeefU;
            if(groupkind == 8)memmove(&fdguard,process.R.FP+104,sizeof(fdguard));
            else memmove(process.R.FP+104,&guard,sizeof(guard));
            if(conversion == -2 || (conversion == -3 && groupkind == 9)){
                require(groupkind == 9 && (conversion == -3 || calls == 0));fdthrow=1;
                require(!kprotect(execute,&process) && strcmp(message,"fd out of range or not open") == 0);fdthrow=0;
                memmove(&result,callerfp+104,sizeof(result));
                require(result == guard && cachereleases == cacheacquires);
                fault=1;break;
            }
            if(conversion == -1 || (conversion == -4 && groupkind == 8 && calls > 0)){
                require(groupkind == 8 && (conversion == -4 || calls == 0));
                literalpoolfail=literalpoolcalls+1;
                require(!kprotect(execute,&process) && strcmp(message,exHeap) == 0);literalpoolfail=0;
                memmove(&fdresult,callerfp+96,sizeof(fdresult));
                memmove(&result,callerfp+104,sizeof(result));
                require(fdresult == H && result == fdguard && fdopened-openbefore == (conversion == -4 ? 2 : 1) && fdclosed-closebefore == (conversion == -4 ? 2 : 1) && cachereleases == cacheacquires);
                fault=1;break;
            }
            if(conversion > 0){
                require(groupkind >= 6 && calls == 0);
                failat=allocations+conversion;
                require(!kprotect(execute,&process) && strcmp(message,exNomem) == 0);failat=0;
                memmove(&result,process.R.FP+104,sizeof(result));
                require(result == guard && fixedos == 0 && groupreleases == 0 && cachereleases == cacheacquires);
                fault=1;break;
            }
        }
        require(kprotect(execute,&process));
        if(dropping)require(fdclosed == dropbefore+(dropping == 2));
        if(pc->op == IMCALL){
            event=test->events+calls;memmove(&result,process.R.FP+104,sizeof(result));expected=0x1122334400000000ULL|(UWORD)event->status;
            if(groupkind == 8){
                require(result == fdguard);
                memmove(&fdresult,process.R.FP+96,sizeof(fdresult));
                if(event->status < 0)require(fdresult == H);
                else require(fdresult != H && D2H(fdresult)->t == TFD && fdresult->fd == event->status);
            }else if(result != expected){fprintf(stderr,"Sys fixed result predicate\n");exit(17);}
            require(groupwrong == 0xdeadbeefU && cachereleases == cacheacquires && !fixedinside);
            if(groupkind == 0)require(strcmp(worker.env->errstr,event->text) == 0 && fixedos == osbefore && groupcalls == pathbefore);
            else if(groupkind < 3)require(groupcalls == pathbefore+1 && strcmp(grouppath,event->text) == 0 && groupresult == event->status && fixedos == osbefore);
            else require(groupcalls == pathbefore && fixedos == osbefore+(groupkind == 4 && event->first <= 0 ? 0 : 1));
            calls++;
        }
        numericcollect(&process);
    }
    if(conversion)require(fault && !fixedinside);
    else{
        require(step<2048 && calls == test->calls && !fixedinside);
        require(!kprotect(execute,&process) && message[0] == 0);
    }
    destroystack(&R);managedprocess=nil;unload(sys);numericdrain();
    require(modules == nil && live == before && worker.nerr == 0 && Tstring.ref == 1 && Tmodlink.ref == 1 && cachereleases == cacheacquires && TFD->ref == 1 && fdgroup.r.ref == 1 && fdopened-openbefore == fdclosed-closebefore);
    dis64bootmanaged=0;dis64bootprovider=nil;
}

static void
fixedconstructors(int *native,int *heap)
{
    Module *sys;
    Modlink *caller;
    unsigned int i,j;
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
    start=allocations;poolstart=literalpoolcalls;
    sys->type[6]->map[1]=0x80;
    require(prepare64sysstring("bad-pair-map",sysfixed_cases[18].image,sysfixed_cases[18].bytes,0) == nil);
    sys->type[6]->map[1]=0xc0;
    sys->type[7]->size=80;
    require(prepare64sysstring("bad-bind-size",sysfixed_cases[18].image,sysfixed_cases[18].bytes,0) == nil);
    sys->type[7]->size=88;
    sys->ext[7].u.runt=Sys_unmount;
    require(prepare64sysstring("bad-bind-function",sysfixed_cases[18].image,sysfixed_cases[18].bytes,0) == nil);
    sys->ext[7].u.runt=Sys_bind;
    require(allocations == start && literalpoolcalls == poolstart && live == before && sys->ref == 1);
    for(j=0;j<nelem(sysfixed_cases) && sysfixed_cases[j].imports != 10;j++) ;
    require(j<nelem(sysfixed_cases));start=allocations;poolstart=literalpoolcalls;
    sys->type[8]->map[1]=0x80;
    require(prepare64sysstring("bad-fd-result-frame",sysfixed_cases[j].image,sysfixed_cases[j].bytes,0) == nil);
    sys->type[8]->map[1]=0;
    sys->type[9]->map[1]=0;
    require(prepare64sysstring("bad-fd-argument-frame",sysfixed_cases[j].image,sysfixed_cases[j].bytes,0) == nil);
    sys->type[9]->map[1]=0x80;
    sys->ext[8].u.runt=Sys_iounit;
    require(prepare64sysstring("bad-fd-function",sysfixed_cases[j].image,sysfixed_cases[j].bytes,0) == nil);
    sys->ext[8].u.runt=Sys_fildes;
    require(allocations == start && literalpoolcalls == poolstart && live == before && sys->ref == 1);
    unload(sys);numericdrain();dis64bootmanaged=0;dis64bootprovider=nil;
}

int
main(void)
{
    unsigned int i;
    int native=0,heap=0,fdallocationchecked=0,fdexceptionchecked=0;
    numericpoolinit();Tstring.size=sizeof(String);Tstring.free=freestring;Tstring.mark=noptrs;
    worker.env=&worker.defenv;worker.env->errstr=worker.defenv.errbuf0;
    fdgroup.r.ref=1;worker.env->fgrp=&fdgroup;
    {uchar map[]={0};TFD=dtype(freeFD,sizeof(FD),map,sizeof(map));require(TFD != nil);}
    optab[ILOAD]=iload;optab[IMOVP]=movp;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;optab[IEXIT]=iexit;
    optab[IMOVW]=movw;optab[IADDW]=addw;optab[ISUBW]=subw;optab[ISHLW]=shlw;optab[IBGEW]=bgew;optab[IBNEW]=bnew;optab[IBEQW]=beqw;optab[IBLEW]=blew;optab[IJMP]=jmp;
    fixedconstructors(&native,&heap);
    for(i=0;i<nelem(sysfixed_cases);i++){fixedexecute(sysfixed_cases+i,0,0);fixedexecute(sysfixed_cases+i,1,0);
        if(!fdexceptionchecked && strcmp(sysfixed_cases[i].events[0].method,"iounit") == 0 && sysfixed_cases[i].events[0].first < 0){
            fixedexecute(sysfixed_cases+i,0,-2);fixedexecute(sysfixed_cases+i,1,-2);fdexceptionchecked=1;
        }
        if(!fdallocationchecked && strcmp(sysfixed_cases[i].events[0].method,"fildes") == 0 && sysfixed_cases[i].events[0].first >= 0){
            fixedexecute(sysfixed_cases+i,0,-1);fixedexecute(sysfixed_cases+i,1,-1);
            fixedexecute(sysfixed_cases+i,0,-4);fixedexecute(sysfixed_cases+i,1,-4);
            fixedexecute(sysfixed_cases+i,0,-3);fixedexecute(sysfixed_cases+i,1,-3);fdallocationchecked=1;
        }
        if(sysfixed_cases[i].events[0].method && (strcmp(sysfixed_cases[i].events[0].method,"unmount") == 0 || strcmp(sysfixed_cases[i].events[0].method,"bind") == 0) &&
           (uchar)sysfixed_cases[i].events[0].text[0] >= 128){
            fixedexecute(sysfixed_cases+i,0,1);fixedexecute(sysfixed_cases+i,0,2);
            fixedexecute(sysfixed_cases+i,1,1);fixedexecute(sysfixed_cases+i,1,2);
        }}
    require(fdallocationchecked && fdexceptionchecked);freetype(TFD);TFD=nil;require(live == 0 && fdopened == fdclosed && fdgroup.r.ref == 1);
    printf("PASS mixed fixed Sys owning runtime: %u C/JIT/GC cases, 64/72/80/88-byte callee maps, exact scalar args/results/ordinals, FD lifetime/nil/replace/retire, 4 real mkfd allocation and 4 iounit native-error unwinds, release snapshots, 8 held-VM dual-conversion failures, %u wire plus 9 generated-entry pre-allocation refusals, %d native/%d heap rollback refusals\n",2*(unsigned int)nelem(sysfixed_cases),(unsigned int)nelem(sysfixed_bad),native,heap);
    return 0;
}
