#include "typed-body-native64.h"
static void typedrequire(int ok,int line){if(!ok)fprintf(stderr,"typed calls assertion at line %d\n",line);require(ok);}
#define require(c) typedrequire(!!(c),__LINE__)

/* Checked source bodies execute in actual stack frames; caller setup is trusted C. */
static void
typedcall(int kind,int jit,int grow,int nested,int quanta,int trial)
{
    const Native64TypedStep *bodies[]={typed_count,typed_identity,typed_wide,typed_fraction,typed_octet,typed_items};
    unsigned int counts[]={nelem(typed_count),nelem(typed_identity),nelem(typed_wide),nelem(typed_fraction),nelem(typed_octet),nelem(typed_items)};
    unsigned int sizes[]={typed_count_frame_bytes,typed_identity_frame_bytes,typed_wide_frame_bytes,typed_fraction_frame_bytes,typed_octet_frame_bytes,typed_items_frame_bytes};
    const uchar *maps[]={typed_count_frame_map,typed_identity_frame_map,typed_wide_frame_map,typed_fraction_frame_map,typed_octet_frame_map,typed_items_frame_map};
    unsigned int args[]={typed_count_args[0],typed_identity_args[0],typed_wide_args[0],typed_fraction_args[0],typed_octet_args[0],typed_items_args[0]};
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
    m->ntype=5;m->type=mallocz(5*sizeof(Type*),1);require(m->type!=nil);
    rootmap[1]=ref?0xc0:0; /* Caller argument64 and result72. */
    m->type[0]=dtype(freeheap,8,nil,0);
    m->type[1]=dtype(freeheap,104,rootmap,2);
    m->type[2]=dtype(freeheap,sizes[kind],(uchar*)maps[kind],(sizes[kind]+63)/64);
    m->type[3]=dtype(freeheap,8,childmap,1);
    m->type[4]=dtype(freeheap,8,nil,0);
    for(i=0;i<5;i++)require(m->type[i]!=nil);
    m->entryt=m->type[1];m->ss=grow?0:4096;
    m->nprog=12+counts[kind];m->prog=mallocz(m->nprog*sizeof(Inst),1);require(m->prog!=nil);p=m->prog;
    callinstruction(p,IFRAME,AIMM,AFP,2,40);
    callargument(p+1,ops[kind],AFP,64,40,args[kind]);
    callargument(p+2,IMOVL,AFP,48,40,32); /* Eight native address bytes, never Dis LEA. */
    callinstruction(p+3,ICALL,AFP,AIMM,40,0);p[3].d.ins=p+(nested?7:12);
    callinstruction(p+4,IEXIT,AXXX,AXXX,0,0);
    callinstruction(p+5,IEXIT,AXXX,AXXX,0,0);callinstruction(p+6,IEXIT,AXXX,AXXX,0,0);
    callinstruction(p+7,IFRAME,AIMM,AFP,2,40);
    callargument(p+8,ops[kind],AFP,args[kind],40,args[kind]);
    callargument(p+9,IMOVL,AFP,32,40,32); /* Forward the original caller result address. */
    callinstruction(p+10,ICALL,AFP,AIMM,40,0);p[10].d.ins=p+12;
    callinstruction(p+11,IRET,AXXX,AXXX,0,0);
    require(native64typedcode(p+12,counts[kind],bodies[kind],counts[kind],sizes[kind],typed_count_data_bytes));
    m->entry=p;m->origmp=H2D(uchar*,heapz(m->type[0]));
    if(jit)require(compile(m,m->nprog,nil));
    memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
    process.R.M=link;process.R.PC=m->entry;
    entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(m->entryt,entry);source=entry;R.s=&source;
    newstack(&process);free(entry);root=process.R.FP;
    if(ref && !trial){
        child=H2D(void*,heapz(m->type[4]));owner=H2D(void*,heapz(m->type[3]));memmove(owner,&child,sizeof(child));
        memmove(root+64,&owner,sizeof(owner));
        if(kind==5){D2H(owner)->ref++;memmove(root+72,&owner,sizeof(owner));} /* Same-object replacement. */
        else{observed=H2D(void*,heapz(m->type[4]));memmove(root+72,&observed,sizeof(observed));}
    }else if(ref){ /* nil must retire an old nonnil result. */
        observed=H2D(void*,heapz(m->type[4]));memmove(root+72,&observed,sizeof(observed));
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

static void
typedencodingtests(void)
{
    Native64TypedStep s[256];Inst out[256];uchar before[sizeof(out)];unsigned int i,j;
    require(nelem(typed_count)<nelem(s));
    for(i=0;i<8;i++){
        memset(out,0xa5,sizeof(out));memmove(before,out,sizeof(out));memmove(s,typed_count,sizeof(typed_count));
        if(i==0)s[0].op=IFRAME;
        if(i==1)s[0].mode=256;
        if(i==2)s[0].m=1;
        if(i==3){for(j=0;j<nelem(typed_count);j++)if(native64typedbranch(s[j].op)){s[j].d=nelem(typed_count);break;}require(j<nelem(typed_count));}
        if(i==4){s[0].mode=SRC(AFP|AIND)|DST(AFP);s[0].s=64;}
        if(i==5){s[0].mode=SRC(AFP)|DST(AFP);s[0].s=1048576;}
        if(i==6){s[0].mode=(s[0].mode&~ARM)|AXIMM;s[0].m=32768;}
        if(i==7){s[0].mode=(s[0].mode&~ARM)|AXINF;s[0].m=65536;}
        if(native64typedcode(out,nelem(out),s,nelem(typed_count),typed_count_frame_bytes,typed_count_data_bytes)){fprintf(stderr,"typed encoding refusal removed: case=%u\n",i);exit(10);}
        require(!memcmp(before,out,sizeof(out)));
    }
    memset(out,0xa5,sizeof(out));memmove(before,out,sizeof(out));
    require(!native64typedcode(out,0,typed_count,nelem(typed_count),typed_count_frame_bytes,typed_count_data_bytes));
    require(!native64typedcode(out,nelem(out),nil,nelem(typed_count),typed_count_frame_bytes,typed_count_data_bytes));
    require(!native64typedcode(out,nelem(out),typed_count,0,typed_count_frame_bytes,typed_count_data_bytes));
    require(!native64typedcode(out,nelem(out),typed_count,nelem(typed_count),63,typed_count_data_bytes));
    require(!native64typedcode(out,nelem(out),typed_count,nelem(typed_count),typed_count_frame_bytes,3));
    require(!memcmp(before,out,sizeof(out)));
    printf("PASS typed native materialization: 13 encoding/branch/geometry refusals publish no partial code\n");
}

int main(void)
{
    int kind,jit,grow,nested,q,trial,cases=0;int quanta[]={1,3,1000};
    optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMOVP]=movp;optab[IMOVB]=movb;
    optab[IMOVW]=movw;optab[IMOVL]=movl;optab[IMOVF]=movf;optab[IADDW]=addw;optab[ISUBW]=subw;
    optab[IJMP]=jmp;optab[IBEQW]=beqw;optab[IBNEW]=bnew;optab[IBLTW]=bltw;optab[IBLEW]=blew;optab[IBGTW]=bgtw;optab[IBGEW]=bgew;optab[IEXIT]=iexit;
    typedencodingtests();
    for(kind=0;kind<6;kind++)for(jit=0;jit<2;jit++)for(grow=0;grow<2;grow++)for(nested=0;nested<2;nested++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        typedcall(kind,jit,grow,nested,quanta[q],trial);cases++;
    }
    printf("PASS source-derived typed local calls: %d C/RX, active frames, nested calls, extent growth, quanta, scalar widths and managed ownership cases WORD32\n",cases);
    return 0;
}
