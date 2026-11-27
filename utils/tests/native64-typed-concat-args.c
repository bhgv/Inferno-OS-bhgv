#include "concat.h"
static Module*concatowner;
static uint concatfunction,concatsteps,concatcalls;
static int concatfailure,concatallocates;
static Rune wanted[64];static uint nwanted;
static void concatcheck(int ok,char*why){if(!ok){fprintf(stderr,"concat predicate: %s\n",why);exit(10);}}
static uint textlength(String*s){return s==H?0:s->len<0?-s->len:s->len;}
static Rune textrune(String*s,uint i){return s->len<0?s->Srune[i]:s->Sascii[i];}
static void concattext(String*s)
{
 uint i;concatcheck(textlength(s)==nwanted,"complete result length");for(i=0;i<nwanted;i++)concatcheck(textrune(s,i)==wanted[i],"complete ASCII/Rune/NUL result");
}
static void checkedconcat(void)
{
 const Native64TypedStep*step;uint i,base=0,index;void *middle,*source;
 require(concatowner && R.M->m==concatowner);
 for(i=0;i<concatfunction;i++)base+=concat_client_functions[i].count;
 index=concat_pcs[concatfunction]-base;step=concat_client_functions[concatfunction].code+index;
 concatcheck(R.PC-1==concatowner->prog+concat_pcs[concatfunction],"concat exact PC");
 middle=((step->mode&ARM)==AXINF?R.FP:R.MP)+step->m;source=(USRC(step->mode)==AFP?R.FP:R.MP)+step->s;
 concatcheck(R.m==middle,"concat middle publication");concatcheck(R.s==source,"concat source publication");
 concatsteps++;if(concatfailure>0 && concatallocates && concatsteps==(uint)concatfailure)failat=allocations+1;
 actual_addc();concattext(*(String**)R.d);
}
static void concatmcall(void)
{
 Frame*pending;String*text;WORD mode;
 if(concatowner && R.M->m==concatowner){memmove(&pending,R.s,sizeof(pending));memmove(&text,(uchar*)pending+64,sizeof(text));memmove(&mode,(uchar*)pending+72,4);concatcheck(mode==0,"imported scalar suffix preserved");concattext(text);concatcalls++;}
 actual_mcall();
}
static void concatpreflight(void)
{
 Native64ModuleSpec spec=concat_client_spec;Native64ModuleFunction functions[nelem(concat_client_functions)];Native64TypedStep code[128];Native64ModuleSite sites[4];Native64ModuleImport request=concat_requests[0];Native64ModuleTable table={&request,1};
 Native64ModuleOwner owner={0};uchar map[128],rmap[2];REG saved=R;uint i,j,bit;int before;
 for(i=0;i<12;i++){
  memmove(functions,concat_client_functions,sizeof(functions));spec.functions=functions;spec.tables=&table;request=concat_requests[0];memmove(rmap,concat_request_map,2);request.map=rmap;
  require(functions[0].count<=nelem(code) && functions[0].nsites<=nelem(sites) && functions[0].mapbytes<=sizeof(map));
  memmove(code,functions[0].code,functions[0].count*sizeof(*code));functions[0].code=code;
  memmove(sites,functions[0].sites,functions[0].nsites*sizeof(*sites));functions[0].sites=sites;
  memmove(map,functions[0].map,functions[0].mapbytes);functions[0].map=map;
  for(j=0;j<functions[0].count;j++)if(code[j].op==IADDC)break;require(j<functions[0].count && USRC(code[j].mode)==AFP);
  if(i==0){bit=code[j].s/8;map[bit/8]&=~(0x80>>(bit%8));}
  if(i==1)rmap[1]=0;
  if(i==2)code[j].mode=(code[j].mode&~ARM)|AXIMM;
  if(i==3)code[j].m++;
  if(i==4)code[j].m=65536;
  if(i==5)code[j].s=functions[0].bytes-4;
  if(i==6)code[j].d=72;
  if(i==7)code[j].d=request.bytes;
  if(i==8)code[j].returned=0;
  if(i==9)sites[0].first=j;
  if(i==10)sites[0].external=0;
  if(i==11)code[j].mode|=256;
  before=allocations;
  if(native64typedmodule(&spec,0,&owner)){fprintf(stderr,"concat metadata refusal removed: case=%u\n",i);exit(10);}
  concatcheck(allocations==before,"metadata refuses before allocation");require(!live && !nlinkpins && !owner.module && !memcmp(&R,&saved,sizeof(R)));
 }
 puts("PASS concat metadata: twelve no-allocation role/map/span/site refusals");
}
static void concatconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){
  before=allocations;require(native64typedmodule(&concat_client_spec,jit,&owner));n=allocations-before;require(native64typedmoduleclear(&owner));
  for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&concat_client_spec,jit,&owner));failat=0;require(!owner.module && !owner.rooted && !live && !nlinkpins && !memcmp(&R,&saved,sizeof(R)));}
  printf("PASS concat constructor: jit=%d %d allocation refusals and complete rollback\n",jit,n);
 }
}
static Modlink*concatinstance(Module*m,const char*name,uint sig)
{
 Import request[2]={{0}};Modlink*ml;request[0].name=(char*)name;memmove(&request[0].sig,&sig,4);m->ref++;ml=linkmod(m,request,1);require(ml!=H);return ml;
}
static Module*concatlauncher(int jit,int grow,int fp)
{
 Module*m;Inst*p;uchar map[]={0,0xf8};int n=0,i,a;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;
 m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,128,map,2);m->entryt=m->type[0];require(m->entryt!=nil);
 p=m->prog=mallocz(15*sizeof(*p),1);require(p!=nil);m->entry=p;
 for(i=0;i<2;i++){
  p[n].op=IMFRAME;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=72;p[n++].d.ind=40;
  for(a=0;a<(fp?3:2);a++){p[n].op=IMOVP;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=a==0?64:72+8*a;p[n].d.i.f=40;p[n++].d.i.s=64+8*a;}
  p[n].op=ILEA;p[n].reg=24;p[n].add=SRC(AFP)|DST(AFP|AIND);p[n].s.ind=96;p[n].d.i.f=40;p[n++].d.i.s=32;
  p[n].op=IMCALL;p[n].add=SRC(AFP)|AXIMM|DST(AFP);p[n].s.ind=40;p[n++].d.ind=72;
 }
 p[n].op=IEXIT;p[n++].add=SRC(AXXX)|DST(AXXX);m->nprog=n;if(jit)require(compile(m,n,nil));return m;
}
static String*concatinput(int which)
{
 String*s;if(which==0 || which==5)return H;if(which==1 || which==6)return newstring(0);
 if(which==2)return c2string("x\0y",3);
 if(which==3){s=newrunes(3);s->Srune[0]=0x4e16;s->Srune[1]=0;s->Srune[2]=0x1f680;return s;}
 return c2string("abc",3);
}
static void concatrun(int pjit,int cjit,int grow,int quantum,int fn,int input,int failure)
{
 Native64ModuleOwner provider={0},client={0};Modlink*p,*c;Module*outer;Prog*process;uchar*root;String*left,*right,*result;uint i,j,done,steps;
 require(native64typedmodule(&concat_provider_spec,pjit,&provider) && native64typedmodule(&concat_client_spec,cjit,&client));
 p=concatinstance(provider.module,"open",concat_provider_functions[0].signature);c=concatinstance(client.module,concat_client_functions[fn].name,concat_client_functions[fn].signature);
 concatowner=client.module;concatfunction=fn;concatsteps=concatcalls=0;concatfailure=failure;
 left=concatinput(input);right=input==4 || input==6?left:input==5?H:concatinput((input+2)%5);if((input==4 || input==6) && left!=H)D2H(left)->ref++;
 concatallocates=!(fn==3 && left==H && right==H);nwanted=0;
 if(fn==0 || fn==1){wanted[nwanted++]='#';wanted[nwanted++]='e';wanted[nwanted++]='/';}
 for(i=0;i<textlength(left);i++)wanted[nwanted++]=textrune(left,i);
 if(fn==2){wanted[nwanted++]='#';wanted[nwanted++]='e';wanted[nwanted++]='/';}
 if(fn==3)for(i=0;i<textlength(right);i++)wanted[nwanted++]=textrune(right,i);
 outer=concatlauncher(cjit,grow,fn==3);starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;
 memmove(root+64,&p,sizeof(p));memmove(root+72,&c,sizeof(c));memmove(root+80,&left,sizeof(left));memmove(root+88,&right,sizeof(right));memset(root+104,0xa5,24);process->quanta=quantum;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}
 failat=0;
 {int oom=failure>0 && concatallocates && (!concat_fault || failure==1);const char*expected=oom?exHeap:concat_fault?exZdiv:"";
  if(steps>=10000 || strcmp(worker.env->errstr,expected))fprintf(stderr,"concat jit=%d/%d grow=%d q=%d fn=%d input=%d fault=%d error=%s expected=%s\n",pjit,cjit,grow,quantum,fn,input,failure,worker.env->errstr,expected);
  concatcheck(steps<10000 && !strcmp(worker.env->errstr,expected),"source concat call result");
  concatcheck(concatsteps==(oom?(uint)failure:expected[0]?1:2),"concat instruction observations");
  if(!cjit)concatcheck(concatcalls==(oom?(uint)failure-1:expected[0]?1:2),"explicit imported call observations");
  if(!expected[0]){require(R.FP==root);memmove(&result,root+96,sizeof(result));concattext(result);if(left!=H)concatcheck(D2H(left)->ref==(left==right?2:1),"source ownership unchanged");}
 }
 for(j=104;j<128;j++)require(root[j]==0xa5);
 concatowner=nil;observed=process;progexit();emptyqueues();require(provider.module->ref==1 && client.module->ref==1);require(native64typedmoduleclear(&client) && native64typedmoduleclear(&provider));
 require(!live && !nlinkpins && !modules && !worker.nerr && Tstring.ref==1 && Tmodlink.ref==1);
 memset(nativeobservations,0,sizeof(nativeobservations));
}
int main(int argc,char**argv)
{
 int p,c,g,q,fn,input,failure,total=0;int quantum[]={1,3,1000};USED(argv);require(argc==1);
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;
 worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;
 require(dis64selectlaunch("dis64-calls"));concatpreflight();concatconstruction();
 CONCAT_HANDLERS
 optab[IADDC]=checkedconcat;optab[IMCALL]=concatmcall;optab[IMOVP]=actual_movp;optab[ILENC]=actual_lenc;
 for(p=0;p<2;p++)for(c=0;c<2;c++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(fn=0;fn<4;fn++)for(input=0;input<7;input++)for(failure=0;failure<3;failure++){
  concatrun(p,c,g,quantum[q],fn,input,failure);total++;
 }
 printf("PASS computed String arguments: %d C/JIT cases, FP/MP order, ASCII/Rune/NUL/nil, alias/replacement, allocation failure and full retirement\n",total);return 0;
}
