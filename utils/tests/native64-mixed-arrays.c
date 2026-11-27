#include "mixed-native.h"
#include "full.h"
static void mcheck(int ok,const char*why)
{if(!ok){fprintf(stderr,"mixed array predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p)
{USED(m);USED(t);USED(p);error(exModule);return H;}
static uint shape,newcalls,newfault;
static void mixednew(void)
{
 Array*a;void*v;WORD word;uint i,j;const struct MixedLayout*l=mixed_layouts+shape;
 newcalls++;if(newfault==newcalls)failat=allocations+1;actual_newaz();failat=0;memmove(&a,R.d,8);
 mcheck(a!=H&&a->t->size==(int)l->width&&a->t->np==1&&a->t->map[0]==(uchar)(255U<<(8-l->roots)),"exact mixed element Type");
 for(i=0;i<(uint)a->len;i++){
  for(j=0;j<l->roots;j++){memmove(&v,a->data+l->width*i+8*j,8);mcheck(v==H,"managed element cells initialized to H");}
  memmove(&word,a->data+l->width*i+l->first,4);mcheck(word==0,"first Word initialized to zero");
  memmove(&word,a->data+l->width*i+l->last,4);mcheck(word==0,"last Word initialized to zero");
 }
}
static void mixedindex(void)
{
 Array*a;WORD i;void*p;memmove(&a,R.s,8);memmove(&i,R.d,4);actual_indx();memmove(&p,R.m,8);
 mcheck(p==a->data+mixed_layouts[shape].width*i,"native mixed element stride");
}
static void mixedmove(void)
{
 WORD index;Type*t;memmove(&index,R.m,4);t=moduletype(R.M,index);
 mcheck(t->size==(int)mixed_layouts[shape].copy&&t->np==1&&t->map[0]==(uchar)(255U<<(8-mixed_layouts[shape].roots)),"nested mapped copy descriptor");actual_movmp();
}
static Module*mroot(int get,int grow)
{
 Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,0x3c|(get?2:0),0};uint j;
 require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->entryt=m->type[0]=dtype(freeheap,160,map,3);require(m->entryt!=nil);
 p=m->prog=mallocz(9*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=9;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=104;p[0].d.ind=40;
 p[1].op=IMOVW;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=64;p[1].d.i.f=40;p[1].d.i.s=64;
 p[2]=p[1];p[2].s.ind=68;p[2].d.i.s=68;
 for(j=0;j<3;j++){p[3+j].op=IMOVP;p[3+j].add=SRC(AFP)|DST(AFP|AIND);p[3+j].s.ind=80+8*j;p[3+j].d.i.f=40;p[3+j].d.i.s=72+8*j;}
 p[6].op=ILEA;p[6].reg=get?24:4;p[6].add=SRC(AFP)|DST(AFP|AIND);p[6].s.ind=112;p[6].d.i.f=40;p[6].d.i.s=32;
 p[7].op=IMCALL;p[7].add=SRC(AFP)|AXIMM|DST(AFP);p[7].s.ind=40;p[7].d.ind=104;
 p[8].op=IEXIT;p[8].add=SRC(AXXX)|DST(AXXX);return m;
}
static void*mobject(uint k)
{
 if(k==1)return c2string("mixed \342\230\203",9);
 if(k==2){Channel*c=cnewc(nil,actual_movw,0);require(c!=H&&c!=nil);c->mid.w=4;return c;}
 {Type*t=dtype(freeheap,sizeof(Mixedarrays_Box),nil,0);void*v;require(t!=nil);v=H2D(void*,heapz(t));freetype(t);return v;}
}
static void mvalues(Array*a,WORD n,WORD index,void**inputs)
{
 const struct MixedLayout*l=mixed_layouts+shape;uint i,j;void*v;WORD word;
 mcheck(a!=H&&a->len==n+1&&a->t->size==(int)l->width&&a->t->map[0]==(uchar)(255U<<(8-l->roots)),"grown array geometry and map");
 for(i=0;i<(uint)a->len;i++){
  for(j=0;j<l->roots;j++){memmove(&v,a->data+l->width*i+8*j,8);mcheck(v==(i==(uint)index?inputs[j]:H),"exact mixed root cells and untouched neighbors");}
  memmove(&word,a->data+l->width*i+l->first,4);mcheck(word==(i==(uint)index?17:0),"nested value Word preserved");
  memmove(&word,a->data+l->width*i+l->last,4);mcheck(word==(i==(uint)index?(l->words==2?29:17):0),"copy padding preserves active state");
 }
}
typedef struct MixedClone MixedClone;
struct MixedClone{Array*source,*result;};
static int mclone(void*arg)
{MixedClone*c=arg;c->result=arraycpy(c->source);return 1;}
static void mclones(Modlink*ml,Array*a,WORD n,WORD index,void**inputs)
{
 Type*t=D2H(ml->MP)->t;uchar*mp=H2D(uchar*,heapz(t));Array*b;uint j;int base,before,count,i;MixedClone c={0};WORD value=73;const struct MixedLayout*l=mixed_layouts+shape;
 ptradd(D2H(mp));newmp(mp,ml->MP,t);memmove(&b,mp+mixed_globals[shape][0],8);
 mcheck(b!=a&&b->data!=a->data&&b->t==a->t,"independent mixed array MP clone");mvalues(b,n,index,inputs);
 memmove(b->data+l->width*index+l->last,&value,4);mvalues(a,n,index,inputs);
 ptrdel(D2H(mp));destroy(mp);
 c.source=a;before=allocations;require(kprotect(mclone,&c));count=allocations-before;mvalues(c.result,n,index,inputs);destroy(c.result);base=live;
 for(i=1;i<=count;i++){
  int refs[3];for(j=0;j<l->roots;j++)refs[j]=inputs[j]==H?0:D2H(inputs[j])->ref;
  c.result=H;failat=allocations+i;mcheck(!kprotect(mclone,&c),"mixed clone allocation fault propagated");failat=0;
  mcheck(c.result==H&&live==base,"partial mixed clone retired");
  for(j=0;j<l->roots;j++)mcheck(inputs[j]==H||D2H(inputs[j])->ref==refs[j],"clone failure restores root ownership");
 }
}
static void mrun(uint kind,int get,int jit,int quantum,WORD n,WORD index,uint profile,uint fault)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import request[2]={{0}};Prog*p;uchar*root;void*objects[3],*inputs[3],*marker,*v;Array*a;uint j,steps,fn=mixed_functions[kind][get];int done,refs;WORD word,mark=0x12345678,failing=fault==4;
 const struct MixedLayout*l=mixed_layouts+kind;const char*failure=n<0?exNegsize:n==2147483647?exHeap:fault==1?exHeap:failing?"mixedfault":index<0||index>=n?exBounds:fault==2||fault==3?exHeap:"";
 shape=kind;require(native64typedmodule(&full_spec,jit,&owner));for(j=0;j<3;j++){objects[j]=mobject(j);inputs[j]=profile?H:objects[j];}marker=mobject(0);
 request[0].name=(char*)full_functions[fn].name;memmove(&request[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,request,1);require(ml!=H&&ml!=nil);memmove(ml->MP+mixed_fail,&failing,4);
 m=mroot(get,profile&1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;
 memmove(root+64,&n,4);memmove(root+68,&index,4);memset(root+72,0x5a,8);memmove(root+104,&ml,8);
 for(j=0;j<3;j++){if(inputs[j]!=H)D2H(inputs[j])->ref++;memmove(root+80+8*j,inputs+j,8);}
 if(get){D2H(marker)->ref++;memmove(root+112,&marker,8);}else memmove(root+112,&mark,4);
 memset(root+(get?120:116),0x5a,get?40:44);newcalls=0;newfault=fault<4?fault:0;p->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}failat=0;newfault=0;
 if(steps>=10000||strcmp(worker.env->errstr,failure))fprintf(stderr,"mixed shape=%u get=%d jit=%d q=%d n=%d i=%d profile=%u fault=%u error=%s expected=%s\n",kind,get,jit,quantum,n,index,profile,fault,worker.env->errstr,failure);
 mcheck(steps<10000&&!strcmp(worker.env->errstr,failure),"exact mixed array completion/error");
 for(j=72;j<80;j++)mcheck(root[j]==0x5a,"argument alignment guard");for(j=get?120:116;j<160;j++)mcheck(root[j]==0x5a,"result and caller guard");
 if(get){memmove(&v,root+112,8);mcheck(v==(*failure?marker:inputs[0]),"direct borrowed root result and failure preservation");}
 else{memmove(&word,root+112,4);mcheck(word==(*failure?mark:l->words==2?29:17),"exact Word result and failure preservation");}
 if(!*failure){
  memmove(&a,ml->MP+mixed_globals[kind][0],8);mvalues(a,n,index,inputs);
  for(j=0;j<3;j++){
   v=H;if(j<l->roots)memmove(&v,ml->MP+mixed_globals[kind][1+j],8);mcheck(v==(j<l->roots?inputs[j]:H),"exact published mixed roots");
   refs=1+(inputs[j]!=H)*(1+(j<l->roots?2:0)+(get&&j==0));mcheck(D2H(objects[j])->ref==refs,"exact mixed root reference ownership");
  }
  mclones(ml,a,n,index,inputs);
  for(j=0;j<3;j++){refs=1+(inputs[j]!=H)*(1+(j<l->roots?2:0)+(get&&j==0));mcheck(D2H(objects[j])->ref==refs,"mixed clone retirement restores exact ownership");}
 }
 if(p->exval!=H){REG saved=R;mcheck(!handler(worker.env->errstr),"unhandled source exception");mcheck(p->exval==H&&!memcmp(&R,&saved,sizeof(R)),"published exception retired");}
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);require(native64typedmoduleclear(&owner));
 for(j=0;j<3;j++){mcheck(D2H(objects[j])->ref==1,"fixture roots retained once after teardown");destroy(objects[j]);}mcheck(D2H(marker)->ref==1,"result marker retained once");destroy(marker);
 mcheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"complete mixed array retirement");
}
static void mflow(void)
{
 uchar fm[]={0,0x80};Native64ModuleSpec s={0};Native64ModuleFunction f={0};Native64TypedStep c[3];
 s.databytes=0;f.bytes=128;f.map=fm;f.mapbytes=2;f.count=3;f.code=c;
 c[0]=(Native64TypedStep){IINDX,SRC(AFP)|AXINF|DST(AIMM),64,(61<<8)|48,0,80};
 c[1]=(Native64TypedStep){IMOVMP,SRC(AFP|AIND)|AXIMM|DST(AFP|AIND),48,28,48,65};
 c[2]=(Native64TypedStep){IRET,SRC(AXXX)|DST(AXXX),0,0,0,0};
 mcheck(native64modulerawarrays(&s,&f),"nested copy span accepted");
 c[0].m=(29<<8)|48;mcheck(!native64modulerawarrays(&s,&f),"copy padding preserves active state");
 c[0].m=(61<<8)|48;c[1]=(Native64TypedStep){IMOVW,SRC(AIMM)|DST(AFP|AIND),1,28<<16,48,65};mcheck(!native64modulerawarrays(&s,&f),"padding is not a Word field");
 c[1].m=32<<16;mcheck(native64modulerawarrays(&s,&f),"padded state Word accepted");
 c[1].m=0;mcheck(!native64modulerawarrays(&s,&f),"root cell is not a Word field");
 puts("PASS mixed spans: nested copy preserves parent state; padding/root-as-Word refused");
}

static void
mpairclear(int jit, int grow, int quantum, int nils)
{
 Native64ModuleOwner owner={0};
 Module *rootmod;
 Modlink *link;
 Import request[2]={{0}};
 Prog *process;
 uchar *frame;
 Array *array;
 void *inputs[2],*value;
 WORD count=3,index=1,result;
 unsigned int i,j,steps;

 require(native64typedmodule(&full_spec,jit,&owner));
 request[0].name=(char*)full_functions[mixed_pair_function].name;
 memmove(&request[0].sig,&full_functions[mixed_pair_function].signature,4);
 owner.module->ref++;
 link=linkmod(owner.module,request,1);
 require(link!=nil&&link!=H);
 rootmod=mroot(0,grow);
 if(jit)require(compile(rootmod,rootmod->nprog,nil));
 rootmod->ref++;
 starting=rootmod;
 require(kprotect(start,nil));
 process=started;
 rootenvironment(process);
 frame=process->R.FP;
 memmove(frame+64,&count,4);
 memmove(frame+68,&index,4);
 for(i=0;i<2;i++){
  inputs[i]=nils?H:mobject(i);
  if(inputs[i]!=H)D2H(inputs[i])->ref++;
  memmove(frame+80+8*i,&inputs[i],8);
 }
 memmove(frame+104,&link,8);
 process->quanta=quantum;
 for(steps=0;steps<10000&&kprotect(executeprocess,process);steps++);
 mcheck(steps<10000&&!worker.env->errstr[0],"complete pair clear");
 memmove(&result,frame+112,4);
 memmove(&array,link->MP+mixed_pairs_offset,8);
 mcheck(result==count&&array!=H&&array->len==count&&array->t->size==16&&array->t->map[0]==0xc0,
        "mapped array pair layout preserved");
 for(i=0;i<(unsigned int)count;i++)for(j=0;j<2;j++){
  memmove(&value,array->data+16*i+8*j,8);
  mcheck(value==(i==0?inputs[j]:H),"nil pair cleared only its own element");
 }
 for(i=0;i<2;i++)if(inputs[i]!=H)
  mcheck(D2H(inputs[i])->ref==3,"cleared element relinquished both references");
 observed=process;
 progexit();
 emptyqueues();
 unload(rootmod);
 require(native64typedmoduleclear(&owner));
 for(i=0;i<2;i++)if(inputs[i]!=H){
  mcheck(D2H(inputs[i])->ref==1,"pair frame and array released their references");
  destroy(inputs[i]);
 }
 mcheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"complete pair clear retirement");
}
static void mconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&full_spec,jit,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&full_spec,jit,&owner));failat=0;mcheck(!live&&!nlinkpins&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"mixed constructor rollback");}
  printf("PASS mixed constructor: jit=%d %d allocation faults with complete rollback\n",jit,n);
 }
}
static void mrefusals(void)
{
 Native64ModuleSpec s;Native64ModuleFunction functions[18];Native64TypedStep code[128];Native64ModuleOwner owner={0};REG saved=R;
 uint fn,pc,k,total=0;int before;
 for(fn=0;fn<nelem(functions);fn++)for(pc=0;pc<full_functions[fn].count;pc++){
  const Native64TypedStep*old=full_functions[fn].code+pc;if(old->returned!=65&&old->returned!=79&&old->returned!=80)continue;
  for(k=0;k<5;k++){
   s=full_spec;memmove(functions,full_functions,sizeof(functions));s.functions=functions;require(functions[fn].count<=nelem(code));
   memmove(code,functions[fn].code,functions[fn].count*sizeof(*code));functions[fn].code=code;
   switch(k){case 0:code[pc].op=INOP;break;case 1:code[pc].mode|=256;break;case 2:code[pc].m=LONG_MAX;break;case 3:code[pc].s=USRC(old->mode)==AIMM?LONG_MAX:-1;break;case 4:code[pc].d=-1;break;}
   before=allocations;
   if(native64typedmodule(&s,0,&owner)){fprintf(stderr,"mixed metadata fn=%u pc=%u k=%u\n",fn,pc,k);mcheck(0,"malformed mixed array metadata refused");}
   if(allocations!=before||owner.module||live||memcmp(&R,&saved,sizeof(R)))fprintf(stderr,"mixed preallocation fn=%u pc=%u k=%u op=%u role=%u allocations=%d live=%d\n",fn,pc,k,old->op,old->returned,allocations-before,live);
   mcheck(allocations==before&&!owner.module&&!live&&!memcmp(&R,&saved,sizeof(R)),"mixed metadata refusal before allocation");total++;
  }
 }
 printf("PASS mixed metadata: %u opcode/mode/class/operand refusals before allocation\n",total);
}
int main(void)
{
 uint kind,profile,k,j,f,total=0;int get,jit,q,quanta[]={1,3,1000};WORD counts[]={-1,0,1,4,2147483647};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tarray.free=freearray;Tarray.size=sizeof(Array);Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 MIXEDARRAY_HANDLERS
 optab[IRAISE]=actual_iraise;optab[IMOVP]=actual_movp;optab[IMOVMP]=mixedmove;optab[INEWAZ]=mixednew;optab[IINDX]=mixedindex;optab[ILENA]=actual_lena;optab[ISLICEA]=actual_slicea;optab[ISLICELA]=actual_slicela;
 mcheck(limbo_native64_Mixedarrays_layout(),"generated natural C geometry");mflow();mrefusals();mconstruction();
 for(kind=0;kind<9;kind++)for(get=0;get<2;get++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(k=0;k<nelem(counts);k++){
  WORD indices[]={-2147483647-1,-1,0,counts[k]>0?counts[k]-1:0,counts[k],2147483647};
  for(j=0;j<nelem(indices);j++)for(profile=0;profile<2;profile++){mrun(kind,get,jit,quanta[q],counts[k],indices[j],profile,0);total++;}
 }
 for(kind=0;kind<9;kind++)for(get=0;get<2;get++)for(jit=0;jit<2;jit++)for(f=1;f<=4;f++){mrun(kind,get,jit,1,4,2,0,f);total++;}
 printf("PASS mixed arrays: %u C/JIT cases across nine layouts, real mapped allocation/copy/growth/field/results, GC maps, nil, bounds, faults, MP/array clones, exact ownership and complete retirement\n",total);
 optab[INEWAZ]=actual_newaz;
 optab[IINDX]=actual_indx;
 optab[IMOVMP]=actual_movmp;
 for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(k=0;k<2;k++)for(j=0;j<2;j++)
  mpairclear(jit,k,quanta[q],j);
 puts("PASS mixed array nil pair: 24 C/JIT cases with neighbor and root ownership checks");
 return 0;
}
