typedef struct Expected Expected;struct Expected{uint bytes;const uchar*text;};
typedef struct ParseCase ParseCase;struct ParseCase{uint bytes;const uchar*text;uint count;const Expected*words;int nilinput;};
#include "unquoted.h"
static Module*parseowner;static uint selfcalls;
static void parsecheck(int ok,char*why){if(!ok){fprintf(stderr,"real unquoted predicate: %s\n",why);exit(10);}}
static void parsepreflight(void)
{
 Native64ModuleSpec spec=parse_spec;Native64ModuleFunction functions[2];Native64TypedStep code[128];Native64ModuleSite sites[4];Native64ModuleOwner owner={0};uchar map[128];REG saved=R;
 uint i,j,index=~0U,slice=~0U,concat=~0U,bit;int before;
 for(j=0;j<parse_functions[1].count;j++){
  if(parse_functions[1].code[j].op==IINDC && index==~0U)index=j;
  if(parse_functions[1].code[j].op==ISLICEC && slice==~0U)slice=j;
  if(parse_functions[1].code[j].op==IADDC && !parse_functions[1].code[j].returned && concat==~0U)concat=j;
 }
 require(index!=~0U && slice!=~0U && concat!=~0U && parse_functions[1].count<=nelem(code) && parse_functions[1].nsites<=nelem(sites) && parse_functions[1].mapbytes<=sizeof(map));
 for(i=0;i<15;i++){
  memmove(functions,parse_functions,sizeof(functions));spec.functions=functions;
  memmove(code,functions[1].code,functions[1].count*sizeof(*code));functions[1].code=code;
  memmove(sites,functions[1].sites,functions[1].nsites*sizeof(*sites));functions[1].sites=sites;
  memmove(map,functions[1].map,functions[1].mapbytes);functions[1].map=map;
  if(i==0){bit=code[concat].s/8;map[bit/8]&=~(0x80>>(bit%8));}
  if(i==1){bit=code[index].d/8;map[bit/8]|=0x80>>(bit%8);}
  if(i==2)code[index].s+=4;
  if(i==3)code[index].d=code[concat].s;
  if(i==4)code[slice].mode=(code[slice].mode&~ARM)|AXINM;
  if(i==5){code[slice].mode=(code[slice].mode&~56)|SRC(AIMM);code[slice].s=536870912L;}
  if(i==6)code[concat].m=65536;
  if(i==7)code[concat].returned=24;
  if(i==8)sites[0].inherited=0;
  if(i==9)sites[0].target=0;
  if(i==10)code[sites[0].end+1].op=INOP;
  if(i==11)code[sites[0].first+1].d=72;
  if(i==12)code[index].m=code[concat].s;
  if(i==13)code[concat].mode=(code[concat].mode&~56)|SRC(AIMM);
  if(i==14)code[index].mode=(code[index].mode&~56)|SRC(AFP|AIND);
  before=allocations;
  if(native64typedmodule(&spec,0,&owner)){fprintf(stderr,"real unquoted metadata refusal removed: case=%u\n",i);exit(10);}
  parsecheck(allocations==before,"metadata preallocation refusal");require(!owner.module && !live && !nlinkpins && !memcmp(&R,&saved,sizeof(R)));
 }
 puts("PASS real parser metadata: fifteen role/map/span/self-tail refusals before allocation");
}
static void parseconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&parse_spec,jit,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&parse_spec,jit,&owner));failat=0;require(!owner.module && !owner.rooted && !live && !nlinkpins && !memcmp(&R,&saved,sizeof(R)));}
  printf("PASS real parser constructor: jit=%d %d allocation failures, unchanged REG and complete rollback\n",jit,n);
 }
}
static void parsecall(void)
{
 Inst*target;Frame*pending;void*from,*to;
 memmove(&target,R.d,sizeof(target));
 if(parseowner && target==parseowner->prog+unquoted_entry){
  memmove(&pending,R.s,sizeof(pending));memmove(&from,R.FP+32,sizeof(from));memmove(&to,(uchar*)pending+32,sizeof(to));
  parsecheck(from==to,"inherited result pointer");if(R.PC>parseowner->prog+unquoted_entry)selfcalls++;
 }
 actual_call();
}
static Module*parselauncher(int jit,int grow)
{
 Module*m;Inst*p;uchar map[]={0,0xe0};int n=0;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,128,map,2);m->entryt=m->type[0];require(m->entryt!=nil);
 p=m->prog=mallocz(5*sizeof(*p),1);require(p!=nil);m->entry=p;
 p[n].op=IMFRAME;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=64;p[n++].d.ind=40;
 p[n].op=IMOVP;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=72;p[n].d.i.f=40;p[n++].d.i.s=64;
 p[n].op=ILEA;p[n].reg=24;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=80;p[n].d.i.f=40;p[n++].d.i.s=32;
 p[n].op=IMCALL;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=40;p[n++].d.ind=64;
 p[n].op=IEXIT;p[n++].add=SRC(AXXX)|DST(AXXX);m->nprog=n;if(jit)require(compile(m,n,nil));return m;
}
static uint parserun(int jit,int outerjit,int grow,int quantum,uint which,int fault)
{
 Native64ModuleOwner owner={0};Modlink*ml;Module*outer;Import request[2]={{0}};Prog*process;uchar*root;String*input;List*result;uint i,j,steps,done,allocs;int before;
 const ParseCase*test=parse_cases+which;
 require(native64typedmodule(&parse_spec,jit,&owner));request[0].name="parse";memmove(&request[0].sig,&parse_functions[0].signature,4);owner.module->ref++;ml=linkmod(owner.module,request,1);require(ml!=H);
 parseowner=owner.module;selfcalls=0;outer=parselauncher(outerjit,grow);starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;
 input=test->nilinput?H:c2string((char*)test->text,test->bytes);memmove(root+64,&ml,sizeof(ml));memmove(root+72,&input,sizeof(input));memset(root+88,0xa5,40);process->quanta=quantum;
 before=allocations;if(fault)failat=allocations+fault;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}
 allocs=allocations-before;failat=0;
 if(steps>=10000 || (!fault && worker.env->errstr[0]))fprintf(stderr,"real unquoted jit=%d/%d grow=%d q=%d case=%u fault=%d error=%s\n",jit,outerjit,grow,quantum,which,fault,worker.env->errstr);
 parsecheck(steps<10000,"termination");
 if(fault)parsecheck(!strcmp(worker.env->errstr,exHeap) || !strcmp(worker.env->errstr,exNomem),"injected allocation error");
 else{
  parsecheck(!worker.env->errstr[0] && R.FP==root,"ordinary return");memmove(&result,root+80,sizeof(result));
  for(i=0;i<test->count;i++){
   String*actual,*expected;parsecheck(result!=H,"word count");actual=*(String**)result->data;expected=c2string((char*)test->words[i].text,test->words[i].bytes);
   parsecheck((actual==H?0:actual->len<0?-actual->len:actual->len)==(expected->len<0?-expected->len:expected->len),"word length");
   for(j=0;j<(uint)(expected->len<0?-expected->len:expected->len);j++)parsecheck((actual->len<0?actual->Srune[j]:actual->Sascii[j])==(expected->len<0?expected->Srune[j]:expected->Sascii[j]),"word contents");
   destroy(expected);result=result->tail;
  }
  parsecheck(result==H,"no extra words");if(input!=H)parsecheck(D2H(input)->ref==1,"input ownership");
  if(!jit && (which==6 || which==7))parsecheck(selfcalls==1,"actual balancing recursion");
 }
 for(i=88;i<128;i++)require(root[i]==0xa5);
 parseowner=nil;observed=process;progexit();emptyqueues();require(owner.module->ref==1 && native64typedmoduleclear(&owner));
 require(!live && !nlinkpins && !modules && !worker.nerr && Tstring.ref==1 && Tlist.ref==1 && Tptr.ref==1 && Tmodlink.ref==1);
 memset(nativeobservations,0,sizeof(nativeobservations));return allocs;
}
int main(int argc,char**argv)
{
 int jit,outer,g,q,f,cases=0,faults=0;uint i,n;int quantum[]={1,3,1000};USED(argv);require(argc==1 || argc==2);
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));parsepreflight();parseconstruction();if(argc==2)return 0;
 PARSE_HANDLERS
 optab[IMOVP]=actual_movp;optab[ILENC]=actual_lenc;optab[IADDC]=actual_addc;optab[IINDC]=actual_indc;optab[ISLICEC]=actual_slicec;optab[ICONSP]=actual_consp;optab[ICALL]=parsecall;
 for(jit=0;jit<2;jit++)for(outer=0;outer<2;outer++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(i=0;i<nelem(parse_cases);i++){
  n=parserun(jit,outer,g,quantum[q],i,0);cases++;
  if(outer==0 && q==0)for(f=1;f<=(int)n;f++){parserun(jit,outer,g,quantum[q],i,f);faults++;}
 }
 printf("PASS unchanged emuinit.unquoted: %d C/JIT inputs, %d execution allocation failures, real quote balancing recursion and complete retirement\n",cases,faults);return 0;
}
