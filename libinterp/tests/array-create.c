#include "dat.h"
#include "fns.h"
#include "interp.h"
#include "isa.h"
#include "pool.h"
#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>

REG R;
Type Tarray={1};
Pool *heapmem;
int mutator;
void (*heapmonitor)(int, void*, ulong);
char exHeap[]="heap", exModule[]="module", exNegsize[]="negative", exNilref[]="nil";
static jmp_buf boundary;
static char *raised;
static int calls, live, failure;
static void *last;
static ulong lastsize;


/* Disable process-based leak inspection; every observed object is counted explicitly. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Observe the exact constructor error before publication. */
void
error(char *message)
{
    raised=message;
    longjmp(boundary,1);
}


/* Keep trailing canaries outside the actual requested allocation. */
void*
poolalloc(Pool *pool, ulong size)
{
    uchar *value;

    (void)pool; calls++;
    if(failure || size>4096)return nil;
    value=malloc(size+32);
    if(value==nil)abort();
    memset(value,0xa5,size+32);
    last=value; lastsize=size; live++;
    return value;
}


/* Release mapped cells and the array's element descriptor using production cleanup. */
void
destroy(void *object)
{
    Heap *h;
    Type *type;
    int i;

    if(object==H)return;
    if(object==nil)abort();
    h=D2H(object);
    if(h->ref!=1)abort();
    type=h->t;
    if(h!=last)abort();
    for(i=0;i<32;i++)if(((uchar*)h)[lastsize+i]!=0xa5)abort();
    if(type->free!=nil)type->free(h,0);
    if(type->ref<=1)abort();
    type->ref--; free(h); live--; last=nil;
}

#define OP(name) void name(void) // Extract actual instruction entry points.
IMPLEMENTATION


/* Report the independent geometry and map oracle which failed. */
static void
failcase(int size, WORD count, int np, int bit, int clear, int alias, int failed, int line)
{
    fprintf(stderr,"array create size=%d count=%lld np=%d bit=%d clear=%d alias=%d fail=%d line=%d\n",
            size,(long long)count,np,bit,clear,alias,failed,line);
    abort();
}


/* Keep descriptor errors ahead of negative counts and reserve both retains for Tarray elements. */
static void
referencebounds(void)
{
    int refs[]={0,-1,1,0x7ffffffd,0x7ffffffe,0x7fffffff};
    Type type, *table[1], *element;
    Module module;
    Modlink link;
    WORD count, index;
    Array *result;
    int shared, ri, ai, clear, negative, valid, beforecalls, beforetype, beforearray;
    unsigned long checks;

    checks=0;
    for(shared=0;shared<2;shared++)
    for(ri=0;ri<6;ri++)
    for(ai=0;ai<6;ai++)
    for(clear=0;clear<2;clear++)
    for(negative=0;negative<2;negative++){
        memset(&type,0,sizeof(type)); memset(&Tarray,0,sizeof(Tarray));
        memset(&module,0,sizeof(module)); memset(&link,0,sizeof(link));
        type.ref=refs[ri]; Tarray.ref=refs[ai]; Tarray.free=freearray;
        element=shared?&Tarray:&type;
        table[0]=element; module.type=table; module.ntype=1; link.m=&module;
        count=negative?-1:0; index=0; result=H;
        R.M=&link; R.s=&count; R.m=&index; R.d=&result;
        beforetype=type.ref; beforearray=Tarray.ref; beforecalls=calls;
        valid=element->ref>0 && element->ref<=0x7ffffffe - shared && Tarray.ref>0 && Tarray.ref!=0x7fffffff;
        if(getenv("ARRAY_NO_ERRORS") && (!valid || negative))continue;
        raised=nil;
        if(setjmp(boundary)==0){if(clear)newaz();else newa();}
        if(!valid || negative){
            if(raised!=(valid?exNegsize:exHeap) || calls!=beforecalls || live || result!=H ||
               type.ref!=beforetype || Tarray.ref!=beforearray)abort();
        }else{
            if(raised || calls!=beforecalls+1 || live!=1 || result->t!=element || result->len!=0 ||
               Tarray.ref!=beforearray+1+shared || type.ref!=beforetype+!shared)abort();
            destroy(result);
            if(live || type.ref!=beforetype || Tarray.ref!=beforearray)abort();
        }
        checks++;
    }
    Tarray.ref=1;
    printf("PASS array descriptor reserves: %lu shared-Type/refcount/error-order cases\n",checks);
}


/* Exercise real array constructors with maps, scalar clearing, aliases and allocation failures. */
int
main(void)
{
    int sizes[]={0,1,7,8,9,15,16,31,32,63,64,65};
    WORD counts[]={-1,0,1,2,5,0x7fffffff,(WORD)((uvlong)1<<32),(WORD)(((uvlong)1<<32)+1)};
    Type type, stale, *table[4], *cached[4];
    Module module;
    Modlink link;
    WORD count, index;
    uchar slots[96], before[96];
    REG saved;
    Array *result;
    int si, ci, np, bit, clear, alias, failed, size, valid, geometry, allocatefail, beforecalls, i, element, mapped, status;
    unsigned long checks;

    referencebounds();
    checks=0;
    for(si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++)
    for(ci=0;ci<sizeof(counts)/sizeof(counts[0]);ci++)
    for(np=-1;np<=2;np++)
    for(bit=-1;bit<16;bit++)
    for(clear=0;clear<2;clear++)
    for(alias=0;alias<4;alias++)
    for(failed=0;failed<2;failed++){
        size=sizes[si]; count=counts[ci]; index=2;
        memset(&type,0,sizeof(type)); type.ref=1; type.size=size; type.np=np;
        if(bit>=0)type.map[bit/8]=0x80>>(bit%8);
        stale=type; stale.size=0; stale.np=0;
        table[0]=table[1]=table[3]=&stale; table[2]=&type;
        cached[0]=cached[1]=cached[2]=cached[3]=&stale;
        memset(&module,0,sizeof(module)); memset(&link,0,sizeof(link));
        module.type=table; module.ntype=4; link.m=&module; link.type=cached;
        memset(slots,0x5a,sizeof(slots));
        R.M=&link; R.s=slots+1; R.m=slots+25; R.d=slots+57;
        if(alias==1)R.s=R.m;
        if(alias==2)R.d=R.s;
        if(alias==3)R.d=R.m;
        memmove(R.s,&count,sizeof(count)); memmove(R.m,&index,sizeof(index));
        result=H; memmove(R.d,&result,sizeof(result));
        memmove(&count,R.s,sizeof(count)); memmove(&index,R.m,sizeof(index));
        geometry=count>=0 && (uvlong)count<=0x7fffffff &&
                 (size==0 || (uvlong)count<=(0x7fffffff-sizeof(Heap)-sizeof(Array))/size);
        valid=index==2 && geometry && np>=0 && np<=(size+8*sizeof(void*)-1)/(8*sizeof(void*));
        if(bit>=0 && bit<np*8 && (bit+1)*sizeof(void*)>size)valid=0;
        allocatefail=failed || (valid && size!=0 && (uvlong)count*size+sizeof(Heap)+sizeof(Array)>4096);
        if(getenv("ARRAY_NO_ERRORS") && (!valid || allocatefail))continue;
        Tarray.ref=1; Tarray.free=freearray;
        beforecalls=calls; raised=nil; failure=failed; saved=R; memmove(before,slots,sizeof(slots));
        if(setjmp(boundary)==0){if(clear)newaz();else newa();}
        failure=0;
        if(!valid || allocatefail){
            if(raised==nil || calls!=beforecalls+valid || live || type.ref!=1 || Tarray.ref!=1 ||
               memcmp(before,slots,sizeof(slots)))
                failcase(size,count,np,bit,clear,alias,failed,__LINE__);
            if(index!=2){if(strcmp(raised,"invalid type"))abort();}
            else if(count<0){if(raised!=exNegsize)abort();}
            else if(raised!=exHeap)abort();
        }else{
            if(raised || calls!=beforecalls+1 || live!=1 || type.ref!=2 || Tarray.ref!=2)
                failcase(size,count,np,bit,clear,alias,failed,__LINE__);
            memmove(&result,R.d,sizeof(result));
            if(result->len!=count || result->t!=&type || result->root!=H || result->data!=(uchar*)result+sizeof(Array) ||
               D2H(result)->t!=&Tarray || D2H(result)->ref!=1 || lastsize!=sizeof(Heap)+sizeof(Array)+size*count)
                failcase(size,count,np,bit,clear,alias,failed,__LINE__);
            for(element=0;size!=0 && element<count;element++)
            for(i=0;i<size;i++){
                mapped=bit>=0 && bit<np*8 && i>=bit*sizeof(void*) && i<(bit+1)*sizeof(void*);
                status=mapped?255:clear?0:0xa5;
                if(result->data[element*size+i]!=status)
                    failcase(size,count,np,bit,clear,alias,failed,__LINE__);
            }
            destroy(result);
            if(live || type.ref!=1 || Tarray.ref!=1)abort();
        }
        if(memcmp(&R,&saved,sizeof(R)))abort();
        checks++;
    }
    printf("PASS NEWA/NEWAZ: %lu map/stride/zero/alias/failure/canary cases WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
