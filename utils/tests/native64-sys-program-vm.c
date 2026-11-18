#include "callsites-native64.h"
#include "emuinit-varargs-native64.h"
static int programcase,programmode,programcalls;
static void programcheck(int ok,const char *text){if(!ok){fprintf(stderr,"program VM predicate: %s\n",text);exit(10);}}
/* Bounded producer seam: real PC/frame/binder/MCALL/scheduler/JIT and ordinary
 * heap ownership execute here. Actual formatter/GC is a separate positive gate. */
static String*programoutput(const Native64SysFormatRequest*r){String*s;programcheck(r->prog==currun(),"current process identity");
 programcalls++;if(programmode==3)failat=allocations+1;s=c2string("typed error",11);failat=0;return s;}
int Sys_sprint64(const Native64SysFormatRequest*r){String*s,*old;void*cell;
 s=programoutput(r);cell=r->resolve(r->argument);programcheck(cell!=nil && cell!=H,"late String owner");memmove(&old,cell,8);memmove(cell,&s,8);destroy(old);return 1;}
int Sys_fprint64(const Native64SysWriteRequest*r){String*s;WORD n=71+programcase;void*cell;
 s=programoutput(&r->format);programcheck(r->fd!=nil && r->fd!=H,"captured FD");destroy(s);cell=r->format.resolve(r->format.argument);programcheck(cell!=nil && cell!=H,"late scalar owner");memmove(cell,&n,4);return 1;}
int Sys_aprint64(const Native64SysFormatRequest*r){USED(r);programcheck(0,"unexpected aprint");return 0;}
int Sys_print64(const Native64SysWriteRequest*r){USED(r);programcheck(0,"unexpected print");return 0;}
static void programforeign(void*frame){USED(frame);programcheck(0,"foreign native entry reached");}
static void programrun(int b,int jit,int q,int mode){
 const Native64GeneratedStore *programs[]={emuinit_call_0,emuinit_call_1,emuinit_call_2};
 const Native64GeneratedBank *banks[]={&emuinit_call_0_bank,&emuinit_call_1_bank,&emuinit_call_2_bank};
 const uchar *maps[]={emuinit_call_0_framemap,emuinit_call_1_framemap,emuinit_call_2_framemap};
 const unsigned sizes[]={emuinit_call_0_framebytes,emuinit_call_1_framebytes,emuinit_call_2_framebytes};
 const Native64GeneratedBank*bank=banks[b];const Native64GeneratedStore*store;const Native64GeneratedCell*cell;
 Module*m,*target;Modlink*native;Prog*process;uchar*root,map[16];void*value;String*text;Type*fdtype;Sys_FD*fd;
 Native64SysProgramSpec spec;Native64TailSlot slots[2];unsigned i,j,pc,nargs=b==0?3:b==1?1:4,count=b==0?1:b==1?0:2;
 unsigned resultoffset=bank->bytes,linkoffset=resultoffset+8,niloffset=linkoffset+8;Inst*p;int done,steps;WORD status;uvlong guard;
 programcase=b;programmode=mode;programcalls=0;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->rt=DONTCOMPILE;m->origmp=H;m->ss=4096;m->cstep=native64typedlea8step;
 m->name=strdup("trusted-Sys-program");m->path=strdup("trusted-Sys-program");require(m->name&&m->path);
 m->ntype=3;m->type=mallocz(3*sizeof(Type*),1);require(m->type!=nil);m->type[0]=dtype(freeheap,0,nil,0);
 memset(map,0,sizeof(map));memmove(map,bank->map,bank->mapbytes);map[linkoffset/64]|=128>>((linkoffset/8)%8);map[niloffset/64]|=128>>((niloffset/8)%8);if(b==1)map[resultoffset/64]|=128>>((resultoffset/8)%8);
 m->entryt=m->type[1]=dtype(freeheap,niloffset+16,map,(niloffset+79)/64);m->type[2]=dtype(freeheap,sizes[b],(uchar*)maps[b],2);for(i=0;i<3;i++)require(m->type[i]!=nil);
 p=m->prog=mallocz(16*sizeof(Inst),1);require(p!=nil);pc=0;p[pc].op=IFRAME;p[pc].add=SRC(AIMM)|DST(AFP);p[pc].s.imm=2;p[pc++].d.ind=40;
 for(i=0;i<nargs;i++){store=programs[b]+i;p[pc].op=IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=store->source_offset;p[pc].d.i.f=40;p[pc++].d.i.s=store->destination;
  if(b!=1&&i==0){p[pc].op=IMOVP;p[pc].add=SRC(AFP)|DST(AFP);p[pc].s.ind=niloffset;p[pc++].d.ind=store->source_offset;}}
 memset(&spec,0,sizeof(spec));spec.bindpc=pc;spec.callpc=pc+1;
 p[pc].op=ILEA;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].reg=b==1?24:4;p[pc].s.ind=resultoffset;p[pc].d.i.f=40;p[pc++].d.i.s=32;
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=linkoffset;p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;m->entry=p;
 for(i=0;i<count;i++)slots[i]=(Native64TailSlot){Native64TailRef,8*i,8,&Tstring};spec.frame=(Native64SysFrameSpec){b==1?Native64SysSprint:Native64SysFprint,m->type[2],m->type[1],resultoffset,slots,count,8,128};require(Sys_setprogram64(m,&spec,1));
 target=mallocz(sizeof(*target),1);require(target!=nil);target->ref=1;target->origmp=H;target->ntype=1;target->type=mallocz(sizeof(Type*),1);require(target->type!=nil);target->type[0]=m->type[2];target->type[0]->ref++;
 target->ref++;native=mklinkmod(target,1);require(native!=H);native->MP=H;native->links[0].frame=m->type[2];native->links[0].u.runt=mode==2?programforeign:Sys_programcall64;
 if(jit)require(compile(m,m->nprog,nil));if(mode==1)m->prog[spec.bindpc].reg^=8;
 m->ref++;starting=m;started=nil;require(kprotect(start,nil));process=started;rootenvironment(process);root=process->R.FP;memmove(root+linkoffset,&native,8);
 fdtype=dtype(freeheap,sizeof(Sys_FD),nil,0);require(fdtype!=nil);
 for(i=0;i<bank->count;i++){cell=bank->cells+i;store=nil;for(j=0;j<nargs;j++)if(programs[b][j].cell==(int)i){store=programs[b]+j;break;}require(store!=nil);
  if(b!=1 && store->fixed && store->ordinal==0){fd=H2D(Sys_FD*,heapz(fdtype));fd->fd=7;value=fd;}
  else{text=cell->seed==1?c2string((char*)cell->initial,cell->length):c2string(store->ordinal==0?"chosen":"failure",store->ordinal==0?6:7);value=text;}memmove(root+64+8*i,&value,8);}
 if(b!=1)memset(root+resultoffset,0x5a,8);memset(root+niloffset+8,0xa5,8);process->quanta=q;
 for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,process);if(!done)break;}failat=0;
 if(steps>=10000 || strcmp(worker.env->errstr,mode==1||mode==2?exBounds:mode==3?exHeap:""))fprintf(stderr,"program case=%d jit=%d q=%d mode=%d error=%s\n",b,jit,q,mode,worker.env->errstr);
 programcheck(steps<10000 && !strcmp(worker.env->errstr,mode==1||mode==2?exBounds:mode==3?exHeap:""),"complete VM outcome");
 programcheck(programcalls==((mode==0||mode==3)?1:0),"dispatch count");
 if(mode){if(b==1){memmove(&value,root+resultoffset,8);programcheck(value==H,"failed String preserved");}else{memmove(&guard,root+resultoffset,8);programcheck(guard==0x5a5a5a5a5a5a5a5aULL,"failed scalar preserved");}}
 else if(b==1){memmove(&text,root+resultoffset,8);programcheck(text!=H && text->len==11 && !memcmp(text->Sascii,"typed error",11),"String result published");}
 else{memmove(&status,root+resultoffset,4);programcheck(status==71+b,"Word result published");for(i=4;i<8;i++)programcheck(root[resultoffset+i]==0x5a,"Word width preserved");}
 for(i=0;i<8;i++)programcheck(root[niloffset+8+i]==0xa5,"caller guard");
 observed=process;progexit();emptyqueues();require(m->ref==1 && target->ref==1);unload(m);unload(target);freetype(fdtype);
 programcheck(!live && !modules && !nlinkpins && !worker.nerr && Tstring.ref==1 && Tmodlink.ref==1,"complete bridge retirement");memset(nativeobservations,0,sizeof(nativeobservations));
}
int main(int argc,char**argv){int b,j,q,k,total=0;int quanta[]={1,3,1000};require(argc==1||argc==2);
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;require(dis64selectlaunch("dis64-calls"));
 PROGRAM_HANDLERS
 optab[IMOVP]=actual_movp;
 for(b=0;b<3;b++)for(j=0;j<2;j++)for(q=0;q<3;q++)for(k=0;k<4;k++){if(argc==2 && k!=2)continue;programrun(b,j,quanta[q],k);total++;}
 printf("PASS full Sys program VM: %d C/JIT recipe calls, quanta 1/3/1000, stale-code and foreign-target refusal, producer allocation failure and complete retirement; formatter producer bounded\n",total);return 0;}
