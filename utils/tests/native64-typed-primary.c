#include "primary.h"
static uint primaryloads;
static int primarymiss;
static void primaryconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,before,attempts,i;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&primary_spec,jit,&owner));attempts=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=attempts;i++){
   failat=allocations+i;require(!native64typedmodule(&primary_spec,jit,&owner));failat=0;
   require(!owner.module && !owner.rooted && !live && !nlinkpins && !memcmp(&R,&saved,sizeof(R)));
  }
  printf("PASS primary constructor: jit=%d %d allocation refusals, complete rollback and unchanged REG\n",jit,attempts);
 }
}
static Module *primarycaller;
static List *primaryarguments;
static uint primaryinits;
static void primarymcall(void)
{
 Frame*pending;void*ctxt;List*args;
 if(primarycaller && R.M->m==primarycaller){
  memmove(&pending,R.s,sizeof(pending));memmove(&ctxt,(uchar*)pending+64,sizeof(ctxt));memmove(&args,(uchar*)pending+72,sizeof(args));
  commandcheck(ctxt==H,"primary nil Context publication");
  if(primaryarguments!=H)commandcheck(args==primaryarguments,"primary original argv identity");
  else{commandcheck(args!=H && args->tail==H,"primary default argv shape");argvtext(*(String**)args->data,"-l");}
  primaryinits++;
 }
 actual_mcall();
}
static Modlink *primaryload(Module*m,WORD table,char*path)
{
 commandpath(path);primaryloads++;if(primarymiss)return H;return read64ownedimport(m,table,path);
}
static Module *primarylauncher(int jit,int grow)
{
 Module*m;Inst*p;uchar map[]={0,0xe0};int i,a,n=0;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,128,map,2);require(m->type[0]!=nil);m->entryt=m->type[0];
 m->prog=p=mallocz(9*sizeof(Inst),1);require(p!=nil);m->entry=p;
 for(i=0;i<2;i++){
  p[n].op=IMFRAME;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=64;p[n++].d.ind=40;
  for(a=0;a<2;a++){p[n].op=IMOVP;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=72+8*a;p[n].d.i.f=40;p[n++].d.i.s=64+8*a;}
  p[n].op=IMCALL;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=40;p[n++].d.ind=64;
 }
 p[n].op=IEXIT;p[n++].add=SRC(AXXX)|DST(AXXX);m->nprog=n;if(jit)require(compile(m,n,nil));return m;
}
static void primaryrun(int pjit,int cjit,int grow,int quantum,int input,int mode)
{
 Native64ModuleOwner owner={0};Module *cm,*outer;Import request[2]={{0}};Modlink*consumer;
 Prog*process;uchar*root,first;List *args=H,*node;String *s;uint i,steps,done,count;int opens,admits,ctors,expectcalls;
 const char *expected=mode==1 || mode==2 || mode==3 || mode==4 || mode==6 || mode==7 || mode==8?"fail:error":command_fault?exZdiv:"";
 require(native64typedmodule(&primary_spec,cjit,&owner));cm=owner.module;
 require(install64typedimports(cm,command_file_tables,1,pjit));cm->native64importload=primaryload;
 request[0].name="init";memmove(&request[0].sig,&primary_functions[0].signature,4);cm->ref++;consumer=linkmod(cm,request,1);require(consumer!=H);
 outer=primarylauncher(cjit,grow);starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;
 if(input){
  const char*words[]={"sh","-c","$*","w\0m","-p"};
  for(i=5;i>0;i--){s=c2string((char*)words[i-1],i==4?3:2);R.s=&s;R.d=&args;actual_consp();destroy(s);}
 }
 memmove(root+64,&consumer,sizeof(consumer));{void*ctxt=H;memmove(root+72,&ctxt,sizeof(ctxt));}memmove(root+80,&args,sizeof(args));memset(root+88,0xa5,40);
 opens=fileopens;admits=admissioncalls;ctors=namespaceconstructors;primaryloads=primarypaths=primaryinits=0;primarymiss=mode==6;primaryrunning=1;primarycaller=cm;primaryarguments=args;
 namespacefault=mode==1?1:mode==2?5:mode==4?9:0;dontcompile=mode==5;librarypinfail=mode==7;admissionrequired=mode==8;
 first=((uchar*)filesource)[0];if(mode==3)((uchar*)filesource)[0]^=0x20;process->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}
 ((uchar*)filesource)[0]=first;namespacefault=dontcompile=librarypinfail=admissionrequired=primaryrunning=primarymiss=0;
 if(steps>=10000 || strcmp(worker.env->errstr,expected))fprintf(stderr,"primary jit=%d/%d grow=%d q=%d input=%d mode=%d error=%s expected=%s\n",pjit,cjit,grow,quantum,input,mode,worker.env->errstr,expected);
 commandcheck(steps<10000 && !strcmp(worker.env->errstr,expected),"primary LOAD/init result");
 count=expected[0]?1:2;expectcalls=mode==6?0:count;
 commandcheck(primaryloads==count && primarypaths==(uint)expectcalls,"primary exact path and repeated source LOAD");
 if(!cjit)commandcheck(primaryinits==(!strcmp(expected,"fail:error")?0:count),"primary explicit init observation");
 require(!fileopen && fileopens==filecloses && fileopens==opens+(mode==1?0:expectcalls));
 if(mode==1 || mode==2 || mode==4 || mode==6)require(admissioncalls==admits && namespaceconstructors==ctors);
 else if(mode==8)require(admissioncalls==admits+count && namespaceconstructors==ctors);
 else require(admissioncalls==admits+count && namespaceconstructors==ctors+count);
 if(!strcmp(expected,"fail:error")){
  commandcheck(process->exval!=H,"primary source raise root");argvtext(process->exval,"fail:error");
  require(!handler(worker.env->errstr) && process->exval==H);
 }else require(process->exval==H);
 if(!expected[0]){
  commandcheck(R.FP==root,"primary returned to caller");if(input){
   commandcheck(D2H(args)->ref==1,"primary argv identity and retirement");node=args;
   for(i=0;i<5;i++){require(node!=H);s=*(String**)node->data;require(s->len==(i==3?3:2));node=node->tail;}require(node==H);
  }
 }
 for(i=88;i<128;i++)require(root[i]==0xa5);
 primarycaller=nil;observed=process;progexit();emptyqueues();require(cm->ref==1 && native64typedmoduleclear(&owner));
 require(!live && !nlinkpins && !modules && !worker.nerr && Tstring.ref==1 && Tmodlink.ref==1 && Tlist.ref==1 && Tptr.ref==1);
 memset(nativeobservations,0,sizeof(nativeobservations));
}
int main(int argc,char**argv)
{
 static uchar image[1048576];int p,c,g,q,input,mode,cases=0;int quantum[]={1,3,1000};require(argc==2);
 filesource=image;filebytes=libraryread(argv[1],image,sizeof(image));
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));primaryconstruction();optab[ILOAD]=actual_iload;optab[IMOVP]=actual_movp;optab[ICONSP]=actual_consp;optab[IHEADP]=actual_headp;optab[ITAIL]=actual_tail;optab[ILENL]=actual_lenl;optab[ILENC]=actual_lenc;optab[IRAISE]=actual_iraise;
 PRIMARY_HANDLERS
 optab[IFRAME]=actual_frame;optab[ICALL]=actual_call;optab[IRET]=actual_ret;optab[ILEA]=actual_lea;
 optab[IMCALL]=primarymcall;
 for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(input=0;input<2;input++)for(mode=0;mode<9;mode++){
  primaryrun(p,c,g,quantum[q],input,mode);cases++;
 }
 printf("PASS actual primary loadmod: %d C/JIT roots, exact AWM namespace path, direct Limbo init, nil/default argv, repeated LOAD and full teardown\n",cases);return 0;
}
