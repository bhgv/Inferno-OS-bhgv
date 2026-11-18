typedef struct EnvWord EnvWord;struct EnvWord{uint bytes;const uchar*text;};
typedef struct EnvCase EnvCase;struct EnvCase{uint bytes;const uchar*text;uint count;const EnvWord*words;};
#include "env.h"
static const EnvCase*envcase;static int envmode,envcreated,envclosed,envreads;static Type*envfdtype;
static void envcheck(int ok,char*why){if(!ok){fprintf(stderr,"real getenv predicate: %s\n",why);exit(10);}}
static void envfdclear(Heap*h,int swept){envclosed++;freeheap(h,swept);}
static Sys_FD*envmkfd(int n){Sys_FD*f=H2D(Sys_FD*,heapz(envfdtype));memmove(f,&n,4);envcreated++;return f;}
static int envopen(char*path,int mode){envcheck(!strcmp(path,"#e/v") && mode==0,"actual concatenated open path");return envmode==2?-1:23;}
static Dir*envdir(int fd){Dir*d;envcheck(fd==23,"actual fstat FD");if(envmode==3)return nil;d=mallocz(sizeof(*d),1);if(!d)return nil;d->name="name";d->uid="uid";d->gid="gid";d->muid="muid";d->length=envcase->bytes+(envmode==1?7:0);return d;}
static long envread(int fd,void*buf,long n){envcheck(fd==23 && n==(long)envcase->bytes+(envmode==1?7:0),"computed length in actual Sys.read frame");envreads++;if(envmode==4)return -1;if(envmode==5)return 0;memmove(buf,envcase->text,envcase->bytes);return envcase->bytes;}
static Module*envbuiltin(void){Module*m=mallocz(sizeof(*m),1);uint i;void(*entries[])(void*)={Sys_fstat,Sys_open,Sys_read};require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->name=strdup("$Sys");m->path=strdup("$Sys");require(m->name&&m->path);m->ntype=4;m->type=mallocz(4*sizeof(Type*),1);require(m->type!=nil);m->ext=mallocz(4*sizeof(Link),1);require(m->ext!=nil);
 for(i=0;i<3;i++){m->type[i]=dtype(freeheap,env_imports[i].bytes,(uchar*)env_imports[i].map,2);require(m->type[i]!=nil);m->ext[i].name=strdup(env_imports[i].name);require(m->ext[i].name!=nil);memmove(&m->ext[i].sig,&env_imports[i].signature,4);m->ext[i].frame=m->type[i];m->ext[i].u.runt=entries[i];}
 m->type[3]=envfdtype=dtype(envfdclear,4,nil,0);require(envfdtype!=nil);return m;}
static Modlink*envinstance(Module*m,const Import*req){Modlink*l;m->ref++;l=linkmod(m,(Import*)req,1);require(l!=H);return l;}
static Module*envlaunch(int jit,int grow){Module*m=mallocz(sizeof(*m),1);Inst*p;uchar map[]={0,0xe0};require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=grow?0:4096;m->cstep=native64typedlea8step;m->ntype=1;m->type=mallocz(sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,128,map,2);m->entryt=m->type[0];require(m->entryt!=nil);p=m->prog=mallocz(5*sizeof(*p),1);require(p!=nil);m->entry=p;m->nprog=5;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=64;p[0].d.ind=40;
 p[1].op=IMOVP;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=72;p[1].d.i.f=40;p[1].d.i.s=64;
 p[2].op=ILEA;p[2].reg=24;p[2].add=SRC(AFP)|DST(AFP|AIND);p[2].s.ind=80;p[2].d.i.f=40;p[2].d.i.s=32;
 p[3].op=IMCALL;p[3].add=SRC(AFP)|AXIMM|DST(AFP);p[3].s.ind=40;p[3].d.ind=64;p[4].op=IEXIT;p[4].add=SRC(AXXX)|DST(AXXX);if(jit)require(compile(m,5,nil));return m;}
static void envwords(List*l){uint i,j;String*s,*expected;for(i=0;i<envcase->count;i++){envcheck(l!=H,"result list length");memmove(&s,l->data,8);expected=c2string((char*)envcase->words[i].text,envcase->words[i].bytes);envcheck(s==H?envcase->words[i].bytes==0:s->len==expected->len,"word length");if(s!=H)for(j=0;j<(uint)(s->len<0?-s->len:s->len);j++)envcheck((s->len<0?s->Srune[j]:s->Sascii[j])==(expected->len<0?expected->Srune[j]:expected->Sascii[j]),"word contents");destroy(expected);l=l->tail;}envcheck(l==H,"complete list tail");}
static int envrun(int jit,int grow,int q,int c,int mode,int failure){Native64ModuleOwner owner={0};Module*b,*outer;Modlink*sys,*client;Import request[2]={{0}};Prog*process;uchar*root;String*v;List*result;int before,used,steps;uint done,i;
 envcase=env_cases+c;envmode=mode;envcreated=envclosed=envreads=0;require(native64typedmodule(&env_spec,jit,&owner));b=envbuiltin();b->ref++;sys=linkmod(b,owner.module->ldt[0],3);require(sys!=H);memmove(owner.module->origmp+env_sys_offset,&sys,8);
 request[0].name="getenv";memmove(&request[0].sig,&env_functions[0].signature,4);client=envinstance(owner.module,request);v=c2string("v",1);outer=envlaunch(jit,grow);starting=outer;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;memmove(root+64,&client,8);memmove(root+72,&v,8);memset(root+88,0xa5,40);process->quanta=q;
 before=allocations;if(failure)failat=allocations+failure;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}used=allocations-before;failat=0;envcheck(steps<10000,"bounded execution");
 if(worker.env->errstr[0]){if(!failure || (strcmp(worker.env->errstr,exHeap)&&strcmp(worker.env->errstr,exNomem)))fprintf(stderr,"env jit=%d grow=%d q=%d c=%d mode=%d fail=%d error=%s\n",jit,grow,q,c,mode,failure,worker.env->errstr);envcheck(failure && (!strcmp(worker.env->errstr,exHeap)||!strcmp(worker.env->errstr,exNomem)),"allocation error family");}
 else{memmove(&result,root+80,8);if(mode>=2 || failure)envcheck(result==H,"failure path returns nil");else envwords(result);}
 for(i=88;i<128;i++)envcheck(root[i]==0xa5,"caller guards");observed=process;progexit();emptyqueues();require(owner.module->ref==1);require(native64typedmoduleclear(&owner));require(b->ref==1);unload(b);envcheck(envcreated==envclosed && !live && !nlinkpins && !modules && !worker.nerr && Tstring.ref==1 && Tarray.ref==1 && Tlist.ref==1 && Tmodlink.ref==1,"complete FD/array/list/String/module retirement");memset(nativeobservations,0,sizeof(nativeobservations));return used;}
static void envpreflight(void){Native64ModuleSpec spec=env_spec;Native64ModuleFunction functions[3];Native64TypedStep code[128];Native64ModuleSite sites[8];Native64ModuleImport request[3];Native64ModuleTable table;Native64ModuleOwner owner={0};uchar map[64],rmap[2];REG saved=R;uint j,len=~0U,convert=~0U,local=~0U,bit;int i,before;
 for(j=0;j<env_functions[0].count;j++){if(env_functions[0].code[j].op==ILENA && env_functions[0].code[j].returned==64)len=j;if(env_functions[0].code[j].op==ICVTAC && env_functions[0].code[j].returned==64)convert=j;}
 for(j=0;j<env_functions[0].nsites;j++)if(!env_functions[0].sites[j].external)local=j;
 require(len!=~0U && convert!=~0U && local!=~0U);
 for(i=0;i<12;i++){
 memmove(functions,env_functions,sizeof(functions));spec.functions=functions;memmove(code,functions[0].code,functions[0].count*sizeof(*code));functions[0].code=code;memmove(sites,functions[0].sites,functions[0].nsites*sizeof(*sites));functions[0].sites=sites;memmove(map,functions[0].map,functions[0].mapbytes);functions[0].map=map;
 memmove(request,env_imports,sizeof(request));memmove(rmap,env_import_map_2,2);request[2].map=rmap;table=(Native64ModuleTable){request,3};spec.tables=&table;
 if(i==0)code[len].s=code[len+1].s;
 if(i==1)code[convert].s=code[len+1].s;
 if(i==2)code[len].d=72;
 if(i==3){for(bit=64;bit+8<=functions[1].bytes;bit+=8)if(!(functions[1].map[bit/64]&(0x80>>(bit/8%8))))break;code[convert].d=bit;}
 if(i==4)code[len].returned=0;
 if(i==5)code[convert].returned=0;
 if(i==6)code[sites[local].first+1].d=code[len+1].s;
 if(i==7){code[sites[local].first+2].s=code[len].s;code[sites[local].first+2].mode=(code[sites[local].first+2].mode&~56)|SRC(AFP);}
 if(i==8)sites[local].external=1;
 if(i==9)rmap[1]|=0x20;
 if(i==10)request[2].bytes=80;
 if(i==11)code[convert].s=0;
 before=allocations;if(native64typedmodule(&spec,0,&owner)){fprintf(stderr,"getenv metadata refusal removed: case=%d\n",i);exit(10);}
 if(before!=allocations)fprintf(stderr,"getenv preflight allocated at %d\n",i);envcheck(before==allocations && !live && !owner.module && !memcmp(&saved,&R,sizeof(R)),"metadata refused before allocation");
 }
 puts("PASS getenv metadata: twelve computed-argument/root/prelude/frame refusals before allocation");
}
static void envpublication(Module*m){uint i,j,base=0;const Native64ModuleFunction*f;for(i=0;i<3;i++){f=env_functions+i;for(j=0;j<f->count;j++)if(!f->code[j].returned && (f->code[j].op==ISLICEA || (f->code[j].op==IMOVP && UDST(f->code[j].mode)==AFP))){envcheck(m->prog[base+j].d.ind==f->code[j].d,"direct call prelude publication");}base+=f->count;}}
static void envconstruction(void){Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,before;
 for(jit=0;jit<2;jit++){before=allocations;require(native64typedmodule(&env_spec,jit,&owner));n=allocations-before;if(!jit)envpublication(owner.module);require(native64typedmoduleclear(&owner));for(i=1;i<=n;i++){failat=allocations+i;require(!native64typedmodule(&env_spec,jit,&owner));failat=0;envcheck(!live&&!nlinkpins&&!owner.module&&!owner.rooted&&!memcmp(&saved,&R,sizeof(R)),"complete constructor rollback");}printf("PASS getenv constructor: jit=%d %d allocation failures, unchanged REG and complete rollback\n",jit,n);}
}
int main(int argc,char**argv){int jit,g,q,c,m,f,n,total=0,faults=0;int quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;Tarray.size=sizeof(Array);require(dis64selectlaunch("dis64-calls"));
 envpreflight();envconstruction();if(argc==2)return 0;USED(argv);
 ENV_HANDLERS
 optab[IMOVP]=actual_movp;optab[ICVTLW]=actual_cvtlw;optab[ICONSP]=actual_consp;optab[INEWAZ]=actual_newaz;optab[ISLICEA]=actual_slicea;optab[ICVTAC]=actual_cvtac;optab[ILENA]=actual_lena;optab[IADDC]=actual_addc;optab[IINDC]=actual_indc;optab[ISLICEC]=actual_slicec;optab[ILENC]=actual_lenc;
 for(jit=0;jit<2;jit++)for(g=0;g<2;g++)for(q=0;q<3;q++)for(c=0;c<(int)nelem(env_cases);c++)for(m=0;m<6;m++){n=envrun(jit,g,quanta[q],c,m,0);total++;if(!q)for(f=1;f<=n;f++){envrun(jit,g,quanta[q],c,m,f);faults++;}}
 printf("PASS unchanged real getenv/unquoted: %d C/JIT inputs, %d execution allocation failures, real Sys open/fstat/read bodies, short reads, Unicode/NUL/quotes and complete retirement\n",total,faults);return 0;}
