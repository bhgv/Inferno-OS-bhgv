#include "../include/native64-library-layout.h"
static unsigned int
typedlibraryrecord(Type*t)
{
 static char*names[][9]={{"fd","buffer","index","size","dirty","bufpos","filpos","lastop","mode"},{"h","t","n"},{"h","t","n"},{"last","h","t"},{"iobuf","fill","next"}};
 Dis64Layout l={0};Decl*d;unsigned int id=0,i=0,k,n;int ok=0;
 if(t && t->kind==Tref)t=t->tof;if(!t || t->kind!=Tadt || !t->decl || !t->decl->sym)return 0;
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
 if((in->sm!=Afpind && in->sm!=Ampind) || !typedlibraryrecord(typedfieldowner(&in->s)) || in->op!=(root?IMOVP:kind==Tbig?IMOVL:kind==Treal?IMOVF:IMOVW) || in->mm!=Anone || in->branch || in->dm!=Afpind || in->d.reg!=frame->d.reg || in->d.offset!=old || !typedlibraryoperand(in->sm,&in->s,root?TypedRoot:kind,p,&value,&sm,&actual,&sid))return 0;
 p->code[pc]=(Dis64Instruction){in->op,SRC(sm)|DST(AFP|AIND),(long)sid<<24,value,native};p->returns[pc]=155;return 1;
}

static int
typedlibrarymodule(Inst*frame,Dis64TypedBody*p,long*value,unsigned int*mode,int*actual)
{unsigned int id;if((frame->sm!=Afpind&&frame->sm!=Ampind)||typedlibraryrecord(typedfieldowner(&frame->s))!=5||frame->s.offset!=4||!typedlibraryoperand(frame->sm,&frame->s,TypedRoot,p,value,mode,actual,&id))return 0;*value|=5L<<24;*actual=Tmodule;return 1;}

/* Borrowed byte address is consumed exactly once by the next instruction. */
static int
typedlibrarybyteindex(Inst*in,Inst*use,Dis64TypedBody*p,unsigned int pc)
{
 long source,index,value;unsigned int sm,dm,vm,sid=0,did=0;int actual,read;Addr*a;
 if(in->op!=IINDB || (!typedbytearrayaddr(&in->s) && !(typedlibraryrecord(typedfieldowner(&in->s))==1 && in->s.offset==4)) || in->mm!=Afp || !in->m.decl || in->m.offset || in->branch || !typedlibraryoperand(in->sm,&in->s,TypedRoot,p,&source,&sm,&actual,&sid) || !typedlibraryoperand(in->dm,&in->d,Tint,p,&index,&dm,&actual,&did) || !use || use->branch || use->mm!=Anone)return typedfail(p,"library-byte-index");
 read=use->sm==Afpind;a=read?&use->s:&use->d;
 if((!read&&use->dm!=Afpind) || a->decl!=in->m.decl || a->reg!=in->m.reg || a->offset || (read?(use->op!=IMOVB&&use->op!=ICVTBW):(use->op!=IMOVB&&use->op!=ICVTWB)))return typedfail(p,"library-byte-consumer");
 if(read && use->dm==Afpind && use->d.reg==16 && !use->d.offset && use->op==ICVTBW && p->resultkind==Tint){value=32;vm=AFP|AIND;}
 else if(!typedoperand(read?use->dm:use->sm,read?&use->d:&use->s,use->op==IMOVB?Tbyte:Tint,!read,0,p,&value,&vm,&actual))return 0;
 p->code[pc]=(Dis64Instruction){IINDB,SRC(sm)|AXINF|DST(dm),((long)sid<<24)|((long)did<<16)|48,source,index};p->returns[pc]=157;
 p->code[pc+1]=(Dis64Instruction){use->op,read?SRC(AFP|AIND)|DST(vm):SRC(vm)|DST(AFP|AIND),0,read?48:value,read?value:48};p->returns[pc+1]=158;return 1;
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
