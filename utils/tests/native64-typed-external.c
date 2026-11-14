#include "typed-external-native64.h"
static Modlink *externalinstance(Module *m)
{
    Modlink *ml;Import request={0};unsigned i;
    m->ref++;ml=mklinkmod(m,nelem(ext_imports));ml->MP=H2D(uchar*,heapz(m->type[0]));newmp(ml->MP,m->origmp,m->type[0]);
    for(i=0;i<nelem(ext_imports);i++){
        request.name=(char*)ext_imports[i].name;memcpy(&request.sig,&ext_imports[i].signature,4);request.sig^=1;
        if(linkm(m,ml,i,&request)>=0){fprintf(stderr,"module signature guard removed\n");exit(10);}
        memcpy(&request.sig,&ext_imports[i].signature,4);require(linkm(m,ml,i,&request)==0);
    }
    return ml;
}
static WORD triangle(WORD n){return n>0?n*(n+1)/2:0;}
static WORD relaycount(WORD n){return triangle(n+1)+triangle(n);}
static WORD chaincount(WORD n){return relaycount(n)+(n>0?n*(n+1)*(n+2)/6:0);}
static void externalcall(int kind,int providerjit,int callerjit,int grow,int forward,int quantum,int trial,int failure)
{
    Native64ModuleOwner provider={0};const ExternalBody *body=ext_bodies+(kind==8?16:kind*2+forward);Native64TypedCall calls[16];
    Module *m,*pm;Modlink *callee,*idle,*link;Prog process;Frame *entry,*source;uchar *root,map[]={0,0x80},childmap[]={0x80};
    unsigned i,offset;int done,initial;WORD n=trial?-3:kind==7?2:7,word,seed=0x1234,marker=0x55667788;
    LONG big=trial?(-0x7fffffffffffffffLL-1):0x7fffffffffffffffLL,gotbig;REAL real=trial?-1.25:1e100,gotreal;BYTE byte=trial?255:128;
    unsigned width=kind==0 || kind==6 || kind==7 || kind==8?4:kind==4?1:8,ref=kind==1 || kind==5;
    int ops[]={IMOVW,IMOVP,IMOVL,IMOVF,IMOVB,IMOVP,IMOVW,IMOVW,IMOVW};void *owner=H,*child=H,*observed,*result;uchar old[8],guards[24];
    require(native64typedmodule(&module_spec,providerjit,&provider));pm=provider.module;callee=externalinstance(pm);idle=externalinstance(pm);
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->name=strdup("External");m->path=strdup("trusted/External");
    m->ntype=5;m->type=mallocz(5*sizeof(Type*),1);require(m->type!=nil);map[1]=ref?0xe0:0x80;
    m->type[0]=dtype(freeheap,16,nil,0);m->type[1]=dtype(freeheap,112,map,2);m->type[2]=dtype(freeheap,body->bytes,(uchar*)body->map,body->mapbytes);
    m->type[3]=dtype(freeheap,8,childmap,1);m->type[4]=dtype(freeheap,8,nil,0);for(i=0;i<5;i++)require(m->type[i]!=nil);
    m->entryt=m->type[1];m->ss=grow?0:4096;m->origmp=H2D(uchar*,heapz(m->type[0]));memcpy(m->origmp+8,&seed,4);
    m->nprog=7+body->count;m->prog=mallocz(m->nprog*sizeof(Inst),1);require(m->prog!=nil);m->entry=m->prog;m->cstep=native64typedlea8step;
    callinstruction(m->prog,IFRAME,AIMM,AFP,2,40);callargument(m->prog+1,IMOVP,AFP,64,40,64);
    callargument(m->prog+2,ops[kind],AFP,72,40,72);
    callargument(m->prog+3,ILEA,AFP,80,40,32);m->prog[3].reg=width|(ref?16:0);
    callinstruction(m->prog+4,ICALL,AFP,AIMM,40,0);m->prog[4].d.ins=m->prog+7;
    callinstruction(m->prog+5,IMOVW,AIMM,AMP,marker,8);callinstruction(m->prog+6,IEXIT,AXXX,AXXX,0,0);
    require(body->nsites<=nelem(calls));memset(calls,0,sizeof(calls));
    for(i=0;i<body->nsites;i++){
        const ExternalSite *site=body->sites+i;require(site->binding<nelem(ext_imports));
        calls[i]=(Native64TypedCall){site->first,site->end,callee->links[site->binding].frame,nil,site->binding,site->contract,site->inherited,1,site->mode,site->offset};
    }
    require(native64typedmulticode(m->prog+7,body->count,body->code,body->count,body->bytes,16,calls,body->nsites));
    if(failure){offset=7+body->sites[body->nsites-1].end+1;require(offset<(unsigned)m->nprog);callinstruction(m->prog+offset,IDIVW,AIMM,AFP,0,56);}
    if(callerjit)require(compile(m,m->nprog,nil));
    memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
    process.R.M=link;process.R.PC=m->entry;entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(m->entryt,entry);
    source=entry;R.s=&source;newstack(&process);free(entry);root=process.R.FP;memcpy(root+64,&callee,8);
    if(ref && !trial){
        child=H2D(void*,heapz(m->type[4]));owner=H2D(void*,heapz(m->type[3]));memcpy(owner,&child,8);memcpy(root+72,&owner,8);
        observed=H2D(void*,heapz(m->type[4]));memcpy(root+80,&observed,8);
    }else if(ref){observed=H2D(void*,heapz(m->type[4]));memcpy(root+80,&observed,8);}
    else if(kind==0 || kind==6 || kind==7 || kind==8)memcpy(root+72,&n,4);else if(kind==2)memcpy(root+72,&big,8);else if(kind==3)memcpy(root+72,&real,8);else memcpy(root+72,&byte,1);
    if(kind==6)memset(root+80,0x6b,4);memset(root+80+width,0x5a,8-width);memset(root+88,0x3c,24);memcpy(guards,root+88,24);
    result=root+80;memcpy(old,result,8);process.quanta=quantum;initial=raised;message[0]='x';
    for(i=0;i<100000;i++){done=kprotect(execute,&process);if(!done)break;}
    if(i>=100000 || raised!=initial+1 || (failure?strcmp(message,exZdiv):message[0]))fprintf(stderr,"external case %d/%d/%d/%d/%d/%d/%d failure=%d error=%s\n",kind,providerjit,callerjit,grow,forward,quantum,trial,failure,message);
    require(i<100000 && raised==initial+1 && worker.nerr==0 && (failure?!strcmp(message,exZdiv):!message[0]));
    if(failure)require(!memcmp(result,old,8));
    else{
        require(R.M==link && R.MP==link->MP && R.FP==root);
        if(ref){memcpy(&observed,result,8);require(observed==owner);if(owner!=H)require(D2H(owner)->ref==2 && D2H(child)->ref==1);}
        else if(kind==0){memcpy(&word,result,4);require(word==(forward?chaincount(relaycount(n)):relaycount(n)));}
        else if(kind==8){WORD expected=0,k;for(k=1;k<=n;k++)expected+=relaycount(k);memcpy(&word,result,4);require(word==expected);}
        else if(kind==7){WORD a=triangle(triangle(n));memcpy(&word,result,4);require(word==(forward?triangle(triangle(triangle(a))):a));}
        else if(kind==6){memcpy(&word,callee->MP+module_out,4);require(word==n+(forward?0:2) && !memcmp(result,old,8));}
        else if(kind==2){memcpy(&gotbig,result,8);require(gotbig==big);}else if(kind==3){memcpy(&gotreal,result,8);require(gotreal==real);}else require(*(BYTE*)result==byte);
    }
    memcpy(&word,m->origmp+8,4);require(word==(failure?seed:marker));memcpy(&word,pm->origmp+module_out,4);require(word==1234);
    memcpy(&word,idle->MP+module_out,4);require(word==1234);require(!memcmp(guards,root+88,24));for(i=width;i<8;i++)require(((uchar*)result)[i]==0x5a);
    destroystack(&R);require(m->ref==1);unload(m);destroy(idle);require(pm->ref==1 && native64typedmoduleclear(&provider));require(live==0 && rootcount==0);
}
static void externalguards(void)
{
    const ExternalBody *body=ext_bodies;Native64TypedCall calls[16];Native64TypedStep steps[64];Inst output[64],before[64];Type frame={0};unsigned i,j;
    require(body->count<=nelem(steps) && body->nsites<=nelem(calls));frame.size=128;
    for(i=0;i<8;i++){
        memcpy(steps,body->code,body->count*sizeof(*steps));memset(calls,0,sizeof(calls));memset(output,0x5a,sizeof(output));memcpy(before,output,sizeof(output));
        for(j=0;j<body->nsites;j++){const ExternalSite *e=body->sites+j;calls[j]=(Native64TypedCall){e->first,e->end,&frame,nil,e->binding,e->contract,e->inherited,1,e->mode,e->offset};}
        if(i==0)calls[0].external=2;
        if(i==1){calls[0].moduleoffset++;steps[calls[0].first].s++;steps[calls[0].end].d++;}
        if(i==2){calls[0].ordinal=32768;steps[calls[0].first].m=steps[calls[0].end].m=32768;}
        if(i==3)steps[calls[0].first].m++;
        if(i==4)steps[calls[0].end].d+=8;
        if(i==5){steps[body->count-1]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,calls[0].first+1,0};}
        if(i==6){frame.size=64;}
        if(i==7)steps[calls[0].end].returned=32;
        if(native64typedmulticode(output,nelem(output),steps,body->count,body->bytes,16,calls,body->nsites)){
            fprintf(stderr,"external materializer guard removed: case=%u\n",i);exit(10);
        }
        require(!memcmp(output,before,sizeof(output)));frame.size=128;
    }
    /* Same canonical binding from a module-data root rather than an FP root. */
    memcpy(steps,body->code,body->count*sizeof(*steps));
    for(j=0;j<body->nsites;j++){const ExternalSite *e=body->sites+j;calls[j]=(Native64TypedCall){e->first,e->end,&frame,nil,e->binding,e->contract,e->inherited,1,AMP,0};
        steps[e->first].mode=SRC(AMP)|AXIMM|DST(AFP);steps[e->first].s=0;steps[e->end].mode=SRC(AFP)|AXIMM|DST(AMP);steps[e->end].d=0;}
    require(native64typedmulticode(output,nelem(output),steps,body->count,body->bytes,16,calls,body->nsites));
    printf("PASS imported-call materializer: eight atomic refusals and FP/MP module binding\n");
}
int main(int argc,char **argv)
{
    char *oldargv[]={"external","construct-only",nil};int kind,pjit,cjit,grow,forward,q,trial,cases=0;int quanta[]={1,3,1000};
    USED(argv);require(moduleoldmain(2,oldargv)==0);externalguards();if(argc>1){require(argc==2);return 0;}
    for(kind=0;kind<8;kind++)for(pjit=0;pjit<2;pjit++)for(cjit=0;cjit<2;cjit++)for(grow=0;grow<2;grow++)for(forward=0;forward<2;forward++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        externalcall(kind,pjit,cjit,grow,forward,quanta[q],trial,0);cases++;
    }
    for(pjit=0;pjit<2;pjit++)for(cjit=0;cjit<2;cjit++)for(grow=0;grow<2;grow++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        externalcall(8,pjit,cjit,grow,0,quanta[q],trial,0);cases++;
    }
    for(kind=1;kind<=5;kind+=4)for(pjit=0;pjit<2;pjit++)for(cjit=0;cjit<2;cjit++)for(grow=0;grow<2;grow++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        externalcall(kind,pjit,cjit,grow,0,quanta[q],trial,1);cases++;
    }
    printf("PASS source external bodies: %d cases, real MFRAME/MCALL/RET, independent C/RX pairs, source fresh/inherited results, MP clones and complete teardown\n",cases);return 0;
}
