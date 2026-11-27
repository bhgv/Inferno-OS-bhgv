#include "typed-loads.h"
static Module *serviceprovider,*servicecaller;
static Modlink *loaded[2];
static int servicecalls,servicefault,servicepins;
static int pininstance(void *arg){ptradd(D2H((Modlink*)arg));return 1;}
static Modlink *trustedload(Module *caller,WORD table,char *path)
{
    Modlink *ml;int call=servicecalls++;
    require(caller==servicecaller && table==0 && caller->ldt && caller->ldt[0]);
    if(call==1 && servicefault==2)error("loader failed");
    if(servicefault==3 || (call==1 && servicefault==1))return nil;
    if(servicefault==4 || (call==1 && servicefault==5))return H;
    if(strcmp(path,"/dis/probe/strings") && strcmp(path,"/дис/🚀"))return nil;
    serviceprovider->ref++;ml=linkmod(serviceprovider,caller->ldt[table],1);if(ml==H)return nil;
    if(!kprotect(pininstance,ml)){destroy(ml);return nil;}
    servicepins++;require(call<2);loaded[call]=ml;D2H(ml)->ref++; /* Independent observer, not the publication pin. */
    return ml;
}
static unsigned loadcall(Inst *p,unsigned ordinal,int arg,unsigned source,unsigned result)
{
    unsigned n=0;callinstruction(p+n++,IMFRAME,AFP,AFP,64,40);p[0].add|=AXIMM;p[0].reg=ordinal;
    if(arg)callargument(p+n++,IMOVP,AFP,source,40,64);
    if(result){callargument(p+n,ILEA,AFP,result,40,32);p[n++].reg=4;}
    callinstruction(p+n,IMCALL,AFP,AFP,40,64);p[n].add|=AXIMM;p[n++].reg=ordinal;return n;
}
static Modlink *loadinstance(Module *m,Import *imports)
{
    Modlink *ml;m->ref++;ml=linkmod(m,imports,1);require(ml!=H);return ml;
}
static int observedindex;
static Modlink *indexload(Module *caller,WORD index,char *path)
{
    require(caller->ldt[index] && caller->ldt[index+1]==nil && !path[0]);observedindex=index;return nil;
}
static int directload(void *unused){USED(unused);iload();return 1;}
static void loadindices(void)
{
    Native64ModuleSpec s=client_spec;Native64ModuleFunction fn[4];Native64TypedStep code[4];Native64ModuleTable tables[64];
    Native64ModuleOwner owner={0};Modlink *ml,*destination=H;Import empty[1]={{0}};String *path=H;WORD index;REG prior=R,saved;unsigned n,i;
    s.functions=fn;s.tables=tables;s.load=indexload;memmove(fn,client_functions,sizeof(fn));
    memmove(code,fn[client_open].code,fn[client_open].count*sizeof(*code));fn[client_open].code=code;
    for(i=0;i<64;i++)tables[i]=client_tables[0];
    for(n=2;n<=64;n+=62){
        s.ntables=n;index=n-1;code[0].m=index;require(native64typedmodule(&s,0,&owner));ml=loadinstance(owner.module,empty);
        R=prior;R.M=ml;R.MP=ml->MP;R.s=&path;R.m=&index;R.d=&destination;saved=R;observedindex=-1;
        require(kprotect(directload,nil) && observedindex==index && destination==H && !memcmp(&saved,&R,sizeof(R)));
        owner.module->native64importload=(Modlink*(*)(Module*,WORD,char*))H;
        require(!kprotect(directload,nil) && !strcmp(message,exModule) && destination==H && !memcmp(&saved,&R,sizeof(R)));
        R=prior;destroy(ml);require(native64typedmoduleclear(&owner) && !live && !rootcount);
    }
    printf("PASS LOAD private table indices: 1/63, nil publication and invalid callback preserve complete REG\n");
}
static void loadmetadata(void)
{
    Native64ModuleSpec s;Native64ModuleFunction fn[4];Native64TypedStep code[4];Native64ModuleOwner owner={0};
    uchar data[8],frame[8];int i,j,start,attempts;REG saved=R;
    require(client_functions[client_open].code[0].op==ILOAD);
    for(j=0;j<2;j++){
        start=allocations;require(native64typedmodule(&client_spec,j,&owner));attempts=allocations-start;
        require(owner.module->native64importload==trustedload && native64typedmoduleclear(&owner));
        for(i=1;i<=attempts;i++){
            failat=allocations+i;require(!native64typedmodule(&client_spec,j,&owner));failat=0;
            require(!live && !rootcount && !owner.module && !owner.self && !memcmp(&R,&saved,sizeof(R)));
        }
        printf("PASS LOAD constructor: jit=%d %d allocation failures, callback publication and unchanged REG\n",j,attempts);
    }
    for(i=0;i<14;i++){
        s=client_spec;memmove(fn,client_functions,sizeof(fn));s.functions=fn;memmove(code,fn[client_open].code,fn[client_open].count*sizeof(*code));fn[client_open].code=code;
        memmove(data,s.datamap,s.mapbytes);s.datamap=data;memmove(frame,fn[client_open].map,fn[client_open].mapbytes);fn[client_open].map=frame;
        if(i==0)s.load=nil;if(i==1)s.load=(Modlink*(*)(Module*,WORD,char*))H;
        if(i==2)code[0].m=-1;if(i==3)code[0].m=s.ntables;if(i==4)code[0].m=64;
        if(i==5)frame[1]^=0x80;
        if(i==6)data[client_provider_offset/64]^=0x80>>((client_provider_offset/8)%8);
        if(i==7)code[0].s++;if(i==8)code[0].d++;
        if(i==9)code[0].mode=SRC(AIMM)|AXIMM|DST(AMP);
        if(i==10)code[0].mode=SRC(AFP)|DST(AMP);
        if(i==11)code[0].returned=4;if(i==12)code[0].s=32;
        if(i==13){code[0].mode=SRC(AFP)|AXIMM|DST(AFP);code[0].d=32;}
        start=allocations;
        if(native64typedmodule(&s,0,&owner)){fprintf(stderr,"LOAD metadata guard omitted: case=%d\n",i);exit(10);}
        require(start==allocations && !live && !rootcount && !owner.module && !memcmp(&R,&saved,sizeof(R)));
    }
    printf("PASS LOAD metadata: 14 no-allocation callback/table/String/module root and operand refusals\n");
}
static void loadgraph(unsigned kind,const Input *input,int pjit,int cjit,int grow,int quantum,int unicode)
{
    Native64ModuleOwner provider={0},client={0};Module *pm,*cm,*m;Modlink *consumer,*dormant,*idle,*link,*selected;
    Import requests[5]={{0}};Prog process;Frame *entry,*source;String *value=H,*path,*seen;uchar *root,map[]={0,0xe0};
    unsigned i,n=0;WORD word;void *result;int before,done,first=kind<3 || kind==5 || kind==6,twice=kind==1,failed=kind>=2;
    uvlong guard=0x7766554433221100ULL,after;int baselinepins;
    servicefault=kind==2?1:kind==3?3:kind==4?4:kind==5?2:kind==6?5:0;servicecalls=servicepins=0;loaded[0]=loaded[1]=nil;
    require(native64typedmodule(&spec,pjit,&provider) && native64typedmodule(&client_spec,cjit,&client));pm=provider.module;cm=client.module;
    serviceprovider=pm;servicecaller=cm;baselinepins=rootcount;
    idle=loadinstance(pm,cm->ldt[0]);
    for(i=0;i<4;i++){requests[i].name=(char*)client_functions[i].name;memmove(&requests[i].sig,&client_functions[i].signature,4);}
    consumer=loadinstance(cm,requests);dormant=loadinstance(cm,requests);
    m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->name=strdup("LoadLauncher");m->path=strdup("trusted/LoadLauncher");
    m->ss=grow?0:4096;m->cstep=native64typedlea8step;m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->type!=nil);
    m->type[0]=dtype(freeheap,8,nil,0);m->type[1]=dtype(freeheap,128,map,2);require(m->type[0] && m->type[1]);m->entryt=m->type[1];
    m->origmp=H2D(uchar*,heapz(m->type[0]));m->prog=mallocz(40*sizeof(Inst),1);require(m->prog!=nil);m->entry=m->prog;
    n+=loadcall(m->prog+n,client_open,1,72,0);
    if(first)n+=loadcall(m->prog+n,client_run,1,80,88);
    if(kind==1 || kind==2 || kind==5 || kind==6)n+=loadcall(m->prog+n,client_open,1,72,0);
    if(twice)n+=loadcall(m->prog+n,client_run,1,80,96);
    if(kind!=5)n+=loadcall(m->prog+n,client_forward,1,80,104);
    callinstruction(m->prog+n++,IEXIT,AXXX,AXXX,0,0);m->nprog=n;if(cjit)require(compile(m,m->nprog,nil));
    memset(&process,0,sizeof(process));m->ref++;link=mklinkmod(m,0);link->MP=m->origmp;D2H(link->MP)->ref++;
    process.R.M=link;process.R.PC=m->entry;entry=mallocz(m->entryt->size,1);require(entry!=nil);entry->t=m->entryt;initmem(entry->t,entry);source=entry;R.s=&source;newstack(&process);free(entry);root=process.R.FP;
    path=unicode?c2string("/дис/🚀",sizeof("/дис/🚀")-1):c2string("/dis/probe/strings",sizeof("/dis/probe/strings")-1);
    if(kind==7){destroy(path);path=H;}
    if(!input->nilvalue)value=c2string(input->text,input->bytes);
    memmove(root+64,&consumer,8);memmove(root+72,&path,8);memmove(root+80,&value,8);memset(root+88,0x5a,24);memmove(root+112,&guard,8);result=root+88;memmove(root+48,&result,8);
    before=raised;message[0]='x';process.quanta=quantum;
    for(i=0;i<10000;i++){done=kprotect(execute,&process);if(!done)break;}
    if(i>=10000 || (failed?strcmp(message,kind==5?"loader failed":exModule):message[0]))fprintf(stderr,"LOAD graph kind=%u input=%u jit=%d/%d grow=%d q=%d unicode=%d error=%s\n",kind,(unsigned)(input-inputs),pjit,cjit,grow,quantum,unicode,message);
    require(i<10000 && raised==before+1 && !worker.nerr && (failed?!strcmp(message,kind==5?"loader failed":exModule):!message[0]));
    if(!failed)require(R.M==link && R.MP==link->MP && R.FP==root);
    if(first){memmove(&word,root+88,4);require(word==2*input->expected);}else require(!memcmp(root+88,"ZZZZ",4));
    if(twice){memmove(&word,root+96,4);require(word==2*input->expected);}else require(!memcmp(root+96,"ZZZZ",4));
    if(!failed){memmove(&word,root+104,4);require(word==input->expected);}else require(!memcmp(root+104,"ZZZZ",4));
    for(i=0;i<3;i++)require(!memcmp(root+92+8*i,"ZZZZ",4));memmove(&after,root+112,8);require(after==guard);
    require(servicecalls==((kind==1 || kind==2 || kind==5 || kind==6)?2:1));require(servicepins==(kind==1?2:first?1:0));
    /* Publication consumes the temporary pin; source and client owners remain. */
    if(rootcount!=baselinepins){fprintf(stderr,"LOAD publication pin leaked\n");exit(10);}
    memmove(&selected,consumer->MP+client_provider_offset,8);require(selected==(kind==0 || kind==5?loaded[0]:kind==1?loaded[1]:H));
    memmove(&word,consumer->MP+client_calls_offset,4);require(word==(twice?19:first?18:17));
    memmove(&word,dormant->MP+client_calls_offset,4);require(word==17 && *(void**)(dormant->MP+client_provider_offset)==H);
    require(*(void**)(cm->origmp+client_provider_offset)==H && *(void**)(pm->origmp+current_offset)==H && *(void**)(idle->MP+current_offset)==H);
    for(i=0;i<2;i++)if(loaded[i]){
        memmove(&seen,loaded[i]->MP+current_offset,8);require(seen==value);
        require(D2H(loaded[i])->ref==(selected==loaded[i]?2:1));
    }
    destroystack(&R);require(m->ref==1);unload(m);destroy(dormant);destroy(idle);
    for(i=0;i<2;i++)if(loaded[i])destroy(loaded[i]);
    serviceprovider=servicecaller=nil;
    require(cm->ref==1 && pm->ref==1 && native64typedmoduleclear(&client) && native64typedmoduleclear(&provider));
    require(!live && !rootcount && Tstring.ref==1);
}
int main(void)
{
    unsigned k,v,p,c,g,q,u,cases=0;int quantum[]={1,3,1000};Tstring.free=freestring;
    optab[ILOAD]=iload;optab[IFRAME]=frame;optab[ICALL]=call;optab[IRET]=ret;optab[IMFRAME]=mframe;optab[IMCALL]=mcall;
    optab[IMOVW]=movw;optab[IMOVL]=movl;optab[IMOVP]=movp;optab[IADDW]=addw;optab[ILENC]=lenc;optab[IEXIT]=iexit;
    loadmetadata();loadindices();
    for(k=0;k<8;k++)for(v=0;v<nelem(inputs);v++)for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<nelem(quantum);q++)for(u=0;u<2;u++){
        loadgraph(k,inputs+v,p,c,g,quantum[q],u);cases++;
    }
    printf("PASS trusted source LOAD: %u C/JIT graph roots, real String conversion/linkmod/ILOAD, reload isolation, Unicode paths, nil/H/service errors, pin transfer and retirement\n",cases);return 0;
}
