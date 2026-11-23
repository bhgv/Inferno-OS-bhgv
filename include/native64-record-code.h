#ifndef NATIVE64_RECORD_CODE_H
#define NATIVE64_RECORD_CODE_H
#include "native64-record-layout.h"
static int
native64recordoperand(unsigned int mode,long value,unsigned int width,unsigned int root,unsigned int fp,unsigned int mp,int result)
{
 unsigned int field,base;
 if(result&&mode==(AFP|AIND)&&value==32)return 1;
 if(mode==AFP||mode==AMP)return root?native64typedrootoperand(mode,value,fp,mp):native64typedwordoperand(mode,value,fp,mp);
 if(mode==AIMM)return !root&&native64typedwordoperand(mode,value,fp,mp);
 if((mode!=(AFP|AIND)&&mode!=(AMP|AIND))||value<0||(unsigned long)value>0x1000ffffUL)return 0;
 base=value&65535;field=(unsigned long)value>>16;
 return field<=4096&&field%width==0&&native64typedrootoperand(mode&~AIND,base,fp,mp);
}
static int
native64typedrecord(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int w[2],r[2],sm=USRC(s->mode),dm=UDST(s->mode),mm=s->mode&ARM,branch=native64typedbranch(s->op);
 if(s->returned!=Native64TypedRecord||s->mode>255||!native64recordshape(s->op,w,r)||
    (!(sm&AIND)&&!(dm&AIND))||!native64recordoperand(sm,s->s,w[0],r[0],fp,mp,0))return 0;
 if(s->op==ICASE||s->op==IGOTO)
  return (sm&AIND)&&dm==AMP&&mm==AXNON&&!s->m&&
   native64typedwordoperand(AMP,s->d,fp,mp)&&(s->op!=IGOTO||s->d>=4);
 if(branch){if(dm!=AIMM||s->d<0||mm==AXNON)return 0;}
 else if(dm==AIMM||!native64recordoperand(dm,s->d,w[1],r[1],fp,mp,1))return 0;
 if(mm==AXNON)return s->op!=IINDC&&!s->m;
 if(s->op==IMOVP||s->op==IMOVW||s->op==ILENC||s->op==ILENA||s->op==ILENL||s->op==ICVTCW||s->op==ICVTWC)return 0;
 if(mm==AXIMM)return (!r[0]||s->op==IINDC)&&s->m>=-32768&&s->m<=32767;
 return s->m>=0&&s->m<=65535&&native64recordoperand(mm==AXINF?AFP:AMP,s->m,s->op==IINDC?4:w[0],s->op==IINDC?0:r[0],fp,mp,0);
}
static void
native64recordcode(Inst*p,const Native64TypedStep*s,Inst*out)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 p->reg=s->m;
 if(sm&AIND){p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;}else p->s.imm=s->s;
 if(native64typedbranch(s->op))p->d.ins=out+s->d;
 else if(dm&AIND){p->d.i.f=s->d&65535;p->d.i.s=(unsigned long)s->d>>16;}else p->d.imm=s->d;
}

static int
native64recordargshape(const Native64TypedStep *s, unsigned int fp, unsigned int mp)
{
 unsigned int sm=USRC(s->mode);
 int root=s->op==IMOVP, length=s->op==ILENA&&s->returned==Native64TypedRecordArg;
 unsigned int width=root||s->returned==Native64TypedRecordVararg?8:4;

 if(s->returned!=Native64TypedRecordArg&&s->returned!=Native64TypedRecordVararg)
  return 0;
 return (root||length||s->op==(width==8?ICVTWL:IMOVW))&&!s->m&&
  s->mode==(SRC(sm)|DST(AFP|AIND))&&(sm&AIND)&&
  native64recordoperand(sm,s->s,root||length?8:4,root||length,fp,mp,0)&&
  s->d>=64&&s->d<=65535&&s->d%width==0;
}

static int
native64recordarglayout(const Native64TypedStep *s, unsigned int fp, unsigned int mp,
                       unsigned int bytes, const uchar *map, unsigned int np)
{
 unsigned int bit,width;
 int root=s->op==IMOVP;

 if(!native64recordargshape(s,fp,mp)||map==nil||map==H||np!=(bytes+63)/64)
  return 0;
 width=root||s->returned==Native64TypedRecordVararg?8:4;
 if((unsigned long)s->d>bytes||width>bytes-s->d)
  return 0;
 bit=s->d/8;
 return !!(map[bit/8]&(128>>(bit%8)))==root;
}

static void
native64recordargcode(Inst *out, const Native64TypedStep *s)
{
 out->s.i.f=s->s&65535;
 out->s.i.s=(unsigned long)s->s>>16;
 out->d.i.f=40;
 out->d.i.s=s->d;
}

static int
native64typedrecordnew(const Native64TypedStep *s, unsigned int fp, unsigned int mp)
{
 unsigned int dm=UDST(s->mode);
 return s->returned==Native64TypedRecordNew&&s->op==INEWZ&&
  s->mode==(SRC(AIMM)|DST(dm))&&!s->s&&native64recorddescriptor(s->m)&&
  ((dm==(AFP|AIND)&&s->d==32)||native64typedrootoperand(dm,s->d,fp,mp));
}

static int
native64recordcopyoperand(unsigned int mode, long value, unsigned int bytes,
                         unsigned int fp, unsigned int mp, int mapped)
{
 unsigned int limit=mode==AFP?fp:mp;
 if(mode&AIND)return native64recordoperand(mode,value,mapped?8:4,mapped,fp,mp,0);
 return (mode==AFP||mode==AMP)&&(mapped?native64typedrootoperand(mode,value,fp,mp):native64typedwordoperand(mode,value,fp,mp))&&
  (unsigned long)value<=limit&&bytes<=limit-value;
}

static int
native64typedrecordcopy(const Native64TypedStep *s, unsigned int fp, unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),bytes;
 int mapped=s->op==IMOVMP;
 if(s->returned!=Native64TypedRecordCopy||(!mapped&&s->op!=IMOVM)||
    s->mode!=(SRC(sm)|AXIMM|DST(dm))||
    !native64recorddescriptor(s->m)||!!native64recordmask(s->m)!=mapped||(!mapped&&!((sm|dm)&AIND)))
  return 0;
 bytes=native64recordbytes(s->m);
 return native64recordcopyoperand(sm,s->s,bytes,fp,mp,mapped)&&
  native64recordcopyoperand(dm,s->d,bytes,fp,mp,mapped);
}

static int
native64typedrecordresult(const Native64TypedStep *s, unsigned int fp)
{
 return s->returned==Native64TypedRecordResult&&s->op==ILEA&&
  s->mode==(SRC(AFP|AIND)|DST(AFP|AIND))&&!s->m&&s->d==32&&
  native64recordoperand(AFP|AIND,s->s,8,1,fp,0,0);
}

static int
native64typedrecordpair(const Native64TypedStep *s, unsigned int fp, unsigned int mp)
{
 unsigned int sm=USRC(s->mode);
 return s->returned==Native64TypedRecordPair&&s->op==IMOVP&&
  s->mode==(SRC(sm)|DST(AFP|AIND))&&!s->m&&s->d==32&&(sm&AIND)&&
  native64recordoperand(sm,s->s,8,1,fp,mp,0);
}

static void
native64recordresultcode(Inst *out, const Native64TypedStep *s)
{
 out->reg=Native64RecordResultFlag|24;
 out->s.i.f=s->s&65535;
 out->s.i.s=(unsigned long)s->s>>16;
 out->d.i.f=40;
 out->d.i.s=32;
}
#endif
