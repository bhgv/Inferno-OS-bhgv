#include "full.h"
static Modlink*fullload(Module*m,WORD i,char*p){USED(m);USED(i);USED(p);return H;}
static void tokencheck(int ok,char*why){if(!ok){fprintf(stderr,"tokenize predicate: %s\n",why);exit(10);}}
static int tokenword(String*s,const char*text){
 String*expected=c2string((char*)text,strlen(text));int ok;
 if(s==H)ok=expected==H||expected->len==0;
 else if(expected==H)ok=s->len==0;
 else ok=s->len==expected->len && !memcmp(s->len<0?(void*)s->Srune:(void*)s->Sascii,expected->len<0?(void*)expected->Srune:(void*)expected->Sascii,s->len<0?-s->len*sizeof(Rune):s->len);
 destroy(expected);return ok;
}
static Module*tokenbuiltin(void){
 Module*m=mallocz(sizeof(*m),1);const Native64ModuleImport*r=full_import_0;
 require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->name=strdup("$Sys");m->path=strdup("$Sys");
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,r->bytes,(uchar*)r->map,r->mapbytes);require(m->type[0]!=nil);
 m->ext=mallocz(2*sizeof(Link),1);require(m->ext!=nil);m->ext[0].name=strdup("tokenize");m->ext[0].sig=r->signature;m->ext[0].frame=m->type[0];m->ext[0].u.runt=Sys_tokenize;return m;
}
static Modlink*tokenlink(Module*m,char*name,uint sig){Import req[2]={{0}};Modlink*l;req[0].name=name;memmove(&req[0].sig,&sig,4);m->ref++;l=linkmod(m,req,1);require(l!=H&&l!=nil);return l;}
static Module*tokenlauncher(int jit,int grow){
 Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,128};require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->entryt=m->type[0]=dtype(freeheap,80,map,2);require(m->entryt!=nil);
 p=m->prog=mallocz(5*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=5;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=64;p[0].d.ind=40;
 p[1].op=IMCALL;p[1].add=SRC(AFP)|AXIMM|DST(AFP);p[1].s.ind=40;p[1].d.ind=64;
 p[2]=p[0];p[3]=p[1];p[4].op=IEXIT;p[4].add=SRC(AXXX)|DST(AXXX);if(jit)require(compile(m,5,nil));return m;
}
static void tokencase(int jit,int quantum,int which,int grow){
 static const char*input[]={" ,alpha,,beta gamma, ","alpha","",",,,","a,b","мирé🚀"};
 static const char*seps[]={" ,",",",",",",","","é"};
 static const char*expected[][3]={{"alpha","beta","gamma"},{"alpha",nil,nil},{nil,nil,nil},{nil,nil,nil},{"a,b",nil,nil},{"мир","🚀",nil}};
 Native64ModuleOwner owner={0};Module*b,*outer;Modlink*ml,*sys;Prog*p;String*s,*word;List*l;uchar*root;WORD n;uint steps,i;int done;
 require(native64typedmodule(&full_spec,jit,&owner));b=tokenbuiltin();sys=tokenlink(b,"tokenize",full_import_0[0].signature);memmove(owner.module->origmp+token_offsets[0],&sys,8);
 s=c2string((char*)input[which],strlen(input[which]));memmove(owner.module->origmp+token_offsets[1],&s,8);
 s=c2string((char*)seps[which],strlen(seps[which]));memmove(owner.module->origmp+token_offsets[2],&s,8);
 ml=tokenlink(owner.module,"run",full_functions[0].signature);D2H(ml)->ref++;
 outer=tokenlauncher(jit,grow);starting=outer;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+64,&ml,8);p->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 if(worker.env->errstr[0])fprintf(stderr,"tokenize execution error=%s\n",worker.env->errstr);
 tokencheck(steps<10000&&!worker.env->errstr[0],"source call completion");
 memmove(&n,ml->MP+token_offsets[3],4);memmove(&l,ml->MP+token_offsets[4],8);
 for(i=0;i<3&&expected[which][i];i++){tokencheck(l!=H,"list length");memmove(&word,l->data,8);tokencheck(tokenword(word,expected[which][i]),"list values and order");l=l->tail;}
 if(TOKEN_ROOT_PAIR){memmove(&s,ml->MP+token_offsets[5],8);if(!tokenword(s,input[which])){fprintf(stderr,"root pair case jit=%d q=%d input=%d grow=%d actual=%s\n",jit,quantum,which,grow,s==H?"<nil>":string2c(s));tokencheck(0,"forwarded second root");}}
 tokencheck(l==H&&n==(WORD)i,"tuple count and complete list");observed=p;progexit();emptyqueues();destroy(ml);
 require(native64typedmoduleclear(&owner));require(b->ref==1);unload(b);
 if(live||nlinkpins||modules||worker.nerr)fprintf(stderr,"token retirement live=%d pins=%d modules=%p errors=%d strings=%d list=%d ptr=%d\n",live,nlinkpins,(void*)modules,worker.nerr,Tstring.ref,Tlist.ref,Tptr.ref);
 tokencheck(!live&&!nlinkpins&&!modules&&!worker.nerr,"complete retirement");
}
static void tokenmetadata(void){
 Native64ModuleSpec spec=full_spec;Native64ModuleFunction f[sizeof(full_functions)/sizeof(full_functions[0])];Native64ModuleImport request;
 Native64ModuleTable table;Native64TypedStep code[64],resultcode[64];Native64ModuleSite site;
 Native64ModuleOwner owner={0};uchar map[64];unsigned int bit,base;int i,before;
 require(full_functions[0].count<=64 && full_functions[0].mapbytes<=64);
 for(i=0;i<((TOKEN_IMPORTED_FORWARD?8:full_spec.nfunctions>2?15:8));i++){
  memmove(f,full_functions,sizeof(f));spec.functions=f;
  memmove(code,f[0].code,f[0].count*sizeof(*code));f[0].code=code;
  memmove(map,f[0].map,f[0].mapbytes);f[0].map=map;
  site=f[0].sites[0];f[0].sites=&site;base=code[site.end-1].s;
  request=full_import_0[0];table=(Native64ModuleTable){&request,1};spec.tables=&table;
  if(i<2){bit=(base+8*i)/8;map[bit/8]^=128>>(bit%8);}
  if(i==2)code[site.end-1].s+=4;
  if(i==3)request.resultcontract=0;
  if(i==4)site.contract=4;
  if(i==5)request.resultcontract=48;
  if(i==6)site.inherited=1;
  if(i==7)code[site.end-1].returned=85;
  if(i>=8){
   unsigned int pc;
   require(f[2].count<=64);memmove(resultcode,f[2].code,f[2].count*sizeof(*resultcode));f[2].code=resultcode;
   for(pc=0;pc<f[2].count && resultcode[pc].returned!=77;pc++);
   require(pc<f[2].count);
   if(i==8)resultcode[pc].m|=4;
   if(i==9)resultcode[pc].m^=8<<8;
   if(i==10){memmove(map,f[2].map,f[2].mapbytes);f[2].map=map;bit=(resultcode[pc].s+8)/8;map[bit/8]^=128>>(bit%8);}
   if(i==13)resultcode[pc]=(Native64TypedStep){INOP,SRC(AXXX)|DST(AXXX),0,0,0,0};
   if(i==14)resultcode[pc].m=((TOKEN_ROOT_PAIR?16:48)<<16)|(16<<8);
   if(i==11)f[3].resultcontract=0;
   if(i==12){site=f[3].sites[0];site.contract=24;f[3].sites=&site;}
  }
  before=allocations;tokencheck(!native64typedmodule(&spec,0,&owner),"malformed tuple metadata refused");
  if(allocations!=before)fprintf(stderr,"token metadata case=%d allocated=%d\n",i,allocations-before);
  tokencheck(allocations==before&&!owner.module&&!owner.rooted,"metadata refusal before allocation");
 }
 printf("PASS mixed results: %d map, contract, alignment, copy and forwarding refusals before allocation\n",(TOKEN_IMPORTED_FORWARD?8:full_spec.nfunctions>2?15:8));
}
static void tokenforwardmetadata(void){
 Native64ModuleSpec spec=full_spec;Native64ModuleFunction f[sizeof(full_functions)/sizeof(full_functions[0])];
 Native64ModuleImport request;Native64ModuleTable table;Native64ModuleSite site;
 Native64TypedStep code[64];Native64ModuleOwner owner={0};uint at;int i,before;
 if(!TOKEN_IMPORTED_FORWARD)return;
 for(i=0;i<11;i++){
  memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[2].count<=64);
  memmove(code,f[2].code,f[2].count*sizeof(*code));f[2].code=code;
  site=f[2].sites[0];require(site.external&&site.inherited&&site.contract==16);f[2].sites=&site;at=site.end-1;
  request=full_import_0[0];table=(Native64ModuleTable){&request,1};spec.tables=&table;
  if(i==0)f[2].resultcontract=0;
  if(i==1)request.resultcontract=0;
  if(i==2)site.contract=24;
  if(i==3)site.inherited=0;
  if(i==4)code[at].s=40;
  if(i==5)code[at].m=8;
  if(i==6)code[at].d=40;
  if(i==7)code[at].returned=144;
  if(i==8)code[at].op=ILEA;
  if(i==9)code[site.end].returned=0;
  if(i==10)code[at]=(Native64TypedStep){INOP,SRC(AXXX)|DST(AXXX),0,0,0,0};
  before=allocations;tokencheck(!native64typedmodule(&spec,0,&owner),"invalid imported forwarding refused");
  if(before!=allocations)fprintf(stderr,"import forwarding metadata case=%d allocated=%d\n",i,allocations-before);
  tokencheck(before==allocations&&!owner.module&&!owner.rooted,"imported forwarding refusal before allocation");
 }
 puts("PASS imported forwarding: 11 caller, import, contract and binding refusals before allocation");
}
static void tokenconstruction(void){
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&full_spec,jit,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){
   failat=allocations+i;require(!native64typedmodule(&full_spec,jit,&owner));failat=0;
   tokencheck(!owner.module&&!owner.rooted&&!live&&!nlinkpins&&!memcmp(&R,&saved,sizeof(R)),"mixed descriptor constructor rollback");
  }
  printf("PASS local mixed constructor: jit=%d %d allocation failures and complete rollback\n",jit,n);
 }
}
int main(void){int j,q,c,g;int quantum[]={1,3,1000};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));
 TOKEN_OPTAB
 optab[IMOVMP]=actual_movmp;optab[ISLICEC]=actual_slicec;
 tokenmetadata();
 tokenforwardmetadata();
 if(full_spec.nfunctions>2)tokenconstruction();
 for(j=0;j<2;j++)for(q=0;q<3;q++)for(c=0;c<6;c++)for(g=0;g<2;g++)tokencase(j,quantum[q],c,g);
 puts("PASS original Sys.tokenize: 72 C/JIT cases, repeated calls, stack growth, scalar/list result, delimiter edges, exact count/order and complete retirement");return 0;
}
