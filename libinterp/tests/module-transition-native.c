
void (*optab[256])(void); // Real complete-entry C handler table.
char exOp[]="opcode"; // Invalid decoded opcode error identity.
#define OP(name) void name(void) // Borrow the actual interpreter operation definitions.

typedef struct TransitionOwner TransitionOwner;

struct TransitionOwner {
    Heap heap; // Native header immediately before the module owner.
    Modlink link; // Module instance retained through the fixture's destructor boundary.
};

static TransitionOwner transcurrent, transother, transresult; // Stable, independently counted module owners.

static struct {
    Heap heap; // Native owner count and GC color used by actual self-load code.
    uchar data[32]; // Pointer-free module data, retained only on successful self linking.
} transdata;
static Module transmodule, transexternal; // Original module and separately loaded module.
static Import transimports[2]; // Exact selected import identity and sentinel.
static uchar transtable[3*sizeof(Import*)+8]; // Import pointer table at all byte alignments.
static uchar transslots[3][40], transpoison[40]; // Operand cells and an unrelated output canary.
static String transname; // Borrowed conversion source; spelling is supplied at its native boundary.
static void *transdestination; // Actual output cell observed during destruction.
static int transself, transstage, transfailure, transmutation; // Selected spelling, callback stage, failure and mutation scope.
static int transconverts, transreads, translinks, transdestroys; // Real handler boundary observations.
static void *transdestroyed; // Old owner released after captured output publication.


/* Keep every predicate active in release-style builds. */
static void
transitioncheck(int condition)
{
    if(!condition)abort();
}


/* Mutate live decoded registers while preserving all borrowed source object lifetimes. */
static void
transitionhook(int stage)
{
    if(stage!=transstage)return;
    if(transmutation!=2){R.d=transpoison;R.s=H;R.m=H;}
    if(transmutation!=1)R.M=&transother.link;
}


/* A native conversion boundary may change registers before returning a stable spelling. */
char*
string2c(String *name)
{
    transitioncheck(name==&transname);
    transconverts++;
    transitionhook(1);
    return transself ? "$self" : "external";
}


/* Keep cache lookup and filesystem reading distinguishable without replacing LOAD logic. */
Module*
lookmod(char *name)
{
    transitioncheck(!strcmp(name,"external"));
    return nil;
}


/* Return an owned external Module through the same callback boundary as the real reader. */
Module*
readmod(char *name,Module *cached,int sync)
{
    transitioncheck(!strcmp(name,"external") && cached==nil && sync==1);
    transreads++;
    transexternal.ref=1;
    transitionhook(2);
    return &transexternal;
}


/* Model the linker's acquired-Module ownership contract at its native call boundary. */
Modlink*
linkmod(Module *module,Import *imports,int copydata)
{
    transitioncheck(imports==&transimports[1] && module->ref>0);
    transitioncheck(module==(transself ? &transmodule : &transexternal) && copydata==!transself);
    translinks++;
    transitionhook(3);
    if(transfailure){
        module->ref--;
        if(transfailure==2)error(exHeap);
        return H;
    }
    transresult.link.m=module;
    transresult.link.MP=H;
    return &transresult.link;
}


/* Observe publication at the captured cell, then release the exact previous owner. */
void
destroy(void *owner)
{
    Modlink *published;
    Heap *heap;

    if(owner==H)return;
    memmove(&published,transdestination,sizeof(published));
    transitioncheck(published==(transfailure ? H : &transresult.link));
    transitioncheck(owner==&transcurrent.link || owner==&transother.link);
    heap=D2H(owner);
    transitioncheck(heap->ref==3);
    heap->ref--;
    transdestroyed=owner;
    transdestroys++;
}

IMPLEMENTATION


/* Build real native owner headers and optionally unaligned import pointer cells. */
static void
transitionloadimage(int sa,int ma,int da,int tablealign,int data,int alias)
{
    static Import *table[3]; // Stable callback fixture image across error unwinds.
    WORD index=1;
    String *name=&transname;
    Modlink *old=&transcurrent.link;

    memset(&transcurrent,0,sizeof(transcurrent));memset(&transother,0,sizeof(transother));
    memset(&transresult,0,sizeof(transresult));memset(&transdata,0,sizeof(transdata));
    memset(&transmodule,0,sizeof(transmodule));memset(&transexternal,0,sizeof(transexternal));
    memset(transslots,0xa5,sizeof(transslots));memset(transpoison,0x5a,sizeof(transpoison));
    memset(&R,0,sizeof(R));
    transcurrent.heap.ref=transother.heap.ref=3;transresult.heap.ref=1;
    transdata.heap.ref=5;transmodule.ref=5;transmodule.rt=HASLDT;
    transcurrent.link.m=&transmodule;transcurrent.link.MP=data ? transdata.data : H;
    transother.link.m=&transexternal;transother.link.MP=H;
    table[0]=&transimports[0];table[1]=&transimports[1];table[2]=nil;
    memmove(transtable+tablealign,table,sizeof(table));transmodule.ldt=(Import**)(transtable+tablealign);
    R.M=&transcurrent.link;R.s=transslots[0]+sa;R.m=transslots[1]+ma;
    R.d=alias ? (void*)&R.M : transslots[2]+da;transdestination=R.d;
    memmove(R.s,&name,sizeof(name));memmove(R.m,&index,sizeof(index));
    if(!alias)memmove(R.d,&old,sizeof(old));
    transconverts=transreads=translinks=transdestroys=0;transdestroyed=nil;raised=nil;
    mutator=2;nprop=0;
}


/* Check captured output, source MP and exact retained owners across C and complete RX tails. */
static unsigned long
transitionloads(Jit *jit)
{
    int sa,ma,da,data,alias,stage,failure,route,ok,aligned;
    Modlink *published,*old;
    static uchar poison[40];
    unsigned long checks=0;

    for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++)
    for(transself=0;transself<2;transself++)for(data=0;data<2;data++)for(alias=0;alias<2;alias++)
    for(stage=0;stage<4;stage++)for(failure=0;failure<3;failure++)for(route=0;route<2;route++){
        if(transself && stage==2)continue;
        if(getenv("LOAD_STAGE") && stage!=atoi(getenv("LOAD_STAGE")))continue;
        aligned=getenv("LOAD_TABLE_ALIGNED") ? 0 : sa;
        transstage=stage;transfailure=failure;
        transmutation=getenv("LOAD_MUTATION") ? atoi(getenv("LOAD_MUTATION")) : 0;
        transitionloadimage(sa,ma,da,aligned,data,alias);
        memmove(poison,transpoison,sizeof(poison));
        ok=0;
        if(setjmp(boundary)==0){if(route==0)iload();else transitioncheck(jit->dispatch[ILOAD](&R)==1);ok=1;}
        transitioncheck(ok==(failure!=2) && transconverts==1 && transreads==!transself && translinks==1);
        memmove(&published,transdestination,sizeof(published));
        old=alias && stage!=0 && transmutation!=1 ? &transother.link : &transcurrent.link;
        transitioncheck(transdestroys==(failure!=2));
        if(failure==2)transitioncheck(raised==exHeap && published==old && transmodule.ref==5);
        else{
            transitioncheck(published==(failure ? H : &transresult.link) && transdestroyed==old);
            transitioncheck(D2H(old)->ref==2);
        }
        transitioncheck(transmodule.ref==5+(transself && !failure));
        transitioncheck(transdata.heap.ref==5+(transself && data && !failure));
        transitioncheck(nprop==(transself && data && !failure));
        if(!failure && transself)transitioncheck(transresult.link.MP==(data ? transdata.data : H));
        transitioncheck(!memcmp(poison,transpoison,sizeof(poison)));
        checks++;
    }
    return checks;
}


/* Reject malformed decoded cells and import-table spans before any native callback. */
static unsigned long
transitionloaderrors(Jit *jit)
{
    int field,offset,route,failed;
    void *bad;
    static REG saved;
    unsigned long checks=0;

    for(field=0;field<7;field++)for(offset=0;offset<40;offset++)for(route=0;route<2;route++){
        if(getenv("LOAD_ERROR_FIELD") && field!=atoi(getenv("LOAD_ERROR_FIELD")))continue;
        if(field<3 && offset> (field==1 ? sizeof(WORD) : sizeof(void*)))continue;
        if(field==3 && (offset==0 || offset>sizeof(Modlink)))continue;
        if(field==4 && (offset==0 || offset>sizeof(void*)))continue;
        if(field==5 && (offset==0 || offset>sizeof(Heap)))continue;
        if(field==6 && (offset==0 || offset>=sizeof(void*)))continue;
        transself=field>=5;transstage=0;transfailure=0;
        transitionloadimage(0,0,0,0,0,0);
        bad=offset==0 ? nil : (void*)((uintptr)0-(uintptr)offset);
        if(field==0)R.s=bad;
        if(field==1)R.m=bad;
        if(field==2)R.d=bad;
        if(field==3)R.M=bad;
        if(field==4)transmodule.ldt=bad;
        if(field==5)transcurrent.link.MP=(uchar*)(uintptr)offset;
        if(field==6)transcurrent.link.MP=transdata.data+offset;
        memmove(&saved,&R,sizeof(R));failed=0;
        if(setjmp(boundary)==0){if(route==0)iload();else jit->dispatch[ILOAD](&R);}else failed=1;
        transitioncheck(failed && transconverts==(field>=5) && transreads==0 && translinks==0 && transdestroys==0);
        transitioncheck(!memcmp(&saved,&R,sizeof(R)) && transmodule.ref==5 && transdata.heap.ref==5);
        checks++;
    }
    return checks;
}


/* Restore the captured callee frame even if a native builtin changes global FP. */
static unsigned long
transitionbuiltins(Jit *jit)
{
    static uchar frames[1024],types[sizeof(Type)+8],cells[3][32];
    static Frame frame,expected; // Stable frame images across native errors.
    Frame *argument;
    static REG saved,wanted;
    static Type type;
    Modlink *callee=&transresult.link;
    WORD index=0;
    int sa,ma,da,fa,ta,mode,route;
    unsigned long checks=0;

    type.ref=1;type.size=sizeof(Frame)+32;
    for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++)for(fa=0;fa<8;fa++)
    for(ta=0;ta<8;ta++)for(mode=0;mode<2;mode++)for(route=0;route<3;route++){
        if(getenv("BUILTIN_MODE") && mode!=atoi(getenv("BUILTIN_MODE")))continue;
        if(getenv("BUILTIN_TYPE_ALIGN") && ta!=atoi(getenv("BUILTIN_TYPE_ALIGN")))continue;
        memset(&R,0,sizeof(R));memset(&transresult,0,sizeof(transresult));
        memset(&transcurrent,0,sizeof(transcurrent));memset(frames,0xa5,sizeof(frames));
        memset(&frame,0,sizeof(frame));memmove(types+ta,&type,sizeof(type));
        transresult.heap.ref=5;callee->nlinks=1;callee->m=&transmodule;
        transcurrent.link.m=&transmodule;transcurrent.link.MP=H;
        callee->links[0].u.runt=builtin;argument=(Frame*)(frames+64+fa);
        frame.t=(Type*)(types+ta);memmove(argument,&frame,sizeof(frame));
        R.M=&transcurrent.link;R.MP=H;R.PC=(Inst*)(frames+800);R.FP=frames+512;R.SP=frames+600;R.IC=29;R.t=17;
        R.s=cells[0]+sa;R.m=cells[1]+ma;R.d=cells[2]+da;
        memmove(R.s,&argument,sizeof(argument));memmove(R.m,&index,sizeof(index));memmove(R.d,&callee,sizeof(callee));
        memmove(&saved,&R,sizeof(R));memmove(&wanted,&R,sizeof(R));
        wanted.SP=(uchar*)argument;wanted.t=0;
        expected=frame;expected.lr=saved.PC;expected.fp=saved.FP;expected.mr=saved.M;
        transitionbuiltinmode=mode;transitionbuiltinfp=frames+700;callbacks=transitionfreecalls=0;
        if(route==0)mcall();
        else if(route==1){transitioncheck(!jit->entry[IMCALL](&R) && !memcmp(&saved,&R,sizeof(R)));mcall();}
        else transitioncheck(jit->dispatch[IMCALL](&R)==1);
        transitioncheck(callbacks==1 && transitionfreecalls==0 && transresult.heap.ref==5 && !memcmp(&R,&wanted,sizeof(R)));
        transitioncheck(!memcmp(argument,&expected,sizeof(expected)));
        checks++;
    }
    transitionbuiltinmode=0;
    return checks;
}


/* Reject invalid callback-return metadata before decrementing owners or publishing caller state. */
static unsigned long
transitionbuiltinerrors(Jit *jit)
{
    static uchar frames[1024],types[sizeof(Type)+8],cells[3][32];
    static Frame frame;
    Frame *argument;
    static Type type;
    Modlink *callee=&transresult.link;
    WORD index=0;
    int kind,offset,route,failed,shift;
    unsigned long checks=0;

    for(kind=0;kind<4;kind++)for(offset=1;offset<=O(Type,np)+sizeof(int);offset++)
    for(shift=0;shift<8;shift++)for(route=0;route<2;route++){
        if(kind!=0 && offset!=1)continue;
        if(getenv("TRANSITION_ERROR_ROUTE") && route!=atoi(getenv("TRANSITION_ERROR_ROUTE")))continue;
        if(getenv("BUILTIN_BAD") && kind!=atoi(getenv("BUILTIN_BAD")))continue;
        memset(&R,0,sizeof(R));memset(&frame,0,sizeof(frame));memset(&type,0,sizeof(type));
        memset(&transresult,0,sizeof(transresult));memset(&transcurrent,0,sizeof(transcurrent));
        type.ref=1;type.size=sizeof(Frame)+32;
        if(kind==1)type.np=-1;
        if(kind==2)type.np=(-0x7fffffff-1);
        memmove(types+shift,&type,sizeof(type));frame.t=(Type*)(types+shift);
        if(kind==0)frame.t=(Type*)((uintptr)0-(uintptr)offset);
        argument=(Frame*)(frames+64+shift);memmove(argument,&frame,sizeof(frame));
        transresult.heap.ref=5;callee->nlinks=1;callee->m=&transmodule;callee->links[0].u.runt=builtin;
        R.M=&transcurrent.link;R.PC=(Inst*)(frames+800);R.FP=frames+512;R.SP=frames+600;
        R.s=cells[0]+shift;R.m=cells[1]+shift;R.d=cells[2]+shift;
        memmove(R.s,&argument,sizeof(argument));memmove(R.m,&index,sizeof(index));memmove(R.d,&callee,sizeof(callee));
        transitionbuiltinmode=kind==3 ? 2 : 0;callbacks=transitionfreecalls=0;raised=nil;failed=0;
        if(setjmp(boundary)==0){if(route==0)mcall();else jit->dispatch[IMCALL](&R);}else failed=1;
        transitioncheck(failed && raised==exHeap && callbacks==1 && transitionfreecalls==0 && R.M==callee && R.SP==frames+600);
        transitioncheck(transresult.heap.ref==(kind==3 ? 0 : 6));
        checks++;
    }
    transitionbuiltinmode=0;
    return checks;
}


/* Exercise real native complete entries while callbacks retain ordinary C ownership. */
int
main(void)
{
    Module compiled={0};
    Inst instructions[2]={{0}};
    unsigned long loads=0,errors=0,builtins=0,returns=0;
    char *family=getenv("TRANSITION_CASE");

    instructions[0].op=ILOAD;instructions[1].op=IMCALL;compiled.prog=instructions;compiled.nprog=2;
    optab[ILOAD]=iload;optab[IMCALL]=mcall;
    transitioncheck(compile(&compiled,2,nil) && compiled.jit && compiled.jit->entry[ILOAD]==nil);
    if(family==nil || !strcmp(family,"loads"))loads=transitionloads(compiled.jit);
    if(family==nil || !strcmp(family,"errors"))errors=transitionloaderrors(compiled.jit);
    if(family==nil || !strcmp(family,"builtins"))builtins=transitionbuiltins(compiled.jit);
    if(family==nil || !strcmp(family,"builtinerrors"))returns=transitionbuiltinerrors(compiled.jit);
    transitioncheck(loads || errors || builtins || returns);
    compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);
    printf("PASS module transitions WORD%zu: %lu LOAD C/RX routes, %lu rejections, %lu builtin C/RX routes, %lu callback-return errors\n",sizeof(WORD)*8,loads,errors,builtins,returns);
    return 0;
}
