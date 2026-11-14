#include "typed-multi-native64.h"
static void typedrequire(int ok,int line){if(!ok)fprintf(stderr,"typed calls assertion at line %d\n",line);require(ok);}
#define require(c) typedrequire(!!(c),__LINE__)

static int bindprobe(void *unused){USED(unused);native64typedlea8step();return 1;}
static int decodedprobe(void *unused){USED(unused);cjitexec(&R,ILEA,nil);return 1;}
static int probecount;
/* Faults are restored before execution/ownership cleanup; guard failure must
 * precede all pointer/result/PC publication and perform no allocation. */
static void bindingprobes(void)
{
    REG saved=R;Inst *pc=R.PC,original=*pc;Frame *pending;Type *caller,*callee;
    void *oldpending;uchar pointer[8],result[8];unsigned int offset,bit;int i,before,oldref,oldsize;
    caller=((Frame*)R.FP)->t;if(caller==nil)caller=SEXTYPE((Frame*)R.FP)->reg.TR;
    memmove(&pending,R.FP+40,sizeof(pending));callee=pending->t;if(callee==nil)callee=((Stkext*)R.EX)->reg.TR;
    memmove(pointer,(uchar*)pending+32,8);offset=original.s.ind;memmove(result,R.FP+offset,8);
    oldsize=callee->size;oldref=callee->ref;bit=offset/8;before=allocations;
    for(i=0;i<12;i++){
        if(i==0)pc->reg=2;
        if(i==1)pc->reg^=16;
        if(i==2)pc->s.ind=0;
        if(i==3)pc->s.ind=caller->size;
        if(i==4)pc->s.ind=-1;
        if(i==5)pc->d.i.f=48;
        if(i==6)pc->d.i.s=40;
        if(i==7){oldpending=R.FP;memmove(R.FP+40,&oldpending,sizeof(oldpending));}
        if(i==8)callee->size-=8;
        if(i==9)caller->map[bit/8]^=0x80>>(bit%8);
        if(i==10)caller->map[0]^=1;
        if(i==11)callee->ref=0;
        if(kprotect(bindprobe,nil)!=0){fprintf(stderr,"local binding guard removed: case=%d\n",i);exit(10);}
        require(!strcmp(message,exBounds) && R.PC==saved.PC && !memcmp(pointer,(uchar*)pending+32,8) && !memcmp(result,R.FP+offset,8) && allocations==before);
        *pc=original;memmove(R.FP+40,&pending,sizeof(pending));callee->size=oldsize;callee->ref=oldref;
        if(i==9)caller->map[bit/8]^=0x80>>(bit%8);
        if(i==10)caller->map[0]^=1;
        R=saved;probecount++;
    }
    saved.M->m->cstep=nil;
    require(native64typedlea8step()==0 && R.PC==saved.PC && !memcmp(pointer,(uchar*)pending+32,8));
    saved.M->m->cstep=native64typedlea8step;
    require(kprotect(decodedprobe,nil)==0 && !strcmp(message,exCompile) && !memcmp(pointer,(uchar*)pending+32,8));
    R=saved;
}

/* Checked source bodies execute in actual stack frames; caller setup is trusted C. */
static void
typedcall(int kind,int jit,int grow,int nested,int quanta,int trial,int failure)
{
    int ops[]={IMOVW,IMOVP,IMOVL,IMOVF,IMOVB,IMOVP,IMOVW,IMOVW};
    unsigned int width=(kind==0 || kind==6 || kind==7)?4:kind==4?1:8,ref=kind==1 || kind==5;
    uchar rootmap[]={0,0},childmap[]={0x80};
    uchar *root;Module *m;Modlink *link;Frame *entry,*source;Prog process;
    Inst *p;void *owner=H,*child=H,*result,*observed;
    LONG big=trial?(-0x7fffffffffffffffLL-1):0x7fffffffffffffffLL,gotbig;
    REAL real=trial?-1.25:1.0e100,gotreal;WORD n=trial?-3:7,word;
    BYTE byte=trial?255:128;uchar before[24],oldresult[8];int i,done,initial,probed=0;
    uvlong canary=0xfedc123456789abcULL,after;

    require(sizeof(WORD)==4 && sizeof(void*)==8);
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;
    m->name=strdup("source-typed-call");m->path=strdup("source-typed-call");require(m->name && m->path);
    m->ntype=nelem(tail_bodies)+4;m->type=mallocz(m->ntype*sizeof(Type*),1);require(m->type!=nil);
    rootmap[1]=ref?0xc0:0; /* Caller argument64 and result72. */
    m->type[0]=dtype(freeheap,multi_data_bytes,(uchar*)multi_data_map,(multi_data_bytes+63)/64);
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
        const TailBody *b=tail_bodies+j;Native64TypedCall calls[32];unsigned int k;
        require(b->nsites<=nelem(calls));
        for(k=0;k<b->nsites;k++){
            const MultiSite *site=b->sites+k;
            calls[k]=(Native64TypedCall){site->first,site->end,m->type[site->target+2],p+offsets[site->target],site->target+2,site->contract,site->inherited};
        }
        require(native64typedmulticode(p+offsets[j],b->count,b->code,b->count,b->bytes,multi_data_bytes,calls,b->nsites));
      }
      if(failure)callinstruction(p+offsets[body]+tail_bodies[body].sites[tail_bodies[body].nsites-1].end+1,IDIVW,AIMM,AFP,0,56);
    }
    m->cstep=native64typedlea8step;m->entry=p;m->origmp=H2D(uchar*,heapz(m->type[0]));
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
    }else if(kind==0 || kind==6 || kind==7)memmove(root+64,&n,4);
    else if(kind==2)memmove(root+64,&big,8);
    else if(kind==3)memmove(root+64,&real,8);
    else memmove(root+64,&byte,1);
    if(kind==6)memset(root+72,0x6b,4);
    memset(root+72+(ref?8:width),0x5a,8-(ref?8:width));
    memmove(root+80,&canary,8);memset(root+88,0x3c,16);memmove(before,root+80,24);
    result=root+72;memmove(root+48,&result,sizeof(result));
    memmove(oldresult,result,8);process.quanta=quanta;initial=raised;message[0]='x';
    for(i=0;i<10000;i++){
        if(quanta==1 && !probed && process.R.PC->op==ILEA){R=process.R;bindingprobes();probed=1;initial=raised;message[0]='x';}
        done=kprotect(execute,&process);if(!done)break;
    }
    if(i>=10000 || raised!=initial+1 || worker.nerr || (failure?strcmp(message,exZdiv):message[0]))fprintf(stderr,"case kind=%d jit=%d grow=%d nested=%d q=%d trial=%d steps=%d err=%s\n",kind,jit,grow,nested,quanta,trial,i,message);
    require(i<10000 && raised==initial+1 && worker.nerr==0 && (failure?!strcmp(message,exZdiv):message[0]==0));
    if(failure){
        require(!memcmp(result,oldresult,8));
        if(owner!=H){
            Type *t=((Frame*)R.FP)->t;unsigned int at,refs=1+(kind==5);void *value;
            if(t==nil)t=SEXTYPE((Frame*)R.FP)->reg.TR;
            for(at=64;at<(unsigned int)t->size;at+=8)if(at/64<(unsigned int)t->np && (t->map[at/64]&(0x80>>(at/8%8)))){
                memmove(&value,R.FP+at,8);if(value==owner)refs++;
            }
            require(D2H(owner)->ref==refs && D2H(child)->ref==1);
        }
    }else if(ref){memmove(&observed,result,sizeof(observed));require(observed==owner);if(owner!=H)require(D2H(owner)->ref==2 && D2H(child)->ref==1);}
    else if(kind==0){memmove(&word,result,4);require(word==((n+1)>0?(n+1)*(n+2)/2:0)+(n>0?n*(n+1)/2:0)+(nested && n>0?n*(n+1)*(n+2)/6:0));}
    else if(kind==7){WORD expected=n>0?n*(n+1)/2:0;expected=expected*(expected+1)/2;if(nested)expected=expected*(expected+1)/2;memmove(&word,result,4);require(word==expected);}
    else if(kind==6){memmove(&word,m->origmp+multi_out,4);require(word==n+(nested?2:0) && !memcmp(result,oldresult,8));}
    else if(kind==2){memmove(&gotbig,result,8);require(gotbig==big);}
    else if(kind==3){memmove(&gotreal,result,8);require(gotreal==real);}
    else require(*(BYTE*)result==byte);
    for(i=width;i<8;i++)require(((uchar*)result)[i]==0x5a);
    memmove(&after,root+80,8);require(after==canary && !memcmp(before,root+80,24));
    if(!failure)require(R.FP==root);destroystack(&R);require(m->ref==1);unload(m);require(live==0);
}

static void multiencodingtests(void)
{
    Native64TypedStep steps[64];Native64TypedCall calls[2];Inst out[64],target;Type callee={0};
    uchar before[sizeof(out)];unsigned int i,k,n=nelem(tail_relay_count),ns;
    callee.size=72;require(n<nelem(steps) && nelem(sites_relay_count)==2);
    for(i=0;i<12;i++){
        unsigned int fp=88;ns=2;memmove(steps,tail_relay_count,sizeof(tail_relay_count));
        for(k=0;k<2;k++)calls[k]=(Native64TypedCall){sites_relay_count[k].first,sites_relay_count[k].end,&callee,&target,1,4,0};
        memset(out,0xa5,sizeof(out));memmove(before,out,sizeof(out));
        if(i==0)calls[1].first=calls[0].first;
        if(i==1)calls[1].end=n-1;
        if(i==2)steps[calls[0].first+1].d=72;
        if(i==3)steps[calls[0].end-1].s=0;
        if(i==4)steps[0]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,calls[0].first+1,0};
        if(i==5)steps[0]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,n,0};
        if(i==6)ns=1;
        if(i==7)steps[calls[0].first+1].returned=0;
        if(i==8)calls[0].contract^=16;
        if(i==9)calls[0].ordinal=~0U;
        if(i==10)calls[0].inherited=1;
        if(i==11)fp=64;
        if(native64typedmulticode(out,nelem(out),steps,n,fp,multi_data_bytes,calls,ns)){
            fprintf(stderr,"multi native guard removed: case=%u\n",i);exit(10);
        }
        require(!memcmp(before,out,sizeof(out)));
    }
    printf("PASS multi native code: 12 site/argument/branch/refusal cases preserve all output bytes\n");
}

int main(void)
{
    int kind,jit,grow,nested,q,trial,cases=0;int quanta[]={1,3,1000};
    optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMOVP]=movp;optab[IMOVB]=movb;
    optab[IMOVW]=movw;optab[IMOVL]=movl;optab[IMOVF]=movf;optab[IADDW]=addw;optab[ISUBW]=subw;
    optab[IJMP]=jmp;optab[IBEQW]=beqw;optab[IBNEW]=bnew;optab[IBLTW]=bltw;optab[IBLEW]=blew;optab[IBGTW]=bgtw;optab[IBGEW]=bgew;optab[IEXIT]=iexit;optab[IDIVW]=divw;



    multiencodingtests();
    for(kind=0;kind<8;kind++)for(jit=0;jit<2;jit++)for(grow=0;grow<2;grow++)for(nested=0;nested<2;nested++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        typedcall(kind,jit,grow,nested,quanta[q],trial,0);cases++;
    }
    for(kind=1;kind<=5;kind+=4)for(jit=0;jit<2;jit++)for(grow=0;grow<2;grow++)for(nested=0;nested<2;nested++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        typedcall(kind,jit,grow,nested,quanta[q],trial,1);cases++;
    }
    printf("PASS source multiple-call bodies: %d C/RX, active frames, nested calls, extent growth, quanta, scalar widths and managed ownership cases (384 source successes + 96 private continuation faults) WORD32\n",cases);
    printf("PASS local binding runtime: %d prepublication guard refusals, wrong-owner and decoded-JIT refusal with untouched pointers/results/PC and no allocations\n",probecount);
    return 0;
}
