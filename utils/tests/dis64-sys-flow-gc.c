/* Independent source event oracles, actual String-to-Word calls and root-disjoint scalar flow. */
#include "sys-flow-images.h"

static void
sysflowexecute(const SysFlowCase *test, int jit, int conversion)
{
    Module *sys;
    Modlink *caller;
    Prog process = {0};
    Frame *entry;
    Type *frame;
    Inst *pc;
    uvlong result, guard=0x11223344ffffffffULL;
    unsigned int step, calls=0;
    int before, fault=0;
    numericdrain();require(modules == nil);before=live;
    sys=sysstringmodule();dis64bootmanaged=1;dis64bootprovider="$Sys";
    caller=prepare64sysstring("sys-flow",test->image,test->bytes,jit);require(caller != nil);
    frame=caller->m->entryt;require(frame->size == 112 && frame->map[0] == 0 && frame->map[1] == 0x80);
    if(jit && caller->m->jit != nil){rxbegin=(uintptr)caller->m->jit->code;rxend=rxbegin+caller->m->jit->size;}
    caller->m->ss=4096;process.exval=H;process.state=Pready;process.quanta=1;
    process.R.M=caller;process.R.PC=caller->prog;
    entry=mallocz(frame->size,1);require(entry != nil);entry->t=frame;R.s=&entry;
    newstack(&process);free(entry);initmem(frame,process.R.FP);
    ptrdel(D2H(caller));managedprocess=&process;
    for(step=0;step<2048;step++){
        pc=process.R.PC;
        if(pc == caller->prog+caller->m->nprog-1)break;
        if(pc->op == IMCALL)memmove(process.R.FP+104,&guard,sizeof(guard));
        if(conversion && pc->op == IMCALL){
            failat=allocations+1;
            require(!kprotect(execute,&process) && strcmp(message,exNomem) == 0);
            failat=0;memmove(&result,process.R.FP+104,sizeof(result));require(result == guard);
            fault=2;break;
        }
        if(!kprotect(execute,&process)){
            require(test->fault && strcmp(message,exZdiv) == 0);fault=1;break;
        }
        if(pc->op == IMCALL){
            memmove(&result,process.R.FP+104,sizeof(result));
            if(result != 0x1122334400000000ULL){fprintf(stderr,"Sys flow Word-result predicate\n");exit(13);}
            if(calls >= test->calls || strcmp(worker.env->errstr,test->events[calls]) != 0){fprintf(stderr,"Sys flow event predicate: %s call %u\n",test->name,calls);exit(15);}
            calls++;
        }
        numericcollect(&process);
    }
    require(step<2048 && (conversion ? (calls == 0 && fault == 2) : (calls == test->calls && fault == test->fault)));
    if(!fault)require(!kprotect(execute,&process) && message[0] == 0);
    destroystack(&R);managedprocess=nil;unload(sys);numericdrain();
    require(modules == nil && live == before && worker.nerr == 0 && Tstring.ref == 1 && Tmodlink.ref == 1);
    dis64bootmanaged=0;dis64bootprovider=nil;
}

static void
sysflowadmission(void)
{
    Module *sys;
    Modlink *caller;
    unsigned int i;
    int before, attempts, pools;
    numericdrain();sys=sysstringmodule();numericdrain();before=live;
    dis64bootmanaged=1;dis64bootprovider="$Sys";
    for(i=0;i<nelem(sysflow_raw);i++){
        attempts=allocations;pools=literalpoolcalls;
        caller=prepare64sysstring("raw-flow",sysflow_raw[i].image,sysflow_raw[i].bytes,0);
        if((caller != nil) != sysflow_raw[i].accepted){fprintf(stderr,"Sys flow admission predicate\n");exit(14);}
        if(caller != nil){ptrdel(D2H(caller));destroy(caller);numericdrain();}
        else require(allocations == attempts && literalpoolcalls == pools);
        require(live == before && modules == sys && sys->ref == 1 && Tstring.ref == 1);
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
    optab[IMOVW]=movw;optab[IADDW]=addw;optab[ISUBW]=subw;optab[IMULW]=mulw;optab[IDIVW]=divw;
    optab[IMODW]=modw;optab[IANDW]=andw;optab[IORW]=orw;optab[IXORW]=xorw;optab[ISHLW]=shlw;optab[ISHRW]=shrw;
    optab[IBEQW]=beqw;optab[IBNEW]=bnew;optab[IBLTW]=bltw;optab[IBLEW]=blew;optab[IBGTW]=bgtw;optab[IBGEW]=bgew;optab[IJMP]=jmp;
    sysflowadmission();
    for(i=0;i<nelem(sysflow_cases);i++){
        SysStringCase construction={sysflow_cases[i].image,sysflow_cases[i].bytes,"",0,0};
        sysstringrollback(&construction,&native,&heap);
        sysflowexecute(sysflow_cases+i,0,0);sysflowexecute(sysflow_cases+i,1,0);
        if(sysflow_cases[i].calls && sysflow_cases[i].events[0][0]){sysflowexecute(sysflow_cases+i,0,1);conversions++;}
    }
    require(live == 0);
    printf("PASS Sys String scalar ownership: %u C/JIT/GC runs, exact call events and Word padding, %u CFG oracles, %d conversion unwinds, %d native/%d heap rollback refusals\n",2*(unsigned int)nelem(sysflow_cases),(unsigned int)nelem(sysflow_raw),conversions,native,heap);
    return 0;
}
