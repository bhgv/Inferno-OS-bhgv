#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>
#include <malloc.h>

typedef struct Heapqry Heapqry;
struct Heapqry {char fmt;ulong addr,module;int count;};
Module *modules;
Type Tarray={1},Tlist={1},Tstring={1},Tchannel={1};
Type *Trdchan,*Twrchan;
String snil;
char Ebadctl[]="control",exNilref[]="nil",exNomem[]="memory";
static int checks;
static Inst instructions[8];
extern int _efgfmt(Fmt*);

/* Invalid module identifiers are not used by these bounded address queries. */
void error(char *s) {(void)s;assert(0);}
/* Keep full address/stack instrumentation without unsupported process inspection. */
int __lsan_is_turned_off(void) {return 1;}
/* Reuse the native allocator's actual capacity for production String.tmp caches. */
ulong msize(void *p) {return malloc_usable_size(p);}
void rerrstr(char *p,uint n) {if(n)snprintf(p,n,"fixture");}

IMPLEMENTATION

/* Validate the instruction address selected by the production query, using real fmt. */
static int
instructionfmt(Fmt *f)
{
    Inst *pc=va_arg(f->args,Inst*);
    assert(pc>=instructions && pc<instructions+8);
    return fmtprint(f,"I%ld",(long)(pc-instructions));
}

/* Allocate actual native heap headers for typed query values. */
static void*
object(Type *t,int bytes)
{
    Heap *h=calloc(1,sizeof(Heap)+bytes);assert(h);h->t=t;h->ref=1;
    return H2D(void*,h);
}

/* Compare paged bytes with an independent complete-record string. */
static void
records(char format,void *values,int bytes,int items,char *text)
{
    uchar output[528],*allocation,*slot,*saved;
    Heapqry query={0};
    ulong offsets[]={0,1,3,1000,((ulong)1<<32)+1,(ulong)-1};
    int alignment,offset,n,wanted,i;
    ulong skip;

    for(alignment=0;alignment<=4;alignment+=4){
        allocation=malloc(bytes+alignment);saved=malloc(bytes);assert(allocation && saved);
        memset(allocation,0xa5,bytes+alignment);slot=allocation+alignment;
        memcpy(slot,values,bytes);memcpy(saved,slot,bytes);
        query.fmt=format;query.addr=(ulong)slot;query.count=items;
        for(offset=0;offset<nelem(offsets);offset++){
            skip=offsets[offset];memset(output,0xa5,sizeof(output));
            n=progheap(&query,(char*)output+8,512,skip);
            wanted=skip<strlen(text) ? strlen(text)-skip : 0;
            assert(n==wanted && (n==0 || !memcmp(output+8,text+skip,n)));
            assert(!memcmp(slot,saved,bytes));
            for(i=0;i<8;i++)assert(output[i]==0xa5 && output[520+i]==0xa5);
            checks++;
        }
        for(i=1;i<=8;i++){
            memset(output,0xa5,sizeof(output));n=progheap(&query,(char*)output+8,i,0);
            assert(n>=0 && n<=i);assert(output[7]==0xa5 && output[8+i]==0xa5);checks++;
        }
        free(saved);free(allocation);
    }
}

/* Exercise exact scalar widths, native reference strides and nil module owners. */
static void
valid(void)
{
    WORD words[]={-3,(WORD)0x100000001LL};LONG big[]={(-9223372036854775807LL-1),0x100000001LL};
    uchar bytes[]={0,128,255};
    REAL reals[]={1.5,-2.0};void *pointers[]={H,nil,(void*)(uintptr)0x123456789abcULL};
    Modlink *module=object(&Tlist,sizeof(Modlink)),*links[]={H,module};
    Array *array=object(&Tarray,sizeof(Array)),*arrays[]={H,array};
    String *narrow=object(&Tstring,sizeof(String)+16),*wide=object(&Tstring,sizeof(String)+16),*strings[]={H,narrow,wide};
    Channel *channel=object(&Tchannel,sizeof(Channel)),*channels[]={H,channel};
    List *list=object(&Tlist,sizeof(List)),*lists[]={list,list};
    char text[512];

    snprintf(text,sizeof(text),"%lld\n%lld\n",(vlong)words[0],(vlong)words[1]);records('W',words,sizeof(words),2,text);
    records('B',bytes,sizeof(bytes),3,"0\n128\n255\n");
    records('V',big,sizeof(big),2,"-9223372036854775808\n4294967297\n");
    records('R',reals,sizeof(reals),2,"1.5\n-2\n");
    records('P',pointers,sizeof(pointers),3,"nil\n0\n123456789abc\n");
    module->MP=(void*)(uintptr)0x12345678;records('M',links,sizeof(links),2,"nil\n12345678\n");
    array->len=(WORD)0x100000002LL;array->data=(void*)(uintptr)0x12340000;
    snprintf(text,sizeof(text),"nil\n%lld.12340000\n",(vlong)array->len);records('A',arrays,sizeof(arrays),2,text);
    narrow->len=2;memcpy(narrow->Sascii,"xy",3);wide->len=-1;wide->Srune[0]=0x3a9;
    records('C',strings,sizeof(strings),3,"0.2.xy1.\xce\xa9");
    channel->buf=H;
    snprintf(text,sizeof(text),"nil\n0.%lx\n",(ulong)channel);records('c',channels,sizeof(channels),2,text);
    snprintf(text,sizeof(text),"%lx.%lx\n%lx.%lx\n",(ulong)&list->tail,(ulong)list->data,(ulong)&list->tail,(ulong)list->data);
    records('L',lists,sizeof(lists),2,text);
    free(wide->tmp);free(D2H(wide));free(D2H(narrow));free(D2H(list));free(D2H(channel));free(D2H(array));free(D2H(module));
}

/* Read instruction indices one by one rather than repeatedly relocating addresses. */
static void
code(void)
{
    Module module={0};Heapqry query={0};char output[128];int start,n;
    modules=&module;module.prog=instructions;module.nprog=8;
    query.fmt='I';query.module=(ulong)&module;query.count=2;
    for(start=0;start<7;start++){
        char expected[32];snprintf(expected,sizeof(expected),"I%d\nI%d\n",start,start+1);
        query.addr=start;n=progheap(&query,output,sizeof(output),0);
        assert(n==strlen(expected) && !memcmp(output,expected,n));checks++;
    }
    query.count=1;query.addr=8;assert(progheap(&query,output,sizeof(output),0)==-1);
    query.addr=0;module.compiled=1;assert(progheap(&query,output,sizeof(output),0)==-1);checks+=2;
}

/* Reject malformed owners, lengths and alignments before formatting or reading them. */
static void
invalid(void)
{
    Heapqry query={0};void *value=nil;uchar output[64];String *string;
    char formats[]="MALCc";int i,alignment;
    for(i=0;i<sizeof(formats)-1;i++){
        query.fmt=formats[i];query.addr=(ulong)&value;query.count=1;
        memset(output,0xa5,sizeof(output));assert(progheap(&query,(char*)output,sizeof(output),0)==-1);
        assert(output[0]==0xa5);checks++;
    }
    string=object(&Tstring,sizeof(String)+16);string->len=(-0x7fffffff-1);value=string;
    query.fmt='C';query.addr=(ulong)&value;assert(progheap(&query,(char*)output,sizeof(output),0)==-1);free(D2H(string));checks++;
    for(alignment=1;alignment<4;alignment++){
        query.fmt='V';query.addr=(ulong)1+alignment*4;query.addr=(query.addr&~3UL)+alignment;
        assert(progheap(&query,(char*)output,sizeof(output),0)==-1);checks++;
    }
    assert(progheap((Heapqry*)1,(char*)1,0,0)==0 && progheap((Heapqry*)1,(char*)1,-1,0)==0);checks+=2;
}

/* Force exact eight-byte scalar allocations to expose any wider host read. */
static void
exact(void)
{
    Heapqry query={0};LONG value=-9223372036854775807LL-1;REAL real=1.5;
    uchar *allocation;char output[64];int n,alignment;
    for(alignment=0;alignment<=4;alignment+=4){
        allocation=malloc(sizeof(value)+alignment);assert(allocation);
        memcpy(allocation+alignment,&value,sizeof(value));
        query.fmt='V';query.addr=(ulong)(allocation+alignment);query.count=1;
        n=progheap(&query,output,sizeof(output),0);assert(n==21 && !memcmp(output,"-9223372036854775808\n",21));
        memcpy(allocation+alignment,&real,sizeof(real));query.fmt='R';
        n=progheap(&query,output,sizeof(output),0);assert(n==4 && !memcmp(output,"1.5\n",4));
        free(allocation);checks+=2;
    }
}

/* Language nil module values must never be dereferenced while formatting. */
static void
nilmodule(void)
{
    Heapqry query={0};void *value=H;char output[32];int n;
    query.fmt='M';query.addr=(ulong)&value;query.count=1;
    n=progheap(&query,output,sizeof(output),0);assert(n==4 && !memcmp(output,"nil\n",4));checks++;
}

/* Preserve the high offset bits rather than treating a large read as offset one. */
static void
largeoffset(void)
{
    Heapqry query={0};uchar values[]={1,2};char output[32];
    query.fmt='B';query.addr=(ulong)values;query.count=2;
    assert(progheap(&query,output,sizeof(output),((ulong)1<<32)+1)==0);checks++;
}

/* Keep independent old-source failure families selectable. */
int
main(int argc,char **argv)
{
    fmtinstall('g',_efgfmt);fmtinstall('D',instructionfmt);
    if(argc==1 || !strcmp(argv[1],"exact"))exact();
    if(argc==1 || !strcmp(argv[1],"nilmodule"))nilmodule();
    if(argc==1 || !strcmp(argv[1],"offset"))largeoffset();
    if(argc==1 || !strcmp(argv[1],"valid"))valid();
    if(argc==1 || !strcmp(argv[1],"code"))code();
    if(argc==1 || !strcmp(argv[1],"invalid"))invalid();
    assert(checks>0);printf("PASS heap queries: %d width/stride/owner/pagination checks WORD%zu\n",checks,sizeof(WORD)*8);return 0;
}
