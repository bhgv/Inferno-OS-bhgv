#include "full.h"
static void spcheck(int ok,const char*why){if(!ok){fprintf(stderr,"spawn predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);spcheck(0,"unexpected LOAD");return H;}
static Module*sproot(int grow){Module*m;Inst*p;uchar map[]={0,0x70};uint pc=0,i;m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("spawn-caller");m->path=strdup("spawn-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->name&&m->path&&m->type);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,112,map,2);require(m->type[0]&&m->type[1]);p=m->prog=mallocz(6*sizeof(Inst),1);require(p!=nil);m->entry=p;m->cstep=native64typedlea8step;p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=88;p[pc++].d.ind=40;for(i=0;i<3;i++){p[pc].op=i?IMOVP:IMOVW;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64+8*i;p[pc].d.i.f=40;p[pc++].d.i.s=64+8*i;}p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=88;p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;return m;}
static List*
splist(String *text)
{
 List *items=H,*head=cons(8,&items);
 head->t=&Tptr;
 Tptr.ref++;
 if(text!=H)D2H(text)->ref++;
 memmove(head->data,&text,sizeof(text));
 text=c2string("prefix",6);
 head=cons(8,&items);
 head->t=&Tptr;
 Tptr.ref++;
 memmove(head->data,&text,sizeof(text));
 return items;
}

static void *spitem;
static int sprepfault;

static Array*
sparray(String *text)
{
 Array *array=H2D(Array*,heaparray(&Tptr,2));
 spitem=H;
 if(text!=H){
  spitem=H2D(void*,heapz(&Tptr));
  D2H(text)->ref++;
  memmove(spitem,&text,sizeof(text));
  memmove(array->data+8,&spitem,sizeof(spitem));
 }
 return array;
}

static void
spnewz(void)
{
 if(sprepfault==1)failat=allocations+1;
 actual_newz();
}

static void
spconsmp(void)
{
 if(sprepfault==2)failat=allocations+1;
 actual_consmp();
}

static void
sprecord(Modlink *ml, uint fn, int expected, WORD number)
{
 void *wrapper,*item,*empty;
 List *list;
 WORD words[3];

 memmove(&wrapper,ml->MP+spawn_globals[4],sizeof(wrapper));
 if(fn<5 || !expected){
  spcheck(wrapper==H,"no unpublished record retained");
  return;
 }
 spcheck(wrapper!=H&&D2H(wrapper)->t->size==8&&D2H(wrapper)->t->map[0]==0x80,
         "spawn child received the mapped wrapper");
 memmove(&list,wrapper,sizeof(list));
 if(fn==5){
  spcheck(list==H,"direct record allocation initialized its list root");
  return;
 }
 spcheck(list!=H&&list->tail==H&&list->t->size==32&&list->t->map[0]==0xc0,
         "compound argument kept its singleton list and mixed layout");
 memmove(&item,list->data,sizeof(item));
 memmove(&empty,(uchar*)list->data+8,sizeof(empty));
 memmove(words,(uchar*)list->data+16,sizeof(words));
 spcheck(item==spitem&&empty==H&&words[0]==number&&words[1]==number+1&&words[2]==number+2,
         "child record kept its array element, nil and inline Words");
}

static void
sprecordmetadata(void)
{
 Native64ModuleSpec spec=full_spec;
 Native64ModuleFunction functions[nelem(full_functions)],*f;
 Native64ModuleOwner owner={0};
 Native64TypedStep code[64];
 uint k,start,n;
 int before;

 memmove(functions,full_functions,sizeof(functions));
 spec.functions=functions;
 for(k=0;k<12;k++){
  n=spawn_functions[k==0?5:6];
  f=functions+n;
  *f=full_functions[n];
  spcheck(f->count<=nelem(code),"bounded record metadata fixture");
  memmove(code,f->code,f->count*sizeof(*code));
  f->code=code;
  for(start=f->sites[0].first+1;start<f->sites[0].end;start++)
   if(code[start].returned==Native64TypedRecordNew)break;
  spcheck(start<f->sites[0].end,"record allocation belongs to a spawn site");
  switch(k){
  case 0: code[start].d=64; break;
  case 1: code[start].m^=1; break;
  case 2: code[start+1].m=56; break;
  case 3: code[start+2].d+=8; break;
  case 4: code[start+3].s+=8; break;
  case 5: code[start+4].m-=8; break;
  case 6: code[start+6].m^=1; break;
  case 7: code[start+7].d|=8<<16; break;
  case 8: code[start+9].d+=8; break;
  case 9: code[start+11].d=64; break;
  case 10: code[start+12].returned=0; break;
  case 11: code[0]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,start+4,0}; break;
  }
  before=allocations;
  spcheck(!native64typedmodule(&spec,0,&owner)&&allocations==before&&!owner.module,
          "malformed record preparation refused before allocation");
  *f=full_functions[n];
 }
 puts("PASS spawn record metadata: 12 allocation, mixed layout, rooting and control flow refusals");
}

static void sprun(uint fn,int jit,int quantum,WORD number,int fault,int second,int nils,int roundrobin){Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};Prog*p,*parent;String*s,*saved;Channel*c,*channel;uchar*root;WORD count,total;uint i,ticks=0,retired=0,broken=0;int done,steps,expected,children=fn==1?2:1;spawnfailure=0;spawnattempts=0;
 spcheck(native64typedmodule(&full_spec,jit,&owner),"owning spawn module");s=nils?H:c2string("argument",8);c=nils?H:cnewc(nil,actual_movw,0);if(s!=H)ptradd(D2H(s));if(c!=H)ptradd(D2H(c));imports[0].name=(char*)full_functions[spawn_functions[fn]].name;memmove(&imports[0].sig,&full_functions[spawn_functions[fn]].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;m=sproot(quantum==1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=parent=started;rootenvironment(p);root=p->R.FP;memmove(root+88,&ml,8);memmove(root+64,&number,4);memset(root+68,0xa5,4);if(fn==4||fn==5){List *items=splist(s);memmove(root+72,&items,8);}else if(fn==6){Array *items=sparray(s);memmove(root+72,&items,8);}else{if(s!=H)D2H(s)->ref++;memmove(root+72,&s,8);}if(c!=H)D2H(c)->ref++;memmove(root+80,&c,8);memset(root+96,0x5a,16);spawnattempts=0;spawnfailure=fault<4?fault:0;sprepfault=fault>=4?fault-3:0;spawnfailat=second?2:1;
 while((p=currun())!=nil){spcheck(++ticks<100000,"bounded round-robin schedule");p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;if(roundrobin){delrun(Pready);addrun(p);break;}}if(done)continue;failat=0;spcheck(steps<10000,"bounded scheduler execution");if(worker.env->errstr[0]){broken++;spcheck(!strcmp(worker.env->errstr,exNomem)||!strcmp(worker.env->errstr,"child fault")||(fault>=4&&!strcmp(worker.env->errstr,exHeap)),"exact process error");}
 if(p->exval!=H){spcheck(!handler(worker.env->errstr),"unhandled child exception");spcheck(p->exval==H,"child exception root retired");}if(p==parent){for(i=68;i<72;i++)spcheck(root[i]==0xa5,"parent Word guard");for(i=96;i<112;i++)spcheck(root[i]==0x5a,"parent frame guard");}observed=p;progexit();retired++;}
 spawnfailure=0;emptyqueues();memmove(&count,ml->MP+spawn_globals[0],4);memmove(&total,ml->MP+spawn_globals[1],4);memmove(&saved,ml->MP+spawn_globals[2],8);memmove(&channel,ml->MP+spawn_globals[3],8);expected=fault?(second?(fn==1?1:0):0):children;spcheck(count==expected&&total==expected*number,"exact children and Word arguments");spcheck(retired==(uint)(1+expected+(!fault&&fn==2?1:0)+(fault&&second&&fn==2?1:0)),"actual scheduler process retirement");spcheck(broken==(uint)((fault?1:0)+(number<0?expected:0)),"parent/child failure isolation");if(expected){spcheck(channel==c,"child channel argument");if(fn==3){uint len=nils?1:9;spcheck(saved!=H&&saved->len==(int)len&&saved->Sascii[len-1]=='!',"temporary String survives parent exit");}else spcheck(saved==(fn==6?H:s),"child String argument");}else spcheck(saved==H&&channel==H,"failed spawn did not dispatch child");
 sprecord(ml,fn,expected,number);sprepfault=0;require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);spcheck(native64typedmoduleclear(&owner),"spawn module retired");if(s!=H){spcheck(D2H(s)->ref==1,"String argument retired");ptrdel(D2H(s));destroy(s);}if(c!=H){spcheck(D2H(c)->ref==1,"channel argument retired");ptrdel(D2H(c));destroy(c);}if(live||modules||nlinkpins||worker.nerr||Tchannel.ref!=1||Tstring.ref!=1||Tmodlink.ref!=1)fprintf(stderr,"spawn retirement live=%d modules=%p pins=%d err=%d channel=%d string=%d modlink=%d\n",live,modules,nlinkpins,worker.nerr,Tchannel.ref,Tstring.ref,Tmodlink.ref);spcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tchannel.ref==1&&Tstring.ref==1&&Tmodlink.ref==1,"complete process/environment/frame/module retirement");spcheck(rootpgrp.r.ref==1&&rootfgrp.r.ref==1&&rootegrp.r.ref==1,"shared environment references retired");memset(nativeobservations,0,sizeof(nativeobservations));}
static void spmetadata(void){Native64ModuleSpec spec;Native64ModuleFunction f[nelem(full_functions)];Native64TypedStep code[64],childcode[64];Native64ModuleSite sites[8];Native64ModuleOwner owner={0};uchar map[16],childmap[16];REG saved=R;uint k,pc,target,bit;int before;
 for(k=0;k<16;k++){spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[0].count<=nelem(code)&&f[0].nsites<=nelem(sites));memmove(code,f[0].code,f[0].count*sizeof(*code));f[0].code=code;memmove(sites,f[0].sites,f[0].nsites*sizeof(*sites));f[0].sites=sites;memmove(map,f[0].map,f[0].mapbytes);f[0].map=map;pc=sites[0].end;target=sites[0].target;require(f[target].count<=nelem(childcode));memmove(childcode,f[target].code,f[target].count*sizeof(*childcode));f[target].code=childcode;memmove(childmap,f[target].map,f[target].mapbytes);f[target].map=childmap;
 switch(k){case 0:code[pc].op=ICALL;break;case 1:code[pc].m=1;break;case 2:code[pc].s=48;break;case 3:code[pc].mode=SRC(AMP)|DST(AIMM);break;case 4:code[pc].returned=0;break;case 5:sites[0].external=1;break;case 6:sites[0].contract=24;break;case 7:sites[0].inherited=1;break;case 8:sites[0].target=nelem(full_functions);break;case 9:f[target].resultcontract=48;break;case 10:childcode[0].returned=24;break;case 11:f[0].nsites=0;break;case 12:bit=72/8;map[bit/8]&=~(128>>(bit%8));break;case 13:bit=72/8;childmap[bit/8]&=~(128>>(bit%8));break;case 14:bit=64/8;map[bit/8]|=128>>(bit%8);break;case 15:bit=64/8;childmap[bit/8]|=128>>(bit%8);break;}
 before=allocations;spcheck(!native64typedmodule(&spec,0,&owner),"malformed spawn metadata refused");spcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"spawn refusal before allocation");}puts("PASS spawn metadata: 16 operation/call-owner/void-callee/argument-map refusals before allocation");}
static void spconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;for(jit=0;jit<2;jit++){base=allocations;spcheck(native64typedmodule(&full_spec,jit,&owner),"spawn constructor baseline");n=allocations-base;spcheck(native64typedmoduleclear(&owner),"spawn constructor cleared");for(i=1;i<=n;i++){failat=allocations+i;spcheck(!native64typedmodule(&full_spec,jit,&owner),"spawn constructor failure");failat=0;spcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"spawn constructor rollback");}printf("PASS spawn constructor: jit=%d %d allocation failures with complete rollback\n",jit,n);}}
int main(void){uint fn,total=0;int jit,q,n,fault,second,nils,roundrobin;int quanta[]={1,3,1000};WORD numbers[]={-7,0,19};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tptr.free=freeheap;Tarray.free=freearray;require(dis64selectlaunch("dis64-calls"));
 SPAWN_HANDLERS
 optab[IMOVP]=actual_movp;optab[IMOVM]=actual_movm;optab[IINDL]=actual_indl;optab[INEWZ]=spnewz;optab[ICONSMP]=spconsmp;optab[IADDC]=actual_addc;optab[ITAIL]=actual_tail;optab[IHEADP]=actual_headp;optab[ISPAWN]=actual_spawn;optab[IRAISE]=actual_iraise;spmetadata();sprecordmetadata();spconstruction();for(roundrobin=0;roundrobin<2;roundrobin++)for(fn=0;fn<nelem(spawn_functions);fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(n=0;n<3;n++)for(nils=0;nils<2;nils++)for(fault=0;fault<(fn==6?6:fn==5?5:4);fault++)for(second=0;second<=(fault&&(fn==1||fn==2));second++){sprun(fn,jit,quanta[q],numbers[n],fault,second,nils,roundrobin);total++;}printf("PASS local spawn: %u C/JIT runs, FIFO/round-robin child/grandchild publication and retirement, pending root transfer, parent exit, nil/temporary args, list tails and fresh mixed records, isolated child errors and allocation failures\n",total);return 0;}
