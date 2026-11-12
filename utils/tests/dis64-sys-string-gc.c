/* Execute the actual Sys handler after every serialized preparation step. */
#include "sys-string-images.h"

static Module*
sysstringmodule(void)
{
    Module *module;
    Type *frame;
    const LimboNative64Entry *entry = &limbo_native64_Sys_werrstr_entry;
    module = mallocz(sizeof(*module),1); require(module != nil);
    module->ref = 1; module->origmp = H;
    module->name = strdup("Sys"); module->path = strdup("$Sys");
    module->type = mallocz(sizeof(Type*),1); require(module->name && module->path && module->type);
    frame = dtype(freeheap,entry->frame_size,(uchar*)entry->map,entry->map_bytes); require(frame != nil);
    module->type[0] = frame; module->ntype = 1;
    module->ext = mallocz(2*sizeof(Link),1); require(module->ext != nil);
    newlink(&module->ext[0],(char*)entry->name,entry->legacy_signature,frame);
    module->ext[0].u.runt = entry->function;
    modules = module;
    return module;
}

static void
sysstringexpected(const SysStringCase *test)
{
    char expected[ERRMAX];
    unsigned int size = strlen(test->expected);
    if(size < ERRMAX) memmove(expected,test->expected,size+1);
    else {
        /* The only overlong expected error is ASCII, so truncation cannot split UTF-8. */
        memset(expected,'x',ERRMAX-5); memmove(expected+ERRMAX-5,"...",4);
    }
    if(strcmp(worker.env->errstr,expected) != 0){
        fprintf(stderr,"Sys String error-text predicate: actual=[%s] expected=[%s]\n",worker.env->errstr,expected); exit(12);
    }
}

static void
sysstringexecute(const SysStringCase *test, int jit, int failure)
{
    Module *sys;
    Modlink *caller, *loaded;
    Prog process = {0};
    Frame *entry;
    Type *frame;
    Inst *pc;
    uvlong result, guard = 0x11223344ffffffffULL;
    unsigned int step, calls = 0;
    int before;
    numericdrain(); require(modules == nil); before = live;
    sys = sysstringmodule();
    dis64bootmanaged = 1; dis64bootprovider = "$Sys";
    caller = prepare64sysstring("sys-string",test->image,test->bytes,jit); require(caller != nil);
    require(caller->m->cstep == native64stringlea8step && caller->m->entryt->map[1] == 0x80);
    frame = caller->m->entryt; caller->m->ss = 4096;
    if(jit && caller->m->jit != nil){rxbegin=(uintptr)caller->m->jit->code;rxend=rxbegin+caller->m->jit->size;}
    process.exval = H; process.state = Pready; process.quanta = 1;
    process.R.M = caller; process.R.PC = caller->prog;
    entry = mallocz(frame->size,1); require(entry != nil);
    entry->t = frame; R.s = &entry; newstack(&process); free(entry); initmem(frame,process.R.FP);
    ptrdel(D2H(caller)); managedprocess = &process;
    for(step=0; step<512; step++){
        pc = process.R.PC;
        if(pc == caller->prog+caller->m->nprog-1) break;
        if(pc->op == IMCALL) memmove(process.R.FP+72,&guard,sizeof(guard));
        if(failure && pc->op == IMCALL){
            failat=allocations+1;
            require(!kprotect(execute,&process) && strcmp(message,exNomem) == 0);
            failat=0;
            memmove(&result,process.R.FP+72,sizeof(result));
            require(result == guard);
            break;
        }
        require(kprotect(execute,&process));
        if(pc->op == ILOAD){
            memmove(&loaded,caller->MP,sizeof(loaded));
            require(loaded != nil && loaded != H && loaded->m == sys && loaded->links[0].u.runt == Sys_werrstr);
        }
        if(pc->op == IMCALL){
            calls++; memmove(&result,process.R.FP+72,sizeof(result));
            if(result != 0x1122334400000000ULL){fprintf(stderr,"Sys String Word-result predicate\n");exit(13);}
            if(test->same || calls == test->calls) sysstringexpected(test);
        }
        numericcollect(&process);
    }
    require(step < 512 && (failure || calls == test->calls));
    if(!failure) require(!kprotect(execute,&process) && message[0] == 0);
    destroystack(&R); managedprocess = nil;
    unload(sys); numericdrain();
    require(modules == nil && live == before && worker.nerr == 0 && Tstring.ref == 1 && Tmodlink.ref == 1);
    dis64bootmanaged = 0; dis64bootprovider = nil;
}

/* Fail every native/heap constructor allocation with the real builtin held independently. */
static void
sysstringrollback(const SysStringCase *test, int *native, int *heap)
{
    Module *sys;
    Modlink *caller;
    int before, start, poolstart, attempts, poolattempts, failure;
    numericdrain(); sys = sysstringmodule(); numericdrain(); before = live;
    dis64bootmanaged = 1; dis64bootprovider = "$Sys";
    start=allocations; poolstart=literalpoolcalls;
    caller=prepare64sysstring("sys-string",test->image,test->bytes,0);require(caller != nil);
    attempts=allocations-start; poolattempts=literalpoolcalls-poolstart;
    ptrdel(D2H(caller));destroy(caller);numericdrain();require(live == before && modules == sys);
    for(failure=1; failure<=attempts; failure++){
        failat=allocations+failure;
        require(prepare64sysstring("sys-string",test->image,test->bytes,0) == nil);
        failat=0;numericdrain();require(live == before && modules == sys && sys->ref == 1 && worker.nerr == 0 && Tstring.ref == 1);
    }
    for(failure=1; failure<=poolattempts; failure++){
        literalpoolfail=literalpoolcalls+failure;
        require(prepare64sysstring("sys-string",test->image,test->bytes,0) == nil);
        literalpoolfail=0;numericdrain();require(live == before && modules == sys && sys->ref == 1 && worker.nerr == 0 && Tstring.ref == 1);
    }
    *native+=attempts;*heap+=poolattempts;
    unload(sys);numericdrain();require(modules == nil);
    dis64bootmanaged=0;dis64bootprovider=nil;
}


/* Malformed images and a tampered builtin are rejected before the first owner allocation. */
static void
sysstringrefusals(void)
{
    Module *sys;
    int before, attempts, pools, sig;
    unsigned int i;
    numericdrain();sys=sysstringmodule();before=live;
    dis64bootmanaged=1;dis64bootprovider="$Sys";
    attempts=allocations;pools=literalpoolcalls;
    for(i=0;i<nelem(sysstring_bad);i++){
        require(prepare64sysstring("bad-sys-string",sysstring_bad[i].image,sysstring_bad[i].bytes,0) == nil);
        require(allocations == attempts && literalpoolcalls == pools && live == before && sys->ref == 1);
    }
    sig=sys->ext[0].sig;sys->ext[0].sig^=1;
    require(prepare64sysstring("bad-sys-export",sysstring_cases[0].image,sysstring_cases[0].bytes,0) == nil);
    sys->ext[0].sig=sig;
    sys->type[0]->map[1]=0;
    require(prepare64sysstring("bad-sys-map",sysstring_cases[0].image,sysstring_cases[0].bytes,0) == nil);
    sys->type[0]->map[1]=0x80;
    require(allocations == attempts && literalpoolcalls == pools && live == before && sys->ref == 1);
    unload(sys);numericdrain();require(modules == nil);
    dis64bootmanaged=0;dis64bootprovider=nil;
    printf("PASS Sys String pre-allocation refusals: %u malformed images and two native export mutations\n",(unsigned int)nelem(sysstring_bad));
}

int
main(void)
{
    unsigned int i;
    int native=0, heap=0;
    numericpoolinit();
    Tstring.size=sizeof(String);Tstring.free=freestring;Tstring.mark=noptrs;
    worker.env=&worker.defenv;worker.env->errstr=worker.defenv.errbuf0;
    optab[ILOAD]=iload;optab[IMOVP]=movp;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;optab[IEXIT]=iexit;
    sysstringrefusals();
    for(i=0;i<nelem(sysstring_cases);i++){
        sysstringrollback(sysstring_cases+i,&native,&heap);
        sysstringexecute(sysstring_cases+i,0,0);sysstringexecute(sysstring_cases+i,1,0);
        if(i != 10 && i != 11)sysstringexecute(sysstring_cases+i,0,1);
    }
    require(live == 0);
    printf("PASS serialized Sys String owning runtime: %u C/JIT/GC runs, 12 conversion-error unwinds, exact error text and Word result, %d native/%d heap rollback refusals\n",2*(unsigned int)nelem(sysstring_cases),native,heap);
    return 0;
}
