#include "stat.h"
static int statfailure,statseed,statnativefailure,statcalls,statrootfault;
static LONG statlength=(1LL<<44)+123;
static char*stattexts[]={"мир","user","🚀","muid"};
static void fstatcheck(int ok,char*why){if(!ok){fprintf(stderr,"fstat predicate: %s\n",why);exit(10);}}
static Dir*statdirfstat(int fd){
 Dir*d;fstatcheck(fd==23,"captured native FD");if(statnativefailure)return nil;
 d=mallocz(sizeof(*d),1);if(!d)return nil;d->name=stattexts[0];d->uid=stattexts[1];d->gid=stattexts[2];d->muid=stattexts[3];d->length=statlength;d->qid.path=(1ULL<<48)+7;d->qid.vers=0x12345678;d->qid.type=0x81;d->mode=0644;d->atime=123;d->mtime=456;d->type=17;d->dev=19;return d;
}
static void statentry(void*fp){
 F_Sys_fstat*f=fp;uchar*result=(uchar*)f->ret;String*old=H,*s,*expected;WORD status;uint i;int failure;Sys_Dir record;
 if(statrootfault)fstatcheck(0,"binder fourth root refused before builtin");
 statcalls++;fstatcheck(sizeof(*f)==72 && O(F_Sys_fstat,fd)==64 && O(F_Sys_fstat,ret)==32 && sizeof(*f->ret)==88 && sizeof(Sys_Dir)==80,"generated native result geometry");
 memset(result,0xa5,88);for(i=0;i<4;i++){s=H;if(statseed){if(statseed==2 && i){s=old;D2H(s)->ref++;}else{s=c2string("old",3);if(!i)old=s;}}memmove(result+8+8*i,&s,8);}
 if(statfailure)failat=allocations+statfailure;
 Sys_fstat(fp);failat=0;memmove(&status,result,4);failure=statnativefailure || (statfailure>=1 && statfailure<=5);fstatcheck(status==(failure?-1:0),"captured status");
 for(i=4;i<8;i++)fstatcheck(result[i]==0xa5,"tuple padding preserved");
 memmove(&record,result+8,sizeof(record));
 if(!failure){fstatcheck(record.length==statlength && record.qid.path==(1ULL<<48)+7 && record.qid.vers==0x12345678 && record.qid.qtype==0x81 && record.mode==0644 && record.atime==123 && record.mtime==456 && record.dtype==17 && record.dev==19,"all native scalar fields");}
 else for(i=40;i<88;i++)fstatcheck(result[i]==0xa5,"failed aggregate scalar tail unchanged");
 for(i=0;i<4;i++){
  memmove(&s,result+8+8*i,8);if(!failure || (!statnativefailure && statfailure>i+2)){
   expected=c2string(stattexts[i],strlen(stattexts[i]));fstatcheck(s!=H && s->len==expected->len,"published String length");fstatcheck(!memcmp(s->len<0?(void*)s->Srune:(void*)s->Sascii,expected->len<0?(void*)expected->Srune:(void*)expected->Sascii,(s->len<0?-s->len*sizeof(Rune):s->len)),"published complete String");destroy(expected);
  }else fstatcheck(statseed?s!=H:s==H,"failed String owner preserved");
 }
}
static Module*statbuiltin(void){
 Module*m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->name=strdup("$Sys");m->path=strdup("$Sys");require(m->name&&m->path);
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,72,(uchar*)stat_request_map,2);require(m->type[0]!=nil);m->ext=mallocz(2*sizeof(Link),1);require(m->ext!=nil);m->ext[0].name=strdup("fstat");require(m->ext[0].name!=nil);m->ext[0].sig=0xda4499c2U;m->ext[0].frame=m->type[0];m->ext[0].u.runt=statentry;return m;
}
static Modlink*statinstance(Module*m,const char*name,uint sig){Import request[2]={{0}};Modlink*ml;request[0].name=(char*)name;memmove(&request[0].sig,&sig,4);m->ref++;ml=linkmod(m,request,1);require(ml!=H);return ml;}
static Module*statlauncher(int jit,int grow,int fn){
 Module*m;Inst*p;uchar map[]={0,0xc0};m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,128,map,2);m->entryt=m->type[0];require(m->entryt!=nil);
 p=m->prog=mallocz(6*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=5;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=72;p[0].d.ind=40;
 p[1].op=IMOVP;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=64;p[1].d.i.f=40;p[1].d.i.s=64;
 p[2].op=ILEA;p[2].reg=(fn==2 || fn==4)?8:4;p[2].add=SRC(AFP)|DST(AFP|AIND);p[2].s.ind=80;p[2].d.i.f=40;p[2].d.i.s=32;
 p[3].op=IMCALL;p[3].add=SRC(AFP)|AXIMM|DST(AFP);p[3].s.ind=40;p[3].d.ind=72;p[4].op=IEXIT;p[4].add=SRC(AXXX)|DST(AXXX);
 if(fn==4){memmove(p+2,p+1,4*sizeof(*p));p[2].op=IMOVW;p[2].s.ind=56;p[2].d.i.s=72;m->nprog=6;}
 if(jit)require(compile(m,m->nprog,nil));return m;
}
static void statrun(int jit,int grow,int q,int fn,int seed,int failure,int nativefailure){
 Native64ModuleOwner client={0};Module*b,*outer;Modlink*c,*sys;Prog*process;uchar*root;void*fd;WORD number=23,status;LONG length;uint done,steps,i,rootbit=0;Type*damaged=nil;uchar savedmap=0;
 statseed=seed;statfailure=failure;statnativefailure=nativefailure;statcalls=0;require(native64typedmodule(&stat_spec,jit,&client));
 if(statrootfault){const Native64ModuleFunction*f=stat_functions+fn;rootbit=(f->code[f->sites[0].end-1].s+32)/8;damaged=client.module->type[fn+1];savedmap=damaged->map[rootbit/8];damaged->map[rootbit/8]&=~(0x80>>(rootbit%8));}
 b=statbuiltin();sys=statinstance(b,"fstat",0xda4499c2U);memmove(client.module->origmp+stat_sys_offset,&sys,8);
 c=statinstance(client.module,stat_functions[fn].name,stat_functions[fn].signature);
 {Type*t=dtype(freeheap,8,nil,0);require(t!=nil);fd=H2D(void*,heapz(t));memmove(fd,&number,4);freetype(t);}
 outer=statlauncher(jit,grow,fn);starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;
 {WORD marker=17;memmove(root+56,&marker,4);}memmove(root+64,&fd,8);memmove(root+72,&c,8);memset(root+80,0x5a,48);process->quanta=q;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}
 failat=0;if(statrootfault){fstatcheck(steps<10000 && !strcmp(worker.env->errstr,exBounds) && !statcalls,"fourth root rejected before builtin");goto cleanup;}if(steps>=10000 || worker.env->errstr[0])fprintf(stderr,"fstat jit=%d grow=%d fn=%d seed=%d failure=%d native=%d err=%s\n",jit,grow,fn,seed,failure,nativefailure,worker.env->errstr);
 fstatcheck(steps<10000 && !worker.env->errstr[0],"complete source call");fstatcheck(statcalls==(fn==3?2:1),"actual Sys.fstat count");
 if(fn==2 || fn==4){memmove(&length,root+80,8);fstatcheck(length==(nativefailure || (failure>=1&&failure<=5)?-1:statlength+(fn==4?18:0)),"source Dir.length full width");}
 else{memmove(&status,root+80,4);fstatcheck(status==(nativefailure || (failure>=1&&failure<=5)?-1:0),"source status result");for(i=84;i<88;i++)fstatcheck(root[i]==0x5a,"scalar result width");}
 for(i=88;i<128;i++)fstatcheck(root[i]==0x5a,"caller guards");cleanup: observed=process;progexit();emptyqueues();if(damaged)damaged->map[rootbit/8]=savedmap;require(client.module->ref==1);require(native64typedmoduleclear(&client));require(b->ref==1);unload(b);fstatcheck(!live && !nlinkpins && !modules && !worker.nerr && Tstring.ref==1 && Tmodlink.ref==1,"complete roots/module/type retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void statpreflight(void){
 Native64ModuleSpec spec=stat_spec;Native64ModuleFunction functions[5];Native64TypedStep code[128];Native64ModuleSite site;Native64ModuleImport request;Native64ModuleTable table;Native64ModuleOwner owner={0};uchar map[128],rmap[2];REG saved=R;uint base,bit;int i,before;
 for(i=0;i<19;i++){
  memmove(functions,stat_functions,sizeof(functions));spec.functions=functions;spec.tables=&table;request=stat_requests[0];table=(Native64ModuleTable){&request,1};request.map=rmap;memmove(rmap,stat_request_map,2);
  site=functions[1].sites[0];functions[1].sites=&site;memmove(code,functions[1].code,functions[1].count*sizeof(*code));functions[1].code=code;memmove(map,functions[1].map,functions[1].mapbytes);functions[1].map=map;base=code[site.end-1].s;
  if(i<4){bit=(base+8+8*i)/8;map[bit/8]&=~(0x80>>(bit%8));}
  if(i==4 || i==5 || i==6){bit=(base+(i==4?0:i==5?40:80))/8;map[bit/8]|=0x80>>(bit%8);}
  if(i==7)code[site.end-1].s++;
  if(i==8)code[site.end-1].s=functions[1].bytes-8;
  if(i==9)code[site.end-1].returned=88;
  if(i==10)site.contract=24;
  if(i==11)site.external=0;
  if(i==12)site.inherited=1;
  if(i==13)request.signature^=1;
  if(i==14)request.name="stat";
  if(i==15)request.bytes=80;
  if(i==16)rmap[1]=0;
  if(i==17)code[site.end-1].op=IMOVL;
  if(i==18)code[site.end-1].s=(1LL<<32)+base;
  before=allocations;if(native64typedmodule(&spec,0,&owner)){fprintf(stderr,"fstat metadata refusal removed: case=%d\n",i);exit(10);}
  if(allocations!=before)fprintf(stderr,"fstat preflight allocated at case=%d\n",i);
  fstatcheck(allocations==before && !live && !owner.module && !owner.rooted && !memcmp(&R,&saved,sizeof(R)),"metadata rejected before allocation");
 }
 puts("PASS fstat metadata: nineteen complete result/root/span/import/contract refusals before allocation");
}
static void statconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){before=allocations;require(native64typedmodule(&stat_spec,jit,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&stat_spec,jit,&owner));failat=0;fstatcheck(!owner.module && !owner.rooted && !live && !nlinkpins && !memcmp(&R,&saved,sizeof(R)),"constructor rollback");}
  printf("PASS fstat constructor: jit=%d %d allocation refusals and complete rollback\n",jit,n);
 }
}
int main(int argc,char**argv){int jit,g,q,fn,seed,failure,native,total=0;int quanta[]={1,3,1000};require(argc==1 || argc==2);
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 statpreflight();statconstruction();if(argc==2 && !strcmp(argv[1],"metadata"))return 0;
 STAT_HANDLERS
 optab[IMOVP]=actual_movp;optab[ILENC]=actual_lenc;
 if(argc==2){statrootfault=1;for(jit=0;jit<2;jit++)statrun(jit,1,1,1,0,0,0);puts("PASS fstat binder: fourth result root checked before builtin in C/JIT");return 0;}
 for(jit=0;jit<2;jit++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(fn=1;fn<5;fn++)for(seed=0;seed<3;seed++)for(failure=0;failure<7;failure++)for(native=0;native<2;native++){statrun(jit,g,quanta[q],fn,seed,failure,native);total++;}
 printf("PASS actual Sys.fstat: %d C/JIT cases, 88-byte results, all fields, four String roots/replacements/aliases, native/partial-allocation failures and full retirement\n",total);return 0;}
