#include "typed-module-native64.h"
static void modulecheck(int ok,int line){if(!ok)fprintf(stderr,"typed module assertion at line %d\n",line);require(ok);}
#define require(c) modulecheck(!!(c),__LINE__)

static Modlink *instance(Module *m,int target)
{
    Modlink *ml;Import request={0};Modl before;int allocations_before;
    m->ref++;ml=mklinkmod(m,1);ml->MP=H2D(uchar*,heapz(m->type[0]));newmp(ml->MP,m->origmp,m->type[0]);
    memcpy(&before,ml->links,sizeof(before));request.name="missing";request.sig=0;allocations_before=allocations;
    require(linkm(m,ml,0,&request)<0 && !memcmp(&before,ml->links,sizeof(before)) && allocations==allocations_before);
    request.name="hidden";memcpy(&request.sig,&module_import_signatures[target],4);
    require(linkm(m,ml,0,&request)<0 && !memcmp(&before,ml->links,sizeof(before)));
    request.name=(char*)module_functions[target].name;request.sig^=1;
    if(linkm(m,ml,0,&request)>=0){fprintf(stderr,"module signature guard removed\n");exit(10);}
    require(!memcmp(&before,ml->links,sizeof(before)));
    memcpy(&request.sig,&module_import_signatures[target],4);require(linkm(m,ml,0,&request)==0);
    require(ml->links[0].frame==m->type[target+1]);return ml;
}

static void modulecall(int kind,int providerjit,int callerjit,int grow,int nested,int quanta,int trial,int failure)
{
    Native64ModuleOwner provider={0};Module *m,*pm;Modlink *callee,*idle,*link;Frame *entry,*source;Prog process;
    void *owner=H,*child=H,*result,*observed;uchar map[]={0,0x80},childmap[]={0x80},old[8],guards[24];uchar *root;
    int i,initial,done,body=module_entries[kind][nested];unsigned int width=(kind==0 || kind==6 || kind==7)?4:kind==4?1:8,ref=kind==1 || kind==5;
    int ops[]={IMOVW,IMOVP,IMOVL,IMOVF,IMOVB,IMOVP,IMOVW,IMOVW};
    LONG big=trial?(-0x7fffffffffffffffLL-1):0x7fffffffffffffffLL,gotbig;REAL real=trial?-1.25:1e100,gotreal;
    WORD n=trial?-3:7,word,marker=0x55667788,seed=0x1234;BYTE byte=trial?255:128;
    require(native64typedmodule(&module_spec,failure?0:providerjit,&provider));pm=provider.module;require(pm->entry==nil && pm->entryt==nil);
    if(failure){ /* Private fault only after the selected source caller's final managed call. */
        unsigned int offset=0;for(i=0;i<body;i++)offset+=module_functions[i].count;
        callinstruction(pm->prog+offset+module_functions[body].sites[module_functions[body].nsites-1].end+1,IDIVW,AIMM,AFP,0,56);
        /* Fault bytes must be selected before a JIT snapshot is built. */
        if(providerjit)require(compile(pm,pm->nprog,nil));
    }
    callee=instance(pm,body);idle=instance(pm,body);require(!native64typedmoduleclear(&provider) && provider.rooted);
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->name=strdup("Consumer");m->path=strdup("trusted/Consumer");
    m->ntype=4;m->type=mallocz(4*sizeof(Type*),1);require(m->type!=nil);map[1]=ref?0xe0:0x80;
    m->type[0]=dtype(freeheap,16,nil,0);m->type[1]=dtype(freeheap,112,map,2);
    m->type[2]=dtype(freeheap,8,childmap,1);m->type[3]=dtype(freeheap,8,nil,0);
    for(i=0;i<4;i++)require(m->type[i]!=nil);
    m->entryt=m->type[1];m->ss=grow?0:4096;m->origmp=H2D(uchar*,heapz(m->type[0]));memcpy(m->origmp+8,&seed,4);
    m->nprog=6;m->prog=mallocz(6*sizeof(Inst),1);require(m->prog!=nil);m->entry=m->prog;m->cstep=native64typedlea8step;
    callinstruction(m->prog,IMFRAME,AFP,AFP,64,40);m->prog[0].add|=AXIMM;m->prog[0].reg=0;
    callargument(m->prog+1,ops[kind],AFP,72,40,64);
    if(kind==6)callargument(m->prog+2,IMOVL,AFP,48,40,32);
    else{callargument(m->prog+2,ILEA,AFP,80,40,32);m->prog[2].reg=width|(ref?16:0);}
    callinstruction(m->prog+3,IMCALL,AFP,AFP,40,64);m->prog[3].add|=AXIMM;m->prog[3].reg=0;
    callinstruction(m->prog+4,IMOVW,AIMM,AMP,marker,8);callinstruction(m->prog+5,IEXIT,AXXX,AXXX,0,0);
    if(callerjit)require(compile(m,m->nprog,nil));
    memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
    process.R.M=link;process.R.PC=m->entry;entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(m->entryt,entry);
    source=entry;R.s=&source;newstack(&process);free(entry);root=process.R.FP;memcpy(root+64,&callee,8); /* Transfer one instance owner. */
    if(ref && !trial){
        child=H2D(void*,heapz(m->type[3]));owner=H2D(void*,heapz(m->type[2]));memcpy(owner,&child,8);memcpy(root+72,&owner,8);
        if(kind==5){D2H(owner)->ref++;memcpy(root+80,&owner,8);}else{observed=H2D(void*,heapz(m->type[3]));memcpy(root+80,&observed,8);}
    }else if(ref){observed=H2D(void*,heapz(m->type[3]));memcpy(root+80,&observed,8);}
    else if(kind==0 || kind==6 || kind==7)memcpy(root+72,&n,4);
    else if(kind==2)memcpy(root+72,&big,8);else if(kind==3)memcpy(root+72,&real,8);else memcpy(root+72,&byte,1);
    if(kind==6)memset(root+80,0x6b,4);memset(root+80+width,0x5a,8-width);memset(root+88,0x3c,24);
    memcpy(guards,root+88,24);result=root+80;memcpy(old,result,8);memcpy(root+48,&result,8);
    process.quanta=quanta;initial=raised;message[0]='x';
    for(i=0;i<10000;i++){done=kprotect(execute,&process);if(!done)break;}
    if(i>=10000 || raised!=initial+1 || (failure?strcmp(message,exZdiv):message[0]))fprintf(stderr,"module case %d/%d/%d/%d/%d/%d/%d failure=%d error=%s\n",kind,providerjit,callerjit,grow,nested,quanta,trial,failure,message);
    require(i<10000 && raised==initial+1 && worker.nerr==0 && (failure?!strcmp(message,exZdiv):!message[0]));
    if(failure)require(!memcmp(result,old,8));
    else{
        require(R.M==link && R.MP==link->MP && R.FP==root);
        if(ref){memcpy(&observed,result,8);require(observed==owner);if(owner!=H)require(D2H(owner)->ref==2 && D2H(child)->ref==1);}
        else if(kind==0){memcpy(&word,result,4);require(word==((n+1)>0?(n+1)*(n+2)/2:0)+(n>0?n*(n+1)/2:0)+(nested && n>0?n*(n+1)*(n+2)/6:0));}
        else if(kind==7){WORD expected=n>0?n*(n+1)/2:0;expected=expected*(expected+1)/2;if(nested)expected=expected*(expected+1)/2;memcpy(&word,result,4);require(word==expected);}
        else if(kind==6){memcpy(&word,callee->MP+module_out,4);require(word==n+(nested?2:0) && !memcmp(result,old,8));}
        else if(kind==2){memcpy(&gotbig,result,8);require(gotbig==big);}else if(kind==3){memcpy(&gotreal,result,8);require(gotreal==real);}else require(*(BYTE*)result==byte);
    }
    memcpy(&word,m->origmp+8,4);require(word==(failure?seed:marker));
    memcpy(&word,pm->origmp+module_out,4);require(word==1234);memcpy(&word,idle->MP+module_out,4);require(word==1234);
    require(!memcmp(guards,root+88,24));for(i=width;i<8;i++)require(((uchar*)result)[i]==0x5a);
    destroystack(&R);require(m->ref==1);unload(m);destroy(idle);require(pm->ref==1 && native64typedmoduleclear(&provider));require(live==0 && rootcount==0);
}

static void constructiontests(void)
{
    Native64ModuleOwner owner={0},copy;Native64ModuleSpec s=module_spec;Native64ModuleFunction f[32];Native64ModuleInit init=module_initial[0];
    REG saved;int i,j,attempts,base;
    memset(&saved,0,sizeof(saved));saved.IC=17;saved.t=9;R=saved;
    for(j=0;j<2;j++){
        base=allocations;require(native64typedmodule(&s,j,&owner));attempts=allocations-base;require(!memcmp(&saved,&R,sizeof(R)) && owner.rooted && rootcount==1);
        copy=owner;
        if(native64typedmoduleclear(&copy)){fprintf(stderr,"module owner copy guard removed\n");exit(10);}
        require(native64typedmoduleclear(&owner) && native64typedmoduleclear(&owner) && !native64typedmoduleclear(&copy) && live==0 && rootcount==0);
        for(i=1;i<=attempts;i++){
            failat=allocations+i;require(!native64typedmodule(&s,j,&owner));failat=0;
            if(owner.module!=nil || owner.rooted || memcmp(&saved,&R,sizeof(R)) || live!=0 || rootcount!=0 || modules!=nil){fprintf(stderr,"module failure rollback guard removed\n");exit(10);}
        }
        printf("PASS module constructor: %d allocation failure positions jit=%d, pinned MP, empty output and preserved REG\n",attempts,j);
    }
    for(i=0;i<8;i++){
        int before=allocations;s=module_spec;memcpy(f,module_functions,sizeof(module_functions));s.functions=f;init=module_initial[0];s.initial=&init;
        if(i==0)s.entry=s.nfunctions;if(i==1)f[1].name=f[0].name;if(i==2)f[0].mapbytes=0;if(i==3)init.offset=0;
        if(i==4)init.width=2;if(i==5)f[0].exported=2;if(i==6)s.nfunctions=1025;if(i==7)init.bits=0x100000000ULL;
        if(native64typedmodule(&s,0,&owner)){fprintf(stderr,"module metadata guard removed: case=%d\n",i);exit(10);}
        require(allocations==before && live==0 && rootcount==0);
    }
    printf("PASS module metadata: eight preallocation refusals, exact output/ownership invariants\n");
    { Native64TypedStep bad[64];int before;
      s=module_spec;memcpy(f,module_functions,sizeof(module_functions));s.functions=f;require(f[0].count<nelem(bad));
      memcpy(bad,f[0].code,f[0].count*sizeof(*bad));bad[0].op=ILOAD;f[0].code=bad;before=allocations;
      require(!native64typedmodule(&s,0,&owner) && allocations>before && owner.module==nil && !owner.rooted && live==0 && rootcount==0 && !memcmp(&saved,&R,sizeof(R)));
    }
    { const Native64TypedStep retcode[]={{IRET,SRC(AXXX)|DST(AXXX),0,0,0,0}};const uchar map[]={0};
      Native64ModuleFunction one={"noop",0,1,retcode,1,64,map,1,nil,0};
      Native64ModuleInit values[]={{0,1,255},{4,4,0x89abcdefULL},{8,8,0x8000000000000000ULL},{16,8,0x3ff4000000000000ULL}};
      Native64ModuleSpec small={"Scalar","trusted/Scalar",&one,1,0,24,map,1,values,4};uvlong bits;UWORD word;REAL real;
      require(native64typedmodule(&small,0,&owner));require(owner.module->origmp[0]==255);
      memcpy(&word,owner.module->origmp+4,4);memcpy(&bits,owner.module->origmp+8,8);memcpy(&real,owner.module->origmp+16,8);
      require(word==0x89abcdefU && bits==0x8000000000000000ULL && real==1.25 && native64typedmoduleclear(&owner));
      small.databytes=0;small.datamap=nil;small.mapbytes=0;small.initial=nil;small.ninitial=0;
      require(native64typedmodule(&small,0,&owner) && !owner.rooted && owner.module->origmp==H && native64typedmoduleclear(&owner));
      require(live==0 && rootcount==0 && !memcmp(&saved,&R,sizeof(R)));
    }
    printf("PASS constructor payloads: protected bad-code rollback, byte/Word/big/real bits and empty MP ownership\n");
}

int main(int argc,char **argv)
{
    USED(argv);int kind,pjit,cjit,grow,nested,q,trial,cases=0;int quanta[]={1,3,1000};
    optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;optab[IMOVP]=movp;optab[IMOVB]=movb;
    optab[IMOVW]=movw;optab[IMOVL]=movl;optab[IMOVF]=movf;optab[IADDW]=addw;optab[ISUBW]=subw;
    optab[IJMP]=jmp;optab[IBEQW]=beqw;optab[IBNEW]=bnew;optab[IBLTW]=bltw;optab[IBLEW]=blew;optab[IBGTW]=bgtw;optab[IBGEW]=bgew;optab[IEXIT]=iexit;optab[IDIVW]=divw;
    constructiontests();if(argc>1){require(argc==2);return 0;}
    for(kind=0;kind<8;kind++)for(pjit=0;pjit<2;pjit++)for(cjit=0;cjit<2;cjit++)for(grow=0;grow<2;grow++)for(nested=0;nested<2;nested++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        modulecall(kind,pjit,cjit,grow,nested,quanta[q],trial,0);cases++;
    }
    for(kind=1;kind<=5;kind+=4)for(pjit=0;pjit<2;pjit++)for(cjit=0;cjit<2;cjit++)for(grow=0;grow<2;grow++)for(nested=0;nested<2;nested++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        modulecall(kind,pjit,cjit,grow,nested,quanta[q],trial,1);cases++;
    }
    printf("PASS whole native modules: %d cross-module MFRAME/MCALL/RET cases, C/RX pairs, separate MP clones, export hashes/privacy and complete teardown\n",cases);
    return 0;
}
