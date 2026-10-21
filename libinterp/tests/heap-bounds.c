/* Real heap sizing, allocation and VM operation bodies. The pool is bounded. */
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "pool.h"
#include "raise.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static Proc worker;
static char *last_error;
Proc *getup(void) {return &worker;}
void nexterror(void) {assert(worker.nerr>0);siglongjmp(worker.estack[--worker.nerr],1);}
void error(char *s) {last_error=s;nexterror();}
int __lsan_is_turned_off(void) {return 1;}
char exHeap[]="heap",exNegsize[]="negative",exBounds[]="bounds",exNilref[]="nil";
Pool *heapmem;
int mutator,nprop;
void (*heapmonitor)(int,void*,ulong);
Type Tarray={1,0,0,sizeof(Array)},Tmodlink={1},Tlist={1};
REG R;
static ulong requested;
static int poolcalls,live,force_failure;
void *poolalloc(Pool *p,ulong n) {
    void *v;(void)p;requested=n;poolcalls++;
    if(force_failure || n>1024*1024)return nil;
    v=malloc(n);assert(v);memset(v,0xa5,n);live++;return v;
}
void freetype(Type *t) {assert(t->ref>1);t->ref--;}
void destroy(void *p) {
    Heap *h;Array *a;
    if(p==H)return;
    h=D2H(p);assert(h->ref>0);if(--h->ref)return;
    if(h->t==&Tarray) {
        a=p;
        if(a->root==H && a->t->np) {
            WORD j;for(j=0;j<a->len;j++)freeptrs(a->data+j*a->t->size,a->t);
        }
        freetype(a->t);if(a->root!=H)destroy(a->root);Tarray.ref--;
    } else if(h->t==&Tmodlink) Tmodlink.ref--;
    else if(h->t==&Tlist)Tlist.ref--;
    else if(h->t)freetype(h->t);
    free(h);live--;
}
#define OP(n) void n(void)
#define W(r) *((WORD*)(R.r))
#define A(r) *((Array**)(R.r))
#define BIT(bt,nb) ((bt)&(1<<(nb)))
IMPLEMENTATION
static int which,tsz,n,link_count;
static WORD length;
static ulong listsize;
static Type type={1};
static List *list=H;
static Module module;
static Heap *allocated;
static Array *copyfrom;
static int run(void *unused) {
    (void)unused;
    switch(which) {
    case 0:acheck(tsz,length);break;
    case 1:allocated=nheap(n);break;
    case 2:allocated=heap(&type);break;
    case 3:allocated=heapz(&type);break;
    case 4:allocated=heaparray(&type,n);break;
    case 5:allocated=D2H(arraycpy(copyfrom));break;
    case 6:allocated=D2H(mklinkmod(&module,link_count));break;
    case 7:cons(listsize,&list);break;
    case 8:slicea();break;
    case 9:slicela();break;
    case 10:newa();break;
    case 11:newaz();break;
    }
    return 1;
}
static void rejects(char *expected,int noallocation) {
    int before=poolcalls,objects=live;
    last_error=nil;assert(!kprotect(run,nil));
    assert(last_error==expected && worker.nerr==0 && live==objects);
    if(noallocation)assert(before==poolcalls);
}
int main(void) {
    static const int widths[]={0,1,2,4,8,16,4096,0x7fffffff};
    unsigned int i;int checks=0,limit,before;
    Array *a,*b,*old;Heap *h;
    Type *types[]={&type};Modlink ml={0};
    WORD count,first,last,offset;
    List *saved;
    type.size=4;ml.type=types;R.M=&ml;
    which=0;
    for(i=0;i<nelem(widths);i++) {
        tsz=widths[i];
        limit=tsz==0?0x7fffffff:(0x7fffffff-sizeof(Heap)-sizeof(Array))/tsz;
        length=limit;assert(kprotect(run,nil));checks++;
        if(limit<0x7fffffff){length=(WORD)limit+1;rejects(exHeap,1);checks++;}
        length=-1;rejects(exNegsize,1);checks++;
    }
    tsz=-1;length=0;rejects(exHeap,1);
    tsz=16;length=0x11111111;rejects(exHeap,1); /* old multiply-and-wrap heuristic */
    if(sizeof(WORD)>4){tsz=0;length=(WORD)((uvlong)1<<32);rejects(exHeap,1);}
    which=1;n=-1;rejects(exHeap,1);
    n=0x7fffffff;rejects(exHeap,1);
    n=0x7fffffff-sizeof(Heap);force_failure=1;rejects(exHeap,0);
    assert(requested==0x7fffffff);force_failure=0;
    n=0;assert(kprotect(run,nil));assert(allocated->t==nil && allocated->ref==1);
    destroy(H2D(void*,allocated));
    for(which=2;which<=3;which++) {
        type.size=-1;rejects(exHeap,1);type.size=0x7fffffff;rejects(exHeap,1);
        type.size=16;assert(kprotect(run,nil));
        if(which==3)for(i=0;i<16;i++)assert(H2D(uchar*,allocated)[i]==0);
        destroy(H2D(void*,allocated));
    }
    type.size=16;which=4;n=0x11111111;rejects(exHeap,1);
    n=-1;rejects(exNegsize,1);n=3;assert(kprotect(run,nil));
    a=H2D(Array*,allocated);assert(a->len==3 && a->t==&type && a->root==H);
    memset(a->data,42,48);copyfrom=a;which=5;assert(kprotect(run,nil));
    b=H2D(Array*,allocated);assert(b->len==3 && memcmp(a->data,b->data,48)==0);
    old=b;copyfrom=a;a->len=0x11111111;rejects(exHeap,1);a->len=3;
    destroy(a);destroy(old);
    which=6;link_count=-1;rejects(exHeap,1);link_count=0x7fffffff;rejects(exHeap,1);
    link_count=0;assert(kprotect(run,nil));
    assert(requested==sizeof(Heap)+sizeof(Modlink)-sizeof(Modl));
    assert(H2D(Modlink*,allocated)->nlinks==0 && H2D(Modlink*,allocated)->MP==H);
    destroy(H2D(void*,allocated));
    which=7;listsize=~(ulong)0;saved=list;rejects(exHeap,1);assert(list==saved);
    listsize=sizeof(WORD);assert(kprotect(run,nil));destroy(list);list=H;
    /* Generated pointer slots are initialized to H, not zero. */
    type.size=sizeof(void*);type.np=1;type.map[0]=0x80;
    a=H2D(Array*,heaparray(&type,3));
    for(i=0;i<3;i++)assert(((void**)a->data)[i]==H);
    destroy(a);type.np=0;type.size=sizeof(WORD);
    a=H2D(Array*,heaparray(&type,5));
    for(i=0;i<5;i++)((WORD*)a->data)[i]=i+1;
    first=1;last=4;R.s=&first;R.m=&last;R.d=&a;which=8;
    old=a;assert(kprotect(run,nil));assert(a->len==3 && a->root==old && a->data==old->data+sizeof(WORD));
    destroy(a); /* also releases the transferred root */
    a=H2D(Array*,heaparray(&type,2));first=INT_MIN;last=INT_MAX;
    rejects(exBounds,1);destroy(a);
    a=H;first=last=INT_MIN;assert(kprotect(run,nil));
    /* Check end positions by subtraction, before an overflowing addition. */
    a=H2D(Array*,heaparray(&type,3));b=H2D(Array*,heaparray(&type,5));
    R.s=&a;R.d=&b;R.m=&offset;which=9;
    offset=INT_MAX;rejects(exBounds,1);offset=4;rejects(exBounds,1);
    offset=2;assert(kprotect(run,nil));destroy(a);destroy(b);
    /* Dis sizes keep their full WORD value until the range check. */
    a=H;count=0x11111111;type.size=16;offset=0;R.s=&count;R.m=&offset;R.d=&a;
    for(which=10;which<=11;which++) {
        rejects(exHeap,1);assert(a==H);
        if(sizeof(WORD)>4){count=(WORD)((uvlong)1<<32);rejects(exHeap,1);}
        count=2;assert(kprotect(run,nil));assert(a->len==2);destroy(a);a=H;
        count=0x11111111;
    }
    {
        Type scalar={1};void *value;
        scalar.size=sizeof(WORD);type.size=sizeof(void*);type.np=1;type.map[0]=0x80;
        a=H2D(Array*,heaparray(&type,3));b=H2D(Array*,heaparray(&type,5));
        value=H2D(void*,heapz(&scalar));((void**)a->data)[0]=value;
        ((void**)a->data)[2]=H2D(void*,heapz(&scalar));
        R.s=&a;R.d=&b;R.m=&offset;offset=1;which=9;
        assert(kprotect(run,nil));assert(D2H(value)->ref==2);
        destroy(a);assert(D2H(value)->ref==1);destroy(b);assert(scalar.ref==1);
        type.np=0;
    }
    assert(live==0 && worker.nerr==0 && type.ref==1 && Tarray.ref==1 && Tmodlink.ref==1 && Tlist.ref==1);
    printf("PASS heap bounds: %d array boundaries, overflow counterexample, pool/header limits, copies, links, lists, slices and full-WORD VM sizes\n",checks);
    return 0;
}
