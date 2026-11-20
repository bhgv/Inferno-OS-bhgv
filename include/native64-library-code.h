#ifndef NATIVE64_LIBRARY_CODE_H
#define NATIVE64_LIBRARY_CODE_H
#include "native64-library-layout.h"
/* Explicit source/destination widths and GC roles, before operand decoding. */
static int
native64libraryshape(const Native64TypedStep*s,unsigned int*w,unsigned int*r)
{
 unsigned int i,id=(unsigned long)s->m>>24,field=(unsigned long)s->s>>16;
 w[0]=w[1]=4;r[0]=r[1]=0;
 switch(s->op){
 case ISLICELA:case IMOVP:case ICONSP:case IHEADP:case ITAIL:w[0]=w[1]=8;r[0]=r[1]=1;break;
 case ILENC:case ILENA:case ILENL:case IHEADW:w[0]=8;r[0]=1;break;
 case ICONSW:w[1]=8;r[1]=1;break;
 case ISLICEA:case INEWAZ:w[1]=8;r[1]=1;break;
 case IMOVW:case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:break;
 case IMOVL:case IADDL:case ISUBL:case IMULL:case IDIVL:case IMODL:case IMOVF:w[0]=w[1]=8;break;
 case ICVTWL:w[1]=8;break;case ICVTLW:w[0]=8;break;
 case IBEQL:case IBNEL:case IBLTL:case IBLEL:case IBGTL:case IBGEL:w[0]=8;if(s->op==IBEQL||s->op==IBNEL)for(i=0;i<native64libraryfields(id);i++)if(field==native64libraryoffset(id,i))r[0]=native64libraryroot(id,i);break;
 default:return 0;
 }
 return 1;
}
static long
native64librarymiddle(const Native64TypedStep*s)
{return (s->mode&ARM)==AXIMM?(short)(s->m&65535):s->m&65535;}
static int
native64libraryoperand(unsigned int mode,long value,unsigned int id,unsigned int width,unsigned int root,unsigned int fp,unsigned int mp,int result)
{
 unsigned int i,field=(unsigned long)value>>16,base=value&65535,bytes=mode==AFP?fp:mp;
 if(mode==AIMM)return !id&&!root&&width==4&&native64typedwordoperand(mode,value,fp,mp);
 if(mode&AIND){
  if(!id)return result&&mode==(AFP|AIND)&&value==32;
  if(id>Native64LibraryRecords || !native64typedrootoperand(mode&~AIND,base,fp,mp))return 0;
  for(i=0;i<native64libraryfields(id);i++)if(field==native64libraryoffset(id,i))return width==native64librarywidth(id,i)&&root==(unsigned int)native64libraryroot(id,i);
  return 0;
 }
 return !id&&native64typedcodeoperand(mode,value,fp,mp)&&value%width==0&&width<=bytes-value;
}
static int
native64typedlibrary(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),mm=s->mode&ARM,w[2],r[2],sid=(unsigned long)s->m>>24,did=(s->m>>16)&255;
 long mid=native64librarymiddle(s);
 if(s->mode>255 || s->m<0 || (unsigned long)s->m>0x05ffffffUL)return 0;
 if(s->returned==154)return s->op==INEWZ&&s->mode==(SRC(AIMM)|DST(dm))&&s->s>=1&&s->s<=Native64LibraryRecords&&!s->m&&(((dm==AFP||dm==AMP)&&native64typedrootoperand(dm,s->d,fp,mp))||(dm==(AFP|AIND)&&s->d==32));
 if(s->returned!=153 || (!sid&&!did) || !native64libraryshape(s,w,r) || !native64libraryoperand(sm,s->s,sid,w[0],r[0],fp,mp,0))return 0;
 if(native64typedbranch(s->op)){if(did||dm!=AIMM||s->d<0)return 0;}
 else if(!native64libraryoperand(dm,s->d,did,w[1],r[1],fp,mp,1) || dm==AIMM)return 0;
 if(mm==AXNON)return mid==0;
 return native64libraryoperand(mm==AXIMM?AIMM:mm==AXINF?AFP:AMP,mid,0,s->op==ISLICELA?4:w[0],s->op==ISLICELA?0:r[0],fp,mp,0);
}
static int
native64typedlibraryarg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const unsigned char*map,unsigned int np)
{
 unsigned int sm=USRC(s->mode),sid=(unsigned long)s->m>>24,w[2],r[2],i,bit;
 if(s->returned!=155 || (s->op!=IMOVP && s->op!=IMOVW && s->op!=IMOVL && s->op!=IMOVF) || s->mode!=(SRC(sm)|DST(AFP|AIND)) || (sm!=(AFP|AIND)&&sm!=(AMP|AIND)) || !sid || sid>Native64LibraryRecords || s->m!=((long)sid<<24) || !native64libraryshape(s,w,r) || !native64libraryoperand(sm,s->s,sid,w[0],r[0],fp,mp,0) || !native64libraryoperand(AFP,s->d,0,w[1],r[1],bytes,0,0) || np!=(bytes+63)/64 || map==nil || map==H)return 0;
 for(i=0;i<w[1];i++){bit=(s->d+i)/8;if(!!(map[bit/8]&(128>>(bit%8)))!=r[1])return 0;}return 1;
}
/* Only the complete Filler layout's BufioFill field may supply a module. */
static int
native64librarymoduleoperand(unsigned int mode,unsigned int offset,unsigned int fp,unsigned int mp)
{
 if(mode==AFP||mode==AMP)return native64typedrootoperand(mode,offset,fp,mp);
 return (mode==(AFP|AIND)||mode==(AMP|AIND)) && (offset>>24)==5 && ((offset>>16)&255)==8 && native64typedrootoperand(mode&~AIND,offset&65535,fp,mp);
}
static int
native64librarybyte(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),width=s->op==IMOVB?1:4;long value;
 if(s->mode>255)return 0;
 if(s->returned==157){unsigned int sid=(unsigned long)s->m>>24,did=(s->m>>16)&255;
 return s->op==IINDB && s->mode==(SRC(sm)|AXINF|DST(dm)) && (s->m&65535)==48 && sid<=1 && did<=1 && native64libraryoperand(sm,s->s,sid,8,1,fp,mp,0) && (!sid || ((unsigned long)s->s>>16)==8) && native64libraryoperand(dm,s->d,did,4,0,fp,mp,0);
 }
 if(s->returned!=158 || s->m || (s->op!=IMOVB && s->op!=ICVTBW && s->op!=ICVTWB))return 0;
 if(sm==(AFP|AIND)){if(s->s!=48 || (s->op!=IMOVB&&s->op!=ICVTBW) || s->mode!=(SRC(sm)|DST(dm)))return 0;value=s->d;sm=dm;if(dm==(AFP|AIND))return s->op==ICVTBW && s->d==32;if(sm==AIMM)return 0;}
 else{if(dm!=(AFP|AIND)||s->d!=48||(s->op!=IMOVB&&s->op!=ICVTWB)||s->mode!=(SRC(sm)|DST(dm)))return 0;value=s->s;}
 if(sm==AIMM)return value>=-536870912L && value<=536870911L && (width!=1 || (value>=0&&value<=255));
 return native64typedcodeoperand(sm,value,fp,mp) && value%width==0 && width<=(sm==AFP?fp:mp)-value;
}
static int
native64libraryslicearg(const Native64TypedStep*s,unsigned int fp,unsigned int mp,unsigned int bytes,const uchar*map,unsigned int np)
{
 unsigned int id=(unsigned long)s->m>>24,mm=s->mode&ARM,bit;long mid=native64librarymiddle(s);
 return s->returned==156 && s->op==ISLICEA && s->mode==(SRC(USRC(s->mode))|mm|DST(AFP|AIND)) && (s->m&0xff0000)==0 && id<=Native64LibraryRecords && native64libraryoperand(USRC(s->mode),s->s,id,4,0,fp,mp,0) && mm!=AXNON && native64typedwordoperand(mm==AXIMM?AIMM:mm==AXINF?AFP:AMP,mid,fp,mp) && s->d>=64 && native64typedrootoperand(AFP,s->d,bytes,0) && s->d<=65535 && np==(bytes+63)/64 && map!=nil && map!=H && (bit=s->d/8,!!(map[bit/8]&(128>>(bit%8))));
}
static int
native64librarybytecopy(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int mm=s->mode&ARM;return s->returned==159 && s->op==ISLICELA && s->mode<=255 && mm!=AXNON && native64typedrootoperand(USRC(s->mode),s->s,fp,mp) && native64typedrootoperand(UDST(s->mode),s->d,fp,mp) && native64typedwordoperand(mm==AXIMM?AIMM:mm==AXINF?AFP:AMP,s->m,fp,mp) && (mm==AXIMM?(s->m>=-32768&&s->m<=32767):s->m<=65535);}
#endif
