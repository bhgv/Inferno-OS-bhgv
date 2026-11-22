#include "full.h"
static void racheck(int ok,const char *why)
{if(!ok){fprintf(stderr,"managed array predicate: %s\n",why);exit(10);}}
static Modlink *fullload(Module*m,WORD t,char*p)
{USED(m);USED(t);USED(p);error(exModule);return H;}
static void *raobject(uint kind)
{
 Type *t;void *v;Module *m;
 switch(kind){
 case 0:return c2string("managed",7);
 case 1:{List*l=H;String*s=c2string("list payload",12);List*n=cons(sizeof(s),&l);n->t=&Tptr;Tptr.ref++;memmove(n->data,&s,8);return n;}
 case 2:return H2D(void*,heaparray(&Tbyte,3));
 case 3:t=dtype(freeheap,8,nil,0);require(t!=nil);v=H2D(void*,heapz(t));freetype(t);return v;
 case 4:{Channel*c=cnewc(nil,actual_movw,0);require(c!=H&&c!=nil);c->mid.w=4;return c;}
 default:m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->name=strdup("array-module-value");m->path=strdup("array-module-value");m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->name&&m->path&&m->type);m->type[0]=dtype(freeheap,0,nil,0);require(m->type[0]!=nil);return mklinkmod(m,0);
 }
}
static uint ranewcalls,rafail;
static void ranew(void)
{
 Array*a;uint i;ranewcalls++;
 if(rafail&&ranewcalls==rafail)failat=allocations+1;
 actual_newaz();failat=0;memmove(&a,R.d,8);
 racheck(a!=H&&a->t->size==8&&a->t->np==1&&a->t->map[0]==0x80,"mapped pointer-sized element Type");
 for(i=0;i<(uint)a->len;i++){void*v;memmove(&v,a->data+8*i,8);racheck(v==H,"all elements initialized to H");}
}
static void raindex(void)
{
 Array*a;WORD i;void*address;memmove(&a,R.s,8);memmove(&i,R.d,4);actual_indl();memmove(&address,R.m,8);
 racheck(address==a->data+8*i,"native pointer element stride");
}
static Module *raroot(int grow)
{
 Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,0x38};require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->entryt=m->type[0]=dtype(freeheap,128,map,2);require(m->entryt!=nil);
 p=m->prog=mallocz(7*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=7;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=88;p[0].d.ind=40;
 p[1].op=IMOVW;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=64;p[1].d.i.f=40;p[1].d.i.s=64;
 p[2]=p[1];p[2].s.ind=68;p[2].d.i.s=68;
 p[3].op=IMOVP;p[3].add=SRC(AFP)|DST(AFP|AIND);p[3].s.ind=80;p[3].d.i.f=40;p[3].d.i.s=72;
 p[4].op=ILEA;p[4].reg=24;p[4].add=SRC(AFP)|DST(AFP|AIND);p[4].s.ind=96;p[4].d.i.f=40;p[4].d.i.s=32;
 p[5].op=IMCALL;p[5].add=SRC(AFP)|AXIMM|DST(AFP);p[5].s.ind=40;p[5].d.ind=88;
 p[6].op=IEXIT;p[6].add=SRC(AXXX)|DST(AXXX);return m;
}
static void rarun(uint kind,uint form,int jit,int quantum,WORD n,WORD index,uint profile,uint fault)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import request[2]={{0}};Prog*p;uchar*root;
 void*x,*old,*input,*oldvalue,*marker,*value,*out;Array*saved,*result;uint fn=rootarray_functions[kind][form],i,steps;int done,refs;WORD failing=fault==3;
 const char*failure=n<0?exNegsize:n==2147483647?exHeap:fault==1||fault==2?exHeap:failing?"arrayfault":index<0||index>=n?exBounds:"";
 require(native64typedmodule(&full_spec,jit,&owner));x=profile==3&&kind==0?newstring(0):raobject(kind);old=raobject(kind);marker=form?H2D(void*,heaparray(&Tbyte,2)):raobject(kind);
 input=profile==1?H:x;oldvalue=profile==2?x:old;
 request[0].name=(char*)full_functions[fn].name;memmove(&request[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,request,1);require(ml!=H&&ml!=nil);
 D2H(oldvalue)->ref++;memmove(ml->MP+rootarray_globals[kind][1],&oldvalue,8);memmove(ml->MP+rootarray_fail,&failing,4);
 m=raroot(profile&1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;
 memmove(root+64,&n,4);memmove(root+68,&index,4);memmove(root+88,&ml,8);if(input!=H)D2H(input)->ref++;memmove(root+80,&input,8);D2H(marker)->ref++;memmove(root+96,&marker,8);memset(root+104,0x5a,24);
 ranewcalls=0;rafail=fault==1?1:fault==2?2:0;
 if(fault==2&&form)failure=index<0||index>=n?exBounds:"";
 p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 failat=0;rafail=0;
 if(steps>=10000||strcmp(worker.env->errstr,failure))fprintf(stderr,"root array kind=%u form=%u jit=%d q=%d n=%d i=%d profile=%u fault=%u error=%s expected=%s\n",kind,form,jit,quantum,n,index,profile,fault,worker.env->errstr,failure);
 racheck(steps<10000&&!strcmp(worker.env->errstr,failure),"exact execution/error");
 memmove(&value,root+96,8);memmove(&saved,ml->MP+rootarray_globals[kind][0],8);memmove(&out,ml->MP+rootarray_globals[kind][2],8);
 for(i=104;i<128;i++)racheck(root[i]==0x5a,"caller guard bytes");
 if(*failure)racheck(value==marker,"failed call preserves existing result owner");
 else{
  racheck(saved!=H&&saved->len==n&&saved->t->size==8&&saved->t->map[0]==0x80,"saved mapped array geometry");
  for(i=0;i<(uint)n;i++){void*element;memmove(&element,saved->data+8*i,8);racheck(element==(i==(uint)index?input:H),"exact saved payload and untouched cells");}
  if(form){result=value;racheck(result!=H&&result->len==n-index&&result->t==saved->t&&result->root==saved,"slice retains exact backing owner");memmove(&value,result->data,8);racheck(value==input,"slice begins at indexed element");}
  else {racheck(value==input,"direct borrowed element result");racheck(out==(profile==3&&kind==0?H:input),"nil and allocated-empty String comparison semantics");}
  refs=1+(input==x)+(oldvalue==x)+(input==x)+(form?0:(out==x)+(input==x));
  racheck(D2H(x)->ref==refs,"exact managed element reference ownership");
 }
 if(p->exval!=H){REG savedregs=R;racheck(!handler(worker.env->errstr),"unhandled source exception");racheck(p->exval==H&&!memcmp(&R,&savedregs,sizeof(R)),"published exception retired");}
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);require(native64typedmoduleclear(&owner));
 racheck(D2H(x)->ref==1&&D2H(old)->ref==1&&D2H(marker)->ref==1,"all fixture owners retained once");destroy(x);destroy(old);destroy(marker);
 racheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"complete managed array retirement");
}
static void rarefusals(void)
{
 Native64ModuleSpec spec;Native64ModuleFunction functions[12];Native64TypedStep code[128];Native64ModuleOwner owner={0};uchar map[64],data[64];
 uint fn,pc,k,b,mode,bit,total=0;REG saved=R;int before;
 for(fn=0;fn<12;fn++)for(pc=0;pc<full_functions[fn].count;pc++){
  const Native64TypedStep*old=full_functions[fn].code+pc;
  if(old->returned!=254&&old->returned!=255&&old->returned!=111)continue;
  for(k=0;k<8;k++){
   spec=full_spec;memmove(functions,full_functions,sizeof(functions));spec.functions=functions;
   require(functions[fn].count<=nelem(code)&&functions[fn].mapbytes<=nelem(map)&&spec.mapbytes<=nelem(data));memmove(code,functions[fn].code,functions[fn].count*sizeof(*code));functions[fn].code=code;
   memmove(map,functions[fn].map,functions[fn].mapbytes);functions[fn].map=map;memmove(data,spec.datamap,spec.mapbytes);spec.datamap=data;
   switch(k){
   case 0:if(old->op==ISLICELA)code[pc].m=32768;else code[pc].m++;break;case 1:code[pc].s++;break;case 2:if(native64typedbranch(old->op))code[pc].d=functions[fn].count;else code[pc].d++;break;case 3:code[pc].op=INOP;break;case 4:code[pc].returned=0;break;case 5:code[pc].mode^=4;break;
   case 6:code[pc].s=-8;break;
   case 7:
    if(old->op==IMOVP){for(b=0;b<functions[fn].count;b++)if(native64typedbranch(code[b].op))break;require(b<functions[fn].count);code[b].d=pc;}
    else if(old->op==INEWAZ||old->op==IINDL||old->op==ISLICELA){mode=old->op==INEWAZ?UDST(old->mode):USRC(old->mode);bit=(old->op==INEWAZ?old->d:old->s)/8;if(mode==AFP)map[bit/8]^=128>>(bit%8);else data[bit/8]^=128>>(bit%8);}
    else {bit=old->m/8;data[bit/8]^=128>>(bit%8);}break;
   }
   before=allocations;
   if(native64typedmodule(&spec,0,&owner)){fprintf(stderr,"array metadata fn=%u pc=%u k=%u role=%u op=%u\n",fn,pc,k,old->returned,old->op);racheck(0,"malformed array metadata refused");}
   if(allocations!=before||owner.module||live||memcmp(&R,&saved,sizeof(R)))fprintf(stderr,"array preallocation fn=%u pc=%u k=%u role=%u op=%u allocated=%d live=%d\n",fn,pc,k,old->returned,old->op,allocations-before,live);
   racheck(allocations==before&&!owner.module&&!live&&!memcmp(&R,&saved,sizeof(R)),"array metadata refused before allocation");total++;
  }
 }
 printf("PASS managed array metadata: %u shape/map/borrow/branch refusals before allocation\n",total);
}
static void raconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&full_spec,jit,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&full_spec,jit,&owner));failat=0;racheck(!live&&!nlinkpins&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor rollback");}
  printf("PASS managed array constructor: jit=%d %d allocation failures with rollback\n",jit,n);
 }
}
int main(void)
{
 uint kind,form,profile,k,j,f,total=0;int jit,q,quanta[]={1,3,1000};WORD counts[]={-1,0,1,4,2147483647};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;
 Tarray.free=freearray;Tarray.size=sizeof(Array);Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 ROOTARRAY_HANDLERS
 optab[IRAISE]=actual_iraise;optab[IMOVP]=actual_movp;optab[INEWAZ]=ranew;optab[IINDL]=raindex;optab[ILENA]=actual_lena;optab[ISLICEA]=actual_slicea;optab[ISLICELA]=actual_slicela;optab[IBEQC]=actual_beqc;optab[IBNEC]=actual_bnec;
 raconstruction();rarefusals();
 for(kind=0;kind<6;kind++)for(form=0;form<2;form++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(k=0;k<5;k++){
  WORD indices[]={-2147483647-1,-1,0,counts[k]>0?counts[k]-1:0,counts[k],2147483647};
  for(j=0;j<6;j++)for(profile=0;profile<(kind?3:4);profile++){rarun(kind,form,jit,quanta[q],counts[k],indices[j],profile,0);total++;}
 }
 for(kind=0;kind<6;kind++)for(form=0;form<2;form++)for(jit=0;jit<2;jit++)for(f=1;f<=3;f++){rarun(kind,form,jit,1,4,2,0,f);total++;}
 printf("PASS managed arrays: %u C/JIT scenarios across six real root kinds, allocation/index/copy/slice/result/nil/alias/empty-string semantics, signed bounds, faults, exact ownership and retirement\n",total);return 0;
}
