#include "full.h"
static void paircheck(int ok,const char *why)
{if(!ok){fprintf(stderr,"imported pair predicate: %s\n",why);exit(10);}}
static Modlink *fullload(Module *m,WORD t,char *p)
{USED(m);USED(t);USED(p);paircheck(0,"unexpected LOAD");return H;}
static int paircalls;
static void pairproduce(void *frame)
{
 String **result,*value,*old;WORD flag;uint i;
 paircalls++;memmove(&flag,(uchar*)frame+64,4);
 if(flag)error("pairfault");
 memmove(&result,(uchar*)frame+32,8);
 for(i=0;i<2;i++){
  memmove(&value,(uchar*)frame+72+8*i,8);
  if(value!=H)D2H(value)->ref++;
  memmove(&old,result+i,8);memmove(result+i,&value,8);destroy(old);
 }
}
static Modlink *pairlink(Module *m,const char *name,uint signature)
{
 Import request[2]={{0}};Modlink *ml;
 request[0].name=(char*)name;memmove(&request[0].sig,&signature,4);
 m->ref++;ml=linkmod(m,request,1);require(ml!=nil&&ml!=H);return ml;
}
static Module *pairbuiltin(void)
{
 const Native64ModuleImport *r=full_tables[0].imports;Module *m=mallocz(sizeof(*m),1);
 require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;
 m->name=strdup("pair-native");m->path=strdup("pair-native");
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);m->ext=mallocz(2*sizeof(Link),1);
 require(m->name&&m->path&&m->type&&m->ext);
 m->type[0]=dtype(freeheap,r->bytes,(uchar*)r->map,r->mapbytes);require(m->type[0]!=nil);
 m->ext[0].name=strdup(r->name);require(m->ext[0].name!=nil);
 memmove(&m->ext[0].sig,&r->signature,4);m->ext[0].frame=m->type[0];m->ext[0].u.runt=pairproduce;
 return m;
}
static Module *pairroot(int fault,int grow)
{
 Module *m=mallocz(sizeof(*m),1);Inst *p;uchar map[]={0,0xec};uint pc=0,k,i;
 require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;
 m->name=strdup("pair-caller");m->path=strdup("pair-caller");m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->name&&m->path&&m->type);
 m->entryt=m->type[0]=dtype(freeheap,128,map,2);require(m->entryt!=nil);
 p=m->prog=mallocz(13*sizeof(*p),1);require(p!=nil);m->entry=p;
 for(k=0;k<2;k++){
  p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=80;p[pc++].d.ind=40;
  p[pc].op=IMOVW;p[pc].add=SRC(AIMM)|DST(AFP|AIND);p[pc].s.imm=fault;p[pc].d.i.f=40;p[pc++].d.i.s=64;
  for(i=0;i<2;i++){p[pc].op=IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64+8*i;p[pc].d.i.f=40;p[pc++].d.i.s=72+8*i;}
  p[pc].op=ILEA;p[pc].reg=48;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=96;p[pc].d.i.f=40;p[pc++].d.i.s=32;
  p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=80;
 }
 p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);require(pc==13);m->nprog=pc;return m;
}
static void pairrun(uint fn,int jit,int producer,int quantum,uint mode)
{
 Native64ModuleOwner owner={0},provider={0};Module *builtin=nil,*m;Modlink *sink,*ml;
 String *objects[4],*inputs[2],*wanted[2],*value;Prog *p;uchar *root;uint i,j,steps;int done,expected;
 const char *failure=mode==5?exModule:mode==6?"pairfault":"";
 paircalls=0;require(native64typedmodule(&full_spec,jit,&owner));
 if(mode!=5){
  if(!producer){builtin=pairbuiltin();sink=pairlink(builtin,full_tables[0].imports[0].name,full_tables[0].imports[0].signature);}
  else {require(native64typedmodule(&full_spec,producer==2,&provider));sink=pairlink(provider.module,"produce",full_tables[0].imports[0].signature);}
  memmove(owner.module->origmp+pair_global,&sink,8);
 }
 objects[0]=c2string("left",4);objects[1]=c2string("право",10);
 objects[2]=c2string("old-left",8);objects[3]=c2string("old-right",9);
 inputs[0]=mode==1||mode==2?H:objects[0];inputs[1]=mode==1||mode==4?H:objects[mode==3?0:1];
 ml=pairlink(owner.module,full_functions[fn].name,full_functions[fn].signature);
 m=pairroot(mode==6,quantum==1);if(jit)require(compile(m,m->nprog,nil));
 m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;
 memmove(root+80,&ml,8);
 for(i=0;i<2;i++){
  if(inputs[i]!=H)D2H(inputs[i])->ref++;memmove(root+64+8*i,inputs+i,8);
  D2H(objects[i+2])->ref++;memmove(root+96+8*i,objects+i+2,8);wanted[i]=*failure?objects[i+2]:inputs[i];
 }
 memset(root+112,0x5a,16);p->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 if(steps>=10000||strcmp(worker.env->errstr,failure))fprintf(stderr,"pair fn=%u jit=%d producer=%d q=%d mode=%u error=%s expected=%s\n",fn,jit,producer,quantum,mode,worker.env->errstr,failure);
 paircheck(steps<10000&&!strcmp(worker.env->errstr,failure),"execution and exact exception");
 paircheck(paircalls==(!producer&&mode!=5?(mode==6?1:2):0),"C producer dispatch count");
 for(i=0;i<2;i++){memmove(&value,root+96+8*i,8);paircheck(value==wanted[i],"both exact result pointers");}
 for(i=112;i<128;i++)paircheck(root[i]==0x5a,"result guard bytes");
 if(!*failure)for(i=0;i<4;i++){
  expected=1;for(j=0;j<2;j++){if(inputs[j]==objects[i])expected++;if(wanted[j]==objects[i])expected++;}
  paircheck(D2H(objects[i])->ref==expected,"exact result ownership after repeated calls");
 }
 if(p->exval!=H){REG saved=R;paircheck(!handler(worker.env->errstr),"unhandled provider exception");paircheck(p->exval==H&&!memcmp(&R,&saved,sizeof(R)),"published exception retired");}
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);
 require(native64typedmoduleclear(&owner));
 if(producer&&mode!=5)require(native64typedmoduleclear(&provider));
 if(builtin){require(builtin->ref==1);unload(builtin);}
 for(i=0;i<4;i++){paircheck(D2H(objects[i])->ref==1,"fixture retained one owner");destroy(objects[i]);}
 paircheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"complete caller and provider retirement");
}
static void pairrefusals(void)
{
 Native64ModuleSpec spec;Native64ModuleFunction functions[6];Native64ModuleTable table;
 Native64ModuleImport request;Native64ModuleSite site;Native64TypedStep code[64];Native64ModuleOwner owner={0};
 uchar map[32];uint k,fn,pc,bit;int before;REG saved=R;
 for(k=0;k<24;k++){
  fn=pair_functions[k<12?0:1];spec=full_spec;memmove(functions,full_functions,sizeof(functions));spec.functions=functions;
  require(functions[fn].count<=nelem(code)&&functions[fn].mapbytes<=nelem(map)&&functions[fn].nsites==1);
  memmove(code,functions[fn].code,functions[fn].count*sizeof(*code));functions[fn].code=code;
  memmove(map,functions[fn].map,functions[fn].mapbytes);functions[fn].map=map;
  site=functions[fn].sites[0];functions[fn].sites=&site;pc=site.end-1;
  table=full_tables[0];request=table.imports[0];table.imports=&request;spec.tables=&table;
  switch(k%12){
  case 0:request.resultcontract=0;break;
  case 1:request.resultcontract=16;break;
  case 2:site.contract=24;break;
  case 3:site.inherited=!site.inherited;break;
  case 4:code[pc].s+=8;break;
  case 5:code[pc].d=40;break;
  case 6:code[pc].m=8;break;
  case 7:code[pc].returned=0;break;
  case 8:code[pc].op=INOP;break;
  case 9:if(k<12)functions[fn].resultcontract=0;else {bit=(code[pc].s+8)/8;map[bit/8]&=~(128>>(bit%8));}break;
  case 10:site.end++;break;
  case 11:if(k<12)code[site.end].returned=0;else {bit=code[pc].s/8;map[bit/8]&=~(128>>(bit%8));}break;
  }
  before=allocations;paircheck(!native64typedmodule(&spec,0,&owner),"malformed import result metadata refused");
  paircheck(allocations==before&&!owner.module&&!live&&!memcmp(&R,&saved,sizeof(R)),"metadata refused before allocation");
 }
 puts("PASS imported pair metadata: 24 import/site/result/map/binding refusals before allocation");
}
static void pairmaterializer(void)
{
 uint fn=pair_functions[0];const Native64ModuleFunction *f=full_functions+fn;const Native64ModuleSite *s=f->sites;
 const Native64ModuleImport *r=full_tables[0].imports;Type *t=dtype(freeheap,r->bytes,(uchar*)r->map,r->mapbytes);
 Native64TypedCall call={s->first,s->end,t,nil,s->target,48,1,1,s->modulemode,s->moduleoffset,48,48};
 Inst out[64],saved[64];int before;require(t!=nil);memset(out,0xa5,sizeof(out));
 paircheck(native64typedmulticode(out,64,f->code,f->count,f->bytes,full_spec.databytes,&call,1),"import materializer baseline");
 memmove(saved,out,sizeof(out));before=allocations;call.resultcontract=0;
 paircheck(!native64typedmulticode(out,64,f->code,f->count,f->bytes,full_spec.databytes,&call,1),"callee pair proof required");
 call.resultcontract=48;call.callerresultcontract=0;
 paircheck(!native64typedmulticode(out,64,f->code,f->count,f->bytes,full_spec.databytes,&call,1),"caller pair proof required");
 paircheck(allocations==before&&!memcmp(out,saved,sizeof(out)),"materializer refusal atomicity");freetype(t);
 puts("PASS imported pair materializer: caller/callee proofs and atomic refusal");
}
static void pairconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&full_spec,jit,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&full_spec,jit,&owner));failat=0;paircheck(!owner.module&&!live&&!nlinkpins&&!memcmp(&R,&saved,sizeof(R)),"constructor complete rollback");}
  printf("PASS imported pair constructor: jit=%d %d allocation failures with rollback\n",jit,n);
 }
}
int main(void)
{
 uint f,mode,total=0;int jit,producer,q,quanta[]={1,3,1000};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));
 PAIR_HANDLERS
 optab[IMOVP]=actual_movp;optab[IMOVMP]=actual_movmp;optab[IRAISE]=actual_iraise;
 pairrefusals();pairmaterializer();pairconstruction();
 for(f=0;f<4;f++)for(jit=0;jit<2;jit++)for(producer=0;producer<3;producer++)for(q=0;q<3;q++)for(mode=0;mode<7;mode++){
  pairrun(pair_functions[f],jit,producer,quanta[q],mode);total++;
 }
 printf("PASS imported root pairs: %u C/JIT cases, C/interpreted/JIT providers, repeated calls, copied/inherited results, nil/alias roots, stack growth, exceptions and complete retirement\n",total);return 0;
}
