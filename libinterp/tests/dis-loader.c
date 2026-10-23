/* Allocator, heap and compiler boundaries around the production parser. */
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
static Proc worker;
Proc *getup(void) {return &worker;}
void nexterror(void) {assert(worker.nerr>0);siglongjmp(worker.estack[--worker.nerr],1);}
void error(char *s) {(void)s;nexterror();}
int __lsan_is_turned_off(void) {return 1;}
static void *owned[512];
static int live,fail_after=-1,throw_heap,throw_array,throw_string,throw_compile;
static void *testmalloc(size_t n) {
    void *p;int i;
    if(fail_after==0 || n>1024*1024)return nil;
    if(fail_after>0)fail_after--;
    p=calloc(1,n?n:1);assert(p);
    for(i=0;i<512;i++)if(!owned[i]){owned[i]=p;live++;return p;}
    assert(0);return nil;
}
static void testfree(void *p) {
    int i;if(!p)return;
    for(i=0;i<512;i++)if(owned[i]==p){owned[i]=nil;live--;free(p);return;}
    assert(0);
}
static void *testmallocz(size_t n,int z) {(void)z;return testmalloc(n);}
static char *teststrdup(const char *s) {
    size_t n=strlen(s)+1;char *p=testmalloc(n);if(p)memcpy(p,s,n);return p;
}
#define malloc(n) testmalloc(n)
#define mallocz(n,z) testmallocz(n,z)
#define free(p) testfree(p)
#define strdup(s) teststrdup(s)
#define BIT(bt,nb) ((bt)&(1<<(nb)))
#define A(r) *((Array**)(r))
static char message[128];
void kwerrstr(char *fmt,...) {snprintf(message,sizeof(message),"%s",fmt);}
char exNomem[]="out of memory";
Module *modules;
int cflag,dontcompile;
Type Tarray={1}, Tstring={1};
void markheap(Type *t,void *p) {(void)t;(void)p;}
void freeheap(Heap *h,int n) {(void)h;(void)n;}
static void pointers(Type *t,uchar *data) {
    int i,bit;
    for(i=0;i<t->np;i++)for(bit=0;bit<8;bit++)
        if(t->map[i]&(0x80>>bit))destroy(*(void**)(data+(i*8+bit)*sizeof(void*)));
}
void destroy(void *p) {
    Heap *h;Type *t;Array *ar;int i;
    if(p==H)return;
    assert(p!=nil);h=D2H(p);t=h->t;assert(h->ref>0);
    if(--h->ref)return;
    if(t==&Tarray) {
        ar=p;for(i=0;i<ar->len;i++)pointers(ar->t,ar->data+i*ar->t->size);
        freetype(ar->t);Tarray.ref--;
    } else if(t==&Tstring) Tstring.ref--;
    else {pointers(t,p);freetype(t);}
    testfree(h);
}
Heap *heapz(Type *t) {
    Heap *h;if(throw_heap)error("heap injected");
    h=testmalloc(sizeof(Heap)+t->size);if(!h)error(exNomem);
    h->t=t;h->ref=1;t->ref++;initmem(t,H2D(void*,h));return h;
}
Heap *nheap(int n) {
    Heap *h;if(throw_array)error("array injected");
    h=testmalloc(sizeof(Heap)+n);if(!h)error(exNomem);
    h->ref=1;return h;
}
String *c2string(char *s,int n) {
    Heap *h;(void)s;(void)n;if(throw_string)error("string injected");
    h=testmalloc(sizeof(Heap)+sizeof(String));if(!h)error(exNomem);
    h->ref=1;h->t=&Tstring;Tstring.ref++;return H2D(String*,h);
}
int mustbesigned(char *p,uchar *b,ulong n,Dir *d) {(void)p;(void)b;(void)n;(void)d;return 0;}
int verifysigner(uchar *a,int b,uchar *c,ulong d) {(void)a;(void)b;(void)c;(void)d;return 0;}
int compile(Module *m,int n,Modlink *ml) {
    (void)m;(void)n;(void)ml;if(throw_compile)error("compile injected");return 0;
}
IMPLEMENTATION
static uchar buffer[2048];
static int at,map_at,arraylen_at,exportpc_at,exporttype_at,exportname_at,handlertype_at,handlerpc_at;
static void byte(int n) {assert(at<sizeof(buffer));buffer[at++]=n;}
static void integer(int v) {
    unsigned int u=(unsigned int)v;
    if(v>=-64 && v<=63)byte(v&127);
    else if(v>=-8192 && v<=8191){byte(0x80|((u>>8)&63));byte(u&255);}
    else {byte(0xc0|((u>>24)&63));byte((u>>16)&255);byte((u>>8)&255);byte(u&255);}
}
static void word(unsigned int v) {byte(v>>24);byte(v>>16);byte(v>>8);byte(v);}
static void name(char *s) {while(*s)byte(*s++);byte(0);}
static void fixture(void) {
    at=0;
    integer(XMAGIC);integer(HASLDT|HASEXCEPT);integer(1024);
    integer(1);integer(64);integer(2);integer(1);integer(0);integer(1);
    byte(IRET);byte(SRC(AXXX)|DST(AXXX));
    integer(0);integer(64);integer(1);map_at=at;byte(0x80);
    integer(1);integer(sizeof(WORD));integer(0);
    byte(DBYTE(DEFS,3));integer(0);byte('a');byte('b');byte('c');
    byte(DBYTE(DEFS,2));integer(0);byte('x');byte('y');
    byte(DBYTE(DEFA,1));integer(0);word(1);arraylen_at=at;word(2);
    byte(DBYTE(DIND,1));integer(0);word(0);
    byte(DBYTE(DEFW,2));integer(0);word(42);word(99);
    byte(DBYTE(DAPOP,1));integer(0);
    byte(DBYTE(DEFB,1));integer(sizeof(void*));byte(7);
    byte(0);name("fixture");
    exportpc_at=at;integer(0);exporttype_at=at;integer(1);word(42);exportname_at=at;name("run");
    integer(1);integer(1);word(42);name("import");byte(0);
    integer(1);integer(0);integer(0);integer(1);handlertype_at=at;integer(1);
    integer(0x10001);name("exception");handlerpc_at=at;integer(0);integer(-1);byte(0);
}
static void instructionfixture(int add,int first,int second) {
    at=0;
    integer(XMAGIC);integer(0);integer(1024);integer(1);
    integer(0);integer(0);integer(0);integer(0);integer(-1);
    byte(IRET);byte(add);
    if((add&ARM)!=AXNON)integer(first);
    if(USRC(add)==(AFP|AIND)){integer(first);integer(second);}
    if(UDST(add)==(AFP|AIND)){integer(first);integer(second);}
    byte(0);name("instruction");
}
static Module *parse(int n) {
    uchar *copy=testmalloc(n);Dir d={0};Module *m;
    assert(copy);memcpy(copy,buffer,n);
    m=parsemod("fixture",copy,n,&d);testfree(copy);return m;
}
static void drop(Module *m) {
    destroy(m->origmp);destroylinks(m);freemod(m);modules=nil;
    assert(live==0 && worker.nerr==0 && Tarray.ref==1 && Tstring.ref==1);
}
static void rejected(void) {
    assert(parse(at)==nil && live==0 && worker.nerr==0 && !modules);
    assert(Tarray.ref==1 && Tstring.ref==1);
}
int main(void) {
    static const int values[]={-536870912,-8193,-8192,-65,-64,-1,0,1,63,64,8191,8192,536870911};
    Module *m;Array *ar;uchar *p;unsigned int i;int n,total,j,mutations=0,failures=0,successes=0;
    for(i=0;i<nelem(values);i++){at=0;integer(values[i]);p=buffer;assert(operand(&p)==values[i] && p==buffer+at);}
    at=0;word(0xffffffff);p=buffer;assert(disw(&p)==0xffffffffUL);
    fixture();total=at;m=parse(total);if(!m)fprintf(stderr,"parse failed: %s\n",message);assert(m && m->jit==nil);
    ar=A(m->origmp);assert(ar->len==2 && ((WORD*)ar->data)[0]==42 && ((WORD*)ar->data)[1]==99);
    assert(m->origmp[sizeof(void*)]==7 && m->htab[0].pc2==1);drop(m);
    for(n=0;n<total;n++){fixture();assert(parse(n)==nil && live==0 && !modules && worker.nerr==0);}
    /* Allocation cutoffs include export names and nested metadata tables. */
    for(n=1;n<45;n++){
        fixture();fail_after=n;m=parse(at);fail_after=-1;
        if(m){successes++;drop(m);}else{failures++;assert(live==0 && !modules && worker.nerr==0);}
    }
    assert(failures && successes);
    fixture();buffer[map_at]=0;rejected();
    fixture();buffer[arraylen_at]=0xff;rejected();
    fixture();buffer[exportpc_at]=127;rejected();
    fixture();buffer[exportpc_at]=127;buffer[exporttype_at]=127;
    memcpy(buffer+exportname_at,".mp",3);
    m=parse(at);assert(m && m->ext[0].u.pc==nil && m->ext[0].frame==nil);drop(m);
    fixture();buffer[exportpc_at]=1;rejected();
    fixture();buffer[exporttype_at]=2;rejected();
    fixture();buffer[handlertype_at]=2;rejected();
    fixture();buffer[handlerpc_at]=1;rejected();
    fixture();throw_heap=1;rejected();throw_heap=0;
    fixture();throw_array=1;rejected();throw_array=0;
    fixture();throw_string=1;rejected();throw_string=0;
    fixture();cflag=1;throw_compile=1;rejected();throw_compile=0;cflag=0;
    /* Deterministic byte mutations run the production parser without executing
     * the resulting bytecode. Cap host fixture allocations at one MiB. */
    for(n=0;n<total;n++)for(j=0;j<256;j++) {
        fixture();buffer[n]=j;m=parse(at);
        if(m)drop(m);
        else assert(live==0 && worker.nerr==0 && !modules && Tarray.ref==1 && Tstring.ref==1);
        mutations++;
    }
    {
        static const int values[]={-32769,-32768,-1,0,32767,65535,65536};
        unsigned int k;int mode,valid,indirect;
        for(mode=AXIMM;mode<=AXINM;mode+=AXIMM)for(k=0;k<nelem(values);k++) {
            instructionfixture(SRC(AXXX)|DST(AXXX)|mode,values[k],0);
            valid=values[k]>=-32768 && values[k]<=65535 && (mode==AXIMM || values[k]>=0);
            if(valid){m=parse(at);assert(m);drop(m);}else rejected();
        }
        for(indirect=0;indirect<2;indirect++)for(k=0;k<nelem(values);k++) {
            int add=indirect?SRC(AXXX)|DST(AFP|AIND):SRC(AFP|AIND)|DST(AXXX);
            instructionfixture(add,values[k],0);
            if(values[k]>=0 && values[k]<=65535){m=parse(at);assert(m);drop(m);}else rejected();
            instructionfixture(add,0,values[k]);
            if(values[k]>=0 && values[k]<=65535){m=parse(at);assert(m);drop(m);}else rejected();
        }
        instructionfixture(SRC(6)|DST(AXXX),0,0);rejected();
        instructionfixture(SRC(AXXX)|DST(7),0,0);rejected();
    }
    at=0;integer(SMAGIC);integer(-1);rejected();
    assert(!parsemod("nil",nil,0,nil));
    printf("PASS Dis loader: %d truncations, %d mutations, allocation cutoffs, instruction field bounds, GC/data maps, nested arrays, metadata indices, overwrites and VM-error cleanup\n",total,mutations);
}
