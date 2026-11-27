#include "full.h"
static void wcheck(int ok,const char *why)
{if(!ok){fprintf(stderr,"Word array predicate: %s\n",why);exit(10);}}
static Modlink *fullload(Module*m,WORD t,char*p)
{USED(m);USED(t);USED(p);error(exModule);return H;}
static WORD dense[]={-2147483647-1,17,0,2147483647,-31};
static WORD sparsevalues[]={0,0,11,0,0,-7,0};
static uint rawcalls;
static void windex(void){rawcalls++;actual_indx();}
static Modlink*wlink(Module*m,uint fn)
{Import r[2]={{0}};Modlink*l;r[0].name=(char*)full_functions[fn].name;memmove(&r[0].sig,&full_functions[fn].signature,4);m->ref++;l=linkmod(m,r,1);require(l!=H&&l!=nil);return l;}
static Array*warray(uchar*mp,uint slot)
{Array*a;memmove(&a,mp+word_globals[slot],8);return a;}
static void winitial(Module*m)
{
 uint i,j;Array*a;WORD v;uint lengths[]={5,7,0};
 for(i=0;i<3;i++){a=warray(m->origmp,i);wcheck(a!=H&&a->len==(int)lengths[i]&&a->root==H&&a->t->size==4&&a->t->np==0,"exact initialized Word32 array geometry");for(j=0;j<lengths[i];j++){memmove(&v,a->data+4*j,4);wcheck(v==(i?sparsevalues[j]:dense[j]),"exact dense and sparse initializer bits");}}
}
static Module*wroot(int grow)
{
 Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,0x10};require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->entryt=m->type[0]=dtype(freeheap,128,map,2);require(m->entryt!=nil);p=m->prog=mallocz(6*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=6;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=88;p[0].d.ind=40;
 p[1].op=IMOVW;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=64;p[1].d.i.f=40;p[1].d.i.s=64;
 p[2]=p[1];p[2].s.ind=68;p[2].d.i.s=68;
 p[3].op=ILEA;p[3].reg=4;p[3].add=SRC(AFP)|DST(AFP|AIND);p[3].s.ind=80;p[3].d.i.f=40;p[3].d.i.s=32;
 p[4].op=IMCALL;p[4].add=SRC(AFP)|AXIMM|DST(AFP);p[4].s.ind=40;p[4].d.ind=88;
 p[5].op=IEXIT;p[5].add=SRC(AXXX)|DST(AXXX);return m;
}
static void wrun(uint method,int jit,int quantum,WORD index,WORD n,uint profile)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml,*other;Prog*p;uchar*root;Array*a,*original,*second,*instance;Type*poisoned=nil;int oldnp=0,oldmap=0;WORD got,marker=0x12345678,wanted=0,v;uint i,steps;int done;const char*error="";
 uint fn=word_functions[method];require(native64typedmodule(&full_spec,jit,&owner));winitial(owner.module);ml=wlink(owner.module,fn);other=wlink(owner.module,fn);ptradd(D2H(other));original=warray(owner.module->origmp,0);second=warray(other->MP,0);instance=warray(ml->MP,0);
 wcheck(ml->MP!=other->MP&&instance!=original&&second!=original&&second!=instance&&D2H(original)->ref==1,"independent MP instances clone raw array values");
 if(method==0||method==3||method==7||method==8){if(index<0||index>=5)error=exBounds;else wanted=method==3?n:method==7?dense[index]<n:dense[index];}
 else if(method==1){if(index<0||index>=7)error=exBounds;else wanted=sparsevalues[index];}
 else if(method==2)error=exBounds;
 else if(method==4||method==6||method==11||method==12){if(n<0)error=exNegsize;else if(n==2147483647)error=exHeap;else if(method==6&&n<5)error=exBounds;else if(index<0||index>=n)error=exBounds;else wanted=method==6?(index<5?dense[index]:0):n;}
 else if(method==5){if(index<0||n<index||n>5)error=exBounds;else wanted=n-index;}
 else wanted=n;
 if(profile){
  wcheck(method==0,"poisoned read fixture");
  if(profile==1){a=H;memmove(ml->MP+word_globals[0],&a,8);destroy(instance);error=exBounds;}
  if(profile==2){a=H2D(Array*,heaparray(&Tbyte,5));memmove(ml->MP+word_globals[0],&a,8);destroy(instance);error=exBounds;}
  if(profile==3){poisoned=original->t;oldnp=poisoned->np;oldmap=poisoned->map[0];poisoned->np=1;poisoned->map[0]=0x80;error=exBounds;}
 }
 m=wroot(quantum==1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;
 memmove(root+64,&index,4);memmove(root+68,&n,4);memmove(root+80,&marker,4);memmove(root+88,&ml,8);memset(root+72,0x5a,8);memset(root+84,0x5a,4);memset(root+96,0x5a,32);p->quanta=quantum;rawcalls=0;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 if(steps>=10000||strcmp(worker.env->errstr,error))fprintf(stderr,"Word method=%u jit=%d q=%d i=%d n=%d profile=%u error=%s expected=%s\n",method,jit,quantum,index,n,profile,worker.env->errstr,error);
 wcheck(steps<10000&&!strcmp(worker.env->errstr,error),"exact call completion/error");memmove(&got,root+80,4);
 if(got!=(*error?marker:wanted))fprintf(stderr,"Word result method=%u i=%d n=%d got=%d wanted=%d\n",method,index,n,got,*error?marker:wanted);
 wcheck(got==(*error?marker:wanted),"exact Word32 result and failed-call preservation");
 if(profile>=2)wcheck(rawcalls==0,"raw Type rejected before opcode dispatch");
 for(i=72;i<80;i++)wcheck(root[i]==0x5a,"argument guard bytes");for(i=84;i<88;i++)wcheck(root[i]==0x5a,"four-byte result guard");for(i=96;i<128;i++)wcheck(root[i]==0x5a,"caller guard bytes");
 if(!profile){
  for(i=0;i<5;i++){memmove(&v,second->data+4*i,4);wcheck(v==dense[i],"MP clones isolate raw payload writes");}
  if(!*error&&method==4){a=warray(ml->MP,0);wcheck(a!=original&&warray(other->MP,0)==second&&a->len==n,"MP rebinding preserves other instance owner");}
  if(!*error&&method==5){a=warray(ml->MP,3);wcheck(a!=H&&a->len==n-index&&a->root==instance&&a->data==instance->data+4*index,"MP slice retains exact raw backing owner");}
  if(!*error&&method==6){a=warray(ml->MP,3);wcheck(a!=H&&a->len==n&&a->t->size==4&&a->t->np==0,"copied raw array geometry");for(i=0;i<(uint)n;i++){memmove(&v,a->data+4*i,4);wcheck(v==(i<5?dense[i]:0),"copied payload and zero remainder");}}
  if(!*error&&(method==11||method==12)){a=warray(ml->MP,method==11?4:5);wcheck(a!=H&&a->len==n&&a->t->size==(method==11?16:12)&&a->t->np==0,"global Rect/Delta raw array geometry");memmove(&v,a->data+(method==11?16:12)*index+8,4);wcheck(v==n,"global record field offset");}
  if(!*error&&method==10)wcheck(warray(ml->MP,0)==H&&warray(other->MP,0)==second,"clearing MP owner preserves clone");
 }
 if(poisoned){poisoned->np=oldnp;poisoned->map[0]=oldmap;}
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);ptrdel(D2H(other));destroy(other);require(native64typedmoduleclear(&owner));
 wcheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"complete Word array/MP retirement");
}
static void wflow(void)
{
 uchar fm[]={0,128},mp[]={128,128};Native64ModuleSpec s={0};Native64ModuleFunction f={0};Native64TypedStep c[4];
 s.databytes=128;s.datamap=mp;s.mapbytes=2;f.bytes=128;f.map=fm;f.mapbytes=2;f.count=4;f.code=c;
 c[0]=(Native64TypedStep){IINDX,SRC(AMP)|AXINF|DST(AFP),64,(1<<8)|48,80,80};
 c[1]=(Native64TypedStep){IMOVP,SRC(AMP)|DST(AFP),0,0,64,0};
 c[2]=(Native64TypedStep){IMOVW,SRC(AFP|AIND)|DST(AFP),48,0,96,65};
 c[3]=(Native64TypedStep){IRET,SRC(AXXX)|DST(AXXX),0,0,0,0};
 wcheck(native64modulerawarrays(&s,&f),"FP/MP owner identity kept distinct");
 c[1].mode=SRC(AMP)|DST(AMP);wcheck(!native64modulerawarrays(&s,&f),"MP owner overwrite refused");
 c[1]=(Native64TypedStep){ICALL,SRC(AFP)|DST(AIMM),40,0,0,0};wcheck(!native64modulerawarrays(&s,&f),"global borrow invalidated by call");
 c[0].mode=SRC(AFP)|AXINF|DST(AFP);wcheck(native64modulerawarrays(&s,&f),"local owner retained across call");
 puts("PASS raw owner flow: FP/MP identity, global overwrite/call invalidation and local-call preservation");
}
static void wmetadata(void)
{
 Native64ModuleSpec s;Native64ModuleWordArray arrays[3];Native64ModuleString text={0};Native64ModuleInit init;
 Native64ModuleOwner owner={0};uchar map[64];uint k;REG saved=R;int before;
 require(full_spec.nwordarrays==3&&full_spec.mapbytes<=nelem(map));
 for(k=0;k<14;k++){
  s=full_spec;memmove(arrays,full_spec.wordarrays,sizeof(arrays));s.wordarrays=arrays;memmove(map,s.datamap,s.mapbytes);s.datamap=map;
  switch(k){
  case 0:s.nwordarrays=257;break;case 1:s.wordarrays=nil;break;case 2:s.wordarrays=H;break;
  case 3:arrays[0].offset++;break;case 4:arrays[0].offset=s.databytes;break;case 5:arrays[1].offset=arrays[0].offset;break;
  case 6:arrays[0].count=65537;break;case 7:arrays[2].values=nil;break;case 8:arrays[2].values=H;break;
  case 9:map[arrays[0].offset/64]&=~(128>>((arrays[0].offset/8)%8));break;
  case 10:arrays[0].count=65536;arrays[1].count=1;break;
  case 11:text.offset=arrays[0].offset;text.bytes=0;text.text=(uchar*)"";s.strings=&text;s.nstrings=1;break;
  case 12:init=(Native64ModuleInit){arrays[0].offset,4,0};s.initial=&init;s.ninitial=1;break;
  case 13:arrays[2].offset=arrays[0].offset;break;
  }
  before=allocations;if(native64typedmodule(&s,0,&owner)){fprintf(stderr,"Word initializer metadata case=%u accepted\n",k);wcheck(0,"malformed initializer metadata refused");}
  wcheck(allocations==before&&!owner.module&&!live&&!memcmp(&R,&saved,sizeof(R)),"initializer refusal before allocation");
 }
 puts("PASS Word initializer metadata: 14 count/pointer/map/collision refusals before allocation");
}
static void wconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&full_spec,jit,&owner));winitial(owner.module);n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&full_spec,jit,&owner));failat=0;wcheck(!owner.module&&!live&&!nlinkpins&&!memcmp(&R,&saved,sizeof(R)),"initializer constructor rollback");}
  printf("PASS Word constructor: jit=%d %d allocation failures and complete rollback\n",jit,n);
 }
}
typedef struct WordClone WordClone;
struct WordClone{Module*m;Import imports[2];Modlink*result;};
static int wclone(void*arg)
{WordClone*c=arg;c->m->ref++;c->result=linkmod(c->m,c->imports,1);if(c->result==H||c->result==nil)error(exModule);return 1;}
static void wclonefailures(void)
{
 Native64ModuleOwner owner={0};WordClone c={0};REG saved=R;int before,n,i,base,pins;
 require(native64typedmodule(&full_spec,0,&owner));c.m=owner.module;c.imports[0].name=(char*)full_functions[word_functions[0]].name;memmove(&c.imports[0].sig,&full_functions[word_functions[0]].signature,4);
 before=allocations;require(kprotect(wclone,&c));n=allocations-before;destroy(c.result);base=live;pins=nlinkpins;
 for(i=1;i<=n;i++){
  c.result=H;failat=allocations+i;wcheck(!kprotect(wclone,&c),"clone failure propagated");failat=0;
  wcheck(c.result==H&&c.m->ref==1&&live==base&&nlinkpins==pins&&!memcmp(&R,&saved,sizeof(R)),"partial MP/array clone rollback");
 }
 require(native64typedmoduleclear(&owner));wcheck(!live&&!nlinkpins,"clone fixture retired");
 printf("PASS Word MP cloning: %d allocation failures with exact rollback\n",n);
}
int main(void)
{
 uint method,k,j,p,total=0;int jit,q,quanta[]={1,3,1000};WORD indices[]={-2147483647-1,-1,0,1,4,5,6,7,2147483647},counts[]={-1,0,1,3,5,7,2147483647};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tarray.free=freearray;Tarray.size=sizeof(Array);Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 WORDARRAY_HANDLERS
 optab[IMOVP]=actual_movp;optab[INEWAZ]=actual_newaz;optab[IINDX]=windex;optab[ILENA]=actual_lena;optab[ISLICEA]=actual_slicea;optab[ISLICELA]=actual_slicela;
 wflow();wconstruction();wmetadata();wclonefailures();
 for(method=0;method<nelem(word_functions);method++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(k=0;k<nelem(indices);k++)for(j=0;j<nelem(counts);j++){wrun(method,jit,quanta[q],indices[k],counts[j],0);total++;}
 for(jit=0;jit<2;jit++)for(p=1;p<=3;p++){wrun(0,jit,1,0,5,p);total++;}
 printf("PASS global Word arrays: %u C/JIT cases, dense/sparse/empty initializers, signed values/bounds, FP/MP allocation/index/slices/copies/results, independent payload and MP clones, malformed Types and complete retirement\n",total);return 0;
}
