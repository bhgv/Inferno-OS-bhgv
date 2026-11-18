#include "full.h"
#include "arg.h"
static Module *fullbuiltins[3],*fullcurrent;
static Type *fullfd;
static int fullmode,fullloads[4],fullbinds,fullopens,fullfildes,fullprints,fullsprints,fullcommands,fullarginits,fullopts,fullargs,fullargvs;
static void fullcheck(int ok,const char*text){if(!ok){fprintf(stderr,"full emuinit predicate: %s\n",text);exit(10);}}
static int fullprovided(void){return fullmode>=5;}
static int fullmissing(void){return fullmode==1||fullmode==2||fullmode==6||fullmode==7;}
static int fullterminal(void){return fullmode==2||fullmode==7;}
static void fulltext(String*s,const char*t){fullcheck(s!=nil&&s!=H&&s->len==(int)strlen(t)&&!memcmp(s->Sascii,t,s->len),"exact String operand");}
static void*fullret(void*fp){void*p;memmove(&p,(uchar*)fp+32,8);fullcheck(p!=nil&&p!=H,"full result address");return p;}
static void fullpointer(void*fp,void*value){void*p=fullret(fp),*old;memmove(&old,p,8);memmove(p,&value,8);destroy(old);}
static void fullscalar(void*fp,WORD value){memmove(fullret(fp),&value,4);}
static void fullbind(void*fp){String*a,*b;memmove(&a,(uchar*)fp+64,8);memmove(&b,(uchar*)fp+72,8);fulltext(a,"#e");fulltext(b,"/env");fullbinds++;fullscalar(fp,0);}
static void fullopen(void*fp){String*path;memmove(&path,(uchar*)fp+64,8);fulltext(path,"#e/emuargs");fullopens++;fullpointer(fp,H);}
static void fullfildesentry(void*fp){WORD n;Sys_FD*fd;memmove(&n,(uchar*)fp+64,4);fullcheck(n==2,"diagnostic FD number");fd=H2D(Sys_FD*,heapz(fullfd));fd->fd=2;fullfildes++;fullpointer(fp,fd);}
static void fullnever(void*fp){USED(fp);fullcheck(0,"unreached Sys method called");}
static void fullarginit(void*fp){List*l;memmove(&l,(uchar*)fp+64,8);fullcheck(l==H,"initial args from unavailable environment");fullarginits++;}
static void fullopt(void*fp){fullopts++;fullscalar(fp,fullmode==4&&fullopts==1?'g':0);}
static void fullarg(void*fp){fullargs++;fullpointer(fp,c2string("geometry",8));}
static void fullargv(void*fp){List*l=H;String*s;fullargvs++;
 if(fullprovided()){s=c2string("-x",2);R.s=&s;R.d=&l;actual_consp();destroy(s);s=c2string("myprog",6);R.s=&s;R.d=&l;actual_consp();destroy(s);}fullpointer(fp,l);}
static void fullcommand(void*fp){void*ctxt;List*l;String*s;const char*expected[5];int i,n=0;
 memmove(&ctxt,(uchar*)fp+64,8);memmove(&l,(uchar*)fp+72,8);fullcheck(ctxt==H,"Limbo nil Context");
 if(fullmissing()){expected[n++]="sh";expected[n++]="-c";expected[n++]="$*";}
 if(fullprovided()){expected[n++]="myprog";expected[n++]="-x";}else expected[n++]="-l";
 for(i=0;i<n;i++){fullcheck(l!=H&&l!=nil,"Command argv extent");memmove(&s,l->data,8);fulltext(s,expected[i]);l=l->tail;}
 fullcheck(l==H,"Command argv exact tail");fullcommands++;
}
/* The complete source invokes real frame/program services. These producers and
 * LOAD/OS effects are controlled; real formatting and nonempty getenv have
 * separate owning/GC gates. */
int Sys_sprint64(const Native64SysFormatRequest*r){String*s;void*p,*old;fullcheck(r->count==0,"empty sprint tail");fulltext(r->pattern,"%r");s=c2string("typed error",11);p=r->resolve(r->argument);fullcheck(p!=nil&&p!=H,"sprint result owner");memmove(&old,p,8);memmove(p,&s,8);destroy(old);fullsprints++;return 1;}
int Sys_fprint64(const Native64SysWriteRequest*r){String*s;WORD n=71;void*p;Sys_FD*fd=r->fd;fullcheck(fd!=nil&&fd!=H&&D2H(fd)->t==fullfd&&fd->fd==2,"fprint FD owner");
 if(!fullprints && fullmode<3){fulltext(r->format.pattern,"emuinit: cannot load %s: %r\n");fullcheck(r->format.count==1,"Arg diagnostic tail");memmove(&s,r->format.values,8);fulltext(s,"/dis/lib/arg.dis");}
 else{fullcheck(fullterminal(),"terminal diagnostic branch");fulltext(r->format.pattern,"emuinit: unable to load %s: %s\n");fullcheck(r->format.count==2,"terminal diagnostic tail");memmove(&s,r->format.values,8);fulltext(s,fullprovided()?"myprog":"/dis/sh.dis");memmove(&s,r->format.values+1,8);fulltext(s,"typed error");}
 p=r->format.resolve(r->format.argument);fullcheck(p!=nil&&p!=H,"fprint result owner");memmove(p,&n,4);fullprints++;return 1;}
int Sys_print64(const Native64SysWriteRequest*r){USED(r);fullcheck(0,"unexpected print");return 0;}
int Sys_aprint64(const Native64SysFormatRequest*r){USED(r);fullcheck(0,"unexpected aprint");return 0;}
static void(*fullfunction(unsigned space,const char*name))(void*){
 if(space==2)return fullcommand;
 if(space==1){if(!strcmp(name,"init"))return fullarginit;if(!strcmp(name,"opt"))return fullopt;if(!strcmp(name,"arg"))return fullarg;if(!strcmp(name,"argv"))return fullargv;}
 else{if(!strcmp(name,"bind"))return fullbind;if(!strcmp(name,"open"))return fullopen;if(!strcmp(name,"fildes"))return fullfildesentry;if(!strcmp(name,"sprint")||!strcmp(name,"fprint"))return Sys_programcall64;if(!strcmp(name,"read")||!strcmp(name,"fstat"))return fullnever;}
 fullcheck(0,"known imported method");return nil;
}
static Module*fullbuiltin(unsigned table){Module*m;unsigned i;const Native64ModuleTable*t=full_tables+table;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->ntype=t->count;m->name=strdup("controlled-service");m->path=strdup("controlled-service");require(m->name&&m->path);
 m->type=mallocz(t->count*sizeof(Type*),1);m->ext=mallocz((t->count+1)*sizeof(Link),1);require(m->type&&m->ext);
 for(i=0;i<t->count;i++){const Native64ModuleImport*r=t->imports+i;m->type[i]=dtype(freeheap,r->bytes,(uchar*)r->map,r->mapbytes);require(m->type[i]!=nil);m->ext[i].name=strdup(r->name);require(m->ext[i].name!=nil);memmove(&m->ext[i].sig,&r->signature,4);m->ext[i].frame=m->type[i];m->ext[i].u.runt=fullfunction(full_namespaces[table],r->name);}return m;
}
static Modlink*fullload(Module*caller,WORD table,char*path){Module*target;Modlink*ml;unsigned space;
 fullcheck(caller==fullcurrent && table>=0 && table<3,"LOAD caller/table");space=full_namespaces[table];target=fullbuiltins[space];
 if(space==0){fullcheck(!strcmp(path,"$Sys"),"Sys path");fullloads[0]++;}
 else if(space==1){fullcheck(!strcmp(path,"/dis/lib/arg.dis"),"Arg path");fullloads[1]++;if(fullmode<3)return H;}
 else if(!strcmp(path,"/dis/wm/awm.dis")){fullloads[2]++;if(fullmissing())return H;}
 else{fullcheck(!strcmp(path,"/dis/sh.dis"),"shell fallback path");fullloads[3]++;if(fullterminal())return H;}
 target->ref++;ml=linkmod(target,caller->ldt[table],1);fullcheck(ml!=nil&&ml!=H,"native service link");ptradd(D2H(ml));return ml;
}
static void fullrun(int jit,int quantum,int mode){Native64ModuleOwner owner={0};Prog*p;unsigned i,steps,done;int wantedprints;
 fullmode=mode;fullbinds=fullopens=fullfildes=fullprints=fullsprints=fullcommands=fullarginits=fullopts=fullargs=fullargvs=0;memset(fullloads,0,sizeof(fullloads));
 for(i=0;i<3;i++)fullbuiltins[full_namespaces[i]]=fullbuiltin(i);fullfd=dtype(freeheap,sizeof(Sys_FD),nil,0);require(fullfd!=nil);
 fullcheck(native64sysmodule(&full_spec,full_variadics,3,jit,&owner),"owning complete module constructor");fullcurrent=owner.module;fullcurrent->ref++;starting=fullcurrent;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);p->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 if(steps>=10000 || strcmp(worker.env->errstr,fullterminal()?"fail:error":""))fprintf(stderr,"full emuinit jit=%d q=%d mode=%d error=%s pc=%ld bind=%d open=%d fd=%d print=%d cmd=%d\n",jit,quantum,mode,worker.env->errstr,(long)(((uintptr)R.PC-(uintptr)fullcurrent->prog)/sizeof(Inst)),fullbinds,fullopens,fullfildes,fullprints,fullcommands);
 fullcheck(steps<10000&&!strcmp(worker.env->errstr,fullterminal()?"fail:error":""),"complete init outcome");
 fullcheck(fullbinds==1&&fullopens==1&&fullloads[0]==1&&fullloads[1]==1&&fullloads[2]==1&&fullloads[3]==fullmissing(),"exact startup paths");
 wantedprints=(mode<3)+fullterminal();fullcheck(fullprints==wantedprints&&fullfildes==wantedprints&&fullsprints==fullmissing()&&fullcommands==!fullterminal(),"diagnostics and direct Limbo Command.init");
 fullcheck(fullarginits==(mode>=3)&&fullopts==(mode>=3?(mode==4?2:1):0)&&fullargs==(mode==4)&&fullargvs==(mode>=3),"Arg lifecycle/case path");
 if(fullterminal()){REG saved=R;fullcheck(!handler(worker.env->errstr),"unhandled terminal raise");fullcheck(p->exval==H&&!memcmp(&R,&saved,sizeof(R)),"terminal exception owner retirement");}
 observed=p;progexit();emptyqueues();fullcheck(fullcurrent->ref==1&&native64typedmoduleclear(&owner),"full module retirement");fullcurrent=nil;
 for(i=0;i<3;i++){fullcheck(fullbuiltins[i]->ref==1,"service instance retirement");unload(fullbuiltins[i]);fullbuiltins[i]=nil;}freetype(fullfd);fullfd=nil;
 fullcheck(!live&&!nlinkpins&&!modules&&!worker.nerr&&Tstring.ref==1&&Tmodlink.ref==1&&Tlist.ref==1,"complete startup ownership retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
/* Malformed metadata is rejected before allocating or changing the output. */
static void fullrefusals(void){
 Native64ModuleVariadic v[3];Native64ModuleSpec spec;Native64ModuleFunction functions[4];Native64TypedStep code[1024];uchar map[128];Native64ModuleOwner owner={0};REG saved=R;
 unsigned i,n,f,first,end;int before;
 for(i=0;i<19;i++){
  memmove(v,full_variadics,sizeof(v));spec=full_spec;memmove(functions,full_functions,sizeof(functions));spec.functions=functions;f=v[0].function;n=functions[f].count;fullcheck(n<=nelem(code)&&v[0].mapbytes<=sizeof(map),"bounded refusal fixture");
  memmove(code,functions[f].code,n*sizeof(*code));functions[f].code=code;memmove(map,v[0].map,v[0].mapbytes);v[0].map=map;first=v[0].first;end=v[0].end;
  switch(i){
  case 0:map[0]=128;break;
  case 1:v[0].bytes+=8;break;
  case 2:v[0].count=257;break;
  case 3:v[0].operation=Native64SysPrint;break;
  case 4:v[0].contract^=8;break;
  case 5:v[0].function=4;break;
  case 6:v[0].moduleoffset=1;break;
  case 7:v[0].binding++;break;
  case 8:v[0].end=n;break;
  case 9:v[0].first=end;break;
  case 10:v[0].resultoffset=32;break;
  case 11:code[first].d=48;break;
  case 12:code[end].s=48;break;
  case 13:code[end-1].returned=0;break;
  case 14:code[first+1].d+=8;break;
  case 15:code[first+1].s=0;break;
  case 16:v[1]=v[0];break;
  case 17:code[n-1]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,first+1,0};break;
  case 18:v[0].mapbytes=0;break;
  }
  before=allocations;fullcheck(!native64sysmodule(&spec,v,3,0,&owner),"malformed full-module metadata refused");fullcheck(allocations==before&&!live&&!owner.module&&!owner.self&&!owner.rooted&&!memcmp(&R,&saved,sizeof(R)),"metadata refusal before allocation/publication");
 }
 puts("PASS complete emuinit metadata: 19 malformed maps/operations/contracts/slots/sites/branches refused before allocation");
}
/* String branch operands and MP slice results must remain mapped roots. */
static void argrefusals(void){Native64ModuleSpec spec;Native64ModuleFunction functions[8];Native64TypedStep code[1024];Native64ModuleOwner owner={0};REG saved=R;unsigned f,j,k,n,found=0;int before;
 for(k=0;k<5;k++){
  spec=arg_source_spec;memmove(functions,arg_source_functions,sizeof(functions));spec.functions=functions;
  for(f=0;f<8;f++){n=functions[f].count;fullcheck(n<=nelem(code),"bounded Arg refusal fixture");memmove(code,functions[f].code,n*sizeof(*code));
   for(j=0;j<n;j++)if(k<3?(code[j].op==IBEQC||code[j].op==IBNEC):(code[j].op==ISLICEC&&UDST(code[j].mode)==AMP))break;
   if(j<n)break;
  }
  fullcheck(f<8,"Arg String boundary coverage");functions[f].code=code;
  if(k==0){code[j].mode=(code[j].mode&~(7<<3))|SRC(AMP);code[j].s=arg_source_scalar;} /* Independent layout: scalar printusage. */
  if(k==1)code[j].d=n;
  if(k==2)code[j].mode=(code[j].mode&~ARM)|AXIMM;
  if(k==3)code[j].d++;
  if(k==4)code[j].d=arg_source_scalar;
  before=allocations;fullcheck(!native64sysmodule(&spec,arg_source_variadics,1,0,&owner),"malformed Arg String roles refused");fullcheck(before==allocations&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"Arg String refusal before allocation/publication");found++;
 }
 printf("PASS Arg String metadata: %u branch root/target/middle and MP slice refusals before allocation\n",found);
}
static void fullconstruction(void){Native64ModuleOwner owner={0};REG before=R;int jit,i,n,base,kind;
 const Native64ModuleSpec*specs[]={&full_spec,&arg_source_spec};const Native64ModuleVariadic*vars[]={full_variadics,arg_source_variadics};unsigned counts[]={3,1};
 for(kind=0;kind<2;kind++)for(jit=0;jit<2;jit++){base=allocations;fullcheck(native64sysmodule(specs[kind],vars[kind],counts[kind],jit,&owner),"constructor baseline");n=allocations-base;fullcheck(native64typedmoduleclear(&owner),"constructor clear");
  for(i=1;i<=n;i++){failat=allocations+i;fullcheck(!native64sysmodule(specs[kind],vars[kind],counts[kind],jit,&owner),"constructor failure refused");failat=0;fullcheck(!owner.module&&!owner.rooted&&!owner.self&&!live&&!nlinkpins&&!memcmp(&R,&before,sizeof(R)),"constructor atomic rollback");}
  printf("PASS complete %s constructor: jit=%d %d allocation failures, unchanged REG/output and complete rollback\n",specs[kind]->name,jit,n);
 }
}
int main(void){int jit,q,m,total=0;int quanta[]={1,3,1000};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 FULL_HANDLERS
 optab[ILOAD]=actual_iload;optab[IMOVP]=actual_movp;optab[ICONSP]=actual_consp;optab[IHEADP]=actual_headp;optab[ITAIL]=actual_tail;optab[ILENL]=actual_lenl;optab[ILENC]=actual_lenc;optab[ICASE]=actual_icase;optab[IRAISE]=actual_iraise;optab[IADDC]=actual_addc;
 fullrefusals();argrefusals();fullconstruction();for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(m=0;m<8;m++){fullrun(jit,quanta[q],m);total++;}
 printf("PASS complete unchanged emuinit init/loadmod: %d C/JIT launches, Arg missing/available/options/argv, exact AWM-first LOAD, shell fallback, terminal raise, Limbo nil/argv init and full retirement; services controlled, no AWM window\n",total);return 0;}
