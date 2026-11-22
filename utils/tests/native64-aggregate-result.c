#include "full.h"
static void agcheck(int ok,const char*why){if(!ok){fprintf(stderr,"Aggregate predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);error(exModule);return H;}
static Module*agroot(void){Module*m;Inst*p;uchar map[]={0,0x40};m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=4096;m->name=strdup("aggregate-caller");m->path=strdup("aggregate-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);p=m->prog=mallocz(4*sizeof(Inst),1);require(p!=nil);m->entry=p;m->nprog=4;m->cstep=native64typedlea8step;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=72;p[0].d.ind=40;p[1].op=IMOVW;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=64;p[1].d.i.f=40;p[1].d.i.s=64;p[2].op=IMCALL;p[2].add=SRC(AFP)|AXIMM|DST(AFP);p[2].s.ind=40;p[2].d.ind=72;p[3].op=IEXIT;p[3].add=SRC(AXXX)|DST(AXXX);return m;}
static void
agseednodes(Modlink *ml)
{
 uchar map[]={0xc0};
 Type *type=dtype(freeheap,16,map,1);
 List *nodes=H;
 const char *words[]={"submenu","parent"};
 int i;
 require(type!=nil);
 for(i=0;i<2;i++){
  void **entry=H2D(void**,heapz(type));
  entry[1]=c2string((char*)words[i],strlen(words[i]));
  R.s=&entry;
  R.d=&nodes;
  actual_consp();
  destroy(entry);
 }
 freetype(type);
 memmove(ml->MP+aggregate_globals[21],&nodes,8);
}
static List*
agprovider(Modlink *link, List *tail)
{
 R.s=&link;
 R.d=&tail;
 actual_consp();
 return tail;
}

static void
agseedbuiltins(Modlink *ml, Modlink *marker)
{
 uchar pairmap[]={0xc0},builtinmap[]={0x80};
 Import empty={0};
 Modlink *other=linkmod(agroot(),&empty,1);
 Type *pair=dtype(freeheap,16,pairmap,1),*builtins=dtype(freeheap,16,builtinmap,1);
 Array *table=H2D(Array*,heaparray(pair,3));
 uchar *record=H2D(uchar*,heapz(builtins));
 void **slots=(void**)table->data;
 WORD used=3;
 const char *names[]={"a","beta","c"};
 int i;
 for(i=0;i<3;i++)slots[2*i]=c2string((char*)names[i],strlen(names[i]));
 slots[1]=agprovider(marker,H);
 slots[3]=agprovider(marker,agprovider(other,H));
 slots[5]=agprovider(other,H);
 destroy(other);
 memmove(record,&table,8);
 memmove(record+8,&used,4);
 memmove(ml->MP+aggregate_globals[43],&record,8);
 freetype(pair);
 freetype(builtins);
}

static void
agcheckbuiltins(Modlink *ml, WORD n)
{
 uchar *record;
 Array *table;
 void **slots;
 List *providers;
 Modlink *marker;
 WORD used,offset;
 const WORD expected[]={-1,-3,-4,-1};
 unsigned int i;
 memmove(&marker,ml->MP+aggregate_globals[40],8);
 memmove(&providers,ml->MP+aggregate_globals[44],8);
 agcheck(providers!=H&&providers->tail!=H&&providers->tail->tail==H&&
         *(void**)providers->data==marker&&*(void**)providers->tail->data!=marker,
         "binary search returns the provider chain");
 memmove(&providers,ml->MP+aggregate_globals[45],8);
 agcheck(providers==H,"missing builtin returns nil");
 for(i=0;i<nelem(expected);i++){
  memmove(&offset,ml->MP+aggregate_globals[46+i],4);
  agcheck(offset==expected[i],"binary search preserves insertion positions");
 }
 memmove(&record,ml->MP+aggregate_globals[43],8);
 memmove(&table,record,8);
 memmove(&used,record+8,4);
 slots=(void**)table->data;
 agcheck(used==(n<0?3:2)&&table->len==3,"builtin table count");
 if(n<0)return;
 agcheck(!strcmp(string2c(slots[0]),"beta")&&!strcmp(string2c(slots[2]),"c"),
         "builtin compaction preserves sorted order");
 providers=slots[1];
 agcheck(providers!=H&&providers->tail==H&&*(void**)providers->data!=H&&
         *(void**)providers->data==*(void**)((List*)slots[3])->data,
         "builtin removal keeps the remaining provider");
 agcheck(slots[4]==H&&slots[5]==H,"builtin removal clears both trailing roots");
}

static void
aglookup(Modlink *ml)
{
 const char *want[]={"child","parent","second"};
 List *value;
 void **node;
 unsigned int i;
 for(i=0;i<nelem(want);i++){
  memmove(&value,ml->MP+aggregate_globals[36+i],8);
  agcheck(value!=H&&value->tail==H,"lookup yields one shell word");
  memmove(&node,value->data,8);
  agcheck(node!=H&&node[1]!=H&&!strcmp(string2c(node[1]),want[i]),"lookup follows scope and positional rules");
 }
 memmove(&value,ml->MP+aggregate_globals[39],8);
 agcheck(value==H,"unknown name returns nil");
}

#if defined(DIS64_TEST_ARM64) || defined(DIS64_TEST_RISCV64)
enum { CatchJit=1 };
#else
enum { CatchJit=0 };
#endif
static void
agshwords(Modlink *ml)
{
 const char *words[]={"first","second"};
 List *list;
 void **entry;
 unsigned int i;
 memmove(&list,ml->MP+aggregate_globals[24],8);
 for(i=0;i<nelem(words);i++){
  agcheck(list!=H,"shell argument is present");
  memmove(&entry,list->data,8);
  agcheck(entry!=H&&entry[0]==H&&entry[1]!=H&&!strcmp(string2c(entry[1]),words[i]),
   "shell argument order and nil command field");
  list=list->tail;
 }
 agcheck(list==H,"shell argument list is terminated");
 memmove(&list,ml->MP+aggregate_globals[25],8);
 agcheck(list==H,"empty shell argument list stays nil");
}
static void
agshcontext(Modlink *ml, WORD n)
{
 void **context,**environment,**builtins,**local;
 Array *vars;
 void *cell;
 List *entries;
 void **entry,**old;
 unsigned int i;
 WORD value;
 memmove(&context,ml->MP+aggregate_globals[26],8);
 agcheck(context!=H&&D2H(context)->t->size==32,"shell Context layout");
 environment=context[0];
 agcheck(environment!=H&&context[1]==H&&context[2]==H&&context[3]==H,"shell Context roots");
 builtins=environment[1];
 local=environment[3];
 agcheck(builtins!=H&&builtins==environment[0]&&local!=H&&environment[2]==H,"shell Environment roots");
 memmove(&value,builtins+1,4);
 agcheck(builtins[0]==H&&value==n,"shell Builtins Word and nil array");
 memmove(&value,local+2,4);
 agcheck(local[0]!=H&&local[1]==H&&value==n,"shell Localenv Word and mapped array");
 vars=local[0];
 agcheck(vars->len==7&&vars->t->size==8&&vars->t->map[0]==0x80,"shell hash bucket array descriptor");
 for(i=0;i<7;i++){
  memmove(&cell,vars->data+8*i,8);
  if(i!=3)agcheck(cell==H,"unused shell hash bucket stays nil");
 }
 memmove(&entries,vars->data+24,8);
 agcheck(entries!=H&&entries->tail!=H&&entries->tail->tail==H,"two entries share one bucket");
 memmove(&entry,entries->data,8);
 memmove(&old,entries->tail->data,8);
 agcheck(entry!=H&&old!=H&&!strcmp(string2c(entry[0]),"new")&&
  !strcmp(string2c(old[0]),"old"),"hashadd prepends and preserves the tail");
 memmove(&entry,ml->MP+aggregate_globals[34],8);
 agcheck(entry==old,"hashfind traverses the bucket");
 memmove(&entry,ml->MP+aggregate_globals[35],8);
 agcheck(entry==H,"hashfind on an empty bucket returns nil");
 memmove(&cell,ml->MP+aggregate_globals[21],8);
 agcheck(cell!=H,"fixture supplies a live list for array retirement");
 D2H(cell)->ref++;
 Setmark(D2H(cell));
 memmove(vars->data,&cell,8);
 memmove(&value,ml->MP+aggregate_globals[27],4);
 agcheck(value==0,"shell field reads");
}
static void agcase(int jit,int quantum,WORD n){Native64ModuleOwner owner={0};Module*m;Modlink*ml,*marker,*matched;Import imports[2]={{0}};Prog*p;uchar*root;String*s;void*ref;WORD got;uint ticks=0,i,steps;int done;
 Native64ModuleSpec spec=full_spec;
 if(jit&&!CatchJit){spec.catches=nil;spec.ncatches=0;}
 agcheck(native64typedmodule(&spec,jit,&owner),"source module");imports[0].name="run";memmove(&imports[0].sig,&full_functions[0].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;agseednodes(ml);{Import empty={0};marker=linkmod(agroot(),&empty,1);agcheck(marker!=nil&&marker!=H,"fixture builtin link");memmove(ml->MP+aggregate_globals[40],&marker,8);agseedbuiltins(ml,marker);}m=agroot();if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));rootenvironment(started);root=started->R.FP;memmove(root+64,&n,4);memmove(root+72,&ml,8);memset(root+80,0x5a,48);
 while((p=currun())!=nil){agcheck(++ticks<10000,"bounded scheduler");p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;}if(done)continue;if(worker.env->errstr[0]&&handler(worker.env->errstr)){worker.env->errstr[0]=0;continue;}if(worker.env->errstr[0])fprintf(stderr,"aggregate error=%s\n",worker.env->errstr);agcheck(steps<10000&&!worker.env->errstr[0],"exact completion");for(i=80;i<128;i++)agcheck(root[i]==0x5a,"caller guards");observed=p;progexit();}
 emptyqueues();for(i=1;i<7;i++){memmove(&got,ml->MP+aggregate_globals[i],4);agcheck(got==((i==1||i==2)?n:i==3?2:i==4?3:i==5?4:5),"all raw result leaves");}memmove(&s,ml->MP+aggregate_globals[0],8);agcheck(!strcmp(string2c(s),n<0?"negative":"positive"),"mixed/triple String result");memmove(&s,ml->MP+aggregate_globals[9],8);agcheck(!strcmp(string2c(s),"end"),"last mapped root");for(i=7;i<9;i++){memmove(&ref,ml->MP+aggregate_globals[i],8);agcheck(ref==H,"nil reference result");}for(i=0;i<4;i++){WORD expected[]={(-2147483647-1),2147483647,-1,0};if(n>=0){WORD swapped[]={2147483647,(-2147483647-1),0,-1};memmove(expected,swapped,sizeof(expected));}memmove(&got,ml->MP+aggregate_globals[10]+4*i,4);agcheck(got==expected[i],"original Wmlib s2r rectangle");}memmove(&got,ml->MP+aggregate_globals[11],4);agcheck(got==28,"original Wmlib final position");memmove(&s,ml->MP+aggregate_globals[12],8);agcheck(!strcmp(string2c(s),"a'b"),"original Wmlib doubled quote");memmove(&got,ml->MP+aggregate_globals[13],4);agcheck(got==7,"original Wmlib quote position");for(i=14;i<17;i++){List*l;memmove(&l,ml->MP+aggregate_globals[i],8);if(i==14||(i==16&&n<0))agcheck(l==H,"select nil results");else if(i==16){WORD expected[]={0,1,1,3};agcheck(l!=H&&l->tail==H&&l->t==nil&&!memcmp(l->data,expected,16),"original Winplace selected rectangle");}else{WORD expected[]={1,2,1,3};agcheck(l!=H&&l->t==nil&&!memcmp(l->data,expected,16),"original Winplace select rectangle");l=l->tail;if(n<0)agcheck(l==H,"select single output");else{WORD rest[]={0,2,1,3};agcheck(l!=H&&l->tail==H&&l->t==nil&&!memcmp(l->data,rest,16),"original Winplace split rectangle");}}}for(i=0;i<2;i++){memmove(&got,ml->MP+aggregate_globals[17]+4*i,4);agcheck(got==((n<0?i==0:i==1)?n:1),"partial Point result");}{Array*a;memmove(&a,ml->MP+aggregate_globals[18],8);agcheck(a!=H&&a->len==12,"original Wmlib bplong buffer");for(i=0;i<12;i++)agcheck(a->data[i]==(i>=4&&i<8?((UWORD)n>>(8*(i-4)))&255:0),"original Wmlib bplong bytes and guards");}for(i=19;i<21;i++){memmove(&ref,ml->MP+aggregate_globals[i],8);agcheck(ref==H,"Tk result roots");}memmove(&s,ml->MP+aggregate_globals[22],8);agcheck(!strcmp(string2c(s),"submenu"),"Sh Listnode allocation, cons, tail, head and word");memmove(&got,ml->MP+aggregate_globals[23],4);agcheck(got==(n<0?17:0),"compiler-generated string catch");agshwords(ml);agshcontext(ml,n);aglookup(ml);agcheckbuiltins(ml,n);memmove(&matched,ml->MP+aggregate_globals[41],8);agcheck(matched==(n<0?H:marker),"registry preserves the builtin owner");memmove(&matched,ml->MP+aggregate_globals[42],8);agcheck(matched==H,"nil module and empty registry stay nil");{const char *expected[]={"B","2","A","1"};int indices[]={28,30,31,32};for(i=0;i<4;i++){memmove(&s,ml->MP+aggregate_globals[indices[i]],8);agcheck(n<0?s==H:s!=H&&!strcmp(string2c(s),expected[i]),"environment string pair order");}}memmove(&got,ml->MP+aggregate_globals[29],4);agcheck(got==(n<0?0:2),"environment pair list traversal");{void **var;memmove(&var,ml->MP+aggregate_globals[33],8);if(n<0)agcheck(var==H,"empty environment adds no variable");else{agcheck(var!=H,"shell Var exists");memmove(&got,var+2,4);agcheck(var[1]==H&&got==1&&!strcmp(string2c(var[0]),"B"),"shell Var descriptor and fields");}}require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);agcheck(native64typedmoduleclear(&owner),"module retired");agcheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"complete ownership retirement");memset(nativeobservations,0,sizeof(nativeobservations));}
static void agmetadata(void){Native64ModuleSpec spec;Native64ModuleFunction f[nelem(full_functions)];Native64TypedStep code[256];uchar fmap[32],dmap[32];Native64ModuleOwner owner={0};REG saved=R;uint fn,pc,k,total=0;int before;
 for(fn=0;fn<nelem(full_functions);fn++)for(pc=0;pc<full_functions[fn].count;pc++)if(full_functions[fn].code[pc].returned==77)for(k=0;k<7;k++){
  spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;
  switch(k){case 0:code[pc].op=IEXIT;break;case 1:code[pc].m^=256;break;case 2:code[pc].d+=8;break;case 3:code[pc].m^=1;break;case 4:code[pc].mode^=SRC(AIND);break;case 5:if(USRC(code[pc].mode)==AIMM)continue;code[pc].s=USRC(code[pc].mode)==AFP?f[fn].bytes:spec.databytes;break;case 6:code[pc]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,pc+1,0};break;}
  before=allocations;agcheck(!native64typedmodule(&spec,0,&owner),"malformed aggregate or undefined path refused");agcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"refusal before allocation");total++;
 }printf("PASS aggregate metadata: %u refusals before allocation, including missing result writes\n",total);}
static void agbytemetadata(void){Native64ModuleSpec spec;Native64ModuleFunction f[nelem(full_functions)];Native64TypedStep code[64];Native64ModuleOwner owner={0};REG saved=R;uint fn,pc,k,total=0;int before;
 for(fn=0;fn<nelem(full_functions);fn++)for(pc=0;pc<full_functions[fn].count;pc++)if(full_functions[fn].code[pc].returned==157&&((full_functions[fn].code[pc].m>>8)&255))for(k=0;k<8;k++){
  spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;
  switch(k){case 0:code[pc].m^=256;break;case 1:code[pc].m+=256;break;case 2:code[pc+1].op=IMOVP;break;case 3:code[pc+1].d=48;break;case 4:code[pc+1].d=code[pc].s;break;case 5:code[pc+1].s=32;break;case 6:code[pc-1]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,pc+1,0};break;case 7:code[pc-1]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,pc+2,0};break;}
  before=allocations;agcheck(!native64typedmodule(&spec,0,&owner),"malformed byte borrow refused");agcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"byte borrow refusal before allocation");total++;
 }printf("PASS Wmlib byte borrow: %u invalid gaps, owner overwrites and interior branches refused before allocation\n",total);
}
static int
agcheckpairlayout(void *value)
{
 native64rawarraycheck(value,16,1);
 return 1;
}

static void
agpairmetadata(void)
{
 uchar good[]={0xc0},bad[]={0x80};
 Type *element;
 Array *array;
 Native64ModuleSpec spec=full_spec;
 Native64ModuleFunction f[nelem(full_functions)];
 Native64ModuleOwner owner={0};
 Native64TypedStep code[64];
 unsigned int fn,pc,k,total=0;
 int before;
 for(k=0;k<2;k++){
  element=dtype(freeheap,16,k?bad:good,1);
  array=H2D(Array*,heaparray(element,1));
  agcheck(kprotect(agcheckpairlayout,array)==!k,"runtime checks both element roots");
  worker.env->errstr[0]=0;
  destroy(array);
  freetype(element);
 }
 spec.functions=f;
 for(fn=0;fn<nelem(full_functions);fn++){
  const Native64ModuleFunction *original=full_functions+fn;
  for(pc=0;pc<original->count;pc++){
   if(original->code[pc].op!=IINDX||original->code[pc].returned!=255)continue;
   for(k=0;k<4;k++){
    memmove(f,full_functions,sizeof(f));
    require(original->count<=nelem(code)&&pc>0);
    memmove(code,original->code,original->count*sizeof(*code));
    f[fn].code=code;
    switch(k){
    case 0:code[pc].m=56;break;
    case 1:code[pc+1].returned=194;break;
    case 2:code[pc+1].m=1;break;
    case 3:code[pc-1]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,pc+1,0};break;
    }
    before=allocations;
    agcheck(!native64typedmodule(&spec,0,&owner)&&allocations==before,
            "malformed builtin table borrow refused before allocation");
    total++;
   }
  }
 }
 agcheck(total==20&&!live,"all five builtin table accesses checked");
 printf("PASS builtin table: %u invalid index/copy pairs refused, exact runtime element map\n",total);
}

static void agconstruction(void){Native64ModuleOwner owner={0};REG saved=R;Native64ModuleSpec spec;unsigned int jit;int base,n,i;for(jit=0;jit<2;jit++){spec=full_spec;if(jit&&!CatchJit){spec.catches=nil;spec.ncatches=0;}base=allocations;agcheck(native64typedmodule(&spec,jit,&owner),"constructor baseline");n=allocations-base;agcheck(native64typedmoduleclear(&owner),"constructor clear");for(i=1;i<=n;i++){failat=allocations+i;agcheck(!native64typedmodule(&spec,jit,&owner),"constructor failure");failat=0;agcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor rollback");}printf("PASS aggregate constructor jit=%d: %d allocation failures with rollback\n",jit,n);}}
static void
agcatch(void)
{
 static const uchar frame[]={0,0xc0},cleanup[]={0,0x40},badcleanup[]={0,0x20},data[]={0xc0};
 static const Native64TypedStep code[]={
  {IMOVP,SRC(AMP)|DST(AFP),0,0,72,0},
  {IRAISE,SRC(AMP)|DST(AXXX),0,0,0,0},
  {IEXIT,SRC(AXXX)|DST(AXXX),0,0,0,0},
  {IMOVP,SRC(AFP)|DST(AMP),64,0,8,0},
  {IEXIT,SRC(AXXX)|DST(AXXX),0,0,0,0},
 };
 static const Native64ModuleFunction run={
  .name="run",.code=code,.count=nelem(code),.bytes=80,.map=frame,.mapbytes=2,
 };
 static const Native64ModuleString message={0,10,(const uchar*)"fail:probe"};
 Native64ModuleCatch entry={0,1,2,3,64,"fail:*",cleanup,2};
 Native64ModuleSpec spec={
  .name="catch-fixture",.path="trusted/catch-fixture",.functions=&run,.nfunctions=1,
  .entry=0,.databytes=16,.datamap=data,.mapbytes=1,.string=c2string,
  .strings=&message,.nstrings=1,.catches=&entry,.ncatches=1,
 };
 Native64ModuleOwner owner={0};
 Prog *p;
 String *caught;
 int i,before,done,ticks=0,delivered=0;
 REG saved=R;
 entry.slot=56;
 before=allocations;
 agcheck(!native64typedmodule(&spec,0,&owner)&&allocations==before,"catch slot outside mapped roots");
 entry.slot=64;
 entry.target=0;
 agcheck(!native64typedmodule(&spec,0,&owner)&&allocations==before,"catch target in protected range");
 entry.target=3;
 entry.cleanup=badcleanup;
 agcheck(!native64typedmodule(&spec,0,&owner)&&allocations==before,"catch cleanup outside mapped roots");
 entry.cleanup=cleanup;
 agcheck(native64typedmodule(&spec,0,&owner),"string catch constructor");
 i=allocations-before;
 agcheck(native64typedmoduleclear(&owner),"string catch constructor clear");
 for(;i>0;i--){
  failat=allocations+i;
  agcheck(!native64typedmodule(&spec,0,&owner),"catch construction failure");
  failat=0;
  agcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"catch construction rollback");
 }
 agcheck(native64typedmodule(&spec,0,&owner),"string catch fixture module");
 owner.module->ref++;starting=owner.module;started=nil;
 require(kprotect(start,nil));rootenvironment(started);
 while((p=currun())!=nil){
  agcheck(++ticks<1000,"bounded catch scheduler");
  p->quanta=1;
  done=kprotect(executeprocess,p);
  if(done){
   memmove(&caught,p->R.M->MP+8,8);
   if(caught!=H){
    agcheck(!strcmp(string2c(caught),"fail:probe"),"mapped exception value");
    delivered=1;
   }
   continue;
  }
  if(worker.env->errstr[0]){
   agcheck(handler(worker.env->errstr),"caught fail prefix");
   memmove(&caught,R.FP+72,8);
   agcheck(caught==H,"catch clears the selected temporary");
   worker.env->errstr[0]=0;
   continue;
  }
  observed=p;progexit();
 }
 emptyqueues();
 agcheck(delivered,"catch branch executed");
 agcheck(native64typedmoduleclear(&owner)&&!live&&!modules&&!nlinkpins,"catch ownership retired");
 puts("PASS native64 string catch: dispatch, exception root, metadata refusal and rollback");
}
int main(void){uint jit,q,n,total=0;int quanta[]={1,3,1000};WORD words[]={(-2147483647-1),-1,0,2147483647};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;Tarray.size=sizeof(Array);require(dis64selectlaunch("dis64-calls"));
 AGGREGATE_HANDLERS
 optab[INEWAZ]=actual_newaz;optab[IINDB]=actual_indb;optab[IINDL]=actual_indl;optab[IINDX]=actual_indx;optab[ILENA]=actual_lena;optab[IBNEC]=actual_bnec;optab[IBLEC]=actual_blec;optab[IBGEC]=actual_bgec;optab[ICVTWB]=actual_cvtwb;optab[ISHRW]=actual_shrw;optab[ICONSM]=actual_consm;optab[IHEADM]=actual_headm;optab[IHEADP]=actual_headp;optab[ICONSP]=actual_consp;optab[ICONSMP]=actual_consmp;optab[IHEADMP]=actual_headmp;optab[INEWZ]=actual_newz;optab[ITAIL]=actual_tail;optab[ILENC]=actual_lenc;optab[IINDC]=actual_indc;optab[IINSC]=actual_insc;optab[ISLICEC]=actual_slicec;optab[ICVTCW]=actual_cvtcw;optab[IMOVP]=actual_movp;optab[IMOVM]=actual_movm;optab[IMOVMP]=actual_movmp;optab[IRAISE]=actual_iraise;agcatch();agmetadata();agbytemetadata();agpairmetadata();agconstruction();for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(n=0;n<nelem(words);n++){if(jit&&!CatchJit&&words[n]<0)continue;agcase(jit,quanta[q],words[n]);total++;}printf("PASS aggregate results: %u C/JIT processes, mixed/nested/mapped results, extreme Words, caller guards and complete retirement\n",total);return 0;}
