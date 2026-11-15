/* A borrowed native address exists only between an adjacent index/byte consumer.
 * The array owner is still mapped at every scheduler/GC boundary. */
static int
dis64clientindex(Dis64ScalarClientView *v,unsigned int pc,Dis64ClientSlot *slots,
 unsigned int *count,unsigned int need[2],unsigned int give[2])
{
 const Dis64Instruction *index=&v->decoded[pc].instruction,*use;Dis64CallDecoded *d;
 unsigned int mode,kind;long scratch=v->frame-16;
 if(pc+1>=v->count || index->op!=IINDB || index->mode!=(SRC(AFP)|AXINF|DST(AFP)) && index->mode!=(SRC(AFP)|AXINF|DST(AIMM)) || index->middle!=scratch)return 0;
 if(!dis64clientrootoperand(slots,*count,AFP,index->source,Dis64ClientByteArray,need) ||
    !dis64clientbit(slots,count,v->frame,UDST(index->mode),index->destination,Dis64Int,need))return 0;
 d=&v->decoded[pc+1];use=&d->instruction;if(d->secondary || (use->mode&ARM)!=AXNON)return 0;
 if((use->op==IMOVB || use->op==ICVTBW) && use->mode==(SRC(AFP|AIND)|DST(AFP))){
  if(use->source!=scratch)return 0;kind=use->op==IMOVB?Dis64ClientByte:Dis64Int;
  return dis64clientbit(slots,count,v->frame,AFP,use->destination,kind,give);
 }
 if((use->op==IMOVB || use->op==ICVTWB) && UDST(use->mode)==(AFP|AIND)){
  mode=USRC(use->mode);if(use->destination!=scratch || (mode!=AFP && mode!=AIMM))return 0;
  kind=use->op==IMOVB?Dis64ClientByte:Dis64Int;
  return dis64clientbit(slots,count,v->frame,mode,use->source,kind,need);
 }
 return 0;
}
