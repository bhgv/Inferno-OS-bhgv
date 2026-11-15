/* Included after client slots/bit helpers. Internal classes, no public kind IDs. */
static unsigned int
dis64clientrootkind(Dis64ClientSlot *slots,unsigned int count,unsigned int mode,long offset)
{
 unsigned int i;
 if(mode==AMP)return offset==8?Dis64ClientRoot:(offset==16 || (offset>=24 && offset%8==0 && offset<24+8*Dis64ManagedLiterals))?Dis64ClientString:0;
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
 switch(in->op){case IMOVP:return UDST(in->mode)==AFP;case INEW:case INEWZ:case IINDC:case IBEQC:case IBNEC:case IBLTC:case IBLEC:case IBGTC:case IBGEC:case ILENC:case ILENL:case ICONSP:case IHEADP:case ITAIL:return 1;}
 return (in->op==IBEQL || in->op==IBNEL) && in->mode==(SRC(AFP)|AXINM|DST(AIMM));
}
/* LONG numeric equality uses the same opcode/mode. A mapped root operand,
 * not the opcode alone, selects reference-role checking. Mixed roles refuse. */
static int
dis64clientreferencecompare(Dis64ClientSlot *slots,unsigned int count,const Dis64Instruction *in)
{
 return (in->op==IBEQL || in->op==IBNEL) && in->mode==(SRC(AFP)|AXINF|DST(AIMM)) &&
  (dis64clientrootkind(slots,count,AFP,in->source) || dis64clientrootkind(slots,count,AFP,in->middle));
}
static int
dis64clientmanagedroles(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,
 const Dis64Instruction *in,unsigned int need[2],unsigned int give[2])
{
 unsigned int source,destination,sk=Dis64ClientRoot,dk=Dis64ClientRoot;
 if(in->op==IINDC){
  unsigned int mm=in->mode&ARM;
  if(UDST(in->mode)!=AFP || (mm!=AXIMM && mm!=AXINF) || (mm==AXIMM && (in->middle < -32768 || in->middle>32767)))return 0;
  return dis64clientrootoperand(slots,*count,USRC(in->mode),in->source,Dis64ClientString,need) &&
   dis64clientbit(slots,count,frame,mm==AXIMM?AIMM:AFP,in->middle,Dis64Int,need) &&
   dis64clientbit(slots,count,frame,AFP,in->destination,Dis64Int,give);
 }
 if(in->op==IBEQC || in->op==IBNEC || in->op==IBLTC || in->op==IBLEC || in->op==IBGTC || in->op==IBGEC){
  unsigned int mm=in->mode&ARM;
  return UDST(in->mode)==AIMM && (mm==AXINF || mm==AXINM) &&
   dis64clientrootoperand(slots,*count,USRC(in->mode),in->source,Dis64ClientString,need) &&
   dis64clientrootoperand(slots,*count,mm==AXINF?AFP:AMP,in->middle,Dis64ClientString,need);
 }
 if(in->op==INEW || in->op==INEWZ)return in->mode==(SRC(AIMM)|DST(AFP)) && (in->source==1 || in->source==2) &&
  dis64clientrootoperand(slots,*count,AFP,in->destination,Dis64ClientOpaque,give);
 if(in->op==IBEQL || in->op==IBNEL){
  if((in->mode&ARM)==AXINM)return in->middle==8 && dis64clientrootoperand(slots,*count,AFP,in->source,Dis64ClientRoot,need);
  source=dis64clientrootkind(slots,*count,AFP,in->source);destination=dis64clientrootkind(slots,*count,AFP,in->middle);
  if(!source || !destination)return 0;sk=source==Dis64ClientRoot?destination:source;
  return dis64clientrootoperand(slots,*count,AFP,in->source,sk,need) && dis64clientrootoperand(slots,*count,AFP,in->middle,sk,need);
 }
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

/* Context fields have disjoint opaque roles; none is a Context/list/string. */
static int
dis64clientfieldop(const Dis64CallDecoded *d)
{
 const Dis64Instruction *in=&d->instruction;
 return ((in->op==IBEQL || in->op==IBNEL) && USRC(in->mode)==(AFP|AIND)) || in->op==IMOVP && (USRC(in->mode)==(AFP|AIND) || (UDST(in->mode)==(AFP|AIND) && d->secondary<24));
}
static int
dis64clientfieldroles(Dis64ClientSlot *slots,unsigned int count,const Dis64CallDecoded *d,unsigned int need[2],unsigned int give[2])
{
 const Dis64Instruction *in=&d->instruction;int read=USRC(in->mode)==(AFP|AIND);unsigned int kind;
 if(((in->op==IBEQL || in->op==IBNEL)?in->mode!=(SRC(AFP|AIND)|AXINM|DST(AIMM)) || in->middle!=8:(in->mode&ARM)!=AXNON) || d->secondary<0 || d->secondary>16 || d->secondary%8 ||
    (in->op==IMOVP && (read?UDST(in->mode)!=AFP:UDST(in->mode)!=(AFP|AIND))))return 0;
 kind=Dis64ClientDisplay+d->secondary/8;
 if(!dis64clientrootoperand(slots,count,AFP,read?in->source:in->destination,Dis64ClientOpaque,need))return 0;
 if(in->op!=IMOVP)return 1;
 return read?dis64clientrootoperand(slots,count,AFP,in->destination,kind,give):
  dis64clientrootoperand(slots,count,USRC(in->mode),in->source,kind,need);
}
