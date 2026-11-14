/* Both caller CODE and provider CODE originate in compiler files. */
static int clientproviderjit,clientloads;
static void clientloadstep(void){
 void*destination=R.d;Modlink*ml;actual_iload();memmove(&ml,destination,sizeof(ml));require(!nlinkpins);
 if(ml!=H){require(ml!=nil);observedispatch(ml->m,clientproviderjit && !dontcompile);clientloads++;}
}
static void clientempty(void){require(!live && !modules && !nlinkpins && !worker.nerr && !fileopen && fileopens==filecloses && Tstring.ref==1);}
int main(int argc,char**argv)
{
 static uchar client[1048576],saved[1048576],provider[1048576];Dis64ScalarClientView view;Module*m;Prog*p;uchar*root;uvlong expected,value;unsigned int result,offset=0;int failure,jit,pjit,grow,q,mode,steps,done,total,fault;
 require(argc==6 || argc==7);clientfilebytes=libraryread(argv[1],client,sizeof(client));providerfilebytes=libraryread(argv[2],provider,sizeof(provider));clientfile=client;providerfile=provider;memmove(saved,client,clientfilebytes);
 result=atoi(argv[3]);expected=strtoull(argv[4],nil,10);failure=atoi(argv[5]);require(dis64clientcheck(client,clientfilebytes,&view));
 for(unsigned int i=0;i<view.count;i++)if(view.decoded[i].instruction.op==IMCALL && i+1<view.count && view.decoded[i+1].instruction.source==view.frame-8)offset=view.decoded[i+1].instruction.destination;
 if(result)require(offset>=64);
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));optab[ILOAD]=clientloadstep;
 CLIENT_HANDLERS
 for(jit=0;jit<2;jit++){
  cflag=jit;allocations=0;m=readmod("client.dis64",nil,0);require(m!=nil && !nlinkpins && (m->rt&Dis64ScalarClient) && m->native64importload!=nil);total=allocations;unload(m);clientempty();
  for(fault=1;fault<=total;fault++){
   allocations=0;failat=fault;m=readmod("client.dis64",nil,0);failat=0;if(m)unload(m);require(m==nil);clientempty();
  }
 }
 if(argc==7)return 0;
 for(jit=0;jit<2;jit++)for(pjit=0;pjit<2;pjit++)for(grow=0;grow<2;grow++)for(q=0;q<3;q++)for(mode=0;mode<4;mode++){
  cflag=jit;m=readmod("client.dis64",nil,0);require(m!=nil && !nlinkpins);observedispatch(m,jit);m->ss=grow?0:4096;
  /* No compiler/image metadata may remain borrowed when startup allocates. */
  memset(client,0xa5,clientfilebytes);starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);R=p->R;root=p->R.FP;
  clientproviderjit=cflag=pjit;p->quanta=q==0?1:q==1?3:1000;
  if(mode==1)namespacefault=1;if(mode==2)admissionrequired=1;if(mode==3)dontcompile=1;
  for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
  require(steps<10000);namespacefault=admissionrequired=dontcompile=0;
  if(mode==1 || mode==2)require(worker.env->errstr[0]);
  else if(failure)require(!strcmp(worker.env->errstr,exZdiv));
  else {require(!worker.env->errstr[0] && R.FP==root);value=0;if(result){memmove(&value,root+offset,result==Dis64Int?4:8);require(value==expected);}}
  namespaceend(p);clientempty();memmove(client,saved,clientfilebytes);
 }
 if(!failure)require(nativecalls && nativeaccepted && ccalls && bridgecalls);require(clientloads>0);return 0;
}
