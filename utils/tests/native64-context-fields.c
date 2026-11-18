#include "full.h"
static void ctxcheck(int ok,const char*why){if(!ok){fprintf(stderr,"Context predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);ctxcheck(0,"unexpected LOAD");return H;}
static const uint ctxoffsets[]={context_display,context_screen,context_wm};
static Module*ctxroot(int local){Module*m;Inst*p;uchar map[]={0,0x60};int pc=0;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=4096;m->name=strdup("context-caller");m->path=strdup("context-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->name&&m->path&&m->type);
 m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);require(m->type[0]&&m->type[1]);p=m->prog=mallocz(4*sizeof(Inst),1);require(p!=nil);m->entry=p;m->cstep=native64typedlea8step;
 p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=72;p[pc++].d.ind=40;
 if(local){p[pc].op=IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=80;p[pc].d.i.f=40;p[pc++].d.i.s=64;}
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=72;
 p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;return m;
}
static void ctxrun(uint fn,int jit,int quantum,int mode){Native64ModuleOwner owner={0};Module*m;Modlink*provider;Import imports[2]={{0}};Prog*p;uchar*root,*ctxt;void*children[6],*v;Type*ct,*child;uchar map[]={0xe0};int i,n,steps,done,missing=mode==1,mask=mode==2?7:mode>=3?1<<(mode-3):0,local=fn==0||fn==1||fn==4||fn==6;
 ctxcheck(native64typedmodule(&full_spec,jit,&owner),"owning source module");
 ct=dtype(freeheap,24,map,1);child=dtype(freeheap,8,nil,0);require(ct&&child);ctxt=H2D(uchar*,heapz(ct));ptradd(D2H(ctxt));
 for(i=0;i<6;i++){children[i]=H2D(void*,heapz(child));ptradd(D2H(children[i]));if(i<3 && !(mask&(1<<i))){D2H(children[i])->ref++;memmove(ctxt+8*i,children+i,8);}}
 imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);owner.module->ref++;provider=linkmod(owner.module,imports,1);require(provider!=H&&provider!=nil);
 for(i=3;i<6;i++){D2H(children[i])->ref++;memmove(provider->MP+ctxoffsets[i-3],children+i,8);}
 if(!local&&!missing){D2H(ctxt)->ref++;memmove(provider->MP+context_saved,&ctxt,8);}
 m=ctxroot(local);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+72,&provider,8);memset(root+112,0x5a,16);
 if(!missing){D2H(ctxt)->ref++;memmove(root+80,&ctxt,8);}
 p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 if(steps>=10000||strcmp(worker.env->errstr,missing?exNilref:""))fprintf(stderr,"ctx fn=%u jit=%d q=%d missing=%d error=%s\n",fn,jit,quantum,missing,worker.env->errstr);
 ctxcheck(steps<10000&&!strcmp(worker.env->errstr,missing?exNilref:""),"source execution completed");
 for(i=0;i<3;i++){
  memmove(&v,ctxt+8*i,8);ctxcheck(v==(missing||fn==0||fn==2||fn==6?((mask&(1<<i))?H:children[i]):fn==1||fn==3?children[i+3]:H),"exact Context field value");
  memmove(&v,provider->MP+ctxoffsets[i],8);ctxcheck(v==(!missing&&(fn==0||fn==2||fn==6)?((mask&(1<<i))?H:children[i]):children[i+3]),"exact MP field value");
 }
 for(i=0;i<6;i++){
  n=1;if(missing)n++;else if(fn==0||fn==2||fn==6){if(i<3&&!(mask&(1<<i)))n+=2;}else if(fn==1||fn==3){if(i>=3)n+=2;}else if(i>=3)n++;
  ctxcheck(D2H(children[i])->ref==n,"reference replacement and old value retirement");
 }
 for(i=112;i<128;i++)ctxcheck(root[i]==0x5a,"root guard preserved");
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);ctxcheck(native64typedmoduleclear(&owner),"source retirement");
 ctxcheck(D2H(ctxt)->ref==1,"Context retains only fixture owner");ptrdel(D2H(ctxt));destroy(ctxt);
 for(i=0;i<6;i++){ctxcheck(D2H(children[i])->ref==1,"child retains only fixture owner");ptrdel(D2H(children[i]));destroy(children[i]);}freetype(ct);freetype(child);
 ctxcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1,"complete ownership retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void ctxrefusals(void){Native64ModuleSpec spec;Native64ModuleFunction f[7];Native64TypedStep code[64];uchar fmap[8],dmap[8];Native64ModuleOwner owner={0};Native64ModuleSite site={0,3,0};REG saved=R;uint k,pc;int before;
 for(k=0;k<22;k++){
  spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[0].count<=nelem(code));memmove(code,f[0].code,f[0].count*sizeof(*code));f[0].code=code;for(pc=0;pc<f[0].count;pc++)if(code[pc].returned==96)break;require(pc<f[0].count);
  memmove(fmap,f[0].map,f[0].mapbytes);f[0].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;
  switch(k){case 0:code[pc].m=4;break;case 1:code[pc].m=24;break;case 2:code[pc].m=-8;break;case 3:code[pc].op=IMOVW;break;
  case 4:code[pc].mode=SRC(AIMM)|DST(AMP);break;case 5:code[pc].mode=SRC(AFP|AIND)|DST(AIMM);break;case 6:code[pc].s++;break;case 7:code[pc].d++;break;
  case 8:code[pc].mode|=AXIMM;break;case 9:fmap[1]=0;break;case 10:dmap[context_display/64]&=~(128>>((context_display/8)%8));break;
  case 11:code[pc].s=f[0].bytes;break;case 12:code[pc].d=spec.databytes;break;
  case 13:code[0].m++;break;case 14:code[0].m=spec.databytes;break;case 15:code[0].s=f[0].bytes;break;
  case 16:code[0].d=f[0].count;break;case 17:code[0].mode|=256;break;case 18:code[0].op=IBEQW;break;
  case 19:dmap[code[0].m/64]&=~(128>>((code[0].m/8)%8));break;case 20:code[0].mode=(code[0].mode&~ARM)|AXINF;break;
  case 21:f[0].nsites=1;f[0].sites=&site;break;}
  before=allocations;ctxcheck(!native64typedmodule(&spec,0,&owner),"malformed Context metadata refused");ctxcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"Context refusal before allocation");
 }
 puts("PASS Context metadata: 22 field/branch/geometry/root-map refusals before allocation");
}
static void ctxconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;ctxcheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;ctxcheck(native64typedmoduleclear(&owner),"constructor clear");
  for(i=1;i<=n;i++){failat=allocations+i;ctxcheck(!native64typedmodule(&full_spec,jit,&owner),"constructor allocation failure");failat=0;ctxcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor atomic rollback");}
  printf("PASS Context constructor: jit=%d %d allocation failures and complete rollback\n",jit,n);
 }
}
int main(void){int fn,jit,q,missing,total=0;int quanta[]={1,3,1000};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;require(dis64selectlaunch("dis64-calls"));
 CONTEXT_HANDLERS
 optab[IMOVP]=actual_movp;ctxrefusals();ctxconstruction();
 for(fn=0;fn<7;fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(missing=0;missing<6;missing++){ctxrun(fn,jit,quanta[q],missing);total++;}
 printf("PASS Draw.Context fields: %d C/JIT calls, FP/MP owners and values, three exact fields, replacement/nil clearing/nil owner, refcounts and complete retirement; no Draw service or AWM window\n",total);return 0;
}
