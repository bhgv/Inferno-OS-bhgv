#include "geometry-provider.h"
#include "full.h"
static void geocheck(int ok,const char*why){if(!ok){fprintf(stderr,"Geometry predicate: %s\n",why);exit(10);}}
static Modlink*geometryload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);geocheck(0,"unexpected LOAD");return H;}
static Modlink*fullload(Module*m,WORD t,char*p){return geometryload(m,t,p);}
static void*georootextent;static uint geogrowth;
static void geocopy(void){if(R.EX!=georootextent)geogrowth++;actual_movm();}
static const uint geoobserved[]={geometry_a,geometry_b,geometry_c,geometry_d};
static const WORD geocases[][6]={{0,0,0,0,0,0},{-1,2,-3,4,-5,6},{(-2147483647-1),2147483647,-1,0,123,-456},{123456,-654321,77,-999,2048,-4096}};
static const BYTE geotags[]={0,255,128,37};
static UWORD geofold(const WORD*v){return (UWORD)v[0]^((UWORD)v[1]<<1)^((UWORD)v[2]<<2)^((UWORD)v[3]<<3);}
static Module*georoot(uint fn,int grow){Module*m;Inst*p;const Native64ModuleFunction*f=full_functions+fn;uint i,pc=0,n=geometry_arguments[fn].n,result=f->code[f->count-2].returned,bytes=f->bytes+80;uchar map[64]={0};
 require((bytes+63)/64<=sizeof(map));map[1]=0x40;for(i=0;i<f->mapbytes;i++)map[i+1]|=f->map[i];
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("geometry-caller");m->path=strdup("geometry-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->name&&m->path&&m->type);
 m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,bytes,map,(bytes+63)/64);require(m->type[0]&&m->type[1]);p=m->prog=mallocz((n+4)*sizeof(Inst),1);require(p!=nil);m->entry=p;m->cstep=native64typedlea8step;
 p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=72;p[pc++].d.ind=40;
 for(i=0;i<n;i++){const uint*a=geometry_arguments[fn].args[i];p[pc].op=a[2]==11||a[2]==12?IMOVP:a[1]==1?IMOVB:a[1]==4?IMOVW:IMOVL;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=a[0]+64;p[pc].d.i.f=40;p[pc++].d.i.s=a[0];}
 /* Test exports are all Word-returning except the two void reshape callers. */
 if(fn!=full_fn_literal&&fn!=full_fn_fields){p[pc].op=ILEA;p[pc].reg=4;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64;p[pc].d.i.f=40;p[pc++].d.i.s=32;}
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=72;p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;USED(result);return m;
}
static void georun(uint fn,int pj,int cj,int quantum,uint row,int mode){Native64ModuleOwner po={0},co={0};Module*m;Modlink*provider,*consumer;Import imports[2]={{0}};Type*ct;uchar map[]={0xe0},*ctxt,*root;Prog*p;uint i,wi=0,guard=full_functions[fn].bytes+64;WORD actual;UWORD expected=0x5aa55aa5U;WORD coordinates[6],values[4]={0};const char*error=mode==1?exModule:mode==2?"fail:geometry":"";LONG big=row&1?-9223372036854775807LL-1:9223372036854775807LL;int done,steps;
 memmove(coordinates,geocases[row],sizeof(coordinates));if(mode==2)coordinates[fn==full_fn_mixed?2:0]=777;
 geocheck(native64typedmodule(&geometry_provider_spec,pj,&po),"owning provider");geocheck(native64typedmodule(&full_spec,cj,&co),"owning consumer");po.module->ref++;provider=linkmod(po.module,co.module->ldt[0],1);require(provider!=H&&provider!=nil);ptradd(D2H(provider));
 imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);co.module->ref++;consumer=linkmod(co.module,imports,1);require(consumer!=H&&consumer!=nil);ptradd(D2H(consumer));
 ct=dtype(freeheap,24,map,1);require(ct!=nil);ctxt=H2D(uchar*,heapz(ct));ptradd(D2H(ctxt));m=georoot(fn,row&1);if(cj)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+72,&consumer,8);ptrdel(D2H(consumer));memmove(root+64,&expected,4);memset(root+guard,0x5a,16);
 for(i=0;i<geometry_arguments[fn].n;i++){const uint*a=geometry_arguments[fn].args[i];uchar*cell=root+64+a[0];
  if(a[2]==11){Modlink*v=mode==1?H:provider;memmove(cell,&v,8);}else if(a[2]==12){D2H(ctxt)->ref++;memmove(cell,&ctxt,8);}else if(a[2]==5)memmove(cell,geotags+row,1);else if(a[2]==4)memmove(cell,&big,8);else{require(a[2]==9&&wi<6);memmove(cell,&coordinates[wi++],4);}
 }
 if(mode!=1)ptrdel(D2H(provider));
 if(fn==full_fn_literal){values[2]=values[3]=100;}
 else if(fn==full_fn_global){for(i=0;i<4;i++)values[i]=i+1;}
 else if(fn==full_fn_mixed)memmove(values,coordinates+2,16);
 else memmove(values,coordinates,fn==full_fn_point?8:16);
 if(fn!=full_fn_literal&&fn!=full_fn_fields){expected=geofold(values);if(fn==full_fn_freshrect)expected+=7;if(fn==full_fn_mixed)expected^=(UWORD)geocases[row][0]^((UWORD)geocases[row][1]<<1)^geotags[row]^(UWORD)big;}
 if(mode){expected=0x5aa55aa5U;memset(values,0,sizeof(values));}
 georootextent=p->R.EX;geogrowth=0;p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 if(steps>=10000||strcmp(worker.env->errstr,error))fprintf(stderr,"geometry fn=%u jit=%d/%d q=%d row=%u mode=%d error=%s\n",fn,pj,cj,quantum,row,mode,worker.env->errstr);
 geocheck(steps<10000&&!strcmp(worker.env->errstr,error),"source execution completed");if(!mode&&(row&1)&&!cj&&fn!=full_fn_literal&&fn!=full_fn_fields)geocheck(geogrowth>0,"actual stack extent growth");memmove(&actual,root+64,4);geocheck((UWORD)actual==expected,"exact Word result and fresh/forwarded ABI");
 for(i=0;i<4;i++){memmove(&actual,provider->MP+geoobserved[i],4);geocheck(actual==values[i],"ordered Point/Rect values in actual provider");}
 if(fn==full_fn_mixed&&!mode){void*v;LONG seen;memmove(&actual,provider->MP+geometry_px,4);geocheck(actual==geocases[row][0],"mixed Point.x");memmove(&actual,provider->MP+geometry_py,4);geocheck(actual==geocases[row][1],"mixed Point.y");memmove(&actual,provider->MP+geometry_t,4);geocheck(actual==geotags[row],"mixed byte");memmove(&seen,provider->MP+geometry_last,8);geocheck(seen==big,"mixed Big");memmove(&v,provider->MP+geometry_seen,8);geocheck(v==ctxt&&D2H(ctxt)->ref==3,"mixed receiver ownership");}
 for(i=0;i<16;i++)geocheck(root[guard+i]==0x5a,"caller guard");if(mode==2){REG saved=R;geocheck(p->exval!=H,"raise publishes exception owner");geocheck(!handler(worker.env->errstr),"real unhandled provider exception");geocheck(p->exval==H&&!memcmp(&R,&saved,sizeof(R)),"exception owner retired");}else geocheck(p->exval==H,"no exception owner");observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);if(mode==1){ptrdel(D2H(provider));destroy(provider);}geocheck(native64typedmoduleclear(&co)&&native64typedmoduleclear(&po),"source owners retired");geocheck(D2H(ctxt)->ref==1,"receiver ownership retired");ptrdel(D2H(ctxt));destroy(ctxt);freetype(ct);
 if(live||modules||nlinkpins||worker.nerr||Tmodlink.ref!=1)fprintf(stderr,"geometry retire fn=%u mode=%d live=%d modules=%p pins=%d nerr=%d Tmodlink=%d\n",fn,mode,live,modules,nlinkpins,worker.nerr,Tmodlink.ref);
 geocheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1,"complete ownership retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void georefusals(void){Native64ModuleSpec spec;Native64ModuleFunction f[8];Native64TypedStep code[32];Native64ModuleSite sites[2];Native64ModuleTable table;Native64ModuleImport imports[4];Native64ModuleOwner owner={0};REG saved=R;uchar fmap[8],imap[8];uint k,pc=1,fn=full_fn_rect;int before;
 require(full_tables[0].count==4&&full_functions[fn].code[pc].op==IMOVM);
 for(k=0;k<22;k++){
  spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;
  sites[0]=sites[1]=f[fn].sites[0];f[fn].sites=sites;table=full_tables[0];memmove(imports,table.imports,sizeof(imports));table.imports=imports;spec.tables=&table;
  memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(imap,imports[sites[0].target].map,imports[sites[0].target].mapbytes);imports[sites[0].target].map=imap;
  switch(k){case 0:code[pc].m=4;break;case 1:code[pc].m=12;break;case 2:code[pc].m=24;break;case 3:code[pc].m=-8;break;
  case 4:code[pc].s++;break;case 5:code[pc].d++;break;case 6:code[pc].mode=SRC(AIMM)|AXIMM|DST(AFP|AIND);break;case 7:code[pc].d=32;break;
  case 8:code[pc].s=32;break;case 9:code[pc].d=imports[sites[0].target].bytes;break;case 10:code[pc].s=f[fn].bytes;break;
  case 11:fmap[code[pc].s/64]|=128>>((code[pc].s/8)%8);break;case 12:imap[code[pc].d/64]|=128>>((code[pc].d/8)%8);break;
  case 13:code[pc].returned=0;break;case 14:code[pc].mode=(code[pc].mode&~ARM)|AXINF;break;case 15:sites[0].external=0;break;
  case 16:f[fn].nsites=0;break;case 17:sites[0].target=table.count;break;case 18:f[fn].nsites=2;break;
  case 19:code[pc].mode=SRC(AMP)|AXIMM|DST(AFP|AIND);code[pc].s=spec.databytes-4;break;
  case 20:code[pc].m=0x100000000LL;break;case 21:code[pc].mode=SRC(AMP)|AXIMM|DST(AFP|AIND);code[pc].s=geometry_loaded;break;}
  before=allocations;geocheck(!native64typedmodule(&spec,0,&owner),"malformed geometry metadata refused");geocheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"geometry refusal before allocation");
 }
 puts("PASS geometry metadata: 22 width/span/map/mode/call-island refusals before allocation");
}
static void geoconstruction(void){Native64ModuleOwner owner={0};const Native64ModuleSpec*spec;REG saved=R;int which,jit,i,n,base;
 for(which=0;which<2;which++)for(jit=0;jit<2;jit++){
  spec=which?&full_spec:&geometry_provider_spec;base=allocations;geocheck(native64typedmodule(spec,jit,&owner),"constructor baseline");n=allocations-base;geocheck(native64typedmoduleclear(&owner),"constructor clear");
  for(i=1;i<=n;i++){failat=allocations+i;geocheck(!native64typedmodule(spec,jit,&owner),"constructor allocation failure");failat=0;geocheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor atomic rollback");}
  printf("PASS geometry constructor: provider=%d jit=%d %d allocation failures and complete rollback\n",!which,jit,n);
 }
}
int main(void){uint fn,row,total=0;int pj,cj,q,quanta[]={1,3,1000};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 GEOMETRY_HANDLERS
 optab[IMOVP]=actual_movp;optab[IMOVM]=geocopy;optab[IMOVB]=actual_movb;optab[ICVTBW]=actual_cvtbw;optab[ICVTLW]=actual_cvtlw;optab[IRAISE]=actual_iraise;
 georefusals();geoconstruction();
 for(fn=1;fn<8;fn++)for(pj=0;pj<2;pj++)for(cj=0;cj<2;cj++)for(q=0;q<3;q++)for(row=0;row<4;row++){georun(fn,pj,cj,quanta[q],row,0);total++;}
 for(fn=1;fn<8;fn++)for(pj=0;pj<2;pj++)for(cj=0;cj<2;cj++)for(q=0;q<3;q++){
  georun(fn,pj,cj,quanta[q],2,1);total++;
  if(fn!=full_fn_literal&&fn!=full_fn_global){georun(fn,pj,cj,quanta[q],2,2);total++;}
 }
 printf("PASS numeric Draw geometry: %u Limbo-to-Limbo C/JIT calls, generated .m prefixes, Point/Rect literal/scalar/FP/MP arguments, mixed receiver/byte/Big, fresh/inherited Word result, stack growth, nil module/real provider raise and complete retirement; no Draw service or AWM window\n",total);return 0;
}
