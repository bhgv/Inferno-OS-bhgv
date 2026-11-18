#include "full.h"
#include "draw-native.h"
static void drcheck(int ok,const char*why){if(!ok){fprintf(stderr,"Draw field predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);drcheck(0,"unexpected LOAD");return H;}
static const uint counts[]={3,3,5},sizes[]={72,32,40};
static const uchar maps[3][2]={{3,128},{112,0},{248,0}};
static const uint fields[3][5]={{48,56,64},{8,16,24},{0,8,16,24,32}};
static const uint words[]={0,4,8,12,16,20,24,28,32,36,40};
static Module*drroot(uint fn,int result,int grow){Module*m;Inst*p;uchar map[]={0,0xe0};uint pc=0;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("draw-caller");m->path=strdup("draw-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->name&&m->path&&m->type);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);require(m->type[0]&&m->type[1]);p=m->prog=mallocz(5*sizeof(Inst),1);require(p!=nil);m->entry=p;m->cstep=native64typedlea8step;
 p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=72;p[pc++].d.ind=40;
 if(fn<3 || ((fn-3)%7)!=2&&((fn-3)%7)!=3&&((fn-3)%7)!=5){p[pc].op=IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=80;p[pc].d.i.f=40;p[pc++].d.i.s=64;}
 if(result){p[pc].op=ILEA;p[pc].reg=24;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64;p[pc].d.i.f=40;p[pc++].d.i.s=32;}
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=72;p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;return m;
}
static Modlink*drinstance(Native64ModuleOwner*owner,uint fn){Import imports[2]={{0}};Modlink*ml;imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);owner->module->ref++;ml=linkmod(owner->module,imports,1);require(ml!=nil&&ml!=H);return ml;}
static void drfields(uint kind,uint action,int jit,int quantum,uint mode){Native64ModuleOwner owner={0};uint n=counts[kind],fn=3+kind*7+action,i,mask=mode==2?(1U<<n)-1:mode>=3?1U<<(mode-3):0,local=action!=2&&action!=3&&action!=5;Module*m;Modlink*ml;Type*type,*child;void*children[10],*value;uchar*object,*root;Prog*p;int done,steps,expected;
 drcheck(native64typedmodule(&full_spec,jit,&owner),"owning source constructor");type=dtype(freeheap,sizes[kind],(uchar*)maps[kind],(sizes[kind]+63)/64);child=dtype(freeheap,8,nil,0);require(type&&child);object=H2D(uchar*,heapz(type));ptradd(D2H(object));
 for(i=0;i<2*n;i++){children[i]=kind==0&&i%n==2?(void*)c2string("name",4):H2D(void*,heapz(child));ptradd(D2H(children[i]));if(i<n&&!(mask&(1U<<i))){D2H(children[i])->ref++;memmove(object+fields[kind][i],children+i,8);}}
 ml=drinstance(&owner,fn);for(i=n;i<2*n;i++){D2H(children[i])->ref++;memmove(ml->MP+draw_offsets[kind][i-n],children+i,8);}if(!local&&mode!=1){D2H(object)->ref++;memmove(ml->MP+draw_offsets[kind][n],&object,8);}
 m=drroot(fn,0,mode&1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+72,&ml,8);if(mode!=1){D2H(object)->ref++;memmove(root+80,&object,8);}memset(root+112,0x5a,16);
 for(i=0;i<(kind==0?11:kind==1?1:0);i++){UWORD guard=0x60000000U+i;memmove(object+words[i],&guard,4);}
 p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 drcheck(steps<10000&&!strcmp(worker.env->errstr,mode==1?exNilref:""),"root source execution completed");
 for(i=0;i<n;i++){void*original=(mask&(1U<<i))?H:children[i];memmove(&value,object+fields[kind][i],8);drcheck(value==(mode==1||action==0||action==2||action==6?original:action==1||action==3?children[i+n]:H),"exact Draw payload field value");memmove(&value,ml->MP+draw_offsets[kind][i],8);drcheck(value==(mode!=1&&(action==0||action==2||action==6)?original:children[i+n]),"exact MP field value");}
 for(i=0;i<2*n;i++){expected=1;if(mode==1)expected++;else if(action==0||action==2||action==6){if(i<n&&!(mask&(1U<<i)))expected+=2;}else if(action==1||action==3){if(i>=n)expected+=2;}else if(i>=n)expected++;drcheck(D2H(children[i])->ref==expected,"exact field reference retirement");}
 for(i=0;i<(kind==0?11:kind==1?1:0);i++){UWORD guard;memmove(&guard,object+words[i],4);drcheck(guard==0x60000000U+i,"scalar/Rect fields preserved");}for(i=112;i<128;i++)drcheck(root[i]==0x5a,"caller guard");
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);drcheck(native64typedmoduleclear(&owner),"source owner retired");drcheck(D2H(object)->ref==1,"payload fixture owner");ptrdel(D2H(object));destroy(object);for(i=0;i<2*n;i++){drcheck(D2H(children[i])->ref==1,"child fixture owner");ptrdel(D2H(children[i]));destroy(children[i]);}freetype(type);freetype(child);drcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1&&Tstring.ref==1,"complete root ownership retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
static Type*drtypes[3];static void*drexpected;static int drfault,drcalls[2];
static void*drnew(int kind){return H2D(void*,heapz(drtypes[kind]));}
static void drcolor(void*frame){void*d,*value,*old;void**result;WORD color;drcalls[0]++;memmove(&d,(uchar*)frame+64,8);memmove(&color,(uchar*)frame+72,4);memmove(&result,(uchar*)frame+32,8);drcheck(d==drexpected&&color==123,"actual Display.color prefix/receiver");if(d==H)error(exNilref);if(drfault==3)failat=allocations+1;value=drnew(0);failat=0;D2H(d)->ref++;memmove((uchar*)value+48,&d,8);memmove(&old,result,8);memmove(result,&value,8);destroy(old);}
static void drallocate(void*frame){void*image,*fill,*value,*display,*old;void**result;WORD pub;drcalls[1]++;memmove(&image,(uchar*)frame+64,8);memmove(&fill,(uchar*)frame+72,8);memmove(&pub,(uchar*)frame+80,4);memmove(&result,(uchar*)frame+32,8);drcheck(image!=H&&fill!=H&&pub==0,"actual Screen.allocate prefix");drcheck(D2H(fill)->ref==1,"temporary fill cleared before allocate");memmove(&display,(uchar*)image+48,8);if(drfault==4)failat=allocations+1;value=drnew(1);failat=0;D2H(image)->ref++;D2H(fill)->ref++;D2H(display)->ref++;memmove((uchar*)value+8,&image,8);memmove((uchar*)value+16,&fill,8);memmove((uchar*)value+24,&display,8);memmove(&old,result,8);memmove(result,&value,8);destroy(old);}
static void drseed(Native64ModuleOwner*owner){Module*m;Modlink*ml;const Native64ModuleImport*t;uint i;m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->name=strdup("controlled-Draw");m->path=strdup("controlled-Draw");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);m->ext=mallocz(3*sizeof(Link),1);require(m->name&&m->path&&m->type&&m->ext);
 for(i=0;i<2;i++){t=full_tables[0].imports+i;m->type[i]=dtype(freeheap,t->bytes,(uchar*)t->map,t->mapbytes);require(m->type[i]!=nil);m->ext[i].name=strdup(t->name);require(m->ext[i].name!=nil);memmove(&m->ext[i].sig,&t->signature,4);m->ext[i].frame=m->type[i];m->ext[i].u.runt=!strcmp(t->name,"Display.color")?drcolor:drallocate;}ml=linkmod(m,owner->module->ldt[0],0);require(ml!=nil&&ml!=H);memmove(owner->module->origmp+draw_module_offset,&ml,8);
}
static void drcall(uint fn,int jit,int quantum,int mode){Native64ModuleOwner owner={0};Module*m;Modlink*ml;uchar*root;void*input,*display,*marker,*value,*field;Prog*p;uint i;int done,steps;char*error=mode==1||mode==2?exNilref:mode>=3?exHeap:"";
 drfault=0;drcalls[0]=drcalls[1]=0;drcheck(native64typedmodule(&full_spec,jit,&owner),"call source constructor");drseed(&owner);for(i=0;i<3;i++){drtypes[i]=dtype(freeheap,sizes[i],(uchar*)maps[i],(sizes[i]+63)/64);require(drtypes[i]!=nil);}input=drnew(0);display=drnew(2);marker=drnew(0);ptradd(D2H(input));ptradd(D2H(display));ptradd(D2H(marker));if(mode!=2){D2H(display)->ref++;memmove((uchar*)input+48,&display,8);}drexpected=mode==2?H:display;
 ml=drinstance(&owner,fn);m=drroot(fn,1,quantum==1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+72,&ml,8);D2H(marker)->ref++;memmove(root+64,&marker,8);if(mode!=1){D2H(input)->ref++;memmove(root+80,&input,8);}memset(root+112,0x5a,16);drfault=mode;p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}failat=0;
 if(steps>=10000||strcmp(worker.env->errstr,error))fprintf(stderr,"Draw call fn=%u jit=%d q=%d mode=%d error=%s expected=%s\n",fn,jit,quantum,mode,worker.env->errstr,error);drcheck(steps<10000&&!strcmp(worker.env->errstr,error),"field argument execution completed");memmove(&value,root+64,8);drcheck(*error?value==marker:value!=H&&value!=marker,"result replacement/failure preserved");if(!*error){memmove(&field,(uchar*)value+(fn==1?48:8),8);drcheck(field==(fn==1?display:input),"returned handle aliases");}drcheck(drcalls[0]==(mode==1?0:1)&&drcalls[1]==(fn==2&&(mode==0||mode==4)?1:0),"actual controlled Draw dispatch");for(i=112;i<128;i++)drcheck(root[i]==0x5a,"call guard");observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);drcheck(native64typedmoduleclear(&owner),"call module retired");drcheck(D2H(input)->ref==1&&D2H(marker)->ref==1,"input/result owners retired");ptrdel(D2H(input));destroy(input);drcheck(D2H(display)->ref==1,"display aliases retired");ptrdel(D2H(display));destroy(display);ptrdel(D2H(marker));destroy(marker);for(i=0;i<3;i++)freetype(drtypes[i]);drcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1,"complete Draw call retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void drrefusals(void){Native64ModuleSpec spec;Native64ModuleFunction f[24];Native64TypedStep code[64];Native64ModuleSite sites[3];Native64ModuleOwner owner={0};Native64ModuleTable table;Native64ModuleImport imports[2];uchar fmap[16],dmap[32],imap[8];REG saved=R;uint k,fn,pc,j;int before;
 for(k=0;k<32;k++){
  fn=k<12?3:k<22?1:2;spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;
  memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;
  if(f[fn].nsites)memmove(sites,f[fn].sites,f[fn].nsites*sizeof(*sites));f[fn].sites=sites;
  table=full_tables[0];memmove(imports,table.imports,sizeof(imports));table.imports=imports;spec.tables=&table;
  for(pc=0;pc<f[fn].count;pc++)if(code[pc].returned==(k<12?113:k<22?125:126))break;require(pc<f[fn].count);
  switch(k){
  case 0:code[pc].m=40;break;case 1:code[pc].m=49;break;case 2:code[pc].m=-8;break;case 3:code[pc].op=IMOVW;break;
  case 4:code[pc].s++;break;case 5:code[pc].d++;break;case 6:code[pc].mode|=AXIMM;break;case 7:fmap[code[pc].s/64]&=~(128>>((code[pc].s/8)%8));break;
  case 8:dmap[code[pc].d/64]&=~(128>>((code[pc].d/8)%8));break;case 9:code[0].returned=116;break;case 10:code[0].d=f[fn].count;break;case 11:code[0].m++;break;
  case 12:code[pc].m=40;break;case 13:code[pc].m++;break;case 14:code[pc].s++;break;case 15:code[pc].d++;break;case 16:code[pc].mode=SRC(AFP)|DST(AFP|AIND);break;
  case 17:f[fn].nsites=0;break;case 18:sites[1]=sites[0];f[fn].nsites=2;break;case 19:code[sites[0].first].op=IFRAME;break;
  case 20:fmap[code[pc].s/64]&=~(128>>((code[pc].s/8)%8));break;
  case 21:j=sites[0].target;memmove(imap,imports[j].map,imports[j].mapbytes);imports[j].map=imap;imap[code[pc].d/64]&=~(128>>((code[pc].d/8)%8));break;
  case 22:code[pc].op=ICONSP;break;case 23:code[pc].s++;break;case 24:code[pc].d++;break;case 25:code[pc].mode|=AXIMM;break;
  case 26:code[pc-1].s+=8;break;case 27:code[pc-1].d=80;break;case 28:f[fn].nsites=0;break;
  case 29:fmap[code[pc].d/64]&=~(128>>((code[pc].d/8)%8));break;case 30:dmap[code[pc].s/64]&=~(128>>((code[pc].s/8)%8));break;
  case 31:spec.nstrings=1;spec.strings=nil;break;}
  before=allocations;drcheck(!native64typedmodule(&spec,0,&owner),"malformed Draw metadata refused");drcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"Draw refusal before allocation");
 }puts("PASS Draw metadata: 32 field/branch/argument/clear/root-map refusals before allocation");
}
static void drconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;drcheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;drcheck(native64typedmoduleclear(&owner),"constructor clear");
  for(i=1;i<=n;i++){failat=allocations+i;drcheck(!native64typedmodule(&full_spec,jit,&owner),"constructor allocation failure");failat=0;drcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor atomic rollback");}
  printf("PASS Draw constructor: jit=%d %d allocation failures and complete rollback\n",jit,n);
 }
}
int main(void){uint kind,action,mode,fn,total=0;int jit,q,quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 DRAW_HANDLERS
 optab[IMOVP]=actual_movp;drrefusals();drconstruction();drcheck(limbo_native64_Draw_layout()&&sizeof(Draw_Image)==72&&sizeof(Draw_Screen)==32&&sizeof(Draw_Display)==40,"generated Draw native C layouts");for(kind=0;kind<3;kind++)for(action=0;action<7;action++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(mode=0;mode<counts[kind]+3;mode++){drfields(kind,action,jit,quanta[q],mode);total++;}for(fn=1;fn<3;fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(mode=0;mode<(fn==1?4:5);mode++){drcall(fn,jit,quanta[q],mode);total++;}printf("PASS Draw fields/calls: %u C/JIT cases, 11 mapped fields, pointer nil branches, Image.display argument, fresh/inherited handles, fill clear, allocation failures and complete retirement; controlled Draw services, no AWM window\n",total);return 0;
}
