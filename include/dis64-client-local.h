/* Local managed calls keep Command arguments; result classes come from bodies. */
static int dis64clientbodyprove(Dis64ScalarClientView*,int,int);
static int
dis64clientresultstore(const Dis64CallDecoded *d)
{
 return (d->instruction.op==IMOVP || d->instruction.op==IHEADP || d->instruction.op==ITAIL) && UDST(d->instruction.mode)==(AFP|AIND) && d->instruction.destination==32;
}
static int
dis64clientreturn(Dis64ScalarClientView *v,const Dis64CallDecoded *d,Dis64ClientSlot *slots,unsigned int count,unsigned int need[2])
{
 const Dis64Instruction *in=&d->instruction;unsigned int kind;
 if(!v->returning || d->secondary || (in->mode&ARM)!=AXNON ||
    !dis64clientrootoperand(slots,count,USRC(in->mode),in->source,in->op==IMOVP?Dis64ClientRoot:Dis64ClientListString,need))return 0;
 kind=in->op==IHEADP?Dis64ClientString:in->op==ITAIL?Dis64ClientListString:dis64clientrootkind(slots,count,USRC(in->mode),in->source);
 kind=dis64clientmergekind(v->returnkind,kind);if(!kind)return 0;v->returnkind=kind;return 1;
}
/* Binder presence determines result arity; the destination is always a mapped
 * caller root, or the already checked inherited address in current FP32. */
static unsigned int
dis64clientlocalend(const Dis64ScalarClientView *v,unsigned int pc)
{
 if(pc>=v->count || v->count-pc<4)return 0;
 return v->decoded[pc+3].instruction.op==ILEA || v->decoded[pc+3].instruction.op==IMOVL?4:3;
}
static int
dis64clientlocalcall(Dis64ScalarClientView *v,unsigned int pc,Dis64ClientSlot *slots,unsigned int count,unsigned int need[2],unsigned int *assigned)
{
 Dis64Instruction *frame,*call,*in;Dis64CallDecoded *d;unsigned int i,j,end=dis64clientlocalend(v,pc),kind;
 if(!end || v->count-pc<=end)return 0;
 frame=&v->decoded[pc].instruction;call=&v->decoded[pc+end].instruction;
 if(frame->op!=IFRAME || frame->mode!=(SRC(AIMM)|DST(AFP)) || frame->destination!=40 ||
    call->op!=ICALL || call->mode!=(SRC(AFP)|DST(AIMM)) || call->source!=40)return 0;
 for(i=0;i<v->localcount;i++)if(frame->source==v->localtype[i] && call->destination==v->localentry[i])break;
 if(i==v->localcount || !!v->localvalue[i]!=(end==4))return 0;
 for(j=0;j<2;j++){
  d=v->decoded+pc+1+j;in=&d->instruction;
  if(in->op!=IMOVP || (in->mode&ARM)!=AXNON || UDST(in->mode)!=(AFP|AIND) || in->destination!=40 || d->secondary!=64+8*j ||
     !dis64clientrootoperand(slots,count,USRC(in->mode),in->source,j?Dis64ClientListString:Dis64ClientOpaque,need))return 0;
 }
 if(end==4){
  d=v->decoded+pc+3;in=&d->instruction;kind=v->localresult[i];
  if(!kind || in->mode!=(SRC(AFP)|DST(AFP|AIND)) || in->destination!=40 || d->secondary!=32)return 0;
  if(in->op==ILEA){unsigned int bits[2]={0};
   if(!dis64clientrootoperand(slots,count,AFP,in->source,kind,bits))return 0;
  }else{
   if(in->op!=IMOVL || in->source!=32 || !v->returning || !(kind=dis64clientmergekind(v->returnkind,kind)))return 0;
   v->returnkind=kind;*assigned=1;
  }
 }
 v->localused|=1U<<i;return 1;
}
/* Discover bodies and the full graph first; prove callees before their callers
 * so a result class can never be asserted by an untrusted call-site manifest. */
static int
dis64clientlocalprove(Dis64ScalarClientView *v)
{
 Dis64ScalarClientView body;Dis64Type type;
 unsigned int at=4,used,i,j,k,start,end,step,edges[9]={0},reach,expected,done=0,progress,owner,resultmask=0;
 for(i=0;i<v->count;i++){
  if(at>=v->code.bytes || !dis64clientinstructionread(v->envelope.data+v->code.offset+at,v->code.bytes-at,v->decoded+i,&used))return 0;at+=used;
 }
 if(at!=v->code.bytes || !dis64clientliteraluses(v))return 0;
 for(i=0;i<v->count;i++)if(v->decoded[i].instruction.op==IFRAME){
  Dis64Instruction *f=&v->decoded[i].instruction,*call;
  step=dis64clientlocalend(v,i);if(!step || v->count-i<=step)return 0;call=&v->decoded[i+step].instruction;
  for(j=0;j<v->localcount;j++)if(f->source==v->localtype[j])break;
  if(j==v->localcount || call->op!=ICALL || call->destination<=0 || (unsigned long)call->destination>=v->count ||
     (v->localentry[j] && (v->localentry[j]!=(unsigned long)call->destination || v->localvalue[j]!=(step==4))))return 0;
  v->localentry[j]=call->destination;v->localvalue[j]=step==4;
 }
 for(i=0;i<v->localcount;i++)if(!v->localentry[i] || (i && v->localentry[i]<=v->localentry[i-1]))return 0;
 for(i=0;i<v->count;i++)if(v->decoded[i].instruction.op==IFRAME){
  for(owner=0;owner<v->localcount && i>=v->localentry[owner];owner++);
  for(j=0;j<v->localcount;j++)if(v->decoded[i].instruction.source==v->localtype[j])break;
  if(j==v->localcount)return 0;edges[owner]|=1U<<j;
 }
 /* Transitive closure detects cycles even in otherwise unreachable helpers. */
 for(k=0;k<v->localcount;k++)for(i=0;i<=v->localcount;i++)if(edges[i]&(1U<<k))edges[i]|=edges[k+1];
 for(i=0;i<v->localcount;i++)if(edges[i+1]&(1U<<i))return 0;
 expected=(1U<<v->localcount)-1;reach=edges[0];if(reach!=expected)return 0;
 for(i=0;i<v->localcount;i++)if(v->localvalue[i])resultmask|=1U<<i;
 for(k=0;k<=v->localcount;k++){
  progress=0;
  for(i=0;i<=v->localcount;i++)if(!(done&(1U<<i)) && !(edges[i]&resultmask&~(done>>1))){
   start=i?v->localentry[i-1]:0;end=i<v->localcount?v->localentry[i]:v->count;
   if(start>=end || !dis64typeget(&v->types,i?v->localtype[i-1]:0,&type))return 0;
   body=*v;body.prooftype=i?v->localtype[i-1]:0;body.frame=type.bytes;body.count=end-start;body.calls=body.localused=0;
   body.returning=i?v->localvalue[i-1]:0;body.returnkind=body.returning?Dis64ClientRoot:0;
   for(j=0;j<body.count;j++){
    body.decoded[j]=v->decoded[start+j];
    if(dis64clientbranch(body.decoded[j].instruction.op)){
     long target=body.decoded[j].instruction.destination;
     if(target<(long)start || target>=(long)end)return 0;
     body.decoded[j].instruction.destination=target-start;
    }
   }
   if(!dis64clientbodyprove(&body,1,1))return 0;
   if(i)v->localresult[i-1]=body.returnkind;
   done|=1U<<i;progress=1;
  }
  if(done==((1U<<(v->localcount+1))-1))return 1;
  if(!progress)break;
 }
 return 0;
}
