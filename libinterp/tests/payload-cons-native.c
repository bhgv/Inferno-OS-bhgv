#include "comp-amd64.c"
#include "tests/leaf-plain-stubs.h"
#include "pool.h"
#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>

REG R;
Type Tstring={1}, Tarray={1}, Tlist={1}, Tptr={1};
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
char exHeap[]="heap", exNilref[]="nil";
static jmp_buf boundary;
static char *raised;
static int calls, live, failure, changegc;
static void *blocks[8];
static ulong amounts[8];


/* Count every owner and guard instead of relying on process-based leak discovery. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Unwind real native allocator calls through the same outer VM error boundary as C. */
void
error(char *message)
{
    raised=message;
    longjmp(boundary,1);
}


/* Allocate separately guarded list objects and expose forced failure before publication. */
void*
poolalloc(Pool *pool, ulong size)
{
    void *object;
    int i;

    (void)pool; calls++;
    if((uintptr)__builtin_frame_address(0)%16)abort();
    if(failure || size>4096)return nil;
    for(i=0;i<8;i++)if(blocks[i]==nil)break;
    if(i==8)abort();
    object=malloc(size+32); if(object==nil)abort();
    memset(object,0xa5,size+32); blocks[i]=object; amounts[i]=size; live++;
    if(changegc){mutator=11;Tptr.ref=5;}
    return object;
}


/* Verify exact node bounds and transfer tail ownership without incrementing its reference. */
void
destroy(void *object)
{
    Heap *h;
    List *node;
    int i, j;

    if(object==H)return;
    if(object==nil)abort();
    h=D2H(object); if(h->ref==0)abort();
    if(--h->ref)return;
    for(i=0;i<8;i++)if(blocks[i]==h)break;
    if(i==8 || h->t!=&Tlist)abort();
    for(j=0;j<32;j++)if(((uchar*)h)[amounts[i]+j]!=0xa5)abort();
    node=object;
    if(node->t!=nil){
        void *child;
        if(node->t!=&Tptr || Tptr.ref<=1)abort();
        memmove(&child,node->data,sizeof(child));destroy(child);Tptr.ref--;
    }
    destroy(node->tail);
    if(Tlist.ref<=1)abort();
    Tlist.ref--; free(h); blocks[i]=nil; live--;
}


/* Keep publication metadata outside the measured VM pool. */
void*
mallocz(ulong size, int clear)
{
    void *object=malloc(size); if(object && clear)memset(object,0,size);
    return object;
}


/* Publish real executable leaves without substituting allocation or error behavior. */
int
kprotect(int (*fn)(void*), void *arg)
{
    return fn(arg);
}

#define OP(name) void name(void) // Extract actual interpreter constructor signatures.
IMPLEMENTATION


/* Construct independent roots outside each observed operation. */
static List*
rootnode(int bytes)
{
    List *node=H;
    cons(bytes,&node);
    memset(node->data,0x6b,bytes);
    return node;
}


/* Report every alias, profile and size which contributes to an oracle mismatch. */
static void
failcase(int pointer, int size, int profile, int sa, int da, int alias, int pass, int line)
{
    fprintf(stderr,"payload cons ptr=%d size=%d profile=%d sa=%d da=%d alias=%d pass=%d line=%d\n",
            pointer,size,profile,sa,da,alias,pass,line);
    abort();
}


/* Reject invalid raw ranges before allocation; zero-byte inputs remain unread. */
static void
spanpreflight(Jit *jit)
{
    uchar bytes[16];
    List *tail;
    WORD amount;
    int profile, pass, beforecalls, status;

    for(profile=0;profile<8;profile++)for(pass=0;pass<2;pass++){
        tail=H; amount=8; Tlist.ref=1; Tptr.ref=1; failure=1;
        memset(bytes,0x5a,sizeof(bytes));memset(&R,0,sizeof(R));R.s=bytes;R.m=&amount;R.d=&tail;
        switch(profile){
        case 0:R.s=nil;break;
        case 1:R.s=H;break;
        case 2:R.s=(void*)((uintptr)-8);break;
        case 3:R.d=nil;break;
        case 4:R.d=H;break;
        case 5:R.d=(void*)((uintptr)-4);break;
        case 6:amount=-1;R.d=nil;break;
        case 7:amount=0;R.s=H;break;
        }
        raised=nil;beforecalls=calls;
        if(setjmp(boundary)==0){
            if(pass){status=jit->entry[ICONSM](&R);if(status==0)consm();else abort();}
            else consm();
        }
        failure=0;
        if(raised!=exHeap || calls!=beforecalls+(profile==7) || tail!=H || live || Tlist.ref!=1 || Tptr.ref!=1)abort();
    }
    puts("PASS CONSM preflight: 16 source/destination/error-order cases");
}


/* Check full-WORD raw spans and copy-before-publication against real native calls. */
static void
rawcases(Jit *jit)
{
    int sizes[]={0,1,4,7,8,16,31,128,256,257,511,512};
    List *tail, *source, *result;
    Heap th, sh;
    REG registers;
    WORD amount;
    uchar slots[2048], before[2048], expected[2048], captured[512], payload[1024];
    int si, profile, sa, da, alias, pass, n, valid, failed, beforecalls, flags, i, status, expectedcalls, beforelist;
    unsigned long checks=0;

    for(si=0;si<12;si++)for(profile=0;profile<15;profile++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)
    for(alias=0;alias<3;alias++)for(pass=0;pass<2;pass++){
        n=sizes[si];
        if(profile==5 && n>0 && n<8)continue;
        if(getenv("PAYLOAD_NO_ERRORS") && profile!=0 && profile!=14)continue;
        Tlist.ref=1;Tptr.ref=1;failure=changegc=0;mutator=7;nprop=0;
        tail=rootnode(64);source=rootnode(1024);D2H(tail)->ref=2;
        D2H(tail)->color=73;D2H(tail)->hprof=0x12345678;
        D2H(source)->color=7;memset(slots,0x5a,sizeof(slots));
        for(i=0;i<1024;i++)((uchar*)source->data)[i]=(uchar)(i*17+sa+da);
        amount=n;
        if(profile==1)amount=-1;
        if(profile==2)amount=0x7fffffff;
        if(profile==13)amount=(WORD)((uvlong)1<<32);
        if(profile==14)amount=0;
        memset(&R,0,sizeof(R));R.s=(uchar*)source->data+sa;R.d=slots+512+da;R.m=slots+256+sa;
        if(alias==1)R.s=R.d;
        if(alias==2)R.m=R.s;
        memmove(R.m,&amount,sizeof(amount));memmove(R.d,&tail,sizeof(tail));
        if(profile==3)R.s=nil;
        if(profile==4 || profile==14)R.s=H;
        if(profile==5)R.s=(void*)((uintptr)-8);
        if(profile==6)R.d=nil;
        if(profile==7)R.d=H;
        if(profile==8)R.d=(void*)((uintptr)-4);
        failed=profile==9;
        if(profile==10)D2H(tail)->ref=0;
        if(profile==11)Tlist.ref=0;
        if(profile==12)Tlist.ref=0x7fffffff;
        memmove(&amount,R.m,sizeof(amount));
        valid=amount>=0 && (uvlong)amount<=0x7fffffff-sizeof(Heap)-(sizeof(List)-sizeof(((List*)0)->data));
        valid=valid && R.d!=nil && R.d!=H && (uintptr)R.d<=(uintptr)-1-sizeof(tail);
        valid=valid && (amount==0 || (R.s!=nil && R.s!=H && (uintptr)R.s<=(uintptr)-1-(uintptr)amount));
        valid=valid && profile!=10 && profile!=11 && profile!=12;
        if(valid && amount>0)memmove(captured,R.s,(int)amount);
        memmove(before,slots,sizeof(slots));memmove(expected,slots,sizeof(slots));memmove(payload,source->data,1024);
        th=*D2H(tail);sh=*D2H(source);registers=R;beforelist=Tlist.ref;beforecalls=calls;raised=nil;failure=failed;
        fesetround(FE_DOWNWARD);feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);flags=fetestexcept(FE_ALL_EXCEPT);
        if(setjmp(boundary)==0){
            if(pass){
                status=jit->entry[ICONSM](&R);
                if(status==0){
                    if(calls!=beforecalls || memcmp(slots,before,sizeof(slots)) || memcmp(D2H(tail),&th,sizeof(th)) || memcmp(D2H(source),&sh,sizeof(sh)))abort();
                    consm();
                }else if(status!=1 || amount>256)abort();
            }else consm();
        }
        failure=0;
        if(!valid || failed){
            expectedcalls=failed&&valid;
            if(raised!=exHeap || calls!=beforecalls+expectedcalls || live!=2 || Tlist.ref!=beforelist || memcmp(slots,before,sizeof(slots)) ||
               memcmp(D2H(tail),&th,sizeof(th)) || memcmp(D2H(source),&sh,sizeof(sh)) || nprop)
                failcase(0,n,profile,sa,da,alias,pass,__LINE__);
            result=tail;
        }else{
            if(raised || calls!=beforecalls+1 || live!=3)failcase(0,n,profile,sa,da,alias,pass,__LINE__);
            memmove(&result,R.d,sizeof(result));memmove(expected+512+da,&result,sizeof(result));
            if(result==tail || result->tail!=tail || result->t!=nil || memcmp(result->data,captured,(int)amount) || Tlist.ref!=4 ||
               D2H(tail)->ref!=2 || D2H(tail)->color!=propagator || !nprop || memcmp(slots,expected,sizeof(slots)) || D2H(result)->ref!=1 || D2H(result)->t!=&Tlist || D2H(result)->color!=mutator)
                failcase(0,n,profile,sa,da,alias,pass,__LINE__);
        }
        if(valid && !failed)for(i=(int)amount;i<(int)amount+sizeof(List)-sizeof(((List*)0)->data)-offsetof(List,data);i++)
            if(((uchar*)result->data)[i]!=0xa5)abort();
        if(memcmp(&R,&registers,sizeof(R)) || memcmp(source->data,payload,1024) || fegetround()!=FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT)!=flags)abort();
        if(!valid){Tlist.ref=3;D2H(tail)->ref=2;}
        destroy(result);destroy(tail);destroy(source);
        if(live || Tlist.ref!=1 || Tptr.ref!=1)abort();
        checks++;
    }
    printf("PASS CONSM C/RX: %lu copy/WORD/alias/span/failure/FP cases WORD%zu\n",checks,sizeof(WORD)*8);
}


/* Retain sources after allocation while loading live collector and Type cells. */
static void
pointercases(Jit *jit)
{
    List *tail, *source, *value, *actual, *result;
    Heap th, sh, expectedtail, expectedsource;
    REG registers;
    uchar slots[256], before[256], expected[256];
    int profile, sa, da, alias, pass, valid, sourcevalid, typevalid, failed, beforecalls, typerefs, postrefs, flags, status, beforelist;
    unsigned long checks=0;

    Tptr.size=sizeof(void*);Tptr.np=1;Tptr.map[0]=0x80;
    for(profile=0;profile<19;profile++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(alias=0;alias<2;alias++)for(pass=0;pass<2;pass++){
        if(getenv("PAYLOAD_NO_ERRORS") && profile!=0 && profile!=1 && profile!=11 && profile!=15 && profile!=16 && profile!=17)continue;
        Tlist.ref=1;Tptr.ref=1;failure=changegc=0;mutator=7;nprop=0;
        tail=rootnode(64);source=rootnode(64);D2H(tail)->ref=2;
        D2H(tail)->color=D2H(source)->color=7;D2H(tail)->hprof=D2H(source)->hprof=0x12345678;
        value=source;failed=0;
        switch(profile){
        case 1:value=H;break;
        case 2:value=nil;break;
        case 3:value=(List*)8;break;
        case 4:value=(List*)(uintptr)sizeof(Heap);break;
        case 5:value=(List*)((uchar*)source+1);break;
        case 6:D2H(source)->ref=0;break;
        case 7:D2H(source)->ref=(ulong)-1;break;
        case 8:Tptr.ref=0;break;
        case 9:Tptr.ref=-1;break;
        case 10:Tptr.ref=0x7fffffff;break;
        case 11:Tptr.ref=0x7ffffffe;break;
        case 12:failed=1;break;
        case 13:D2H(tail)->ref=0;break;
        case 14:Tlist.ref=0;break;
        case 15:D2H(tail)->color=D2H(source)->color=73;break;
        case 16:changegc=1;break;
        case 17:nprop=1;break;
        case 18:Tlist.ref=0x7fffffff;break;
        }
        memset(slots,0x5a,sizeof(slots));memset(&R,0,sizeof(R));R.s=slots+sa;R.d=slots+128+da;
        if(alias)R.s=R.d;
        memmove(R.s,&value,sizeof(value));memmove(R.d,&tail,sizeof(tail));memmove(&actual,R.s,sizeof(actual));
        sourcevalid=actual==H || (actual==tail && D2H(tail)->ref!=0 && D2H(tail)->ref!=(ulong)-1) ||
                    (actual==source && D2H(source)->ref!=0 && D2H(source)->ref!=(ulong)-1);
        typevalid=Tptr.ref>0 && Tptr.ref!=0x7fffffff;
        valid=sourcevalid && typevalid && profile!=13 && profile!=14 && profile!=18;
        th=*D2H(tail);sh=*D2H(source);registers=R;beforelist=Tlist.ref;typerefs=Tptr.ref;beforecalls=calls;
        memmove(before,slots,sizeof(slots));memmove(expected,slots,sizeof(slots));raised=nil;failure=failed;
        fesetround(FE_DOWNWARD);feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);flags=fetestexcept(FE_ALL_EXCEPT);
        if(setjmp(boundary)==0){
            if(pass){
                status=jit->entry[ICONSP](&R);
                if(status==0){
                    if(sourcevalid && typevalid)abort();
                    if(calls!=beforecalls || Tptr.ref!=typerefs || memcmp(slots,before,sizeof(slots)) ||
                       memcmp(D2H(tail),&th,sizeof(th)) || memcmp(D2H(source),&sh,sizeof(sh)))abort();
                    consp();
                }else if(status!=1 || !sourcevalid || !typevalid)abort();
            }else consp();
        }
        failure=changegc=0;
        if(!valid || failed){
            if(raised!=(actual==nil?exNilref:exHeap) || calls!=beforecalls+(failed&&valid) || live!=2 || Tlist.ref!=beforelist || Tptr.ref!=typerefs || nprop ||
               memcmp(slots,before,sizeof(slots)) || memcmp(D2H(tail),&th,sizeof(th)) || memcmp(D2H(source),&sh,sizeof(sh)))
                failcase(1,8,profile,sa,da,alias,pass,__LINE__);
            result=tail;
        }else{
            postrefs=profile==16?5:typerefs;
            if(raised || calls!=beforecalls+1 || live!=3 || Tptr.ref!=postrefs+1 || Tlist.ref!=4)
                failcase(1,8,profile,sa,da,alias,pass,__LINE__);
            memmove(&result,R.d,sizeof(result));memmove(expected+128+da,&result,sizeof(result));
            expectedtail=th;expectedsource=sh;
            expectedtail.color=th.color==mutator?mutator:propagator;
            if(actual==tail)expectedtail.ref++;
            if(actual==source){expectedsource.ref++;expectedsource.color=sh.color==mutator?mutator:propagator;}
            if(result==tail || result->tail!=tail || result->t!=&Tptr || memcmp(result->data,&actual,sizeof(actual)) ||
               memcmp(D2H(tail),&expectedtail,sizeof(th)) || memcmp(D2H(source),&expectedsource,sizeof(sh)) ||
               nprop!=(profile==17 || th.color!=mutator || (actual==source && sh.color!=mutator)) || memcmp(slots,expected,sizeof(slots)) ||
               D2H(result)->ref!=1 || D2H(result)->t!=&Tlist || D2H(result)->color!=mutator)
                failcase(1,8,profile,sa,da,alias,pass,__LINE__);
        }
        if(memcmp(&R,&registers,sizeof(R)) || fegetround()!=FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT)!=flags)abort();
        if(!valid || failed){Tptr.ref=1;Tlist.ref=3;D2H(tail)->ref=2;D2H(source)->ref=1;}
        else {if(profile==6 || profile==7)D2H(source)->ref=1;}
        destroy(result);
        if(valid && !failed){if(Tptr.ref!=(profile==16?5:typerefs))abort();Tptr.ref=1;}
        destroy(tail);destroy(source);
        if(live || Tlist.ref!=1 || Tptr.ref!=1)abort();
        checks++;
    }
    printf("PASS CONSP C/RX: %lu retain/GC/live-cells/alias/error/FP cases WORD%zu\n",checks,sizeof(WORD)*8);
}


/* Compile both allocating payload paths and exercise their actual RX entry points. */
int
main(void)
{
    Module compiled={0};
    Inst instructions[2]={{0}};

    instructions[0].op=ICONSM;instructions[1].op=ICONSP;compiled.prog=instructions;compiled.nprog=2;
    if(!compile(&compiled,2,nil) || compiled.jit==nil || compiled.jit->entry[ICONSM]==nil || compiled.jit->entry[ICONSP]==nil)abort();
    if(!getenv("PAYLOAD_NO_ERRORS"))spanpreflight(compiled.jit);
    if(!getenv("PAYLOAD_PREFLIGHT_ONLY")){rawcases(compiled.jit);pointercases(compiled.jit);}
    compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);
    return 0;
}
