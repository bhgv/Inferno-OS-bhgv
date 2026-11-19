#include "full.h"
#include "window-native.h"
static void wlcheck(int ok,const char*why){if(!ok){fprintf(stderr,"Window list predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);wlcheck(0,"unexpected LOAD");return H;}
static Module*wlroot(int grow)
{
 Module*m;Inst*p;uchar map[]={0,0xf0,0};uint i,pc=0;m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("window-list-caller");m->path=strdup("window-list-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,144,map,3);p=m->prog=mallocz(6*sizeof(Inst),1);m->entry=p;m->cstep=native64typedlea8step;
 p[pc].op=IMFRAME;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=88;p[pc++].d.ind=40;for(i=0;i<3;i++){p[pc].op=IMOVP;p[pc].add=SRC(AFP)|DST(AFP|AIND);p[pc].s.ind=64+8*i;p[pc].d.i.f=40;p[pc++].d.i.s=64+8*i;}p[pc].op=IMCALL;p[pc].add=SRC(AFP)|AXIMM|DST(AFP);p[pc].s.ind=40;p[pc++].d.ind=88;p[pc].op=IEXIT;p[pc++].add=SRC(AXXX)|DST(AXXX);m->nprog=pc;return m;
}
static Wmsrv_Window*wlhead(List*l){Wmsrv_Window*w;require(l!=H);memmove(&w,l->data,8);return w;}
static List*wlmake(Wmsrv_Window**v,uint n)
{
 REG was=R;List*l=H;Wmsrv_Window*w;while(n){w=v[--n];R.s=&w;R.d=&l;actual_consp();}memmove(&R,&was,sizeof(R));if(l!=H)ptradd(D2H(l));return l;
}
static uint wlcollect(List**nodes,uint n,List*l)
{
 uint i;while(l!=H){for(i=0;i<n&&nodes[i]!=l;i++);if(i<n)break;wlcheck(n<64,"bounded list ownership graph");nodes[n++]=l;l=l->tail;}return n;
}
static void wlgraph(List*input,List*saved,List*made,List*rest,Wmsrv_Window*last,String*tag,Draw_Image*image,Wmsrv_Window*a,Wmsrv_Window*b,String*ta,String*tb,Draw_Image*ia,Draw_Image*ib)
{
 List*nodes[64],*roots[]={input,saved,made,rest};uint n=0,i,j,refs[64]={0},ar=1,br=1,tar=ta==H?0:1,tbr=tb==H?0:1,iar=1,ibr=1;Wmsrv_Window*w;
 for(i=0;i<4;i++)n=wlcollect(nodes,n,roots[i]);for(i=0;i<4;i++)if(roots[i]!=H){for(j=0;nodes[j]!=roots[i];j++);refs[j]++;}
 for(i=0;i<n;i++){wlcheck(nodes[i]->t==&Tptr&&D2H(nodes[i])->t==&Tlist,"actual pointer-list node descriptors");if(nodes[i]->tail!=H){for(j=0;nodes[j]!=nodes[i]->tail;j++);refs[j]++;}w=wlhead(nodes[i]);wlcheck(w==H||w==a||w==b,"exact Window list element class/identity");ar+=w==a;br+=w==b;}
 ar+=last==a;br+=last==b;wlcheck(last==H||last==a||last==b,"last Window identity");
 tar+=ta!=H&&a->tag==ta;tar+=ta!=H&&b->tag==ta;tar+=ta!=H&&tag==ta;tbr+=tb!=H&&a->tag==tb;tbr+=tb!=H&&b->tag==tb;tbr+=tb!=H&&tag==tb;
 iar+=a->img==ia;iar+=b->img==ia;iar+=image==ia;ibr+=a->img==ib;ibr+=b->img==ib;ibr+=image==ib;
 for(i=0;i<n;i++)wlcheck(D2H(nodes[i])->ref==refs[i],"exact shared-tail list owners");wlcheck(D2H(a)->ref==ar&&D2H(b)->ref==br,"exact list-head Window owners");if(ta!=H)wlcheck(D2H(ta)->ref==tar,"exact tag A owners");if(tb!=H)wlcheck(D2H(tb)->ref==tbr,"exact tag B owners");wlcheck(D2H(ia)->ref==iar&&D2H(ib)->ref==ibr,"exact Image owners");wlcheck(Tlist.ref==1+(int)n&&Tptr.ref==1+(int)n,"no unreachable list node leases");
}
static void wlcase(uint fn,int jit,int quantum,int profile,int failcons)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};Type*wt,*it;uchar wm[]=Wmsrv_Window_map,im[]=Draw_Image_map;Wmsrv_Window*a,*b,*argA,*argB,*input[8],*sequence[8],*last=H,*expectedlast=H;String*ta,*tb,*tag=H,*expectedtag=H;Draw_Image*ia,*ib,*image=H,*expectedimage=H;List*l,*saved,*made,*rest,*expectedtail=H;Prog*p;uchar*root,asnap[32],bsnap[32];WORD count,sum,guard=0x31592653,expectedcount=0,expectedsum=0;Draw_Rect rect,mirror,expectedrect={{0,0},{0,0}};Draw_Point point,pointmirror,expectedpoint={0,0};uint i,n=0,sn=0,ticks=0,retired=0;int done,error;
 wlcheck(native64typedmodule(&full_spec,jit,&owner),"owning Window list module");wlcheck(sizeof(Wmsrv_Window)==32&&sizeof(Draw_Rect)==16&&sizeof(Draw_Point)==8&&wm[0]==0x90,"canonical Window mixed layout");
 wt=dtype(freeheap,32,wm,sizeof(wm));it=dtype(freeheap,sizeof(Draw_Image),im,sizeof(im));require(wt&&it);a=H2D(Wmsrv_Window*,heapz(wt));b=H2D(Wmsrv_Window*,heapz(wt));ia=H2D(Draw_Image*,heapz(it));ib=H2D(Draw_Image*,heapz(it));ptradd(D2H(a));ptradd(D2H(b));ptradd(D2H(ia));ptradd(D2H(ib));
 ta=profile%3==0?H:profile%3==1?c2string("",0):newrunes(3);tb=profile==5?H:c2string("beta",4);if(ta!=H){ptradd(D2H(ta));if(profile%3==2){ta->Srune[0]=0x3bb;ta->Srune[1]=0;ta->Srune[2]=0x3a9;}D2H(ta)->ref++;}if(tb!=H){ptradd(D2H(tb));D2H(tb)->ref++;}a->tag=ta;b->tag=tb;
 if(profile%2==0){D2H(ia)->ref++;a->img=ia;}D2H(ib)->ref++;b->img=ib;a->r=(Draw_Rect){{-17,(-2147483647-1)},{2147483647,29}};b->r=(Draw_Rect){{31,2147483647},{-101,(-2147483647-1)}};memmove(asnap,a,32);memmove(bsnap,b,32);argA=profile==4?H:a;argB=profile==3?a:b;
 if(profile==1){input[n++]=a;}else if(profile==2){input[n++]=a;input[n++]=H;input[n++]=b;}else if(profile==3){input[n++]=a;input[n++]=b;input[n++]=a;input[n++]=H;}else if(profile==4){input[n++]=H;input[n++]=H;}else if(profile==5){input[n++]=b;input[n++]=a;}l=wlmake(input,n);
 error=((fn==4||fn==5||fn==6)&&!n)||((fn==8||fn==9||fn==10)&&argA==H);
 if(fn<=1){sn=n;memmove(sequence,input,n*sizeof(*input));}else if(fn==2){sn=n;for(i=0;i<n;i++)sequence[i]=input[n-i-1];}else if(fn==3){sn=3;sequence[0]=H;sequence[1]=argB;sequence[2]=argA;}else if(fn==7){sn=n+1;sequence[0]=argA;memmove(sequence+1,input,n*sizeof(*input));}
 if(failcons)sn=0;
 if(fn<=3&&!failcons)for(i=0;i<sn;i++){expectedcount++;if(sequence[i]!=H){expectedlast=sequence[i];expectedtag=expectedlast->tag;expectedimage=expectedlast->img;expectedrect=expectedlast->r;expectedpoint=expectedrect.min;expectedsum+=expectedrect.min.x;}}
 if(fn==4&&n)expectedlast=input[0];if((fn==4||fn==5||fn==6)&&n)expectedtail=l->tail;
 if(fn==8){expectedlast=argA;if(argA!=H){expectedcount=1;expectedtag=argA->tag;expectedimage=argA->img;expectedrect=argA->r;expectedpoint=argA->r.min;memmove(asnap+16,asnap+8,8);}}else if(fn==9&&argA!=H){void*nilimage=H;memmove(asnap+24,&nilimage,8);}else if(fn==10&&argA!=H){void*niltag=H;memmove(asnap,&niltag,8);}
 imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;memmove(ml->MP+window_globals[12],&guard,4);
 m=wlroot(quantum==1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+88,&ml,8);if(l!=H)D2H(l)->ref++;memmove(root+64,&l,8);if(argA!=H)D2H(argA)->ref++;memmove(root+72,&argA,8);if(argB!=H)D2H(argB)->ref++;memmove(root+80,&argB,8);memset(root+96,0x5a,48);
 wlconsfail=failcons;wlconscount=0;
 while(currun()!=nil){wlcheck(++ticks<10000,"bounded Window list scheduler");p->quanta=quantum;done=kprotect(executeprocess,p);if(done)continue;if(!error&&!failcons&&worker.env->errstr[0])fprintf(stderr,"Window list fn=%u jit=%d q=%d profile=%d error=%s\n",fn,jit,quantum,profile,worker.env->errstr);if(strcmp(worker.env->errstr,failcons?exHeap:error?exNilref:""))fprintf(stderr,"failure case fn=%u jit=%d q=%d profile=%d consfail=%d seen=%d actual=%s expected=%s\n",fn,jit,quantum,profile,failcons,wlconscount,worker.env->errstr,failcons?exHeap:error?exNilref:"");wlcheck(!strcmp(worker.env->errstr,failcons?exHeap:error?exNilref:""),"Window list exact completion/error");for(i=96;i<144;i++)wlcheck(root[i]==0x5a,"Window list caller guard");failat=0;wlconsfail=0;observed=p;progexit();retired++;}
 wlcheck(retired==1&&nprog()==0,"Window list process retired");emptyqueues();memmove(&saved,ml->MP+window_globals[0],8);memmove(&made,ml->MP+window_globals[1],8);memmove(&rest,ml->MP+window_globals[2],8);memmove(&last,ml->MP+window_globals[3],8);memmove(&tag,ml->MP+window_globals[4],8);memmove(&image,ml->MP+window_globals[5],8);memmove(&rect,ml->MP+window_globals[6],16);memmove(&mirror,ml->MP+window_globals[7],16);memmove(&point,ml->MP+window_globals[8],8);memmove(&pointmirror,ml->MP+window_globals[9],8);memmove(&count,ml->MP+window_globals[10],4);memmove(&sum,ml->MP+window_globals[11],4);WORD check;memmove(&check,ml->MP+window_globals[12],4);
 wlcheck(count==expectedcount&&sum==expectedsum&&last==expectedlast&&tag==expectedtag&&image==expectedimage&&check==guard,"exact Window list traversal and captured roots");wlcheck(!memcmp(&rect,&expectedrect,16)&&!memcmp(&mirror,&expectedrect,16)&&!memcmp(&point,&expectedpoint,8)&&!memcmp(&pointmirror,&expectedpoint,8),"exact relocated Window geometry");wlcheck(!memcmp(a,asnap,32)&&!memcmp(b,bsnap,32),"Window fields and adjacent roots preserved");
 wlcheck(saved==(fn==1?H:fn==6?expectedtail:fn==7?made:l)&&rest==expectedtail,"exact saved/tail alias identities");if(fn==2||fn==3||fn==7){List*q=made;for(i=0;i<sn;i++){wlcheck(q!=H&&wlhead(q)==sequence[i],"exact constructed/reversed list order");q=q->tail;}wlcheck(q==H,"exact constructed list length");}else wlcheck(made==H,"untouched made root");wlgraph(l,saved,made,rest,last,tag,image,a,b,ta,tb,ia,ib);
 require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);wlcheck(native64typedmoduleclear(&owner),"Window list module retired");if(l!=H){wlcheck(D2H(l)->ref==1,"input list true owner retired");ptrdel(D2H(l));destroy(l);}wlcheck(D2H(a)->ref==1&&D2H(b)->ref==1,"all list head/last owners retired");ptrdel(D2H(a));destroy(a);ptrdel(D2H(b));destroy(b);if(ta!=H){wlcheck(D2H(ta)->ref==1,"tag A true owner retired");ptrdel(D2H(ta));destroy(ta);}if(tb!=H){wlcheck(D2H(tb)->ref==1,"tag B true owner retired");ptrdel(D2H(tb));destroy(tb);}wlcheck(D2H(ia)->ref==1&&D2H(ib)->ref==1,"Image true owners retired");ptrdel(D2H(ia));destroy(ia);ptrdel(D2H(ib));destroy(ib);wlcheck(wt->ref==1&&it->ref==1,"Window list descriptors retired");freetype(wt);freetype(it);wlcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1&&Tstring.ref==1&&Tlist.ref==1&&Tptr.ref==1,"complete Window list module/frames/roots retirement");wlcheck(rootpgrp.r.ref==1&&rootfgrp.r.ref==1&&rootegrp.r.ref==1,"Window list environment refs retired");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void wlmetadata(void)
{
 Native64ModuleSpec spec;Native64ModuleFunction f[12];Native64TypedStep code[256],*s;Native64ModuleOwner owner={0};REG saved=R;uchar fmap[64],dmap[64],*map;uint fn,pc,k,mode,bit,total=0;int before;
 for(fn=0;fn<12;fn++)for(pc=0;pc<full_functions[fn].count;pc++)if(full_functions[fn].code[pc].returned>=213&&full_functions[fn].code[pc].returned<=215)for(k=0;k<12;k++){
  spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code)&&f[fn].mapbytes<=sizeof(fmap)&&spec.mapbytes<=sizeof(dmap));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;s=code+pc;
  switch(k){case 0:s->op=INOP;break;case 1:s->m=s->returned==213?1:s->returned==214?8:(4<<8)|(s->m&255);break;case 2:s->s+=2;break;case 3:s->d+=2;break;case 4:s->mode&=~ARM;if(s->returned!=215)s->mode|=AXIMM;break;case 5:s->mode=SRC(AIMM)|(s->mode&ARM)|DST(UDST(s->mode));break;case 6:s->mode=SRC(USRC(s->mode))|(s->mode&ARM)|DST(AIMM);break;case 7:s->s=(USRC(s->mode)&~AIND)==AFP?f[fn].bytes:spec.databytes;break;case 8:s->d=(UDST(s->mode)&~AIND)==AFP?f[fn].bytes:spec.databytes;break;case 9:case 10:mode=k==9?USRC(s->mode):UDST(s->mode);bit=(k==9?s->s:s->d)/8;map=(mode&~AIND)==AFP?fmap:dmap;map[bit/8]^=128>>(bit%8);break;case 11:s->returned=s->returned==213?214:s->returned==214?215:216;break;}
  before=allocations;wlcheck(!native64typedmodule(&spec,0,&owner),"malformed Window metadata refused");wlcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"Window refusal before allocation");total++;
 }
 printf("PASS Window list metadata: %u opcode/class/span/source-and-destination-GC refusals before allocation\n",total);
}
static void wlconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;wlcheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;wlcheck(native64typedmoduleclear(&owner),"constructor cleared");for(i=1;i<=n;i++){failat=allocations+i;wlcheck(!native64typedmodule(&full_spec,jit,&owner),"constructor failure");failat=0;wlcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor rollback");}printf("PASS Window list constructor: jit=%d %d allocation failures with complete rollback\n",jit,n);}
}
int main(void)
{
 uint fn,total=0;int jit,q,profile;int quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;require(dis64selectlaunch("dis64-calls"));
 WINDOW_HANDLERS
 optab[IMOVP]=actual_movp;optab[IMOVM]=actual_movm;optab[ICONSP]=actual_consp;optab[IHEADP]=actual_headp;optab[ITAIL]=actual_tail;wlmetadata();wlconstruction();for(fn=0;fn<11;fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(profile=0;profile<6;profile++){wlcase(fn,jit,quanta[q],profile,0);total++;}for(jit=0;jit<2;jit++)for(q=0;q<2;q++)for(profile=1;profile<=3;profile++){wlcase(3,jit,q?1000:1,2,profile);total++;}printf("PASS Window lists/fields: %u C/JIT runs, nil/cons/head/tail, loops/reverse/alias/shared tails, canonical mixed fields/geometry, exact complete ownership graph, twelve cons-allocation unwinds and retirement\n",total);return 0;
}
