/* Native importer uses actual compiler-generated .m payloads, not provider exports. */
#include NAMESPACE_HEADER
static unsigned int namespaceordinal;
static uchar requestcopy[20000],namecopy[70000];
static ulong requestsize,namesize;
static const uchar *namespaceimage;
static unsigned int namespacebytes;
static int namespacebaseline;

static Prog*
namespacebegin(unsigned int kind,unsigned int count,const unsigned int*kinds,int jit,int grow)
{
 Module*m=libraryouter(kind,count,kinds,jit,grow);Prog*p;
 starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);R=p->R;
 namespacebaseline=live;return p;
}
static void namespaceend(Prog*p){observed=p;progexit();emptyqueues();require(!live && !modules && !nlinkpins && Tmodlink.ref==1 && !worker.nerr);}
static Modlink*
namespaceload(int jit,int sync)
{
 Modlink*m;int r=releases,a=acquires;
 m=read64calllibrary("library.dis64",jit,requestcopy,requestsize,namecopy,namesize,sync);
 if(namespaceacquiremissed){namespaceacquiremissed=0;require(m==nil);namespaceend(currun());librarycheck(0,"namespace acquisition order");}
 if(fileopen){kclose(7);if(m!=nil)dropinstance(m);namespaceend(currun());librarycheck(0,"namespace descriptor cleanup");}
 require(releases==r+sync && acquires==a+sync && worker.iprog==nil && currun()!=nil && !fileopen && fileopens==filecloses && !worker.nerr);
 return m;
}
static void namespaceclean(void){require(live==namespacebaseline && !modules && !nlinkpins && !fileopen && fileopens==filecloses);}
static void namespacefailure(int jit,int sync){Modlink*m=namespaceload(jit,sync);if(m!=nil){dropinstance(m);namespaceend(currun());librarycheck(0,"namespace failure refused");}namespaceclean();}

int main(int argc,char**argv)
{
 static uchar image[1048576],saved[1048576];unsigned int result,count,kinds[3],bytes,i;uvlong values[3],expected,value;
 DisStringPoolView pool;Dis64Bindings requests;Dis64Binding binding;Modlink*m,*other;Module*shadow;Prog*p;uchar*root;
 int failure,jit,sync,total,fault,grow,q,outerjit,steps,done,opens,admits,ctors,materialize,rejected;
 require(argc>=8);bytes=libraryread(argv[1],image,sizeof(image));memmove(saved,image,bytes);namespaceimage=image;namespacebytes=bytes;
 result=atoi(argv[2]);expected=strtoull(argv[3],nil,10);failure=atoi(argv[4]);namespaceordinal=NAMESPACE_ORDINAL;require(namespaceordinal==(unsigned int)atoi(argv[5]));
 materialize=strcmp(argv[6],"materialize")==0;rejected=strcmp(argv[6],"reject")==0;
 requestsize=sizeof(limbo_native64_Library_scalar_imports);namesize=sizeof(limbo_native64_Library_scalar_strings);
 require(requestsize<=sizeof(requestcopy) && namesize<=sizeof(namecopy));memmove(requestcopy,limbo_native64_Library_scalar_imports,requestsize);memmove(namecopy,limbo_native64_Library_scalar_strings,namesize);
 require(limbo_native64_Library_layout());require(disstringpoolcheck(namecopy,namesize,65536,&pool));require(dis64bindingscheck(requestcopy,requestsize,0,&pool,0,0,64,64,&requests));
 require(dis64bindingget(&requests,namespaceordinal,&binding));count=binding.signature.parameters;require(count<=3 && argc==8+(int)count);
 for(i=0;i<count;i++){kinds[i]=dis64u32(binding.signature.data+12+4*i);values[i]=strtoull(argv[8+i],nil,10);}
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;
 filesource=image;filebytes=bytes;
 if(strcmp(argv[7],"default")==0){
  Module caller={0};Native64ImportRequest table={requestcopy,namecopy,requestsize,namesize};
  caller.ref=1;caller.origmp=H;caller.cstep=native64typedlea8step;
  require(!install64callimports(&caller,&table,1,0) && !caller.native64imports && !caller.native64importload && !live);
  m=read64calllibrary("library.dis64",0,requestcopy,requestsize,namecopy,namesize,0);if(m!=nil){dropinstance(m);librarycheck(0,"namespace startup profile");}
  require(!fileopens && !live);
  require(!dis64selectlaunch("dis64-calls"));return 0;
 }
 require(dis64selectlaunch("dis64-calls"));
 NAMESPACE_HANDLERS
 for(jit=0;jit<2;jit++)for(sync=0;sync<2;sync++){
  p=namespacebegin(result,count,kinds,0,0);opens=fileopens;admits=admissioncalls;ctors=namespaceconstructors;
  if(rejected){namespacefailure(jit,sync);require(admissioncalls==admits+1 && namespaceconstructors==ctors+1);namespaceend(p);continue;}
  allocations=0;m=namespaceload(jit,sync);require(m!=nil);total=allocations;
  if(admissioncalls!=admits+1){dropinstance(m);namespaceend(p);librarycheck(0,"namespace admission route");}
  require((m->m->jit!=nil)==jit && m->m->entry==nil && m->m->entryt==nil && m->nlinks==(int)requests.count);
  require(admissioncalls==admits+1 && namespaceconstructors==ctors+1 && fileopens==opens+1);dropinstance(m);namespaceclean();
  /* New namespace work runs independently of any old module/cache alias. */
  m=namespaceload(jit,sync);require(m!=nil);shadow=m->m;shadow->ref++;dropinstance(m);modules=shadow;
  opens=fileopens;admits=admissioncalls;other=namespaceload(jit,sync);require(other!=nil && other->m!=shadow && fileopens==opens+1 && admissioncalls==admits+1);dropinstance(other);unload(shadow);namespaceclean();
  /* Rejected admission precedes even the bounded constructor proof workspace. */
  for(i=1;i<=2;i++){
   admissionrequired=i==1;admissionthrow=i==2;ctors=namespaceconstructors;namespacefailure(jit,sync);require(namespaceconstructors==ctors);
  }admissionrequired=admissionthrow=0;
  for(i=1;i<=9;i++){namespacefault=i;ctors=namespaceconstructors;admits=admissioncalls;namespacefailure(jit,sync);require(namespaceconstructors==ctors && admissioncalls==admits);}namespacefault=0;
  for(i=0;i<4;i++){
   namespaceoverride=1;namespacelength=i==0?-1:i==1?0:i==2?Dis64CallImageBytes+1:0x100000000ULL;
   namespacefailure(jit,sync);
  }namespaceoverride=0;
  image[0]^=0x20;admits=admissioncalls;ctors=namespaceconstructors;namespacefailure(jit,sync);
  require(admissioncalls==admits+1 && namespaceconstructors==ctors+1);image[0]^=0x20;
  /* Empty requests still demand a complete proved library. */
  {uchar empty[4]={0};m=read64calllibrary("library.dis64",jit,empty,4,namecopy,namesize,sync);require(m!=nil && !m->nlinks);dropinstance(m);namespaceclean();}
  for(fault=1;fault<=total;fault++){
   allocations=0;failat=fault;m=namespaceload(jit,sync);failat=0;if(m!=nil)dropinstance(m);
   require(m==nil);namespaceclean();
  }
  librarypinfail=1;namespacefailure(jit,sync);librarypinfail=0;
  dontcompile=1;m=namespaceload(1,sync);require(m!=nil && m->m->jit==nil);dropinstance(m);dontcompile=0;namespaceclean();
  namespacefault=10;m=namespaceload(1,sync);require(m!=nil && m->m->jit==nil);dropinstance(m);namespacefault=dontcompile=0;namespaceclean();
  opens=fileopens;
  require(read64calllibrary("$Sys",jit,requestcopy,requestsize,namecopy,namesize,sync)==nil);
  require(read64calllibrary("$self",jit,requestcopy,requestsize,namecopy,namesize,sync)==nil);
  require(read64calllibrary("",jit,requestcopy,requestsize,namecopy,namesize,sync)==nil);
  require(read64calllibrary("library.dis64",2,requestcopy,requestsize,namecopy,namesize,sync)==nil);
  require(read64calllibrary("library.dis64",jit,requestcopy,requestsize,namecopy,namesize,2)==nil);
  requestcopy[0]=0xff;require(read64calllibrary("library.dis64",jit,requestcopy,requestsize,namecopy,namesize,sync)==nil);requestcopy[0]=0;
  require(fileopens==opens);namespaceclean();namespaceend(p);
  if(materialize)continue;
  for(outerjit=0;outerjit<2;outerjit++)for(grow=0;grow<2;grow++)for(q=0;q<3;q++){
   p=namespacebegin(result,count,kinds,outerjit,grow);root=p->R.FP;m=namespaceload(jit,sync);require(m!=nil);
   observedispatch(p->R.M->m,outerjit);observedispatch(m->m,jit);
   memmove(root+64,&m,sizeof(m));ptrdel(D2H(m));for(i=0;i<count;i++)memmove(root+72+8*i,values+i,kinds[i]==Dis64Int?4:8);
   memset(image,0xa5,bytes);memset(requestcopy,0xa5,requestsize);memset(namecopy,0xa5,namesize);p->quanta=q==0?1:q==1?3:1000;
   for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;if(!steps && p->quanta==1){Frame*f;memmove(&f,root+40,sizeof(f));require((f->t==nil)==grow);}}
   require(steps<10000);if(failure)require(!strcmp(worker.env->errstr,exZdiv));else{require(!worker.env->errstr[0] && R.FP==root);value=0;if(result){memmove(&value,root+104,result==Dis64Int?4:8);require(value==expected);}}
   namespaceend(p);memmove(image,saved,bytes);memmove(requestcopy,limbo_native64_Library_scalar_imports,requestsize);memmove(namecopy,limbo_native64_Library_scalar_strings,namesize);
  }
 }
 if(!materialize && !rejected && !failure)require(nativecalls && nativeaccepted && bridgecalls && ccalls);
 require(!live && !nlinkpins && !modules && fileopens==filecloses && !worker.nerr);return 0;
}
