#include "full.h"
static void mixcheck(int ok,const char*why){if(!ok){fprintf(stderr,"coordinate predicate: %s\n",why);exit(10);}}
static Module*mixedsource;
static uchar*mixedroot,*mixedcaller;static void*savedparent;
static int mixedfault,mixedcalls,mixedinjected;
int Sys_sprint64(const Native64SysFormatRequest*r){USED(r);mixcheck(0,"unexpected sprint");return 0;}
int Sys_aprint64(const Native64SysFormatRequest*r){Frame f;void*address;mixedcalls++;
 if(mixedfault==5)failat=allocations+1;
 if(mixedfault==6){memmove(&f,R.FP,sizeof(f));address=mixedroot+104;memmove((uchar*)f.fp+32,&address,8);memmove(R.FP+32,&address,8);}
 if(mixedfault==7){memmove(&f,R.FP,sizeof(f));mixedcaller=(uchar*)f.fp;memmove(&savedparent,mixedcaller+O(Frame,fp),8);address=nil;memmove(mixedcaller+O(Frame,fp),&address,8);}
 return productionSys_aprint64(r);
}
static void mixedframe(void){void*address;actual_frame();
 if(mixedsource&&R.M->m==mixedsource && mixedfault>0&&mixedfault<5&&!mixedinjected++){
  mixedcaller=R.FP;address=mixedfault==1?mixedroot+80:mixedfault==2?mixedroot+65:R.FP+64;
  if(mixedfault<4)memmove(R.FP+32,&address,8);
  else{memmove(&savedparent,R.FP+O(Frame,fp),8);address=nil;memmove(R.FP+O(Frame,fp),&address,8);}
 }
}

int Sys_print64(const Native64SysWriteRequest*r){USED(r);mixcheck(0,"unexpected print");return 0;}
int Sys_fprint64(const Native64SysWriteRequest*r){USED(r);mixcheck(0,"unexpected fprint");return 0;}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);mixcheck(0,"unexpected source LOAD");return H;}
static void outputequal(Array*a,const char*text){mixcheck(a!=nil&&a!=H,"owned Array result");mixcheck(a->len==(int)strlen(text)&&!memcmp(a->data,text,a->len),"exact byte formatting");}
static void seedsys(Native64ModuleOwner*owner){Module*m;Modlink*ml;const Native64ModuleImport*t=full_tables[0].imports;
 mixcheck(full_tables[0].count==1&&!strcmp(t->name,"aprint"),"canonical Sys aprint import");
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->name=strdup("typed-Sys");m->path=strdup("typed-Sys");m->ntype=1;m->type=mallocz(sizeof(Type*),1);m->ext=mallocz(2*sizeof(Link),1);require(m->name&&m->path&&m->type&&m->ext);
 m->type[0]=dtype(freeheap,t->bytes,(uchar*)t->map,t->mapbytes);require(m->type[0]!=nil);m->ext[0].name=strdup("aprint");require(m->ext[0].name!=nil);memmove(&m->ext[0].sig,&t->signature,4);m->ext[0].frame=m->type[0];m->ext[0].u.runt=Sys_programcall64;
 ml=linkmod(m,owner->module->ldt[0],0);require(ml!=H&&ml!=nil);memmove(owner->module->origmp+mixed_sys_offset,&ml,8);
}
static Module*rootmodule(Modlink*provider,int kind){Module*m;uchar map[]={0,0xc4};Inst*p;unsigned i,pc=0,offsets[4]={80,84,88,92},dest[4]={64,68,72,76},ops[4]={IMOVW,IMOVW,IMOVW,IMOVW};
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=4096;m->name=strdup("coordinate-caller");m->path=strdup("coordinate-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->name&&m->path&&m->type);
 m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);require(m->type[0]&&m->type[1]);p=m->prog=mallocz(8*sizeof(Inst),1);require(p!=nil);m->entry=p;m->nprog=8;m->cstep=native64typedlea8step;
 p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=72;p[pc++].d.ind=40;
 if(kind>=2){offsets[1]=88;offsets[2]=96;offsets[3]=104;dest[1]=72;dest[2]=80;dest[3]=88;ops[1]=IMOVL;ops[2]=IMOVF;ops[3]=IMOVP;}
 for(i=0;i<4;i++){p[pc].op=ops[i];p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=offsets[i];p[pc].d.i.f=40;p[pc++].d.i.s=dest[i];}
 p[pc].op=ILEA;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].reg=24;p[pc].s.ind=64;p[pc].d.i.f=40;p[pc++].d.i.s=32;
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=72;p[pc].op=IEXIT;p[pc].add=SRC(AXXX)|DST(AXXX);USED(provider);return m;
}
static void mixrun(int kind,int jit,int quantum,int row,int mode){Native64ModuleOwner owner={0};Module*m;Modlink*provider;Import imports[2]={{0}};Prog*p;uchar*root;Array*value,*old;String*seed;WORD words[4];LONG big;REAL real;int i,done,steps;char expected[512];
 const char*names[]={"wmrect","awmrect","mix","fresh"};const WORD cases[][4]={{0,0,0,0},{-1,2,-3,4},{(-2147483647-1),2147483647,-1,0},{123456,-654321,77,-999}};
 mixedfault=mode;mixedcalls=mixedinjected=0;mixedcaller=nil;savedparent=nil;
 mixcheck(native64sysmodule(&full_spec,full_variadics,4,jit,&owner),"source owning constructor");mixedsource=owner.module;seedsys(&owner);
 imports[0].name=(char*)names[kind];for(i=0;i<full_spec.nfunctions;i++)if(!strcmp(full_functions[i].name,names[kind])){memmove(&imports[0].sig,&full_functions[i].signature,4);break;}require(i<full_spec.nfunctions);
 owner.module->ref++;provider=linkmod(owner.module,imports,1);require(provider!=H&&provider!=nil);m=rootmodule(provider,kind);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;mixedroot=root;old=mem2array("old",3);ptradd(D2H(old));D2H(old)->ref++;memmove(root+64,&old,8);memset(root+112,0x5a,16);memmove(root+72,&provider,8);memmove(words,cases[row],sizeof(words));memmove(root+80,words,16);
 if(kind>=2){big=row&1?-9223372036854775807LL-1:9223372036854775807LL;real=row&1?-1.5:0.25;seed=c2string("h\xc3\xa9 \xce\xbb",6);memmove(root+88,&big,8);memmove(root+96,&real,8);memmove(root+104,&seed,8);snprintf(expected,sizeof(expected),"%d %lld %.2f h\xc3\xa9 \xce\xbb",words[0],(long long)big,real);}
 else snprintf(expected,sizeof(expected),"%d %d %d %d",words[0],words[1],words[2],words[3]);

 p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}failat=0;
 {const char*error=mode?(mode==5?exNomem:mode>=6?exCompile:exBounds):"";
 if(steps>=10000||strcmp(worker.env->errstr,error))fprintf(stderr,"coordinate kind=%d jit=%d q=%d row=%d mode=%d error=%s expected=%s\n",kind,jit,quantum,row,mode,worker.env->errstr,error);
 mixcheck(steps<10000&&!strcmp(worker.env->errstr,error),"source execution completed");}
 mixcheck(mixedcalls==((mode>0&&mode<5)?0:1),"actual formatter dispatch count");memmove(&value,root+64,8);
 if(!mode)outputequal(value,expected);else mixcheck(value==old,"failed inherited result preserved");mixcheck(D2H(old)->ref==(mode?2:1),"old Array result retired exactly once");
 for(i=112;i<128;i++)mixcheck(root[i]==0x5a,"caller result guard");
 if(mode==4||mode==7)memmove(mixedcaller+O(Frame,fp),&savedparent,8);
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);mixcheck(native64typedmoduleclear(&owner),"source owner retirement");mixcheck(D2H(old)->ref==1,"old Array fixture owner retired");ptrdel(D2H(old));destroy(old);mixcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tstring.ref==1&&Tarray.ref==1&&Tbyte.ref==1&&Tmodlink.ref==1,"complete formatter ownership retirement");memset(nativeobservations,0,sizeof(nativeobservations));mixedsource=nil;mixedroot=nil;
}
static void mixedrefusals(void){Native64ModuleSpec spec;Native64ModuleFunction functions[5];Native64ModuleVariadic vars[4];Native64TailSlot slots[8];Native64TypedStep code[128];uchar map[8];Native64ModuleOwner owner={0};REG saved=R;unsigned k,which,f,n,first;int before;
 for(k=0;k<16;k++){
  spec=full_spec;memmove(functions,full_functions,sizeof(functions));spec.functions=functions;memmove(vars,full_variadics,sizeof(vars));memmove(slots,vars[0].tail,4*sizeof(*slots));memmove(slots+4,vars[1].tail,4*sizeof(*slots));vars[0].tail=slots;vars[1].tail=slots+4;
  which=k==12||k==13?1:0;f=vars[which].function;n=functions[f].count;mixcheck(n<=nelem(code)&&vars[which].mapbytes<=sizeof(map),"bounded metadata fixture");memmove(code,functions[f].code,n*sizeof(*code));functions[f].code=code;first=vars[which].first;
  switch(k){
  case 0:slots[0].bytes=4;break;case 1:slots[0].offset=8;break;case 2:slots[0].kind=Native64TailByte;break;case 3:slots[0].type=&Tstring;break;
  case 4:slots[0].kind=Native64TailRef;slots[0].type=&Tstring;break;case 5:vars[0].tail=nil;break;case 6:vars[0].inherited=0;break;case 7:vars[0].inherited=2;break;case 8:vars[0].resultoffset=64;break;
  case 9:code[first+2].op=IMOVW;break;case 10:code[first+2].s+=2;break;case 11:code[first+2].mode=SRC(AMP)|DST(AFP|AIND);code[first+2].s=0;break;
  case 12:slots[7].type=&Tarray;break;case 13:code[first+3].op=IMOVF;break;
  case 14:memmove(map,vars[0].map,vars[0].mapbytes);map[0]=128;vars[0].map=map;break;case 15:slots[0].kind=Native64TailReal;break;
  }
  before=allocations;mixcheck(!native64sysmodule(&spec,vars,3,0,&owner),"mixed metadata refused");mixcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"mixed metadata refused before allocation");
 }
 for(k=0;k<12;k++){
  spec=full_spec;memmove(functions,full_functions,sizeof(functions));spec.functions=functions;memmove(vars,full_variadics,sizeof(vars));which=k%2+2;f=vars[which].function;n=functions[f].count;require(n<=nelem(code));memmove(code,functions[f].code,n*sizeof(*code));functions[f].code=code;
  for(first=vars[which].first+1;first<vars[which].end;first++)if(code[first].returned==95)break;require(first<vars[which].end);
  switch(k/2){case 0:code[first].op=IMOVW;break;case 1:code[first].mode=SRC(AFP)|DST(AFP|AIND);break;case 2:code[first].s++;break;case 3:code[first].m++;break;case 4:code[first].d=64;break;case 5:code[functions[f].count-1]=code[first];break;}
  before=allocations;mixcheck(!native64sysmodule(&spec,vars,4,0,&owner),"malformed Client formatter metadata refused");mixcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"Client metadata refused before allocation");
 }
 puts("PASS aprint metadata: 28 kind/width/map/store/result/Client-field refusals before allocation");
}
static void mixedconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;mixcheck(native64sysmodule(&full_spec,full_variadics,4,jit,&owner),"constructor baseline");n=allocations-base;mixcheck(native64typedmoduleclear(&owner),"constructor clear");
  for(i=1;i<=n;i++){failat=allocations+i;mixcheck(!native64sysmodule(&full_spec,full_variadics,4,jit,&owner),"constructor allocation failure refused");failat=0;mixcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor atomic rollback");}
  printf("PASS coordinate constructor: jit=%d %d allocation failures and complete rollback\n",jit,n);
 }
}
int main(void){int kind,jit,q,row,mode,total=0;int quanta[]={1,3,1000};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;Tarray.size=sizeof(Array);require(dis64selectlaunch("dis64-calls"));
 MIX_HANDLERS
 optab[INEWZ]=actual_newz;optab[INEW]=actual_new;optab[IFRAME]=mixedframe;optab[IMOVP]=actual_movp;optab[IADDC]=actual_addc;optab[ICVTWC]=actual_cvtwc;extern int _efgfmt(Fmt*);require(!fmtinstall('f',_efgfmt));
 mixedrefusals();mixedconstruction();
 for(kind=0;kind<4;kind++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(row=0;row<4;row++){mixrun(kind,jit,quanta[q],row,0);total++;}
 for(kind=0;kind<3;kind+=2)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(mode=1;mode<8;mode++){mixrun(kind,jit,quanta[q],2,mode);total++;}
 printf("PASS checked aprint: %d C/JIT calls, actual typed Sys formatter/lib9, FP/MP Client.token, owned byte arrays, Int/Big/Real/String tails, forwarded result, extreme coordinates, Unicode, seven result/producer failures and complete retirement; no AWM window\n",total);return 0;
}
