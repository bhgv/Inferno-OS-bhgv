#ifndef NATIVE64_RAW_ARRAY_CODE_H
#define NATIVE64_RAW_ARRAY_CODE_H
#include "native64-raw-array-runtime.h"
static int native64rawslot(long n){return n==48||n==56;}
static int native64rawbranch(unsigned int op){return op==IBEQW||op==IBNEW||op==IBLTW||op==IBLEW||op==IBGTW||op==IBGEW;}
static int
native64typedrawindex(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{return s->returned==80&&s->op==IINDX&&s->m>=0&&native64rawarraywidth((unsigned long)s->m>>8)&&native64rawslot(s->m&255)&&s->mode==(SRC(USRC(s->mode))|AXINF|DST(UDST(s->mode)))&&native64typedrootoperand(USRC(s->mode),s->s,fp,mp)&&native64typedwordoperand(UDST(s->mode),s->d,fp,mp);}
static int
native64typedrawnew(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{return s->returned==79&&s->op==INEWAZ&&native64rawarraywidth(s->m)&&s->mode==(SRC(USRC(s->mode))|AXIMM|DST(UDST(s->mode)))&&native64typedwordoperand(USRC(s->mode),s->s,fp,mp)&&native64typedrootoperand(UDST(s->mode),s->d,fp,mp);}
static int native64typedrawslice(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),arm=s->mode&ARM;return s->returned==79&&s->m>=0&&native64rawarraywidth((unsigned long)s->m>>24)&&(s->m&0xff0000)==0&&native64typedrootoperand(UDST(s->mode),s->d,fp,mp)&&(s->op==ISLICEA?native64typedwordoperand(sm,s->s,fp,mp):s->op==ISLICELA&&native64typedrootoperand(sm,s->s,fp,mp))&&(arm==AXIMM||((arm==AXINF||arm==AXINM)&&native64typedwordoperand(arm==AXINF?AFP:AMP,s->m&65535,fp,mp)));}
static unsigned int native64rawusewidth(const Native64TypedStep*s){return s->op==IMOVM||s->op==IHEADM?s->m&65535:4;}
static int
native64typedrawuse(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),arm=s->mode&ARM,n=native64rawusewidth(s),so=(s->m>>24)&255,d=(s->m>>16)&255,mid=s->m&65535,branch=native64rawbranch(s->op),i;long value[2]={s->s,s->d};unsigned int mode[2]={sm,dm};
 if(s->returned!=65||s->m<0||(unsigned long)s->m>0xffffffffUL||s->mode>255||!n||(n!=4&&n!=8&&n!=12&&n!=16)||so%4||d%4||so>12||d>12)return 0;
 switch(s->op){case IMOVM:case IHEADM:if(arm!=AXIMM)return 0;break;case IMOVW:if(arm||mid)return 0;break;case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:break;default:if(!branch)return 0;}
 if(s->op==IHEADM&&(n!=16||sm==(AFP|AIND)||!native64typedrootoperand(sm,s->s,fp,mp)))return 0;
 if(!(sm==(AFP|AIND)&&native64rawslot(s->s))&&!(dm==(AFP|AIND)&&native64rawslot(s->d)))return 0;
 for(i=0;i<2;i++){if(branch&&i){if(dm!=AIMM||s->d<0)return 0;continue;}if(mode[i]==(AFP|AIND)){if(!native64rawslot(value[i]) && !(i&&value[i]==32&&s->op==IMOVW&&sm==(AFP|AIND)) && (!i||value[i]<64||value[i]>65535||value[i]%4))return 0;}
  else if(s->op==IHEADM&&!i)continue;
  else if(n==4){if(!native64typedwordoperand(mode[i],value[i],fp,mp)||(i&&mode[i]==AIMM))return 0;}
  else{unsigned int bytes=mode[i]==AFP?fp:mp;if((mode[i]!=AFP&&mode[i]!=AMP)||value[i]<0||(mode[i]==AFP&&value[i]<64)||value[i]%4||(unsigned long)value[i]>bytes||n>bytes-value[i])return 0;}}
 if(so&&sm!=(AFP|AIND))return 0;if(d&&(dm!=(AFP|AIND)||!native64rawslot(s->d)))return 0;
 if(s->op!=IMOVM&&s->op!=IHEADM){if(arm==AXINF&&!native64typedwordoperand(AFP,mid,fp,mp))return 0;if(arm==AXINM&&!native64typedwordoperand(AMP,mid,fp,mp))return 0;if(branch&&arm==AXNON)return 0;}
 return 1;}
static void
native64rawusecode(Inst*p,const Native64TypedStep*s)
{unsigned int sm=USRC(s->mode),dm=UDST(s->mode),arm=s->mode&ARM;memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;p->reg=arm==AXIMM?(short)(s->m&65535):s->m&65535;
 if(sm==(AFP|AIND)){p->s.i.f=s->s;p->s.i.s=(s->m>>24)&255;}else p->s.imm=s->s;
 if(dm==(AFP|AIND)){p->d.i.f=native64rawslot(s->d)||s->d==32?s->d:40;p->d.i.s=s->d==32?0:native64rawslot(s->d)?(s->m>>16)&255:s->d;}else p->d.imm=s->d;}
#endif
