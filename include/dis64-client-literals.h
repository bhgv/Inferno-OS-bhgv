/* Existing private MP24+String roots / trailing Rune pool convention.
 * No new public kind or section. An independent Context descriptor is required
 * when MP grows; source buffers remain borrowed only during construction. */
#ifndef DIS64_CLIENT_LITERALS_H
#define DIS64_CLIENT_LITERALS_H
#include "dis64-managed-trial.h"
static int
dis64clientdatacheck(Dis64ScalarClientView *v)
{
 Dis64Type mp,context;DisRuneView value;unsigned int i,j,map,total=0,base=v->imports.count+3;
 if(v->strings.count<base || v->strings.count-base>Dis64ManagedLiterals)return 0;
 v->literals=v->strings.count-base;v->contexttype=v->literals?2:1;
 if(v->module.datatype!=1 || v->module.databytes!=24+8*v->literals || v->types.count!=(v->literals?3U:2U) ||
    !dis64typeget(&v->types,1,&mp) || mp.bytes!=v->module.databytes || mp.align!=8 || mp.mapbytes!=(3+v->literals+7)/8)return 0;
 for(i=0;i<mp.mapbytes;i++){
  map=0;for(j=0;j<8 && 8*i+j<3+v->literals;j++)map|=0x80U>>j;
  if(mp.map[i]!=map)return 0;
 }
 if(!dis64typeget(&v->types,v->contexttype,&context) || context.bytes!=24 || context.align!=8 || context.mapbytes!=1 || context.map[0]!=0xe0)return 0;
 for(i=0;i<v->literals;i++){
  if(!disstringpoolget(&v->strings,base+i,&value) || value.count>255 || value.count>Dis64ManagedLiteralRunes-total)return 0;
  total+=value.count;
 }
 return 1;
}
/* Every extra mapped String is actually used; no out-of-range MP or data-type
 * allocation may borrow a larger descriptor as a Context. */
static int
dis64clientliteraluses(Dis64ScalarClientView *v)
{
 unsigned int i,j,seen=0,expected;Dis64Instruction *in;long index;
 for(i=0;i<v->count;i++){
  in=&v->decoded[i].instruction;
  if((in->op==INEW || in->op==INEWZ) && (unsigned long)in->source!=v->contexttype)return 0;
  for(j=0;j<2;j++){
   long source=j?in->middle:in->source;
   if((j?(in->mode&ARM)!=AXINM:USRC(in->mode)!=AMP) || source<24)continue;
   index=(source-24)/8;
   if(source%8 || (unsigned long)index>=v->literals)return 0;
   seen|=1U<<index;
  }
 }
 expected=v->literals==32?0xffffffffU:(1U<<v->literals)-1;
 return seen==expected;
}
#endif
