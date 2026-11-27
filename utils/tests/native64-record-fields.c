#include "record-native.h"
#include "full.h"
#include "lexer-native.h"
#include "lexer-full.h"
static void hcheck(int ok,const char*why)
{if(!ok){fprintf(stderr,"record predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p)
{USED(m);USED(t);USED(p);error(exModule);return H;}
static Records_Record*current;static uint fieldcalls;
static void fieldword(void)
{uintptr a=(uintptr)current,b=a+sizeof(*current);if((uintptr)R.s>=a&&(uintptr)R.s<b||(uintptr)R.d>=a&&(uintptr)R.d<b)fieldcalls++;actual_movw();}
static void*object(uint k)
{
 Type*t;void*v;Module*m;
 switch(k){
 case 1:return c2string("\342\230\203x",4);
 case 2:return H2D(void*,heaparray(&Tbyte,3));
 case 3:{List*l=H;String*s=c2string("list",4);List*n=cons(sizeof(s),&l);n->t=&Tptr;Tptr.ref++;memmove(n->data,&s,8);return n;}
 case 4:{Channel*c=cnewc(nil,actual_movw,0);require(c!=H&&c!=nil);c->mid.w=4;return c;}
 case 5:m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->name=strdup("field-module-value");m->path=strdup("field-module-value");m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->name&&m->path&&m->type);m->type[0]=dtype(freeheap,0,nil,0);require(m->type[0]!=nil);return mklinkmod(m,0);
 default:t=dtype(freeheap,sizeof(Records_Box),nil,0);require(t!=nil);v=H2D(void*,heapz(t));freetype(t);return v;
 }
}
static uint fields[]={offsetof(Records_Record,inner.link),offsetof(Records_Record,inner.text),offsetof(Records_Record,ary),offsetof(Records_Record,seq),offsetof(Records_Record,pipe),offsetof(Records_Record,mod)};
static Module*caller(int get,int grow)
{
 Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,0xfe,0x80|(get?0x40:0)};uint j;
 require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->entryt=m->type[0]=dtype(freeheap,176,map,3);require(m->entryt!=nil);
 p=m->prog=mallocz(12*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=12;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=128;p[0].d.ind=40;
 for(j=0;j<7;j++){p[1+j].op=IMOVP;p[1+j].add=SRC(AFP)|DST(AFP|AIND);p[1+j].s.ind=64+8*j;p[1+j].d.i.f=40;p[1+j].d.i.s=64+8*j;}
 p[8].op=IMOVW;p[8].add=SRC(AFP)|DST(AFP|AIND);p[8].s.ind=120;p[8].d.i.f=40;p[8].d.i.s=120;
 p[9].op=ILEA;p[9].reg=get?24:4;p[9].add=SRC(AFP)|DST(AFP|AIND);p[9].s.ind=136;p[9].d.i.f=40;p[9].d.i.s=32;
 p[10].op=IMCALL;p[10].add=SRC(AFP)|AXIMM|DST(AFP);p[10].s.ind=40;p[10].d.ind=128;
 p[11].op=IEXIT;p[11].add=SRC(AXXX)|DST(AXXX);return m;
}
static void runfield(uint method,int jit,int quantum,WORD n,uint profile)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import request[2]={{0}};Type*t;uchar map[]=Records_Record_map,*root;Records_Record*r,*input;
 void*values[6],*old[6],*args[6],*v,*marker;Prog*p;uint j,steps,fn=record_functions[method];int done,refs,size,mask,np,get=method>=1&&method<=6,clear=method==7;WORD word,mark=0x12345678;
 const char*failure=profile==4?exNilref:profile>=5?exBounds:"";
 require(native64typedmodule(&full_spec,jit,&owner));t=dtype(freeheap,sizeof(*r),map,sizeof(map));require(t!=nil);current=r=H2D(Records_Record*,heapz(t));
 input=profile==4?H:r;
 for(j=0;j<6;j++){values[j]=profile==2&&j==1?newstring(0):object(j);old[j]=object(j);args[j]=profile==1?H:values[j];v=profile==3?args[j]:old[j];if(v!=H)D2H(v)->ref++;memmove((uchar*)r+fields[j],&v,8);}
 r->pad=0x31415926;r->inner.number=-9;r->left=17;r->right=-7;r->ending=-31;
 memset((uchar*)r+4,0x5a,4);memset((uchar*)r+offsetof(Records_Record,inner.number)+4,0x5a,4);memset((uchar*)r+offsetof(Records_Record,ending)+4,0x5a,4);
 marker=object(get?method-1:0);request[0].name=(char*)full_functions[fn].name;memmove(&request[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,request,1);require(ml!=H&&ml!=nil);
 m=caller(get,profile&1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;
 if(input!=H)D2H(input)->ref++;memmove(root+64,&input,8);
 for(j=0;j<6;j++){if(args[j]!=H)D2H(args[j])->ref++;memmove(root+72+8*j,args+j,8);}
 memmove(root+120,&n,4);memset(root+124,0x5a,4);memmove(root+128,&ml,8);
 if(get){D2H(marker)->ref++;memmove(root+136,&marker,8);}else memmove(root+136,&mark,4);memset(root+(get?144:140),0x5a,get?32:36);
 size=t->size;mask=t->map[0];np=t->np;if(profile==5){t->size=offsetof(Records_Record,inner.number);t->np=1;}if(profile==6)t->map[0]|=0x40;
 fieldcalls=0;p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 t->size=size;t->map[0]=mask;t->np=np;
 if(profile>=5)hcheck(fieldcalls==0,profile==5?"heap extent rejected before opcode dispatch":"heap GC role rejected before opcode dispatch");
 if(steps>=10000||strcmp(worker.env->errstr,failure))fprintf(stderr,"field method=%u jit=%d q=%d n=%d profile=%u error=%s expected=%s\n",method,jit,quantum,n,profile,worker.env->errstr,failure);
 hcheck(steps<10000&&!strcmp(worker.env->errstr,failure),"exact record completion/error");
 for(j=0;j<4;j++)hcheck(((uchar*)r)[4+j]==0x5a&&((uchar*)r)[offsetof(Records_Record,inner.number)+4+j]==0x5a&&((uchar*)r)[offsetof(Records_Record,ending)+4+j]==0x5a,"four-byte field padding guards");
 for(j=124;j<128;j++)hcheck(root[j]==0x5a,"argument guard");for(j=get?144:140;j<176;j++)hcheck(root[j]==0x5a,"result/caller guard");
 if(get){memmove(&v,root+136,8);hcheck(v==(*failure?marker:args[method-1]),"exact managed field result");}
 else{memmove(&word,root+136,4);hcheck(word==(*failure?mark:n),"exact Word field result");}
 if(!*failure){
  hcheck(r->pad==0x31415926&&r->inner.number==n&&r->left==23+(!clear&&args[0]!=H)&&r->right==(clear||profile==1||profile==2?29:-7)&&r->ending==-32,"nested field values and neighboring guards");
  for(j=0;j<6;j++){
   memmove(&v,(uchar*)r+fields[j],8);hcheck(v==(clear?H:args[j]),"exact heap root fields");
   refs=1+(args[j]!=H)*(1+!clear+(j==0&&!clear)+(get&&method-1==j));hcheck(D2H(values[j])->ref==refs,"exact heap root reference counts");hcheck(D2H(old[j])->ref==1,"replaced root retired once");
  }
  hcheck(D2H(r)->ref==3,"FP argument and MP owner retained");memmove(&v,ml->MP+record_globals[1],8);hcheck(v==(clear?H:args[0]),"published root field");
  {WORD expected[]={clear||profile==1?0:profile==2?0:2,clear||profile==1?0:3,clear||profile==1?0:1,clear||profile==1||profile==2?-1:9731};for(j=0;j<4;j++){memmove(&word,ml->MP+record_globals[2+j],4);hcheck(word==expected[j],"field lengths and Unicode character");}}
 }
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);require(native64typedmoduleclear(&owner));hcheck(D2H(r)->ref==1,"record fixture owner retained once");destroy(r);freetype(t);
 for(j=0;j<6;j++){hcheck(D2H(values[j])->ref==1&&D2H(old[j])->ref==1,"all field roots retired once");destroy(values[j]);destroy(old[j]);}hcheck(D2H(marker)->ref==1,"result marker retained once");destroy(marker);
 hcheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"complete field/frame/module retirement");
}
static void refusals(void)
{
 Native64ModuleSpec s;Native64ModuleFunction f[9];Native64TypedStep c[160];Native64ModuleOwner owner={0};REG saved=R;uint fn,pc,k,total=0;int before;
 for(fn=0;fn<9;fn++)for(pc=0;pc<full_functions[fn].count;pc++){
  const Native64TypedStep*v=full_functions[fn].code+pc;if(v->returned!=72)continue;
  for(k=0;k<6;k++){
   s=full_spec;memmove(f,full_functions,sizeof(f));s.functions=f;require(f[fn].count<=nelem(c));memmove(c,f[fn].code,f[fn].count*sizeof(*c));f[fn].code=c;
   switch(k){case 0:c[pc].op=INOP;break;case 1:c[pc].mode|=256;break;case 2:c[pc].s=LONG_MAX;break;case 3:c[pc].d=LONG_MAX;break;case 4:c[pc].m=LONG_MAX;break;case 5:if(USRC(v->mode)&AIND)c[pc].s++;else c[pc].d++;break;}
   before=allocations;
   if(native64typedmodule(&s,0,&owner)){fprintf(stderr,"field refusal fn=%u pc=%u k=%u\n",fn,pc,k);hcheck(0,"malformed field metadata refused");}
   hcheck(allocations==before&&!owner.module&&!live&&!memcmp(&R,&saved,sizeof(R)),"field metadata refused before allocation");total++;
  }
 }
 printf("PASS record metadata: %u shape/mode/offset/operand refusals before allocation\n",total);
}
static void construction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,before,n,i;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&full_spec,jit,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&full_spec,jit,&owner));failat=0;hcheck(!owner.module&&!live&&!nlinkpins&&!memcmp(&R,&saved,sizeof(R)),"field capability constructor rollback");}
  printf("PASS field constructor: jit=%d %d allocation faults with complete rollback\n",jit,n);
 }
}
static Module*lexcaller(int grow)
{
 Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,0xe0};require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->entryt=m->type[0]=dtype(freeheap,128,map,2);require(m->entryt!=nil);
 p=m->prog=mallocz(5*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=5;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=80;p[0].d.ind=40;
 p[1].op=IMOVP;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=64;p[1].d.i.f=40;p[1].d.i.s=64;
 p[2]=p[1];p[2].s.ind=72;p[2].d.i.s=72;
 p[3].op=IMCALL;p[3].add=SRC(AFP)|AXIMM|DST(AFP);p[3].s.ind=40;p[3].d.ind=80;
 p[4].op=IEXIT;p[4].add=SRC(AXXX)|DST(AXXX);return m;
}
static void lexcase(int jit,int quantum,WORD position,uint profile,int length,int file)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import request[2]={{0}};Type*t,*at;uchar map[2]={0},*root;YYLEX*l;Array*a;String*e,*old,*input;void*fd;Prog*p;
 uint roots[]={offsetof(YYLEX,lval.node),offsetof(YYLEX,lval.word),offsetof(YYLEX,lval.redir),offsetof(YYLEX,err),offsetof(YYLEX,path),offsetof(YYLEX,cbuf),offsetof(YYLEX,f),offsetof(YYLEX,s),offsetof(YYLEX,prompt)},j,steps;WORD want,nc=position;UWORD bits;int done,replace;
 for(j=0;j<nelem(roots);j++)map[roots[j]/64]|=128>>(roots[j]/8%8);
 require(native64typedmodule(&lexer_spec,jit,&owner));t=dtype(freeheap,sizeof(*l),map,2);require(t!=nil);l=H2D(YYLEX*,heapz(t));at=dtype(freeheap,4,nil,0);require(at!=nil);a=length<0?H:H2D(Array*,heaparray(at,length));freetype(at);fd=object(0);e=object(1);old=profile==2?newstring(0):object(1);input=profile==3?H:e;
 l->linenum=31337;l->errline=-17;l->strpos=position;l->ncbuf=nc;
 if(profile==1||profile==2){D2H(old)->ref++;l->err=old;}if(a!=H){D2H(a)->ref++;l->cbuf=a;}if(file){D2H(fd)->ref++;l->f=fd;}
 replace=profile!=1;bits=(UWORD)position-1;memmove(&want,&bits,4);if(file&&want<0)want=(length<0?0:length)-1;if(file){bits=(UWORD)nc+1;memmove(&nc,&bits,4);}
 request[0].name="run";memmove(&request[0].sig,&lexer_functions[0].signature,4);owner.module->ref++;ml=linkmod(owner.module,request,1);require(ml!=H&&ml!=nil);
 m=lexcaller(quantum==1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;
 D2H(l)->ref++;memmove(root+64,&l,8);if(input!=H)D2H(input)->ref++;memmove(root+72,&input,8);memmove(root+80,&ml,8);memset(root+88,0x5a,40);p->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}
 hcheck(steps<10000&&!worker.env->errstr[0],"unchanged lexer methods completed");hcheck(l->strpos==want&&l->ncbuf==nc,"unchanged ungetc wrap/buffer arithmetic");
 hcheck(l->err==(replace?input:old)&&l->errline==(replace?31337:-17)&&l->linenum==31337,"unchanged error first-message/String-nil semantics");
 for(j=88;j<128;j++)hcheck(root[j]==0x5a,"lexer caller guards");hcheck(D2H(l)->ref==2&&D2H(e)->ref==1+(input!=H)*(1+replace)&&D2H(old)->ref==1+!replace,"exact lexer roots");
 observed=p;progexit();emptyqueues();require(m->ref==1);unload(m);require(native64typedmoduleclear(&owner));destroy(l);freetype(t);if(a!=H){hcheck(D2H(a)->ref==1,"lexer buffer retained once");destroy(a);}hcheck(D2H(e)->ref==1&&D2H(old)->ref==1&&D2H(fd)->ref==1,"lexer fixture ownership restored");destroy(e);destroy(old);destroy(fd);
 hcheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"complete original lexer retirement");
}
int main(void)
{
 uint method,k,profile,total=0,lexers=0;int jit,q,b,file,quanta[]={1,3,1000},lengths[]={-1,0,4};WORD values[]={-2147483647-1,-1,0,1,17,2147483647};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tarray.free=freearray;Tarray.size=sizeof(Array);Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 RECORD_HANDLERS
 optab[IMOVW]=fieldword;optab[IMOVP]=actual_movp;optab[ILENA]=actual_lena;optab[ILENC]=actual_lenc;optab[ILENL]=actual_lenl;optab[IINDC]=actual_indc;optab[IBEQC]=actual_beqc;optab[IBNEC]=actual_bnec;
 hcheck(limbo_native64_Records_layout(),"natural C nested geometry");refusals();construction();
 for(method=0;method<9;method++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(k=0;k<nelem(values);k++)for(profile=0;profile<7;profile++){runfield(method,jit,quanta[q],values[k],profile);total++;}
 printf("PASS generic record fields: %u C/JIT cases, nested FP/MP fields, six root results, Word arithmetic, nil/alias/empty String, Unicode, actual heap size/GC roles, guards, exact ownership and complete retirement\n",total);
 current=nil;hcheck(limbo_native64_Lexer_layout(),"original lexer natural C layout");
 for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(k=0;k<nelem(values);k++)for(profile=0;profile<4;profile++)for(b=0;b<3;b++)for(file=0;file<2;file++){lexcase(jit,quanta[q],values[k],profile,lengths[b],file);lexers++;}
 printf("PASS unchanged Sh lexer methods: %u C/JIT cases, original error/ungetc bodies, local call chain, 128-byte YYLEX map, overflow/buffer wrap, nil/empty/first-error semantics and complete retirement\n",lexers);return 0;
}
