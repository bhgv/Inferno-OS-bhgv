#include "full.h"
#include "draw-native.h"
static void lgcheck(int ok,const char*why){if(!ok){fprintf(stderr,"Local geometry predicate: %s\n",why);exit(10);}}
static Modlink*fullload(Module*m,WORD t,char*p){USED(m);USED(t);USED(p);lgcheck(0,"unexpected LOAD");return H;}
static Module*lgroot(int grow)
{
 Module*m;Inst*p;uchar map[]={0,0xc0};m=mallocz(sizeof(*m),1);require(m!=nil);m->ref=1;m->origmp=H;m->rt=DONTCOMPILE;m->ss=grow?0:4096;m->name=strdup("geometry-caller");m->path=strdup("geometry-caller");m->ntype=2;m->type=mallocz(2*sizeof(Type*),1);m->type[0]=dtype(freeheap,0,nil,0);m->entryt=m->type[1]=dtype(freeheap,128,map,2);
 p=m->prog=mallocz(4*sizeof(Inst),1);m->entry=p;m->nprog=4;m->cstep=native64typedlea8step;p[0].op=IMFRAME;p[0].add=SRC(AFP)|AXIMM|DST(AFP);p[0].s.ind=72;p[0].d.ind=40;p[1].op=IMOVP;p[1].add=SRC(AFP)|DST(AFP|AIND);p[1].s.ind=64;p[1].d.i.f=40;p[1].d.i.s=64;p[2].op=IMCALL;p[2].add=SRC(AFP)|AXIMM|DST(AFP);p[2].s.ind=40;p[2].d.ind=72;p[3].op=IEXIT;p[3].add=SRC(AXXX)|DST(AXXX);return m;
}
static void lgcase(uint fn,int jit,int quantum,int profile)
{
 Native64ModuleOwner owner={0};Module*m;Modlink*ml;Import imports[2]={{0}};Type*it,*dt,*st;uchar im[]=Draw_Image_map,dm[]=Draw_Display_map,sm[]=Draw_Screen_map;Draw_Image*image=H;Draw_Display*display;Draw_Screen*screen;String*name;Prog*p;uchar*root,*expectedmp,expectedimage[72],originalimage[72];UWORD rect[4],point[2],words[]={0x80000000U,0x7fffffffU,0xffffffffU,0,0x76543210U,0x89abcdefU,17,0xfedcba98U};WORD guard=0x31592653,lead=0x555aaa55,count=1;uint i,ticks=0,retired=0,width=fn%6<2?16:8,offsets[]={0,16,0,8,16,24},offset=offsets[fn%6];int done,capture=fn<6||(fn>=12&&fn<18);
 lgcheck(native64typedmodule(&full_spec,jit,&owner),"owning local geometry module");lgcheck(sizeof(Draw_Image)==72&&sizeof(Draw_Rect)==16&&sizeof(Draw_Point)==8,"canonical Word32 geometry sizes");
 it=dtype(freeheap,sizeof(Draw_Image),im,sizeof(im));dt=dtype(freeheap,sizeof(Draw_Display),dm,sizeof(dm));st=dtype(freeheap,sizeof(Draw_Screen),sm,sizeof(sm));require(it&&dt&&st);display=H2D(Draw_Display*,heapz(dt));screen=H2D(Draw_Screen*,heapz(st));name=c2string("preserved",9);ptradd(D2H(display));ptradd(D2H(screen));ptradd(D2H(name));
 if(profile!=5){image=H2D(Draw_Image*,heapz(it));ptradd(D2H(image));memset(image,0x39+profile,48);for(i=0;i<8;i++)memmove((uchar*)image+4*i,words+(i+profile)%8,4);D2H(display)->ref++;image->display=display;D2H(screen)->ref++;image->screen=screen;D2H(name)->ref++;image->iname=name;memmove(originalimage,image,72);memmove(expectedimage,image,72);}
 for(i=0;i<4;i++)rect[i]=words[(i+profile+3)%8];for(i=0;i<2;i++)point[i]=words[(i+profile+5)%8];
 imports[0].name=(char*)full_functions[fn].name;memmove(&imports[0].sig,&full_functions[fn].signature,4);owner.module->ref++;ml=linkmod(owner.module,imports,1);require(ml!=nil&&ml!=H);ptradd(D2H(ml));D2H(ml)->ref++;memmove(ml->MP+geometry_globals[1],rect,16);memmove(ml->MP+geometry_globals[2],point,8);memset(ml->MP+geometry_globals[3],0xa3,16);memset(ml->MP+geometry_globals[4],0xb4,16);memset(ml->MP+geometry_globals[5],0xc5,8);memset(ml->MP+geometry_globals[6],0xd6,8);memmove(ml->MP+geometry_globals[8],&guard,4);memmove(ml->MP+geometry_globals[9],&lead,4);lgcheck(geometry_globals[1]%8==4&&geometry_globals[2]%8==4,"four-byte-aligned raw geometry exercised");
 expectedmp=mallocz(full_spec.databytes,0);require(expectedmp!=nil);memmove(expectedmp,ml->MP,full_spec.databytes);memmove(expectedmp+geometry_globals[0],&image,8);
 if(fn<24&&!capture)memmove(expectedmp+geometry_globals[width==16?4:6],width==16?(void*)rect:(void*)point,width);
 if(profile!=5){memmove(expectedmp+geometry_globals[7],&count,4);if(fn<24){if(capture){memmove(expectedmp+geometry_globals[width==16?3:5],originalimage+offset,width);memmove(expectedmp+geometry_globals[width==16?4:6],originalimage+offset,width);}else{memmove(expectedmp+geometry_globals[width==16?4:6],width==16?(void*)rect:(void*)point,width);memmove(expectedimage+offset,width==16?(void*)rect:(void*)point,width);}}else if(fn==24){memmove(expectedimage,originalimage+16,16);memmove(expectedimage+16,originalimage,16);}else if(fn==25)memmove(expectedimage+8,originalimage,8);}
 m=lgroot(quantum==1);if(jit)require(compile(m,m->nprog,nil));m->ref++;starting=m;started=nil;require(kprotect(start,nil));p=started;rootenvironment(p);root=p->R.FP;memmove(root+72,&ml,8);if(image!=H)D2H(image)->ref++;memmove(root+64,&image,8);memset(root+80,0x5a,48);
 while(currun()!=nil){lgcheck(++ticks<10000,"bounded geometry scheduler");p->quanta=quantum;done=kprotect(executeprocess,p);if(done)continue;if(profile!=5&&worker.env->errstr[0])fprintf(stderr,"geometry fn=%u jit=%d q=%d profile=%d error=%s\n",fn,jit,quantum,profile,worker.env->errstr);lgcheck(!strcmp(worker.env->errstr,profile==5?exNilref:""),"geometry exact completion/error");for(i=80;i<128;i++)lgcheck(root[i]==0x5a,"geometry caller guard");observed=p;progexit();retired++;}
 lgcheck(retired==1&&nprog()==0,"geometry process retired");emptyqueues();lgcheck(!memcmp(ml->MP,expectedmp,full_spec.databytes),"exact geometry outputs and all adjacent MP bytes");if(image!=H)lgcheck(!memcmp(image,expectedimage,72)&&D2H(image)->ref==2,"exact Image geometry and preserved roots/raw guard");lgcheck(D2H(display)->ref==(image==H?1:2)&&D2H(screen)->ref==(image==H?1:2)&&D2H(name)->ref==(image==H?1:2),"untouched Image root owners");
 require(m->ref==1);unload(m);ptrdel(D2H(ml));destroy(ml);lgcheck(native64typedmoduleclear(&owner),"geometry module retired");if(image!=H){lgcheck(D2H(image)->ref==1,"Image true owner retired");ptrdel(D2H(image));destroy(image);}lgcheck(D2H(display)->ref==1&&D2H(screen)->ref==1&&D2H(name)->ref==1,"nested Image owners retired");ptrdel(D2H(display));destroy(display);ptrdel(D2H(screen));destroy(screen);ptrdel(D2H(name));destroy(name);free(expectedmp);lgcheck(it->ref==1&&dt->ref==1&&st->ref==1,"geometry Types retired");freetype(it);freetype(dt);freetype(st);lgcheck(!live&&!modules&&!nlinkpins&&!worker.nerr&&Tmodlink.ref==1&&Tstring.ref==1&&Tarray.ref==1,"complete geometry module/frames/roots retirement");lgcheck(rootpgrp.r.ref==1&&rootfgrp.r.ref==1&&rootegrp.r.ref==1,"geometry environment refs retired");memset(nativeobservations,0,sizeof(nativeobservations));
}
static void lgmetadata(void)
{
 Native64ModuleSpec spec;Native64ModuleFunction f[27];Native64TypedStep code[64],*s;Native64ModuleOwner owner={0};REG saved=R;uchar fmap[32],dmap[32],*map;uint fn,pc,k,width,mode,bit,offset,total=0;int before;
 for(fn=0;fn<27;fn++)for(pc=0;pc<full_functions[fn].count;pc++)if(full_functions[fn].code[pc].returned>=208&&full_functions[fn].code[pc].returned<=212)for(k=0;k<14;k++){
  spec=full_spec;memmove(f,full_functions,sizeof(f));spec.functions=f;require(f[fn].count<=nelem(code)&&f[fn].mapbytes<=sizeof(fmap)&&spec.mapbytes<=sizeof(dmap));memmove(code,f[fn].code,f[fn].count*sizeof(*code));f[fn].code=code;memmove(fmap,f[fn].map,f[fn].mapbytes);f[fn].map=fmap;memmove(dmap,spec.datamap,spec.mapbytes);spec.datamap=dmap;s=code+pc;width=s->m&255;
  switch(k){case 0:s->op=IMOVMP;break;case 1:s->m=s->returned==212?12:(4<<8)|width;break;case 2:s->s+=2;break;case 3:s->d+=2;break;case 4:s->mode&=~ARM;break;case 5:s->m|=1L<<40;break;case 6:s->s=(USRC(s->mode)&~AIND)==AFP?f[fn].bytes:spec.databytes;break;case 7:s->d=(UDST(s->mode)&~AIND)==AFP?f[fn].bytes:spec.databytes;break;case 8:s->mode=SRC(AIMM)|(s->mode&ARM)|DST(UDST(s->mode));break;case 9:s->mode=SRC(USRC(s->mode))|(s->mode&ARM)|DST(AIMM);break;default:mode=k<12?USRC(s->mode):UDST(s->mode);offset=k<12?s->s:s->d;if(!(mode&AIND)&&(k==11||k==13))offset+=width-1;bit=offset/8;map=(mode&~AIND)==AFP?fmap:dmap;map[bit/8]^=128>>(bit%8);break;}
  before=allocations;lgcheck(!native64typedmodule(&spec,0,&owner),"malformed local geometry metadata refused");lgcheck(allocations==before&&!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"geometry refusal before allocation");total++;
 }
 printf("PASS local geometry metadata: %u opcode/class/span/owner/raw-map refusals before allocation\n",total);
}
static void lgconstruction(void)
{
 Native64ModuleOwner owner={0};REG saved=R;int jit,i,n,base;
 for(jit=0;jit<2;jit++){base=allocations;lgcheck(native64typedmodule(&full_spec,jit,&owner),"constructor baseline");n=allocations-base;lgcheck(native64typedmoduleclear(&owner),"constructor cleared");for(i=1;i<=n;i++){failat=allocations+i;lgcheck(!native64typedmodule(&full_spec,jit,&owner),"constructor failure");failat=0;lgcheck(!live&&!owner.module&&!memcmp(&R,&saved,sizeof(R)),"constructor rollback");}printf("PASS geometry constructor: jit=%d %d allocation failures with complete rollback\n",jit,n);}
}
int main(void)
{
 uint fn,total=0;int jit,q,profile;int quanta[]={1,3,1000};rootpgrp.r.ref=rootfgrp.r.ref=rootegrp.r.ref=1;
 worker.defenv.pgrp=&rootpgrp;worker.defenv.fgrp=&rootfgrp;worker.defenv.egrp=&rootegrp;worker.defenv.errstr=worker.defenv.errbuf0;worker.defenv.syserrstr=worker.defenv.errbuf1;worker.env=&worker.defenv;worker.type=Interp;Tstring.free=freeheap;Tarray.free=freearray;require(dis64selectlaunch("dis64-calls"));
 GEOMETRY_HANDLERS
 optab[IMOVP]=actual_movp;optab[IMOVM]=actual_movm;lgmetadata();lgconstruction();for(fn=0;fn<27;fn++)for(jit=0;jit<2;jit++)for(q=0;q<3;q++)for(profile=0;profile<6;profile++){lgcase(fn,jit,quanta[q],profile);total++;}
 printf("PASS local Image geometry: %u C/JIT runs, Rect/Point read/write, FP/MP raw copies, swaps/alias, signed Word patterns, adjacent roots/bytes, nil-owner unwind and complete retirement\n",total);return 0;
}
