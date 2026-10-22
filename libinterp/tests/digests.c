#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "libsec.h"
#include "HEADER"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>

/* The sandbox cannot support LeakSanitizer process inspection. */
int __lsan_is_turned_off(void) { return 1; }
typedef struct XDigestState XDigestState;
struct XDigestState { MOD_DigestState x; DigestState state; };
static Type state_type={1,0,0,sizeof(XDigestState)};
static Type *TDigestState=&state_type;
Type Tarray={1},Tbyte={1};
static char exBadKey[]="key",exBadDigest[]="digest";
char exNilref[]="nil",exType[]="type",exInval[]="invalid",exHeap[]="heap";
static jmp_buf boundary;
static char *raised;
static int live,allocation_failure=-1,native_live,native_calls,native_failure=-1,checks;
/* Track native hash allocations and permit deterministic native allocation failure. */
void *nativealloc(size_t n) {
    void *p;
    native_calls++;
    if(native_failure==0) return nil;
    if(native_failure>0)native_failure--;
    p=malloc(n);assert(p);native_live++;return p;
}
/* Balance only the allocations owned by native digest implementations. */
void nativefree(void *p) { if(p) {native_live--;free(p);} }
/* Use one bounded VM error boundary around each production wrapper. */
void error(char *text) {raised=text;longjmp(boundary,1);}
/* Allocate actual Heap headers and fail before exposing any new owner. */
Heap *heap(Type *t) {
    Heap *h;
    if(allocation_failure==0)error(exHeap);
    if(allocation_failure>0)allocation_failure--;
    h=calloc(1,sizeof(Heap)+t->size);assert(h);h->ref=1;h->t=t;t->ref++;live++;return h;
}
/* Retain actual VM reference-count semantics for alias and staged-result checks. */
void destroy(void *p) {
    Heap *h;
    if(p==H)return;
    assert(p);h=D2H(p);assert(h->ref>0);
    if(--h->ref)return;
    if(h->t==&Tarray) {assert(((Array*)p)->t==&Tbyte);Tbyte.ref--;}
    h->t->ref--;free(h);live--;
}
/* Validate states and acquire the returned owner only after successful validation. */
void *checktype(void *p,Type *t,char *name,int ref) {
    (void)name;if(p==H || p==nil)error(exNilref);
    if(D2H(p)->t!=t)error(exType);
    if(ref)D2H(p)->ref++;
    return p;
}
IMPLEMENTATION
static Array *input,*key,*output;
static MOD_DigestState *state,*result;
static WORD count;
static int operation;
/* Dispatch using each algorithm's real generated builtin frame. */
static void invoke(void) {
    switch(operation) { DISPATCH }
}
/* Catch errors without replacing production validation or publication logic. */
static int call(void) {raised=nil;if(setjmp(boundary)==0) {invoke();return 1;}return 0;}
/* Construct byte arrays with actual metadata and guard their complete output span. */
static Array *bytes(const uchar *data,int n) {
    Heap *h=calloc(1,sizeof(Heap)+sizeof(Array)+n+16);
    Array *a;assert(h);h->ref=1;h->t=&Tarray;Tarray.ref++;Tbyte.ref++;live++;
    a=H2D(Array*,h);a->t=&Tbyte;a->root=H;a->len=n;a->data=(uchar*)(a+1);
    if(data)memmove(a->data,data,n);
    memset(a->data+n,0xa5,16);return a;
}
/* Native digest output must leave every byte beyond its expected length intact. */
static void guard(Array *a) {int i;for(i=0;i<16;i++)assert(a->data[a->len+i]==0xa5);}
/* Allocate a standalone old state whose identity must survive rejected calls. */
static MOD_DigestState *oldstate(void) {return H2D(MOD_DigestState*,heap(TDigestState));}
static int lengths[]={20,28,32,48,64,16,16,20,16};
VECTORS
/* Check known digests plus every split point around the hash block boundaries. */
static void vectors(void) {
    int op,vector,split,n,dlen,before;
    uchar data[129],saved[64];
    for(n=0;n<129;n++)data[n]=(uchar)(n*17+3);
    for(op=0;op<9;op++) for(vector=0;vector<NVECTORS;vector++) {
        if(op==6 && vector>1)continue;
        operation=op;dlen=lengths[op];n=vector_sizes[vector];
        for(split=0;split<=n;split++) {
            key=vector && vector%3==0 ? bytes(nil,0) : vector%3==1 ? bytes(data,64) : bytes((uchar*)"key",3);
            output=H;state=H;result=oldstate();
            input=bytes(vector==0?(uchar*)"abc":data,split);count=split;
            assert(call());destroy(input);
            state=result;input=bytes((vector==0?(uchar*)"abc":data)+split,n-split);count=n-split;
            output=bytes(nil,dlen);native_calls=0;native_failure=0;
            assert(call());assert(native_calls==0);
            native_failure=-1;
            assert(!memcmp(output->data,expected[op][vector],dlen));guard(output);
            memmove(saved,output->data,dlen);
            assert(D2H(result)->ref==1);
            if(op==7 || op==8)assert(native_live==0);
            destroy(input);destroy(key);destroy(output);destroy(result);
            before=live;assert(before==0 && native_live==0);checks++;
        }
    }
}
/* Exercise the public nil/empty span contract against independent empty vectors. */
static void empties(void) {
    int op;
    uchar data[64];
    for(op=0;op<64;op++)data[op]=(uchar)(op*17+3);
    for(op=0;op<9;op++) {
        operation=op;input=H;count=0;key=bytes(data,64);
        state=H;result=oldstate();output=bytes(nil,lengths[op]);
        assert(call() && !memcmp(output->data,expected[op][1],lengths[op]));
        guard(output);destroy(key);destroy(result);destroy(output);
        assert(live==0 && native_live==0);checks++;
    }
}

/* Invalid full-width lengths/types must precede allocation, mutation and publication. */
static void invalid(void) {
    int op,mode,before;WORD savedlen;Type *savedtype;uchar *saveddata;
    MOD_DigestState *old;DigestState saved;
    for(op=0;op<9;op++) for(mode=0;mode<13;mode++) {
        operation=op;input=bytes((uchar*)"abc",3);key=bytes((uchar*)"key",3);
        output=bytes(nil,lengths[op]);result=oldstate();state=result;count=3;
        old=result;saved=((XDigestState*)result)->state;
        savedlen=input->len;savedtype=input->t;saveddata=input->data;
        if(mode==0)count=-1;
        if(mode==1)count=(WORD)((UWORD)1<<(sizeof(WORD)*8-1));
        if(mode==2)input->len=-1;
        if(mode==3)input->len=sizeof(WORD)>4?(WORD)((uvlong)1<<32):-1;
        if(mode==4)input->t=TDigestState;
        if(mode==5)input->data=nil;
        if(mode==6)output->len=lengths[op]-1;
        if(mode==7)output->t=TDigestState;
        if(mode==8)state=nil;
        if(mode==9) {state=H;allocation_failure=0;}
        if(mode==10)count=sizeof(WORD)>4?(WORD)((uvlong)1<<32):0x7fffffff;
        if(mode==11) {destroy(input);input=H;count=1;}
        if(mode==12)state=(MOD_DigestState*)key;
        before=live;native_calls=0;
        if(mode==10) {assert(call());assert(result==old);}
        else {
            assert(!call());assert(result==old && live==before && native_calls==0);
            assert(!memcmp(&saved,&((XDigestState*)result)->state,sizeof(saved)));
        }
        allocation_failure=-1;
        if(input!=H) {input->len=savedlen;input->t=savedtype;input->data=saveddata;}
        output->len=lengths[op];output->t=&Tbyte;
        guard(output);destroy(input);destroy(key);destroy(output);destroy(result);
        assert(live==0 && native_live==0);checks++;
    }
    /* HMAC rejects missing, invalid and oversized keys before acquiring state. */
    for(op=7;op<9;op++) for(mode=0;mode<4;mode++) {
        operation=op;input=bytes((uchar*)"abc",3);output=H;
        key=mode==0?H:bytes(nil,mode==1?65:1);
        if(mode==2)key->len=-1;
        if(mode==3)key->data=nil;
        result=oldstate();old=result;state=result;count=3;
        assert(!call() && result==old && D2H(result)->ref==1);
        if(mode==2)key->len=1;
        if(mode==3)key->data=(uchar*)(key+1);
        destroy(input);destroy(key);destroy(result);assert(live==0);checks++;
    }
}
/* Copy safely when result owns the source, preserving old state on heap failure. */
static void copies(void) {
    MOD_DigestState *old;DigestState saved;int mode;
    operation=9;
    for(mode=0;mode<5;mode++) {
        input=H;result=oldstate();state=result;old=result;
        memset(&((XDigestState*)result)->state,0x35,sizeof(DigestState));
        saved=((XDigestState*)result)->state;
        if(mode==1)allocation_failure=0;
        if(mode==2)state=nil;
        if(mode==3)state=H;
        if(mode==4) {input=bytes(nil,0);state=(MOD_DigestState*)input;}
        if(mode==1 || mode==2 || mode==4) {assert(!call() && result==old);}
        else if(mode==3) {assert(call() && result==H);}
        else {assert(call() && result!=old && !memcmp(&((XDigestState*)result)->state,&saved,sizeof(saved)));}
        allocation_failure=-1;destroy(input);destroy(result);assert(live==0);checks++;
    }
}
/* Native one-shot HMAC needs only its inner allocation, never an outer one. */
static void nativehmac(void) {
    uchar digest[20];int op;
    for(op=7;op<9;op++) {
        native_calls=0;native_failure=1;
        if(op==7)hmac_sha1((uchar*)"abc",3,(uchar*)"key",3,digest,nil);
        else hmac_md5((uchar*)"abc",3,(uchar*)"key",3,digest,nil);
        assert(native_calls==1 && native_live==0);
        assert(!memcmp(digest,expected[op][0],lengths[op]));
        native_failure=-1;checks++;
    }
}

/* Require complete VM/native ownership balance after all production calls. */
int main(void) {
    vectors();empties();invalid();copies();nativehmac();
    assert(live==0 && native_live==0 && state_type.ref==1 && Tarray.ref==1 && Tbyte.ref==1);
    printf("PASS MOD digests: %d vectors/bounds/alias/ownership cases, WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
