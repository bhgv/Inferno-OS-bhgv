/* Real native wrappers, isolated OS boundary and saved-frame mutation during release. */
#include "sys-group-images.h"
static int groupkind, groupcalls, groupvictim, groupresult, groupreleases;
static UWORD groupwrong;
static char grouppath[256];

static void
groupcollect(void)
{
    require(managedprocess != nil);
    managedprocess->R=R;
    numericcollect(managedprocess);
}

/* Retarget borrowed frame fields after the real wrapper captured its own operands.
 * Retire the replaced argument reference exactly once; caller/MP roots stay authoritative. */
static void
grouprelease(void)
{
    void *old, *empty=H, *wrong=&groupwrong;
    require(groupkind == 1 || groupkind == 2);
    memmove(&old,R.FP+64,sizeof(old));
    memmove(R.FP+64,&empty,sizeof(empty));destroy(old);
    memmove(R.FP+32,&wrong,sizeof(wrong));
    groupreleases++;groupcollect();
}

static int
groupnative(int kind, char *path)
{
    int result;
    require(cachereleases == cacheacquires+1 && groupkind == kind && strlen(path)<sizeof(grouppath));
    strcpy(grouppath,path);groupcalls++;
    if(kind == 1)result=strcmp(path,"/ok") == 0 ? 0 : -1;
    else {result=strcmp(path,"/victim") == 0 && groupvictim ? 0 : -1;if(result == 0)groupvictim=0;}
    groupresult=result;groupcollect();return result;
}
int kchdir(char *path){return groupnative(1,path);}
int kremove(char *path){return groupnative(2,path);}

static Module*
groupmodule(void)
{
    Module *module;
    Type *frame;
    const LimboNative64Entry *entry;
    unsigned int i;
    module=mallocz(sizeof(*module),1);require(module != nil);
    module->ref=1;module->origmp=H;module->name=strdup("Sys");module->path=strdup("$Sys");
    module->type=mallocz(3*sizeof(Type*),1);module->ext=mallocz(4*sizeof(Link),1);
    require(module->name && module->path && module->type && module->ext);
    for(i=0;i<3;i++){
        entry=native64sysio[i];frame=dtype(freeheap,entry->frame_size,(uchar*)entry->map,entry->map_bytes);require(frame != nil);
        module->type[i]=frame;module->ntype++;
        newlink(module->ext+i,(char*)entry->name,entry->legacy_signature,frame);module->ext[i].u.runt=entry->function;
    }
    modules=module;return module;
}

static void
groupexecute(const SysGroupCase *test, int jit, int conversion)
{
    Module *sys;
    Modlink *caller,*loaded;
    Prog process={0};
    Frame *entry;
    Type *frame;
    Inst *pc;
    const SysGroupEvent *event;
    uvlong result,guard=0x1122334455667788ULL,expected;
    unsigned int step,calls=0,ordinal;
    int before,fault=0,nativebefore;
    numericdrain();require(modules == nil);before=live;sys=groupmodule();
    dis64bootmanaged=1;dis64bootprovider="$Sys";
    caller=prepare64sysstring("sys-group",test->image,test->bytes,jit);require(caller != nil);
    for(ordinal=0;ordinal<test->imports;ordinal++)require(strcmp(caller->m->ldt[0][ordinal].name,test->names[ordinal]) == 0);
    require(caller->m->ldt[0][test->imports].name == nil);
    frame=caller->m->entryt;caller->m->ss=4096;
    if(jit && caller->m->jit != nil){rxbegin=(uintptr)caller->m->jit->code;rxend=rxbegin+caller->m->jit->size;}
    process.exval=H;process.state=Pready;process.quanta=1;process.R.M=caller;process.R.PC=caller->prog;
    entry=mallocz(frame->size,1);require(entry != nil);entry->t=frame;R.s=&entry;
    newstack(&process);free(entry);initmem(frame,process.R.FP);ptrdel(D2H(caller));managedprocess=&process;
    groupcalls=groupreleases=0;groupvictim=1;groupkind=0;cachereleases=cacheacquires=0;
    for(step=0;step<2048;step++){
        pc=process.R.PC;if(pc == caller->prog+caller->m->nprog-1)break;
        nativebefore=groupcalls;
        if(pc->op == IMCALL){
            require(calls<test->calls);event=test->events+calls;ordinal=pc->reg;
            require(ordinal<test->imports && strcmp(test->names[ordinal],event->method) == 0);
            groupkind=strcmp(event->method,"chdir") == 0 ? 1 : strcmp(event->method,"remove") == 0 ? 2 : 0;
            memmove(&loaded,caller->MP,sizeof(loaded));require(loaded != nil && loaded != H && loaded->m == sys);
            require(loaded->links[ordinal].u.runt == (groupkind==1 ? Sys_chdir : groupkind==2 ? Sys_remove : Sys_werrstr));
            groupwrong=0xdeadbeefU;memmove(process.R.FP+frame->size-8,&guard,sizeof(guard));
            if(conversion){
                failat=allocations+1;require(!kprotect(execute,&process) && strcmp(message,exNomem) == 0);failat=0;
                memmove(&result,process.R.FP+frame->size-8,sizeof(result));require(result == guard);
                require(groupcalls == 0 && groupreleases == 0 && cachereleases == cacheacquires);fault=1;break;
            }
        }
        require(kprotect(execute,&process));
        if(pc->op == IMCALL){
            event=test->events+calls;memmove(&result,process.R.FP+frame->size-8,sizeof(result));
            expected=0x1122334400000000ULL|(UWORD)event->status;
            if(result != expected){fprintf(stderr,"Sys group result predicate\n");exit(17);}
            require(groupwrong == 0xdeadbeefU && cachereleases == cacheacquires);
            if(groupkind){require(groupcalls == nativebefore+1 && strcmp(grouppath,event->argument) == 0 && groupresult == event->status);}
            else require(groupcalls == nativebefore && strcmp(worker.env->errstr,event->argument) == 0);
            calls++;
        }
        numericcollect(&process);
    }
    require(step<2048 && (conversion ? fault && calls==0 : calls==test->calls));
    if(!fault)require(!kprotect(execute,&process) && message[0] == 0);
    destroystack(&R);managedprocess=nil;unload(sys);numericdrain();
    require(modules == nil && live == before && worker.nerr == 0 && Tstring.ref == 1 && Tmodlink.ref == 1 && cachereleases == cacheacquires);
    dis64bootmanaged=0;dis64bootprovider=nil;
}

static void
groupconstructors(int *native, int *heap)
{
    Module *sys;
    Modlink *caller;
    unsigned int i;
    int before,start,poolstart,attempts,pools,failure;
    numericdrain();sys=groupmodule();numericdrain();before=live;dis64bootmanaged=1;dis64bootprovider="$Sys";
    for(i=0;i<nelem(sysgroup_bad);i++){
        start=allocations;poolstart=literalpoolcalls;
        caller=prepare64sysstring("bad-group",sysgroup_bad[i].image,sysgroup_bad[i].bytes,0);
        if(caller != nil){fprintf(stderr,"Sys group admission predicate\n");exit(14);}
        require(allocations == start && literalpoolcalls == poolstart && live == before && sys->ref == 1);
    }
    for(i=0;i<nelem(sysgroup_cases);i++){
        start=allocations;poolstart=literalpoolcalls;
        caller=prepare64sysstring("group",sysgroup_cases[i].image,sysgroup_cases[i].bytes,0);require(caller != nil);
        attempts=allocations-start;pools=literalpoolcalls-poolstart;
        ptrdel(D2H(caller));destroy(caller);numericdrain();require(live == before);
        for(failure=1;failure<=attempts;failure++){
            failat=allocations+failure;require(prepare64sysstring("group",sysgroup_cases[i].image,sysgroup_cases[i].bytes,0) == nil);
            failat=0;numericdrain();require(live == before && modules == sys && sys->ref == 1 && worker.nerr == 0 && Tstring.ref == 1);
        }
        for(failure=1;failure<=pools;failure++){
            literalpoolfail=literalpoolcalls+failure;require(prepare64sysstring("group",sysgroup_cases[i].image,sysgroup_cases[i].bytes,0) == nil);
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
    int native=0,heap=0,conversions=0;
    numericpoolinit();Tstring.size=sizeof(String);Tstring.free=freestring;Tstring.mark=noptrs;
    worker.env=&worker.defenv;worker.env->errstr=worker.defenv.errbuf0;
    optab[ILOAD]=iload;optab[IMOVP]=movp;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;optab[IEXIT]=iexit;
    optab[IMOVW]=movw;optab[IADDW]=addw;optab[IDIVW]=divw;optab[IBNEW]=bnew;optab[IBEQW]=beqw;optab[IBLEW]=blew;optab[IJMP]=jmp;
    groupconstructors(&native,&heap);
    for(i=0;i<nelem(sysgroup_cases);i++){groupexecute(sysgroup_cases+i,0,0);groupexecute(sysgroup_cases+i,1,0);if(sysgroup_cases[i].events[0].argument[0]){groupexecute(sysgroup_cases+i,0,1);conversions++;}}
    require(live == 0);
    printf("PASS fixed Sys groups owning runtime: %u C/JIT/GC runs, %u held-VM conversion failures, exact statuses/ordinals, release-field snapshots, %u pre-allocation refusals, %d native/%d heap rollback refusals\n",2*(unsigned int)nelem(sysgroup_cases),(unsigned int)conversions,(unsigned int)nelem(sysgroup_bad),native,heap);
    return 0;
}
