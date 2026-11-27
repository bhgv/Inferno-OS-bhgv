#include "full.h"
static void ck(int ok,const char*why){if(!ok){fprintf(stderr,"Wmsrv module predicate: %s\n",why);exit(10);}}
int Sys_sprint64(const Native64SysFormatRequest*r){return productionSys_sprint64(r);}
static uint wmformatcalls;
int Sys_aprint64(const Native64SysFormatRequest*r){wmformatcalls++;return productionSys_aprint64(r);}
int Sys_print64(const Native64SysWriteRequest*r){USED(r);return 0;}
int Sys_fprint64(const Native64SysWriteRequest*r){USED(r);return 0;}
static Module*services[2],*activewm;static void*servedfio;static uint wmbinds,wmfiles;
static void unexpected(void*fp){USED(fp);error("unexpected controlled Sys/Draw call");}
static void fixturebind(void*fp){WORD result=0;void*ret;wmbinds++;memmove(&ret,(uchar*)fp+32,8);memmove(ret,&result,4);}
static void fixturefile2chan(void*fp){void*ret;wmfiles++;ck(servedfio!=nil&&servedfio!=H,"controlled file2chan fixture");D2H(servedfio)->ref++;memmove(&ret,(uchar*)fp+32,8);memmove(ret,&servedfio,8);}
static Module*builtin(const Native64ModuleTable*t){Module*m;uint i;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->name=strdup("controlled-Sys-Draw");m->path=strdup("controlled-Sys-Draw");m->ntype=t->count;m->type=mallocz(t->count*sizeof(Type*),1);m->ext=mallocz((t->count+1)*sizeof(Link),1);require(m->type&&m->ext);
 for(i=0;i<t->count;i++){const Native64ModuleImport*r=t->imports+i;m->type[i]=dtype(freeheap,r->bytes,(uchar*)r->map,r->mapbytes);require(m->type[i]!=nil);m->ext[i].name=strdup(r->name);m->ext[i].sig=r->signature;m->ext[i].frame=m->type[i];m->ext[i].u.runt=!strcmp(r->name,"aprint")||!strcmp(r->name,"sprint")?Sys_programcall64:!strcmp(r->name,"bind")?fixturebind:!strcmp(r->name,"file2chan")?fixturefile2chan:unexpected;}return m;
}
static Modlink*fullload(Module*m,WORD table,char*path){Modlink*ml;ck(m==activewm&&table>=0&&table<2&&(!strcmp(path,"$Sys")||!strcmp(path,"$Draw")),"original init Sys/Draw LOAD");services[table]->ref++;ml=linkmod(services[table],m->ldt[table],1);require(ml&&ml!=H);ptradd(D2H(ml));return ml;}
static Module*initroot(void){Module*m;Inst*p;uchar map[]={0,0x80,0xe0};m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=4096;m->name=strdup("Wmsrv-init-caller");m->path=strdup("Wmsrv-init-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,192,map,3);p=m->prog=mallocz(4*sizeof(Inst),1);m->entry=p;m->nprog=4;m->cstep=native64typedlea8step;
 p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=64;p[0].d.ind=40;p[1].op=ILEA;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].reg=72;p[1].s.ind=128;p[1].d.i.f=40;p[1].d.i.s=32;p[2].op=IMCALL;p[2].add=SRC(AFP)|AXIMM|DST(AFP);p[2].s.ind=40;p[2].d.ind=64;p[3].op=IEXIT;p[3].add=SRC(AXXX)|DST(AXXX);return m;}
/* Two real FileIO requests are queued before starting the unchanged infinite
 * server. It must handle both, then park again in its outer ALT. */
static void tokenrun(int jit,int quantum,WORD fid,int mode){Native64ModuleOwner owner={0};Module*m,*rootmod;Modlink*ml;Prog*p,*parent,*server=nil;Type*ft,*rt,*wt,*pair;Channel*read,*write,*rc;void*fio,*returned[3]={H,H,H};void*owned[4];Import imports[2]={{0}};uchar fm[]={0xc0},rm[]={0x20},wm[]={0x50};uint i,ticks=0;int ok;WORD zero=0,count=100;char expected[64];
 ck(native64sysmodule(&full_spec,full_variadics,2,jit,&owner),"token server constructor");m=activewm=owner.module;for(i=0;i<2;i++)services[i]=builtin(full_tables+i);
 ft=dtype(freeheap,16,fm,1);rt=dtype(freeheap,24,rm,1);wt=dtype(freeheap,32,wm,1);pair=dtype(freeheap,16,fm,1);require(ft&&rt&&wt&&pair);
 fio=servedfio=H2D(void*,heapz(ft));read=cnewc(rt,movtmp,2);write=cnewc(wt,movtmp,0);rc=cnewc(pair,movtmp,mode==1?2:0);
 owned[0]=fio;owned[1]=read;owned[2]=write;owned[3]=rc;for(i=0;i<nelem(owned);i++)ptradd(D2H(owned[i]));D2H(read)->ref++;D2H(write)->ref++;memmove(fio,&read,8);memmove((uchar*)fio+8,&write,8);
 for(i=0;i<2;i++){Channel*reply=mode==2?H:rc;uchar*dst=read->buf->data+24*i;memmove(dst,&zero,4);memmove(dst+4,&count,4);memmove(dst+8,&fid,4);if(reply!=H)D2H(reply)->ref++;memmove(dst+16,&reply,8);}read->size=2;
 for(i=0;i<nelem(full_functions);i++)if(!strcmp(full_functions[i].name,"init"))break;require(i<nelem(full_functions));imports[0].name="init";imports[0].sig=full_functions[i].signature;m->ref++;ml=linkmod(m,imports,1);require(ml&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;
 rootmod=initroot();if(jit)require(compile(rootmod,rootmod->nprog,nil));rootmod->ref++;starting=rootmod;started=nil;wmformatcalls=wmbinds=wmfiles=0;worker.env->errstr[0]=0;require(kprotect(start,nil));parent=started;rootenvironment(parent);memmove(parent->R.FP+64,&ml,8);
 while((p=currun())!=nil){p->quanta=quantum;ok=kprotect(executeprocess,p);if(worker.env->errstr[0])fprintf(stderr,"wm init/token jit=%d q=%d mode=%d error=%s module=%s pc=%ld\n",jit,quantum,mode,worker.env->errstr,R.M->m->name,R.M->m->prog?(long)(R.PC-R.M->m->prog):-1L);ck(!worker.env->errstr[0]&&++ticks<100000,"original init/wm token execution");if(!ok){ck(p==parent,"only init caller exits");memmove(returned,p->R.FP+128,sizeof(returned));for(i=0;i<3;i++){ck(returned[i]!=H,"original init returns three channels");D2H(returned[i])->ref++;ptradd(D2H(returned[i]));}observed=p;progexit();}else if(p->state==Palt)server=p;}
 ck(server!=nil&&server->state==Palt&&read->size==0&&wmformatcalls==(mode==2?0:2)&&wmbinds==1&&wmfiles==1,"init spawned server, two requests consumed, outer ALT parked");ck(rc->size==(mode==1?2:0),"exact buffered/default token replies");snprintf(expected,sizeof(expected),"%d",fid);
 if(mode==1)for(i=0;i<2;i++){Array*a;String*e;memmove(&a,rc->buf->data+16*i,8);memmove(&e,rc->buf->data+16*i+8,8);ck(a!=H&&a->t==&Tbyte&&a->len==(int)strlen(expected)&&!memcmp(a->data,expected,a->len)&&e==H,"original wm exact token bytes and nil error");}
 altgone(server);R=server->R;observed=server;progexit();emptyqueues();ck(rootmod->ref==1,"init caller module lease retired");unload(rootmod);ptrdel(D2H(ml));destroy(ml);ck(m->ref==1,"server process module lease retired");ck(native64typedmoduleclear(&owner),"token module retirement");for(i=0;i<2;i++){ck(services[i]->ref==1,"service module lease retired");unload(services[i]);services[i]=nil;}
 for(i=0;i<3;i++){ck(D2H(returned[i])->ref==1,"returned channel owners retired");ptrdel(D2H(returned[i]));destroy(returned[i]);}
 for(i=0;i<nelem(owned);i++){ck(D2H(owned[i])->ref==1,"token fixture owner retired");ptrdel(D2H(owned[i]));destroy(owned[i]);}freetype(ft);freetype(rt);freetype(wt);freetype(pair);
 ck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tstring.ref==1&&Tarray.ref==1&&Tchannel.ref==1&&Tmodlink.ref==1&&Tptr.ref==1&&Tword.ref==1&&Tbyte.ref==1,"complete original init/wm token retirement");memset(nativeobservations,0,sizeof(nativeobservations));activewm=nil;servedfio=nil;
}
int main(void){Native64ModuleOwner owner={0};REG saved;int jit,n,i,base;uint pc,fn,steps=0;
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;Tarray.size=sizeof(Array);require(dis64selectlaunch("dis64-calls"));saved=R;
 WM_HANDLERS
 ck(nelem(full_functions)==34&&nelem(full_variadics)==2,"all original Wmsrv bodies and formatter sites");for(fn=0;fn<nelem(full_functions);fn++)steps+=full_functions[fn].count;
 for(jit=0;jit<2;jit++){base=allocations;ck(native64sysmodule(&full_spec,full_variadics,2,jit,&owner),"complete Wmsrv constructor");n=allocations-base;ck(owner.module->nprog==steps,"all instructions retained");for(fn=0,pc=0;fn<nelem(full_functions);fn++){ck(pc<(uint)owner.module->nprog,"body PC retained");pc+=full_functions[fn].count;}
 ck(native64typedmoduleclear(&owner),"complete Wmsrv retirement");ck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tstring.ref==1&&Tarray.ref==1&&Tmodlink.ref==1,"complete module owners retired");
 for(i=1;i<=n;i++){failat=allocations+i;ck(!native64sysmodule(&full_spec,full_variadics,2,jit,&owner),"constructor allocation failure");failat=0;ck(!owner.module&&!live&&!modules&&!nlinkpins&&!memcmp(&R,&saved,sizeof(R)),"constructor atomic rollback");}
 printf("PASS complete Wmsrv constructor jit=%d: 34 bodies, %u instructions, %d allocation failures and complete retirement\n",jit,steps,n);}
 {int q,mode;uint row,total=0;int quanta[]={1,3,1000};WORD ids[]={-17,(-2147483647-1),2147483647};
 for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(row=0;row<nelem(ids);row++)for(mode=0;mode<3;mode++){tokenrun(jit,quanta[q],ids[row],mode);total++;}printf("PASS unchanged init/wm token loop: %u C/JIT runs, paired FileIO reads, new/existing clients, exact byte replies, default/nil requests and complete ALT cancellation/retirement\n",total);}
 return 0;
}
