#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
#include "pool.h"
#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>

REG R;
Type Tstring={1}, Tarray={1};
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
char exHeap[]="heap", exModule[]="module", exNegsize[]="negative", exNilref[]="nil";
static jmp_buf boundary;
static char *raised;
static int calls, live, failure;
static void *blocks[8];
static ulong amounts[8];


/* Explicit counters and canaries cover every allocation without process-based leak discovery. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Preserve exact VM error delivery for rejected owners and forced allocation failures. */
void
error(char *message)
{
    raised=message;
    longjmp(boundary,1);
}


/* Allocate a bounded set of separately guarded VM objects. */
void*
poolalloc(Pool *pool, ulong size)
{
    int i;
    void *object;

    (void)pool; calls++;
    if(failure || size>4096)return nil;
    for(i=0;i<8;i++)if(blocks[i]==nil)break;
    if(i==8)abort();
    object=malloc(size+32);
    if(object==nil)abort();
    memset(object,0xa5,size+32); blocks[i]=object; amounts[i]=size; live++;
    return object;
}


/* Use actual array finalization while verifying storage and both retained descriptors. */
void
destroy(void *object)
{
    Heap *h;
    Type *type;
    int i, j;

    if(object==H)return;
    if(object==nil)abort();
    h=D2H(object);
    if(h->ref==0)abort();
    if(--h->ref)return;
    for(i=0;i<8;i++)if(blocks[i]==h)break;
    if(i==8)abort();
    for(j=0;j<32;j++)if(((uchar*)h)[amounts[i]+j]!=0xa5)abort();
    type=h->t;
    if(type->free)type->free(h,0);
    if(type->ref<=1)abort();
    type->ref--; free(h); blocks[i]=nil; live--;
}


/* Keep JIT publication metadata separate from observed VM object allocations. */
void*
mallocz(ulong size, int clear)
{
    void *object=malloc(size);
    if(object && clear)memset(object,0,size);
    return object;
}


/* Exercise the actual native publisher without replacing descriptor or owner guards. */
int
kprotect(int (*fn)(void*), void *arg)
{
    return fn(arg);
}

#define OP(name) void name(void) // Retain actual interpreter instruction signatures.
IMPLEMENTATION


/* Identify all constructor parameters before stopping an independent oracle failure. */
static void
failcase(int size, int count, int profile, int clear, int align, int alias, int failed, int line)
{
    fprintf(stderr,"array reuse size=%d count=%d profile=%d clear=%d align=%d alias=%d fail=%d line=%d\n",
            size,count,profile,clear,align,alias,failed,line);
    abort();
}


/* Reject invalid destinations before allocating or replacing any owner. */
static void
ownerpreflight(Jit *jit)
{
    uvlong storage[64], before[64];
    Heap *h;
    Array *object, *result, *saved;
    Type type, *table[1];
    Module module;
    Modlink link;
    WORD count, index;
    REG registers;
    int profile, clear, beforecalls, expectedcalls;
    char *expected;

    for(profile=0;profile<11;profile++)for(clear=0;clear<2;clear++){
        memset(storage,0x5a,sizeof(storage)); memset(&type,0,sizeof(type));
        memset(&module,0,sizeof(module)); memset(&link,0,sizeof(link));
        h=(Heap*)storage; object=H2D(Array*,h); h->t=&Tarray; h->ref=1;
        type.ref=2; type.size=8; Tarray.ref=2;
        object->t=&type; object->len=2; object->root=H; object->data=(uchar*)object+sizeof(Array);
        result=object; expected=exHeap; expectedcalls=0;
        switch(profile){
        case 0: result=nil; expected=exNilref; break;
        case 1: result=(Array*)8; break;
        case 2: h->ref=0; break;
        case 3: result=(Array*)((uchar*)object+1); break;
        case 4: object->data=nil; break;
        case 5: object->data=H; break;
        case 6: object->data=(uchar*)((uintptr)-8); break;
        case 7: result=H; expectedcalls=1; break;
        case 8: object->data=(uchar*)object; break;
        case 9: object->data=(uchar*)object+sizeof(Array)-1; break;
        case 10: result=(Array*)(uintptr)sizeof(Heap); break;
        }
        saved=result; table[0]=&type; module.type=table; module.ntype=1; link.m=&module;
        count=2; index=0; R.s=&count; R.m=&index; R.d=&result; R.M=&link; registers=R;
        memmove(before,storage,sizeof(storage)); beforecalls=calls;
        if(jit->entry[clear?INEWAZ:INEWA](&R)!=0 || result!=saved || calls!=beforecalls || memcmp(storage,before,sizeof(storage)))abort();
        raised=nil; failure=1;
        if(setjmp(boundary)==0){if(clear)newaz();else newa();}
        failure=0;
        if(raised!=expected || calls!=beforecalls+expectedcalls || live || result!=saved ||
           memcmp(storage,before,sizeof(storage)) || memcmp(&R,&registers,sizeof(R)) || type.ref!=2 || Tarray.ref!=2)abort();
    }
    Tarray.ref=1;
    puts("PASS array owner preflight: 22 nil/dead/alignment/data-span/failure cases");
}


/* Zero-width arrays with INT_MAX elements remain metadata-only even with inaccessible data. */
static void
largeempty(Jit *jit)
{
    uvlong storage[64], before[64];
    Heap *h;
    Array *object, *result;
    Type type, *table[1];
    Module module;
    Modlink link;
    WORD count, index;
    int profile, clear, beforecalls;

    for(profile=0;profile<3;profile++)for(clear=0;clear<2;clear++){
        memset(storage,0x5a,sizeof(storage)); memset(&type,0,sizeof(type));
        memset(&module,0,sizeof(module)); memset(&link,0,sizeof(link));
        type.ref=2; Tarray.ref=2;
        h=(Heap*)storage; h->t=&Tarray; h->ref=1;
        object=H2D(Array*,h); object->t=&type; object->len=0x7fffffff; object->root=H;
        object->data=profile==0?nil:profile==1?H:(uchar*)object;
        result=object; table[0]=&type; module.type=table; module.ntype=1; link.m=&module;
        count=0x7fffffff; index=0; R.s=&count; R.m=&index; R.d=&result; R.M=&link;
        beforecalls=calls; memmove(before,storage,sizeof(storage));
        if(jit->entry[clear?INEWAZ:INEWA](&R)!=1 || result!=object || calls!=beforecalls || memcmp(storage,before,sizeof(storage)))abort();
        raised=nil;
        if(setjmp(boundary)==0){if(clear)newaz();else newa();}
        if(raised || result!=object || calls!=beforecalls || live || memcmp(storage,before,sizeof(storage)) || type.ref!=2 || Tarray.ref!=2)abort();
    }
    Tarray.ref=1;
    puts("PASS empty array reuse: 6 INT_MAX-count/data-independent cases");
}


/* Compare real RX and C construction, owner preflight, aliases, clearing and allocation failure. */
int
main(void)
{
    int sizes[]={0,1,3,8,17,128,256,257};
    Module compiled={0}, module={0};
    Modlink link={0};
    Inst instructions[2]={{0}};
    Type type, stale, *table[4], *cache[4];
    Array *old, *result;
    Heap *h, savedheap;
    Array savedarray;
    REG savedreg;
    WORD count, index;
    uchar slots[128], savedslots[128], expectedslots[128];
    uchar original[2048], expected[2048], *base;
    int si, elements, profile, clear, align, alias, failed, size, bytes, status, reuse, native, invalid;
    int beforecalls, beforelive, flags, i, held;
    unsigned long checks;

    instructions[0].op=INEWA; instructions[1].op=INEWAZ; compiled.prog=instructions; compiled.nprog=2;
    if(!compile(&compiled,2,nil) || compiled.jit==nil || compiled.jit->entry[INEWA]==nil || compiled.jit->entry[INEWAZ]==nil)abort();
    if(!getenv("ARRAY_REUSE_NO_ERRORS"))ownerpreflight(compiled.jit);
    largeempty(compiled.jit);
    if(getenv("ARRAY_REUSE_PREFLIGHT_ONLY")){
        compiled.jit->freecode(compiled.jit->code,compiled.jit->size); free(compiled.jit); return 0;
    }
    module.type=table; module.ntype=4; link.m=&module; link.type=cache;
    checks=0;
    for(si=0;si<8;si++)for(elements=0;elements<4;elements++)for(profile=0;profile<16;profile++)
    for(clear=0;clear<2;clear++)for(align=0;align<8;align++)for(alias=0;alias<3;alias++)for(failed=0;failed<2;failed++){
        size=sizes[si]; bytes=size*elements;
        if(profile==14 && alias!=0)continue;
        if(getenv("ARRAY_REUSE_NO_ERRORS") && profile==14 && size<sizeof(void*))continue;
        if(alias==2 && profile!=0 && profile!=12)continue;
        if(getenv("ARRAY_REUSE_NO_ERRORS") && (failed || profile==4 || profile==5 || profile==6 || profile==7 || profile==8 || profile==9 || profile==10))continue;
        memset(&type,0,sizeof(type)); type.ref=1; type.size=size;
        stale=type; stale.size=0;
        table[0]=table[1]=table[3]=&stale; table[2]=&type;
        cache[0]=cache[1]=cache[2]=cache[3]=&stale;
        Tarray.ref=1; Tarray.free=freearray;
        failure=0; h=nheap(sizeof(Array)+2048); h->t=&Tarray; Tarray.ref++;
        old=H2D(Array*,h); old->len=elements; old->t=&type; type.ref++; old->root=H;
        base=(uchar*)old+sizeof(Array); old->data=base; memset(base,0x6b,2048);
        h->color=73; h->hprof=0x12345678; held=0; invalid=0;
        switch(profile){
        case 1: h->ref=2; held=1; break;
        case 2: old->len=elements+1; break;
        case 3: old->t=&stale; type.ref--; stale.ref++; break;
        case 4: h->ref=0; invalid=1; break;
        case 5: type.ref=0; invalid=1; break;
        case 6: type.ref=0x7fffffff; invalid=1; break;
        case 7: Tarray.ref=0x7fffffff; invalid=1; break;
        case 8: old->data=nil; invalid=bytes!=0; break;
        case 9: old->data=H; invalid=bytes!=0; break;
        case 10: type.np=-1; invalid=1; break;
        case 11: old->root=H; old->t=&stale; type.ref--; stale.ref++; break;
        case 12: old->data=base+16; break;
        case 13: h->ref=(ulong)-1; held=2; break;
        case 14: type.np=1; type.map[0]=0x80; if(size<sizeof(void*))invalid=1; break;
        case 15: old->root=H2D(Array*,heaparray(&type,elements)); old->data=old->root->data; break;
        }
        if(profile==14 && !invalid)for(i=0;i<elements;i++){void *nilptr=H;memmove(base+i*size,&nilptr,sizeof(nilptr));}
        memset(slots,0x5a,sizeof(slots)); count=elements; index=2;
        R.M=&link; R.s=slots+align; R.m=slots+32+align; R.d=slots+80+7-align;
        if(alias==1)R.s=base;
        if(alias==2)R.d=base+8;
        memmove(R.s,&count,sizeof(count));memmove(R.m,&index,sizeof(index));memmove(R.d,&old,sizeof(old));
        savedreg=R; savedheap=*h; savedarray=*old;
        memmove(original,base,2048);memmove(expected,base,2048);memmove(savedslots,slots,128);memmove(expectedslots,slots,128);
        reuse=!invalid && (profile==0 || profile==8 || profile==9 || profile==12);
        native=reuse && size<=256 && bytes<=256;
        if(native && clear && bytes)memset(expected+(profile==12?16:0),0,bytes);
        if(native && alias==2)memmove(expected+8,&old,sizeof(old));
        beforecalls=calls;beforelive=live;
        fesetround(FE_DOWNWARD);feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);flags=fetestexcept(FE_ALL_EXCEPT);
        status=compiled.jit->entry[clear?INEWAZ:INEWA](&R);
        if(status!=native || calls!=beforecalls || live!=beforelive || memcmp(base,expected,2048) ||
           memcmp(h,&savedheap,sizeof(*h)) || memcmp(old,&savedarray,sizeof(*old)) ||
           memcmp(&R,&savedreg,sizeof(R)) || memcmp(slots,expectedslots,128))
            failcase(size,elements,profile,clear,align,alias,failed,__LINE__);
        memmove(base,original,2048);memmove(slots,savedslots,128);R=savedreg;
        raised=nil;failure=failed;
        if(setjmp(boundary)==0){if(clear)newaz();else newa();}
        failure=0;
        if(invalid || (failed&&!reuse)){
            if(raised!=exHeap || calls!=beforecalls+(failed&&!invalid&&!reuse) || live!=beforelive ||
               memcmp(base,original,2048) || memcmp(h,&savedheap,sizeof(*h)) || memcmp(old,&savedarray,sizeof(*old)) || memcmp(slots,savedslots,128))
                failcase(size,elements,profile,clear,align,alias,failed,__LINE__);
            result=old;
        }else{
            if(raised)failcase(size,elements,profile,clear,align,alias,failed,__LINE__);
            memmove(&result,R.d,sizeof(result));
            if(reuse){
                memmove(expected,original,2048);
                if(clear && bytes)memset(expected+(profile==12?16:0),0,bytes);
                if(alias==2)memmove(expected+8,&old,sizeof(old));
                if(result!=old || calls!=beforecalls || live!=beforelive || memcmp(base,expected,2048) ||
                   memcmp(h,&savedheap,sizeof(*h)) || memcmp(old,&savedarray,sizeof(*old)))
                    failcase(size,elements,profile,clear,align,alias,failed,__LINE__);
            }else{
                if(result==old || calls!=beforecalls+1 || result->len!=elements || result->t!=&type || result->root!=H)
                    failcase(size,elements,profile,clear,align,alias,failed,__LINE__);
                for(i=0;i<bytes;i++){
                    int mapped=profile==14 && i%size<sizeof(void*);
                    if(result->data[i]!=(mapped?255:clear?0:0xa5))failcase(size,elements,profile,clear,align,alias,failed,__LINE__);
                }
            }
        }
        if(memcmp(&R,&savedreg,sizeof(R)) || fegetround()!=FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT)!=flags)abort();
        if(result==old){
            if(profile==4)h->ref=1;
            if(profile==5 || profile==6)type.ref=2;
            if(profile==7)Tarray.ref=2;
            type.np=0;
        }
        if(held==2){h->ref=1;destroy(old);if(result==old)result=H;}
        destroy(result);
        if(held==1)destroy(old);
        if(live || type.ref!=1 || stale.ref!=1 || Tarray.ref!=1)failcase(size,elements,profile,clear,align,alias,failed,__LINE__);
        checks++;
    }
    compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);
    printf("PASS array reuse C/RX: %lu owner/descriptor/zero/offset/alias/failure/FP cases WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
