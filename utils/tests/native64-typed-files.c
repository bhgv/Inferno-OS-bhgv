#include "typed-files.h"
static void filepredicate(int ok,const char *why){if(!ok){fprintf(stderr,"typed file predicate: %s\n",why);exit(10);}}
static Modlink *clientinstance(Module *m)
{
 Import imports[3]={{0}};Modlink *ml;
 imports[0].name="open";memmove(&imports[0].sig,&client_functions[client_open].signature,4);
 imports[1].name="run";memmove(&imports[1].sig,&client_functions[client_run].signature,4);
 m->ref++;ml=linkmod(m,imports,1);require(ml!=H);return ml;
}
static void installpreflight(void)
{
 Native64ModuleOwner owner={0};Native64TypedImportRequest table;Module *m;DisStringPoolView pool;
 uint hashes[2],oldhash;Type *type;Import *oldldt;char *oldname;int i,before,size,np,ref,ntype;REG saved=R;
 require(native64typedmodule(&client_spec,0,&owner));m=owner.module;table=file_tables[0];memmove(hashes,file_hashes,sizeof(hashes));table.hashes=hashes;
 require(disstringpoolcheck(file_unicode_names,sizeof(file_unicode_names),65536,&pool));
 require(match64typedname(&pool,0,"идентичность🚀") && !match64typedname(&pool,0,"идентичность") && !match64typedname(&pool,1,"id"));
 type=m->type[m->ntype-2];size=type->size;np=type->np;ref=m->ref;ntype=m->ntype;oldname=m->ldt[0][0].name;oldldt=m->ldt[1];oldhash=hashes[0];
 for(i=0;i<22;i++){
  table=file_tables[0];table.hashes=hashes;hashes[0]=oldhash;
  if(i==4)table.count=0;if(i==5)table.hashes=nil;if(i==6)table.hashes=(uint*)H;if(i==7)hashes[0]^=1;
  if(i==8)m->ldt[0][0].name="missing";if(i==9)table.request.importbytes=3;if(i==10)table.request.strings=nil;
  if(i==11)table.count=1;if(i==12)type->size+=8;if(i==13)type->map[1]=0x80;
  if(i==14)m->cstep=native64lea8step;if(i==15)m->native64importload=nil;if(i==16)m->ref++;
  if(i==17)modules=m;if(i==18)m->ldt[1]=(Import*)H;if(i==19)type->np=0;if(i==20)m->compiled=1;if(i==21)m->ntype=2;
  before=allocations;
  if(install64typedimports(m,i==3?nil:&table,i==0?0:i==1?65:1,i==2?2:0)){fprintf(stderr,"typed file install guard omitted: case=%d\n",i);exit(10);}
  require(allocations==before && m->native64imports==nil && !memcmp(&R,&saved,sizeof(R)));
  type->size=size;type->np=np;type->map[1]=0;m->ref=ref;m->ntype=ntype;m->cstep=native64typedlea8step;m->native64importload=read64ownedimport;
  m->ldt[0][0].name=oldname;m->ldt[1]=oldldt;m->compiled=0;modules=nil;
 }
 hashes[0]=oldhash;table=file_tables[0];table.hashes=hashes;before=live;failat=allocations+1;
 require(!install64typedimports(m,&table,1,0));failat=0;require(live==before && !m->native64imports && !memcmp(&R,&saved,sizeof(R)));
 require(install64typedimports(m,&table,1,0) && !install64typedimports(m,&table,1,0));
 require(native64typedmoduleclear(&owner) && !live && !nlinkpins);
 printf("PASS typed file installer: 22 no-allocation refusals, failed allocation rollback, Unicode/NUL names and owned cleanup\n");
}
static Module *filelauncher(int jit,int grow)
{
 uint kinds[]={file_kind};Module *m;Inst *code;uchar map[]={0,0x82};uint i,n=0;
 namespaceordinal=1;m=libraryouter(file_kind,1,kinds,0,grow);
 freetype(m->type[0]);m->type[0]=dtype(freeheap,128,map,2);require(m->type[0]!=nil);m->entryt=m->type[0];
 code=mallocz((m->nprog+6)*sizeof(Inst),1);require(code!=nil);
 for(i=0;i<2;i++){
  code[n].op=IMFRAME;code[n].add=SRC(AFP)|AXIMM|DST(AFP);code[n].s.ind=64;code[n++].d.ind=40;
  code[n].op=IMOVP;code[n].add=SRC(AFP)|DST(AFP|AIND);code[n].s.ind=112;code[n].d.i.f=40;code[n++].d.i.s=64;
  code[n].op=IMCALL;code[n].add=SRC(AFP)|AXIMM|DST(AFP);code[n].s.ind=40;code[n++].d.ind=64;
 }
 memmove(code+n,m->prog,m->nprog*sizeof(Inst));free(m->prog);m->prog=m->entry=code;m->nprog+=n;
 if(jit)require(compile(m,m->nprog,nil));return m;
}
static void filerun(int pjit,int cjit,int grow,int quantum,int trial,int mode)
{
 Native64ModuleOwner owner={0};Native64TypedImportRequest table=file_tables[0];Module *cm,*outer;Modlink *consumer,*provider;
 uchar requests[1024],names[4096],*root,firstbyte;uint hashes[2];String *path;Prog *process;uvlong value=0,expected=0,guard=0xabcdef0123456789ULL,after;
 WORD count;int i,steps,done,opens,admits,ctors,basepins;LONG big;REAL real;
 require(sizeof(file_requests)<=sizeof(requests) && sizeof(file_names)<=sizeof(names));
 memmove(requests,file_requests,sizeof(file_requests));memmove(names,file_names,sizeof(file_names));memmove(hashes,file_hashes,sizeof(hashes));
 table.request.imports=requests;table.request.strings=names;table.hashes=hashes;
 require(native64typedmodule(&client_spec,cjit,&owner));cm=owner.module;
 require(install64typedimports(cm,&table,1,pjit));basepins=nlinkpins;
 consumer=clientinstance(cm);outer=filelauncher(cjit,grow);
 /* Only private owned requests survive; no provider C code/maps were supplied. */
 memset(requests,0xa5,sizeof(requests));memset(names,0xa5,sizeof(names));memset(hashes,0xa5,sizeof(hashes));
 starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;
 memmove(root+64,&consumer,sizeof(consumer));path=newstring(13);memmove(path->Sascii,"library.dis64",14);memmove(root+112,&path,sizeof(path));
 if(file_kind==Dis64Int){WORD n=trial?-3:7,want=n*4;memmove(root+72,&n,4);memmove(&expected,&want,4);}
 else if(file_kind==Dis64Big){big=trial?-3:0x1ffffffffLL;memmove(root+72,&big,8);big*=4;memmove(&expected,&big,8);}
 else{real=trial?-1.25:42.0;memmove(root+72,&real,8);memmove(&expected,&real,8);}
 memset(root+104,0xa5,8);memmove(root+120,&guard,8);firstbyte=((uchar*)filesource)[0];
 namespacefault=mode==1?1:mode==3?5:mode==7?9:0;admissionrequired=mode==2;librarypinfail=mode==5;dontcompile=mode==6;
 if(mode==4)((uchar*)filesource)[0]^=0x20;
 opens=fileopens;admits=admissioncalls;ctors=namespaceconstructors;process->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}
 require(steps<10000);((uchar*)filesource)[0]=firstbyte;namespacefault=admissionrequired=librarypinfail=dontcompile=0;
 if(mode && mode!=6){require(!strcmp(worker.env->errstr,exModule));for(i=0;i<8;i++)require(root[104+i]==0xa5);}
 else{
  if(worker.env->errstr[0])fprintf(stderr,"typed file execution kind=%u jit=%d/%d grow=%d q=%d mode=%d error=%s\n",file_kind,pjit,cjit,grow,quantum,mode,worker.env->errstr);
  filepredicate(!worker.env->errstr[0] && R.FP==root,"source LOAD runtime");memmove(&value,root+104,file_kind==Dis64Int?4:8);require(value==expected);
  memmove(&provider,consumer->MP+client_provider_offset,sizeof(provider));require(provider!=H && provider!=nil && provider->m->entry==nil && provider->m->entryt==nil);
  require((provider->m->jit!=nil)==(pjit && mode!=6));
 }
 memmove(&count,consumer->MP+client_calls_offset,4);require(count==18);
 memmove(&count,cm->origmp+client_calls_offset,4);require(count==17 && *(void**)(cm->origmp+client_provider_offset)==H);
 if(file_kind==Dis64Int)require(!memcmp(root+108,"\xa5\xa5\xa5\xa5",4));memmove(&after,root+120,8);require(after==guard);
 require(nlinkpins==basepins && fileopens==filecloses && !fileopen);
 require(fileopens==opens+(mode==1?0:2));
 if(mode==1 || mode==3 || mode==7)require(admissioncalls==admits && namespaceconstructors==ctors);
 else if(mode==2)require(admissioncalls==admits+2 && namespaceconstructors==ctors);
 else require(admissioncalls==admits+2 && namespaceconstructors==ctors+2);
 observed=process;progexit();emptyqueues();require(cm->ref==1 && native64typedmoduleclear(&owner));
 require(!live && !nlinkpins && !modules && !worker.nerr && Tmodlink.ref==1 && Tstring.ref==1);
 memset(nativeobservations,0,sizeof(nativeobservations));
}
int main(int argc,char **argv)
{
 static uchar image[1048576];uint bytes;int p,c,g,q,t,mode,cases=0;int quanta[]={1,3,1000};require(argc==2 || argc==3);
 bytes=libraryread(argv[1],image,sizeof(image));filesource=image;filebytes=bytes;
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 if(argc==3 && !strcmp(argv[2],"default")){
  Native64ModuleOwner owner={0};require(native64typedmodule(&client_spec,0,&owner));
  require(!install64typedimports(owner.module,file_tables,1,0) && !owner.module->native64imports && !fileopens);
  require(native64typedmoduleclear(&owner) && !live && !nlinkpins);printf("PASS typed file default mode: no installer publication or namespace work\n");return 0;
 }
 require(dis64selectlaunch("dis64-calls"));installpreflight();if(argc==3)return 0;
 optab[ILOAD]=actual_iload;optab[IMOVP]=actual_movp;
 NAMESPACE_HANDLERS
 for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(t=0;t<2;t++)for(mode=0;mode<8;mode++){
  filerun(p,c,g,quanta[q],t,mode);cases++;
 }
 printf("PASS source LOAD -> owned installer -> namespace/admission -> file provider: kind=%u %d C/JIT roots, repeated fresh reads, eight success/fault/policy modes and complete retirement\n",file_kind,cases);return 0;
}
