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
typedef struct XRC4state XRC4state;
struct XRC4state { MOD_RC4state x; RC4state state; };
static Type state_type={1,0,0,sizeof(XRC4state)};
static Type *TRC4state=&state_type;
Type Tarray={1},Tbyte={1};
static char exBadKey[]="key";
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
static Array *input,*seed;
static MOD_RC4state *state,*result;
static WORD count;
static int operation;
/* Use actual generated frames for setup, update, skip and reverse. */
static void invoke(void) { DISPATCH }
/* Catch VM errors before comparing byte-for-byte state and output snapshots. */
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
/* Allocate an old stream owner whose identity must survive rejected calls. */
static MOD_RC4state *oldstate(void) {return H2D(MOD_RC4state*,heap(TRC4state));}
VECTORS
/* Process every split point and compare with an independently generated stream. */
static void vectors(void) {
    int k,n,split,i,before;
    int sizes[]={0,1,15,16,63,64,127,128,255,256,257,512};
    uchar plain[512];
    for(i=0;i<512;i++)plain[i]=(uchar)(i*13+7);
    for(k=0;k<5;k++) for(n=0;n<12;n++) for(split=0;split<=sizes[n];split++) {
        seed=bytes(keys[k],key_sizes[k]);result=oldstate();state=H;operation=0;
        assert(call());destroy(seed);state=result;
        input=bytes(plain,sizes[n]);before=live;operation=1;count=split;
        assert(call() && live==before);
        input->data+=split;input->len-=split;count=input->len;
        assert(call());input->data-=split;input->len+=split;
        for(i=0;i<sizes[n];i++)assert(input->data[i]==(uchar)(plain[i]^streams[k][i]));
        guard(input);destroy(input);destroy(result);assert(live==0);checks++;
    }
}
/* Skipping equals consuming bytes; reversing restores the complete permutation. */
static void movement(void) {
    RC4state initial,advanced;
    int k,n,i;
    for(k=0;k<5;k++) for(n=0;n<=512;n++) {
        seed=bytes(keys[k],key_sizes[k]);result=oldstate();operation=0;
        assert(call());destroy(seed);state=result;
        initial=((XRC4state*)result)->state;
        operation=2;count=n;assert(call());advanced=((XRC4state*)result)->state;
        input=bytes(nil,32);operation=1;count=32;assert(call());
        for(i=0;i<32;i++)assert(input->data[i]==streams[k][n+i]);
        destroy(input);((XRC4state*)result)->state=advanced;
        operation=3;count=n;assert(call());
        assert(!memcmp(&initial,&((XRC4state*)result)->state,sizeof(initial)));
        destroy(result);assert(live==0);checks++;
    }
}
/* Rejected native/VM counts may neither move state nor change payload bytes. */
static void bounds(void) {
    RC4state snapshot;MOD_RC4state *old;WORD savedlen;
    int op,mode,before;
    uchar original[3];
    for(op=1;op<=3;op++) for(mode=0;mode<5;mode++) {
        seed=bytes((uchar*)"Key",3);result=oldstate();operation=0;assert(call());
        destroy(seed);state=result;old=result;input=bytes((uchar*)"abc",3);
        snapshot=((XRC4state*)result)->state;memmove(original,input->data,3);
        operation=op;count=mode==0?-1:mode==1?(WORD)((UWORD)1<<(sizeof(WORD)*8-1)):
            sizeof(WORD)>4?(WORD)((uvlong)1<<32):-2;
        before=live;assert(!call() && raised==exBounds && live==before && result==old);
        assert(!memcmp(&snapshot,&((XRC4state*)result)->state,sizeof(snapshot)));
        assert(!memcmp(input->data,original,3));guard(input);
        destroy(input);destroy(result);assert(live==0);checks++;
    }
    /* Impossible descriptor lengths and element types fail before native access. */
    for(mode=0;mode<5;mode++) {
        seed=bytes((uchar*)"Key",3);result=oldstate();operation=0;assert(call());
        destroy(seed);state=result;input=bytes(nil,0);savedlen=input->len;
        snapshot=((XRC4state*)result)->state;operation=1;count=0;
        if(mode==0)input->len=-1;
        if(mode==1)input->t=TRC4state;
        if(mode==2)input->len=sizeof(WORD)>4?(WORD)((uvlong)1<<32):-1;
        if(mode==3) {input->len=1;input->data=nil;}
        if(mode==4)state=nil;
        assert(!call());assert(!memcmp(&snapshot,&((XRC4state*)result)->state,sizeof(snapshot)));
        input->len=savedlen;input->t=&Tbyte;input->data=(uchar*)(input+1);
        destroy(input);destroy(result);assert(live==0);checks++;
    }
}
/* Setup keeps old owners across invalid seeds and VM allocation failure. */
static void setupfailures(void) {
    int mode,before;MOD_RC4state *old;
    for(mode=0;mode<8;mode++) {
        result=oldstate();old=result;seed=bytes((uchar*)"Key",3);operation=0;
        if(mode==0)seed->len=0;
        if(mode==1)seed->len=-1;
        if(mode==2)seed->len=sizeof(WORD)>4?(WORD)((uvlong)1<<32):-1;
        if(mode==3)seed->t=TRC4state;
        if(mode==4)seed->data=nil;
        if(mode==5)allocation_failure=0;
        if(mode==6) {destroy(seed);seed=nil;}
        if(mode==7) {destroy(seed);seed=H;}
        before=live;
        if(mode==7 && KEYRING) {assert(call() && result==H && live==before-1);}
        else {assert(!call() && result==old && live==before);}
        allocation_failure=-1;
        if(seed!=H && seed!=nil) {seed->len=3;seed->t=&Tbyte;seed->data=(uchar*)(seed+1);destroy(seed);}
        destroy(result);assert(live==0);checks++;
    }
}
/* Native invalid-input guards precede permutation changes and pointer arithmetic. */
static void nativebounds(void) {
    RC4state s,saved;int mode;
    memset(&s,0x5a,sizeof(s));saved=s;
    for(mode=0;mode<5;mode++) {
        raised=nil;
        if(setjmp(boundary)==0) {
            if(mode==0)setupRC4state(&s,(uchar*)1,0);
            if(mode==1)setupRC4state(&s,nil,3);
            if(mode==2)rc4(&s,nil,1);
            if(mode==3)rc4skip(&s,-1);
            if(mode==4)rc4back(&s,-1);
            assert(0);
        }
        assert(raised==exInval && !memcmp(&s,&saved,sizeof(s)));checks++;
    }
    rc4(&s,nil,0);assert(!memcmp(&s,&saved,sizeof(s)));checks++;
}
/* Require real VM type/ref accounting to balance after every case. */
int main(void) {
    vectors();movement();bounds();setupfailures();nativebounds();
    assert(live==0 && state_type.ref==1 && Tarray.ref==1 && Tbyte.ref==1);
    printf("PASS MOD RC4: %d stream/split/reverse/bounds/ownership cases, WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
