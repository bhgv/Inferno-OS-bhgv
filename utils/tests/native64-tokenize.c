#include "full.h"
static Modlink*fullload(Module*m,WORD i,char*p){USED(m);USED(i);USED(p);return H;}
static void tokencheck(int ok,char*why){if(!ok){fprintf(stderr,"tokenize predicate: %s\n",why);exit(10);}}
static int tokenword(String*s,const char*text){String*expected=c2string((char*)text,strlen(text));int ok=s!=H && s->len==expected->len && !memcmp(s->len<0?(void*)s->Srune:(void*)s->Sascii,expected->len<0?(void*)expected->Srune:(void*)expected->Sascii,s->len<0?-s->len*sizeof(Rune):s->len);destroy(expected);return ok;}
static Module*tokenbuiltin(void){
 Module*m=mallocz(sizeof(*m),1);const Native64ModuleImport*r=full_import_0;
 require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->name=strdup("$Sys");m->path=strdup("$Sys");
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,r->bytes,(uchar*)r->map,r->mapbytes);require(m->type[0]!=nil);
 m->ext=mallocz(2*sizeof(Link),1);require(m->ext!=nil);m->ext[0].name=strdup("tokenize");m->ext[0].sig=r->signature;m->ext[0].frame=m->type[0];m->ext[0].u.runt=Sys_tokenize;return m;
}
static Modlink*tokenlink(Module*m,char*name,uint sig){Import req[2]={{0}};Modlink*l;req[0].name=name;memmove(&req[0].sig,&sig,4);m->ref++;l=linkmod(m,req,1);require(l!=H&&l!=nil);return l;}
static Module*tokenlauncher(int jit){
 Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,128};require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->entryt=m->type[0]=dtype(freeheap,80,map,2);require(m->entryt!=nil);
 p=m->prog=mallocz(3*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=3;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=64;p[0].d.ind=40;
 p[1].op=IMCALL;p[1].add=SRC(AFP)|AXIMM|DST(AFP);p[1].s.ind=40;p[1].d.ind=64;
 p[2].op=IEXIT;p[2].add=SRC(AXXX)|DST(AXXX);if(jit)require(compile(m,3,nil));return m;
}
static void tokencase(int jit,int quantum,int which){
 static const char*input[]={" ,alpha,,beta gamma, ","alpha","",",,,","a,b","мирé🚀"};
 static const char*seps[]={" ,",",",",",",","","é"};
 static const char*expected[][3]={{"alpha","beta","gamma"},{"alpha",nil,nil},{nil,nil,nil},{nil,nil,nil},{"a,b",nil,nil},{"мир","🚀",nil}};
 Native64ModuleOwner owner={0};Module*b,*outer;Modlink*ml,*sys;Prog*p;String*s,*word;List*l;uchar*root;WORD n;uint steps,i;int done;
 require(native64typedmodule(&full_spec,jit,&owner));b=tokenbuiltin();sys=tokenlink(b,"tokenize",full_import_0[0].signature);memmove(owner.module->origmp+token_offsets[0],&sys,8);
 s=c2string((char*)input[which],strlen(input[which]));memmove(owner.module->origmp+token_offsets[1],&s,8);
 s=c2string((char*)seps[which],strlen(seps[which]));memmove(owner.module->origmp+token_offsets[2],&s,8);
 ml=tokenlink(owner.module,"run",full_functions[0].signature);D2H(ml)->ref++;
 outer=tokenlauncher(jit);starting=outer;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+64,&ml,8);p->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 if(worker.env->errstr[0])fprintf(stderr,"tokenize execution error=%s\n",worker.env->errstr);
 tokencheck(steps<10000&&!worker.env->errstr[0],"source call completion");
 memmove(&n,ml->MP+token_offsets[3],4);memmove(&l,ml->MP+token_offsets[4],8);
 for(i=0;i<3&&expected[which][i];i++){tokencheck(l!=H,"list length");memmove(&word,l->data,8);tokencheck(tokenword(word,expected[which][i]),"list values and order");l=l->tail;}
 tokencheck(l==H&&n==(WORD)i,"tuple count and complete list");observed=p;progexit();emptyqueues();destroy(ml);
 require(native64typedmoduleclear(&owner));require(b->ref==1);unload(b);
 if(live||nlinkpins||modules||worker.nerr)fprintf(stderr,"token retirement live=%d pins=%d modules=%p errors=%d strings=%d list=%d ptr=%d\n",live,nlinkpins,(void*)modules,worker.nerr,Tstring.ref,Tlist.ref,Tptr.ref);
 tokencheck(!live&&!nlinkpins&&!modules&&!worker.nerr,"complete retirement");
}
static void tokenmetadata(void){
 Native64ModuleSpec spec=full_spec;Native64ModuleFunction f[2];Native64ModuleImport request;
 Native64ModuleTable table;Native64TypedStep code[64];Native64ModuleSite site;
 Native64ModuleOwner owner={0};uchar map[64];unsigned int bit,base;int i,before;
 require(full_functions[0].count<=64 && full_functions[0].mapbytes<=64);
 for(i=0;i<8;i++){
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
  before=allocations;tokencheck(!native64typedmodule(&spec,0,&owner),"malformed tuple metadata refused");
  tokencheck(allocations==before&&!owner.module&&!owner.rooted,"metadata refusal before allocation");
 }
 puts("PASS imported mixed results: 8 map, contract, alignment and binding refusals before allocation");
}
int main(void){int j,q,c;int quantum[]={1,3,1000};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));
 TOKEN_OPTAB
 tokenmetadata();
 for(j=0;j<2;j++)for(q=0;q<3;q++)for(c=0;c<6;c++)tokencase(j,quantum[q],c);
 puts("PASS original Sys.tokenize: 36 C/JIT cases, scalar/list result, delimiter edges, exact count/order and complete retirement");return 0;
}
