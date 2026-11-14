/* Included after client slots/bit helpers. Internal classes, no public kind IDs. */
static unsigned int
dis64clientrootkind(Dis64ClientSlot *slots,unsigned int count,unsigned int mode,long offset)
{
 unsigned int i;
 if(mode==AMP)return offset==8?Dis64ClientRoot:offset==16?Dis64ClientString:0;
 if(mode==AFP)for(i=0;i<count;i++)if(slots[i].offset==(unsigned long)offset && dis64clientisroot(slots[i].kind))return slots[i].kind;
 return 0;
}
/* Only exact mapped FP cells and the two readonly managed MP cells are roots. */
static int
dis64clientrootoperand(Dis64ClientSlot *slots,unsigned int count,unsigned int mode,long offset,unsigned int kind,unsigned int bits[2])
{
 unsigned int i,actual=dis64clientrootkind(slots,count,mode,offset);
 if(!actual || (actual!=Dis64ClientRoot && kind!=Dis64ClientRoot && actual!=kind))return 0;
 if(mode==AMP)return 1;
 for(i=0;i<count;i++)if(slots[i].offset==(unsigned long)offset){
  if(actual==Dis64ClientRoot && kind!=Dis64ClientRoot)slots[i].kind=kind;
  bits[i/32]|=1U<<(i%32);return 1;
 }
 return 0;
}
static int
dis64clientmanagedop(const Dis64Instruction *in)
{
 switch(in->op){case IMOVP:case ILENC:case ILENL:case ICONSP:case IHEADP:case ITAIL:return 1;}
 return (in->op==IBEQL || in->op==IBNEL) && in->mode==(SRC(AFP)|AXINM|DST(AIMM));
}
static int
dis64clientmanagedroles(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,
 const Dis64Instruction *in,unsigned int need[2],unsigned int give[2])
{
 unsigned int source,destination,sk=Dis64ClientRoot,dk=Dis64ClientRoot;
 if(in->op==IBEQL || in->op==IBNEL)return in->middle==8 &&
  dis64clientrootoperand(slots,*count,AFP,in->source,Dis64ClientRoot,need);
 if((in->mode&ARM)!=AXNON || UDST(in->mode)!=AFP)return 0;
 if(in->op==IMOVP){
  source=dis64clientrootkind(slots,*count,USRC(in->mode),in->source);
  destination=dis64clientrootkind(slots,*count,AFP,in->destination);
  if(!source || !destination)return 0;
  sk=dk=source==Dis64ClientRoot?destination:source;
 }else switch(in->op){
 case ILENC:sk=Dis64ClientString;break;
 case ILENL:case ITAIL:sk=Dis64ClientListString;dk=Dis64ClientListString;break;
 case ICONSP:sk=Dis64ClientString;dk=Dis64ClientListString;break;
 case IHEADP:sk=Dis64ClientListString;dk=Dis64ClientString;break;
 default:return 0;
 }
 if(!dis64clientrootoperand(slots,*count,USRC(in->mode),in->source,sk,need))return 0;
 if(in->op==ILENC || in->op==ILENL)return dis64clientbit(slots,count,frame,AFP,in->destination,Dis64Int,give);
 /* CONS reads the previous destination as its tail; middle operands are forbidden. */
 if(in->op==ICONSP && !dis64clientrootoperand(slots,*count,AFP,in->destination,dk,need))return 0;
 return dis64clientrootoperand(slots,*count,AFP,in->destination,dk,give);
}
