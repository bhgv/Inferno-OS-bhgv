#include "full.h"
#include "ref-native.h"
static void brcheck(int ok,const char*why){if(!ok){fprintf(stderr,"Field reference predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);brcheck(0,"unexpected LOAD");return H;}
static Module*brroot(int grow)
{
 Module*m;Inst*p;uchar map[]={0,0xe0};uint pc=0,i;m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("reference-caller");m->path=strdup("reference-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);
 p=m->prog=mallocz(5*sizeof(Inst),1);m->entry=p;m->cstep=native64typedlea8step;p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=80;p[pc++].d.ind=40;
 for(i=0;i<2;i++){p[pc].op=IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64+8*i;p[pc].d.i.f=40;p[pc++].d.i.s=64+8*i;}
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=80;p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;return m;
}
static void brcase(uint fn,int jit,int quantum,int profile)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};Type*ot,*vt;const uint*field=branch_fields[fn/4];void*object=H,*a,*b,*child=H,*other=H,*saved;Prog*p;uchar*root,*snapshot=nil;WORD equal,unequal,seen,guard=0x31592653,check;uint i,ticks=0,retired=0;int done;
 brcheck(native64typedmodule(&full_spec,jit,&owner),"owning field reference module");
 ot=dtype(freeheap,branch_types[field[0]].bytes,(uchar*)branch_types[field[0]].map,branch_types[field[0]].np);vt=dtype(freeheap,branch_types[field[1]].bytes,(uchar*)branch_types[field[1]].map,branch_types[field[1]].np);require(ot&&vt);
 a=H2D(void*,heapz(vt));b=H2D(void*,heapz(vt));ptradd(D2H(a));ptradd(D2H(b));
 if(profile!=5){object=H2D(void*,heapz(ot));ptradd(D2H(object));if(profile>=2)child=a;other=profile==1||profile==4?b:profile==3?a:H;if(child!=H)D2H(child)->ref++;memmove((uchar*)object+field[2],&child,8);snapshot=mallocz(ot->size,0);require(snapshot!=nil);memmove(snapshot,object,ot->size);}
 imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;memmove(ml->MP+branch_globals[3],&guard,4);
 m=brroot(quantum==1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+80,&ml,8);if(object!=H)D2H(object)->ref++;memmove(root+64,&object,8);if(other!=H)D2H(other)->ref++;memmove(root+72,&other,8);memset(root+88,0x5a,40);
 while(currun()!=nil){brcheck(++ticks<10000,"bounded reference scheduler");p->quanta=quantum;done=kprotect(executeprocess,p);if(done)continue;if(profile!=5&&worker.env->errstr[0])fprintf(stderr,"reference fn=%u jit=%d q=%d profile=%d error=%s\n",fn,jit,quantum,profile,worker.env->errstr);brcheck(!strcmp(worker.env->errstr,profile==5?exNilref:""),"reference branch exact completion/error");for(i=88;i<128;i++)brcheck(root[i]==0x5a,"reference caller guard");observed=p;progexit();retired++;}
 brcheck(retired==1&&nprog()==0,"reference process retired");emptyqueues();memmove(&equal,ml->MP+branch_globals[0],4);memmove(&unequal,ml->MP+branch_globals[1],4);memmove(&seen,ml->MP+branch_globals[2],4);memmove(&check,ml->MP+branch_globals[3],4);
 brcheck(equal==(profile==5?0:child==other?11:22)&&unequal==(profile==5?0:child!=other?33:44)&&seen==(profile==5?0:1)&&check==guard,"exact reference identity branches and Word guard");
 memmove(&saved,ml->MP+branch_holdings[fn/4][0],8);brcheck(saved==object,"stored owner identity");memmove(&saved,ml->MP+branch_holdings[fn/4][1],8);brcheck(saved==other,"stored peer identity");
 if(object!=H)brcheck(!memcmp(object,snapshot,ot->size)&&D2H(object)->ref==2,"owner fields and refs preserved");brcheck(D2H(a)->ref==1+(child==a)+(other==a)&&D2H(b)->ref==1+(other==b),"field/peer roots retained exactly");
 require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);brcheck(native64typedmoduleclear(&owner),"reference module retired");if(object!=H){brcheck(D2H(object)->ref==1,"owner true ref retired");ptrdel(D2H(object));destroy(object);}brcheck(D2H(a)->ref==1&&D2H(b)->ref==1,"peer true refs retired");ptrdel(D2H(a));destroy(a);ptrdel(D2H(b));destroy(b);free(snapshot);brcheck(ot->ref==1&&vt->ref==1,"canonical Types retired");freetype(ot);freetype(vt);brcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1&&Tstring.ref==1&&Tarray.ref==1,"complete reference module/frames/roots retirement");brcheck(rootpgrp.r.ref==1&&rootfgrp.r.ref==1&&rootegrp.r.ref==1,"reference environment refs retired");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void brmetadata(void)
{
 Native64ModuleSpec spec;Native64ModuleFunction f[76];Native64TypedStep code[64],*s;Native64ModuleOwner owner={0};REG saved=R;uchar fmap[64],dmap[64],*map;uint fn,pc,k,bit,mode,total=0;int before;
 for(fn=0;fn<76;fn++)for(pc=0;pc<full_functions[fn].count;pc++)if(full_functions[fn].code[pc].returned==207)for(k=0;k<13;k++){
  spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code)&&f[fn].mapbytes<=sizeof(fmap)&&spec.mapbytes<=sizeof(dmap));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;s=code+pc;
  switch(k){case 0:s->op=IBEQW;break;case 1:s->mode=SRC(USRC(s->mode)&~AIND)|(s->mode&ARM)|DST(AIMM);break;case 2:s->m+=4;break;case 3:s->m=(s->m&0xff00ffffL)|(255L<<16);break;case 4:s->m&=0xffffffL;break;case 5:s->s+=4;break;case 6:s->d=f[fn].count;break;case 7:s->d=-1;break;case 8:bit=s->s/8;map=(USRC(s->mode)&~AIND)==AFP?fmap:dmap;map[bit/8]&=~(128>>(bit%8));break;case 9:bit=(s->m&65535)/8;mode=(s->mode&ARM)==AXINF?AFP:AMP;map=mode==AFP?fmap:dmap;map[bit/8]&=~(128>>(bit%8));break;case 10:s->mode&=~ARM;break;case 11:s->m|=0x10000000L;break;case 12:s->returned=208;break;}
  before=allocations;brcheck(!native64typedmodule(&spec,0,&owner),"malformed field reference metadata refused");brcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"reference refusal before allocation");total++;
 }
 printf("PASS reference metadata: %u class/field/mode/extent/target/two-root refusals before allocation\n",total);
}
static void brconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;brcheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;brcheck(native64typedmoduleclear(&owner),"constructor cleared");for(i=1;i<=n;i++){failat=allocations+i;brcheck(!native64typedmodule(&full_spec,jit,&owner),"constructor failure");failat=0;brcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor rollback");}printf("PASS reference constructor: jit=%d %d allocation failures with complete rollback\n",jit,n);}
}
/* Register-bit sentinels are deliberately raw frame cells, not heap owners. */
static void brwidth(int jit,int unequal)
{
 Module*m;Inst*p;uchar map[]={0,0};Native64TypedStep code[5]={{0}};Prog*pr;uchar*root;uvlong data[3]={0,0x1111111176543210ULL,0},other=0x2222222276543210ULL;void*address=data;WORD answer;int done;uint ticks=0;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=4096;m->name=strdup("reference-width");m->path=strdup("reference-width");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);p=m->prog=mallocz(5*sizeof(Inst),1);m->entry=p;m->nprog=5;m->cstep=native64typedlea8step;
 code[0]=(Native64TypedStep){unequal?IBNEL:IBEQL,SRC(AFP|AIND)|AXINF|DST(AIMM),64,(1L<<24)|(8L<<16)|72,3,207};
 code[1]=(Native64TypedStep){IMOVW,SRC(AIMM)|DST(AFP),22,0,88,0};code[2]=(Native64TypedStep){IJMP,SRC(AXXX)|DST(AIMM),0,0,4,0};code[3]=(Native64TypedStep){IMOVW,SRC(AIMM)|DST(AFP),11,0,88,0};code[4]=(Native64TypedStep){IEXIT,SRC(AXXX)|DST(AXXX),0,0,0,0};
 require(native64typedcode(p,5,code,5,128,0));if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));pr=started;rootenvironment(pr);root=pr->R.FP;memmove(root+64,&address,8);memmove(root+72,&other,8);
 while(currun()!=nil){brcheck(++ticks<100,"bounded full-width scheduler");pr->quanta=1;done=kprotect(executeprocess,pr);if(done)continue;brcheck(!worker.env->errstr[0],"full-width execution without error");memmove(&answer,root+88,4);brcheck(answer==(unequal?11:22),"full 64-bit reference comparison");observed=pr;progexit();}
 emptyqueues();require(m->ref==1);unload(m);brcheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"full-width raw fixture retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
int main(void)
{
 uint fn,total=0;int jit,q,profile;int quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;require(dis64selectlaunch("dis64-calls"));
 BRANCH_HANDLERS
 optab[IMOVP]=actual_movp;brmetadata();brconstruction();for(jit=0;jit<2;jit++){brwidth(jit,0);brwidth(jit,1);}
 for(fn=0;fn<76;fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(profile=0;profile<6;profile++){brcase(fn,jit,quanta[q],profile);total++;}
 printf("PASS field reference identity: %u C/JIT runs, 19 canonical fields, four owner/peer FP/MP combinations, equality/inequality, nil/same/different-content-identical objects, nil-owner unwind, four high-bit width sentinels and complete retirement\n",total);return 0;
}
