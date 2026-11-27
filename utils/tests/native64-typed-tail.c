#include "typed-tail-native64.h"
static void typedrequire(int ok,int line){if(!ok)fprintf(stderr,"typed calls assertion at line %d\n",line);require(ok);}
#define require(c) typedrequire(!!(c),__LINE__)

/* Checked source bodies execute in actual stack frames; caller setup is trusted C. */
static void
typedcall(int kind,int jit,int grow,int nested,int quanta,int trial)
{
    int ops[]={IMOVW,IMOVP,IMOVL,IMOVF,IMOVB,IMOVP};
    unsigned int width=kind==0?4:kind==4?1:8,ref=kind==1 || kind==5;
    uchar rootmap[]={0,0},childmap[]={0x80};
    uchar *root;Module *m;Modlink *link;Frame *entry,*source;Prog process;
    Inst *p;void *owner=H,*child=H,*result,*observed;
    LONG big=trial?(-0x7fffffffffffffffLL-1):0x7fffffffffffffffLL,gotbig;
    REAL real=trial?-1.25:1.0e100,gotreal;WORD n=trial?-3:7,word;
    BYTE byte=trial?255:128;uchar before[24];int i,done,initial;
    uvlong canary=0xfedc123456789abcULL,after;

    require(sizeof(WORD)==4 && sizeof(void*)==8);
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;
    m->name=strdup("source-typed-call");m->path=strdup("source-typed-call");require(m->name && m->path);
    m->ntype=nelem(tail_bodies)+4;m->type=mallocz(m->ntype*sizeof(Type*),1);require(m->type!=nil);
    rootmap[1]=ref?0xc0:0; /* Caller argument64 and result72. */
    m->type[0]=dtype(freeheap,8,nil,0);
    m->type[1]=dtype(freeheap,104,rootmap,2);
    for(i=0;i<(int)nelem(tail_bodies);i++)m->type[i+2]=dtype(freeheap,tail_bodies[i].bytes,(uchar*)tail_bodies[i].map,(tail_bodies[i].bytes+63)/64);
    m->type[m->ntype-2]=dtype(freeheap,8,childmap,1);m->type[m->ntype-1]=dtype(freeheap,8,nil,0);
    for(i=0;i<m->ntype;i++)require(m->type[i]!=nil);
    m->entryt=m->type[1];m->ss=grow?0:4096;
    { unsigned int offsets[32],at=6,j;int body=tail_entries[kind][nested];
      require(nelem(tail_bodies)<nelem(offsets));
      for(j=0;j<nelem(tail_bodies);j++){offsets[j]=at;at+=tail_bodies[j].count;}
      m->nprog=at;m->prog=mallocz(at*sizeof(Inst),1);require(m->prog!=nil);p=m->prog;
      callinstruction(p,IFRAME,AIMM,AFP,body+2,40);
      callargument(p+1,ops[kind],AFP,64,40,64);
      callargument(p+2,IMOVL,AFP,48,40,32);
      callinstruction(p+3,ICALL,AFP,AIMM,40,0);p[3].d.ins=p+offsets[body];
      callinstruction(p+4,IEXIT,AXXX,AXXX,0,0);callinstruction(p+5,IEXIT,AXXX,AXXX,0,0);
      for(j=0;j<nelem(tail_bodies);j++){
        const TailBody *b=tail_bodies+j;
        if(b->target<0)require(native64typedcode(p+offsets[j],b->count,b->code,b->count,b->bytes,0));
        else require(native64typedtailcode(p+offsets[j],b->count,b->code,b->count,b->bytes,0,m->type[b->target+2],p+offsets[b->target],b->target+2));
      }
    }
    m->entry=p;m->origmp=H2D(uchar*,heapz(m->type[0]));
    if(jit)require(compile(m,m->nprog,nil));
    memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
    process.R.M=link;process.R.PC=m->entry;
    entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(m->entryt,entry);source=entry;R.s=&source;
    newstack(&process);free(entry);root=process.R.FP;
    if(ref && !trial){
        child=H2D(void*,heapz(m->type[m->ntype-1]));owner=H2D(void*,heapz(m->type[m->ntype-2]));memmove(owner,&child,sizeof(child));
        memmove(root+64,&owner,sizeof(owner));
        if(kind==5){D2H(owner)->ref++;memmove(root+72,&owner,sizeof(owner));} /* Same-object replacement. */
        else{observed=H2D(void*,heapz(m->type[m->ntype-1]));memmove(root+72,&observed,sizeof(observed));}
    }else if(ref){ /* nil must retire an old nonnil result. */
        observed=H2D(void*,heapz(m->type[m->ntype-1]));memmove(root+72,&observed,sizeof(observed));
    }else if(kind==0)memmove(root+64,&n,4);
    else if(kind==2)memmove(root+64,&big,8);
    else if(kind==3)memmove(root+64,&real,8);
    else memmove(root+64,&byte,1);
    memset(root+72+(ref?8:width),0x5a,8-(ref?8:width));
    memmove(root+80,&canary,8);memset(root+88,0x3c,16);memmove(before,root+80,24);
    result=root+72;memmove(root+48,&result,sizeof(result));
    process.quanta=quanta;initial=raised;message[0]='x';
    for(i=0;i<1000;i++){done=kprotect(execute,&process);if(!done)break;}
    if(i>=1000 || raised!=initial+1 || worker.nerr || message[0])fprintf(stderr,"case kind=%d jit=%d grow=%d nested=%d q=%d trial=%d steps=%d err=%s\n",kind,jit,grow,nested,quanta,trial,i,message);
    require(i<1000 && raised==initial+1 && worker.nerr==0 && message[0]==0);
    if(ref){memmove(&observed,result,sizeof(observed));require(observed==owner);if(owner!=H)require(D2H(owner)->ref==2 && D2H(child)->ref==1);}
    else if(kind==0){memmove(&word,result,4);require(word==(n>0?n*(n+1)/2:0));}
    else if(kind==2){memmove(&gotbig,result,8);require(gotbig==big);}
    else if(kind==3){memmove(&gotreal,result,8);require(gotreal==real);}
    else require(*(BYTE*)result==byte);
    for(i=width;i<8;i++)require(((uchar*)result)[i]==0x5a);
    memmove(&after,root+80,8);require(after==canary && !memcmp(before,root+80,24));
    require(R.FP==root);destroystack(&R);require(m->ref==1);unload(m);require(live==0);
}

static void tailencodingtests(void)
{
    Native64TypedStep s[16];Inst out[16],target;uchar before[sizeof(out)];Type callee={0};unsigned int i;
    callee.size=72;require(nelem(tail_relay_count)==5);
    for(i=0;i<8;i++){
        memmove(s,tail_relay_count,sizeof(tail_relay_count));memset(out,0xa5,sizeof(out));memmove(before,out,sizeof(out));
        if(i==0)s[0].d=48;
        if(i==1)s[1].d=72;
        if(i==2)s[2].s=16;
        if(i==3)s[2].returned=8;
        if(i==4)s[3].s=48;
        if(i==5)s[4].op=ICALL;
        if(i==6)s[1].mode=SRC(AFP|AIND)|DST(AFP|AIND);
        if(i==7)s[1].s=1048576;
        if(native64typedtailcode(out,nelem(out),s,5,72,0,&callee,&target,2)){
            fprintf(stderr,"forwarding native refusal removed: case=%u\n",i);exit(10);
        }
        require(!memcmp(before,out,sizeof(out)));
    }
    printf("PASS forwarding native code: eight structural refusals preserve all output bytes\n");
}

int main(void)
{
    int kind,jit,grow,nested,q,trial,cases=0;int quanta[]={1,3,1000};
    optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMOVP]=movp;optab[IMOVB]=movb;
    optab[IMOVW]=movw;optab[IMOVL]=movl;optab[IMOVF]=movf;optab[IADDW]=addw;optab[ISUBW]=subw;
    optab[IJMP]=jmp;optab[IBEQW]=beqw;optab[IBNEW]=bnew;optab[IBLTW]=bltw;optab[IBLEW]=blew;optab[IBGTW]=bgtw;optab[IBGEW]=bgew;optab[IEXIT]=iexit;

    tailencodingtests();
    for(kind=0;kind<6;kind++)for(jit=0;jit<2;jit++)for(grow=0;grow<2;grow++)for(nested=0;nested<2;nested++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        typedcall(kind,jit,grow,nested,quanta[q],trial);cases++;
    }
    printf("PASS source-derived forwarding chains: %d C/RX, active frames, nested calls, extent growth, quanta, scalar widths and managed ownership cases WORD32\n",cases);
    return 0;
}
