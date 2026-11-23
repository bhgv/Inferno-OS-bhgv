static Modlink*graphload(Module*,WORD,char*);
typedef struct GraphSpec GraphSpec;
struct GraphSpec {const Native64ModuleSpec*spec;const Native64ModuleVariadic*variadic;uint count;const char *path;};
#include "graph-boot.h"
#include "graph-arg.h"
#include "graph-awm.h"
#include "graph-string.h"
#include "graph-wmsrv.h"
#include "graph-wmclient.h"
#include "graph-winplace.h"
#include "graph-wmlib.h"
#include "graph-titlebar.h"
#include "graph-specs.h"
#ifdef TEST_AWM_PANEL_METADATA
#include "graph-tkclient.h"
#include "graph-awmpanel.h"
#include "graph-atoolbar.h"
#endif

static void ck(int ok,const char*why){if(!ok){fprintf(stderr,"AWM startup predicate: %s\n",why);exit(10);}}
int Sys_sprint64(const Native64SysFormatRequest*r){return productionSys_sprint64(r);}
int Sys_aprint64(const Native64SysFormatRequest*r){return productionSys_aprint64(r);}
int Sys_print64(const Native64SysWriteRequest*r){USED(r);ck(0,"unexpected print");return 0;}
int Sys_fprint64(const Native64SysWriteRequest*r){USED(r);ck(0,"unexpected diagnostic");return 0;}
static Native64ModuleOwner owners[9];
static Module *builtins[3],*application;
static void*ret(void*fp){void*r;memmove(&r,(uchar*)fp+32,8);ck(r!=nil&&r!=H,"result cell");return r;}
static void word(void*fp,WORD n){memmove(ret(fp),&n,4);}
static void pointer(void*fp,void*v,int retain){void*old,*r=ret(fp);if(retain&&v!=H)D2H(v)->ref++;memmove(&old,r,8);memmove(r,&v,8);destroy(old);}
static int text(String*s,const char*t){return s!=H&&s->len==(int)strlen(t)&&!memcmp(s->Sascii,t,s->len);}
static void unexpected(void*fp){USED(fp);ck(0,"unexpected device service");}

static void *display=H,*baseimage=H;static void*owned[64];static uint nowned,loads,binds,draws,colors,screens,windows,apps,reads[4],retired;static Type*fdtype;
static void*keep(void*p){ck(nowned<nelem(owned),"fixture bound");owned[nowned++]=p;ptradd(D2H(p));return p;}
static void*record(uint bytes,uchar*map,uint n){Type*t=dtype(freeheap,bytes,map,n);void*p;require(t!=nil);p=H2D(void*,heapz(t));freetype(t);return p;}
static void field(void*p,uint off,void*v){void*old;memmove(&old,(uchar*)p+off,8);if(v!=H)D2H(v)->ref++;memmove((uchar*)p+off,&v,8);destroy(old);}
static void*argptr(void*f,uint at){void*p;memmove(&p,(uchar*)f+at,8);return p;}
static WORD argword(void*f,uint at){WORD n;memmove(&n,(uchar*)f+at,4);return n;}
static void*image(const WORD*r,void*screen){uchar map[]={3,128};void*p=record(72,map,2);memmove(p,r,16);memmove((uchar*)p+16,r,16);field(p,48,display);field(p,56,screen);return p;}
static void allocate(void*f){uchar map[]={0xf0};WORD r[]={0,0,640,480};ck(display==H&&argptr(f,64)==H,"Display.allocate nil device");display=keep(record(48,map,1));baseimage=keep(image(r,H));field(display,0,baseimage);pointer(f,display,1);}
static void color(void*f){WORD r[]={0,0,1,1},n=argword(f,72);ck(argptr(f,64)==display&&(n==(WORD)-1||n==(WORD)0xadadadff||n==(WORD)0x777777ff),"Display.color owners/values");pointer(f,image(r,H),0);colors++;}
static void screenalloc(void*f){uchar map[]={0x70};void*p=record(32,map,1);ck(argptr(f,64)!=H&&argptr(f,72)!=H&&!argword(f,80),"Screen.allocate owners/public");field(p,8,argptr(f,64));field(p,16,argptr(f,72));pointer(f,p,0);screens++;}
static void newwindow(void*f){WORD r[4];memmove(r,(uchar*)f+72,16);ck(argptr(f,64)!=H&&argword(f,88)==1,"Screen.newwindow backing");pointer(f,image(r,argptr(f,64)),0);windows++;}
static void draw(void*f){ck(argptr(f,64)!=H&&argptr(f,88)!=H&&argptr(f,96)==H,"Image.draw owner/mask");draws++;}
static void bindservice(void*f){String*a=argptr(f,64),*b=argptr(f,72);ck((text(a,"#e")&&text(b,"/env"))||(text(a,"#s")&&text(b,"/chan")),"original namespace bind");binds++;word(f,0);}
static void openservice(void*f){String*p=argptr(f,64);WORD kind=0;void*fd;if(text(p,"#e/emuargs")||text(p,"/chan/wmrect")){pointer(f,H,0);return;}if(text(p,"/dev/keyboard"))kind=1;else if(text(p,"/dev/pointer"))kind=2;else if(text(p,"/dev/cursor"))kind=3;ck(kind!=0,"original device path");fd=H2D(void*,heapz(fdtype));memmove(fd,&kind,4);pointer(f,fd,0);}
static void readservice(void*f){void*fd=argptr(f,64);Array*a=argptr(f,72);WORD kind,n=0;char packet[64];memmove(&kind,fd,4);ck(kind==1||kind==2,"input FD owner");ck(a!=H&&argword(f,80)==a->len&&a->len==(kind==1?12:49),"input byte buffer");if(!reads[kind]++){if(kind==1){n=2;memmove(a->data,"\xd1\x8f",2);}else{n=snprintf(packet,sizeof packet,"m%11d %11d %11d %11d ",10,20,0,7);ck(n==49,"pointer packet");memmove(a->data,packet,n);}}word(f,n);}
static void pctlservice(void*f){List*l=argptr(f,72);WORD flags=argword(f,64);if(l!=H){uint i=0;for(;l!=H;l=l->tail,i++){WORD n;memmove(&n,l->data,4);ck(n==(WORD)i&&i<3,"command FD list");}ck(i==3,"three inherited FDs");}word(f,flags?0:currun()->pid);}
static void appinit(void*f){void*c=argptr(f,64);List*l=argptr(f,72);ck(c!=H&&argptr(c,0)==display&&argptr(c,16)!=H&&l!=H&&l->tail==H&&text(*(String**)l->data,"wm/atoolbar"),"original command Context/argv");apps++;}
static Channel *fioreads[2],*fiowrites[2],*readreplies[2],*writereply;
static void file2chan(void*f){uchar fm[]={0xc0},rm[]={0x20},wm[]={0x50},wpm[]={0x40};Type*rt=dtype(freeheap,24,rm,1),*wt=dtype(freeheap,32,wm,1),*rp=dtype(freeheap,16,fm,1),*wp=dtype(freeheap,16,wpm,1);void*p=keep(record(16,fm,1));Channel*r=cnewc(rt,movtmp,2),*w=cnewc(wt,movtmp,1),*rc=keep(cnewc(rp,movtmp,2));String*n=argptr(f,72);uint which=text(n,"wmrect"),i;WORD count=128,fid=-17;ck(text(argptr(f,64),"/chan")&&(text(n,"wmctl")||which),"original FileIO service paths");ck(!fioreads[which],"one original FileIO per service");fioreads[which]=r;fiowrites[which]=w;readreplies[which]=rc;
 for(i=0;i<2;i++){uchar*dst=r->buf->data+24*i;WORD off=which&&i?1000:0;memmove(dst,&off,4);memmove(dst+4,&count,4);memmove(dst+8,&fid,4);D2H(rc)->ref++;memmove(dst+16,&rc,8);}r->size=2;
 if(which){uchar*dst=w->buf->data;WORD zero=0;Channel*wc=keep(cnewc(wp,movtmp,1));writereply=wc;memmove(dst,&zero,4);memmove(dst+16,&fid,4);D2H(wc)->ref++;memmove(dst+24,&wc,8);w->size=1;}
 field(p,0,r);field(p,8,w);destroy(r);destroy(w);freetype(rt);freetype(wt);freetype(rp);freetype(wp);pointer(f,p,1);}
static void replies(void){uint j,i;const char*want;Array*a;String*err;WORD n;for(j=0;j<2;j++){ck(fioreads[j]&&fioreads[j]->size==0&&fiowrites[j]->size==0&&readreplies[j]->size==2,"both original FileIO loops consumed requests");for(i=0;i<2;i++){memmove(&a,readreplies[j]->buf->data+16*i,8);memmove(&err,readreplies[j]->buf->data+16*i+8,8);want=j?(i?"":"0 0 640 480"):"-17";ck(a!=H&&a->t==&Tbyte&&a->len==(int)strlen(want)&&!memcmp(a->data,want,a->len)&&err==H,"exact original FileIO read reply");}}
 ck(writereply&&writereply->size==1,"wmrect write reply count");memmove(&n,writereply->buf->data,4);memmove(&err,writereply->buf->data+8,8);ck(n==0&&text(err,"cannot write"),"original wmrect write refusal");}
static void(*service(const char*n))(void*){uint i;for(i=0;i<nelem(geometry);i++)if(!strcmp(n,geometry[i].name))return geometry[i].call;
 /* String names stay explicit: signatures are checked by linkmod. */
 if(!strcmp(n,"sprint")||!strcmp(n,"aprint")||!strcmp(n,"fprint"))return Sys_programcall64;
 if(!strcmp(n,"Display.allocate"))return allocate;if(!strcmp(n,"Display.color"))return color;if(!strcmp(n,"Screen.allocate"))return screenalloc;if(!strcmp(n,"Screen.newwindow"))return newwindow;if(!strcmp(n,"Image.draw"))return draw;
 if(!strcmp(n,"bind"))return bindservice;if(!strcmp(n,"open"))return openservice;if(!strcmp(n,"read"))return readservice;if(!strcmp(n,"pctl"))return pctlservice;if(!strcmp(n,"file2chan"))return file2chan;return unexpected;}
static Module*builtin(const Native64ModuleImport*const*t,uint count,int app){Module*m=mallocz(sizeof(*m),1);uint i;require(m!=nil);m->ref=1;m->origmp=H;m->name=strdup("controlled-startup-device");m->path=strdup(m->name);m->ntype=count;m->type=mallocz(count*sizeof(Type*),1);m->ext=mallocz((count+1)*sizeof(Link),1);require(m->type&&m->ext);for(i=0;i<count;i++){const Native64ModuleImport*r=t[i];m->type[i]=dtype(freeheap,r->bytes,(uchar*)r->map,r->mapbytes);require(m->type[i]!=nil);m->ext[i].name=strdup(r->name);m->ext[i].sig=r->signature;m->ext[i].frame=m->type[i];m->ext[i].u.runt=app?appinit:service(r->name);}return m;}
static Modlink*graphload(Module*m,WORD table,char*path){Module*target=nil;Modlink*ml;uint i,caller;for(caller=0;caller<nelem(owners)&&owners[caller].module!=m;caller++);ck(caller<nelem(owners)&&table>=0&&(uint)table<graph_specs[caller].spec->ntables&&path!=nil,"LOAD caller/table boundary");const char*paths[]={"","/dis/lib/arg.dis","/dis/wm/awm.dis","/dis/lib/string.dis","/dis/lib/wmsrv.dis","/dis/lib/wmclient.dis","/dis/lib/winplace.dis","/dis/lib/wmlib.dis","/dis/lib/titlebar.dis"};for(i=1;i<nelem(paths);i++)if(!strcmp(path,paths[i]))target=owners[i].module;
 if(!strcmp(path,"$Sys"))target=builtins[0];if(!strcmp(path,"$Draw"))target=builtins[1];if(!strcmp(path,"$Tk"))target=builtins[2];if(!strcmp(path,"/dis/sh.dis")||!strcmp(path,"wm/atoolbar.dis"))return H;if(!strcmp(path,"/dis/wm/atoolbar.dis"))target=application;
 if(!target)fprintf(stderr,"unexpected LOAD %s from %s table %d\n",path,m->name,table);ck(target!=nil,"LOAD exact module path");target->ref++;ml=linkmod(target,m->ldt[table],1);ck(ml&&ml!=H,"cross-module signature link");ptradd(D2H(ml));loads++;return ml;}
static void run(int jit,int q){uint i,ticks=0;int ok;Prog*p;const Native64ModuleImport*a=graph_application;uint order[]={0,1,2,5,6,4,7,8,3};loads=binds=draws=colors=screens=windows=apps=retired=0;memset(reads,0,sizeof reads);display=baseimage=H;memset(fioreads,0,sizeof fioreads);memset(fiowrites,0,sizeof fiowrites);memset(readreplies,0,sizeof readreplies);writereply=nil;
 for(i=0;i<nelem(graph_specs);i++){const GraphSpec*g=graph_specs+i;ck(g->count?native64sysmodule(g->spec,g->variadic,g->count,jit,owners+i):native64typedmodule(g->spec,jit,owners+i),g->spec->name);ck((owners[i].module->jit!=nil)==jit,"every graph module uses selected execution mode");}
 builtins[0]=builtin(graph_Sys,nelem(graph_Sys),0);builtins[1]=builtin(graph_Draw,nelem(graph_Draw),0);builtins[2]=builtin(graph_Tk,nelem(graph_Tk),0);application=builtin(&a,1,1);fdtype=dtype(freeheap,4,nil,0);require(fdtype!=nil);
 starting=owners[0].module;starting->ref++;started=nil;worker.env->errstr[0]=0;require(kprotect(start,nil));rootenvironment(started);
 while((p=currun())!=nil){p->quanta=q;ok=kprotect(executeprocess,p);if(worker.env->errstr[0])fprintf(stderr,"startup jit=%d q=%d error=%s module=%s pc=%ld\n",jit,q,worker.env->errstr,R.M->m->name,R.M->m->prog?(long)(R.PC-R.M->m->prog):-1L);ck(!worker.env->errstr[0]&&++ticks<100000,"original startup execution");if(!ok){observed=p;progexit();retired++;}}
 ck(retired==4,"four finite startup workers completed");
 ck(apps==1&&binds==2&&screens==2&&windows==1&&draws==9&&colors==6&&reads[1]==2&&reads[2]==2,"whole original startup and input completed");
 replies();
 {uint waiting[9]={0};for(p=isched.head;p!=nil;p=p->next){for(i=0;i<9;i++)if(p->R.M->m==owners[i].module)break;ck(i<9&&p->state==Palt,"actual module event loop parked");waiting[i]++;if(i==2){void*screen;WORD r[]={0,0,640,480};memmove(&screen,p->R.M->MP+awm_screen_offset,8);ck(screen!=H&&D2H(screen)->t->size==32&&argptr(screen,8)!=H&&!memcmp(argptr(screen,8),r,16),"AWM screen root and exact geometry");}}ck(waiting[2]==1&&waiting[4]==1&&waiting[7]==1,"AWM Wmsrv Wmlib infinite loops reached");}
 for(p=isched.head;p!=nil;p=isched.head){ck(p->state==Palt,"only infinite event loops parked");addrun(p);irestore(p);altdone(p->R.s,p,nil,-1);observed=p;progexit();retired++;}emptyqueues();ck(retired==7,"all startup processes retired");
 for(i=0;i<nelem(order);i++)ck(native64typedmoduleclear(owners+order[i]),"linked module retirement");for(i=0;i<3;i++){ck(builtins[i]->ref==1,"builtin lease retirement");unload(builtins[i]);}ck(application->ref==1,"application lease retirement");unload(application);
 field(display,0,H);for(i=0;i<nowned;i++){ptrdel(D2H(owned[i]));destroy(owned[i]);}nowned=0;freetype(fdtype);ck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tarray.ref==1&&Tstring.ref==1&&Tlist.ref==1&&Tchannel.ref==1&&Tptr.ref==1&&Tword.ref==1&&Tbyte.ref==1&&Tmodlink.ref==1,"whole graph ownership retirement");printf("PASS original emuinit -> AWM jit=%d q=%d: %u LOADs, graphics/input, wmrect/token replies, 7 processes retired\n",jit,q,loads);}
/* Whole Wmlib adds the previously unexecuted Client[sc.id] load/store pair.
 * Both instructions remain one protected borrow; corruptions fail before allocation. */
static void metadata(void){Native64ModuleSpec spec=wmlib_spec;Native64ModuleFunction fn[nelem(wmlib_functions)];Native64ModuleOwner out={0};REG saved=R;uint i,j,k,n=0;int before,baseline;for(i=0;i<nelem(fn);i++){const Native64ModuleFunction*f=wmlib_functions+i;Native64TypedStep*code=mallocz(f->count*sizeof(*code),0);require(code!=nil);memmove(fn,wmlib_functions,sizeof fn);spec.functions=fn;fn[i].code=code;for(j=0;j+1<f->count;j++)if(f->code[j].returned==111&&f->code[j].op==IINDL){uint pc;for(pc=j;pc<=j+1;pc++)for(k=0;k<6;k++){memmove(code,f->code,f->count*sizeof(*code));switch(k){case 0:code[pc].op=255;break;case 1:code[pc].mode=255;break;case 2:code[pc].s=536870912L;break;case 3:code[pc].m=65536;break;case 4:code[pc].d=536870912L;break;case 5:code[pc].returned=999;break;}before=allocations;baseline=live;ck(!native64sysmodule(&spec,wmlib_variadics,nelem(wmlib_variadics),0,&out),"malformed Client registry pair refused");ck(allocations==before&&live==baseline&&!out.module&&!memcmp(&R,&saved,sizeof R),"registry refusal before allocation");n++;}}free(code);}ck(n==36,"all original Client registry pairs checked");printf("PASS full Wmlib registry: %u corruptions refused before allocation\n",n);}
static void construction(void){Native64ModuleOwner o={0};REG saved=R;int jit,base,n,i;for(jit=0;jit<2;jit++){base=allocations;ck(native64sysmodule(&wmlib_spec,wmlib_variadics,nelem(wmlib_variadics),jit,&o),"full Wmlib constructor");n=allocations-base;ck(native64typedmoduleclear(&o),"full Wmlib clear");for(i=1;i<=n;i++){failat=allocations+i;ck(!native64sysmodule(&wmlib_spec,wmlib_variadics,nelem(wmlib_variadics),jit,&o),"full Wmlib allocation failure");failat=0;ck(!live&&!modules&&!nlinkpins&&!o.module&&!memcmp(&R,&saved,sizeof R),"full Wmlib atomic rollback");}printf("PASS full Wmlib constructor jit=%d: %d allocation rollbacks\n",jit,n);}}
#ifdef TEST_AWM_PANEL_METADATA
static void
panellink(Module *caller,const Native64ModuleSpec *spec,Module *target,const char *entry)
{
 const Native64ModuleTable *table;
 unsigned int i,j,slot;
 const Native64ModuleImport *request;
 const Link *provider;
 Modlink *link;
 for(i=0;i<spec->ntables;i++){
  table=spec->tables+i;
  for(j=0;j<table->count;j++)
   if(!strcmp(table->imports[j].name,entry))break;
  if(j<table->count)break;
 }
 ck(i<spec->ntables,"panel import table located");
 for(j=0;j<table->count;j++){
  request=table->imports+j;
  for(provider=target->ext;provider->name;provider++)
   if(!strcmp(provider->name,request->name)&&(UWORD)provider->sig==request->signature)break;
  ck(provider->name&&provider->frame&&provider->frame->size>=request->bytes&&
     provider->frame->np>=request->mapbytes,"real panel provider frame covers imported arguments");
  for(slot=0;slot<request->bytes/8;slot++){
   unsigned int mask=128>>(slot%8);
   ck((provider->frame->map[slot/8]&mask)==(request->map[slot/8]&mask),
      "real panel provider argument roots agree with import");
  }
 }
 target->ref++;
 link=linkmod(target,caller->ldt[i],1);
 ck(link!=nil&&link!=H,"real panel import links");
 destroy(link);
}
static void
panelself(Module *toolbar)
{
 Import empty={0};
 Modlink *current,*other,*self=H;
 String *path=c2string("$self",5);
 REG saved=R;
 WORD table=-1;
 unsigned int pc;
 for(pc=0;pc<(unsigned int)toolbar->nprog;pc++){
  const Inst *load=toolbar->prog+pc;
  String *name;
  if(load->op!=ILOAD||USRC(load->add)!=AMP)continue;
  memmove(&name,toolbar->origmp+load->s.ind,8);
  if(text(name,"$self")){
   table=load->reg;
   break;
  }
 }
 ck(table>=0&&table<(WORD)atoolbar_spec.ntables,"Toolbar self import table");
 toolbar->ref+=2;
 current=linkmod(toolbar,&empty,1);
 other=linkmod(toolbar,&empty,1);
 ck(current!=nil&&current!=H&&other!=nil&&other!=H,"distinct Toolbar instances");
 R.M=current;R.m=&table;R.s=&path;R.d=&self;
 actual_iload();
 R=saved;
 ck(self!=nil&&self!=H&&self->m==toolbar&&self->MP==current->MP&&
    self->MP!=other->MP,"Toolbar self shares only the calling instance's globals");
 destroy(self);destroy(other);destroy(current);destroy(path);
}
static void
panelconstruction(void)
{
 Native64ModuleOwner tkclient={0},toolbar={0},wmlib={0},panel={0};
 ck(native64sysmodule(&wmlib_spec,wmlib_variadics,nelem(wmlib_variadics),0,&wmlib),"Wmlib module construction for panel");
 ck(native64sysmodule(&tkclient_spec,tkclient_variadics,nelem(tkclient_variadics),0,&tkclient),"Tkclient module construction");
 ck(native64sysmodule(&atoolbar_spec,atoolbar_variadics,nelem(atoolbar_variadics),0,&toolbar),"Toolbar module construction");
 ck(toolbar.module->htab && toolbar.module->htab[0].etab &&
    !strcmp(toolbar.module->htab[0].etab[0].s,"fail:*"),"Toolbar catch installed");
 ck(native64typedmodule(&awmpanel_spec,0,&panel),"native64 Android panel module construction");
 panelself(toolbar.module);
 panellink(panel.module,&awmpanel_spec,tkclient.module,"toplevel");
 panellink(toolbar.module,&atoolbar_spec,tkclient.module,"toplevel");
 panellink(tkclient.module,&tkclient_spec,wmlib.module,"splitqword");
 ck(native64typedmoduleclear(&panel)&&native64typedmoduleclear(&toolbar)&&native64typedmoduleclear(&tkclient)&&
    native64typedmoduleclear(&wmlib)&&!live,"panel module retirement");
 puts("PASS original Toolbar -> Tkclient -> Wmlib construction, imports, self and retirement");
}
#endif
int main(void){int jit,q;int quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freestring;Tarray.free=freearray;Tarray.size=sizeof(Array);require(dis64selectlaunch("dis64-calls"));quotefmtinstall();
 GRAPH_HANDLERS
 metadata();construction();
 for(jit=0;jit<2;jit++)for(q=0;q<3;q++)run(jit,quanta[q]);
#ifdef TEST_AWM_PANEL_METADATA
 panelconstruction();
#endif
 return 0;}
