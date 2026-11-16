#include "typed-managed-files.h"
static void managedcheck(int ok,const char *why){if(!ok){fprintf(stderr,"managed file predicate: %s\n",why);exit(10);}}
/* Observe retain immediately, before a missing retain can cause a later UAF. */
static void listconsp(void)
{
 void *value;Heap *head;ulong ref=0;
 memmove(&value,R.s,sizeof(value));head=value==H?nil:D2H(value);if(head)ref=head->ref;
 actual_consp();
 if(head)managedcheck(head->ref==ref+1,"source cons head retain");
}
static void listtail(void)
{
 List *value,*tail=H;void *previous;ulong ref=0;
 memmove(&value,R.s,sizeof(value));memmove(&previous,R.d,sizeof(previous));
 if(value!=H){tail=value->tail;if(tail!=H)ref=D2H(tail)->ref;}
 actual_tail();
 if(tail!=H && previous==H)managedcheck(D2H(tail)->ref==ref+1,"source tail retain");
}
static void listpreflight(void)
{
 Native64ModuleSpec spec=managed_spec;Native64ModuleFunction functions[nelem(managed_functions)];Native64ModuleOwner owner={0};
 Native64TypedStep code[256],*step;uchar map[128];const Native64ModuleFunction *original=managed_functions+managed_round;
 REG saved=R;Inst probe,untouched;uint i,j,cons=~0U,head=~0U,tail=~0U,bit;int before;
 require(original->count<=nelem(code) && original->mapbytes<=sizeof(map));
 for(j=0;j<original->count;j++){
  if(original->code[j].op==ICONSP)cons=j;if(original->code[j].op==IHEADP)head=j;if(original->code[j].op==ITAIL)tail=j;
 }
 require(cons!=~0U && head!=~0U && tail!=~0U);
 for(i=0;i<12;i++){
  memmove(functions,managed_functions,sizeof(functions));memmove(code,original->code,original->count*sizeof(*code));memmove(map,original->map,original->mapbytes);
  spec.functions=functions;functions[managed_round].code=code;functions[managed_round].map=map;step=code+cons;
  if(i==0)step->mode=SRC(AIMM)|DST(UDST(step->mode));
  if(i==1)step->mode=SRC(USRC(step->mode))|DST(AIMM);
  if(i==2)step->s++;if(i==3)step->s=original->bytes-4;if(i==4)step->m=1;
  if(i==5)step->mode|=AXIMM;if(i==6){step->returned=24;step->mode=SRC(USRC(step->mode))|DST(AFP|AIND);step->d=32;}
  if(i==7){require(USRC(step->mode)==AFP);bit=step->s/8;map[bit/8]&=~(0x80>>(bit%8));}
  if(i==8){require(UDST(step->mode)==AFP);bit=step->d/8;map[bit/8]&=~(0x80>>(bit%8));}
  if(i==9){step=code+head;step->returned=8;step->mode=SRC(USRC(step->mode))|DST(AFP|AIND);step->d=32;}
  if(i==10){step=code+tail;step->returned=24;step->mode=SRC(USRC(step->mode))|DST(AFP|AIND);step->d=40;}
  if(i==11){step=code+head;step->mode=SRC(AFP|AIND)|DST(UDST(step->mode));}
  if(i!=7 && i!=8){memset(&probe,0xa5,sizeof(probe));untouched=probe;
   require(!native64typedcode(&probe,1,step,1,original->bytes,spec.databytes) && !memcmp(&probe,&untouched,sizeof(probe)));}
  before=allocations;
  if(native64typedmodule(&spec,0,&owner)){fprintf(stderr,"list constructor guard omitted: case=%u\n",i);exit(10);}
  require(!owner.module && !owner.rooted && allocations==before && !live && !nlinkpins && !memcmp(&saved,&R,sizeof(R)));
 }
 printf("PASS checked list metadata: 12 preallocation role/width/map/middle/result refusals and unchanged REG\n");
}
static Modlink *managedinstance(Module *m,uint method)
{
 Import request[3]={{0}};Modlink *ml;request[0].name="open";request[1].name=(char*)managed_functions[method].name;
 memmove(&request[0].sig,&managed_functions[managed_open].signature,4);memmove(&request[1].sig,&managed_functions[method].signature,4);
 m->ref++;ml=linkmod(m,request,1);require(ml!=H);return ml;
}
static void managedpreflight(void)
{
 Native64ModuleOwner owner={0};Native64TypedImportRequest table;Module *m;Type *t;REG saved=R;
 uint hash[2];int i,start,size,np,ref;char *name;int signature;
 require(native64typedmodule(&managed_spec,0,&owner));m=owner.module;t=m->type[m->ntype-2];size=t->size;np=t->np;ref=m->ref;name=m->ldt[0][0].name;signature=m->ldt[0][0].sig;
 for(i=0;i<13;i++){
  table=managed_tables[0];memmove(hash,managed_hashes,sizeof(hash));table.hashes=hash;
  if(i==0)table.managed=0;if(i==1)table.managed=2;if(i==2)t->size=80;if(i==3)t->np=1;
  if(i==4)t->map[0]=0x80;if(i==5)t->map[1]=0;if(i==6)t->map[1]=0xc0;
  if(i==7){hash[0]^=1;memmove(&m->ldt[0][0].sig,hash,4);}
  if(i==8)m->native64importload=nil;if(i==9)table.count=1;if(i==10)m->ref++;
  if(i==11)m->ldt[0][0].name="wrong";if(i==12)table.request.importbytes--;
  start=allocations;
  if(install64typedimports(m,&table,1,0)){fprintf(stderr,"managed install guard omitted: case=%d\n",i);exit(10);}
  require(allocations==start && !m->native64imports && !memcmp(&R,&saved,sizeof(R)));
  t->size=size;t->np=np;t->map[0]=0;t->map[1]=0x80;m->ref=ref;m->native64importload=read64ownedimport;m->ldt[0][0].name=name;m->ldt[0][0].sig=signature;
 }
 start=live;failat=allocations+1;require(!install64typedimports(m,managed_tables,1,0));failat=0;
 require(live==start && !m->native64imports && !memcmp(&R,&saved,sizeof(R)));
 require(native64typedmoduleclear(&owner) && !live && !nlinkpins);
 printf("PASS managed installer: 13 preallocation selector/hash/root/frame refusals, allocation rollback and unchanged REG\n");
}
static Module *managedlauncher(int jit,int grow,int skip)
{
 Module *m;Inst *p;uchar map[]={0,0xe4};uint n=0,i;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,128,map,2);require(m->type[0]!=nil);m->entryt=m->type[0];
 m->prog=p=mallocz(12*sizeof(Inst),1);require(p!=nil);m->entry=p;
 for(i=0;i<2;i++){
  p[n].op=IMFRAME;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=64;p[n++].d.ind=40;
  p[n].op=IMOVP;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=104;p[n].d.i.f=40;p[n++].d.i.s=64;
  p[n].op=IMCALL;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=40;p[n++].d.ind=64;
 }
 if(!skip){
  p[n].op=IMFRAME;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].reg=1;p[n].s.ind=64;p[n++].d.ind=40;
  p[n].op=ILEA;p[n].reg=24;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=80;p[n].d.i.f=40;p[n++].d.i.s=32;
  p[n].op=IMOVP;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=72;p[n].d.i.f=40;p[n++].d.i.s=64;
  p[n].op=IMCALL;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].reg=1;p[n].s.ind=40;p[n++].d.ind=64;
 }
 p[n].op=IEXIT;p[n++].add=SRC(AXXX)|DST(AXXX);m->nprog=n;if(jit)require(compile(m,m->nprog,nil));return m;
}
static void managedrun(int pjit,int cjit,int grow,int quantum,int input,uint method,int mode)
{
 Native64ModuleOwner owner={0};Native64TypedImportRequest table=managed_tables[0];Module *cm,*outer;Modlink *consumer,*provider;
 uchar request[128],names[4096],*root,firstbyte;uint hash[2],i;String *path,*seed=H;List *node=H;void *arg=H,*result;Prog *process;WORD calls;
 uvlong guard=0xf0e1d2c3b4a59687ULL,after;int opens,admits,ctors,pins,steps,done;
 int skip=mode==8,iofail=(mode>0 && mode<6)||mode==7;
 int listarg=method==managed_join || method==managed_head || method==managed_rest;
 int usesprovider=method!=managed_head && method!=managed_rest;
 const char *expected=!skip && listarg && input==3 && (managed_lists || !iofail)?exNilref:iofail && usesprovider?exModule:"";
 require(sizeof(managed_requests)<=sizeof(request) && sizeof(managed_names)<=sizeof(names));
 memmove(request,managed_requests,sizeof(managed_requests));memmove(names,managed_names,sizeof(managed_names));memmove(hash,managed_hashes,sizeof(hash));table.request.imports=request;table.request.strings=names;table.hashes=hash;
 require(native64typedmodule(&managed_spec,cjit,&owner));cm=owner.module;require(install64typedimports(cm,&table,1,pjit));
 managedcheck(((Dis64ImportOwner*)cm->native64imports)->managed==1,"owned native managed selector");pins=nlinkpins;
 consumer=managedinstance(cm,method);outer=managedlauncher(cjit,grow,skip);memset(request,0xa5,sizeof(request));memset(names,0xa5,sizeof(names));memset(hash,0,sizeof(hash));
 starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;
 path=newstring(13);memmove(path->Sascii,"library.dis64",14);memmove(root+64,&consumer,sizeof(consumer));memmove(root+104,&path,sizeof(path));
 if(input==1){seed=newrunes(3);seed->Srune[0]=0x4e16;seed->Srune[1]=0;seed->Srune[2]=0x1f680;}
 else if(input==0 || input==4){seed=newstring(3);memmove(seed->Sascii,"x\0y",3);}
 if(listarg){if(input!=3){R.s=&seed;R.d=&node;actual_consp();if(input==4){R.s=&seed;actual_consp();}arg=node;}}
 else{arg=seed;if(seed!=H)D2H(seed)->ref++;}
 memmove(root+72,&arg,sizeof(arg));memset(root+112,0xa5,8);memmove(root+120,&guard,8);
 firstbyte=((uchar*)filesource)[0];namespacefault=mode==1?1:mode==3?5:mode==7?9:0;admissionrequired=mode==2;librarypinfail=mode==5;dontcompile=mode==6;
 if(mode==4)((uchar*)filesource)[0]^=0x20;opens=fileopens;admits=admissioncalls;ctors=namespaceconstructors;process->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}
 ((uchar*)filesource)[0]=firstbyte;namespacefault=admissionrequired=librarypinfail=dontcompile=0;
 if(steps>=10000 || strcmp(worker.env->errstr,expected))fprintf(stderr,"managed graph method=%u jit=%d/%d grow=%d q=%d input=%d mode=%d error=%s expected=%s\n",method,pjit,cjit,grow,quantum,input,mode,worker.env->errstr,expected);
 managedcheck(steps<10000 && !strcmp(worker.env->errstr,expected),"source LOAD runtime");
 if(!expected[0]){
  require(R.FP==root);memmove(&result,root+80,sizeof(result));
  if(skip)require(result==H);
  else if(method==managed_split){List *xs=result;require(xs!=H && xs->tail==H && *(String**)xs->data==seed && D2H(xs)->ref==1);}
  else require(result==(method==managed_rest?(input==4?node->tail:H):(void*)seed));
  if(!skip && method==managed_rest && input==4)require(D2H(node->tail)->ref==2);
  if(seed!=H)managedcheck(D2H(seed)->ref==((input==4?3:2)+(skip || method==managed_rest?0:1)),"String retain across mapped frames");
 }
 memmove(&calls,consumer->MP+managed_calls_offset,4);require(calls==(skip?17:18));
 memmove(&calls,cm->origmp+managed_calls_offset,4);require(calls==17 && *(void**)(cm->origmp+managed_provider_offset)==H);
 require(nlinkpins==pins && !fileopen && fileopens==filecloses && fileopens==opens+(mode==1?0:2));
 if(mode==1 || mode==3 || mode==7)require(admissioncalls==admits && namespaceconstructors==ctors);
 else if(mode==2)require(admissioncalls==admits+2 && namespaceconstructors==ctors);
 else require(admissioncalls==admits+2 && namespaceconstructors==ctors+2);
 if(!iofail){memmove(&provider,consumer->MP+managed_provider_offset,sizeof(provider));require(provider!=H && provider->m->entry==nil && provider->m->entryt==nil && (provider->m->jit!=nil)==(pjit && mode!=6));}
 for(i=0;i<8;i++)require(root[112+i]==0xa5);memmove(&after,root+120,8);require(after==guard);
 observed=process;progexit();emptyqueues();require(cm->ref==1 && native64typedmoduleclear(&owner));
 if(seed!=H){require(D2H(seed)->ref==1);destroy(seed);}
 require(!live && !nlinkpins && !modules && !worker.nerr && Tstring.ref==1 && Tmodlink.ref==1 && Tlist.ref==1 && Tptr.ref==1);
 memset(nativeobservations,0,sizeof(nativeobservations));
}
/* A requested subset never grants authority to unrequested provider code. */
static void managedproof(void)
{
 Dis64ManagedFileLoad file={0};uchar request[36],*code;Dis64ManagedSignature signature;
 Dis64Binding binding;static const Dis64ManagedSchema schema={Dis64TrialVersion,Dis64TrialString,Dis64TrialList};
 Modlink *link;REG saved=R;uint i,j;int before;uchar old;
 require(dis64managedfileview(filesource,filebytes,&file.load.image,file.selected,file.entries,&file.catalog)==2);
 dis64put32(request,1);memmove(request+4,managed_requests+4,32);
 require(dis64managedbindingread(request+4,32,0,&schema,&binding,&signature));
 for(i=0;i<2;i++){
  require(dis64managedget(&file.load.image.exports,i,&schema,&binding,&signature));
  if(signature.result != dis64u32(request+24))break;
 }
 require(i<2);code=(uchar*)file.load.image.code.data+6+4*(file.selected[i].entry-1);old=*code;
 before=allocations;*code=old==ICONSP?IHEADP:ICONSP;
 link=actualprepare64managedlibrarylink("library.dis64",(uchar*)filesource,filebytes,0,request,sizeof(request),managed_names,sizeof(managed_names));*code=old;
 managedcheck(link==nil && allocations==before && !live && !nlinkpins && !memcmp(&R,&saved,sizeof(R)),"unrequested provider body proof");
 for(j=0;j<2;j++){
  memmove(request+4,managed_requests+4+j*32,32);
  link=actualprepare64managedlibrarylink("library.dis64",(uchar*)filesource,filebytes,0,request,sizeof(request),managed_names,sizeof(managed_names));
  require(link!=nil && link->nlinks==1 && nlinkpins==1 && link->links[0].frame->size==72);
  ptrdel(D2H(link));destroy(link);require(!live && !nlinkpins);
 }
 memmove(request+4,managed_requests+4,32);request[7]^=1;before=allocations;
 link=actualprepare64managedlibrarylink("library.dis64",(uchar*)filesource,filebytes,0,request,sizeof(request),managed_names,sizeof(managed_names));
 require(link==nil && allocations==before && !live && !nlinkpins && !memcmp(&R,&saved,sizeof(R)));
 printf("PASS managed complete file proof: single-method subsets, unused-body rejection, exact module identity, zero-allocation refusal\n");
}
static void managedconstruction(void)
{
 REG saved=R;Modlink *link;int j,attempts,before,i;
 for(j=0;j<2;j++){
  before=allocations;link=actualprepare64managedlibrarylink("library.dis64",(uchar*)filesource,filebytes,j,managed_requests,sizeof(managed_requests),managed_names,sizeof(managed_names));
  require(link!=nil && link->nlinks==2 && nlinkpins==1);attempts=allocations-before;ptrdel(D2H(link));destroy(link);require(!live && !nlinkpins);
  for(i=1;i<=attempts;i++){
   failat=allocations+i;link=actualprepare64managedlibrarylink("library.dis64",(uchar*)filesource,filebytes,j,managed_requests,sizeof(managed_requests),managed_names,sizeof(managed_names));failat=0;
   require(link==nil && !live && !nlinkpins && !memcmp(&saved,&R,sizeof(R)));
  }
  librarypinfail=1;link=actualprepare64managedlibrarylink("library.dis64",(uchar*)filesource,filebytes,j,managed_requests,sizeof(managed_requests),managed_names,sizeof(managed_names));librarypinfail=0;
  require(link==nil && !live && !nlinkpins && !memcmp(&saved,&R,sizeof(R)));
  printf("PASS managed library construction: jit=%d, %d allocation failures + failed GC pin, real unload/lease retirement\n",j,attempts);
 }
}
int main(int argc,char **argv)
{
 static uchar image[1048576];uint bytes,method;int p,c,g,q,v,mode,cases=0;int quantum[]={1,3,1000};require(argc==2 || argc==3);
 bytes=libraryread(argv[1],image,sizeof(image));filesource=image;filebytes=bytes;
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 if(argc==3 && !strcmp(argv[2],"default")){
  Native64ModuleOwner owner={0};int before=allocations;
  require(read64managedlibrary((char*)H,2,(uchar*)H,~0U,(uchar*)H,~0U,2)==nil && !fileopens && allocations==before);
  require(native64typedmodule(&managed_spec,0,&owner));require(!install64typedimports(owner.module,managed_tables,1,0) && !owner.module->native64imports);
  require(native64typedmoduleclear(&owner) && !live && !nlinkpins);printf("PASS managed default profile: no install, namespace or constructor work\n");return 0;
 }
 require(dis64selectlaunch("dis64-calls"));managedpreflight();if(managed_lists)listpreflight();if(argc==3)return 0;managedproof();managedconstruction();
 optab[ILOAD]=actual_iload;optab[IMOVP]=actual_movp;optab[ICONSP]=managed_lists?listconsp:actual_consp;optab[IHEADP]=actual_headp;optab[ITAIL]=managed_lists?listtail:actual_tail;optab[ILENL]=actual_lenl;optab[ILENC]=actual_lenc;
 MANAGED_HANDLERS
 for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(method=0;method<nelem(managed_functions);method++)if(method==managed_round || method==managed_split || method==managed_join || method==managed_head || method==managed_rest)
 for(v=0;v<(method==managed_join || method==managed_head || method==managed_rest?(managed_lists?5:4):3);v++)for(mode=0;mode<9;mode++){
  managedrun(p,c,g,quantum[q],v,method,mode);cases++;
 }
 printf("PASS checked LOAD -> managed file calls: %d C/JIT roots, String/List results, ASCII/Rune/NUL/nil, namespace faults, fresh reloads and complete retirement\n",cases);if(managed_lists)printf("PASS checked source list composition: cons/head/tail, local result roots, import round-trips and retain observations\n");return 0;
}
