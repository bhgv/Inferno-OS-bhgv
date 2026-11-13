#include "sys-connection-images.h"

static void
connectionexecute(const ConnectionCase *test,int jit,int failure)
{
    Module *sys;Modlink *caller;Prog process={0};Frame *entry;Inst *pc;
    unsigned int step,calls=0;int before,opened=fdopened,closed=fdclosed,fault=0;WORD value;
    numericdrain();before=live;sys=groupmodule();dis64bootmanaged=1;dis64bootprovider="$Sys";
    caller=prepare64sysstring("connection",test->image,test->bytes,jit);
    if(caller==nil){fprintf(stderr,"Sys connection owning predicate\n");exit(25);}
    require(caller->m->entryt->size==144 && caller->m->entryt->np==3 && caller->m->entryt->map[1]==0xdf && caller->m->entryt->map[2]==0);
    require(caller->m->ntype==3 && caller->m->type[2]==&Tbyte);caller->m->ss=4096;
    process.exval=H;process.state=Pready;process.quanta=1;process.R.M=caller;process.R.PC=caller->prog;
    entry=mallocz(144,1);require(entry!=nil);entry->t=caller->m->entryt;R.s=&entry;
    newstack(&process);free(entry);initmem(caller->m->entryt,process.R.FP);ptrdel(D2H(caller));managedprocess=&process;
    dialnext=100;dialcalls=0;groupreleases=0;cachereleases=cacheacquires=0;
    for(step=0;step<2048;step++){
        uchar *callerfp=process.R.FP;
        pc=process.R.PC;
        if(pc->op==IEXIT)break;
        if((failure==1 && pc->op==INEWA) || (failure==2 && pc->op==ICVTAC) || (failure==3 && pc->op==ICVTCA)){
            void *old,*after,*text;Sys_Connection beforetuple,aftertuple;unsigned int offset=failure==2?120:112;
            int refs[4]={TFD->ref,Tbyte.ref,Tarray.ref,Tstring.ref};
            memmove(&text,callerfp+120,sizeof(text));memmove(&old,callerfp+offset,sizeof(old));
            if(failure!=3 || text!=H){
                memmove(&beforetuple,callerfp+88,sizeof(beforetuple));
                literalpoolfail=literalpoolcalls+1;
                require(!kprotect(execute,&process) && !strcmp(message,exHeap));literalpoolfail=0;
                memmove(&after,callerfp+offset,sizeof(after));memmove(&aftertuple,callerfp+88,sizeof(aftertuple));
                require(old==after && !memcmp(&beforetuple,&aftertuple,sizeof(beforetuple)) &&
                        refs[0]==TFD->ref && refs[1]==Tbyte.ref && refs[2]==Tarray.ref && refs[3]==Tstring.ref);
                fault=1;break;
            }
        }

        if(pc->op==IMCALL){
            const SysFixedEvent *event;DialEvent request;uchar preserved[4],after[4];int kind;
            require(calls<test->calls);event=&test->events[calls];
            kind=!strcmp(event->method,"dial")?14:dis64sysfixedkind(event->method);
            require(kind>=0 && !strcmp(caller->m->ldt[0][pc->reg].name,event->method));
            groupkind=kind==14?6:kind;fixedexpected=event;
            request=(DialEvent){event->text,event->other?event->other:""};dialevent=&request;
            memmove(preserved,callerfp+132,4);
            if(!kprotect(execute,&process)){fprintf(stderr,"connection %s jit=%d method=%s: %s\n",test->name,jit,event->method,message);require(0);}
            if(kind==14){
                Sys_Connection result;
                memmove(&value,callerfp+80,4);memmove(&result,callerfp+88,sizeof(result));require(value==event->status);
                if(value<0)require(result.dfd==H && result.cfd==H && result.dir==H);
                else require(result.dfd!=H && result.cfd!=H && result.dir!=H);
            }else{
                memmove(&value,callerfp+128,4);require(value==event->status);
                memmove(after,callerfp+132,4);require(!memcmp(preserved,after,4));
                if(kind==0)require(!strcmp(worker.env->errstr,event->text));
            }
            require(cachereleases==cacheacquires);calls++;
        }else if(!kprotect(execute,&process)){fprintf(stderr,"connection %s jit=%d op=%d: %s\n",test->name,jit,pc->op,message);require(0);}
        numericcollect(&process);
    }
    require(step<2048 && (failure?fault:calls==test->calls));
    memmove(&value,process.R.FP+80,4);require(value==test->status);
    if(!failure)require(!kprotect(execute,&process) && !message[0]);
    destroystack(&R);managedprocess=nil;unload(sys);numericdrain();
    require(live==before && modules==nil && TFD->ref==1 && Tbyte.ref==1 && Tarray.ref==1 && Tstring.ref==1 && SysConnectionStage.ref==1 && fdgroup.r.ref==1 && fdopened-opened==fdclosed-closed && !worker.nerr);
    dis64bootmanaged=0;dis64bootprovider=nil;
}

static void
connectiontests(void)
{
    Module *sys;Modlink *caller;unsigned int i;int before,start,poolstart,attempts,pools,failure;
    for(i=0;i<nelem(connection_cases);i++){connectionexecute(connection_cases+i,0,0);connectionexecute(connection_cases+i,1,0);}
    for(i=1;i<=3;i++){connectionexecute(connection_cases,0,i);connectionexecute(connection_cases,1,i);}
    numericdrain();sys=groupmodule();dis64bootmanaged=1;dis64bootprovider="$Sys";before=live;
    for(i=0;i<nelem(connection_bad);i++){
        start=allocations;poolstart=literalpoolcalls;
        caller=prepare64sysstring("bad-connection",connection_bad[i].image,connection_bad[i].bytes,0);
        if(caller!=nil){fprintf(stderr,"Sys connection admission predicate\n");exit(26);}
        require(allocations==start && literalpoolcalls==poolstart && live==before && sys->ref==1);
    }
    start=allocations;poolstart=literalpoolcalls;
    caller=prepare64sysstring("connection",connection_cases[0].image,connection_cases[0].bytes,0);require(caller!=nil);
    attempts=allocations-start;pools=literalpoolcalls-poolstart;ptrdel(D2H(caller));destroy(caller);numericdrain();require(live==before);
    for(failure=1;failure<=attempts;failure++){
        failat=allocations+failure;require(prepare64sysstring("connection",connection_cases[0].image,connection_cases[0].bytes,0)==nil);failat=0;
        numericdrain();require(live==before && sys->ref==1 && Tbyte.ref==1 && !worker.nerr);
    }
    for(failure=1;failure<=pools;failure++){
        literalpoolfail=literalpoolcalls+failure;require(prepare64sysstring("connection",connection_cases[0].image,connection_cases[0].bytes,0)==nil);literalpoolfail=0;
        numericdrain();require(live==before && sys->ref==1 && Tbyte.ref==1 && !worker.nerr);
    }
    unload(sys);numericdrain();dis64bootmanaged=0;dis64bootprovider=nil;
    printf("PASS Sys connection: %u C/JIT/GC cases, status guards, FD fields, short/error reads, UTF/string/byte flow, durable Words; 6 live-Connection allocation unwinds; %u pre-allocation refusals, %d native/%d heap constructor rollback refusals\n",2*(unsigned int)nelem(connection_cases),(unsigned int)nelem(connection_bad),attempts,pools);
}
