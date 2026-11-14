/* Actual scheduled ILOAD with owned .m requests. Client instructions/maps are
 * still trusted native metadata, not an admitted importing Limbo file. */
char* string2c(String*s){require(s!=nil && s!=H && s->len>=0);return (char*)s->Sascii;}
static int loadrequestedjit,loadinstructions,loadexpectedlinks;
static void loadinstruction(void){
 void *destination=R.d;Modlink *ml;WORD index;memmove(&index,R.m,sizeof(index));actual_iload();loadinstructions++;
 memmove(&ml,destination,sizeof(ml));require(!nlinkpins);
 if(ml!=H){require(ml!=nil && ml->nlinks==(index?loadexpectedlinks:0));observedispatch(ml->m,loadrequestedjit);}
}
static Module*
loadouter(unsigned int kind,unsigned int count,const unsigned int*kinds,int jit,int grow,int providerjit)
{
 Module*m=libraryouter(kind,count,kinds,0,grow);Inst*code;uchar map[2]={0,0x82};Native64ImportRequest tables[2];static uchar empty[4]={0};
 freetype(m->type[0]);m->type[0]=dtype(freeheap,128,map,2);require(m->type[0]!=nil);m->entryt=m->type[0];
 code=mallocz((m->nprog+2)*sizeof(Inst),1);require(code!=nil);memmove(code+2,m->prog,m->nprog*sizeof(Inst));free(m->prog);m->prog=m->entry=code;m->nprog+=2;
 for(int i=0;i<2;i++){code[i].op=ILOAD;code[i].add=SRC(AFP)|AXIMM|DST(AFP);code[i].reg=i;code[i].s.ind=112;code[i].d.ind=64;}
 tables[0]=(Native64ImportRequest){requestcopy,namecopy,requestsize,namesize};tables[1]=tables[0];tables[0].imports=empty;tables[0].importbytes=4;
 require(install64callimports(m,tables,2,providerjit));
 require(!install64callimports(m,tables,2,providerjit));
 /* A published owner's requests must survive destruction of all input bytes. */
 memset(requestcopy,0xa5,requestsize);memset(namecopy,0xa5,namesize);memset(tables,0xa5,sizeof(tables));
 if(jit)require(compile(m,m->nprog,nil));return m;
}
static void installchecks(unsigned int result,unsigned int count,const unsigned int*kinds)
{
 Native64ImportRequest table[65];Module*m;int before;uchar empty[4]={0};
 for(int i=0;i<65;i++)table[i]=(Native64ImportRequest){requestcopy,namecopy,requestsize,namesize};
 m=libraryouter(result,count,kinds,0,0);before=live;
 require(!install64callimports(m,table,0,0) && !install64callimports(m,table,65,0) && !install64callimports(m,table,1,2));
 require(!m->native64imports && !m->native64importload && live==before);
 require(!install64callimports(m,nil,1,0));
 m->cstep=native64lea8step;require(!install64callimports(m,table,1,0));m->cstep=native64typedlea8step;
 table[1].strings=(void*)H;require(!install64callimports(m,table,2,0));table[1]=table[0];
 table[1].imports=(void*)((uintptr)-1-1);table[1].importbytes=4;require(!install64callimports(m,table,2,0));table[1]=table[0];
 require(live==before && !m->native64imports && !m->native64importload);
 table[1].importbytes=3;require(!install64callimports(m,table,2,0));table[1]=table[0];require(live==before && !m->native64imports);
 allocations=0;failat=1;require(!install64callimports(m,table,2,0));failat=0;require(live==before && !m->native64imports && !m->native64importload);
 m->ref++;require(!install64callimports(m,table,1,0));m->ref--;
 modules=m;require(!install64callimports(m,table,1,0));modules=nil;
 m->ldt=(Import**)H;require(!install64callimports(m,table,1,0));m->ldt=nil;
 require(install64callimports(m,table,64,0));unload(m);
 m=libraryouter(result,count,kinds,0,0);table[0].imports=empty;table[0].importbytes=4;require(install64callimports(m,table,1,0));unload(m);require(!live);
}
int main(int argc,char**argv)
{
 static uchar image[1048576];DisStringPoolView pool;Dis64Bindings requests;Dis64Binding binding;
 unsigned int bytes,result,count,kinds[3],i;uvlong values[3],expected,value;int failure,rejected,jit,outerjit,grow,q,mode,steps,done,opens,admits;
 require(argc>=8);bytes=libraryread(argv[1],image,sizeof(image));result=atoi(argv[2]);expected=strtoull(argv[3],nil,10);failure=atoi(argv[4]);namespaceordinal=NAMESPACE_ORDINAL;require(namespaceordinal==(unsigned int)atoi(argv[5]));rejected=!strcmp(argv[6],"reject");
 requestsize=sizeof(limbo_native64_Library_scalar_imports);namesize=sizeof(limbo_native64_Library_scalar_strings);require(requestsize<=sizeof(requestcopy) && namesize<=sizeof(namecopy));
 memmove(requestcopy,limbo_native64_Library_scalar_imports,requestsize);memmove(namecopy,limbo_native64_Library_scalar_strings,namesize);
 require(disstringpoolcheck(namecopy,namesize,65536,&pool) && dis64bindingscheck(requestcopy,requestsize,0,&pool,0,0,64,64,&requests));require(dis64bindingget(&requests,namespaceordinal,&binding));count=binding.signature.parameters;require(count<=3 && argc==8+(int)count);
 for(i=0;i<count;i++){kinds[i]=dis64u32(binding.signature.data+12+4*i);values[i]=strtoull(argv[8+i],nil,10);}
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;
 Tstring.free=freeheap;filesource=image;filebytes=bytes;
 if(!strcmp(argv[7],"default")){Module*m=libraryouter(result,count,kinds,0,0);Native64ImportRequest t={requestcopy,namecopy,requestsize,namesize};require(!install64callimports(m,&t,1,0));unload(m);require(!live && !fileopens);return 0;}
 require(dis64selectlaunch("dis64-calls"));installchecks(result,count,kinds);
 if(!strcmp(argv[6],"materialize"))return 0;
 optab[ILOAD]=loadinstruction;
 NAMESPACE_HANDLERS
 for(jit=0;jit<2;jit++)for(outerjit=0;outerjit<2;outerjit++)for(grow=0;grow<2;grow++)for(q=0;q<3;q++)for(mode=0;mode<5;mode++){
  Module*m;Prog*p;uchar*root;String*path;Modlink*loaded;int previous;
  memmove(requestcopy,limbo_native64_Library_scalar_imports,requestsize);memmove(namecopy,limbo_native64_Library_scalar_strings,namesize);
  loadrequestedjit=jit;loadexpectedlinks=requests.count;m=loadouter(result,count,kinds,outerjit,grow,jit);observedispatch(m,outerjit);starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);R=p->R;root=p->R.FP;
  /* A real String heap owner sits in the caller's mapped path slot. */
  path=newstring(13);memmove(path->Sascii,"library.dis64",14);memmove(root+112,&path,sizeof(path));
  for(i=0;i<count;i++)memmove(root+72+8*i,values+i,kinds[i]==Dis64Int?4:8);
  if(mode==1)namespacefault=1;
  if(mode==2)admissionrequired=1;
  if(mode==3){previous=m->prog[0].reg;m->prog[0].reg=2;}
  if(mode==4){previous=m->prog[0].reg;m->prog[0].reg=(ushort)-1;}
  opens=fileopens;admits=admissioncalls;p->quanta=q==0?1:q==1?3:1000;
  for(steps=0;steps<10000;steps++){
   done=kprotect(executeprocess,p);if(!done)break;
   if(p->quanta==1 && steps<2 && !rejected && !mode){memmove(&loaded,root+64,sizeof(loaded));require(loaded!=H && loaded!=nil && nlinkpins==0);}
  }
  require(steps<10000);namespacefault=admissionrequired=0;
  if(mode>=3){require(!strcmp(worker.env->errstr,exModule) && fileopens==opens);m->prog[0].reg=previous;}
  else if(mode || rejected){require(worker.env->errstr[0] && fileopens==opens+(mode==1?0:2));memmove(&loaded,root+64,sizeof(loaded));require(loaded==H);}
  else if(failure){require(!strcmp(worker.env->errstr,exZdiv) && fileopens==opens+2 && admissioncalls==admits+2);}
  else {require(!worker.env->errstr[0] && R.FP==root && fileopens==opens+2 && admissioncalls==admits+2);value=0;if(result){memmove(&value,root+104,result==Dis64Int?4:8);require(value==expected);}}
  namespaceend(p);require(Tstring.ref==1 && fileopens==filecloses);
 }
 if(!rejected && !failure)require(nativecalls && nativeaccepted && bridgecalls && ccalls);
 require(loadinstructions>0 && !live && !nlinkpins && !modules && !worker.nerr);return 0;
}
