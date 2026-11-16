/* Outer MCALL client is trusted fixture code. Only the provider comes from a
 * compiled file; no provider C code/PC/map declarations enter the constructor. */
static void librarycheck(int ok,char *why){if(!ok){fprintf(stderr,"library semantic failure: %s\n",why);exit(10);}}
static void libraryrequire(int ok,char*expr,int line){if(!ok){fprintf(stderr,"library fixture predicate: %d: %s\n",line,expr);exit(10);}}
#define require(c) libraryrequire((c),#c,__LINE__)
static unsigned int libraryread(char *path,uchar *data,unsigned int capacity){FILE*f=fopen(path,"rb");unsigned int n;require(f!=nil);n=fread(data,1,capacity,f);require(feof(f)&&!ferror(f));fclose(f);return n;}
static void dropinstance(Modlink *m){ptrdel(D2H(m));destroy(m);}

static Module*
libraryouter(unsigned int result,unsigned int parameters,const unsigned int *kinds,int jit,int grow)
{
 Module*m=mallocz(sizeof(*m),1);Inst*in;uchar map[2]={0,0x80};unsigned int i,at=64,n=0;
 require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,112,map,2);require(m->type[0]!=nil);m->entryt=m->type[0];
 m->nprog=parameters+3+(result!=0);m->prog=mallocz(m->nprog*sizeof(Inst),1);require(m->prog!=nil);m->entry=m->prog;
 in=m->prog+n++;in->op=IMFRAME;in->add=SRC(AFP)|AXIMM|DST(AFP);in->s.ind=64;in->d.ind=40;
 for(i=0;i<parameters;i++){
  unsigned int width=kinds[i]==Dis64Int?4:8;at=(at+width-1)&~(width-1);
  in=m->prog+n++;in->op=kinds[i]==Dis64Int?IMOVW:kinds[i]==Dis64Big?IMOVL:IMOVF;
  in->add=SRC(AFP)|DST(AFP|AIND);in->s.ind=72+8*i;in->d.i.f=40;in->d.i.s=at;at+=width;
 }
 if(result){in=m->prog+n++;in->op=ILEA;in->add=SRC(AFP)|DST(AFP|AIND);in->s.ind=104;in->reg=result==Dis64Int?4:8;in->d.i.f=40;in->d.i.s=32;}
 in=m->prog+n++;in->op=IMCALL;in->add=SRC(AFP)|AXIMM|DST(AFP);in->s.ind=40;in->d.ind=64;
 in=m->prog+n++;in->op=IEXIT;in->add=SRC(AXXX)|DST(AXXX);require(n==m->nprog);
 if(jit)require(compile(m,m->nprog,nil));return m;
}

static void
libraryrun(Modlink*provider,unsigned int result,unsigned int parameters,const unsigned int*kinds,
 const uvlong*values,uvlong expected,int failure,int jit,int grow,int quantum)
{
 Module*outer=libraryouter(result,parameters,kinds,jit,grow);Prog*process;uchar*root;uvlong value=0;int steps,done;
 require(provider->m->entry==nil && provider->m->entryt==nil);
 observedispatch(provider->m,provider->m->jit!=nil);observedispatch(outer,jit);
 starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;
 memmove(root+64,&provider,sizeof(provider));ptrdel(D2H(provider));
 for(unsigned int i=0;i<parameters;i++)memmove(root+72+8*i,values+i,kinds[i]==Dis64Int?4:8);
 process->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;
  if(steps==0 && quantum==1){Frame*pending;memmove(&pending,root+40,sizeof(pending));require((pending->t==nil)==grow);}
 }
 require(steps<10000);
 if(failure)require(strcmp(worker.env->errstr,exZdiv)==0);
 else{
  if(worker.env->errstr[0] || R.FP!=root)fprintf(stderr,"library execution: error=%s steps=%d jit=%d grow=%d quantum=%d sameframe=%d\n",worker.env->errstr,steps,jit,grow,quantum,R.FP==root);
  require(worker.env->errstr[0]==0 && R.FP==root);
  if(result){memmove(&value,root+104,result==Dis64Int?4:8);require(value==expected);}
 }
 observed=process;progexit();emptyqueues();
 require(live==0 && modules==nil && nlinkpins==0 && Tmodlink.ref==1 && worker.nerr==0);
 memset(nativeobservations,0,sizeof(nativeobservations));
}

static int librarypinfail;
int main(int argc,char**argv)
{
 static uchar image[1048576],saved[1048576],imports[16384],isaved[16384],names[65536],nsaved[65536];
 Dis64CallImage view;Dis64CallLibraryLinks plan,original;Dis64Bindings requests;DisStringPoolView pool;Dis64Binding binding;
 Modlink*link;unsigned int bytes,ibytes,nbytes,result,parameters,kinds[3],i,at,used,last=0;uvlong values[3],expected;
 int failure,jit,total,fault,grow,q,outerjit,materialize,raisedbefore;Dis64CallDecoded decoded;
 require(argc>=7);bytes=libraryread(argv[1],image,sizeof(image));ibytes=libraryread(argv[2],imports,sizeof(imports));nbytes=libraryread(argv[3],names,sizeof(names));
 result=atoi(argv[4]);expected=strtoull(argv[5],nil,10);failure=atoi(argv[6]);materialize=strcmp(argv[argc-1],"materialize")==0;
 memmove(saved,image,bytes);memmove(isaved,imports,ibytes);memmove(nsaved,names,nbytes);
 require(dis64callimageopenmode(image,bytes,1048576,&view,1));require(disstringpoolcheck(names,nbytes,65536,&pool));
 require(dis64bindingscheck(imports,ibytes,0,&pool,0,0,64,64,&requests));require(dis64bindingget(&requests,0,&binding));parameters=binding.signature.parameters;
 require(parameters<=3 && argc==7+(int)parameters+materialize);
 for(i=0;i<parameters;i++){kinds[i]=dis64u32(binding.signature.data+12+4*i);values[i]=strtoull(argv[7+i],nil,10);}
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;
 optab[IMOVW]=countmovw;optab[IADDW]=countaddw;optab[IDIVW]=countdivw;optab[ISHLW]=countshlw;optab[IEXIT]=countexit;
 optab[IFRAME]=actual_frame;optab[ICALL]=actual_call;optab[IRET]=actual_ret;optab[ILEA]=actual_lea;optab[ICVTWF]=actual_cvtwf;optab[IMOVL]=actual_movl;optab[IMOVF]=actual_movf;
 optab[IMFRAME]=actual_mframe;optab[IMCALL]=actual_mcall;
 LIBRARY_HANDLERS
 require(prepare64calls("library",image,bytes,0)==nil && live==0);
 /* Empty requests instantiate without executing; duplicate exact requests have
  * existing wire semantics, including the constructor's complete64-link limit. */
 {uchar empty[4]={0};link=prepare64calllibrarylink("library",image,bytes,0,empty,4,names,nbytes);require(link!=nil && link->nlinks==0);dropinstance(link);require(live==0);}
 require(dis64bindingrecord(imports+4,ibytes-4,0,&binding,&used));
 {uchar repeated[16384];require(4+65*used<=sizeof(repeated));for(i=0;i<65;i++)memmove(repeated+4+i*used,imports+4,used);
  dis64put32(repeated,64);link=prepare64calllibrarylink("library",image,bytes,0,repeated,4+64*used,names,nbytes);require(link!=nil && link->nlinks==64);dropinstance(link);require(live==0);
  dis64put32(repeated,65);allocations=0;require(prepare64calllibrarylink("library",image,bytes,0,repeated,4+65*used,names,nbytes)==nil && live==0 && allocations==0);
 }
 /* Matching refusals preserve the complete caller output and allocate no VM owner. */
 for(i=0;i<3;i++){
  memset(&plan,0xa5,sizeof(plan));original=plan;
  if(i==0)names[11]^=1; /* First module rune, encoded as uint32. */
  if(i==1)dis64put32(imports+20,result==Dis64Big?Dis64Int:Dis64Big);
  if(i==2){at=4;for(unsigned int j=0;j<requests.count;j++){require(dis64bindingrecord(imports+at,ibytes-at,0,&binding,&used));last=at;at+=used;}dis64put32(imports+last+16,dis64u32(imports+last+16)==3?2:3);}
  allocations=0;link=prepare64calllibrarylink("library",image,bytes,0,imports,ibytes,names,nbytes);
  if(link!=nil)dropinstance(link);
  librarycheck(link==nil && live==0 && allocations==0,"exact import identity");
  require(!dis64calllibrarymatch(&view,imports,ibytes,names,nbytes,&plan));require(memcmp(&plan,&original,sizeof(plan))==0);
  memmove(imports,isaved,ibytes);memmove(names,nsaved,nbytes);
 }
 /* A malformed unrequested body still refuses before any execution. */
 at=4;for(i=0;i<view.instructions;i++){last=at;require(dis64callread(view.code+at,bytes-(view.code-image)-at,&decoded,&used));at+=used;}
 require(decoded.instruction.op==IRET);((uchar*)view.code)[last]=IEXIT;
 link=prepare64calllibrarylink("library",image,bytes,0,imports,ibytes,names,nbytes);if(link!=nil)dropinstance(link);
 librarycheck(link==nil && live==0,"complete library proof");
 {uchar empty[4]={0};link=prepare64calllibrarylink("library",image,bytes,0,empty,4,names,nbytes);if(link!=nil)dropinstance(link);librarycheck(link==nil && live==0,"complete unrequested library proof");}
 memmove(image,saved,bytes);
 for(jit=0;jit<2;jit++){
  allocations=0;raisedbefore=raised;link=prepare64calllibrarylink("library",image,bytes,jit,imports,ibytes,names,nbytes);total=allocations;
  require(link!=nil);int noinit=link->m->entry==nil && link->m->entryt==nil && raised==raisedbefore;
  if(!noinit)dropinstance(link);librarycheck(noinit,"no automatic init");
  require((link->m->jit!=nil)==jit);
  require(link->nlinks==(int)requests.count && link->m->ext==nil && nlinkpins==1);dropinstance(link);require(live==0);
  for(fault=1;fault<=total;fault++){
   allocations=0;failat=fault;link=prepare64calllibrarylink("library",image,bytes,jit,imports,ibytes,names,nbytes);failat=0;
   require(link==nil && live==0 && nlinkpins==0 && Tmodlink.ref==1 && worker.nerr==0);
  }
  librarypinfail=1;link=prepare64calllibrarylink("library",image,bytes,jit,imports,ibytes,names,nbytes);librarypinfail=0;
  require(link==nil && live==0 && nlinkpins==0 && Tmodlink.ref==1);
  if(materialize)continue;
  for(outerjit=0;outerjit<2;outerjit++)for(grow=0;grow<2;grow++)for(q=0;q<3;q++){
   link=prepare64calllibrarylink("library",image,bytes,jit,imports,ibytes,names,nbytes);require(link!=nil);
   memset(image,0xa5,bytes);memset(imports,0xa5,ibytes);memset(names,0xa5,nbytes);
   libraryrun(link,result,parameters,kinds,values,expected,failure,outerjit,grow,q==0?1:q==1?3:1000);
   memmove(image,saved,bytes);memmove(imports,isaved,ibytes);memmove(names,nsaved,nbytes);
  }
 }
 if(!materialize){
  if(failure)require(ccalls>0); /* A constant zero divisor may have no emitted RX entry. */
  else {require(bridgecalls && ccalls);if(nativeleafavailable)require(nativecalls && nativeaccepted);else require(!nativecalls && !nativeaccepted);}
 }
 require(live==0 && nlinkpins==0 && worker.nerr==0);return 0;
}
