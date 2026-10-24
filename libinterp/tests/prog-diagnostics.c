#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>

static int metrics,checks,wideonly,walkmode;
static Heap fixtureheap;
static Inst program[8];
static Stkext *root;

/* Real formatters need a deterministic native error string boundary. */
void rerrstr(char *p,uint n) {if(n)snprintf(p,n,"fixture");}
/* Preserve address/stack instrumentation without unavailable process inspection. */
int __lsan_is_turned_off(void) {return 1;}
/* Model exact allocator sizes, including sums beyond the signed int range. */
int hmsize(void *p) {assert(p==&fixtureheap);return metrics ? 0x7fffffff : 128;}
ulong msize(void *p) {
    assert(p==program || p==root);
    return metrics==2 ? (ulong)0x7fffffff*2048+512 : metrics ? 0x7fffffff : 256;
}
/* Native-PC mapping is outside these uncompiled-frame cases. */
static Inst *pc2dispc(Inst *pc,Module *m) {(void)m;return pc;}

IMPLEMENTATION

/* A source name allocation ends at its NUL, so excess copies fail under ASan. */
static char*
namecopy(char *name)
{
    char *p=malloc(strlen(name)+1);assert(p);strcpy(p,name);return p;
}

/* Verify bounded name output, builtin frame alignment and native PC return values. */
static void
statuses(void)
{
    Module module={0},caller={0};Modlink link={0},owner={0};REG reg={0};
    union {uvlong align;uchar bytes[128];} storage;
    Frame header={0};
    uchar output[64],expected[64];char complete[64];
    int builtin,compiled,alignment,capacity;
    WORD pc,wanted;

    module.name=namecopy("m");caller.name=namecopy("caller");
    link.m=&module;link.prog=program;owner.m=&caller;owner.prog=program;
    reg.M=&link;reg.PC=program+3;header.lr=program+2;header.mr=&owner;
    for(builtin=0;builtin<2;builtin++)for(compiled=0;compiled<2;compiled++)
    for(alignment=0;alignment<8;alignment++)for(capacity=wideonly ? 31 : 0;capacity<32;capacity++){
        free(module.name);module.name=namecopy(builtin ? "$B" : "m");
        link.compiled=owner.compiled=compiled;
        memmove(storage.bytes+16+alignment,&header,sizeof(header));reg.FP=storage.bytes+16+alignment;
        memset(output,0xa5,sizeof(output));memset(expected,0xa5,sizeof(expected));
        strcpy(complete,builtin ? "caller[$B]" : "m");
        if(capacity){
            size_t n=strlen(complete);if(n>capacity-1)n=capacity-1;
            memcpy(expected+8,complete,n);expected[8+n]=0;
        }
        pc=modstatus(&reg,capacity ? (char*)output+8 : (char*)1,capacity);
        wanted=compiled ? (WORD)(uintptr)(builtin ? header.lr : reg.PC) : builtin ? 2 : 2;
        assert(pc==wanted && !memcmp(output,expected,sizeof(output)));checks++;
    }
    free(module.name);free(caller.name);
}

/* Walk byte-aligned frames and retain exact large allocation totals in kilobytes. */
static void
walks(void)
{
    Module module={0};Modlink link={0};Prog process={0};
    Type type={0};Frame header;
    union {uvlong align;uchar bytes[128];} first,second;
    uchar *frames[3];char output[512];
    uvlong size;int a,b,mask,i,n,lines;

    module.name=namecopy("m");module.path=namecopy("fixture.dis");
    link.m=&module;link.MP=H2D(void*,&fixtureheap);link.prog=program;
    root=calloc(1,sizeof(Stkext)+64);assert(root);root->reg.TR=&type;
    for(a=0;a<8;a++)for(b=0;b<8;b++)for(mask=0;mask<8;mask++)for(metrics=0;metrics<3;metrics++){
        frames[0]=first.bytes+16+a;frames[1]=second.bytes+16+b;frames[2]=root->reg.tos.fu;
        size=(uvlong)hmsize(&fixtureheap)+msize(program)+msize(root);
        for(i=0;i<3;i++){
            memset(&header,0,sizeof(header));header.fp=i==2 ? nil : frames[i+1];
            header.lr=i==2 ? nil : program+2;header.t=i==2 ? nil : &type;
            header.mr=mask&(1<<i) ? &link : nil;
            if(header.mr)size+=(uvlong)hmsize(&fixtureheap)+msize(program);
            memmove(frames[i],&header,sizeof(header));
        }
        memset(&process,0,sizeof(process));process.R.M=&link;process.R.FP=frames[0];process.R.PC=program+3;
        if(walkmode!=1)assert(calldepth(&process.R)==3);
        assert(progsize(&process)==(size/1024>0x7fffffff ? 0x7fffffff : (int)(size/1024)));
        if(walkmode==1){checks++;continue;}
        n=progstack(&process.R,Pready,output,sizeof(output),0);assert(n>0 && n<sizeof(output));
        lines=0;for(i=0;i<n;i++)if(output[i]=='\n')lines++;
        assert(lines==3 && strstr(output,"fixture.dis")!=nil);
        assert(progstack((REG*)1,Pready,(char*)1,0,0)==0);
        assert(progstack((REG*)1,Pready,(char*)1,-1,0)==0);
        checks++;
    }
    free(root);free(module.name);free(module.path);
}

/* Keep independent failure-reproduction families selectable. */
int
main(int argc,char **argv)
{
    if(argc>1 && !strcmp(argv[1],"statuswide")){wideonly=1;statuses();}
    if(argc>1 && !strcmp(argv[1],"size")){walkmode=1;walks();}
    if(argc==1 || !strcmp(argv[1],"status"))statuses();
    if(argc==1 || !strcmp(argv[1],"walk"))walks();
    assert(checks>0);printf("PASS process diagnostics: %d bounds/alignment/size checks WORD%zu\n",checks,sizeof(WORD)*8);
    return 0;
}
