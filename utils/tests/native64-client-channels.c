#include "full.h"
#include "wmsrv-native.h"
static void cccheck(int ok,const char*why){if(!ok){fprintf(stderr,"Client channel predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);cccheck(0,"unexpected LOAD");return H;}
static Module*ccroot(int grow,int args)
{
 Module*m;Inst*p;uchar map[]={0,0xe8,0};uint pc=0,i;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("client-caller");m->path=strdup("client-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);
 m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,144,map,3);p=m->prog=mallocz(7*sizeof(Inst),1);m->entry=p;m->cstep=native64typedlea8step;
 p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=96;p[pc++].d.ind=40;
 for(i=0;i<(uint)args;i++){p[pc].op=i==3?IMOVW:IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64+8*i;p[pc].d.i.f=40;p[pc++].d.i.s=64+8*i;}
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=96;p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;return m;
}
static void cccase(uint fn,int jit,int quantum,int capacity,int profile,int roundrobin)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};
 Type*ct,*pt,*wt,*it;uchar cm[]=Wmsrv_Client_map,wm[]=Draw_Wmcontext_map,im[]={0xe0};
 Wmsrv_Client*c,*next;Draw_Wmcontext*context;Channel*channels[4],*images;void*ptr,*oldp,*pout;String*s,*old,*cursor,*sout;
 Prog*p,*parent;uchar*root,clientbytes[96],ptrbytes[16];WORD values[]={(-2147483647-1),2147483647,-17,0},word,wout,count,sent,untouched=0x31415926;uint i,k,kind=fn<12?fn%4:fn==12?0:3,ticks=0,retired=0,blocks=0;int done,steps;
 cccheck(native64typedmodule(&full_spec,jit,&owner),"owning Client module");
 cccheck(sizeof(Wmsrv_Client)==96&&sizeof(cm)==2&&cm[0]==0xf7&&cm[1]==0x90&&sizeof(Draw_Pointer)==16,"canonical complete Client layout");
 ct=dtype(freeheap,sizeof(Wmsrv_Client),cm,sizeof(cm));pt=dtype(freeheap,sizeof(Draw_Pointer),nil,0);wt=dtype(freeheap,sizeof(Draw_Wmcontext),wm,sizeof(wm));it=dtype(freeheap,24,im,1);require(ct&&pt&&wt&&it);
 c=H2D(Wmsrv_Client*,heapz(ct));next=H2D(Wmsrv_Client*,heapz(ct));context=H2D(Draw_Wmcontext*,heapz(wt));ptradd(D2H(c));ptradd(D2H(next));ptradd(D2H(context));
 for(i=0;i<4;i++){channels[i]=cnewc(i==0||i==3?&Tword:&Tptr,i==0||i==3?actual_movw:actual_movp,i==kind?capacity:0);ptradd(D2H(channels[i]));D2H(channels[i])->ref++;memmove((uchar*)c+8*i,channels+i,8);}
 cursor=c2string("cursor",6);ptradd(D2H(cursor));D2H(cursor)->ref++;c->cursor=cursor;D2H(next)->ref++;c->znext=next;images=cnewc(it,movtmp,0);ptradd(D2H(images));D2H(images)->ref++;c->images=images;D2H(context)->ref++;c->wmctxt=context;
 c->flags=values[profile%4];c->id=values[(profile+1)%4];c->fid=values[(profile+2)%4];c->token=values[(profile+3)%4];memset((uchar*)c+36,0x9a,4);memset((uchar*)c+84,0xbc,4);memmove(clientbytes,c,96);
 ptr=profile&1?H2D(void*,heapz(pt)):H;if(ptr!=H){ptradd(D2H(ptr));for(i=0;i<16;i++)((uchar*)ptr)[i]=ptrbytes[i]=0x80+i+profile;}
 s=profile%4==0?H:profile%4==1?c2string("",0):profile%4==2?c2string("message",7):newrunes(3);if(s!=H){ptradd(D2H(s));if(profile%4==3){s->Srune[0]=0x3bb;s->Srune[1]=0;s->Srune[2]=0x3a9;}}
 oldp=H2D(void*,heapz(pt));old=c2string("old",3);ptradd(D2H(oldp));ptradd(D2H(old));
 imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;
 D2H(oldp)->ref++;memmove(ml->MP+client_globals[1],&oldp,8);D2H(old)->ref++;memmove(ml->MP+client_globals[2],&old,8);memmove(ml->MP+client_globals[0],&untouched,4);
 m=ccroot(quantum==1,4);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=parent=started;rootenvironment(p);root=p->R.FP;
 memmove(root+96,&ml,8);D2H(c)->ref++;memmove(root+64,&c,8);if(ptr!=H)D2H(ptr)->ref++;memmove(root+72,&ptr,8);if(s!=H)D2H(s)->ref++;memmove(root+80,&s,8);word=values[profile%4];memmove(root+88,&word,4);memset(root+92,0xa5,4);memset(root+104,0x5a,40);
 while((p=currun())!=nil){cccheck(++ticks<100000,"bounded Client scheduler");p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;if(p->state==Palt||p->state==Psend||p->state==Precv){blocks++;break;}if(roundrobin){delrun(Pready);addrun(p);break;}}if(done)continue;if(worker.env->errstr[0])fprintf(stderr,"Client fn=%u jit=%d q=%d cap=%d profile=%d rr=%d error=%s\n",fn,jit,quantum,capacity,profile,roundrobin,worker.env->errstr);cccheck(steps<10000&&!worker.env->errstr[0],"Client field transfer without errors");if(p==parent){for(i=92;i<96;i++)cccheck(root[i]==0xa5,"Word upper guard");for(i=104;i<144;i++)cccheck(root[i]==0x5a,"Client caller guard");}observed=p;progexit();retired++;}
 cccheck(nprog()==0&&retired==3,"all Client channel peers resumed and retired");emptyqueues();
 memmove(&wout,ml->MP+client_globals[0],4);memmove(&pout,ml->MP+client_globals[1],8);memmove(&sout,ml->MP+client_globals[2],8);memmove(&count,ml->MP+client_globals[3],4);memmove(&sent,ml->MP+client_globals[4],4);
 cccheck(count==1&&sent==1&&wout==(kind==0||kind==3?(fn>=12?-17:word):untouched)&&pout==(kind==1?ptr:oldp)&&sout==(kind==2?s:old),"exact Client channel payload and untouched outputs");
 if(ptr!=H)cccheck(!memcmp(ptr,ptrbytes,16)&&D2H(ptr)->ref==2+(kind==1),"Pointer contents and owners");if(s!=H)cccheck(D2H(s)->ref==2+(kind==2),"String owners and identity");
 cccheck(D2H(oldp)->ref==(kind==1?1:2)&&D2H(old)->ref==(kind==2?1:2),"displaced roots retired");
 cccheck(!memcmp(c,clientbytes,96)&&D2H(c)->ref==2,"all Client fields and padding preserved");
 for(k=0;k<4;k++){Channel*ch=channels[k];cccheck(D2H(ch)->ref==3&&ch->send->prog==nil&&ch->recv->prog==nil&&!ch->send->next&&!ch->recv->next&&ch->size==0,"borrowed channel owners and empty queues");if((k==1||k==2)&&ch->buf!=H)for(i=0;i<(uint)capacity;i++){void*v;memmove(&v,ch->buf->data+8*i,8);cccheck(v==H,"consumed root buffer cleared");}}
 for(i=0;i<4;i++){Channel*saved;memmove(&saved,ml->MP+client_globals[5+i],8);cccheck(saved==channels[i],"projected channel identity");}
 for(i=0;i<4;i++){WORD saved;memmove(&saved,ml->MP+client_globals[14+i],4);cccheck(saved==values[(profile+i)%4],"projected signed Client scalar");}
 cccheck(D2H(cursor)->ref==3&&D2H(next)->ref==3&&D2H(images)->ref==3&&D2H(context)->ref==3,"projected non-channel roots retained");if(!capacity)cccheck(blocks>0,"blocking Client transfer exercised");
 require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);cccheck(native64typedmoduleclear(&owner),"Client module retired");cccheck(D2H(c)->ref==1,"Client true owner retired");ptrdel(D2H(c));destroy(c);
 for(i=0;i<4;i++){cccheck(D2H(channels[i])->ref==1,"channel true owner retired");ptrdel(D2H(channels[i]));destroy(channels[i]);}
 cccheck(D2H(cursor)->ref==1&&D2H(next)->ref==1&&D2H(images)->ref==1&&D2H(context)->ref==1,"non-channel true owners retired");ptrdel(D2H(cursor));destroy(cursor);ptrdel(D2H(next));destroy(next);ptrdel(D2H(images));destroy(images);ptrdel(D2H(context));destroy(context);
 if(ptr!=H){cccheck(D2H(ptr)->ref==1,"payload Pointer retired");ptrdel(D2H(ptr));destroy(ptr);}if(s!=H){cccheck(D2H(s)->ref==1,"payload String retired");ptrdel(D2H(s));destroy(s);}cccheck(D2H(oldp)->ref==1&&D2H(old)->ref==1,"old true owners retired");ptrdel(D2H(oldp));destroy(oldp);ptrdel(D2H(old));destroy(old);
 cccheck(ct->ref==1&&pt->ref==1&&wt->ref==1&&it->ref==1,"Client descriptors retired");freetype(ct);freetype(pt);freetype(wt);freetype(it);cccheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tchannel.ref==1&&Tarray.ref==1&&Tptr.ref==1&&Tstring.ref==1&&Tmodlink.ref==1,"complete Client frames/roots/module retirement");cccheck(rootpgrp.r.ref==1&&rootfgrp.r.ref==1&&rootegrp.r.ref==1,"Client environment refs retired");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void ccmetadata(void)
{
 Native64ModuleSpec spec;Native64ModuleFunction f[36];Native64TypedStep code[256],*s;Native64ModuleOwner owner={0};REG saved=R;uchar fmap[64],dmap[64],*map;uint fn,pc,k,role,bit,mode,offset,total=0;int before;
 for(fn=0;fn<36;fn++)for(pc=0;pc<full_functions[fn].count;pc++){
  role=full_functions[fn].code[pc].returned;if(role<204||role>206)continue;
  for(k=0;k<10;k++){
   spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code)&&f[fn].mapbytes<=sizeof(fmap)&&spec.mapbytes<=sizeof(dmap));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;s=code+pc;
   switch(k){case 0:s->op=INOP;break;case 1:s->m=role==204?(4<<8)|4:36;break;case 2:if(USRC(s->mode)==AIMM)s->s=536870912L;else s->s+=2;break;case 3:s->d+=2;break;case 4:s->mode|=AXIMM;break;case 5:s->m=role==204?(s->m^16):s->m+1;break;case 6:s->mode=SRC(USRC(s->mode)&~AIND)|DST(UDST(s->mode)&~AIND);break;case 7:s->returned=role==204?188:role==205?206:205;break;case 8:case 9:mode=k==8?USRC(s->mode):UDST(s->mode);offset=k==8?s->s:s->d;if(mode==AIMM){s->s=536870912L;break;}bit=offset/8;map=(mode&~AIND)==AFP?fmap:dmap;map[bit/8]^=128>>(bit%8);break;}
   before=allocations;int refused=!native64typedmodule(&spec,0,&owner);if(!refused)fprintf(stderr,"accepted mutation fn=%u pc=%u role=%u case=%u\n",fn,pc,role,k);cccheck(refused,"malformed Client metadata refused");cccheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"Client refusal before allocation");total++;
  }
 }
 printf("PASS Client metadata: %u field/contract/alignment/owner/value-map refusals before allocation\n",total);
}
static void ccconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;cccheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;cccheck(native64typedmoduleclear(&owner),"constructor cleared");for(i=1;i<=n;i++){failat=allocations+i;cccheck(!native64typedmodule(&full_spec,jit,&owner),"constructor failure");failat=0;cccheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor rollback");}printf("PASS Client constructor: jit=%d %d allocation failures with complete rollback\n",jit,n);}
}
static void ccnil(int jit,int quantum,int nilchannel)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};Prog*p;uchar*root;uint i,ticks=0;int done;Type*t=nil;Wmsrv_Client*c=H;uchar map[]=Wmsrv_Client_map;
 cccheck(native64typedmodule(&full_spec,jit,&owner),"nil-owner module");for(i=0;i<full_spec.nfunctions&&strcmp(full_functions[i].name,"inspect");i++);require(i<full_spec.nfunctions);
 if(nilchannel){t=dtype(freeheap,96,map,sizeof(map));c=H2D(Wmsrv_Client*,heapz(t));ptradd(D2H(c));}
 imports[0].name=(char*)full_functions[i].name;memmove(&imports[0].sig,&full_functions[i].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;
 m=ccroot(quantum==1,1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+96,&ml,8);if(c!=H)D2H(c)->ref++;memmove(root+64,&c,8);
 while(currun()!=nil){cccheck(++ticks<1000,"bounded nil Client scheduler");p->quanta=quantum;done=kprotect(executeprocess,p);if(done)continue;cccheck(!strcmp(worker.env->errstr,exNilref),"nil Client/channel rejected before registration");observed=p;progexit();}
 emptyqueues();require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);cccheck(native64typedmoduleclear(&owner),"nil Client module cleared");if(c!=H){cccheck(D2H(c)->ref==1,"nil-channel Client owner retired");ptrdel(D2H(c));destroy(c);cccheck(t->ref==1,"nil-channel descriptor retired");freetype(t);}cccheck(!live&&!modules&&!nlinkpins&&!worker.nerr,"nil Client complete unwind");memset(nativeobservations,0,sizeof(nativeobservations));
}
int main(void)
{
 uint fn,total=0;int jit,q,capacity,profile,roundrobin;int quanta[]={1,3,1000},sizes[]={0,1,3};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;require(dis64selectlaunch("dis64-calls"));
 CLIENT_HANDLERS
 optab[IMOVP]=actual_movp;optab[ISPAWN]=actual_spawn;optab[ISEND]=actual_isend;optab[IRECV]=actual_irecv;
 ccmetadata();ccconstruction();
 for(fn=0;fn<14;fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(capacity=0;capacity<3;capacity++)for(profile=0;profile<8;profile++)for(roundrobin=0;roundrobin<2;roundrobin++){cccase(fn,jit,quanta[q],sizes[capacity],profile,roundrobin);total++;}
 for(jit=0;jit<2;jit++)for(profile=0;profile<2;profile++){ccnil(jit,1,profile);ccnil(jit,1000,profile);}
 printf("PASS Client fields/channels: %u C/JIT runs, four borrowed channels, all thirteen projected fields, exact Word32 and roots, FP/MP/blocking/buffered, eight nil-owner/channel unwinds and complete retirement\n",total);return 0;
}
