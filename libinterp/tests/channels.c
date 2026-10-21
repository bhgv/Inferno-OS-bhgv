#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include "pool.h"
#include <assert.h>
#include <stdio.h>
static Proc worker;
static char *last_error;
Proc *getup(void) {return &worker;}
void nexterror(void) {assert(worker.nerr>0);siglongjmp(worker.estack[--worker.nerr],1);}
void error(char *s) {last_error=s;nexterror();}
int __lsan_is_turned_off(void) {return 1;}
char exNomem[]="malloc",exHeap[]="heap",exNegsize[]="negative";
static char injected[]="injected before allocation";
static void *owned[256];
static int live,fail_after=-1,throw_after=-1,calls;
static void *alloc(size_t n) {
    void *p;int i;calls++;
    if(throw_after==0)error(injected);
    if(throw_after>0)throw_after--;
    if(fail_after==0 || n>1024*1024)return nil;
    if(fail_after>0)fail_after--;
    p=malloc(n?n:1);assert(p);memset(p,0xa5,n);
    for(i=0;i<256;i++)if(!owned[i]){owned[i]=p;live++;return p;}
    assert(0);return nil;
}
static void dealloc(void *p) {
    int i;if(!p)return;
    for(i=0;i<256;i++)if(owned[i]==p){owned[i]=nil;live--;free(p);return;}
    assert(0);
}
static void *allocz(size_t n,int z) {void *p=alloc(n);if(p && z)memset(p,0,n);return p;}
#define malloc(n) alloc(n)
#define mallocz(n,z) allocz(n,z)
#define free(p) dealloc(p)
Pool *heapmem;
int mutator,nprop;
void (*heapmonitor)(int,void*,ulong);
void *poolalloc(Pool *p,ulong n) {(void)p;return alloc(n);}
Type Tarray={1,0,0,sizeof(Array)};
Type Tchannel={1,0,0,sizeof(Channel),1,0,0,{0x80}};
REG R;
void markheap(Type *t,void *p) {(void)t;(void)p;}
void movtmp(void) {}
void movm(void) {}
static void scalar_move(void) {}
void killcomm(Progq **q) {
    Progq *p;
    while((p=*q)!=nil){assert(p->prog==nil);*q=p->next;dealloc(p);}
}
void freechan(Heap*,int);
void freearray(Heap*,int);
void destroy(void *p) {
    Heap *h;Type *t;
    if(p==H)return;
    h=D2H(p);assert(h->ref>0);if(--h->ref)return;
    t=h->t;
    if(t==&Tchannel)freechan(h,0);
    else if(t==&Tarray)freearray(h,0);
    else assert(0);
    freetype(t);dealloc(h);
}
#define OP(n) void n(void)
#define W(r) *((WORD*)(R.r))
#define BIT(bt,nb) ((bt)&(1<<(nb)))
IMPLEMENTATION
static Type element={1,0,0,sizeof(WORD)};
static Channel *result;
static int mode,count;
static WORD size,capacity;
static int run(void *unused) {
    (void)unused;
    switch(mode) {
    case 0:result=cnewc(&element,scalar_move,count);break;
    case 1:result=cnewc(&element,movtmp,count);break;
    case 2:newc(&element,scalar_move);break;
    case 3:newcm();break;
    case 4:result=cnewc(nil,movm,count);break;
    }
    return 1;
}
static void balanced(int expected) {
    assert(live==expected && worker.nerr==0 && element.ref==1 && Tchannel.ref==1 && Tarray.ref==1);
}
int main(void) {
    int i,n,base,successes=0,failures=0,checks=0;
    Channel *old;
    result=H;
    for(mode=0;mode<=1;mode++)for(count=0;count<=3;count+=3) {
        for(i=0;i<8;i++) {
            fail_after=i;
            if(kprotect(run,nil)) {
                successes++;assert(result!=H);
                assert(result->send && result->recv && !result->send->next && !result->recv->next);
                assert(result->front==0 && result->size==0 && result->aux==nil);
                if(count)assert(result->buf->len==count && result->buf->t==&element);
                else assert(result->buf==H);
                if(mode==1)assert(result->mid.t==&element);
                fail_after=-1;destroy(result);result=H;
            } else {
                failures++;assert(last_error==exNomem || last_error==exHeap);
                assert(result==H);fail_after=-1;
            }
            balanced(0);checks++;
        }
        for(i=0;i<(count?4:3);i++) {
            throw_after=i;assert(!kprotect(run,nil));assert(last_error==injected && result==H);
            throw_after=-1;balanced(0);checks++;
        }
    }
    assert(successes && failures);
    mode=0;count=-1;n=calls;assert(!kprotect(run,nil) && last_error==exNegsize && calls==n);
    count=0x7fffffff;n=calls;assert(!kprotect(run,nil) && last_error==exHeap && calls==n);
    mode=4;count=1;n=calls;assert(!kprotect(run,nil) && last_error==exHeap && calls==n);
    count=0;assert(kprotect(run,nil));destroy(result);result=H;balanced(0);
    /* A failed replacement retains the existing channel, including its buffer. */
    old=cnewc(&element,scalar_move,2);result=old;base=live;
    R.d=&result;R.m=&capacity;capacity=3;mode=2;
    for(i=0;i<4;i++) {
        fail_after=i;assert(!kprotect(run,nil));fail_after=-1;
        assert(result==old && live==base && old->buf->len==2 && worker.nerr==0);
    }
    assert(kprotect(run,nil) && result!=old);destroy(result);result=H;balanced(0);
    if(sizeof(WORD)>4) {
        capacity=(WORD)((uvlong)1<<32);n=calls;
        assert(!kprotect(run,nil) && last_error==exHeap && calls==n && result==H);
    }
    /* Raw channels own a temporary descriptor only until construction ends. */
    R.s=&size;R.m=&capacity;R.d=&result;mode=3;size=13;capacity=3;
    for(i=0;i<9;i++) {
        fail_after=i;
        if(kprotect(run,nil)) {
            fail_after=-1;assert(result->mid.w==13 && result->buf->t->ref==1);
            destroy(result);result=H;
        } else {fail_after=-1;assert(result==H);}
        balanced(0);checks++;
    }
    for(i=1;i<5;i++) {
        throw_after=i;assert(!kprotect(run,nil) && last_error==injected && result==H);
        throw_after=-1;balanced(0);checks++;
    }
    size=-1;n=calls;assert(!kprotect(run,nil) && last_error==exNegsize && calls==n);
    size=13;capacity=-1;assert(!kprotect(run,nil) && last_error==exNegsize);balanced(0);
    capacity=0;assert(kprotect(run,nil) && result->buf==H && result->mid.w==13);
    destroy(result);result=H;balanced(0);
    R.m=R.d;assert(kprotect(run,nil) && result->buf==H);destroy(result);result=H;balanced(0);
    printf("PASS channels: %d failure/cleanup scenarios, scalar/raw/typed buffers, reference counts, replacement and preserved VM errors\n",checks);
}
