/* Actual dial wrapper, generated frame, owning loader/VM/GC and native FD boundary. */
#include "sys-dial-images.h"
static const DialEvent *dialevent;
static int dialnext,dialcalls;

int kclose(int fd){if(fd==7)return basekclose(fd);require(fd>=100 && fd<200 && fdlive[fd]);fdlive[fd]=0;fdclosed++;return 0;}
int kdial(char *address,char *local,char *directory,int *control)
{
    int data;
    require(cachereleases==cacheacquires+1 && !strcmp(address,dialevent->address) && !strcmp(local,dialevent->local));
    dialcalls++;groupcollect();
    if(!address[0] || !strcmp(address,"fail"))return -1;
    data=dialnext++;*control=dialnext++;require(*control<200 && !fdlive[data] && !fdlive[*control]);
    fdlive[data]=fdlive[*control]=1;fdopened+=2;strcpy(directory,"directory");return data;
}

static void
dialexecute(const DialCase *test,int jit,int failure,int failcall)
{
    Module *sys;Modlink *caller;Prog process={0};Frame *entry;Inst *pc;
    unsigned int step,calls=0;int before,opened=fdopened,closed=fdclosed,fault=0;
    numericdrain();before=live;sys=groupmodule();
    dis64bootmanaged=1;dis64bootprovider="$Sys";
    caller=prepare64sysstring("dial",test->image,test->bytes,jit);require(caller!=nil && caller->m->cstep==native64diallea8step);
    require(caller->m->entryt->size==112 && caller->m->entryt->map[1]==0xdc);
    caller->m->ss=4096;process.exval=H;process.state=Pready;process.quanta=1;process.R.M=caller;process.R.PC=caller->prog;
    entry=mallocz(112,1);require(entry!=nil);entry->t=caller->m->entryt;R.s=&entry;
    newstack(&process);free(entry);initmem(caller->m->entryt,process.R.FP);ptrdel(D2H(caller));managedprocess=&process;
    dialnext=100;dialcalls=0;groupkind=6;groupreleases=0;cachereleases=cacheacquires=0;
    for(step=0;step<2048;step++){
        uchar *callerfp=process.R.FP;
        pc=process.R.PC;if(pc==caller->prog+caller->m->nprog-1)break;
        if(failure==-1 && pc->op==ILEA){
            Frame *pending;uchar beforebind[8],afterbind[8];
            memmove(&pending,callerfp+40,sizeof(pending));memmove(beforebind,(uchar*)pending+32,8);
            pc->s.ind=88;
            require(!kprotect(execute,&process) && !strcmp(message,exBounds));
            memmove(afterbind,(uchar*)pending+32,8);require(!memcmp(beforebind,afterbind,8));
            pc->s.ind=80;fault=1;break;
        }
        if(pc->op==IMCALL){
            Sys_Connection old,after;WORD status,padding=0x12345678;int oldrefs[3],i;
            void *owners[3];
            require(calls<test->calls);dialevent=&test->events[calls];
            memmove(&old,callerfp+88,sizeof(old));memmove(owners,&old,sizeof(owners));
            for(i=0;i<3;i++)oldrefs[i]=owners[i]==H?0:D2H(owners[i])->ref;
            memmove(callerfp+84,&padding,4);
            if(failure>0 && calls==(unsigned int)failcall){
                int fdstart=fdopened,clstart=fdclosed;
                literalpoolfail=literalpoolcalls+failure;
                require(!kprotect(execute,&process) && !strcmp(message,exHeap));literalpoolfail=0;
                memmove(&after,callerfp+88,sizeof(after));require(!memcmp(&old,&after,sizeof(old)));
                for(i=0;i<3;i++)require(owners[i]==H || D2H(owners[i])->ref==oldrefs[i]);
                memmove(&status,callerfp+80,4);require(status==-1 && fdopened-fdstart==2 && fdclosed-clstart==2);
                require(cachereleases==cacheacquires);fault=1;break;
            }
            require(kprotect(execute,&process));
            memmove(&status,callerfp+80,4);memmove(&after,callerfp+88,sizeof(after));
            if(!dialevent->address[0] || !strcmp(dialevent->address,"fail"))require(status==-1 && after.dfd==H && after.cfd==H && after.dir==H);
            else require(status==0 && after.dfd!=H && after.cfd!=H && after.dfd!=after.cfd && after.dir!=H && !strcmp(string2c(after.dir),"directory"));
            memmove(&status,callerfp+84,4);require(status==padding && cachereleases==cacheacquires);calls++;
        }else require(kprotect(execute,&process));
        numericcollect(&process);
    }
    if(failure)require(fault);else{require(step<2048 && calls==test->calls && dialcalls==(int)calls);require(!kprotect(execute,&process) && !message[0]);}
    destroystack(&R);managedprocess=nil;unload(sys);numericdrain();
    require(live==before && modules==nil && TFD->ref==1 && SysConnectionStage.ref==1 && Tstring.ref==1 && Tmodlink.ref==1 && fdgroup.r.ref==1 && fdopened-opened==fdclosed-closed && !worker.nerr);
    dis64bootmanaged=0;dis64bootprovider=nil;
}

static void
dialtests(void)
{
    unsigned int i;int start,poolstart,before;Module *sys;Modlink *caller;
    for(i=0;i<nelem(dial_cases);i++){dialexecute(&dial_cases[i],0,0,0);dialexecute(&dial_cases[i],1,0,0);}
    dialexecute(&dial_cases[0],0,-1,0);dialexecute(&dial_cases[0],1,-1,0);
    for(i=1;i<=4;i++){
        dialexecute(&dial_cases[4],0,i,0);dialexecute(&dial_cases[4],1,i,0);
        dialexecute(&dial_cases[4],0,i,1);dialexecute(&dial_cases[4],1,i,1);
    }
    numericdrain();sys=groupmodule();dis64bootmanaged=1;dis64bootprovider="$Sys";before=live;
    for(i=0;i<nelem(dial_bad);i++){
        start=allocations;poolstart=literalpoolcalls;caller=prepare64sysstring("bad-dial",dial_bad[i].image,dial_bad[i].bytes,0);
        if(caller!=nil){fprintf(stderr,"Sys dial admission predicate\n");exit(24);}
        require(allocations==start && literalpoolcalls==poolstart && live==before && sys->ref==1);
    }
    caller=prepare64sysstring("dial",dial_cases[0].image,dial_cases[0].bytes,0);require(caller!=nil);
    start=allocations;poolstart=literalpoolcalls;ptrdel(D2H(caller));destroy(caller);numericdrain();
    /* Every partial caller construction must retire before it becomes executable. */
    for(i=1;i<=96;i++){
        failat=allocations+i;caller=prepare64sysstring("dial",dial_cases[0].image,dial_cases[0].bytes,0);failat=0;
        if(caller){ptrdel(D2H(caller));destroy(caller);numericdrain();break;}
        numericdrain();require(live==before && sys->ref==1);
    }
    require(i<96);
    unload(sys);numericdrain();dis64bootmanaged=0;dis64bootprovider=nil;
    printf("PASS Sys dial: 16 C/JIT/GC cases, 16 stage/FD/String allocation unwinds, 2 mapped-result binder refusals, %u pre-allocation refusals, constructor rollback and tuple padding\n",(unsigned int)nelem(dial_bad));
}
