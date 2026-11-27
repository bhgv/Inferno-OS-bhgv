#include "typed-tables.h"
static void tabrequire(int ok,int line){if(!ok){fprintf(stderr,"typed table predicate line=%d\n",line);exit(10);}}
#define require(c) tabrequire(!!(c),__LINE__)
static unsigned tablecalls,totalc;
static void observeicase(void){tablecalls++;icase();}
static void observecasel(void){tablecalls++;casel();}
static void observeigoto(void){tablecalls++;igoto();}

static WORD expected(unsigned kind,vlong value)
{
    if(kind==0)return (WORD)value+41;
    if(kind==1){if(value>=-100 && value<=-90)return 11;if(value==5)return 22;if(value>=1000 && value<=1010)return 33;if(value==2147483647)return 44;return 55;}
    if(kind==2){if(value==-1099511627776LL)return 66;if(value==1099511627776LL)return 77;if(value==9223372036854775807LL)return 88;return 99;}
    return value>=-2 && value<=5?103+(WORD)value:109;
}

static void run(unsigned kind,vlong value,int pjit,int cjit,int grow,int quantum)
{
    Native64ModuleOwner owner={0};Module *m,*pm;Modlink *callee,*link;Import request={0};
    Prog process;Frame *entry,*source;uchar *root;uchar map[]={0,0x80};void *result;
    unsigned i,offset=0;int done,initialraise;WORD input=value,got;uvlong guard=0xfedcba9876543210ULL,after;
    require(native64typedmodule(&spec,pjit,&owner));pm=owner.module;
    /* Each target must use the new module offset, never the classic file PC. */
    for(i=0;i<=kind;i++)offset+=functions[i].count;
    require(offset==legacy_first[kind]+1);
    pm->ref++;callee=mklinkmod(pm,1);callee->MP=H2D(uchar*,heapz(pm->type[0]));newmp(callee->MP,pm->origmp,pm->type[0]);
    request.name=(char*)functions[kind+1].name;memmove(&request.sig,&functions[kind+1].signature,4);
    require(linkm(pm,callee,0,&request)==0 && callee->links[0].u.pc==pm->prog+offset);
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;
    m->name=strdup("CaseCaller");m->path=strdup("trusted/CaseCaller");m->ss=grow?0:4096;
    m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->type!=nil);
    m->type[0]=dtype(freeheap,8,nil,0);m->type[1]=dtype(freeheap,112,map,2);require(m->type[0] && m->type[1]);m->entryt=m->type[1];
    m->origmp=H2D(uchar*,heapz(m->type[0]));m->nprog=5;m->prog=mallocz(5*sizeof(Inst),1);require(m->prog!=nil);m->entry=m->prog;m->cstep=native64typedlea8step;
    callinstruction(m->prog,IMFRAME,AFP,AFP,64,40);m->prog[0].add|=AXIMM;
    callargument(m->prog+1,kind==2?IMOVL:IMOVW,AFP,72,40,args[kind]);
    callargument(m->prog+2,ILEA,AFP,80,40,32);m->prog[2].reg=4;
    callinstruction(m->prog+3,IMCALL,AFP,AFP,40,64);m->prog[3].add|=AXIMM;
    callinstruction(m->prog+4,IEXIT,AXXX,AXXX,0,0);
    if(cjit)require(compile(m,m->nprog,nil));
    memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
    process.R.M=link;process.R.PC=m->entry;
    entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(entry->t,entry);source=entry;R.s=&source;newstack(&process);free(entry);
    root=process.R.FP;memmove(root+64,&callee,8);if(kind==2)memmove(root+72,&value,8);else memmove(root+72,&input,4);
    memmove(root+88,&guard,8);memset(root+80,0x5a,8);result=root+80;memmove(root+48,&result,8);
    initialraise=raised;message[0]='x';process.quanta=quantum;tablecalls=0;
    for(i=0;i<10000;i++){done=kprotect(execute,&process);if(!done)break;}
    if(i>=10000 || raised!=initialraise+1 || message[0])fprintf(stderr,"table run kind=%u value=%lld jit=%d/%d grow=%d quantum=%d error=%s\n",kind,value,pjit,cjit,grow,quantum,message);
    require(i<10000 && raised==initialraise+1 && !message[0] && !worker.nerr && R.FP==root);
    if(pjit)require(!tablecalls);else totalc+=tablecalls;
    memmove(&got,result,4);require(got==expected(kind,value));memmove(&after,root+88,8);require(after==guard);
    for(i=4;i<8;i++)require(root[80+i]==0x5a);
    destroystack(&R);require(m->ref==1);unload(m);require(pm->ref==1 && native64typedmoduleclear(&owner));require(!live && !rootcount && Tmodlink.ref==1);
}

static void construction(void)
{
    Native64ModuleOwner owner={0};int jit,before,attempts,i;REG saved=R;
    for(jit=0;jit<2;jit++){
        before=allocations;require(native64typedmodule(&spec,jit,&owner));attempts=allocations-before;
        require(native64typedmoduleclear(&owner) && !live && !rootcount);
        for(i=1;i<=attempts;i++){
            failat=allocations+i;require(!native64typedmodule(&spec,jit,&owner));failat=0;
            require(!owner.module && !owner.rooted && !owner.self && !live && !rootcount && !memcmp(&saved,&R,sizeof(R)));
        }
        printf("PASS table constructor: %d protected allocation refusals jit=%d\n",attempts,jit);
    }
}

static void refusals(void)
{
    Native64ModuleSpec s;Native64ModuleFunction fn[5];Native64ModuleCase tables[8];Native64ModuleCaseRow rows[16];
    Native64ModuleOwner owner={0};Native64ModuleInit conflict;Native64TypedStep code[256];Native64ModuleSite site;
    uchar map[256];unsigned n;int before;
    require(functions[2].ncases && functions[2].ncases<nelem(tables) && functions[2].count<nelem(code));
    for(n=0;n<20;n++){
        s=spec;memmove(fn,functions,sizeof(fn));s.functions=fn;memmove(tables,fn[2].cases,fn[2].ncases*sizeof(*tables));fn[2].cases=tables;
        require(tables[0].count<nelem(rows));memmove(rows,tables[0].rows,tables[0].count*sizeof(*rows));tables[0].rows=rows;
        memmove(code,fn[2].code,fn[2].count*sizeof(*code));fn[2].code=code;
        if(n==0)rows[0].target=fn[2].count;
        if(n==1)tables[0].fallback=fn[2].count;
        if(n==2)tables[0].offset=s.databytes;
        if(n==3)tables[0].offset++;
        if(n==4)rows[0].low=rows[0].high+1;
        if(n==5)rows[1].low=rows[0].high;
        if(n==6)fn[2].ncases=0;
        if(n==7)tables[0].pc=fn[2].count;
        if(n==8)tables[0].count=4097;
        if(n==9){conflict=(Native64ModuleInit){tables[0].offset,4,0};s.initial=&conflict;s.ninitial=1;}
        if(n==10)code[tables[0].pc].d+=4;
        if(n==11){fn[2].ncases=2;tables[1]=tables[0];tables[1].pc++;}
        if(n==12){site=(Native64ModuleSite){0,rows[0].target,0,0,0};fn[2].sites=&site;fn[2].nsites=1;}
        if(n==13){site=(Native64ModuleSite){tables[0].fallback-1,tables[0].fallback,0,0,0};fn[2].sites=&site;fn[2].nsites=1;}
        if(n==14){memmove(map,s.datamap,s.mapbytes);map[tables[0].offset/64]|=0x80>>((tables[0].offset/8)%8);s.datamap=map;}
        if(n==15){fn[2].ncases=2;tables[1]=tables[0];tables[1].pc++;tables[1].offset+=4;code[tables[1].pc]=code[tables[0].pc];code[tables[1].pc].d+=4;}
        if(n==16)tables[0].rows=H;
        if(n==17)tables[0].op=ICASEC;
        if(n==18)tables[0].fallback=~0U;
        if(n==19)fn[2].ncases=257;
        before=allocations;
        if(native64typedmodule(&s,0,&owner)){fprintf(stderr,"table metadata refusal removed: case=%u\n",n);exit(10);}
        require(allocations==before && !owner.module && !owner.rooted && !live);
    }
    printf("PASS table metadata: %u preallocation refusals and unchanged ownership\n",n);
}

int main(void)
{
    static const vlong values[]={-2147483647LL-1,-101,-100,-95,-90,-89,-3,-2,-1,0,1,2,3,4,5,6,999,1000,1005,1010,1011,2147483647LL,-1099511627777LL,-1099511627776LL,-1099511627775LL,1099511627775LL,1099511627776LL,1099511627777LL,-9223372036854775807LL-1,9223372036854775807LL};
    unsigned kind,v,p,c,g,q,cases=0;int quantum[]={1,3,1000};
    optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;
    optab[IMOVW]=movw;optab[IMOVL]=movl;optab[IADDW]=addw;optab[ISUBW]=subw;optab[IEXIT]=iexit;
    optab[IBEQW]=beqw;optab[IBLTW]=bltw;optab[IBGTW]=bgtw;optab[IBEQL]=beql;
    optab[ICASE]=observeicase;optab[ICASEL]=observecasel;optab[IGOTO]=observeigoto;
    refusals();construction();
    for(kind=0;kind<4;kind++)for(v=0;v<nelem(values);v++){
        if(kind!=2 && (values[v]<(-2147483647LL-1) || values[v]>2147483647LL))continue;
        /* pad uses ordinary signed addition; avoid an intentional overflow. */
        if(kind==0 && values[v]>2147483606LL)continue;
        for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<nelem(quantum);q++){run(kind,values[v],p,c,g,quantum[q]);cases++;}
    }
    require(totalc>0);
    printf("PASS numeric Limbo tables: %u C/JIT module-call roots, %u C table observations, no JIT table fallback; signed limits, ranges, defaults, shifted function PCs and complete retirement\n",cases,totalc);
    return 0;
}
