#include "full.h"
#include "sink-native.h"
static int devicefault,deviceconses,devicecalls;static void*deviceexpected;
static void devcheck(int ok,const char*why){if(!ok){fprintf(stderr,"Device list predicate: %s\n",why);exit(10);}}
List*cons(ulong bytes,void*slot){List*value;deviceconses++;if(devicefault==deviceconses)failat=allocations+1;value=devicecons(bytes,slot);failat=0;return value;}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);devcheck(0,"unexpected LOAD");return H;}
static void devtake(void*frame){F_Sink_take*f=frame;List*next;
 devicecalls++;devcheck(D2H(f->devs)->ref==1,"prepared list owns one reference");devcheck(f->win==deviceexpected,"receiver survives list allocation");devcheck(f->devs!=H&&f->devs->tail!=H&&f->devs->tail->tail==H,"exact two-element list");next=f->devs->tail;argvtext(*(String**)f->devs->data,"kbd");argvtext(*(String**)next->data,"ptr");if(devicefault==3)error(exZdiv);
}
static void devseed(Native64ModuleOwner*owner){Module*m;Modlink*ml;const Native64ModuleImport*t=full_tables[0].imports;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->name=strdup("device-sink");m->path=strdup("device-sink");m->ntype=1;m->type=mallocz(sizeof(Type*),1);m->ext=mallocz(2*sizeof(Link),1);require(m->name&&m->path&&m->type&&m->ext);
 m->type[0]=dtype(freeheap,t->bytes,(uchar*)t->map,t->mapbytes);require(m->type[0]!=nil);m->ext[0].name=strdup("take");require(m->ext[0].name!=nil);memmove(&m->ext[0].sig,&t->signature,4);m->ext[0].frame=m->type[0];m->ext[0].u.runt=devtake;ml=linkmod(m,owner->module->ldt[0],0);require(ml!=H&&ml!=nil);memmove(owner->module->origmp+device_sink,&ml,8);
}
static Module*devroot(int grow){Module*m;Inst*p;uchar map[]={0,0x60};
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("device-caller");m->path=strdup("device-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->name&&m->path&&m->type);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);require(m->type[0]&&m->type[1]);p=m->prog=mallocz(4*sizeof(Inst),1);require(p!=nil);m->entry=p;m->nprog=4;m->cstep=native64typedlea8step;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=72;p[0].d.ind=40;p[1].op=IMOVP;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=80;p[1].d.i.f=40;p[1].d.i.s=64;p[2].op=IMCALL;p[2].add=SRC(AFP)|AXIMM|DST(AFP);p[2].s.ind=40;p[2].d.ind=72;p[3].op=IEXIT;p[3].add=SRC(AXXX)|DST(AXXX);return m;
}
static void devrun(int jit,int q,int absent,int mode){Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};Type*wt;uchar map[]={0x92,0xc4},*root;void*w;Prog*p;int i,done,steps;char*expected=mode==1||mode==2?exHeap:mode==3?exZdiv:"";
 devicefault=deviceconses=devicecalls=0;devcheck(native64typedmodule(&full_spec,jit,&owner),"owning constructor");devseed(&owner);wt=dtype(freeheap,sizeof(Wmclient_Window),map,sizeof(map));require(wt!=nil);w=H2D(void*,heapz(wt));ptradd(D2H(w));deviceexpected=absent?H:w;
 imports[0].name="run";memmove(&imports[0].sig,&full_functions[full_fn_run].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=H&&ml!=nil);m=devroot(q==1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+72,&ml,8);if(!absent){D2H(w)->ref++;memmove(root+80,&w,8);}memset(root+112,0x5a,16);devicefault=mode;
 p->quanta=q;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}failat=0;
 if(steps>=10000||strcmp(worker.env->errstr,expected))fprintf(stderr,"device jit=%d q=%d absent=%d mode=%d error=%s\n",jit,q,absent,mode,worker.env->errstr);
 devcheck(steps<10000&&!strcmp(worker.env->errstr,expected),"source execution completed");devcheck(deviceconses==(mode==1?1:2)&&devicecalls==(mode==1||mode==2?0:1),"actual cons and native service dispatch");for(i=112;i<128;i++)devcheck(root[i]==0x5a,"caller guard");
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);devcheck(native64typedmoduleclear(&owner),"module retirement");devcheck(D2H(w)->ref==1,"receiver retirement");ptrdel(D2H(w));destroy(w);freetype(wt);devcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1&&Tlist.ref==1&&Tptr.ref==1&&Tstring.ref==1,"complete managed retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void devrefusals(void){Native64ModuleSpec spec;Native64ModuleFunction f[2];Native64TypedStep code[512];Native64ModuleSite sites[2];Native64ModuleTable table;Native64ModuleImport request;Native64ModuleOwner owner={0};REG saved=R;uchar fmap[8],dmap[8],imap[8];int k,j,before;
 for(k=0;k<21;k++){
  spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[1].count==9);memmove(code,f[1].code,9*sizeof(*code));f[1].code=code;sites[0]=sites[1]=f[1].sites[0];f[1].sites=sites;table=full_tables[0];request=table.imports[0];table.imports=&request;spec.tables=&table;
  memmove(fmap,f[1].map,f[1].mapbytes);f[1].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;memmove(imap,request.map,request.mapbytes);request.map=imap;
  switch(k){case 0:code[2].returned=0;break;case 1:code[3].op=IMOVP;break;case 2:code[3].d=64;break;case 3:code[6].s=code[3].s;break;
  case 4:code[6].d=64;break;case 5:code[3].s++;break;case 6:code[6].returned=0;break;case 7:code[5].d=32;break;
  case 8:fmap[code[2].d/64]&=~(128>>((code[2].d/8)%8));break;case 9:dmap[code[3].s/64]&=~(128>>((code[3].s/8)%8));break;
  case 10:imap[1]&=~64;break;case 11:sites[0].external=0;break;case 12:f[1].nsites=0;break;case 13:f[1].nsites=2;break;
  case 14:code[5].s=64;break;case 15:code[6].op=ICONSP;break;case 16:code[2].op=ICONSP;break;
  case 17:code[2].mode=SRC(AFP)|DST(AFP);code[2].s=64;break;case 18:code[2].s=code[3].s;code[6].s=code[3].s;break;
  case 19:memmove(code+260,code+5,4*sizeof(*code));for(j=5;j<260;j++)code[j]=code[3];f[1].count=264;sites[0].end+=255;break;
  case 20:spec.strings=nil;break;}
  before=allocations;devcheck(!native64typedmodule(&spec,0,&owner),"malformed list metadata refused");devcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"list refusal before allocation");
 }
 puts("PASS pending list metadata: 21 init/cons/store/clear/map/call-island/257-cons refusals before allocation");
}
static void devconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;devcheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;devcheck(native64typedmoduleclear(&owner),"constructor clear");for(i=1;i<=n;i++){failat=allocations+i;devcheck(!native64typedmodule(&full_spec,jit,&owner),"constructor allocation failure");failat=0;devcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor atomic rollback");}printf("PASS pending list constructor: jit=%d %d allocation failures and complete rollback\n",jit,n);}
}
int main(void){int jit,q,absent,mode,total=0,quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 DEVICE_HANDLERS
 optab[IMOVP]=actual_movp;optab[ICONSP]=actual_consp;devrefusals();devconstruction();devcheck(limbo_native64_Sink_layout(),"actual generated payload/prefix layouts");for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(absent=0;absent<2;absent++)for(mode=0;mode<4;mode++){devrun(jit,quanta[q],absent,mode);total++;}printf("PASS pending String list: %d C/JIT calls, exact kbd/ptr order, nil receiver, first/second cons allocation failure, native service error and complete retirement; controlled service, no AWM window\n",total);return 0;
}
