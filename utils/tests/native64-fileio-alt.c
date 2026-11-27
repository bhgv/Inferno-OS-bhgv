#include "full.h"
#include "fileio-native.h"
static void ficheck(int ok,const char*why){if(!ok){fprintf(stderr,"FileIO ALT predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);ficheck(0,"unexpected LOAD");return H;}
static Module*firoot(int grow,int args)
{
 Module*m;Inst*p;uchar map[]={0,0xf8,0};uint pc=0,i;
 m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;
 m->name=strdup("fileio-caller");m->path=strdup("fileio-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);
 m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,144,map,3);
 p=m->prog=mallocz(7*sizeof(Inst),1);m->entry=p;m->cstep=native64typedlea8step;
 p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=96;p[pc++].d.ind=40;
 for(i=0;i<(uint)args;i++){p[pc].op=IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64+8*i;p[pc].d.i.f=40;p[pc++].d.i.s=64+8*i;}
 p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=96;
 p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;return m;
}
static void fidescriptors(Native64ModuleOwner*o)
{
 uint i,j,pc=0,base=full_spec.nfunctions+1,seen=0;Type*w,*r;
 ficheck(o->module->ntype==(int)base+3,"separate read/write descriptor pools");
 w=o->module->type[base];r=o->module->type[base+1];
 ficheck(w!=r&&w->size==32&&w->np==1&&w->map[0]==0x50&&r->size==24&&r->np==1&&r->map[0]==0x20,"exact mixed descriptor sizes and maps");
 for(i=0;i<full_spec.nfunctions;i++)for(j=0;j<full_functions[i].count;j++,pc++)if(full_functions[i].code[j].returned==201||full_functions[i].code[j].returned==202){ficheck(o->module->prog[pc].reg==base+(full_functions[i].code[j].returned==202),"mixed projection descriptor relocation");seen|=full_functions[i].code[j].returned==201?1:2;}
 ficheck(seen==3,"both mixed projection kinds exercised");
}
static void ficase(uint fn,int jit,int quantum,int capacity,int profile,int roundrobin)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};Type*ft,*readtype,*writetype,*rrtype,*wrtype;
 uchar fm[]=Sys_FileIO_map,rm[]={0x20},wm[]={0x50},rrmap[]={0xc0},wrmap[]={0x40};
 Sys_FileIO*f;Channel*read,*write,*rr,*wr,*oldr,*oldw,*rout,*wout,*ch;Array*a,*olda,*aout;
 Prog*p,*parent;uchar*root,snapshot[19];WORD words[]={(-2147483647-1),2147483647,-17,0},off,count,fid,offsetout,countout,fidout,received,sent,choice,untouched=0x31415926;
 uint i,k,ticks=0,retired=0,blocks=0;int done,steps,iswrite=fn%2==0,mp=fn==2||fn==3;
 ficheck(native64typedmodule(&full_spec,jit,&owner),"owning mixed FileIO module");fidescriptors(&owner);
 ficheck(sizeof(Sys_FileIO)==16&&sizeof(fm)==1&&fm[0]==0xc0,"canonical FileIO layout");
 ft=dtype(freeheap,sizeof(Sys_FileIO),fm,sizeof(fm));readtype=dtype(freeheap,24,rm,1);writetype=dtype(freeheap,32,wm,1);rrtype=dtype(freeheap,16,rrmap,1);wrtype=dtype(freeheap,16,wrmap,1);
 require(ft&&readtype&&writetype&&rrtype&&wrtype);
 f=H2D(Sys_FileIO*,heapz(ft));ptradd(D2H(f));read=cnewc(readtype,movtmp,iswrite?0:capacity);write=cnewc(writetype,movtmp,iswrite?capacity:0);ptradd(D2H(read));ptradd(D2H(write));D2H(read)->ref++;f->read=read;D2H(write)->ref++;f->write=write;
 a=profile&1?H2D(Array*,heaparray(&Tbyte,profile==1?0:19)):H;if(a!=H){ptradd(D2H(a));for(i=0;i<(uint)a->len;i++)a->data[i]=snapshot[i]=i*13+profile;}
 rr=profile&2?cnewc(rrtype,movtmp,0):H;wr=profile&4?cnewc(wrtype,movtmp,0):H;if(rr!=H)ptradd(D2H(rr));if(wr!=H)ptradd(D2H(wr));
 olda=H2D(Array*,heaparray(&Tbyte,3));oldr=cnewc(rrtype,movtmp,0);oldw=cnewc(wrtype,movtmp,0);ptradd(D2H(olda));ptradd(D2H(oldr));ptradd(D2H(oldw));
 off=words[profile%4];count=words[(profile+1)%4];fid=words[(profile+2)%4];memmove(owner.module->origmp+file_globals[7],&off,4);memmove(owner.module->origmp+file_globals[8],&count,4);memmove(owner.module->origmp+file_globals[9],&fid,4);
 for(i=10;i<13;i++)memmove(owner.module->origmp+file_globals[i],&untouched,4);
 imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;
 D2H(olda)->ref++;memmove(ml->MP+file_globals[4],&olda,8);D2H(oldr)->ref++;memmove(ml->MP+file_globals[5],&oldr,8);D2H(oldw)->ref++;memmove(ml->MP+file_globals[6],&oldw,8);
 m=firoot(quantum==1,4);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=parent=started;rootenvironment(p);root=p->R.FP;
 memmove(root+96,&ml,8);D2H(f)->ref++;memmove(root+64,&f,8);if(a!=H)D2H(a)->ref++;memmove(root+72,&a,8);if(rr!=H)D2H(rr)->ref++;memmove(root+80,&rr,8);if(wr!=H)D2H(wr)->ref++;memmove(root+88,&wr,8);memset(root+104,0x5a,40);
 while((p=currun())!=nil){ficheck(++ticks<100000,"bounded mixed scheduler");p->quanta=quantum;for(steps=0;steps<10000;steps++){done=kprotect(executeprocess,p);if(!done)break;if(p->state==Palt||p->state==Psend||p->state==Precv){blocks++;break;}if(roundrobin){delrun(Pready);addrun(p);break;}}if(done)continue;if(worker.env->errstr[0])fprintf(stderr,"FileIO fn=%u jit=%d q=%d cap=%d profile=%d rr=%d error=%s\n",fn,jit,quantum,capacity,profile,roundrobin,worker.env->errstr);ficheck(steps<10000&&!worker.env->errstr[0],"mixed FileIO execution without errors");if(p==parent)for(i=104;i<144;i++)ficheck(root[i]==0x5a,"FileIO caller guard");observed=p;progexit();retired++;}
 ficheck(nprog()==0&&retired==3,"all mixed ALT peers resumed and retired");emptyqueues();
 memmove(&offsetout,ml->MP+file_globals[10],4);memmove(&countout,ml->MP+file_globals[11],4);memmove(&fidout,ml->MP+file_globals[12],4);memmove(&received,ml->MP+file_globals[13],4);memmove(&sent,ml->MP+file_globals[14],4);memmove(&choice,ml->MP+file_globals[15],4);
 memmove(&aout,ml->MP+file_globals[4],8);memmove(&rout,ml->MP+file_globals[5],8);memmove(&wout,ml->MP+file_globals[6],8);
 ficheck(received==1&&sent==1&&(fn>=6?(choice==1||choice==2):choice==(iswrite?1:2)),"exact FileIO send/receive and selection");
 ficheck(offsetout==off&&fidout==fid&&countout==(iswrite?untouched:count),"exact signed Word fields and untouched output");
 ficheck(aout==(iswrite?a:olda)&&rout==(iswrite?oldr:rr)&&wout==(iswrite?wr:oldw),"exact mixed payload roots and untouched outputs");
 if(a!=H)ficheck(a->len==(profile==1?0:19)&&!memcmp(a->data,snapshot,a->len)&&D2H(a)->ref==2+iswrite+(mp&&iswrite),"byte array contents and owners");
 if(rr!=H)ficheck(D2H(rr)->ref==2+!iswrite+(mp&&!iswrite)&&rr->send->prog==nil&&rr->recv->prog==nil,"Rread identity and owners");
 if(wr!=H)ficheck(D2H(wr)->ref==2+iswrite+(mp&&iswrite)&&wr->send->prog==nil&&wr->recv->prog==nil,"Rwrite identity and owners");
 ficheck(D2H(olda)->ref==(iswrite?1:2)&&D2H(oldr)->ref==(iswrite?2:1)&&D2H(oldw)->ref==(iswrite?1:2),"all displaced and untouched output roots");
 ficheck(D2H(f)->ref==2&&f->read==read&&f->write==write&&D2H(read)->ref==2&&D2H(write)->ref==2,"borrowed FileIO fields and true owners preserved");
 for(k=0;k<2;k++){ch=k?write:read;ficheck(ch->send->prog==nil&&ch->recv->prog==nil&&!ch->send->next&&!ch->recv->next&&ch->size==0,"all FileIO registrations cancelled");if(ch->buf!=H)for(i=0;i<(uint)capacity;i++){void*first,*last;if(k)memmove(&first,ch->buf->data+32*i+8,8);else first=H;memmove(&last,ch->buf->data+(k?32:24)*i+(k?24:16),8);ficheck(first==H&&last==H,"consumed mixed buffer roots cleared");}}
 if(!capacity)ficheck(blocks>0,"mixed blocking path exercised");
 require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);ficheck(native64typedmoduleclear(&owner),"FileIO module retired");
 ficheck(D2H(f)->ref==1,"FileIO true owner retired");ptrdel(D2H(f));destroy(f);
 ficheck(D2H(read)->ref==1&&D2H(write)->ref==1,"channel field owners retired");ptrdel(D2H(read));destroy(read);ptrdel(D2H(write));destroy(write);
 if(a!=H){ficheck(D2H(a)->ref==1,"array true owner retired");ptrdel(D2H(a));destroy(a);}if(rr!=H){ficheck(D2H(rr)->ref==1,"Rread true owner retired");ptrdel(D2H(rr));destroy(rr);}if(wr!=H){ficheck(D2H(wr)->ref==1,"Rwrite true owner retired");ptrdel(D2H(wr));destroy(wr);}
 ficheck(D2H(olda)->ref==1&&D2H(oldr)->ref==1&&D2H(oldw)->ref==1,"old true owners retired");ptrdel(D2H(olda));destroy(olda);ptrdel(D2H(oldr));destroy(oldr);ptrdel(D2H(oldw));destroy(oldw);
 ficheck(ft->ref==1&&readtype->ref==1&&writetype->ref==1&&rrtype->ref==1&&wrtype->ref==1,"all mixed descriptors retired");freetype(ft);freetype(readtype);freetype(writetype);freetype(rrtype);freetype(wrtype);
 ficheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tchannel.ref==1&&Tarray.ref==1&&Tbyte.ref==1&&Tptr.ref==1&&Tstring.ref==1&&Tmodlink.ref==1,"complete mixed roots/frames/module retirement");ficheck(rootpgrp.r.ref==1&&rootfgrp.r.ref==1&&rootegrp.r.ref==1,"FileIO environment refs retired");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void fimetadata(void)
{
 Native64ModuleSpec spec;Native64ModuleFunction f[21];Native64TypedStep code[128],*s;Native64ModuleOwner owner={0};REG saved=R;uchar fmap[32],dmap[32],*map;uint fn,pc,k,role,contract,width,slots,cases,bit,offset,mode,total=0;int before;
 for(fn=0;fn<21;fn++)for(pc=0;pc<full_functions[fn].count;pc++){
  role=full_functions[fn].code[pc].returned;if(role<198||role>203||role==200)continue;
  contract=role==199||role==201?104:120;width=native64fileiowidth(contract);slots=width/8;cases=role==198?12:(role==199||role==203)?10+slots:7+2*slots;
  for(k=0;k<cases;k++){
   spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code)&&f[fn].mapbytes<=sizeof(fmap)&&spec.mapbytes<=sizeof(dmap));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;s=code+pc;
   if(role==198){switch(k){case 0:s->op=IMOVP;break;case 1:s->m=4;break;case 2:s->m=s->m?0:8;break;case 3:s->s+=4;break;case 4:s->d+=4;break;case 5:s->mode=SRC(USRC(s->mode)&~AIND)|DST(AFP);break;case 6:s->returned=193;break;case 7:s->m=16;break;case 8:s->s=(USRC(s->mode)&~AIND)==AFP?f[fn].bytes:spec.databytes;break;case 9:bit=s->s/8;map=(USRC(s->mode)&~AIND)==AFP?fmap:dmap;map[bit/8]&=~(128>>(bit%8));break;case 10:bit=s->d/8;fmap[bit/8]|=128>>(bit%8);break;case 11:s->mode|=AXIMM;break;}}
   else if((role==199||role==203)){switch(k){case 0:s->op=IMOVP;break;case 1:s->m=role==199?120:104;break;case 2:s->s+=4;break;case 3:s->d+=4;break;case 4:s->s=USRC(s->mode)==AFP?f[fn].bytes:spec.databytes;break;case 5:s->mode|=AIND;break;case 6:s->returned=role==199?203:199;break;case 7:s->m=48;break;case 8:bit=s->d/8;fmap[bit/8]|=128>>(bit%8);break;case 9:s->mode|=AXIMM;break;default:bit=(s->s+8*(k-10))/8;map=USRC(s->mode)==AFP?fmap:dmap;map[bit/8]^=128>>(bit%8);break;}}
   else{switch(k){case 0:s->op=IMOVP;break;case 1:s->m=1;break;case 2:s->s+=4;break;case 3:s->d+=4;break;case 4:s->s=USRC(s->mode)==AFP?f[fn].bytes:spec.databytes;break;case 5:s->mode|=AIND;break;case 6:s->returned=role==201?202:201;break;default:offset=k-7;mode=offset<slots?USRC(s->mode):UDST(s->mode);bit=((offset<slots?s->s:s->d)+8*(offset%slots))/8;map=mode==AFP?fmap:dmap;map[bit/8]^=128>>(bit%8);break;}}
   before=allocations;ficheck(!native64typedmodule(&spec,0,&owner),"malformed FileIO metadata refused");ficheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"FileIO refusal before allocation");total++;
  }
 }
 printf("PASS FileIO metadata: %u field/class/extent/root-and-nonroot-map refusals before allocation\n",total);
}
static void ficonstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;ficheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;ficheck(native64typedmoduleclear(&owner),"constructor cleared");for(i=1;i<=n;i++){failat=allocations+i;ficheck(!native64typedmodule(&full_spec,jit,&owner),"constructor failure");failat=0;ficheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor rollback");}printf("PASS FileIO constructor: jit=%d %d allocation failures with complete rollback\n",jit,n);}
}
static void ficheckowner(int jit,int quantum,int create)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};Prog*p;uchar*root;uint i,ticks=0;int done;
 ficheck(native64typedmodule(&full_spec,jit,&owner),"nil-owner module");for(i=0;i<full_spec.nfunctions&&strcmp(full_functions[i].name,create?"create":"inspect");i++);require(i<full_spec.nfunctions);
 imports[0].name=(char*)full_functions[i].name;memmove(&imports[0].sig,&full_functions[i].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;
 m=firoot(quantum==1,!create);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+96,&ml,8);
 while(currun()!=nil){ficheck(++ticks<1000,"bounded nil-owner scheduler");p->quanta=quantum;done=kprotect(executeprocess,p);if(done)continue;ficheck(!strcmp(worker.env->errstr,create?"":exNilref),"FileIO nil owner rejected before registration");observed=p;progexit();}
 emptyqueues();
 if(create){
  Sys_FileIO *file;Channel *read,*write;
  memmove(&file,ml->MP+file_globals[0],8);
  ficheck(file!=H,"allocated FileIO");
  read=file->read;write=file->write;
  ficheck(read!=H&&write!=H&&read!=write,"separate FileIO channels");
  ficheck(read->mid.t->size==24&&read->mid.t->map[0]==0x20&&write->mid.t->size==32&&write->mid.t->map[0]==0x50,"allocated channel payload maps");
 }
 require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);ficheck(native64typedmoduleclear(&owner)&&!live&&!modules&&!nlinkpins&&!worker.nerr,"nil-owner complete unwind");memset(nativeobservations,0,sizeof(nativeobservations));
}
int main(void)
{
 uint fn,total=0;int jit,q,capacity,profile,roundrobin;int quanta[]={1,3,1000},sizes[]={0,1,3};
 rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;require(dis64selectlaunch("dis64-calls"));
 FILE_HANDLERS
 optab[INEWZ]=actual_newz;optab[INEWCMP]=actual_newcmp;optab[IMOVP]=actual_movp;optab[IMOVMP]=actual_movmp;optab[ISPAWN]=actual_spawn;optab[ISEND]=actual_isend;optab[IRECV]=actual_irecv;optab[IALT]=actual_alt;optab[INBALT]=actual_nbalt;optab[IGOTO]=actual_igoto;
 fimetadata();ficonstruction();
 for(fn=0;fn<8;fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(capacity=0;capacity<3;capacity++)for(profile=0;profile<8;profile++)for(roundrobin=0;roundrobin<2;roundrobin++){ficase(fn,jit,quanta[q],sizes[capacity],profile,roundrobin);total++;}
 for(jit=0;jit<2;jit++){ficheckowner(jit,1,0);ficheckowner(jit,1000,0);ficheckowner(jit,1,1);ficheckowner(jit,1000,1);}
 printf("PASS FileIO mixed ALT: %u C/JIT runs, signed Word fields, FP/MP read/write tuples, exact mixed GC maps, nil/empty/nonempty arrays and replies, duplicate guards, buffers, four nil-owner unwinds and complete retirement\n",total);return 0;
}
