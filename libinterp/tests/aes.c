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
typedef struct XAESstate XAESstate;
struct XAESstate { MOD_AESstate x; AESstate state; };
static Type state_type={1,0,0,sizeof(XAESstate)};
static Type *TAESstate=&state_type;
Type Tarray={1},Tbyte={1};
static char exBadKey[]="key",exBadIvec[]="iv";
char exBounds[]="bounds";
static char exBadState[]="state";
char exNilref[]="nil",exType[]="type",exInval[]="invalid",exHeap[]="heap";
static jmp_buf boundary;
static char *raised;
static int live,allocation_failure=-1,checks;
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
/* Native fatal errors are caught only for invalid-input preservation tests. */
void sysfatal(char *format,...) { (void)format;error(exInval); }
IMPLEMENTATION
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
/* Allocate an old stream owner whose identity must survive rejected calls. */
static MOD_AESstate *oldstate(void) {return H2D(MOD_AESstate*,heap(TAESstate));}
VECTORS
static Array *key,*iv,*input;
static MOD_AESstate *state,*result;
static WORD count;
static int operation,direction;
/* Invoke setup and CBC using their actual generated builtin frames. */
static void invoke(void) { DISPATCH }
/* Catch VM errors while retaining the exact result/state/payload snapshots. */
static int call(void) {raised=nil;if(setjmp(boundary)==0) {invoke();return 1;}return 0;}
/* Check independent ciphertext and IV vectors across aligned split updates. */
static void vectors(void) {
    int k,n,split,i;uchar plain[129],ivec[16];
    for(i=0;i<129;i++)plain[i]=(uchar)(i*13+7);
    for(i=0;i<16;i++)ivec[i]=i;
    for(k=0;k<3;k++) for(n=0;n<=129;n++) for(split=0;split<=n;split+=16) {
        key=bytes(keys[k],key_sizes[k]);iv=bytes(ivec,16);
        result=oldstate();operation=0;assert(call());state=result;
        input=bytes(plain,n);operation=1;direction=0;count=split;assert(call());
        input->data+=split;input->len-=split;count=n-split;assert(call());
        input->data-=split;input->len+=split;
        assert(!memcmp(input->data,ciphertext[k][n],n));
        assert(!memcmp(((XAESstate*)state)->state.ivec,finaliv[k][n],16));guard(input);
        operation=0;assert(call());state=result;
        operation=1;direction=1;count=split;assert(call());
        input->data+=split;input->len-=split;count=n-split;assert(call());
        input->data-=split;input->len+=split;
        assert(!memcmp(input->data,plain,n));guard(input);
        destroy(key);destroy(iv);destroy(input);destroy(result);assert(live==0);checks++;
    }
}
/* Rejected setup preserves an old owner across every validation/allocation path. */
static void setupfailures(void) {
    int mode,before;MOD_AESstate *old;
    for(mode=0;mode<11;mode++) {
        key=bytes(keys[0],16);iv=bytes(nil,16);result=oldstate();old=result;operation=0;
        if(mode==0)key->len=0;
        if(mode==1)key->len=-1;
        if(mode==2)key->len=sizeof(WORD)>4?(WORD)((uvlong)1<<32):-1;
        if(mode==3)key->t=TAESstate;
        if(mode==4)key->data=nil;
        if(mode==5)iv->len=15;
        if(mode==6)iv->t=TAESstate;
        if(mode==7)iv->data=nil;
        if(mode==8)allocation_failure=0;
        if(mode==9) {destroy(key);key=H;}
        if(mode==10) {destroy(key);key=nil;}
        before=live;assert(!call() && result==old && live==before);
        allocation_failure=-1;
        if(key!=H && key!=nil) {key->len=16;key->t=&Tbyte;key->data=(uchar*)(key+1);destroy(key);}
        iv->len=16;iv->t=&Tbyte;iv->data=(uchar*)(iv+1);
        destroy(iv);destroy(result);assert(live==0);checks++;
    }
}
/* Counts and descriptors are validated before CBC changes the IV or payload. */
static void bounds(void) {
    AESstate saved;uchar payload[16];WORD original;
    int mode;
    for(mode=0;mode<9;mode++) {
        key=bytes(keys[0],16);iv=H;result=oldstate();operation=0;assert(call());state=result;
        input=bytes(nil,16);saved=((XAESstate*)state)->state;memmove(payload,input->data,16);
        original=input->len;operation=1;direction=mode&1;count=16;
        if(mode==0)count=-1;
        if(mode==1)count=sizeof(WORD)>4?(WORD)((uvlong)1<<32):-1;
        if(mode==2)input->len=-1;
        if(mode==3)input->len=sizeof(WORD)>4?(WORD)((uvlong)1<<32):-1;
        if(mode==4)input->t=TAESstate;
        if(mode==5)input->data=nil;
        if(mode==6)state=nil;
        if(mode==7)count=17;
        if(mode==8)state=(MOD_AESstate*)key;
        assert(!call());assert(!memcmp(&saved,&((XAESstate*)result)->state,sizeof(saved)));
        input->len=original;input->t=&Tbyte;input->data=(uchar*)(input+1);
        assert(!memcmp(payload,input->data,16));guard(input);
        destroy(key);destroy(input);destroy(result);assert(live==0);checks++;
    }
    /* An empty non-nil array may have no data; CBC does not move its IV. */
    key=bytes(keys[0],16);iv=H;result=oldstate();operation=0;assert(call());state=result;
    input=bytes(nil,0);input->data=nil;saved=((XAESstate*)state)->state;
    operation=1;count=0;direction=0;assert(call());direction=1;assert(call());
    assert(!memcmp(&saved,&((XAESstate*)state)->state,sizeof(saved)));
    input->data=(uchar*)(input+1);destroy(input);destroy(key);destroy(result);checks++;
}
/* Native rekeying can read key and IV from inside the destination state. */
static void nativechecks(void) {
    AESstate s,saved;uchar ivec[16];int k,i,mode;
    for(i=0;i<16;i++)ivec[i]=i;
    for(k=0;k<3;k++) {
        setupAESstate(&s,keys[k],key_sizes[k],ivec);saved=s;
        setupAESstate(&s,s.key,s.keybytes,s.ivec);assert(!memcmp(&s,&saved,sizeof(s)));checks++;
    }
    setupAESstate(&s,keys[2],64,ivec);
    assert(!memcmp(&s,&saved,sizeof(s)));checks++;
    for(mode=0;mode<6;mode++) {
        saved=s;raised=nil;
        if(setjmp(boundary)==0) {
            if(mode==0)setupAESstate(&s,keys[0],0,nil);
            if(mode==1)setupAESstate(&s,nil,16,nil);
            if(mode==2)setupAESstate(&s,keys[0],17,nil);
            if(mode==3)aesCBCencrypt(nil,1,&s);
            if(mode==4)aesCBCdecrypt(nil,1,&s);
            if(mode==5)aesCBCencrypt((uchar*)1,-1,&s);
            assert(0);
        }
        assert(raised==exInval && !memcmp(&s,&saved,sizeof(s)));checks++;
    }
}
/* Require matching streams and complete ownership balance in both VM word models. */
int main(void) {
    vectors();setupfailures();bounds();nativechecks();
    assert(live==0 && state_type.ref==1 && Tarray.ref==1 && Tbyte.ref==1);
    printf("PASS MOD AES: %d vectors/split/alias/bounds/ownership cases, WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
