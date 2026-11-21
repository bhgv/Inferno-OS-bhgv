#include "../include/native64-library-layout.h"
/* Canonical Draw.Wmcontext: five channels followed by FD and Context roots. */
static int
typedwmcontextrecord(Type*t)
{static char*names[]={"kbd","ptr","ctl","wctl","images","connfd","ctxt"};Dis64Layout l={0};Decl*d;unsigned int i;int ok=0;
 if(!t||t->kind!=Tadt||!t->decl||!t->decl->sym||strcmp(t->decl->sym->name,"Wmcontext")||!t->decl->dot||!t->decl->dot->sym||strcmp(t->decl->dot->sym->name,"Draw")||!dis64layouttype(t,&l)||l.legacybytes!=28||l.bytes!=56||l.count!=7||l.alignment!=8)goto done;
 for(i=0,d=t->ids;i<7;i++,d=d->next){Type*v;if(!d||d->store!=Dfield||!d->sym||strcmp(d->sym->name,names[i])||d->offset!=4*i||!(v=d->ty)||v->kind!=(i<5?Tchan:Tref)||l.leaves[i].legacy!=4*i||l.leaves[i].native!=8*i||l.leaves[i].oldbytes!=4||l.leaves[i].bytes!=8||l.leaves[i].kind!=v->kind)goto done;
  if(i<5){v=v->tof;if(!v||(i==0?v->kind!=Tint:i==1?!typedwmref(v,"Draw","Pointer"):i==4?!typedwmref(v,"Draw","Image"):v->kind!=Tstring))goto done;}else if(!typedwmref(v,i==5?"Sys":"Draw",i==5?"FD":"Context"))goto done;
 }if(d)goto done;ok=1;
 done:dis64layoutfree(&l);return ok;}
/* Private Wmlib.Client is distinct from Wmsrv.Client even when dot is nil. */
static int
typedwmlibclient(Type*t)
{static char*names[]={"ptrpid","kbdpid","ctlpid","req","dir","ctlfd","winfd"};Dis64Layout l={0};Decl*d,*a;unsigned int i,kind;int ok=0;
 if(!t||t->kind!=Tadt||!t->decl||!t->decl->sym||strcmp(t->decl->sym->name,"Client")||!((t->decl->dot&&t->decl->dot->sym&&!strcmp(t->decl->dot->sym->name,"Wmlib"))||(!t->decl->dot&&impdecl&&impdecl->sym&&!strcmp(impdecl->sym->name,"Wmlib")))||!dis64layouttype(t,&l)||l.legacybytes!=28||l.bytes!=48||l.count!=7||l.alignment!=8)goto done;
 for(i=0,d=t->ids;i<7;i++,d=d->next){Type*v;if(!d||d->store!=Dfield||!d->sym||strcmp(d->sym->name,names[i])||d->offset!=4*i||!(v=d->ty))goto done;kind=i<3?Tint:i==3?Tchan:i==4?Tstring:Tref;if(v->kind!=kind||l.leaves[i].legacy!=4*i||l.leaves[i].native!=native64libraryoffset(12,i)||l.leaves[i].bytes!=native64librarywidth(12,i)||l.leaves[i].oldbytes!=4||l.leaves[i].kind!=kind)goto done;
  if(i==3){v=v->tof;if(!v||v->kind!=Ttuple||!(a=v->ids)||a->store!=Dfield||a->offset||!a->ty||a->ty->kind!=Tarray||!a->ty->tof||a->ty->tof->kind!=Tbyte||!(a=a->next)||a->next||a->store!=Dfield||a->offset!=4||!a->ty||a->ty->kind!=Tchan||!a->ty->tof||a->ty->tof->kind!=Ttuple)goto done;
   a=a->ty->tof->ids;if(!a||a->store!=Dfield||a->offset||!a->ty||a->ty->kind!=Tint||!(a=a->next)||a->next||a->store!=Dfield||a->offset!=4||!a->ty||a->ty->kind!=Tstring)goto done;
  }else if(i>=5&&!typedwmref(v,"Sys","FD"))goto done;
 }if(d)goto done;ok=1;
 done:dis64layoutfree(&l);return ok;}
static unsigned int
typedlibraryrecord(Type*t)
{
 static char*names[][9]={{"fd","buffer","index","size","dirty","bufpos","filpos","lastop","mode"},{"h","t","n"},{"h","t","n"},{"last","h","t"},{"iobuf","fill","next"}};
 Dis64Layout l={0};Decl*d;unsigned int id=0,i=0,k,n;int ok=0;Type ref;
 if(t && t->kind==Tref)t=t->tof;if(!t || t->kind!=Tadt || !t->decl || !t->decl->sym)return 0;
 if(!strcmp(t->decl->sym->name,"Toplevel")){unsigned int j;int kinds[]={Tref,Tchan,Tref,Tref,Tint,Tint,Tint,Tint};if(!t->decl->dot||!t->decl->dot->sym||strcmp(t->decl->dot->sym->name,"Tk")||!dis64layouttype(t,&l)||l.legacybytes!=32||l.bytes!=48||l.count!=8||l.alignment!=8){dis64layoutfree(&l);return 0;}for(j=0;j<8;j++)if(l.leaves[j].legacy!=4*j||l.leaves[j].native!=native64libraryoffset(10,j)||l.leaves[j].bytes!=native64librarywidth(10,j)||l.leaves[j].oldbytes!=4||l.leaves[j].kind!=kinds[j])break;dis64layoutfree(&l);return j==8?10:0;}
 if(!strcmp(t->decl->sym->name,"Wmcontext"))return typedwmcontextrecord(t)?8:0;
 if(!strcmp(t->decl->sym->name,"Point")){ok=typedwordgeometry(t,&l)&&l.bytes==8;dis64layoutfree(&l);return ok?9:0;}
 if(!strcmp(t->decl->sym->name,"Client")){if(typedwmlibclient(t))return 12;ok=typedclientlayout(t,&l);dis64layoutfree(&l);return ok?6:0;}
 if(!strcmp(t->decl->sym->name,"Window")){memset(&ref,0,sizeof(ref));ref.kind=Tref;ref.tof=t;return typedwmsrvwindow(&ref)?7:0;}
 if(!strcmp(t->decl->sym->name,"Iobuf"))id=1;else if(!strcmp(t->decl->sym->name,"Iqueue"))id=2;else if(!strcmp(t->decl->sym->name,"Squeue"))id=3;else if(!strcmp(t->decl->sym->name,"Ptrqueue"))id=4;else if(!strcmp(t->decl->sym->name,"Filler"))id=5;
 if(!id || !dis64layouttype(t,&l) || l.bytes!=native64librarybytes(id) || l.count!=(n=native64libraryfields(id)))goto done;
 for(d=t->ids;d;d=d->next){if(d->store!=Dfield)continue;if(i>=n || !d->sym || strcmp(d->sym->name,names[id-1][i]) || !d->ty)goto done;
  k=id==1?(i==0?Tref:i==1?Tarray:i==5||i==6?Tbig:Tint):id<=3?(i<2?Tlist:Tint):id==4?(i==0?Tref:Tlist):i==1?Tmodule:Tref;
  if(d->ty->kind!=k || d->offset!=native64libraryold(id,i) || l.leaves[i].native!=native64libraryoffset(id,i) || l.leaves[i].bytes!=native64librarywidth(id,i) || l.leaves[i].kind!=k)goto done;
  if(id==1&&i==1&&(!d->ty->tof || d->ty->tof->kind!=Tbyte))goto done;
  if(id<=3&&id>=2&&i<2&&(!d->ty->tof || d->ty->tof->kind!=(id==2?Tint:Tstring)))goto done;i++;
 }
 ok=i==n;
 done:dis64layoutfree(&l);return ok?id:0;
}
static int
typedlibraryoperand(int mode,Addr*a,int kind,Dis64TypedBody*p,long*value,unsigned int*wire,int*actual,unsigned int*id)
{
 unsigned int i,base;long owner;Addr copy;Type*t;
 *id=0;if(mode!=Afpind && mode!=Ampind)return typedoperand(mode,a,kind,kind==Tint,0,p,value,wire,actual);
 t=typedfieldowner(a);if(!(*id=typedlibraryrecord(t)))return 0;
 for(i=0;i<native64libraryfields(*id);i++)if(a->offset==(long)native64libraryold(*id,i))break;
 if(i==native64libraryfields(*id) || native64libraryroot(*id,i)!=(kind==TypedRoot) || native64librarywidth(*id,i)!=(kind==TypedRoot||kind==Tbig||kind==Treal?8:4))return 0;
 copy=*a;copy.offset=0;copy.native64type=t;if(!typedoperand(mode==Afpind?Afp:Amp,&copy,TypedRoot,0,0,p,&owner,wire,actual) || owner>65535)return 0;
 base=native64libraryoffset(*id,i);*value=((long)base<<16)|owner;*wire|=AIND;*actual=kind;return 1;
}
static int
typedlibraryinst(Inst*in,Dis64TypedBody*p,unsigned int pc,unsigned int stop)
{
 int sk=Tint,dk=Tint,binary=0,branch=0,actual,rootcompare=0;long source,middle=0,dest;unsigned int sm,dm,mm=AXNON,sid,did,id;Type*t;Inst copy=*in;
 if(in->op==IMOVMP&&in->mm==Adesc&&in->m.decl&&typedlibraryrecord(in->m.decl->ty)==8){Addr a,b;long src,dst;unsigned int sm,dm;int k;
  if(in->branch||(in->sm!=Afpind&&in->sm!=Ampind)||(in->dm!=Afpind&&in->dm!=Ampind)||in->s.offset||in->d.offset||!in->m.decl->desc||in->m.decl->desc->id!=in->m.offset||in->m.decl->desc->size!=28||in->m.decl->desc->nmap!=1||in->m.decl->desc->map[0]!=0xfe||!typedwmref(typedfieldowner(&in->s),"Draw","Wmcontext")||!typedwmref(typedfieldowner(&in->d),"Draw","Wmcontext"))return 0;
  a=in->s;b=in->d;a.native64type=typedfieldowner(&a);b.native64type=typedfieldowner(&b);if(!typedoperand(in->sm==Afpind?Afp:Amp,&a,TypedRoot,0,0,p,&src,&sm,&k)||!typedoperand(in->dm==Afpind?Afp:Amp,&b,TypedRoot,0,0,p,&dst,&dm,&k))return 0;
  p->code[pc]=(Dis64Instruction){IMOVMP,SRC(sm|AIND)|AXIMM|DST(dm|AIND),8,src,dst};p->returns[pc]=90;return 1;
 }
 if(in->op==INEW || in->op==INEWZ){
  if(in->sm!=Adesc || !in->s.decl || !(id=typedlibraryrecord(in->s.decl->ty)) || in->mm!=Anone || in->branch)return 0;
  if(in->dm==Afpind){if(p->resultkind!=Tref || in->d.reg!=16 || in->d.offset)return 0;dest=32;dm=AFP|AIND;}
  else if(!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&dest,&dm,&actual))return 0;
  p->code[pc]=(Dis64Instruction){INEWZ,SRC(AIMM)|DST(dm),0,id,dest};p->returns[pc]=154;return 1;
 }
 if(in->sm!=Afpind && in->sm!=Ampind && in->dm!=Afpind && in->dm!=Ampind)return 0;
 if(!typedlibraryrecord(typedfieldowner(&in->s)) && !typedlibraryrecord(typedfieldowner(&in->d)))return 0;
 switch(in->op){
 case IMOVP:sk=dk=TypedRoot;break;case ISLICEA:dk=TypedRoot;binary=1;break;case ISLICELA:sk=dk=TypedRoot;binary=1;break;case ILENA:case ILENL:case ILENC:case IHEADW:sk=TypedRoot;break;
 case IHEADP:case ITAIL:case ICONSP:sk=dk=TypedRoot;break;case ICONSW:dk=TypedRoot;break;
 case INEWA:case INEWAZ:if(in->mm!=Adesc || !in->m.decl || !in->m.decl->ty || in->m.decl->ty->kind!=Tbyte)return 0;copy.op=INEWAZ;dk=TypedRoot;break;
 case IMOVW:break;case IMOVL:sk=dk=Tbig;break;case IMOVF:sk=dk=Treal;break;
 case ICVTWL:dk=Tbig;break;case ICVTLW:sk=Tbig;break;
 case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:binary=1;break;
 case IADDL:case ISUBL:case IMULL:case IDIVL:case IMODL:sk=dk=Tbig;binary=1;break;
 case IBEQC:case IBNEC:sk=TypedRoot;rootcompare=1;branch=binary=1;break;
 case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:branch=binary=1;break;
 case IBEQL:case IBNEL:case IBLTL:case IBLEL:case IBGTL:case IBGEL:sk=Tbig;branch=binary=1;break;
 default:return 0;
 }
 if(branch&&(in->op==IBEQW||in->op==IBNEW||in->op==IBEQL||in->op==IBNEL)){
  t=in->s.native64type?in->s.native64type:in->s.decl?in->s.decl->ty:nil;
  if(t && (t->kind==Tref||t->kind==Tarray||t->kind==Tlist||t->kind==Tmodule||t->kind==Tchan||t->kind==Tstring)){rootcompare=1;sk=TypedRoot;copy.op=in->op==IBEQW?IBEQL:in->op==IBNEW?IBNEL:in->op;}
 }
 if(!typedlibraryoperand(in->sm,&in->s,sk,p,&source,&sm,&actual,&sid))return 0;
 if(branch){if(in->dm!=Apc || !in->branch || in->d.offset<(long)p->first || in->d.offset>=stop)return 0;dest=in->d.offset-p->first;dm=AIMM;did=0;}
 else if(in->dm==Afpind && in->d.reg==16 && !in->d.offset && (dk==p->resultkind || (dk==TypedRoot && typedclientreferencekind(p->resultkind)))){dest=32;dm=AFP|AIND;did=0;}
 else if(!typedlibraryoperand(in->dm,&in->d,dk,p,&dest,&dm,&actual,&did))return 0;
 if(binary&&in->mm!=Anone){unsigned int wire;int mk=rootcompare?TypedRoot:sk==Tbig?Tbig:Tint;if(!typedoperand(in->mm,&in->m,mk,mk==Tint,1,p,&middle,&wire,&actual))return 0;mm=wire==AIMM?AXIMM:wire==AFP?AXINF:AXINM;}
 else if(copy.op==INEWAZ)mm=AXIMM;
 else if(in->mm!=Anone || (branch&&in->mm==Anone))return 0;
 if(!sid&&!did)return 0;
 p->code[pc]=(Dis64Instruction){copy.op,SRC(sm)|mm|DST(dm),((long)sid<<24)|((long)did<<16)|(middle&65535),source,dest};p->returns[pc]=153;return 1;
}

static int
typedlibraryargument(Inst*in,Inst*frame,Dis64TypedBody*p,unsigned int pc,unsigned int old,unsigned int native,int kind)
{
 unsigned int sm,sid;long value;int actual,root=typedclientreferencekind(kind);
 if(in->sm==Afp && kind==Tarray && typedbytearrayaddr(&in->s) && in->op==IMOVP && in->mm==Anone && !in->branch && in->dm==Afpind && in->d.reg==frame->d.reg && in->d.offset==old && typedoperand(Afp,&in->s,Tarray,0,0,p,&value,&sm,&actual)){
  p->code[pc]=(Dis64Instruction){IMOVP,SRC(AFP)|DST(AFP|AIND),0,value,native};p->returns[pc]=155;return 1;
 }
 if((in->sm!=Afpind && in->sm!=Ampind) || !typedlibraryrecord(typedfieldowner(&in->s)) || in->op!=(root?IMOVP:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW) || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=old || !typedlibraryoperand(in->sm,&in->s,root?TypedRoot:kind,p,&value,&sm,&actual,&sid))return 0;
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|DST(AFP|AIND),(long)sid<<24,value,native};p->returns[pc]=155;return 1;
}

static int
typedlibrarymodule(Inst*frame,Dis64TypedBody*p,long*value,unsigned int*mode,int*actual)
{unsigned int id;if((frame->sm!=Afpind&&frame->sm!=Ampind)||typedlibraryrecord(typedfieldowner(&frame->s))!=5||frame->s.offset!=4||!typedlibraryoperand(frame->sm,&frame->s,TypedRoot,p,value,mode,actual,&id))return 0;*value|=5L<<24;*actual=Tmodule;return 1;}

/* A byte borrow permits one pure Word shift before its single consumer. */
static int
typedlibrarybyteindex(Inst*in,Inst*use,Dis64TypedBody*p,unsigned int pc,unsigned int gap)
{
 long source,index,value;unsigned int sm,dm,vm,sid=0,did=0;int actual,read;Addr*a;
 if(in->op!=IINDB || (!typedbytearrayaddr(&in->s) && !(typedlibraryrecord(typedfieldowner(&in->s))==1 && in->s.offset==4)) || in->mm!=Afp || !in->m.decl || in->m.offset || in->branch || !typedlibraryoperand(in->sm,&in->s,TypedRoot,p,&source,&sm,&actual,&sid) || !typedlibraryoperand(in->dm,&in->d,Tint,p,&index,&dm,&actual,&did) || !use || use->branch || use->mm!=Anone)return typedfail(p,"library-byte-index");
 read=use->sm==Afpind;a=read?&use->s:&use->d;
 if((!read&&use->dm!=Afpind) || a->decl!=in->m.decl || a->reg!=in->m.reg || a->offset || (read?(use->op!=IMOVB&&use->op!=ICVTBW):(use->op!=IMOVB&&use->op!=ICVTWB)))return typedfail(p,"library-byte-consumer");
 if(read && use->dm==Afpind && use->d.reg==16 && !use->d.offset && use->op==ICVTBW && p->resultkind==Tint){value=32;vm=AFP|AIND;}
 else if(!typedoperand(read?use->dm:use->sm,read?&use->d:&use->s,use->op==IMOVB?Tbyte:Tint,!read,0,p,&value,&vm,&actual))return 0;
 p->code[pc]=(Dis64Instruction){IINDB,SRC(sm)|AXINF|DST(dm),((long)sid<<24)|((long)did<<16)|((long)gap<<8)|48,source,index};p->returns[pc]=157;
 p->code[pc+1+gap]=(Dis64Instruction){use->op,read?SRC(AFP|AIND)|DST(vm):SRC(vm)|DST(AFP|AIND),0,read?48:value,read?value:48};p->returns[pc+1+gap]=158;return 1;
}

static int
typedlibraryslicearg(Inst*in,Inst*frame,Dis64TypedBody*p,unsigned int pc,unsigned int old,unsigned int native)
{long source,middle;unsigned int sm,mm,id;int actual;
 if(in->op!=ISLICEA || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=old || !typedlibraryoperand(in->sm,&in->s,Tint,p,&source,&sm,&actual,&id) || !typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual))return 0;
 p->code[pc]=(Dis64Instruction){ISLICEA,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(AFP|AIND),((long)id<<24)|(middle&65535),source,native};p->returns[pc]=156;return 1;}

static int
typedlibrarybytecopy(Inst*in,Dis64TypedBody*p,unsigned int pc)
{long source,middle,dest;unsigned int sm,mm,dm;int actual;
 if(in->op!=ISLICELA || !typedbytearrayaddr(&in->s) || !typedbytearrayaddr(&in->d) || in->branch || !typedoperand(in->sm,&in->s,Tarray,0,0,p,&source,&sm,&actual) || !typedoperand(in->mm,&in->m,Tint,1,1,p,&middle,&mm,&actual) || !typedoperand(in->dm,&in->d,Tarray,0,0,p,&dest,&dm,&actual))return typedfail(p,"library-byte-copy");
 p->code[pc]=(Dis64Instruction){ISLICELA,SRC(sm)|(mm==AIMM?AXIMM:mm==AFP?AXINF:AXINM)|DST(dm),middle,source,dest};p->returns[pc]=159;return 1;}

/* Store a Window's mapped Image root into the immediately prepared array slot. */
static int
typedlibraryarraystore(Inst*in,Dis64TypedBody*p,unsigned int pc)
{long source;unsigned int sm,id;int actual;
 if(in->op!=IMOVP||in->mm!=Anone||in->branch||in->dm!=Afpind||!typedlibraryoperand(in->sm,&in->s,TypedRoot,p,&source,&sm,&actual,&id)||id!=7||in->s.offset!=20)return typedfail(p,"window-array-store");
 p->code[pc]=(Dis64Instruction){IMOVP,SRC(sm)|DST(AFP|AIND),7L<<24,source,48};p->returns[pc]=142;return 1;}

static int
typedwindowrecord(Type*t,Dis64Layout*l)
{unsigned int i;if(t&&t->kind==Tref)t=t->tof;if(!t||t->kind!=Tadt||!t->decl||!t->decl->sym||strcmp(t->decl->sym->name,"Window")||!t->decl->dot||!t->decl->dot->sym||strcmp(t->decl->dot->sym->name,"Wmclient")||!dis64layouttype(t,l)||l->legacybytes!=104||l->bytes!=128||l->alignment!=8||l->count!=26)return 0;for(i=0;i<26;i++)if(l->leaves[i].legacy!=4*i||l->leaves[i].native!=native64libraryoffset(11,i)||l->leaves[i].oldbytes!=4||l->leaves[i].bytes!=native64librarywidth(11,i)||l->leaves[i].kind!=(native64libraryroot(11,i)?i==13?Tchan:Tref:Tint))return 0;return 1;}
static int
typedwmlibclone(Inst*in,Dis64TypedBody*p,unsigned int pc)
{Decl*d=in->m.decl;Dis64Layout l={0};unsigned int first,base,i,dm;long dest;int actual,ok=0;Addr a;
 if(in->op!=IMOVMP||in->sm!=Amp||in->mm!=Adesc||in->dm!=Afpind||in->d.offset||in->branch||!d||!d->desc||d->desc->id!=in->m.offset||d->desc->size!=28||d->desc->nmap!=1||d->desc->map[0]!=0x1e||!typedwmlibclient(d->ty)||!dis64layouttype(d->ty,&l)||!in->s.decl||in->s.reg!=in->s.decl->offset||in->s.offset||typedlibraryrecord(typedfieldowner(&in->d))!=12)goto done;
 for(first=0;first<p->data.count;first++)if(p->data.leaves[first].legacy==(unsigned int)in->s.reg)break;if(first>p->data.count||7>p->data.count-first)goto done;base=p->data.leaves[first].native;for(i=0;i<7;i++){Dis64LayoutLeaf*v=p->data.leaves+first+i;if(v->legacy!=in->s.reg+l.leaves[i].legacy||v->native!=base+l.leaves[i].native||v->kind!=l.leaves[i].kind||v->bytes!=l.leaves[i].bytes||v->oldbytes!=4)goto done;}
 a=in->d;a.offset=0;a.native64type=typedfieldowner(&a);if(!typedoperand(Afp,&a,TypedRoot,0,0,p,&dest,&dm,&actual)||actual!=Tref||!pc||p->returns[pc-1]!=154||p->code[pc-1].source!=12||p->code[pc-1].destination!=dest)goto done;
 p->code[pc]=(Dis64Instruction){IMOVMP,SRC(AMP)|AXIMM|DST(AFP|AIND),12,base,dest};p->returns[pc]=68;ok=1;
 done:dis64layoutfree(&l);return ok;}
static int
typedwindowconstruct(Inst*in,Dis64TypedBody*p,unsigned int pc)
{Dis64Layout l={0};Decl*d;long dest;unsigned int dm,i,first,base;int actual,ok=0;
 if(typedwmlibclone(in,p,pc))return 1;
 if(in->op==INEW||in->op==INEWZ){d=in->s.decl;if(in->sm!=Adesc||in->mm!=Anone||in->branch||!d||!d->desc||d->desc->id!=in->s.offset||d->desc->size!=104||d->desc->nmap!=3||d->desc->map[0]!=0x84||d->desc->map[1]!=0x26||d->desc->map[2]!=4||!typedwindowrecord(d->ty,&l)||!typedoperand(in->dm,&in->d,TypedRoot,0,0,p,&dest,&dm,&actual)||dm!=AFP||actual!=Tref)goto done;p->code[pc]=(Dis64Instruction){INEWZ,SRC(AIMM)|DST(AFP),0,11,dest};p->returns[pc]=154;ok=1;goto done;}
 if(in->op!=IMOVMP||in->sm!=Amp||in->mm!=Adesc||in->dm!=Afpind||in->d.offset||in->branch||!(d=in->m.decl)||!d->desc||d->desc->id!=in->m.offset||d->desc->size!=104||d->desc->nmap!=3||d->desc->map[0]!=0x84||d->desc->map[1]!=0x26||d->desc->map[2]!=4||!typedwindowrecord(d->ty,&l)||!in->s.decl||in->s.reg!=in->s.decl->offset||in->s.offset||!typedwindowowner(&in->d))goto done;
 for(first=0;first<p->data.count;first++)if(p->data.leaves[first].legacy==(unsigned int)in->s.reg)break;if(first>p->data.count||26>p->data.count-first)goto done;base=p->data.leaves[first].native;for(i=0;i<26;i++){Dis64LayoutLeaf*v=p->data.leaves+first+i;if(v->legacy!=in->s.reg+l.leaves[i].legacy||v->native!=base+l.leaves[i].native||v->kind!=l.leaves[i].kind||v->bytes!=l.leaves[i].bytes||v->oldbytes!=4)goto done;}
 {Addr a=in->d;a.offset=0;a.native64type=typedfieldowner(&a);if(!typedoperand(Afp,&a,TypedRoot,0,0,p,&dest,&dm,&actual)||actual!=Tref||!pc||p->returns[pc-1]!=154||p->code[pc-1].source!=11||p->code[pc-1].destination!=dest)goto done;}
 p->code[pc]=(Dis64Instruction){IMOVMP,SRC(AMP)|AXIMM|DST(AFP|AIND),11,base,dest};p->returns[pc]=68;ok=1;
 done:dis64layoutfree(&l);return ok;}
