#include "typed-string-imports.h"

static Modlink *boundinstance(Module *m,Import *imports)
{
    Modlink *ml;m->ref++;ml=linkmod(m,imports,1);require(ml!=H);return ml;
}

static unsigned emitclient(Inst *p,unsigned ordinal,int arg,unsigned source,unsigned result)
{
    unsigned n=0;
    callinstruction(p+n++,IMFRAME,AFP,AFP,64,40);p[0].add|=AXIMM;p[0].reg=ordinal;
    if(arg)callargument(p+n++,IMOVP,AFP,source,40,64);
    if(result){callargument(p+n,ILEA,AFP,result,40,32);p[n++].reg=4;}
    callinstruction(p+n,IMCALL,AFP,AFP,40,64);p[n].add|=AXIMM;p[n++].reg=ordinal;
    return n;
}

static void mpconstruction(void)
{
    Native64ModuleOwner owner={0};Native64ModuleSpec s;Native64ModuleFunction fn[4];Native64ModuleSite sites[2];
    uchar map[8];REG saved=R;int jit,start,attempts,i;
    for(jit=0;jit<2;jit++){
        start=allocations;require(native64typedmodule(&client_spec,jit,&owner));attempts=allocations-start;
        require(native64typedmoduleclear(&owner) && !live && !rootcount);
        for(i=1;i<=attempts;i++){
            failat=allocations+i;require(!native64typedmodule(&client_spec,jit,&owner));failat=0;
            require(!owner.module && !owner.rooted && !owner.self && !live && !rootcount && !memcmp(&saved,&R,sizeof(R)));
        }
        printf("PASS String MP import constructor: jit=%d %d allocation failures preserve REG and retire all owners\n",jit,attempts);
    }
    for(i=0;i<4;i++){
        s=client_spec;memmove(fn,client_functions,sizeof(fn));s.functions=fn;
        memmove(sites,fn[client_run].sites,sizeof(sites));fn[client_run].sites=sites;
        memmove(map,s.datamap,s.mapbytes);s.datamap=map;
        if(i==0)map[client_provider_offset/64]^=0x80>>((client_provider_offset/8)%8);
        if(i==1)sites[0].moduleoffset++;
        if(i==2)sites[0].table=s.ntables;
        if(i==3)sites[0].target=client_tables[0].count;
        start=allocations;
        if(native64typedmodule(&s,0,&owner)){fprintf(stderr,"MP import metadata refusal removed: case=%d\n",i);exit(10);}
        require(start==allocations && !owner.module && !live && !rootcount && !memcmp(&saved,&R,sizeof(R)));
    }
    printf("PASS String MP import metadata: four no-allocation module-root/offset/table/ordinal refusals\n");
}

static void mpgraph(unsigned kind,const Input *input,int pjit,int cjit,int grow,int quantum,int runes)
{
    Native64ModuleOwner provider={0},client={0};Module *pm,*cm,*m;Modlink *a,*b,*idle,*consumer,*dormant,*link,*selected,*observed;
    Import requests[5]={{0}};Prog process;Frame *entry,*source;String *value=H,*converted,*seen;
    uchar map[]={0,0xf0},*root;unsigned i,n=0;WORD got,calls;void *result;int before,done,failed=kind==2;
    uvlong guard=0x8fefacdb12345678ULL,after;
    require(native64typedmodule(&spec,pjit,&provider) && native64typedmodule(&client_spec,cjit,&client));pm=provider.module;cm=client.module;
    a=boundinstance(pm,cm->ldt[0]);b=boundinstance(pm,cm->ldt[0]);idle=boundinstance(pm,cm->ldt[0]);
    for(i=0;i<4;i++){requests[i].name=(char*)client_functions[i].name;memmove(&requests[i].sig,&client_functions[i].signature,4);}
    consumer=boundinstance(cm,requests);dormant=boundinstance(cm,requests);
    require(a!=b && a->MP!=b->MP && consumer->MP!=dormant->MP && consumer->MP!=cm->origmp);
    require(*(void**)(cm->origmp+client_provider_offset)==H && *(void**)(consumer->MP+client_provider_offset)==H);
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;
    m->name=strdup("MPStringLauncher");m->path=strdup("trusted/MPStringLauncher");m->ss=grow?0:4096;m->cstep=native64typedlea8step;
    m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->type!=nil);
    m->type[0]=dtype(freeheap,8,nil,0);m->type[1]=dtype(freeheap,144,map,2);require(m->type[0] && m->type[1]);m->entryt=m->type[1];
    m->origmp=H2D(uchar*,heapz(m->type[0]));m->prog=mallocz(40*sizeof(Inst),1);require(m->prog!=nil);m->entry=m->prog;
    n+=emitclient(m->prog+n,client_bind,1,72,0);
    n+=emitclient(m->prog+n,client_run,1,88,96);
    if(kind==1 || kind==3){
        n+=emitclient(m->prog+n,client_bind,1,kind==1?80:72,0);
        n+=emitclient(m->prog+n,client_run,1,88,104);
    }
    if(kind==2 || kind==3)n+=emitclient(m->prog+n,client_clear,0,0,0);
    if(kind!=3)n+=emitclient(m->prog+n,client_forward,1,88,112);
    callinstruction(m->prog+n++,IEXIT,AXXX,AXXX,0,0);m->nprog=n;
    if(cjit)require(compile(m,m->nprog,nil));
    memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
    process.R.M=link;process.R.PC=m->entry;entry=mallocz(m->entryt->size,1);require(entry!=nil);
    entry->t=m->entryt;initmem(entry->t,entry);source=entry;R.s=&source;newstack(&process);free(entry);root=process.R.FP;
    memmove(root+64,&consumer,8);memmove(root+72,&a,8);memmove(root+80,&b,8);
    if(!input->nilvalue){
        value=c2string(input->text,input->bytes);
        if(runes && value->len>=0){converted=newrunes(value->len);for(i=0;i<(unsigned)value->len;i++)converted->Srune[i]=(uchar)value->Sascii[i];destroy(value);value=converted;}
    }
    memmove(root+88,&value,8);memset(root+96,0x5a,24);memmove(root+120,&guard,8);result=root+96;memmove(root+48,&result,8);
    before=raised;message[0]='x';process.quanta=quantum;
    for(i=0;i<10000;i++){done=kprotect(execute,&process);if(!done)break;}
    if(i>=10000 || (failed?strcmp(message,exModule):message[0]))fprintf(stderr,"MP String kind=%u input=%u jit=%d/%d grow=%d quantum=%d runes=%d error=%s\n",kind,(unsigned)(input-inputs),pjit,cjit,grow,quantum,runes,message);
    require(i<10000 && raised==before+1 && !worker.nerr && (failed?!strcmp(message,exModule):!message[0]));
    if(!failed)require(R.M==link && R.MP==link->MP && R.FP==root);
    memmove(&got,root+96,4);require(got==2*input->expected);
    if(kind==1 || kind==3){memmove(&got,root+104,4);require(got==2*input->expected);}
    else for(i=104;i<108;i++)require(root[i]==0x5a);
    if(kind==0 || kind==1){memmove(&got,root+112,4);require(got==input->expected);}
    else for(i=112;i<116;i++)require(root[i]==0x5a);
    for(i=0;i<3;i++)require(!memcmp(root+100+8*i,"ZZZZ",4));
    memmove(&after,root+120,8);require(after==guard);
    memmove(&selected,consumer->MP+client_provider_offset,8);require(selected==(kind==0?a:kind==1?b:H));
    memmove(&calls,consumer->MP+client_calls_offset,4);require(calls==((kind==1 || kind==3)?19:18));
    memmove(&calls,dormant->MP+client_calls_offset,4);require(calls==17);
    memmove(&calls,cm->origmp+client_calls_offset,4);require(calls==17);
    memmove(&observed,dormant->MP+client_provider_offset,8);require(observed==H);
    memmove(&observed,cm->origmp+client_provider_offset,8);require(observed==H);
    memmove(&seen,a->MP+current_offset,8);require(seen==value);
    memmove(&seen,b->MP+current_offset,8);require(seen==(kind==1?value:H));
    memmove(&seen,idle->MP+current_offset,8);require(seen==H);
    memmove(&seen,pm->origmp+current_offset,8);require(seen==H);
    require(D2H(a)->ref==(kind==0?2:1) && D2H(b)->ref==(kind==1?2:1));
    if(value!=H){
        int expected=kind==1 || failed?3:2; /* The faulting forward frame still owns its argument. */
        if(D2H(value)->ref!=expected)fprintf(stderr,"MP String ownership kind=%u input=%u jit=%d/%d actual=%lu expected=%d\n",kind,(unsigned)(input-inputs),pjit,cjit,(unsigned long)D2H(value)->ref,expected);
        require(D2H(value)->ref==expected);
    }
    if(!failed){
        Modlink *copy=boundinstance(cm,requests);int refs=selected==H?0:D2H(selected)->ref;
        REG saved=R;newmp(copy->MP,consumer->MP,cm->type[0]);
        memmove(&observed,copy->MP+client_provider_offset,8);require(observed==selected && !memcmp(&saved,&R,sizeof(R)));
        if(selected!=H && D2H(selected)->ref!=refs+1){fprintf(stderr,"bound module clone retain omitted\n");exit(10);}
        memmove(&calls,copy->MP+client_calls_offset,4);require(calls==(kind==0?18:19));
        destroy(copy);if(selected!=H)require(D2H(selected)->ref==refs);
    }
    destroystack(&R);require(m->ref==1);unload(m);destroy(dormant);destroy(idle);
    require(cm->ref==1 && pm->ref==1 && native64typedmoduleclear(&client) && native64typedmoduleclear(&provider));
    require(!live && !rootcount && Tstring.ref==1);
}

int main(void)
{
    unsigned k,v,p,c,g,q,r,cases=0;int quantum[]={1,3,1000};Tstring.free=freestring;
    optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;
    optab[IMOVW]=movw;optab[IMOVL]=movl;optab[IMOVP]=movp;optab[IADDW]=addw;optab[ILENC]=lenc;optab[IEXIT]=iexit;
    mpconstruction();
    for(k=0;k<4;k++)for(v=0;v<nelem(inputs);v++)for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<nelem(quantum);q++)for(r=0;r<2;r++){
        mpgraph(k,inputs+v,p,c,g,quantum[q],r);cases++;
    }
    printf("PASS String MP import graph: %u source C/JIT roots, owned linkmod imports, bind/rebind/self-bind/clear/nil failure, Unicode/NUL, instance isolation and complete retirement\n",cases);
    return 0;
}
