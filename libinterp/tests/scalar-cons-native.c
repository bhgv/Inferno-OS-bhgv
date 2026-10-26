#include "comp-amd64.c"
#include "tests/leaf-plain-stubs.h"
#include "pool.h"
#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>

REG R;
Type Tstring={1}, Tarray={1}, Tlist={1};
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
char exHeap[]="heap", exNilref[]="nil";
static jmp_buf boundary;
static char *raised;
static int calls, live, failure;
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
    node=object; if(node->t!=nil)abort();
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

#define OP(name) void name(void) // Extract real interpreter entry points.
IMPLEMENTATION


/* Identify the complete scalar constructor case before rejecting an oracle mismatch. */
static void
failcase(int op, int profile, int sa, int da, int alias, int pass, int value, int line)
{
    fprintf(stderr,"scalar cons op=%d profile=%d sa=%d da=%d alias=%d pass=%d value=%d line=%d\n",
            op,profile,sa,da,alias,pass,value,line);
    abort();
}


/* Validate slot and tail arithmetic before any pool request or destination store. */
static void
badaddresses(Jit *jit)
{
    uvlong storage[32], before[32];
    Heap *h;
    List *tail;
    void *slot;
    int profile, pass, beforecalls;
    uvlong value=0x0123456789abcdefULL;
    List *saved;

    for(profile=0;profile<8;profile++)for(pass=0;pass<2;pass++){
        memset(storage,0,sizeof(storage));h=(Heap*)storage;h->ref=1;h->t=&Tlist;
        tail=H2D(List*,h);slot=&tail;Tlist.ref=2;
        switch(profile){
        case 0:slot=nil;break;
        case 1:slot=H;break;
        case 2:slot=(void*)((uintptr)-4);break;
        case 3:tail=nil;break;
        case 4:tail=(List*)8;break;
        case 5:tail=(List*)((uchar*)tail+1);break;
        case 6:h->ref=0;break;
        case 7:Tlist.ref=0;slot=nil;break;
        }
        beforecalls=calls;raised=nil;saved=tail;R.s=&value;R.d=slot;memmove(before,storage,sizeof(storage));
        if(setjmp(boundary)==0){if(pass)jit->entry[ICONSL](&R);else consl();abort();}
        if(raised!=(profile==3?exNilref:exHeap) || calls!=beforecalls || live || tail!=saved || memcmp(storage,before,sizeof(storage)))abort();
    }
    Tlist.ref=1;
    puts("PASS scalar cons address preflight: 16 slot/tail/error-order cases");
}


/* Compare all head bits, aliases, allocator failures, GC publication and native unwind. */
int
main(void)
{
    int ops[]={ICONSB,ICONSW,ICONSL,ICONSF};
    uvlong values[]={0,1,0xffffffffffffffffULL,0x8000000000000000ULL,0x7ff0000000000001ULL,
                     0x7ff8000012345678ULL,0x0123456789abcdefULL,0xffffffff00000001ULL};
    Module compiled={0};
    Inst instructions[4]={{0}};
    List *tail, *result, *old;
    Heap beforeheap;
    REG beforeR;
    uchar slots[128], before[128], expected[128], captured[8], tailbytes[32];
    int oi, profile, sa, da, alias, pass, vi, width, allocated, beforecalls, beforelive, flags, invalid, failed, i;
    unsigned long checks;

    for(i=0;i<4;i++)instructions[i].op=ops[i];compiled.prog=instructions;compiled.nprog=4;
    if(!compile(&compiled,4,nil) || compiled.jit==nil)abort();
    for(i=0;i<4;i++)if(compiled.jit->entry[ops[i]]==nil)abort();
    if(!getenv("CONS_NO_ERRORS"))badaddresses(compiled.jit);
    if(getenv("CONS_PREFLIGHT_ONLY")){
        compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);return 0;
    }
    checks=0; mutator=7;
    for(oi=0;oi<4;oi++)for(profile=0;profile<10;profile++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)
    for(alias=0;alias<3;alias++)for(pass=0;pass<2;pass++)for(vi=0;vi<8;vi++){
        if(getenv("CONS_NO_ERRORS") && profile>=4 && profile<=8)continue;
        width=oi==0?1:oi==1?sizeof(WORD):8;allocated=oi==0?4:width;
        Tlist.ref=1; failure=0; tail=H;cons(32,&tail); old=tail; D2H(tail)->ref=2;
        memset(tail->data,0x6b,32);memmove(tailbytes,tail->data,32);
        D2H(tail)->color=profile==1?mutator:profile==2?propagator:73;
        D2H(tail)->hprof=0x12345678;nprop=profile==3;
        invalid=0;failed=0;
        switch(profile){
        case 4:Tlist.ref=0;invalid=1;break;
        case 5:Tlist.ref=-1;invalid=1;break;
        case 6:Tlist.ref=0x7fffffff;invalid=1;break;
        case 7:D2H(tail)->ref=0;invalid=1;break;
        case 8:failed=1;break;
        case 9:D2H(tail)->ref=(ulong)-1;break;
        }
        memset(slots,0x5a,128);R.s=slots+sa;R.d=slots+64+da;R.m=slots+32;
        if(alias==1)R.s=R.d;
        if(alias==2)R.s=(uchar*)tail->data+sa;
        memmove(R.s,&values[vi],width);memmove(R.d,&tail,sizeof(tail));
        memmove(tailbytes,tail->data,32);memmove(captured,R.s,width);memmove(before,slots,128);memmove(expected,slots,128);beforeheap=*D2H(tail);beforeR=R;
        beforecalls=calls;beforelive=live;raised=nil;failure=failed;
        fesetround(FE_DOWNWARD);feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);flags=fetestexcept(FE_ALL_EXCEPT);
        if(setjmp(boundary)==0){
            if(pass){if(compiled.jit->entry[ops[oi]](&R)!=1)abort();}
            else if(oi==0)consb();else if(oi==1)consw();else if(oi==2)consl();else consf();
        }
        failure=0;
        if(invalid || failed){
            if(raised!=exHeap || calls!=beforecalls+failed || live!=beforelive || nprop!=(profile==3) || memcmp(slots,before,128) || memcmp(D2H(tail),&beforeheap,sizeof(Heap)))
                failcase(ops[oi],profile,sa,da,alias,pass,vi,__LINE__);
            result=tail;
        }else{
            if(raised || calls!=beforecalls+1 || live!=beforelive+1)failcase(ops[oi],profile,sa,da,alias,pass,vi,__LINE__);
            memmove(&result,R.d,sizeof(result));
            if(result==tail || result->tail!=tail || result->t!=nil || D2H(result)->t!=&Tlist || D2H(result)->ref!=1 || D2H(result)->color!=mutator ||
               memcmp(result->data,captured,width) || Tlist.ref!=3 || D2H(tail)->ref!=beforeheap.ref ||
               D2H(tail)->color!=(beforeheap.color==mutator?mutator:propagator) || nprop!=(profile==3 || beforeheap.color!=mutator))
                failcase(ops[oi],profile,sa,da,alias,pass,vi,__LINE__);
            for(i=width;i<allocated+sizeof(List)-sizeof(((List*)0)->data)-offsetof(List,data);i++)if(((uchar*)result->data)[i]!=0xa5)abort();
            memmove(expected+(64+da),&result,sizeof(result));
            if(memcmp(slots,expected,128))abort();
        }
        if(memcmp(tail->data,tailbytes,32) || memcmp(&R,&beforeR,sizeof(R)) || fegetround()!=FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT)!=flags)abort();
        /* Restore deliberately saturated/dead metadata before releasing the independent fixture root. */
        if(invalid){Tlist.ref=2;D2H(tail)->ref=2;}
        if(profile==9)D2H(tail)->ref=2;
        destroy(result);destroy(old);
        if(live || Tlist.ref!=1)abort();
        checks++;
    }
    compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);
    printf("PASS scalar cons C/RX: %lu bits/aliases/GC/failure/unwind/FP cases WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
