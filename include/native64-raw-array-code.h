#ifndef NATIVE64_RAW_ARRAY_CODE_H
#define NATIVE64_RAW_ARRAY_CODE_H
#include "native64-raw-array-runtime.h"
static int native64rawslot(long n){return n==48||n==56;}
static int native64rawbranch(unsigned int op){return op==IBEQW||op==IBNEW||op==IBLTW||op==IBLEW||op==IBGTW||op==IBGEW;}
static int
native64rawfieldindex(const Native64TypedStep *step)
{
 return step->returned==80&&(USRC(step->mode)&AIND);
}

static int
native64typedrawindex(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),cl=(s->m>>8)&255;
 if(s->op!=IINDX||s->m<0||!native64rawarraywidth(cl)||!native64rawslot(s->m&255)||
    s->mode!=(SRC(sm)|AXINF|DST(dm))||!native64recordoperand(sm,s->s,8,1,fp,mp,0))
  return 0;
 if(s->returned==Native64TypedBorrowIndex)
  return !(sm&AIND)&&dm==(AFP|AIND)&&native64rawslot(s->d)&&(s->m>>16)<=40&&(s->m>>16)%4==0;
 return s->returned==80&&(s->m>>16)==0&&native64recordoperand(dm,s->d,4,0,fp,mp,0);
}
static int
native64typedrawnew(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{return s->returned==79&&s->op==INEWAZ&&s->m>0&&s->m<64&&native64rawarraywidth(s->m)&&s->mode==(SRC(USRC(s->mode))|AXIMM|DST(UDST(s->mode)))&&native64recordoperand(USRC(s->mode),s->s,4,0,fp,mp,0)&&native64recordoperand(UDST(s->mode),s->d,8,1,fp,mp,0);}
static int native64typedrawslice(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),arm=s->mode&ARM;return s->returned==79&&s->mode<=255&&s->m>=0&&((unsigned long)s->m>>24)<64&&native64rawarraywidth((unsigned long)s->m>>24)&&(s->m&0xff0000)==0&&native64recordoperand(UDST(s->mode),s->d,8,1,fp,mp,0)&&(s->op==ISLICEA?native64recordoperand(sm,s->s,4,0,fp,mp,0):s->op==ISLICELA&&native64recordoperand(sm,s->s,8,1,fp,mp,0))&&(arm==AXIMM||((arm==AXINF||arm==AXINM)&&native64typedwordoperand(arm==AXINF?AFP:AMP,s->m&65535,fp,mp)));}
static unsigned int native64rawusewidth(const Native64TypedStep*s){return s->op==IMOVMP?native64arraywidth(s->m&65535):s->op==IMOVP?8:s->op==IMOVM||s->op==IHEADM?s->m&65535:4;}
static int
native64typedrawuse(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),arm=s->mode&ARM,n=native64rawusewidth(s),so=(s->m>>24)&255,d=(s->m>>16)&255,mid=s->m&65535,branch=native64rawbranch(s->op),i;long value[2]={s->s,s->d};unsigned int mode[2]={sm,dm};
 if(s->returned!=65||s->m<0||(unsigned long)s->m>0xffffffffUL||s->mode>255||!n||(s->op!=IMOVMP&&n!=4&&n!=8&&n!=12&&n!=16)||so%4||d%4||so>40||d>40)return 0;
 switch(s->op){case IMOVMP:if(arm!=AXIMM||!native64arrayrecord(mid))return 0;break;case IMOVP:if(arm||mid)return 0;break;case IMOVM:case IHEADM:if(arm!=AXIMM)return 0;break;case IMOVW:if(arm||mid)return 0;break;case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:break;default:if(!branch)return 0;}
 if(s->op==IHEADM&&(n!=16||sm==(AFP|AIND)||!native64typedrootoperand(sm,s->s,fp,mp)))return 0;
 if(!(sm==(AFP|AIND)&&native64rawslot(s->s))&&!(dm==(AFP|AIND)&&native64rawslot(s->d)))return 0;
 for(i=0;i<2;i++){if(branch&&i){if(dm!=AIMM||s->d<0)return 0;continue;}if(mode[i]==(AFP|AIND)){if(!native64rawslot(value[i]) && !(i&&value[i]==32&&(s->op==IMOVW||s->op==IMOVP)&&sm==(AFP|AIND)) && (!i||value[i]<64||value[i]>65535||value[i]%4))return 0;}
  else if(s->op==IMOVMP&&value[i]%8)return 0;
  else if(s->op==IHEADM&&!i)continue;
  else if(s->op==IMOVP){if(!native64typedrootoperand(mode[i],value[i],fp,mp))return 0;}
  else if(n==4){if(!native64typedwordoperand(mode[i],value[i],fp,mp)||(i&&mode[i]==AIMM))return 0;}
  else{unsigned int bytes=mode[i]==AFP?fp:mp;if((mode[i]!=AFP&&mode[i]!=AMP)||value[i]<0||(mode[i]==AFP&&value[i]<64)||value[i]%4||(unsigned long)value[i]>bytes||n>bytes-value[i])return 0;}}
 if(so&&sm!=(AFP|AIND))return 0;if(d&&(dm!=(AFP|AIND)||!native64rawslot(s->d)))return 0;
 if(s->op!=IMOVM&&s->op!=IHEADM&&s->op!=IMOVMP&&s->op!=IMOVP){if(arm==AXINF&&!native64typedwordoperand(AFP,mid,fp,mp))return 0;if(arm==AXINM&&!native64typedwordoperand(AMP,mid,fp,mp))return 0;if(branch&&arm==AXNON)return 0;}
 return 1;}
static void
native64rawusecode(Inst*p,const Native64TypedStep*s)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),arm=s->mode&ARM;memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;p->reg=arm==AXIMM?(short)(s->m&65535):s->m&65535;
 if(sm==(AFP|AIND)){p->s.i.f=s->s;p->s.i.s=(s->m>>24)&255;}else p->s.imm=s->s;
 if(dm==(AFP|AIND)){p->d.i.f=native64rawslot(s->d)||s->d==32?s->d:40;p->d.i.s=s->d==32?0:native64rawslot(s->d)?(s->m>>16)&255:s->d;}else p->d.imm=s->d;}
static int
native64rawusefits(const Native64TypedStep *use, unsigned int cl, unsigned int off)
{
 unsigned int width=native64arraywidth(cl),n=native64rawusewidth(use);
 if(!width||off>width||n>width-off)
  return 0;
 if(use->op==IMOVMP)
  return native64arraycopyspan(cl,off,use->m&65535);
 if(use->op==IMOVP)
  return off%8==0&&off+8<=8*native64arrayroots(cl);
 return !native64arrayroots(cl)||(n==4&&native64arrayword(cl,off));
}

static int
native64rawindexprep(const Native64TypedStep *s, unsigned int fp, unsigned int mp)
{
 Native64TypedStep step=*s;
 Inst probe;
 if(s->returned!=Native64TypedArrayArgIndex||(s->op!=IADDW&&s->op!=ISUBW)||
    (USRC(s->mode)&AIND)||UDST(s->mode)!=AFP)
  return 0;
 step.returned=0;
 return native64typedcode(&probe,1,&step,1,fp,mp);
}

static int
native64rawargindexchain(const Native64TypedStep *prep, unsigned int fp, unsigned int mp)
{
 const Native64TypedStep *index=prep+1,*arg=prep+2;
 return native64rawindexprep(prep,fp,mp)&&index->returned==80&&
  UDST(index->mode)==AFP&&index->d==prep->d&&
  arg->returned==65&&USRC(arg->mode)==(AFP|AIND)&&arg->s==(index->m&255);
}

static int
native64rawrecordside(const Native64TypedStep *step)
{
 return USRC(step->mode)==(AFP|AIND)&&native64rawslot(step->s);
}

static int
native64typedrawrecord(const Native64TypedStep *s, unsigned int fp, unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),mm=s->mode&ARM;
 unsigned int cl=s->m&65535,so=(s->m>>24)&255,d=(s->m>>16)&255;
 int side=native64rawrecordside(s);
 unsigned int mode=side?dm:sm,off=side?so:d;
 long field=side?s->d:s->s;

 if(s->returned!=Native64TypedRecordArray||s->mode>255||s->m<0||
    (unsigned long)s->m>0xffffffffUL||
    !(side^(dm==(AFP|AIND)&&native64rawslot(s->d)))||
    (side?d:so)||off%8||off>40||
    !(mode&AIND)||!native64recordoperand(mode,field,8,1,fp,mp,0))
  return 0;
 return s->op==IMOVP?mm==AXNON&&!cl:
  s->op==IMOVMP&&mm==AXIMM&&native64arrayrecord(cl);
}

static void
native64rawrecordcode(Inst *out, const Native64TypedStep *s)
{
 Adr *adr;
 unsigned long field;
 int side=native64rawrecordside(s);
 native64rawusecode(out,s);
 adr=side?&out->d:&out->s;
 field=side?s->d:s->s;
 adr->i.f=field&65535;
 adr->i.s=field>>16;
}

static int
native64rawfieldpair(const Native64TypedStep *index, const Native64TypedStep *use,
                     unsigned int fp, unsigned int mp)
{
 long slot=index->m&255;
 unsigned int sm=USRC(use->mode),dm=UDST(use->mode),off;
 int read=sm==(AFP|AIND)&&use->s==slot;
 int write=dm==(AFP|AIND)&&use->d==slot;

 if(read==write)
  return 0;
 if(use->returned==Native64TypedRecordArray){
  if(!native64typedrawrecord(use,fp,mp))return 0;
 }else if(!native64typedrawuse(use,fp,mp)||
          (read&&dm==(AFP|AIND)&&use->d!=32)||(write&&(sm&AIND)))
  return 0;
 off=(use->m>>(read?24:16))&255;
 return native64rawusefits(use,(unsigned long)index->m>>8,off);
}
#endif
