#include "typed-command-files.h"
static void commandcheck(int ok,const char *why){if(!ok){fprintf(stderr,"Command file predicate: %s\n",why);exit(10);}}
static Module *localowner;
static uint forwarded;
/* Observe the borrowed pointer before CALL can dereference a broken result.
 * Only the two private forwarding edges have this inherited contract. */
static void checkedlocalcall(void)
{
 uint i,offset=0;Frame *pending;Inst *target;void *from,*to;
 memmove(&target,R.d,sizeof(target));
 if(localowner)for(i=0;i<nelem(command_functions);i++){
  if(target==localowner->prog+offset && (i==command_resolve || i==command_prepare)){
   memmove(&pending,R.s,sizeof(pending));memmove(&from,R.FP+32,sizeof(from));memmove(&to,(uchar*)pending+32,sizeof(to));
   commandcheck(from==to,"inherited result address publication");forwarded++;
  }
  offset+=command_functions[i].count;
 }
 actual_call();
}
static void argvtext(String *text,const char *expected)
{
 require(text!=H && text!=nil && text->len==(int)strlen(expected) && !memcmp(text->Sascii,expected,strlen(expected)));
}
static void argvconstruction(void)
{
 Native64ModuleOwner owner={0};Native64ModuleSpec spec=command_spec;Native64ModuleString literals[4];char bytes[4][8];
 REG saved=R;uint i;String *text;int j,k,before,attempts;
 require(nelem(command_literal_offsets)==4 && spec.nstrings==4);
 for(i=0;i<4;i++){literals[i]=command_spec.strings[i];require(literals[i].bytes<sizeof(bytes[i]));memmove(bytes[i],literals[i].text,literals[i].bytes);literals[i].text=(uchar*)bytes[i];}
 spec.strings=literals;require(native64typedmodule(&spec,0,&owner));memset(bytes,0xa5,sizeof(bytes));
 for(i=0;i<4;i++){memmove(&text,owner.module->origmp+command_literal_offsets[i],sizeof(text));argvtext(text,command_literal_text[i]);}
 require(native64typedmoduleclear(&owner) && !live && !nlinkpins);
 for(j=0;j<2;j++){
  before=allocations;require(native64typedmodule(&command_spec,j,&owner));attempts=allocations-before;require(native64typedmoduleclear(&owner));
  for(k=1;k<=attempts;k++){failat=allocations+k;require(!native64typedmodule(&command_spec,j,&owner));failat=0;
   require(!owner.module && !owner.rooted && !live && !nlinkpins && !memcmp(&R,&saved,sizeof(R)));}
  printf("PASS argv literal constructor: jit=%d %d allocation refusals, owned poisoned-input copies and unchanged REG\n",j,attempts);
 }
}
static Modlink *commandinstance(Module *m)
{
 Import request[4]={{0}};Modlink *ml;request[0].name="open";request[1].name="run";
 memmove(&request[0].sig,&command_functions[command_open].signature,4);memmove(&request[1].sig,&command_functions[command_run].signature,4);
 if(command_argv){request[2].name="ready";memmove(&request[2].sig,&command_functions[command_ready].signature,4);}
 m->ref++;ml=linkmod(m,request,1);require(ml!=H);return ml;
}
static void commandpreflight(void)
{
 Native64ModuleOwner owner={0};Native64TypedImportRequest table;Module *m;Type *t;REG saved=R;
 uchar bytes[128];uint hash=command_hashes[0];int i,start,size,np,ref;char *name;
 require(sizeof(command_file_requests)<sizeof(bytes));require(native64typedmodule(&command_spec,0,&owner));m=owner.module;t=m->type[m->ntype-1];size=t->size;np=t->np;ref=m->ref;name=m->ldt[0][0].name;
 for(i=0;i<17;i++){
  table=command_file_tables[0];memmove(bytes,command_file_requests,sizeof(command_file_requests));table.request.imports=bytes;table.hashes=&hash;hash=command_hashes[0];
  if(i==0)t->size=72;if(i==1)t->map[1]=0;if(i==2)t->map[1]=0x80;if(i==3)t->map[1]=0x40;
  if(i==4)t->map[1]=0xe0;if(i==5)t->np=1;if(i==6)t->map[0]=0x80;if(i==7)hash^=1;
  if(i==8)bytes[20]^=1;if(i==9)bytes[16]^=1;if(i==10)bytes[3]=2;
  if(i==11)m->native64importload=nil;if(i==12)table.count=0;if(i==13)t->size=88;
  if(i==14)m->ref++;if(i==15)m->ldt[0][0].name="other";if(i==16)bytes[27]=3;
  start=allocations;
  if(install64typedimports(m,&table,1,0)){fprintf(stderr,"Command install guard omitted: case=%d\n",i);exit(10);}
  require(allocations==start && !m->native64imports && !memcmp(&R,&saved,sizeof(R)));
  t->size=size;t->np=np;t->map[0]=0;t->map[1]=0xc0;m->ref=ref;m->native64importload=read64ownedimport;m->ldt[0][0].name=name;
 }
 start=live;failat=allocations+1;require(!install64typedimports(m,command_file_tables,1,0));failat=0;
 require(live==start && !m->native64imports && !memcmp(&R,&saved,sizeof(R)));
 require(native64typedmoduleclear(&owner) && !live && !nlinkpins);
 printf("PASS Command installer: 17 no-allocation frame/root/hash/trial refusals, allocation rollback and unchanged REG\n");
}
static Module *commandlauncher(int jit,int grow,int skip)
{
 Module *m;Inst *p;uchar map[]={0,0xe4};uint n=0,i;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,128,map,2);require(m->type[0]!=nil);m->entryt=m->type[0];
 m->prog=p=mallocz(18*sizeof(Inst),1);require(p!=nil);m->entry=p;
 for(i=0;i<2;i++){
  p[n].op=IMFRAME;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=64;p[n++].d.ind=40;
  p[n].op=IMOVP;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=104;p[n].d.i.f=40;p[n++].d.i.s=64;
  p[n].op=IMCALL;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=40;p[n++].d.ind=64;
 }
 if(command_argv){
  p[n].op=IMFRAME;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].reg=2;p[n].s.ind=64;p[n++].d.ind=40;
  p[n].op=ILEA;p[n].reg=4;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=96;p[n].d.i.f=40;p[n++].d.i.s=32;
  p[n].op=IMCALL;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].reg=2;p[n].s.ind=40;p[n++].d.ind=64;
 }
 if(!skip)for(i=0;i<(command_argv?2:1);i++){
  uint a;
  p[n].op=IMFRAME;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].reg=1;p[n].s.ind=64;p[n++].d.ind=40;
  for(a=0;a<2;a++){p[n].op=IMOVP;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=72+8*a;p[n].d.i.f=40;p[n++].d.i.s=64+8*a;}
  p[n].op=IMCALL;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].reg=1;p[n].s.ind=40;p[n++].d.ind=64;
 }
 p[n].op=IEXIT;p[n++].add=SRC(AXXX)|DST(AXXX);m->nprog=n;if(jit)require(compile(m,m->nprog,nil));return m;
}
static void commandrun(int pjit,int cjit,int grow,int quantum,int input,int context,int mode)
{
 Native64ModuleOwner owner={0};Native64TypedImportRequest table=command_file_tables[0];Module *cm,*outer;Modlink *consumer,*provider;
 uchar request[128],names[4096],*root,firstbyte;uint hash=command_hashes[0],i;String *path,*a,*b;List *argv=H,*node;void *ctxt=H;
 Type *ct=nil,*child=nil;Prog *process;WORD calls;uvlong guard=0xf0e1d2c3b4a59687ULL,after;int opens,admits,ctors,pins,steps,done;
 int skip=mode==8,iofail=(mode>0 && mode<6)||mode==7;const char *expected=iofail?exModule:!skip && (command_fault || (!command_argv && input==2))?exZdiv:"";
 require(sizeof(command_file_requests)<=sizeof(request) && sizeof(command_file_names)<=sizeof(names));
 memmove(request,command_file_requests,sizeof(command_file_requests));memmove(names,command_file_names,sizeof(command_file_names));table.request.imports=request;table.request.strings=names;table.hashes=&hash;
 require(native64typedmodule(&command_spec,cjit,&owner));cm=owner.module;require(install64typedimports(cm,&table,1,pjit));pins=nlinkpins;
 localowner=command_local?cm:nil;forwarded=0;
 consumer=commandinstance(cm);outer=commandlauncher(cjit,grow,skip);memset(request,0xa5,sizeof(request));memset(names,0xa5,sizeof(names));hash=0;
 starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;
 path=newstring(13);memmove(path->Sascii,"library.dis64",14);memmove(root+64,&consumer,sizeof(consumer));memmove(root+104,&path,sizeof(path));
 if(input==1){a=newrunes(3);a->Srune[0]=0x4e16;a->Srune[1]=0;a->Srune[2]=0x1f680;}
 else{a=newstring(3);memmove(a->Sascii,"w\0m",3);}
 b=newstring(2);memmove(b->Sascii,"-p",2);
 if(input!=2){
  R.s=&b;R.d=&argv;actual_consp();R.s=&a;R.d=&argv;actual_consp();
  commandcheck(D2H(a)->ref==2 && D2H(b)->ref==2,"argv head retain omitted");
 }
 if(context){
  uchar map[]={0xe0};ct=dtype(freeheap,24,map,1);child=dtype(freeheap,8,nil,0);require(ct!=nil && child!=nil);ctxt=H2D(void*,heapz(ct));
  for(i=0;i<3;i++){void *v=H2D(void*,heapz(child));memmove((uchar*)ctxt+8*i,&v,sizeof(v));}
 }
 memmove(root+72,&ctxt,sizeof(ctxt));memmove(root+80,&argv,sizeof(argv));memset(root+96,0xa5,8);memset(root+112,0xa5,8);memmove(root+120,&guard,8);
 firstbyte=((uchar*)filesource)[0];namespacefault=mode==1?1:mode==3?5:mode==7?9:0;admissionrequired=mode==2;librarypinfail=mode==5;dontcompile=mode==6;
 if(mode==4)((uchar*)filesource)[0]^=0x20;opens=fileopens;admits=admissioncalls;ctors=namespaceconstructors;process->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}
 ((uchar*)filesource)[0]=firstbyte;namespacefault=admissionrequired=librarypinfail=dontcompile=0;
 if(steps>=10000 || strcmp(worker.env->errstr,expected))fprintf(stderr,"Command graph fault=%u jit=%d/%d grow=%d q=%d input=%d context=%d mode=%d error=%s expected=%s\n",command_fault,pjit,cjit,grow,quantum,input,context,mode,worker.env->errstr,expected);
 commandcheck(steps<10000 && !strcmp(worker.env->errstr,expected),"source LOAD/init result");
 if(!expected[0]){
  require(R.FP==root);if(context)require(D2H(ctxt)->ref==1);
  if(command_argv){
   List *prepared;memmove(&prepared,consumer->MP+command_prepared_offset,sizeof(prepared));
   if(skip)require(prepared==H);
   else{
    commandcheck(prepared!=H,"argv MP publication");require(D2H(prepared)->ref==1);node=prepared;
    if(input==2){argvtext(*(String**)node->data,"-l");require(node->tail==H);}
    else{for(i=0;i<3;i++){argvtext(*(String**)node->data,i==0?"sh":i==1?"-c":"$*");node=node->tail;}require(node==argv);}
   }
   for(i=0;i<nelem(command_literal_offsets);i++){String *text;int used=!skip && ((input==2)==!strcmp(command_literal_text[i],"-l"));
    memmove(&text,cm->origmp+command_literal_offsets[i],sizeof(text));argvtext(text,command_literal_text[i]);require(D2H(text)->ref==2+used);}
  }
  if(input!=2){require(D2H(argv)->ref==(command_argv && !skip?2:1) && D2H(a)->ref==2 && D2H(b)->ref==2);node=argv;require(node->tail!=H && node->tail->tail==H);}
 }
 if(command_argv){WORD ready;memmove(&ready,root+96,4);require(ready==!iofail);require(!memcmp(root+100,"\xa5\xa5\xa5\xa5",4));}
 memmove(&calls,consumer->MP+command_calls_offset,4);require(calls==(skip?17:command_argv && !expected[0]?19:18));
 memmove(&calls,cm->origmp+command_calls_offset,4);require(calls==17 && *(void**)(cm->origmp+command_provider_offset)==H);if(command_argv)require(*(void**)(cm->origmp+command_prepared_offset)==H);
 require(nlinkpins==pins && !fileopen && fileopens==filecloses && fileopens==opens+(mode==1?0:2));
 if(mode==1 || mode==3 || mode==7)require(admissioncalls==admits && namespaceconstructors==ctors);
 else if(mode==2)require(admissioncalls==admits+2 && namespaceconstructors==ctors);
 else require(admissioncalls==admits+2 && namespaceconstructors==ctors+2);
 if(!iofail){memmove(&provider,consumer->MP+command_provider_offset,sizeof(provider));require(provider!=H && provider->m->entry==nil && provider->m->entryt==nil && (provider->m->jit!=nil)==(pjit && mode!=6));}
 if(command_local && !cjit)require(forwarded==(skip?2:expected[0]?3:4));
 for(i=0;i<8;i++)require(root[112+i]==0xa5);memmove(&after,root+120,8);require(after==guard);
 localowner=nil;observed=process;progexit();emptyqueues();require(cm->ref==1 && native64typedmoduleclear(&owner));
 require(D2H(a)->ref==1 && D2H(b)->ref==1);destroy(a);destroy(b);
 if(ct){require(ct->ref==1 && child->ref==1);freetype(ct);freetype(child);}
 require(!live && !nlinkpins && !modules && !worker.nerr && Tstring.ref==1 && Tmodlink.ref==1 && Tlist.ref==1 && Tptr.ref==1);
 memset(nativeobservations,0,sizeof(nativeobservations));
}
int main(int argc,char **argv)
{
 static uchar image[1048576];uint bytes;int p,c,g,q,v,t,mode,cases=0;int quantum[]={1,3,1000};require(argc==2 || argc==3);
 bytes=libraryread(argv[1],image,sizeof(image));filesource=image;filebytes=bytes;
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));commandpreflight();if(argc==3)return 0;if(command_argv)argvconstruction();
 optab[ILOAD]=actual_iload;optab[IMOVP]=actual_movp;optab[ICONSP]=actual_consp;optab[IHEADP]=actual_headp;optab[ITAIL]=actual_tail;optab[ILENL]=actual_lenl;optab[ILENC]=actual_lenc;
 COMMAND_HANDLERS
 if(command_local)optab[ICALL]=checkedlocalcall;
 for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(v=0;v<3;v++)for(t=0;t<2;t++)for(mode=0;mode<9;mode++){
  commandrun(p,c,g,quantum[q],v,t,mode);cases++;
 }
 printf("PASS checked LOAD -> file Command.init: %d C/JIT roots, argv ASCII/Rune/NUL/nil, nullable Context, fresh reloads, explicit init/no auto-init and complete managed retirement\n",cases);if(command_argv)printf("PASS source startup argv: exact -l/sh/-c/$* literals, nil-list/module branches, prepared MP aliases and explicit Limbo init\n");if(command_local)printf("PASS private LOAD calls: fresh and forwarded Module/List results, replacement and complete retirement\n");return 0;
}
