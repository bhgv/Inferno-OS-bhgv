#include "comp-amd64.c"
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
char exHeap[]="heap", exModule[]="module", exNilref[]="nil";
static jmp_buf boundary;
static char *raised;
static int calls, live, failure, customcalls, resources;


/* Keep process-based leak inspection out of the bounded, explicitly counted fixture. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Preserve exact VM error delivery without publishing an incomplete result. */
void
error(char *message)
{
    raised=message;
    longjmp(boundary, 1);
}


/* The plain pointer-free marker has no work; its exact identity is part of the contract. */
void
markheap(Type *type, void *object)
{
    (void)type; (void)object;
}


/* A distinct marker forces ordinary allocation even when the descriptor map is empty. */
static void
foreignmark(Type *type, void *object)
{
    (void)type; (void)object;
}


/* Count real VM allocations separately from code publication and custom native resources. */
void*
poolalloc(Pool *pool, ulong size)
{
    void *value;

    (void)pool; calls++;
    if(failure || size>4096) return nil;
    value=malloc(size);
    if(value==nil) abort();
    memset(value, 0xa5, size); live++;
    return value;
}


/* Free complete objects with the selected real/custom finalizer and retained descriptor. */
void
destroy(void *object)
{
    Heap *h;
    Type *type;

    if(object==H) return;
    if(object==nil) abort();
    h=D2H(object);
    if(h->ref==0) abort();
    if(--h->ref) return;
    type=h->t;
    if(type->free!=nil) type->free(h, 0);
    if(type->ref<=1) abort();
    type->ref--; free(h); live--;
}


/* Give a pointer-free custom type a native resource which zeroing in place would leak. */
static void
foreignfree(Heap *h, int swept)
{
    void *resource;

    (void)swept; customcalls++;
    if(h->t->size < (int)sizeof(resource)) return;
    memmove(&resource, H2D(void*, h), sizeof(resource));
    if(resource!=nil){free(resource); resources--;}
}


/* Allocate publication metadata outside the observed VM pool. */
void*
mallocz(ulong size, int clear)
{
    void *value=malloc(size);
    if(value!=nil && clear) memset(value, 0, size);
    return value;
}


/* Invoke normal RX publication without replacing production callback/type guards. */
int
kprotect(int (*fn)(void*), void *arg)
{
    return fn(arg);
}

#define OP(name) void name(void) // Preserve real interpreter instruction signatures.
IMPLEMENTATION


/* Report a complete independent record case before stopping a failed oracle. */
static void
zerofail(int size, int profile, int imported, int sa, int da, int alias, int failed, int line)
{
    fprintf(stderr,"NEWZ size=%d profile=%d imported=%d sa=%d da=%d alias=%d fail=%d line=%d\n",
            size,profile,imported,sa,da,alias,failed,line);
    abort();
}


/* Reject wrapped module/type spans without touching the inaccessible address. */
static void
lookupspans(void)
{
    Modlink link, *owner;
    Module module;
    Type type, *table[4];
    WORD index;
    int profile;
    char *expected;

    for(profile=0;profile<15;profile++){
        memset(&link,0,sizeof(link)); memset(&module,0,sizeof(module)); memset(&type,0,sizeof(type));
        link.m=&module; module.type=table; module.ntype=4; table[2]=&type;
        owner=&link; index=2; expected="invalid type";
        switch(profile){
        case 0: owner=nil; expected=exModule; break;
        case 1: owner=H; expected=exModule; break;
        case 2: owner=(Modlink*)((uintptr)-2); expected=exModule; break;
        case 3: link.m=nil; expected=exModule; break;
        case 4: link.m=H; expected=exModule; break;
        case 5: link.m=(Module*)((uintptr)-2); expected=exModule; break;
        case 6: module.type=nil; break;
        case 7: module.type=H; break;
        case 8: module.type=(Type**)((uintptr)-8); break;
        case 9: table[2]=nil; break;
        case 10: table[2]=H; break;
        case 11: table[2]=(Type*)((uintptr)-2); break;
        case 12: index=-1; break;
        case 13: module.ntype=-1; break;
        case 14: type.size=-1; break;
        }
        raised=nil;
        if(setjmp(boundary)==0){moduletype(owner,index);abort();}
        if(raised==nil || strcmp(raised,expected))abort();
    }
    puts("PASS module lookup spans: 15 nil/sentinel/wrap/index/type cases");
}


/* Check map geometry independently before either constructor can allocate or initialize. */
static void
constructormaps(void)
{
    Type type;
    Heap *object;
    int size, np, bit, zero, valid, beforecalls, i, mapped;
    unsigned long checks;
    uchar *payload;

    checks=0;
    for(size=0;size<=129;size++)
    for(np=-1;np<=4;np++)
    for(bit=-1;bit<32;bit++)
    for(zero=0;zero<2;zero++){
        memset(&type,0,sizeof(type));
        type.ref=1; type.size=size; type.np=np;
        type.free=freeheap; type.mark=markheap;
        if(bit>=0)type.map[bit/8]=0x80>>(bit%8);
        valid=np>=0 && np<=(size+8*sizeof(void*)-1)/(8*sizeof(void*));
        if(bit>=0 && bit<np*8 && (bit+1)*sizeof(void*)>size)valid=0;
        if(getenv("ZERO_NO_ERRORS") && !valid)continue;
        raised=nil; object=nil; beforecalls=calls;
        if(setjmp(boundary)==0)object=zero?heapz(&type):heap(&type);
        if(!valid){
            if(raised!=exHeap || calls!=beforecalls || live || type.ref!=1)abort();
        }else{
            if(raised || object==nil || calls!=beforecalls+1 || live!=1 || type.ref!=2)abort();
            payload=H2D(uchar*,object);
            for(i=0;i<size;i++){
                mapped=bit>=0 && bit<np*8 && i>=bit*sizeof(void*) && i<(bit+1)*sizeof(void*);
                if(payload[i]!=(mapped?255:zero?0:0xa5))abort();
            }
            destroy(payload);
            if(live || type.ref!=1)abort();
        }
        checks++;
    }
    printf("PASS heap/heapz map bounds: %lu allocation/payload/refcount cases\n",checks);
}


/* Compare actual owners, pointer maps, generic finalizers, aliases and allocation failure. */
int
main(void)
{
    int sizes[]={0,1,7,8,15,32,128,255,256,257};
    Module codeowner={0}, targetmodule={0}, wrongmodule={0};
    Modlink link={0}, wronglink={0};
    Inst code[3]={{0}};
    Type type, other, stale, *table[4], *cached[4];
    Heap *oldheap;
    void *old, *result, *resource, *nilptr=H;
    uchar slots[128], slotsbefore[128], expectedslots[128];
    uchar before[512], expected[512];
    Heap header;
    Type beforetype;
    REG saved;
    WORD index;
    int clear, si, profile, imported, sa, da, alias, failed, size, reuse, native, invalid, status;
    int beforecalls, beforelive, beforecustom, flags, oldextra, olddifferent, errorheap;
    unsigned long total;

    if(!getenv("ZERO_NO_ERRORS"))lookupspans();
    constructormaps();
    if(getenv("ZERO_CTOR_ONLY"))return 0;
    code[0].op=INEWZ; code[1].op=IMNEWZ; codeowner.prog=code; code[2].op=INEW; codeowner.nprog=3;
    if(!compile(&codeowner,3,nil) || codeowner.jit==nil ||
       codeowner.jit->entry[INEWZ]==nil || codeowner.jit->entry[IMNEWZ]==nil || codeowner.jit->entry[INEW]==nil) abort();
    link.m=&targetmodule; wronglink.m=&wrongmodule;
    targetmodule.type=table; targetmodule.ntype=4; link.type=cached;
    wrongmodule.type=cached; wrongmodule.ntype=4;
    total=0;
    for(si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++)
    for(profile=0;profile<16;profile++)
    for(clear=0;clear<2;clear++)
    for(imported=0;imported<2;imported++)
    for(sa=0;sa<8;sa++)
    for(da=0;da<8;da++)
    for(alias=0;alias<3;alias++)
    for(failed=0;failed<2;failed++){
        if(!clear && imported)continue;
        if(getenv("ZERO_NO_ERRORS") && (failed || profile==8 || profile==9 || profile==10 || profile==11 || profile==14 || profile==15)) continue;
        if((profile==4 || profile==3) && alias!=0) continue;
        if(alias==2 && profile!=0 && profile!=12 && profile!=13) continue;
        size=sizes[si]; memset(&type,0,sizeof(type)); memset(&other,0,sizeof(other)); memset(&stale,0,sizeof(stale));
        type.ref=1; type.size=size; type.free=freeheap; type.mark=markheap;
        other=stale=type; stale.size=0; stale.free=foreignfree;
        table[0]=table[1]=table[3]=&stale; table[2]=&type;
        cached[0]=cached[1]=cached[2]=cached[3]=&stale;
        failure=0; oldheap=poolalloc(heapmem,sizeof(Heap)+512);
        oldheap->t=&type; type.ref++; oldheap->ref=1; oldheap->color=73; oldheap->hprof=0x12345678;
        old=H2D(void*,oldheap); memset(old,0x6b,512);
        invalid=0; oldextra=0; olddifferent=0; errorheap=1;
        switch(profile){
        case 1: oldheap->ref=2; oldextra=1; break;
        case 2: oldheap->t=&other; type.ref--; other.ref++; olddifferent=1; break;
        case 3:
            type.free=foreignfree;
            if(size>=(int)sizeof(void*)){resource=malloc(8); if(resource==nil)abort(); resources++; memmove(old,&resource,sizeof(resource));}
            break;
        case 4:
            type.np=1; type.map[0]=0x80; memmove(old,&nilptr,sizeof(nilptr));
            if(size<(int)sizeof(void*))invalid=1;
            break;
        case 5: type.mark=foreignmark; break;
        case 6: oldheap->ref=(ulong)-1; oldextra=2; break;
        case 7: oldheap->ref=0; invalid=1; break;
        case 8: type.ref=0; invalid=1; break;
        case 9: type.ref=-1; invalid=1; break;
        case 10: type.size=-1; invalid=1; errorheap=0; break;
        case 11: type.size=0x7fffffff; invalid=1; break;
        case 12: type.ref=0x7fffffff; break;
        case 13: memset(old,0,512); break;
        case 14: type.np=-1; invalid=1; break;
        case 15: type.np=1; type.map[0]=1; invalid=1; break;
        }
        /* Bit 7 is invalid only if the final native pointer cell does not fit. */
        if(profile==15 && size>=8*(int)sizeof(void*)){invalid=0;memmove((uchar*)old+7*sizeof(void*),&nilptr,sizeof(nilptr));}
        memset(slots,0x5a,sizeof(slots)); index=2;
        R.M=imported ? &wronglink : &link;
        R.s=slots+sa; R.m=slots+32+sa; R.d=slots+80+da;
        if(imported)memmove(R.s,&(Modlink*){&link},sizeof(Modlink*));
        else memmove(R.s,&index,sizeof(index));
        memmove(R.m,&index,sizeof(index));
        if(alias==1){if(imported)R.m=old;else R.s=old;memmove(imported?R.m:R.s,&index,sizeof(index));}
        if(alias==2)R.d=(uchar*)old+8;
        memmove(R.d,&old,sizeof(old));
        saved=R; header=*oldheap; beforetype=type;
        memmove(before,old,512);memmove(expected,old,512);
        memmove(slotsbefore,slots,sizeof(slots));memmove(expectedslots,slots,sizeof(slots));
        reuse=!invalid && profile!=1 && profile!=2 && profile!=3 && profile!=4 && profile!=5 && profile!=6 && profile!=15;
        native=reuse && size<=256;
        if(native){if(clear)memset(expected,0,size);if(alias==2)memmove(expected+8,&old,sizeof(old));}
        beforecalls=calls;beforelive=live;beforecustom=customcalls;
        fesetround(FE_DOWNWARD);feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);flags=fetestexcept(FE_ALL_EXCEPT);
        status=codeowner.jit->entry[clear ? (imported ? IMNEWZ : INEWZ) : INEW](&R);
        if(status!=native || calls!=beforecalls || live!=beforelive || customcalls!=beforecustom ||
           memcmp(old,expected,512) || memcmp(oldheap,&header,sizeof(header)) ||
           memcmp(&type,&beforetype,sizeof(type)) || memcmp(slots,expectedslots,sizeof(slots)) || memcmp(&R,&saved,sizeof(R)))
            zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
        memmove(old,before,512);memmove(slots,slotsbefore,sizeof(slots));R=saved;raised=nil;failure=failed;
        if(setjmp(boundary)==0){if(!clear)new();else if(imported)mnewz();else newz();}
        failure=0;
        if(invalid || (failed && !reuse) || (profile==12 && !reuse)){
            if(raised==nil || (errorheap ? raised!=exHeap : strcmp(raised,"invalid type")) ||
               calls!=beforecalls+(failed&&!invalid&&!reuse&&profile!=12) || live!=beforelive || customcalls!=beforecustom ||
               memcmp(old,before,512) || memcmp(oldheap,&header,sizeof(header)) || memcmp(&type,&beforetype,sizeof(type)) ||
               memcmp(slots,slotsbefore,sizeof(slots)))
                zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
            result=old;
        }else{
            if(raised!=nil)zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
            memmove(&result,R.d,sizeof(result));
            if(reuse){
                if(clear)memset(expected,0,size);if(alias==2)memmove(expected+8,&old,sizeof(old));
                if(result!=old || calls!=beforecalls || live!=beforelive || memcmp(old,expected,512) ||
                   memcmp(oldheap,&header,sizeof(header)) || memcmp(&type,&beforetype,sizeof(type)))
                    zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
            }else{
                if(result==old || calls!=beforecalls+1 || D2H(result)->t!=&type || D2H(result)->ref!=1 || D2H(result)->color!=mutator)
                    zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
                for(status=0;status<size;status++){
                    int mapped=(profile==4 && status<(int)sizeof(void*)) || (profile==15 && status>=7*(int)sizeof(void*) && status<8*(int)sizeof(void*));
                    if(((uchar*)result)[status]!=(mapped?255:clear?0:0xa5))zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
                }
                if(profile==3 && customcalls!=beforecustom+1)zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
            }
        }
        if(memcmp(&R,&saved,sizeof(R)) || fegetround()!=FE_DOWNWARD || fetestexcept(FE_ALL_EXCEPT)!=flags)
            zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
        /* Restore only deliberate corruptions before releasing the independent owners. */
        if(result==old){
            type.size=size;type.np=0;
            if(profile==8 || profile==9 || profile==12)type.ref=olddifferent?1:2;
            if(profile==7)oldheap->ref=1;
        }
        if(oldextra==2){oldheap->ref=1;destroy(old);if(result==old)result=H;}
        /* Raw NEW leaves custom scalar resources unspecified; the application initializes them. */
        if(!clear && profile==3 && result!=old && size>=(int)sizeof(void*))
            memset(result,0,sizeof(void*));
        destroy(result);
        if(oldextra==1)destroy(old);
        if(live!=0 || resources!=0 || type.ref!=1 || other.ref!=1)
            zerofail(size,profile,imported,sa,da,alias,failed,__LINE__);
        total++;
    }
    codeowner.jit->freecode(codeowner.jit->code,codeowner.jit->size);free(codeowner.jit);
    printf("PASS NEW/NEWZ/MNEWZ C/RX: %lu callbacks/maps/owners/aliases/failure/FP cases WORD%zu\n",total,sizeof(WORD)*8);
    return 0;
}
