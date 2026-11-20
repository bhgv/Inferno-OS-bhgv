#ifndef NATIVE64_AGGREGATE_RESULT_H
#define NATIVE64_AGGREGATE_RESULT_H
/* Closed flattened results. Padding of String/Word is never a writable field. */
static unsigned int native64aggregatewidth(unsigned int c){return c==12?12:c==48||c==80?16:c==92?20:c==112?16:c==96?8:c==72||c==124?24:0;}
static unsigned int native64aggregatefields(unsigned int c){return c==92?5:c==112?4:c==96?2:c==12||c==72||c==124?3:c==48||c==80?2:0;}
static int native64aggregateroot(unsigned int c,unsigned int off){return c==48||c==72||c==124||(c==80&&off<8);}
static unsigned int
native64aggregatewrites(const Native64TypedStep*s)
{unsigned int c=(unsigned long)s->m>>16,off=s->m&255,n=(s->m>>8)&255,first,fields;if(!native64aggregatefields(c))return 0;first=c==80?(off?1:0):off/((c==48||c==72||c==124)?8:4);fields=s->op==IMOVP?1:n/((c==48||c==72||c==124)?8:4);return ((1U<<fields)-1)<<(first+1);}
static int
native64typedaggregate(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{unsigned int c=(unsigned long)s->m>>16,off=s->m&255,n=(s->m>>8)&255,w=native64aggregatewidth(c),sm=USRC(s->mode),block=s->op==IMOVM||s->op==IMOVMP,total=sm==AFP?fp:mp;
 if(s->returned!=77||s->m<0||!w||s->d!=32||!n||off>w||n>w-off||s->mode!=(SRC(sm)|(block?AXIMM:0)|DST(AFP|AIND)))return 0;
 if(s->op==IMOVP)return n==8&&off%8==0&&native64aggregateroot(c,off)&&native64typedrootoperand(sm,s->s,fp,mp);
 if(s->op==IMOVW)return n==4&&off%4==0&&!native64aggregateroot(c,off)&&(c!=80||off==8)&&native64typedwordoperand(sm,s->s,fp,mp);
 if(sm!=AFP&&sm!=AMP)return 0;
 if(s->op==IMOVMP)return (c==48||c==72||c==124)&&!off&&n==w&&native64typedrootoperand(sm,s->s,fp,mp)&&s->s>=0&&(unsigned long)s->s<=total&&n<=total-s->s;
 return s->op==IMOVM&&(c==12||c==92||c==96||c==112)&&off%4==0&&n%4==0&&native64typedwordoperand(sm,s->s,fp,mp)&&s->s>=0&&(unsigned long)s->s<=total&&n<=total-s->s;}
#endif
