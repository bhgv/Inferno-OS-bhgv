#include "typed-imports-native64.h"

static Modlink *ownedinstance(Module *consumer,unsigned table,Module *provider)
{
    Modlink *ml;require(consumer->ldt && consumer->ldt[table]);provider->ref++;
    ml=linkmod(provider,consumer->ldt[table],1);require(ml!=H);return ml;
}
static void importconstruction(void)
{
    Native64ModuleOwner owner={0};Native64ModuleSpec s=client_spec;Native64ModuleFunction f[32];Native64ModuleSite sites[16];
    Native64ModuleTable tables[3];Native64ModuleImport requests[32];unsigned code,exports,scratch,nimports;REG saved;
    uchar mpmap[]={0x80};int i,j,base,attempts;memset(&saved,0,sizeof(saved));saved.IC=27;saved.t=19;R=saved;
    for(j=0;j<6;j++){
        s=client_spec;tables[0]=tables[1]=client_tables[0];s.tables=tables;s.ntables=j<2?1:2;if(j>=4){s.databytes=8;s.datamap=mpmap;s.mapbytes=1;}
        base=allocations;require(native64typedmodule(&s,j%2,&owner));attempts=allocations-base;
        require(!memcmp(&R,&saved,sizeof(R)) && owner.module->entry==nil && owner.module->entryt==nil && (j<4?(owner.module->origmp==H && !owner.rooted):(owner.module->origmp!=H && owner.rooted && rootcount==1)));
        require(owner.module->ldt && owner.module->ldt[0] && owner.module->ldt[s.ntables]==nil);
        for(i=0;i<(int)client_tables[0].count;i++)require(!strcmp(owner.module->ldt[0][i].name,client_requests[i].name) &&
            !memcmp(&owner.module->ldt[0][i].sig,&client_requests[i].signature,4) && owner.module->ldt[0][i].name!=client_requests[i].name);
        require(owner.module->ldt[0][i].name==nil);require(native64typedmoduleclear(&owner) && live==0 && rootcount==0);
        for(i=1;i<=attempts;i++){
            failat=allocations+i;require(!native64typedmodule(&s,j%2,&owner));failat=0;
            if(owner.module || owner.rooted || memcmp(&saved,&R,sizeof(R)) || live || rootcount || modules){fprintf(stderr,"import partial owner leaked\n");exit(10);}
        }
        printf("PASS owned import constructor: %d failure positions jit=%d, tables=%u, copied tables/Types and complete rollback\n",attempts,j%2,s.ntables);
    }
    for(i=0;i<10;i++){
        base=allocations;s=client_spec;memcpy(f,client_functions,sizeof(client_functions));s.functions=f;memcpy(sites,f[0].sites,f[0].nsites*sizeof(*sites));f[0].sites=sites;
        tables[0]=tables[1]=tables[2]=client_tables[0];s.tables=tables;memcpy(requests,client_requests,sizeof(client_requests));tables[0].imports=requests;
        if(i==0)sites[0].table=s.ntables; /* Backing second table makes the omission control safe. */
        if(i==1)sites[0].target=tables[0].count;
        if(i==2)sites[0].external=2;
        if(i==3)sites[0].moduleoffset=72; /* Ordinary int argument, not a module root. */
        if(i==4){static const uchar bad[]={0x80,0};requests[0].map=bad;}
        if(i==5)requests[0].name="";if(i==6)s.ntables=65;if(i==7)tables[0].count=4097;
        if(i==8)requests[0].bytes=65544;if(i==9)sites[0].moduleoffset=65;
        if(native64modulecheck(&s,&code,&exports,&scratch,&nimports)){fprintf(stderr,"import constructor guard removed: case=%d\n",i);exit(10);}
        require(!native64typedmodule(&s,0,&owner) && allocations==base && owner.module==nil && live==0 && rootcount==0 && !memcmp(&R,&saved,sizeof(R)));
    }
    printf("PASS owned import preflight: ten no-allocation refusals with unchanged REG/output\n");
    /* Independent tables own independent names/maps, and table1 bounds relocate correctly. */
    {char name[64];uchar map[2]={0};
        s=client_spec;tables[0]=tables[1]=client_tables[0];s.tables=tables;s.ntables=2;
        memcpy(requests,client_requests,sizeof(client_requests));tables[1].imports=requests;strcpy(name,requests[0].name);requests[0].name=name;requests[0].map=map;
        memcpy(f,client_functions,sizeof(client_functions));s.functions=f;memcpy(sites,f[0].sites,f[0].nsites*sizeof(*sites));f[0].sites=sites;sites[0].table=1;
        require(native64typedmodule(&s,1,&owner));require(owner.module->ntype==1+18+46 && owner.module->ldt[2]==nil);
        name[0]='?';map[1]=0xff;requests[0].signature^=1;
        require(!strcmp(owner.module->ldt[1][0].name,client_requests[0].name) && !memcmp(&owner.module->ldt[1][0].sig,&client_requests[0].signature,4));
        require(owner.module->type[1+18+23]->map[1]==0);require(native64typedmoduleclear(&owner) && live==0 && rootcount==0);
    }
    printf("PASS owned import table1 relocation and independent deep copies\n");
    {Native64TypedStep steps[64];unsigned k;
        s=client_spec;s.databytes=8;s.datamap=mpmap;s.mapbytes=1;memcpy(f,client_functions,sizeof(client_functions));s.functions=f;
        require(f[0].count<=nelem(steps));memcpy(steps,f[0].code,f[0].count*sizeof(*steps));f[0].code=steps;
        memcpy(sites,f[0].sites,f[0].nsites*sizeof(*sites));f[0].sites=sites;
        for(k=0;k<f[0].nsites;k++){
            sites[k].modulemode=AMP;sites[k].moduleoffset=0;
            steps[sites[k].first].mode=SRC(AMP)|AXIMM|DST(AFP);steps[sites[k].first].s=0;
            steps[sites[k].end].mode=SRC(AFP)|AXIMM|DST(AMP);steps[sites[k].end].d=0;
        }
        require(native64typedmodule(&s,1,&owner) && owner.rooted && rootcount==1 && *(void**)owner.module->origmp==H);
        require(native64typedmoduleclear(&owner) && live==0 && rootcount==0);
    }
    printf("PASS owned import mapped-MP module binding construction (no MP-source execution claim)\n");
}

typedef struct ImportAttempt ImportAttempt;
struct ImportAttempt {Module *provider;Import *imports;Modlink *result;};
static int importattempt(void *arg)
{
    ImportAttempt *a=arg;a->result=linkmod(a->provider,a->imports,1);return a->result!=H;
}
static void importlinkfailures(void)
{
    Native64ModuleOwner provider={0},client={0};Module *pm,*cm;Modlink *result;Import saved;int which,base,roots,refs,i,attempts;REG reg;
    require(native64typedmodule(&module_spec,0,&provider) && native64typedmodule(&client_spec,0,&client));pm=provider.module;cm=client.module;
    reg=R;base=live;roots=rootcount;refs=pm->ref;saved=cm->ldt[0][client_tables[0].count-1];
    for(which=0;which<2;which++){
        if(which==0)cm->ldt[0][client_tables[0].count-1].name="missing";else cm->ldt[0][client_tables[0].count-1].sig^=1;
        pm->ref++;result=linkmod(pm,cm->ldt[0],1);
        if(result!=H){fprintf(stderr,"import binding guard removed: case=%d\n",which);exit(10);}
        require(pm->ref==refs && live==base && rootcount==roots && !memcmp(&R,&reg,sizeof(R)));cm->ldt[0][client_tables[0].count-1]=saved;
    }
    base=allocations;pm->ref++;result=linkmod(pm,cm->ldt[0],1);require(result!=H);attempts=allocations-base;destroy(result);base=live;
    for(i=1;i<=attempts;i++){
        /* linkmod consumes the borrowed extra ref and rethrows allocation failure. */
        ImportAttempt attempt={pm,cm->ldt[0],H};
        pm->ref++;failat=allocations+i;require(!kprotect(importattempt,&attempt));failat=0;require(attempt.result==H);
        require(pm->ref==refs && live==base && rootcount==roots && !memcmp(&R,&reg,sizeof(R)));
    }
    require(native64typedmoduleclear(&client) && native64typedmoduleclear(&provider) && live==0 && rootcount==0);
    printf("PASS owned import link failure: missing/late wrong-hash rollback and %d clone allocation failures\n",attempts);
}

static void ownedcall(int kind,int providerjit,int clientjit,int grow,int forward,int quantum,int trial,int failure)
{
    Native64ModuleOwner provider={0},client={0};const ExternalBody *body=ext_bodies+(kind>=8?16:kind*2+forward);
    Module *m,*pm,*cm;Modlink *callee,*idle,*clientlink,*link;Prog process;Frame *entry,*source;Import request={0};
    uchar map[]={0,0xc0},childmap[]={0x80},*root,old[8],guards[24];unsigned i,offset=0,target=kind==9?17:kind==8?16:kind*2+forward;
    int done,initial;WORD n=trial?-3:kind==7?2:7,word,seed=0x1234,marker=0x55667788;
    LONG big=trial?(-0x7fffffffffffffffLL-1):0x7fffffffffffffffLL,gotbig;REAL real=trial?-1.25:1e100,gotreal;BYTE byte=trial?255:128;
    unsigned width=kind==0 || kind==6 || kind==7 || kind==8 || kind==9?4:kind==4?1:8,ref=kind==1 || kind==5;
    int ops[]={IMOVW,IMOVP,IMOVL,IMOVF,IMOVB,IMOVP,IMOVW,IMOVW,IMOVW,IMOVW};void *owner=H,*child=H,*observed,*result;
    require(native64typedmodule(&module_spec,providerjit,&provider));require(native64typedmodule(&client_spec,failure?0:clientjit,&client));pm=provider.module;cm=client.module;
    require(cm->entry==nil && cm->entryt==nil && cm->origmp==H && cm->ldt && cm->ldt[1]==nil);
    if(failure){for(i=0;i<target;i++)offset+=client_functions[i].count;offset+=body->sites[body->nsites-1].end+1;
        require(offset<(unsigned)cm->nprog);callinstruction(cm->prog+offset,IDIVW,AIMM,AFP,0,56);if(clientjit)require(compile(cm,cm->nprog,nil));}
    callee=ownedinstance(cm,0,pm);ptradd(D2H(callee));idle=ownedinstance(cm,0,pm);ptradd(D2H(idle));
    request.name=(char*)client_functions[target].name;memcpy(&request.sig,&client_functions[target].signature,4);
    {Import requests[2]={{0},{0}};requests[0]=request;cm->ref++;clientlink=linkmod(cm,requests,1);require(clientlink!=H);ptradd(D2H(clientlink));}
    require(!native64typedmoduleclear(&client) && !native64typedmoduleclear(&provider));
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->name=strdup("Harness");m->path=strdup("trusted/Harness");
    m->ntype=4;m->type=mallocz(4*sizeof(Type*),1);require(m->type!=nil);map[1]=ref?0xf0:0xc0;
    m->type[0]=dtype(freeheap,16,nil,0);m->type[1]=dtype(freeheap,128,map,2);m->type[2]=dtype(freeheap,8,childmap,1);m->type[3]=dtype(freeheap,8,nil,0);
    for(i=0;i<4;i++)require(m->type[i]!=nil);m->entryt=m->type[1];m->ss=grow?0:4096;m->origmp=H2D(uchar*,heapz(m->type[0]));memcpy(m->origmp+8,&seed,4);
    m->nprog=7;m->prog=mallocz(7*sizeof(Inst),1);require(m->prog!=nil);m->entry=m->prog;m->cstep=native64typedlea8step;
    callinstruction(m->prog,IMFRAME,AFP,AFP,64,40);m->prog[0].add|=AXIMM;m->prog[0].reg=0;
    callargument(m->prog+1,IMOVP,AFP,72,40,64);callargument(m->prog+2,ops[kind],AFP,80,40,72);
    callargument(m->prog+3,ILEA,AFP,88,40,32);m->prog[3].reg=width|(ref?16:0);
    callinstruction(m->prog+4,IMCALL,AFP,AFP,40,64);m->prog[4].add|=AXIMM;m->prog[4].reg=0;
    callinstruction(m->prog+5,IMOVW,AIMM,AMP,marker,8);callinstruction(m->prog+6,IEXIT,AXXX,AXXX,0,0);
    /* Harness shares the client's dispatch choice; both source modules are built independently. */
    if(clientjit)require(compile(m,m->nprog,nil));memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);ptradd(D2H(link));
    link->MP=m->origmp;D2H(link->MP)->ref++;process.R.M=link;process.R.PC=m->entry;
    entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(m->entryt,entry);source=entry;R.s=&source;newstack(&process);free(entry);root=process.R.FP;
    memcpy(root+64,&clientlink,8);memcpy(root+72,&callee,8);ptrdel(D2H(clientlink));ptrdel(D2H(callee));
    if(ref && !trial){child=H2D(void*,heapz(m->type[3]));owner=H2D(void*,heapz(m->type[2]));memcpy(owner,&child,8);memcpy(root+80,&owner,8);
        observed=H2D(void*,heapz(m->type[3]));memcpy(root+88,&observed,8);
    }else if(ref){observed=H2D(void*,heapz(m->type[3]));memcpy(root+88,&observed,8);}
    else if(kind==0 || kind==6 || kind==7 || kind==8 || kind==9)memcpy(root+80,&n,4);else if(kind==2)memcpy(root+80,&big,8);else if(kind==3)memcpy(root+80,&real,8);else memcpy(root+80,&byte,1);
    if(kind==6)memset(root+88,0x6b,4);memset(root+88+width,0x5a,8-width);memset(root+96,0x3c,24);memcpy(guards,root+96,24);
    result=root+88;memcpy(old,result,8);process.quanta=quantum;initial=raised;message[0]='x';
    for(i=0;i<100000;i++){done=kprotect(execute,&process);if(!done)break;}
    if(i>=100000 || raised!=initial+1 || (failure?strcmp(message,exZdiv):message[0]))fprintf(stderr,"owned import case %d/%d/%d/%d/%d/%d/%d failure=%d error=%s\n",kind,providerjit,clientjit,grow,forward,quantum,trial,failure,message);
    require(i<100000 && raised==initial+1 && worker.nerr==0 && (failure?!strcmp(message,exZdiv):!message[0]));
    if(failure)require(!memcmp(result,old,8));else{
        require(R.M==link && R.MP==link->MP && R.FP==root);
        if(ref){memcpy(&observed,result,8);require(observed==owner);if(owner!=H)require(D2H(owner)->ref==2 && D2H(child)->ref==1);}
        else if(kind==0){memcpy(&word,result,4);require(word==(forward?chaincount(relaycount(n)):relaycount(n)));}
        else if(kind==9){memcpy(&word,result,4);require(word==relaycount(n));}
        else if(kind==8){WORD expected=0,k;for(k=1;k<=n;k++)expected+=relaycount(k);memcpy(&word,result,4);require(word==expected);}
        else if(kind==7){WORD a=triangle(triangle(n));memcpy(&word,result,4);require(word==(forward?triangle(triangle(triangle(a))):a));}
        else if(kind==6){memcpy(&word,callee->MP+module_out,4);require(word==n+(forward?0:2) && !memcmp(result,old,8));}
        else if(kind==2){memcpy(&gotbig,result,8);require(gotbig==big);}else if(kind==3){memcpy(&gotreal,result,8);require(gotreal==real);}else require(*(BYTE*)result==byte);
    }
    memcpy(&word,m->origmp+8,4);require(word==(failure?seed:marker));memcpy(&word,pm->origmp+module_out,4);require(word==1234);
    memcpy(&word,idle->MP+module_out,4);require(word==1234);require(cm->origmp==H && !memcmp(guards,root+96,24));for(i=width;i<8;i++)require(((uchar*)result)[i]==0x5a);
    ptrdel(D2H(link));destroystack(&R);require(m->ref==1);unload(m);ptrdel(D2H(idle));destroy(idle);
    require(cm->ref==1 && pm->ref==1 && native64typedmoduleclear(&client) && native64typedmoduleclear(&provider));require(live==0 && rootcount==0);
}
int main(int argc,char **argv)
{
    char *oldargv[]={"imports","construct-only",nil};int kind,pjit,cjit,grow,forward,q,trial,cases=0;int quanta[]={1,3,1000};USED(argv);
    require(moduleoldmain(2,oldargv)==0);importconstruction();importlinkfailures();if(argc>1){require(argc==2);return 0;}
    for(kind=0;kind<8;kind++)for(pjit=0;pjit<2;pjit++)for(cjit=0;cjit<2;cjit++)for(grow=0;grow<2;grow++)for(forward=0;forward<2;forward++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        ownedcall(kind,pjit,cjit,grow,forward,quanta[q],trial,0);cases++;
    }
    for(pjit=0;pjit<2;pjit++)for(cjit=0;cjit<2;cjit++)for(grow=0;grow<2;grow++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        for(kind=8;kind<=9;kind++){ownedcall(kind,pjit,cjit,grow,0,quanta[q],trial,0);cases++;}
    }
    for(kind=1;kind<=5;kind+=4)for(pjit=0;pjit<2;pjit++)for(cjit=0;cjit<2;cjit++)for(grow=0;grow<2;grow++)for(q=0;q<3;q++)for(trial=0;trial<2;trial++){
        ownedcall(kind,pjit,cjit,grow,0,quanta[q],trial,1);cases++;
    }
    printf("PASS owned source client/provider: %d three-module cases, constructor imports, production linkmod, C/RX pairs and complete teardown\n",cases);return 0;
}
