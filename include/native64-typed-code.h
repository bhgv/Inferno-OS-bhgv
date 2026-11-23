#include "native64-fileio-layout.h"
#ifndef NATIVE64_TYPED_CODE_H
#define NATIVE64_TYPED_CODE_H
/* Trusted checked-IR materialization only; no bytecode/file admission. */
#include "native64-model.h"
#include "native64-geometry-result.h"
/* Private compiler annotations: exact selected Draw/Wmclient root fields. */
enum { Native64TypedContext = 96, Native64TypedContextNil = 97, Native64TypedWindow = 100, Native64TypedWindowNil = 101, Native64TypedDraw = 113, Native64TypedDrawNil = 114, Native64TypedFieldArg = 125 };
typedef struct Native64TypedStep Native64TypedStep;
struct Native64TypedStep {
	unsigned int op,mode;
	long s,m,d;
	unsigned int returned;
};

static int
native64typedbranch(unsigned int op)
{
	switch(op){
	case IJMP:
	case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:
	case IBEQL:case IBNEL:case IBLTL:case IBLEL:case IBGTL:case IBGEL:
	case IBEQF:case IBNEF:case IBLTF:case IBLEF:case IBGTF:case IBGEF:
    case IBEQC:case IBNEC:case IBLTC:case IBLEC:case IBGTC:case IBGEC:return 1;
	}
	return 0;
}

static int
native64typedcodeoperand(unsigned int mode,long offset,unsigned int fpbytes,unsigned int mpbytes)
{
	if(mode==AXXX)return offset==0;
	if(mode==AIMM)return offset>=-536870912L && offset<=536870911L;
	if(offset<0 || (mode!=AFP && mode!=AMP))return 0;
	return (unsigned long)offset<(mode==AFP?fpbytes:mpbytes) && (mode!=AFP || offset>=64);
}

static int
native64typedrootoperand(unsigned int mode,long offset,unsigned int fpbytes,unsigned int mpbytes)
{
 return (mode==AFP || mode==AMP) && native64typedcodeoperand(mode,offset,fpbytes,mpbytes) &&
  offset%8==0 && 8<=(mode==AFP?fpbytes:mpbytes)-offset;
}

static int
native64typedcontextcopy(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 return s->returned==173 && s->op==IMOVP && s->mode==(SRC(sm)|DST(dm)) &&
  (sm==(AFP|AIND) || sm==(AMP|AIND)) && (dm==(AFP|AIND) || dm==(AMP|AIND)) && (s->m==0 || s->m==8 || s->m==16) &&
  native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(dm&~AIND,s->d,fp,mp);
}

static int
native64typedcontextnew(const Native64TypedStep*s,unsigned int fp)
{
 return (s->op==INEW || s->op==INEWZ) && !s->s && !s->m &&
  ((s->returned==172 && s->mode==(SRC(AIMM)|DST(AFP)) && native64typedrootoperand(AFP,s->d,fp,0)) ||
   (s->returned==174 && s->mode==(SRC(AIMM)|DST(AFP|AIND)) && s->d==32));
}

static int
native64typedtriplecopy(const Native64TypedStep*s,unsigned int fp)
{
 return s->returned==171 && s->op==IMOVMP && s->mode==(SRC(AFP)|AXIMM|DST(AFP|AIND)) && !s->m && s->d==32 && s->s>=64 && s->s%8==0 && (unsigned long)s->s<=fp && 24<=fp-s->s;
}

static int
native64typedwordoperand(unsigned int mode,long offset,unsigned int fpbytes,unsigned int mpbytes)
{
 return native64typedcodeoperand(mode,offset,fpbytes,mpbytes) &&
  (mode==AIMM || ((mode==AFP || mode==AMP) && offset%4==0 && 4<=(mode==AFP?fpbytes:mpbytes)-offset));
}

#include "native64-library-code.h"
#include "native64-record-code.h"

static int
native64typedchannelop(unsigned int op)
{
 return op==INEWCB || op==INEWCW || op==INEWCL || op==INEWCF || op==INEWCP;
}
static int
native64typedchannel(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int mm=s->mode&ARM,dm=UDST(s->mode),mode=mm==AXIMM?AIMM:mm==AXINF?AFP:AMP;
 return native64typedchannelop(s->op) && s->mode<=255 && USRC(s->mode)==AXXX && !s->s &&
  (((s->returned==0 || (s->returned==55&&s->op==INEWCL)) && native64typedrootoperand(dm,s->d,fp,mp)) || (s->returned==24 && dm==(AFP|AIND) && s->d==32)) &&
  (mm==AXNON?!s->m:native64typedwordoperand(mode,s->m,fp,mp) && (mm!=AXIMM || (s->m>=-32768 && s->m<=32767)));
}

static int
native64typedwindowlist(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{return s->returned==213 && (s->op==ICONSP||s->op==IHEADP||s->op==ITAIL) && s->mode<=255 && (s->mode&ARM)==AXNON && !s->m && native64typedrootoperand(USRC(s->mode),s->s,fp,mp) && native64typedrootoperand(UDST(s->mode),s->d,fp,mp);}
static int
native64typedwmsrvwindow(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);int read=sm==(AFP|AIND)||sm==(AMP|AIND);
 return s->returned==214 && s->op==IMOVP && s->mode<=255 && (s->mode&ARM)==AXNON && (s->m==0||s->m==24) &&
  (read?native64typedrootoperand(sm&~AIND,s->s,fp,mp)&&native64typedrootoperand(dm,s->d,fp,mp):((dm==(AFP|AIND)||dm==(AMP|AIND))&&native64typedrootoperand(sm,s->s,fp,mp)&&native64typedrootoperand(dm&~AIND,s->d,fp,mp)));
}

static int
native64typedlocalgeometry(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),width=s->m&255,field=(unsigned long)s->m>>8,read=s->returned==208||s->returned==209||((s->returned==215||s->returned==216)&&(sm&AIND));unsigned int mode;long value,owner;
 if((s->returned<208 || s->returned>212) && s->returned!=215 && s->returned!=216)return 0;
 if(s->op!=IMOVM || s->mode>255 || (s->mode&ARM)!=AXIMM || s->m<0 || (unsigned long)s->m>0x2010UL)return 0;
 if(s->returned==212)return (sm==AFP||sm==AMP) && (dm==AFP||dm==AMP) && (s->m==8||s->m==16) && native64typedwordoperand(sm,s->s,fp,mp) && native64typedwordoperand(dm,s->d,fp,mp) && width<=(sm==AFP?fp:mp)-s->s && width<=(dm==AFP?fp:mp)-s->d;
 if(s->returned==216){if(width!=16 || field!=32)return 0;}
 else if(s->returned==215){if((width!=8&&width!=16) || (width==16?field!=8:(field!=0&&field!=4&&field!=8&&field!=16)))return 0;}
 else if(width!=((s->returned==208||s->returned==210)?16:8) || (width==16?(field!=0&&field!=16):(field>24||field%8)))return 0;
 mode=read?sm:dm;owner=read?s->s:s->d;value=read?s->d:s->s;
 return (mode==(AFP|AIND)||mode==(AMP|AIND)) && ((read?dm:sm)==AFP || (read?dm:sm)==AMP) && native64typedrootoperand(mode&~AIND,owner,fp,mp) && native64typedwordoperand(read?dm:sm,value,fp,mp) && width<=((read?dm:sm)==AFP?fp:mp)-value;
}

static int
native64typedinthead(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode);
 return s->returned==253 && !s->m && ((s->op==ITAIL || s->op==IHEADP)?s->mode==(SRC(sm)|DST(AFP)) && native64typedrootoperand(sm,s->s,fp,mp):s->op==IMOVP && s->mode==(SRC(AMP)|DST(AFP)) && native64typedrootoperand(AMP,s->s,fp,mp)) && native64typedrootoperand(AFP,s->d,fp,mp);
}

static int
native64typedheadprep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode);
 return s->returned==252 && !s->m && ((s->op==ITAIL && s->mode==(SRC(sm)|DST(AFP)) && native64typedrootoperand(sm,s->s,fp,mp)) ||
  (s->op==IMOVP && s->mode==(SRC(AMP)|DST(AFP)) && native64typedrootoperand(AMP,s->s,fp,mp))) && native64typedrootoperand(AFP,s->d,fp,mp);
}

static int
native64typedrectlist(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);long value;
 if(s->returned==250)return s->op==ITAIL && !s->m && s->mode==(SRC(sm)|DST(dm)) && native64typedrootoperand(sm,s->s,fp,mp) && native64typedrootoperand(dm,s->d,fp,mp);
 if(s->op==ICONSMP||s->op==IHEADMP){
  unsigned int cons=s->op==ICONSMP,mode=cons?sm:dm;
  long payload=cons?s->s:s->d;
  return s->returned==(cons?248:249)&&s->m==16&&s->mode==(SRC(sm)|AXIMM|DST(dm))&&
         native64typedrootoperand(cons?dm:sm,cons?s->d:s->s,fp,mp)&&
         native64typedrootoperand(mode,payload,fp,mp)&&native64typedrootoperand(mode,payload+8,fp,mp);
 }
 if((s->returned!=248 && s->returned!=249) || s->op!=(s->returned==248?ICONSM:IHEADM) || s->mode!=(SRC(sm)|AXIMM|DST(dm)) || s->m!=16)return 0;
 if(s->returned==248){if(!native64typedrootoperand(dm,s->d,fp,mp))return 0;if(sm&AIND)return (sm==(AFP|AIND)||sm==(AMP|AIND)) && native64typedrootoperand(sm&~AIND,s->s,fp,mp);value=s->s;}
 else{if(!native64typedrootoperand(sm,s->s,fp,mp))return 0;sm=dm;value=s->d;}
 return (sm==AFP || sm==AMP) && native64typedwordoperand(sm,value,fp,mp) && 16<=(sm==AFP?fp:mp)-value;
}

static int
native64typedgeometryreturn(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int mode=USRC(s->mode),width=s->returned==246?16:8;
 return (s->returned==246 || s->returned==247) && s->op==IMOVM && s->mode==(SRC(mode)|AXIMM|DST(AFP|AIND)) && (mode==AFP || mode==AMP) && s->m==width && s->d==32 &&
  native64typedwordoperand(mode,s->s,fp,mp) && width<=(mode==AFP?fp:mp)-s->s;
}

static int
native64typedpointerstore(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 return ((s->returned==243 && s->op==IMOVW) || (s->returned==244 && s->op==ICVTCW)) && s->mode==(SRC(sm)|DST(dm)) && (dm==(AFP|AIND) || dm==(AMP|AIND)) && s->m>=0 && s->m<=12 && s->m%4==0 &&
  native64typedrootoperand(dm&~AIND,s->d,fp,mp) && (s->returned==244?native64typedrootoperand(sm,s->s,fp,mp):native64typedwordoperand(sm,s->s,fp,mp));
}

static int
native64typedpointerword(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 if((s->returned!=228 && s->returned!=229) || s->mode>255 || (sm!=(AFP|AIND) && sm!=(AMP|AIND)) || !native64typedrootoperand(sm&~AIND,s->s,fp,mp) || s->m<0 || s->m>12 || s->m%4)return 0;
 if(s->returned==229)return (s->op==IBEQW || s->op==IBNEW) && s->mode==(SRC(sm)|AXIMM|DST(AIMM));
 return s->op==IMOVW && s->mode==(SRC(sm)|DST(dm)) && (dm==AFP || dm==AMP) && native64typedwordoperand(dm,s->d,fp,mp);
}

static int
native64typedpointerpair(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),mm=s->mode&ARM;long field=(unsigned long)s->m>>16,other=s->m&65535;
 if(s->returned==139)return s->op==IMOVM&&s->mode==(SRC(sm)|AXIMM|DST(dm))&&(sm==(AFP|AIND)||sm==(AMP|AIND))&&(dm==(AFP|AIND)||dm==(AMP|AIND))&&s->m==16&&native64typedrootoperand(sm&~AIND,s->s,fp,mp)&&native64typedrootoperand(dm&~AIND,s->d,fp,mp);
 return s->returned==138&&(s->op==IBEQW||s->op==IBNEW)&&s->mode<=255&&dm==AIMM&&(mm==AXINF||mm==AXINM)&&(sm==(AFP|AIND)||sm==(AMP|AIND))&&s->m>=0&&field<=12&&field%4==0&&native64typedrootoperand(sm&~AIND,s->s,fp,mp)&&native64typedwordoperand(mm==AXINF?AFP:AMP,other,fp,mp);}

static int
native64typedclientcompare(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),mm=s->mode&ARM;
 return s->returned==231 && s->m<=65535 && (s->op==IBEQL || s->op==IBNEL) && s->mode<=255 && UDST(s->mode)==AIMM && (mm==AXINF || mm==AXINM) &&
  native64typedrootoperand(sm,s->s,fp,mp) && native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fp,mp);
}

static int
native64typedcursorcompare(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),mm=s->mode&ARM;
 return s->returned==232 && s->m<=65535 && (s->op==IBEQC || s->op==IBNEC) && s->mode<=255 && UDST(s->mode)==AIMM && (sm==(AFP|AIND) || sm==(AMP|AIND)) && (mm==AXINF || mm==AXINM) &&
  native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fp,mp);
}

static int
native64typedflagsand(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 return s->returned==234 && s->op==IANDW && s->mode==(SRC(sm)|AXIMM|DST(dm)) && (sm==(AFP|AIND) || sm==(AMP|AIND)) && (dm==AFP || dm==AMP) &&
  s->m>=0 && (unsigned long)s->m>=0x200000UL && (unsigned long)s->m<=0x20ffffUL && native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedwordoperand(dm,s->d,fp,mp);
}

static int
native64typedflagsor(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int dm=UDST(s->mode);
 return s->returned==236 && (s->op==IORW || s->op==IANDW || s->op==IXORW) && s->mode==(SRC(AIMM)|DST(dm)) && (dm==(AFP|AIND) || dm==(AMP|AIND)) && s->m==32 && s->s>=-32768 && s->s<=32767 && native64typedrootoperand(dm&~AIND,s->d,fp,mp);
}

static int
native64typedidstring(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode);
 return s->returned==237 && s->op==ICVTWC && s->mode==(SRC(sm)|DST(AFP)) && (sm==(AFP|AIND)||sm==(AMP|AIND)) && s->m==72 && native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(AFP,s->d,fp,mp);
}

static int
native64typedchannelnil(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),field=(unsigned long)s->m>>16;long other=(s->returned==241 || s->returned==251)?s->m&65535:s->m;
 if((s->returned!=239 && s->returned!=241 && s->returned!=251) || (s->op!=IBEQL && s->op!=IBNEL) || s->mode!=(SRC(sm)|AXINM|DST(AIMM)) || s->m<0)return 0;
 if(s->returned==241 || s->returned==251){if((s->returned==251?field!=48:(field!=0&&field!=8&&field!=16&&field!=24&&field!=64)) || (unsigned long)s->m>0x40ffffUL || (sm!=(AFP|AIND)&&sm!=(AMP|AIND)))return 0;sm&=~AIND;}
 else if(s->m>65535)return 0;
 return native64typedrootoperand(sm,s->s,fp,mp) && native64typedrootoperand(AMP,other,fp,mp);
}

static int
native64typedfieldreference(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),mm=(s->mode&ARM)==AXINF?AFP:AMP,family=(unsigned long)s->m>>24,field=((unsigned long)s->m>>16)&255,other=s->m&65535;int valid=0;
 if(s->returned!=207 || (s->op!=IBEQL&&s->op!=IBNEL) || s->mode>255 || UDST(s->mode)!=AIMM || ((s->mode&ARM)!=AXINF&&(s->mode&ARM)!=AXINM) || (sm!=(AFP|AIND)&&sm!=(AMP|AIND)) || s->m<0 || (unsigned long)s->m>0x04ffffffUL)return 0;
 if(family==1)valid=field==0||field==8||field==16||field==24||field==32||field==48||field==56;
 else if(family==2)valid=field==0||field==24||field==48||field==72||field==104;
 else if(family==3)valid=field==0||field==8;
 else if(family==4)valid=field==56||field==88;
 return valid && native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(mm,other,fp,mp);
}

static int
native64typedclientfield(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),owner,value;long base,offset;int read=sm==(AFP|AIND)||sm==(AMP|AIND),root=s->returned==205;
 if((s->returned!=205 && s->returned!=206) || s->op!=(root?IMOVP:IMOVW) || s->mode>255 || (s->mode&ARM)!=AXNON)return 0;
 if(root?(s->m!=0&&s->m!=8&&s->m!=16&&s->m!=24&&s->m!=40&&s->m!=48&&s->m!=56&&s->m!=64&&s->m!=88):(s->m!=32&&s->m!=72&&s->m!=76&&s->m!=80))return 0;
 owner=read?sm:dm;value=read?dm:sm;base=read?s->s:s->d;offset=read?s->d:s->s;
 if((owner!=(AFP|AIND)&&owner!=(AMP|AIND)) || !native64typedrootoperand(owner&~AIND,base,fp,mp))return 0;
 if(root)return native64typedrootoperand(value,offset,fp,mp);
 return (value==AIMM?!read:native64typedwordoperand(value,offset,fp,mp)) && (value!=AIMM || (offset>=-536870912L&&offset<=536870911L));
}

static int
native64typedcursor(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),vm,cm;long value,channel;
 if(s->returned==50)return s->op==IMOVMP&&s->mode==(SRC(sm)|AXIMM|DST(dm))&&!s->m&&native64typedrootoperand(sm,s->s,fp,mp)&&native64typedrootoperand(dm,s->d,fp,mp)&&24<=(sm==AFP?fp:mp)-s->s&&24<=(dm==AFP?fp:mp)-s->d;
 if(s->returned!=52||(s->op!=ISEND&&s->op!=IRECV)||s->m!=152||s->mode!=(SRC(sm)|DST(dm)))return 0;vm=s->op==ISEND?sm:dm;cm=s->op==ISEND?dm:sm;value=s->op==ISEND?s->s:s->d;channel=s->op==ISEND?s->d:s->s;return native64typedrootoperand(cm,channel,fp,mp)&&native64typedrootoperand(vm,value,fp,mp)&&24<=(vm==AFP?fp:mp)-value;}

static int
native64typedtransfer(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int cm=s->op==ISEND?UDST(s->mode):USRC(s->mode),vm=s->op==ISEND?USRC(s->mode):UDST(s->mode),contract=s->m,field=0,width;
 long channel=s->op==ISEND?s->d:s->s,value=s->op==ISEND?s->s:s->d;
 if(s->op!=ISEND && s->op!=IRECV)return 0;
 if(s->returned==204||s->returned==94){if(s->m<0 || s->m>65535 || (cm!=(AFP|AIND) && cm!=(AMP|AIND)))return 0;field=contract>>8;contract&=255;
  if((s->returned==94?(field!=0&&field!=8&&field!=16&&field!=24&&field!=32):(field!=0 && field!=8 && field!=16 && field!=24)) || contract!=(s->returned==94?(field==0?4:24):((field==8 || field==16)?24:4)))return 0;cm&=~AIND;
 }else if(s->returned!=188&&s->returned!=54)return 0;
 if(s->returned==54)return contract==8&&s->mode==(SRC(s->op==ISEND?vm:cm)|DST(s->op==ISEND?cm:vm))&&native64typedrootoperand(cm,channel,fp,mp)&&(vm==AFP||vm==AMP)&&native64typedwordoperand(vm,value,fp,mp)&&8<=(vm==AFP?fp:mp)-value;
 width=contract&15;
 if(s->mode>255 || (s->mode&ARM)!=AXNON || (contract!=1 && contract!=4 && contract!=8 && contract!=24) || !native64typedrootoperand(cm,channel,fp,mp))return 0;
 if(contract==24)return native64typedrootoperand(vm,value,fp,mp);
 if(vm==AIMM)return s->op==ISEND && width<8 && value>=-536870912L && value<=536870911L && (width!=1 || (value>=0 && value<=255));
 return (vm==AFP || vm==AMP) && native64typedcodeoperand(vm,value,fp,mp) && value%width==0 && width<=(vm==AFP?fp:mp)-value;
}

/* Ordinary mapped root groups and field channel construction. */
static int
native64typedwmchannel(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),cm,vm,width,field;long channel,value;
 if(s->mode>255)return 0;
 if(s->returned==90)return s->op==IMOVMP&&s->mode==(SRC(sm)|AXIMM|DST(dm))&&s->m==8&&(sm==(AFP|AIND)||sm==(AMP|AIND))&&(dm==(AFP|AIND)||dm==(AMP|AIND))&&native64typedrootoperand(sm&~AIND,s->s,fp,mp)&&native64typedrootoperand(dm&~AIND,s->d,fp,mp);
 if(s->returned==92)return s->op==INEWCMP&&s->mode==(SRC(AIMM)|AXIMM|DST(dm))&&(s->s==2||s->s==3||s->s==5||s->s==6)&&!s->m&&(dm==(AFP|AIND)||dm==(AMP|AIND))&&native64typedrootoperand(dm&~AIND,s->d,fp,mp);
 if(s->returned==108)return (s->op==INEWCW||s->op==INEWCP)&&s->mode==(SRC(AXXX)|AXIMM|DST(dm))&&!s->m&&((s->s>=256&&s->s<=280&&(s->s-256)%8==0)||(s->s==0||s->s==8||s->s==16||s->s==32||s->s==576))&&s->op==((s->s==0||s->s==256||s->s==280)?INEWCW:INEWCP)&&(dm==(AFP|AIND)||dm==(AMP|AIND))&&native64typedrootoperand(dm&~AIND,s->d,fp,mp);
 if(s->returned==109)return s->op==IRECV&&s->mode==(SRC(sm)|DST(AFP|AIND))&&((s->m==4&&s->d==32)||((s->m==65540||s->m==66564||s->m==67588)&&native64typedrootoperand(AFP,s->d,fp,mp)))&&native64typedrootoperand(sm,s->s,fp,mp);
 if(s->returned==110)return s->op==IMOVM&&s->mode==(SRC(sm)|AXIMM|DST(dm))&&s->m==16&&(sm==(AFP|AIND)||sm==(AMP|AIND))&&(dm==(AFP|AIND)||dm==(AMP|AIND))&&native64typedrootoperand(sm&~AIND,s->s,fp,mp)&&native64typedrootoperand(dm&~AIND,s->d,fp,mp);
 if(s->returned!=107||(s->op!=ISEND&&s->op!=IRECV)||(s->mode&ARM)!=AXNON||s->m<0)return 0;
 cm=s->op==ISEND?dm:sm;vm=s->op==ISEND?sm:dm;channel=s->op==ISEND?s->d:s->s;value=s->op==ISEND?s->s:s->d;width=s->m&255;field=(unsigned long)s->m>>8;
 if(width!=16&&width!=24)return 0;if(cm&AIND){if(!((field==64&&width==24)||(field==16&&width==16)))return 0;cm&=~AIND;}else if(field)return 0;
 return native64typedrootoperand(cm,channel,fp,mp)&&native64typedrootoperand(vm,value,fp,mp)&&width<=(vm==AFP?fp:mp)-value;
}

/* Payload root-count is a checked role, relocated to a private native Type. */
static int
native64typedmappedchannel(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 Native64TypedStep copy=*s;
 if(s->op!=INEWCMP || USRC(s->mode)!=AIMM || (s->s!=1 && s->s!=2 && s->s!=3 && s->s!=4) || (s->returned!=183 && s->returned!=185))return 0;
 copy.op=INEWCP;copy.mode=(s->mode&~(7<<3))|SRC(AXXX);copy.s=0;copy.returned=s->returned==185?24:0;
 return native64typedchannel(&copy,fp,mp);
}

static int
native64typedstringstep(const Native64TypedStep *s,unsigned int fpbytes,unsigned int mpbytes)
{
 unsigned int mm=s->mode&ARM;
 if(s->op==ISLICEC && (s->returned==24 || s->returned==40 || s->returned==56))return UDST(s->mode)==(AFP|AIND) && s->d==(s->returned==56?40:32) && (mm==AXIMM || mm==AXINF) && native64typedwordoperand(USRC(s->mode),s->s,fpbytes,mpbytes) && native64typedwordoperand(mm==AXIMM?AIMM:AFP,s->m,fpbytes,mpbytes) && (mm!=AXIMM || (s->m>=-32768 && s->m<=32767));
 if(s->op==IADDC && s->returned==24){
  return s->mode<=255 && UDST(s->mode)==(AFP|AIND) && s->d==32 && (mm==AXINF || mm==AXINM) && s->m<=65535 &&
   native64typedrootoperand(USRC(s->mode),s->s,fpbytes,mpbytes) && native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes);
 }
 if(s->mode>255 || s->returned || (UDST(s->mode)!=AFP && !(s->op==ISLICEC && UDST(s->mode)==AMP)))return 0;
 if(s->op==IADDC)return (mm==AXINF || mm==AXINM) &&
  native64typedrootoperand(USRC(s->mode),s->s,fpbytes,mpbytes) &&
  native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes) && s->m<=65535 &&
  native64typedrootoperand(AFP,s->d,fpbytes,mpbytes);
 if(s->op==IINSC)return (mm==AXIMM || mm==AXINF || mm==AXINM) && native64typedwordoperand(USRC(s->mode),s->s,fpbytes,mpbytes) && native64typedrootoperand(AFP,s->d,fpbytes,mpbytes) && native64typedwordoperand(mm==AXIMM?AIMM:mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes) && (mm!=AXIMM || (s->m>=-32768 && s->m<=32767));
 if(s->op!=IINDC && s->op!=ISLICEC)return 0;
 if(mm!=AXIMM && mm!=AXINF)return 0;
 if(mm==AXIMM){if(s->m< -32768 || s->m>32767)return 0;}
 else if(s->m>65535 || !native64typedwordoperand(AFP,s->m,fpbytes,mpbytes))return 0;
 return s->op==IINDC?
  native64typedrootoperand(USRC(s->mode),s->s,fpbytes,mpbytes) && native64typedwordoperand(AFP,s->d,fpbytes,mpbytes):
  native64typedwordoperand(USRC(s->mode),s->s,fpbytes,mpbytes) && native64typedrootoperand(UDST(s->mode),s->d,fpbytes,mpbytes);
}

/* Byte arrays only. NEWAZ's zero is a private byte-Type token; the owning
 * module builder resolves it to its separately retained primitive descriptor. */
static int
native64typedbytestep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int mm=s->mode&ARM;
 if(s->mode>255)return 0;
 if(s->returned){if((s->op!=ICVTAC && s->op!=ISLICEA) || s->returned!=24 || UDST(s->mode)!=(AFP|AIND) || s->d!=32)return 0;}
 else if(UDST(s->mode)!=AFP || !native64typedrootoperand(AFP,s->d,fp,mp))return 0;
 if(s->op==ICVTCA || s->op==ICVTAC)return mm==AXNON && !s->m && native64typedrootoperand(USRC(s->mode),s->s,fp,mp);
 if(!native64typedwordoperand(USRC(s->mode),s->s,fp,mp))return 0;
 if(s->op==INEWAZ)return mm==AXIMM && s->m==0;
 if(s->op!=ISLICEA)return 0;
 return mm==AXIMM?(s->m>=-32768 && s->m<=32767):mm==AXINF && s->m<=65535 && native64typedwordoperand(AFP,s->m,fp,mp);
}

static int
native64typedarraycall(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 return (s->returned==127 || s->returned==131) && s->op==IINDL && s->m==(s->returned==127?56:48) &&
  s->mode==(SRC(sm)|AXINF|DST(dm)) && (s->returned!=131 || sm==AFP) && native64typedrootoperand(sm,s->s,fp,mp) && native64typedwordoperand(dm,s->d,fp,mp);
}
static int
native64typedprotectedentry(const Native64TypedStep*steps,unsigned int count,unsigned int target)
{
 unsigned int back;if(target<count)for(back=1;back<=12&&back<=target;back++)if(steps[target-back].returned==157&&((steps[target-back].m>>8)&255)>=back-1)return 1;
 if(target<count&&target&&steps[target-1].returned==80&&(USRC(steps[target-1].mode)&AIND))return 1;
 return target<count && ((target && steps[target-1].returned==157 && ((steps[target-1].m>>8)&255)) || steps[target].returned==158 || (steps[target].returned==111&&(steps[target].op==IMOVP||native64typedbranch(steps[target].op))) || steps[target].returned==142 || (target&&steps[target].returned==213&&steps[target-1].returned==255&&steps[target-1].op==IINDL) || (steps[target].returned==255 && (steps[target].op==IMOVP || steps[target].op==IMOVMP)) ||
  (target && steps[target-1].returned==131 && steps[target-1].op==IINDL));
}
static int
native64typedwordargop(unsigned int op)
{
 return op==IADDW || op==ISUBW || op==IMULW || op==IDIVW || op==IMODW || op==IANDW || op==IORW || op==IXORW || op==ISHLW || op==ISHRW;
}

static int
native64typedimageword(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),mm=s->mode&ARM,mode=mm==AXIMM?AIMM:mm==AXINF?AFP:AMP;unsigned long field=(unsigned long)s->s>>16;
 if(s->returned!=133 || s->s<0 || (field>40 && field!=72 && field!=76 && field!=80) || field%4 || (sm!=(AFP|AIND) && sm!=(AMP|AIND)) || !native64typedrootoperand(sm&~AIND,s->s&65535,fp,mp))return 0;
 if(s->op==IMOVW)return mm==AXNON && !s->m && native64typedwordoperand(dm,s->d,fp,mp) && dm!=AIMM;
 if(!native64typedwordargop(s->op) && s->op!=IBEQW && s->op!=IBNEW && s->op!=IBLTW && s->op!=IBLEW && s->op!=IBGTW && s->op!=IBGEW)return 0;
 return mm!=AXNON && native64typedwordoperand(mode,s->m,fp,mp) && (mode==AIMM?(s->m>=-32768&&s->m<=32767):s->m<=65535) &&
  (native64typedbranch(s->op)?dm==AIMM:native64typedwordoperand(dm,s->d,fp,mp)&&dm!=AIMM);
}

static int
native64typedwindowgeometry(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),read=s->returned==83,bytes=s->m&255,field=(unsigned long)s->m>>8;long owner=read?s->s:s->d,value=read?s->d:s->s;unsigned int om=read?sm:dm,vm=read?dm:sm,total=vm==AFP?fp:mp;
 if((s->returned!=82&&s->returned!=83)||s->op!=IMOVM||s->m<0||!native64windowgeometryfield(field,bytes)||(om!=(AFP|AIND)&&om!=(AMP|AIND))||s->mode!=(SRC(sm)|AXIMM|DST(dm))||!native64typedrootoperand(om&~AIND,owner,fp,mp)||(vm!=AFP&&vm!=AMP)||!native64typedwordoperand(vm,value,fp,mp)||(unsigned long)value>total||bytes>total-value)return 0;return 1;}

static int native64typedcode(Inst*,unsigned int,const Native64TypedStep*,unsigned int,unsigned int,unsigned int);
static int
native64typedbytestring(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{Native64TypedStep c=*s;Inst out;if(s->returned==71)return s->op==IMOVP&&s->mode==(SRC(AMP)|DST(AFP))&&!s->m&&native64typedrootoperand(AMP,s->s,fp,mp)&&native64typedrootoperand(AFP,s->d,fp,mp);if(s->returned!=69||UDST(s->mode)!=AFP||(s->op!=IMOVP&&s->op!=ISLICEA))return 0;c.returned=0;return native64typedcode(&out,1,&c,1,fp,mp);}
static int
native64typedrootbindingfield(long field)
{return native64windowrootfield(field)||field==288||field==296;}
static int
native64typedwindowclone(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{return s->returned==68&&s->op==IMOVMP&&s->mode==(SRC(AMP)|AXIMM|DST(AFP|AIND))&&(s->m==11||s->m==12)&&s->s>=0&&s->s%8==0&&(unsigned long)s->s<=mp&&native64librarybytes(s->m)<=mp-s->s&&native64typedrootoperand(AFP,s->d,fp,mp);}
static int
native64rootpathfield(unsigned int kind,unsigned int field)
{return kind==1?native64windowrootfield(field):kind==2?field<=48&&field%8==0:kind==3?field<=16&&field%8==0:kind==4?field<=32&&field%8==0:kind==5?field<=24&&field%8==0:0;}
static int
native64typedrootcopy(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode);unsigned long m=s->m;
 return s->returned==73&&s->op==IMOVP&&s->m>=0&&s->mode==(SRC(sm)|DST(dm))&&(sm==(AFP|AIND)||sm==(AMP|AIND))&&(dm==(AFP|AIND)||dm==(AMP|AIND))&&native64rootpathfield(m>>24,(m>>8)&4095)&&native64rootpathfield((m>>20)&15,m&255)&&native64typedrootoperand(sm&~AIND,s->s,fp,mp)&&native64typedrootoperand(dm&~AIND,s->d,fp,mp);}
static int
native64typedrootpath(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode);if(s->op!=IMOVP)return 0;if(s->returned==66)return s->mode==(SRC(AMP)|DST(AFP))&&!s->m&&native64typedrootoperand(AMP,s->s,fp,mp)&&native64typedrootoperand(AFP,s->d,fp,mp);return s->returned==61&&s->m>=0&&native64rootpathfield((unsigned long)s->m>>16,s->m&65535)&&(sm==(AFP|AIND)||sm==(AMP|AIND))&&s->mode==(SRC(sm)|DST(AFP))&&native64typedrootoperand(sm&~AIND,s->s,fp,mp)&&native64typedrootoperand(AFP,s->d,fp,mp);}
static int
native64typedheapgeometry(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),n=s->m&255,sf=(s->m>>16)&255,df=(s->m>>8)&255,w=(unsigned long)s->m>>24;if(s->returned!=62||s->op!=IMOVM||s->m<0||w>1||!native64windowgeometryfield(df,n)||s->mode!=(SRC(sm)|AXIMM|DST(dm))||(sm!=(AFP|AIND)&&sm!=(AMP|AIND))||(dm!=(AFP|AIND)&&dm!=(AMP|AIND))||!native64typedrootoperand(sm&~AIND,s->s,fp,mp)||!native64typedrootoperand(dm&~AIND,s->d,fp,mp))return 0;return w?native64windowgeometryfield(sf,n):n==16?(sf==0||sf==16):n==8&&sf<=24&&sf%8==0;}
static int
native64windowwordfield(unsigned int field)
{return (field>=8&&field<=20&&field%4==0)||(field>=32&&field<=44&&field%4==0)||field==56||field==60||(field>=80&&field<=100&&field%4==0)||(field>=112&&field<=124&&field%4==0);}
static int
native64typedwindowword(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),mm=s->mode&ARM,mode=mm==AXIMM?AIMM:mm==AXINF?AFP:AMP;unsigned long field=(unsigned long)s->s>>16;
 if(s->mode>255)return 0;
 if(s->returned==88)return s->op==IMOVW&&s->mode==(SRC(sm)|DST(dm))&&(dm==(AFP|AIND)||dm==(AMP|AIND))&&s->m>=0&&native64windowwordfield(s->m)&&native64typedrootoperand(dm&~AIND,s->d,fp,mp)&&native64typedwordoperand(sm,s->s,fp,mp);
 if(s->returned!=89||s->s<0||!native64windowwordfield(field)||(sm!=(AFP|AIND)&&sm!=(AMP|AIND))||!native64typedrootoperand(sm&~AIND,s->s&65535,fp,mp))return 0;
 if(s->op==IMOVW)return mm==AXNON&&!s->m&&native64typedwordoperand(dm,s->d,fp,mp)&&dm!=AIMM;
 if(!native64typedwordargop(s->op)&&s->op!=IBEQW&&s->op!=IBNEW&&s->op!=IBLTW&&s->op!=IBLEW&&s->op!=IBGTW&&s->op!=IBGEW)return 0;
 return mm!=AXNON&&native64typedwordoperand(mode,s->m,fp,mp)&&(mode==AIMM?(s->m>=-32768&&s->m<=32767):s->m<=65535)&&(native64typedbranch(s->op)?dm==AIMM:native64typedwordoperand(dm,s->d,fp,mp)&&dm!=AIMM);}

static int
native64typedwindowcodec(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode);if(s->returned!=63||!native64windowwordfield(s->m))return 0;if(s->op==ICVTCW)return (dm==(AFP|AIND)||dm==(AMP|AIND))&&s->mode==(SRC(sm)|DST(dm))&&native64typedrootoperand(sm,s->s,fp,mp)&&native64typedrootoperand(dm&~AIND,s->d,fp,mp);return s->op==ICVTWC&&(sm==(AFP|AIND)||sm==(AMP|AIND))&&s->mode==(SRC(sm)|DST(AFP))&&native64typedrootoperand(sm&~AIND,s->s,fp,mp)&&native64typedrootoperand(AFP,s->d,fp,mp);}

static int
native64typedwindowwordarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{Native64TypedStep copy=*s;unsigned int i;copy.returned=89;copy.mode=(copy.mode&~7)|AFP;copy.d=64;
 if(s->returned!=86||UDST(s->mode)!=(AFP|AIND)||!native64typedwindowword(&copy,fp,mp)||native64typedbranch(s->op)||!native64typedwordoperand(AFP,s->d,bytes,0)||s->d>65535)return 0;
 for(i=0;i<4;i++)if((unsigned int)(s->d+i)/64>=np||map[(s->d+i)/64]&(128>>(((s->d+i)/8)%8)))return 0;return 1;}

/* A private 8-byte mapped element Type is installed by the owning builder. */
static int
native64typedrefarray(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 if(s->returned==254)return s->op==INEWAZ && s->mode==(SRC(sm)|AXIMM|DST(dm)) && s->m==2 && native64typedwordoperand(sm,s->s,fp,mp) && native64typedrootoperand(dm,s->d,fp,mp);
 if(s->returned!=255)return 0;
 if(s->op==IINDX)return sm==AFP && s->mode==(SRC(sm)|AXINF|DST(dm)) &&
  s->m==48 && native64typedrootoperand(sm,s->s,fp,mp) &&
  native64typedwordoperand(dm,s->d,fp,mp);
 if(s->op==IMOVMP){
  int read=sm==(AFP|AIND);
  long direct=read?s->d:s->s;
  return s->m==0 && (read?s->s:s->d)==48 &&
   s->mode==((read?SRC(AFP|AIND)|DST(AFP):SRC(AFP)|DST(AFP|AIND))|AXIMM) &&
   native64typedrootoperand(AFP,direct,fp,mp) && fp-direct>=16;
 }
 if(s->op==IINDL)return s->mode==(SRC(sm)|AXINF|DST(dm)) && s->m==48 && native64typedrootoperand(sm,s->s,fp,mp) && native64typedwordoperand(dm,s->d,fp,mp);
 if(s->op!=IMOVP || s->m)return 0;
 return sm==(AFP|AIND)?s->s==48 && s->mode==(SRC(sm)|DST(dm)) && native64typedrootoperand(dm,s->d,fp,mp):
  dm==(AFP|AIND) && s->d==48 && s->mode==(SRC(sm)|DST(dm)) && native64typedrootoperand(sm,s->s,fp,mp);
}

/* Mapped array protocols: borrowed element results and nil comparisons. */
static int
native64typedregistry(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),mm=s->mode&ARM;
 if(s->returned!=111||s->mode>255)return 0;
 if(s->op==ISLICELA)return s->mode==(SRC(sm)|mm|DST(dm))&&mm!=AXNON&&(mm!=AXIMM||(s->m>=-32768&&s->m<=32767))&&native64typedrootoperand(sm,s->s,fp,mp)&&native64typedrootoperand(dm,s->d,fp,mp)&&native64typedwordoperand(mm==AXIMM?AIMM:mm==AXINF?AFP:AMP,s->m,fp,mp);
 if(s->op==IINDL)return s->mode==(SRC(sm)|AXINF|DST(dm))&&s->m==48&&native64typedrootoperand(sm,s->s,fp,mp)&&(dm==(AFP|AIND)||dm==(AMP|AIND))&&s->d>=0&&((unsigned long)s->d>>16)==72&&native64typedrootoperand(dm&~AIND,s->d&65535,fp,mp);
 if(s->op==IMOVP)return s->mode==(SRC(AFP|AIND)|DST(AFP|AIND))&&s->s==48&&!s->m&&s->d==32;
 return (s->op==IBEQL||s->op==IBNEL||s->op==IBEQC||s->op==IBNEC)&&s->mode==(SRC(AFP|AIND)|AXINM|DST(AIMM))&&s->s==48&&native64typedrootoperand(AMP,s->m,fp,mp)&&s->d>=0;
}

/* Context has exactly three pointer fields (Display, Screen, wm channel).
 * The compiler proves the nominal owner and field class; no generic ADT ABI. */
static int
native64typedcontextstep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),base,value;
 if(s->returned>=Native64TypedContextNil && s->returned<Native64TypedContextNil+3){
  return (s->op==IBEQL || s->op==IBNEL) && s->mode==(SRC(sm)|AXINM|DST(AIMM)) &&
   (sm==(AFP|AIND) || sm==(AMP|AIND)) && s->m<=65535 &&
   native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(AMP,s->m,fp,mp);
 }
 if(s->op!=IMOVP || s->returned!=Native64TypedContext || s->mode>255 || (s->mode&ARM)!=AXNON ||
    (s->m!=0 && s->m!=8 && s->m!=16))return 0;
 if(sm==(AFP|AIND) || sm==(AMP|AIND)){base=sm&~AIND;value=dm;}
 else if(dm==(AFP|AIND) || dm==(AMP|AIND)){base=dm&~AIND;value=sm;}
 else return 0;
 return native64typedrootoperand(base,(sm&AIND)?s->s:s->d,fp,mp) &&
  native64typedrootoperand(value,(sm&AIND)?s->d:s->s,fp,mp);
}

static unsigned int
native64windowfieldoffset(unsigned int index)
{
 static const unsigned int offsets[]={0,24,48,64,72,104};
 return index<6?offsets[index]:~0U;
}
static int
native64typedwindowstep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),i;
 if(s->returned>=Native64TypedWindowNil && s->returned<Native64TypedWindowNil+6){
  return (s->op==IBEQL || s->op==IBNEL) && s->mode==(SRC(sm)|AXINM|DST(AIMM)) &&
   (sm==(AFP|AIND) || sm==(AMP|AIND)) && s->m<=65535 &&
   native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(AMP,s->m,fp,mp);
 }
 if(s->returned!=Native64TypedWindow || s->m<0)return 0;
 for(i=0;i<6;i++)if(s->m==native64windowfieldoffset(i))break;
 if(i==6)return 0;
 return s->op==IMOVP && s->mode<=255 && (s->mode&ARM)==AXNON &&
  ((sm==(AFP|AIND) || sm==(AMP|AIND))?
    native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(dm,s->d,fp,mp):
    (dm==(AFP|AIND) || dm==(AMP|AIND)) && native64typedrootoperand(sm,s->s,fp,mp) && native64typedrootoperand(dm&~AIND,s->d,fp,mp));
}

static unsigned int
native64drawfieldoffset(unsigned int index)
{
 static const unsigned int offsets[]={48,56,64,8,16,24,0,8,16,24,32};return index<11?offsets[index]:~0U;
}
static int
native64typedfieldoffset(long offset,int wide)
{
 unsigned int i;if(wide && (offset==72 || offset==104))return 1;
 for(i=0;i<11;i++)if(offset==native64drawfieldoffset(i))return 1;return 0;
}
static int
native64typeddrawstep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 if(s->returned>=Native64TypedDrawNil && s->returned<Native64TypedDrawNil+11){
  return s->returned!=Native64TypedDrawNil+2 && (s->op==IBEQL || s->op==IBNEL) && s->mode==(SRC(sm)|AXINM|DST(AIMM)) &&
   (sm==(AFP|AIND) || sm==(AMP|AIND)) && s->m<=65535 && native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(AMP,s->m,fp,mp);
 }
 if(s->returned!=Native64TypedDraw || !native64typedfieldoffset(s->m,0))return 0;
 return s->op==IMOVP && s->mode<=255 && (s->mode&ARM)==AXNON &&
  ((sm==(AFP|AIND) || sm==(AMP|AIND))?
   native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(dm,s->d,fp,mp):
   (dm==(AFP|AIND) || dm==(AMP|AIND)) && native64typedrootoperand(sm,s->s,fp,mp) && native64typedrootoperand(dm&~AIND,s->d,fp,mp));
}

/* Input, layouts and semantic operand roles were proved by the compiler and
 * remain immutable. This bridge checks native encoding and local branch ownership.
 * Output must be disjoint from input. Refusal leaves all output bytes untouched.
 * No frame addresses are captured: result stores resolve FP[32] at execution. */
static int
native64typedaltstep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),w=native64fileiowidth(s->m)?native64fileiowidth(s->m):s->m==72?24:s->m==48?16:s->m&15;
 if(s->mode>255 || (s->mode&ARM)!=AXNON || dm!=AFP)return 0;
 if(s->returned==91 || s->returned==189 || s->returned==192 || s->returned==193 || s->returned==198)return s->op==IMOVL && native64typedrootoperand(AFP,s->d,fp,mp) &&
  (s->returned==189?(!s->m && native64typedrootoperand(sm,s->s,fp,mp)):((s->returned==91?(s->m==0||s->m==8||s->m==16||s->m==24||s->m==64):s->returned==198?(s->m==0 || s->m==8):s->returned==192?s->m==64:(s->m>=0 && s->m<=32 && s->m%8==0)) && (sm==(AFP|AIND) || sm==(AMP|AIND)) && native64typedrootoperand(sm&~AIND,s->s,fp,mp)));
 if(s->returned==134 || s->returned==190 || s->returned==195 || s->returned==197 || s->returned==199 || s->returned==203)return s->op==ILEA && (s->returned==134?s->m==136:s->returned==199?s->m==104:s->returned==203?s->m==120:s->returned==197?s->m==72:s->returned==195?s->m==48:(s->m==4 || s->m==8 || s->m==24 || s->m==152)) && native64typedrootoperand(AFP,s->d,fp,mp) &&
  (sm==AFP || sm==AMP) && s->s>=0 && !(s->s%((s->m==48 || s->m==72 || native64fileiowidth(s->m))?8:s->m==8?4:w)) && native64typedcodeoperand(sm,s->s,fp,mp) && w<=(sm==AFP?fp:mp)-s->s;
 return s->returned==191 && (s->op==IALT || s->op==INBALT) && sm==AFP && s->m>=1 && s->m<=32 &&
  native64typedrootoperand(AFP,s->s,fp,mp) && 8+16*s->m<=fp-s->s && native64typedwordoperand(AFP,s->d,fp,mp);
}

static int
native64typednumericresult(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int width=s->returned>=150?8:4,sm=USRC(s->mode),mm=s->mode&ARM,mode=mm==AXIMM?AIMM:mm==AXINF?AFP:AMP;
 if((s->returned<145 || s->returned>147) && (s->returned<149 || s->returned>151))return 0;
 if(s->mode>255 || UDST(s->mode)!=(AFP|AIND) || s->d!=(s->returned<=147?32+4*(s->returned-145):32))return 0;
 if(s->returned<=147 && s->op!=IMOVW)return 0;
 if(s->returned==149 && s->op!=IMOVW && !native64typedwordargop(s->op))return 0;
 if((s->returned==150 && s->op==ICVTCL) || (s->returned==151 && s->op==ICVTCF))return mm==AXNON && !s->m && native64typedrootoperand(sm,s->s,fp,mp);
 if(s->returned==150 && s->op!=IMOVL && s->op!=IADDL && s->op!=ISUBL && s->op!=IMULL && s->op!=IDIVL && s->op!=IMODL)return 0;
 if(s->returned==151 && s->op!=IMOVF && s->op!=INEGF && s->op!=IADDF && s->op!=ISUBF && s->op!=IMULF && s->op!=IDIVF)return 0;
 if(!native64typedcodeoperand(sm,s->s,fp,mp) || (sm==AIMM?width!=4:(s->s%width || width>(sm==AFP?fp:mp)-s->s)))return 0;
 if(mm==AXNON)return !s->m;
 if(!native64typedcodeoperand(mode,s->m,fp,mp) || (mm==AXIMM?(width!=4 || s->m< -32768 || s->m>32767):(s->m%width || s->m>65535 || width>(mode==AFP?fp:mp)-s->m)))return 0;
 return 1;
}

#include "native64-aggregate-result.h"
#include "native64-raw-array-code.h"
static int
native64typedcode(Inst *out,unsigned int capacity,const Native64TypedStep *steps,unsigned int count,unsigned int fpbytes,unsigned int mpbytes)
{
	unsigned int i,sm,dm,mm,width,reference;
	const Native64TypedStep *s;
	Inst *p;
	if(!native64model() || out==nil || steps==nil || !count || count>65536 || capacity<count ||
	   fpbytes<64 || fpbytes>1024*1024 || fpbytes%8 || mpbytes>1024*1024 || mpbytes%8)return 0;
	for(i=0;i<count;i++){
		s=steps+i;sm=USRC(s->mode);dm=UDST(s->mode);mm=s->mode&ARM;
		if(s->mode>255 || s->op>255)return 0;
        if(native64typedbranch(s->op) && s->d>=0 && (unsigned long)s->d<count && native64typedprotectedentry(steps,count,s->d))return 0;
        if(s->returned>=Native64TypedContext && s->returned<Native64TypedContextNil+3){
            if(!native64typedcontextstep(s,fpbytes,mpbytes) ||
               (s->returned!=Native64TypedContext && (s->d<0 || (unsigned long)s->d>=count)))return 0;
            continue;
        }
        if(s->returned>=Native64TypedWindow && s->returned<Native64TypedWindowNil+6){
            if(!native64typedwindowstep(s,fpbytes,mpbytes) ||
               (s->returned!=Native64TypedWindow && (s->d<0 || (unsigned long)s->d>=count)))return 0;
            continue;
        }
        if(s->returned>=Native64TypedDraw && s->returned<Native64TypedDrawNil+11){
            if(!native64typeddrawstep(s,fpbytes,mpbytes) || (s->returned!=Native64TypedDraw && (s->d<0 || (unsigned long)s->d>=count)))return 0;continue;
        }
        if(s->returned==173){if(!native64typedcontextcopy(s,fpbytes,mpbytes))return 0;continue;}
        if((s->returned>=145 && s->returned<=147) || (s->returned>=149 && s->returned<=151)){if(!native64typednumericresult(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==90||s->returned==92||(s->returned>=107&&s->returned<=110)){if(!native64typedwmchannel(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==111){if(!native64typedregistry(s,fpbytes,mpbytes)||(native64typedbranch(s->op)&&(unsigned long)s->d>=count))return 0;continue;}
        if(s->returned==142){if(!native64libraryarraystore(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==138 || s->returned==139){if(!native64typedpointerpair(s,fpbytes,mpbytes) || (s->returned==138 && (s->d<0 || (unsigned long)s->d>=count)))return 0;continue;}
        if(s->returned==159){if(!native64librarybytecopy(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==157 || s->returned==158){if(!native64librarybyte(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==153 || s->returned==154){if(!native64typedlibrary(s,fpbytes,mpbytes) || (native64typedbranch(s->op) && (unsigned long)s->d>=count))return 0;continue;}
        if(s->returned==136){if((s->op!=ISEND && s->op!=IRECV) || s->mode!=(SRC(sm)|DST(dm)) || s->m!=136 || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes) || !native64typedrootoperand(dm,s->d,fpbytes,mpbytes) || 16>((s->op==ISEND?sm:dm)==AFP?fpbytes:mpbytes)-(s->op==ISEND?s->s:s->d))return 0;continue;}
        if(s->returned==82||s->returned==83){if(!native64typedwindowgeometry(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==88||s->returned==89){if(!native64typedwindowword(s,fpbytes,mpbytes)||(native64typedbranch(s->op)&&(s->d<0||(unsigned long)s->d>=count)))return 0;continue;}
        if(s->returned==Native64TypedRecord){if(!native64typedrecord(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==79){if(!(s->op==INEWAZ?native64typedrawnew(s,fpbytes,mpbytes):native64typedrawslice(s,fpbytes,mpbytes)))return 0;continue;}
        if(s->returned==80){if(!native64typedrawindex(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==65){if(!native64typedrawuse(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==77){if(!native64typedaggregate(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==73){if(!native64typedrootcopy(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==68){if(!native64typedwindowclone(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==61||s->returned==66){if(!native64typedrootpath(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==62){if(!native64typedheapgeometry(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==63){if(!native64typedwindowcodec(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==133){if(!native64typedimageword(s,fpbytes,mpbytes) || (native64typedbranch(s->op) && (s->d<0 || (unsigned long)s->d>=count)))return 0;continue;}
        if(s->returned==127 || s->returned==131){if(!native64typedarraycall(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==254 || s->returned==255){if(!native64typedrefarray(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==253){if(!native64typedinthead(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==252){if(!native64typedheadprep(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned>=248 && s->returned<=250){if(!native64typedrectlist(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==246 || s->returned==247){if(!native64typedgeometryreturn(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==242){if((s->op!=INEW && s->op!=INEWZ) || s->mode!=(SRC(AIMM)|DST(AFP)) || s->s || s->m || !native64typedrootoperand(AFP,s->d,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==243 || s->returned==244){if(!native64typedpointerstore(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==172 || s->returned==174){if(!native64typedcontextnew(s,fpbytes))return 0;continue;}
        if(s->returned==171){if(!native64typedtriplecopy(s,fpbytes))return 0;continue;}
        if(s->returned==46){Inst probe;Native64TypedStep copy=*s;copy.returned=0;if(s->op!=IADDW||s->mode!=(SRC(AIMM)|AXINF|DST(AFP))||s->s!=1||!native64typedcode(&probe,1,&copy,1,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==45||s->returned==47){if(s->op!=IRECV||s->mode!=(SRC(sm)|DST(AFP|AIND))||s->m!=16||s->d!=32||!native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==50||s->returned==52){if(!native64typedcursor(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==135 || s->returned==194 || s->returned==196 || s->returned==201 || s->returned==202){unsigned int span=s->returned==201?32:(s->returned==135 || s->returned==194)?16:24;if(s->op!=IMOVMP || s->mode>255 || (s->mode&ARM)!=AXIMM || s->m || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes) || !native64typedrootoperand(dm,s->d,fpbytes,mpbytes) || span>(sm==AFP?fpbytes:mpbytes)-s->s || span>(dm==AFP?fpbytes:mpbytes)-s->d)return 0;continue;}
        if(s->returned==91 || (s->returned>=189 && s->returned<=193) || s->returned==134 || s->returned==195 || s->returned==197 || s->returned==198 || s->returned==199 || s->returned==203){if(!native64typedaltstep(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==213){if(!native64typedwindowlist(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==214){if(!native64typedwmsrvwindow(s,fpbytes,mpbytes))return 0;continue;}
        if((s->returned>=208 && s->returned<=212) || s->returned==215 || (s->returned==216 && s->op==IMOVM)){if(!native64typedlocalgeometry(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==228 || s->returned==229){if(!native64typedpointerword(s,fpbytes,mpbytes) || (s->returned==229 && (s->d<0 || (unsigned long)s->d>=count)))return 0;continue;}
        if(s->returned==231 || ((s->op==IBEQL || s->op==IBNEL) && (sm==AFP || sm==AMP) && s->returned && s->returned!=239)){if(!native64typedclientcompare(s,fpbytes,mpbytes) || s->d<0 || (unsigned long)s->d>=count)return 0;continue;}
        if(s->returned==232 || ((s->op==IBEQC || s->op==IBNEC) && (sm==(AFP|AIND) || sm==(AMP|AIND)) && s->returned)){if(!native64typedcursorcompare(s,fpbytes,mpbytes) || s->d<0 || (unsigned long)s->d>=count)return 0;continue;}
        if(s->returned==234 || (s->op==IANDW && (sm==(AFP|AIND) || sm==(AMP|AIND)) && s->returned)){if(!native64typedflagsand(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==236 || (s->op==IORW && (dm==(AFP|AIND) || dm==(AMP|AIND)) && s->returned)){if(!native64typedflagsor(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==237 || (s->op==ICVTWC && (sm&AIND) && s->returned)){if(!native64typedidstring(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==239 || s->returned==241 || s->returned==251){if(!native64typedchannelnil(s,fpbytes,mpbytes) || s->d<0 || (unsigned long)s->d>=count)return 0;continue;}
        if(s->returned==207){if(!native64typedfieldreference(s,fpbytes,mpbytes) || s->d<0 || (unsigned long)s->d>=count)return 0;continue;}
        if(s->returned==205 || s->returned==206){if(!native64typedclientfield(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==54 || s->returned==94 || s->returned==188 || s->returned==204){if(!native64typedtransfer(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==183 || s->returned==185){if(!native64typedmappedchannel(s,fpbytes,mpbytes))return 0;continue;}
        if(native64typedchannelop(s->op)){if(!native64typedchannel(s,fpbytes,mpbytes))return 0;continue;}
		switch(s->op){
		case INEWAZ:case ISLICEA:case ICVTCA:case ICVTAC:
            if(!native64typedbytestep(s,fpbytes,mpbytes))return 0;
            continue;
        case ICVTCW:
            if(s->returned || dm!=AFP || mm!=AXNON || s->m || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes) ||
               !native64typedwordoperand(AFP,s->d,fpbytes,mpbytes))return 0;
            continue;
        case ICVTWC:
            if(s->returned || dm!=AFP || mm!=AXNON || s->m || !native64typedwordoperand(sm,s->s,fpbytes,mpbytes) ||
               !native64typedrootoperand(AFP,s->d,fpbytes,mpbytes))return 0;
            continue;
		case IADDC:case IINDC:case IINSC:case ISLICEC:
            if(!native64typedstringstep(s,fpbytes,mpbytes))return 0;
            continue;
        case IBEQC:case IBNEC:case IBLTC:case IBLEC:case IBGTC:case IBGEC:
            if(s->returned || dm!=AIMM || s->d<0 || (unsigned long)s->d>=count || (mm!=AXINF && mm!=AXINM) || s->m>65535 ||
               !native64typedrootoperand(sm,s->s,fpbytes,mpbytes) || !native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes))return 0;
            continue;
		case IRAISE:
            if(dm!=AXXX || mm!=AXNON || s->m || s->d || s->returned ||
               !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
            continue;
        case ICONSW:
            if(s->returned || mm!=AXNON || s->m || !native64typedwordoperand(sm,s->s,fpbytes,mpbytes) || !native64typedrootoperand(dm,s->d,fpbytes,mpbytes))return 0;
            continue;
        case IHEADW:
            if(mm!=AXNON || s->m || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
            if(s->returned){if(s->returned!=4 || dm!=(AFP|AIND) || s->d!=32)return 0;}
            else if(dm==AIMM || !native64typedwordoperand(dm,s->d,fpbytes,mpbytes))return 0;
            continue;
		case ICONSP:case IHEADP:case ITAIL:
            if(mm!=AXNON || s->m || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
            if(s->returned){
                if(s->op==ICONSP || s->returned!=24 || dm!=(AFP|AIND) || s->d!=32)return 0;
            }else if(!native64typedrootoperand(dm,s->d,fpbytes,mpbytes))return 0;
            continue;
		case ILOAD:
            if((sm!=AFP && sm!=AMP) || (dm!=AFP && dm!=AMP) || mm!=AXIMM || s->m<0 || s->m>=64 ||
               s->returned || !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) ||
               !native64typedcodeoperand(dm,s->d,fpbytes,mpbytes) || s->s%8 || s->d%8 ||
               8>(sm==AFP?fpbytes:mpbytes)-s->s || 8>(dm==AFP?fpbytes:mpbytes)-s->d)return 0;
            continue;
		case ICASE:case ICASEL:case IGOTO:case ICASEC:
            width=s->op==ICASEL || s->op==ICASEC?8:4;
            if((sm!=AFP && sm!=AMP) || dm!=AMP || mm!=AXNON || s->m || s->returned ||
               !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || s->s%width ||
               width>(sm==AFP?fpbytes:mpbytes)-s->s || s->d<0 || s->d%4 ||
               (unsigned long)s->d>mpbytes || 4>mpbytes-s->d || (s->op==IGOTO && s->d<4))return 0;
            continue;
		case INOP:case IRET:case IEXIT:
			if(s->mode!=(SRC(AXXX)|DST(AXXX)) || s->s || s->m || s->d || s->returned)return 0;
			continue;
		case IMOVB:case IMOVW:case IMOVL:case IMOVF:case IMOVP:
		case ICVTBW:case ICVTWB:case ICVTWL:case ICVTLW:case ICVTWF:case ICVTFW:case ICVTLF:case ICVTFL:
		case INEGF:case ILENC:case ILENL:case ILENA:
		case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:
		case IADDL:case ISUBL:case IMULL:case IDIVL:case IMODL:case IANDL:case IORL:case IXORL:case ISHLL:case ISHRL:
		case IADDF:case ISUBF:case IMULF:case IDIVF:break;
		default:if(!native64typedbranch(s->op))return 0;
		}
		if(!native64typedcodeoperand(sm,s->s,fpbytes,mpbytes))return 0;
		if(native64typedbranch(s->op)){
			if(dm!=AIMM || s->d<0 || (unsigned long)s->d>=count || s->returned)return 0;
        }else if(s->returned>=168 && s->returned<=170){
            if(s->op!=IMOVP || dm!=(AFP|AIND) || mm!=AXNON || s->m || s->d!=32+8*(s->returned-168) || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
		}else if(s->returned==40 || s->returned==56){
            if(s->op!=IMOVP || dm!=(AFP|AIND) || mm!=AXNON || s->m || s->d!=(s->returned==40?32:40) ||
               !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
		}else if(s->returned){
			width=s->returned&15;reference=!!(s->returned&16);
			if(dm!=(AFP|AIND) || s->d!=32 || (s->returned&~31) ||
			   (width!=1 && width!=4 && width!=8) || (reference && width!=8))return 0;
		}else if(!native64typedcodeoperand(dm,s->d,fpbytes,mpbytes) || dm==AIMM || dm==AXXX)return 0;
		if(mm==AXNON){if(s->m)return 0;}
		else if(mm==AXIMM){if(s->m < -32768 || s->m>32767)return 0;}
		else if(s->m>65535 || !native64typedcodeoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes))return 0;
	}
	for(i=0;i<count;i++){
		s=steps+i;p=out+i;memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;p->reg=s->m;
		p->s.imm=s->s;
        if(s->returned==46){Native64TypedStep copy=*s;copy.returned=0;if(!native64typedcode(p,1,&copy,1,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==45||s->returned==47){p->reg=0;p->d.i.f=32;p->d.i.s=s->returned==45?16:0;continue;}
        if(s->returned==50||s->returned==52){p->reg=0;p->d.ind=s->d;continue;}
        if(s->returned==135 || s->returned==194 || s->returned==196 || s->returned==201 || s->returned==202){p->reg=0;p->d.ind=s->d;continue;}
        if(s->returned==91 || (s->returned>=189 && s->returned<=193) || s->returned==134 || s->returned==195 || s->returned==197 || s->returned==198 || s->returned==199 || s->returned==203){p->reg=(s->returned==134 || s->returned==190 || s->returned==195 || s->returned==197 || s->returned==199 || s->returned==203)?256|s->m:0;p->d.ind=s->d;if(s->returned==91 || s->returned==192 || s->returned==193 || s->returned==198){p->s.i.f=s->s;p->s.i.s=s->returned==192?64:s->m;}continue;}
        if((s->returned>=208 && s->returned<=212)||s->returned==215||(s->returned==216&&s->op==IMOVM)){p->reg=s->m&255;if(s->returned==208 || s->returned==209 || ((s->returned==215||s->returned==216) && (USRC(s->mode)&AIND))){p->s.i.f=s->s;p->s.i.s=s->m>>8;p->d.ind=s->d;}else if(s->returned==212)p->d.ind=s->d;else{p->d.i.f=s->d;p->d.i.s=s->m>>8;}continue;}
        if(s->returned==213){p->reg=0;p->d.ind=s->d;continue;}
        if(s->returned==214){p->reg=0;if(USRC(s->mode)&AIND){p->s.i.f=s->s;p->s.i.s=s->m;p->d.ind=s->d;}else{p->d.i.f=s->d;p->d.i.s=s->m;}continue;}
        if(s->returned==228 || s->returned==229){p->reg=0;p->s.i.f=s->s;p->s.i.s=s->m;if(s->returned==229)p->d.ins=out+s->d;else p->d.ind=s->d;continue;}
        if(s->returned==231){p->d.ins=out+s->d;continue;}
        if(s->returned==232){p->s.i.f=s->s;p->s.i.s=40;p->d.ins=out+s->d;continue;}
        if(s->returned==234){p->reg=(short)(s->m&65535);p->s.i.f=s->s;p->s.i.s=32;p->d.ind=s->d;continue;}
        if(s->returned==236){p->reg=0;p->d.i.f=s->d;p->d.i.s=32;continue;}
        if(s->returned==237){p->reg=0;p->s.i.f=s->s;p->s.i.s=72;p->d.ind=s->d;continue;}
        if(s->returned==239 || s->returned==241 || s->returned==251){if(s->returned==241 || s->returned==251){p->reg=s->m&65535;p->s.i.f=s->s;p->s.i.s=s->m>>16;}p->d.ins=out+s->d;continue;}
        if(s->returned==207){p->reg=s->m&65535;p->s.i.f=s->s;p->s.i.s=(s->m>>16)&255;p->d.ins=out+s->d;continue;}
        if(s->returned==205 || s->returned==206){p->reg=0;if(USRC(s->mode)&AIND){p->s.i.f=s->s;p->s.i.s=s->m;p->d.ind=s->d;}else{p->d.i.f=s->d;p->d.i.s=s->m;}continue;}
        if(s->returned==204||s->returned==94){p->reg=0;if(s->op==ISEND){p->d.i.f=s->d;p->d.i.s=s->m>>8;}else{p->s.i.f=s->s;p->s.i.s=s->m>>8;p->d.ind=s->d;}continue;}
        if(s->returned==55){p->d.ind=s->d;continue;}
        if(s->returned==54 || s->returned==188){p->reg=0;p->d.ind=s->d;continue;}
        if(s->returned==183){p->d.ind=s->d;continue;}
        if(s->returned==185){p->d.i.f=32;p->d.i.s=0;continue;}
        if((s->returned>=145 && s->returned<=147) || (s->returned>=149 && s->returned<=151)){p->d.i.f=32;p->d.i.s=s->returned<=147?4*(s->returned-145):0;continue;}
        if(s->returned==90){p->reg=0;p->s.i.f=s->s;p->s.i.s=0;p->d.i.f=s->d;p->d.i.s=0;continue;}
        if(s->returned==107){p->reg=0;if(s->op==ISEND){p->s.ind=s->s;if(UDST(s->mode)&AIND){p->d.i.f=s->d;p->d.i.s=(unsigned long)s->m>>8;}else p->d.ind=s->d;}else{p->d.ind=s->d;if(USRC(s->mode)&AIND){p->s.i.f=s->s;p->s.i.s=(unsigned long)s->m>>8;}else p->s.ind=s->s;}continue;}
        if(s->returned==92){p->reg=0;p->d.i.f=s->d;p->d.i.s=s->s==2?16:s->s==3?64:s->s==5?0:8;continue;}
        if(s->returned==108){p->s.imm=0;p->reg=0;p->d.i.f=s->d;p->d.i.s=s->s&255;continue;}
        if(s->returned==109){p->reg=0;p->s.ind=s->s;p->d.i.f=s->m==4?32:s->d;p->d.i.s=s->m==4?0:(s->m>>8)&255;continue;}
        if(s->returned==110){p->reg=16;p->s.i.f=s->s;p->s.i.s=0;p->d.i.f=s->d;p->d.i.s=8;continue;}
        if(s->returned==111){if(s->op==IINDL){p->s.ind=s->s;p->reg=48;p->d.i.f=s->d&65535;p->d.i.s=72;}else if(s->op==IMOVP){p->s.i.f=48;p->s.i.s=0;p->d.i.f=32;p->d.i.s=0;}else if(native64typedbranch(s->op)){p->s.i.f=48;p->s.i.s=0;p->reg=s->m;p->d.ins=out+s->d;}else p->d.ind=s->d;continue;}
        if(s->returned==142){p->reg=0;p->s.i.f=s->s&65535;p->s.i.s=24;p->d.i.f=48;p->d.i.s=0;continue;}
        if(s->returned==138 || s->returned==139){p->s.i.f=s->s;p->s.i.s=s->returned==138?(unsigned long)s->m>>16:0;if(s->returned==138){p->reg=s->m&65535;p->d.ins=out+s->d;}else{p->reg=16;p->d.i.f=s->d;p->d.i.s=0;}continue;}
        if(s->returned==159){p->reg=s->m;p->d.ind=s->d;continue;}
        if(s->returned==157){p->reg=48;if(USRC(s->mode)&AIND){p->s.i.f=s->s&65535;p->s.i.s=8;}else p->s.ind=s->s;if(UDST(s->mode)&AIND){p->d.i.f=s->d&65535;p->d.i.s=(unsigned long)s->d>>16;}else p->d.imm=s->d;continue;}
        if(s->returned==158){p->reg=0;if(USRC(s->mode)&AIND){p->s.i.f=48;p->s.i.s=0;if(UDST(s->mode)&AIND){p->d.i.f=32;p->d.i.s=0;}else p->d.ind=s->d;}else{p->s.imm=s->s;p->d.i.f=48;p->d.i.s=0;}continue;}
        if(s->returned==154){p->reg=0;if(UDST(s->mode)&AIND){p->d.i.f=32;p->d.i.s=0;}else p->d.ind=s->d;continue;}
        if(s->returned==153){p->reg=native64librarymiddle(s);if(USRC(s->mode)&AIND){p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;}if(native64typedbranch(s->op))p->d.ins=out+s->d;else if(UDST(s->mode)&AIND){p->d.i.f=((s->m>>16)&255)?s->d&65535:32;p->d.i.s=((s->m>>16)&255)?(unsigned long)s->d>>16:0;}else p->d.ind=s->d;continue;}
        if(s->returned==136){p->reg=0;p->d.ind=s->d;continue;}
        if(s->returned==82||s->returned==83){p->reg=s->m&255;if(s->returned==83){p->s.i.f=s->s;p->s.i.s=s->m>>8;p->d.ind=s->d;}else{p->d.i.f=s->d;p->d.i.s=s->m>>8;}continue;}
        if(s->returned==79||s->returned==80){
            p->reg=s->returned==80?s->m&255:s->op==INEWAZ?0:s->m&65535;
            if(USRC(s->mode)&AIND){p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;}
            if(UDST(s->mode)&AIND){p->d.i.f=s->d&65535;p->d.i.s=(unsigned long)s->d>>16;}
            else p->d.imm=s->d;
            continue;
        }
        if(s->returned==65){native64rawusecode(p,s);if(native64typedbranch(s->op))p->d.ins=out+s->d;continue;}
        if(s->returned==77){p->reg=(s->op==IMOVM)?(s->m>>8)&255:0;p->d.i.f=32;p->d.i.s=s->m&255;continue;}
        if(s->returned==Native64TypedRecord){native64recordcode(p,s,out);continue;}
        if(s->returned==73){p->reg=0;p->s.i.f=s->s;p->s.i.s=(s->m>>8)&4095;p->d.i.f=s->d;p->d.i.s=s->m&255;continue;}
        if(s->returned==68){p->reg=s->m;p->d.i.f=s->d;p->d.i.s=0;continue;}
        if(s->returned==61||s->returned==66){p->reg=0;p->d.ind=s->d;if(s->returned==61){p->s.i.f=s->s;p->s.i.s=s->m&65535;}continue;}
        if(s->returned==62){p->reg=s->m&255;p->s.i.f=s->s;p->s.i.s=(s->m>>16)&255;p->d.i.f=s->d;p->d.i.s=(s->m>>8)&255;continue;}
        if(s->returned==63){p->reg=0;if(s->op==ICVTCW){p->d.i.f=s->d;p->d.i.s=s->m;}else{p->s.i.f=s->s;p->s.i.s=s->m;p->d.ind=s->d;}continue;}
        if(s->returned==88){p->reg=0;p->d.i.f=s->d;p->d.i.s=s->m;continue;}
        if(s->returned==89||s->returned==133){p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;if(native64typedbranch(s->op))p->d.ins=out+s->d;else p->d.ind=s->d;continue;}
        if(s->returned==127 || s->returned==131){p->s.ind=s->s;p->d.ind=s->d;continue;}
        if(s->returned==254){p->reg=0;p->s.ind=s->s;p->d.ind=s->d;continue;}
        if(s->returned==255){if(s->op==IINDL || s->op==IINDX){p->s.ind=s->s;p->d.ind=s->d;}else if(USRC(s->mode)&AIND){p->s.i.f=48;p->s.i.s=0;p->d.ind=s->d;}else{p->s.ind=s->s;p->d.i.f=48;p->d.i.s=0;}continue;}
        if(s->returned==253){p->s.ind=s->s;p->d.ind=s->d;continue;}
        if(s->returned==252){p->s.ind=s->s;p->d.ind=s->d;continue;}
        if(s->returned>=248 && s->returned<=250){p->d.ind=s->d;if(s->returned==248 && (USRC(s->mode)&AIND)){p->s.i.f=s->s;p->s.i.s=8;}else p->s.ind=s->s;continue;}
        if(s->returned==246 || s->returned==247){p->s.ind=s->s;p->d.i.f=32;p->d.i.s=0;continue;}
        if(s->returned==242){p->d.ind=s->d;continue;}
        if(s->returned==243 || s->returned==244){p->reg=0;p->d.i.f=s->d;p->d.i.s=s->m;continue;}
        if(s->returned==172){p->d.ind=s->d;continue;}
        if(s->returned==174){p->d.i.f=32;p->d.i.s=0;continue;}
        if(s->returned==173){p->reg=0;p->s.i.f=s->s;p->s.i.s = s->m;p->d.i.f=s->d;p->d.i.s=s->m;continue;}
        if(s->returned>=Native64TypedContextNil && s->returned<Native64TypedContextNil+3){
            p->s.i.f=s->s;p->s.i.s=8*(s->returned-Native64TypedContextNil);p->d.ins=out+s->d;continue;
        }
        if(s->returned>=Native64TypedWindowNil && s->returned<Native64TypedWindowNil+6){
            p->s.i.f=s->s;p->s.i.s=native64windowfieldoffset(s->returned-Native64TypedWindowNil);p->d.ins=out+s->d;continue;
        }
        if(s->returned>=Native64TypedDrawNil && s->returned<Native64TypedDrawNil+11){
            p->s.i.f=s->s;p->s.i.s=native64drawfieldoffset(s->returned-Native64TypedDrawNil);p->d.ins=out+s->d;continue;
        }
        if(s->returned==Native64TypedContext || s->returned==Native64TypedWindow || s->returned==Native64TypedDraw){
            p->reg=0;
            if(USRC(s->mode)&AIND){p->s.i.f=s->s;p->s.i.s=s->m;p->d.ind=s->d;}
            else{p->s.ind=s->s;p->d.i.f=s->d;p->d.i.s=s->m;}
            continue;
        }
		if(native64typedbranch(s->op))p->d.ins=out+s->d;
        else if(s->returned>=168 && s->returned<=170){p->d.i.f=32;p->d.i.s=8*(s->returned-168);}
		else if(s->returned){p->d.i.f=32;p->d.i.s=s->returned==56?8:0;}
		else p->d.ind=s->d;
	}
	return 1;
}

/* Canonical compiler-proved forwarding wrapper. target/type belong to the same
 * unpublished trusted module; no native pointers are supplied by a file. */
static int
native64typedtailcode(Inst *out,unsigned int capacity,const Native64TypedStep *steps,unsigned int count,unsigned int fpbytes,unsigned int mpbytes,Type *callee,Inst *target,unsigned int ordinal)
{
    unsigned int i,sm;
    const Native64TypedStep *s;
    Inst *p;
    if(!native64model() || out==nil || steps==nil || count<4 || count>65536 || capacity<count ||
       callee==nil || callee==H || target==nil || target==H || ordinal>536870911 ||
       fpbytes<64 || fpbytes>1024*1024 || fpbytes%8 || mpbytes>1024*1024 || mpbytes%8 ||
       callee->size<64 || callee->size>65536 || callee->size%8)return 0;
    s=steps;
    if(s->op!=IFRAME || s->mode!=(SRC(AIMM)|DST(AFP)) || s->m || s->d!=40 || s->returned)return 0;
    for(i=1;i<count-3;i++){
        s=steps+i;sm=USRC(s->mode);
        if((s->op!=IMOVB && s->op!=IMOVW && s->op!=IMOVL && s->op!=IMOVF && s->op!=IMOVP) ||
           s->mode!=(SRC(sm)|DST(AFP|AIND)) || s->m || s->returned!=64 ||
           !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || sm==AXXX ||
           s->d<64 || (unsigned long)s->d>=(unsigned int)callee->size || s->d>65535)return 0;
    }
    s=steps+count-3;
    if(s->op!=IMOVL || s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->s!=32 || s->m || s->d!=32 || s->returned!=64)return 0;
    s=steps+count-2;
    if(s->op!=ICALL || s->mode!=(SRC(AFP)|DST(AIMM)) || s->s!=40 || s->m || s->returned)return 0;
    s=steps+count-1;
    if(s->op!=IRET || s->mode!=(SRC(AXXX)|DST(AXXX)) || s->s || s->m || s->d || s->returned)return 0;
    for(i=0;i<count;i++){
        s=steps+i;p=out+i;memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;
        if(i==0){p->s.imm=ordinal;p->d.ind=40;}
        else if(i==count-2){p->s.ind=40;p->d.ins=target;}
        else if(i<count-1){p->s.imm=s->s;p->d.i.f=40;p->d.i.s=s->d;}
    }
    return 1;
}

/* One proved call island at entry followed by ordinary typed instructions.
 * Target/Type/ordinal identity is supplied by the trusted unpublished builder. */
static int
native64typedlocalcode(Inst *out,unsigned int capacity,const Native64TypedStep *steps,unsigned int count,unsigned int end,unsigned int fpbytes,unsigned int mpbytes,Type *callee,Inst *target,unsigned int ordinal)
{
    unsigned int i,sm,width,contract;Inst probe,*p;Native64TypedStep checkstep;const Native64TypedStep *s;
    if(!native64model() || out==nil || steps==nil || count<4 || count>65536 || end<2 || end>=count-1 || capacity<count ||
       callee==nil || callee==H || target==nil || target==H || ordinal>536870911 ||
       fpbytes<64 || fpbytes>1024*1024 || fpbytes%8 || mpbytes>1024*1024 || mpbytes%8 ||
       callee->size<64 || callee->size>65536 || callee->size%8)return 0;
    s=steps;
    if(s->op!=IFRAME || s->mode!=(SRC(AIMM)|DST(AFP)) || s->m || s->d!=40 || s->returned)return 0;
    for(i=1;i<end-1;i++){
        s=steps+i;sm=USRC(s->mode);
        if((s->op!=IMOVB && s->op!=IMOVW && s->op!=IMOVL && s->op!=IMOVF && s->op!=IMOVP) ||
           s->mode!=(SRC(sm)|DST(AFP|AIND)) || s->m || s->returned!=64 ||
           !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || sm==AXXX ||
           s->d<64 || (unsigned long)(s->d)>=(unsigned int)callee->size || s->d>65535)return 0;
    }
    s=steps+end-1;contract=s->returned&31;width=contract&15;
    if(s->op!=ILEA || s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->m || s->d!=32 || s->returned!=(128|contract) ||
       (width!=1 && width!=4 && width!=8) || ((contract&16) && width!=8) || s->s<64 ||
       (unsigned long)s->s>fpbytes || width>fpbytes-s->s || s->s%width)return 0;
    s=steps+end;
    if(s->op!=ICALL || s->mode!=(SRC(AFP)|DST(AIMM)) || s->m || s->s!=40 || s->returned)return 0;
    for(i=end+1;i<count;i++){
        checkstep=steps[i];
        if(native64typedbranch(checkstep.op)){
            if(checkstep.d<0 || (unsigned long)checkstep.d>=count || ((unsigned long)checkstep.d<=end))return 0;
            checkstep.d=0;
        }
        if(!native64typedcode(&probe,1,&checkstep,1,fpbytes,mpbytes))return 0;
    }
    for(i=0;i<count;i++){
        s=steps+i;p=out+i;
        if(i>end){
            checkstep=*s;if(native64typedbranch(s->op))checkstep.d=0;
            native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);
            if(native64typedbranch(s->op))p->d.ins=out+s->d;
            continue;
        }
        memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;
        if(i==0){p->s.imm=ordinal;p->d.ind=40;}
        else if(i==end){p->s.ind=40;p->d.ins=target;}
        else{p->s.imm=s->s;p->d.i.f=40;p->d.i.s=s->d;if(i==end-1)p->reg=contract;}
    }
    return 1;
}
#endif
