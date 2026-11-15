/* Local void calls use the existing Command argument geometry, not new kinds. */
static int dis64clientbodyprove(Dis64ScalarClientView*,int,int);
static int
dis64clientlocalcall(Dis64ScalarClientView *v,unsigned int pc,Dis64ClientSlot *slots,unsigned int count,unsigned int need[2])
{
 Dis64Instruction *frame,*call,*in;Dis64CallDecoded *d;unsigned int i,j;
 if(pc>=v->count || v->count-pc<4)return 0;
 frame=&v->decoded[pc].instruction;call=&v->decoded[pc+3].instruction;
 if(frame->op!=IFRAME || frame->mode!=(SRC(AIMM)|DST(AFP)) || frame->destination!=40 ||
    call->op!=ICALL || call->mode!=(SRC(AFP)|DST(AIMM)) || call->source!=40)return 0;
 for(i=0;i<v->localcount;i++)if(frame->source==v->localtype[i] && call->destination==v->localentry[i])break;
 if(i==v->localcount)return 0;
 for(j=0;j<2;j++){
  d=v->decoded+pc+1+j;in=&d->instruction;
  if(in->op!=IMOVP || (in->mode&ARM)!=AXNON || UDST(in->mode)!=(AFP|AIND) || in->destination!=40 || d->secondary!=64+8*j ||
     !dis64clientrootoperand(slots,count,USRC(in->mode),in->source,j?Dis64ClientListString:Dis64ClientOpaque,need))return 0;
 }
 v->localused|=1U<<i;return 1;
}
/* Infer local entries only from exact FRAME/CALL pairs; each appended frame has
 * one entry, and each body is independently proved with its own map and spans. */
static int
dis64clientlocalprove(Dis64ScalarClientView *v)
{
 Dis64ScalarClientView body;Dis64Type type;unsigned int at=4,used,i,j,k,start,end,edges[9]={0},reach,expected;
 for(i=0;i<v->count;i++){
  if(at>=v->code.bytes || !dis64clientinstructionread(v->envelope.data+v->code.offset+at,v->code.bytes-at,v->decoded+i,&used))return 0;at+=used;
 }
 if(at!=v->code.bytes || !dis64clientliteraluses(v))return 0;
 for(i=0;i<v->count;i++)if(v->decoded[i].instruction.op==IFRAME){
  Dis64Instruction *f=&v->decoded[i].instruction,*call;
  if(v->count-i<4)return 0;call=&v->decoded[i+3].instruction;
  for(j=0;j<v->localcount;j++)if(f->source==v->localtype[j])break;
  if(j==v->localcount || call->op!=ICALL || call->destination<=0 || (unsigned long)call->destination>=v->count ||
     (v->localentry[j] && v->localentry[j]!=(unsigned long)call->destination))return 0;
  v->localentry[j]=call->destination;
 }
 for(i=0;i<v->localcount;i++)if(!v->localentry[i] || (i && v->localentry[i]<=v->localentry[i-1]))return 0;
 for(i=0;i<=v->localcount;i++){
  start=i?v->localentry[i-1]:0;end=i<v->localcount?v->localentry[i]:v->count;
  if(start>=end || !dis64typeget(&v->types,i?v->localtype[i-1]:0,&type))return 0;
  body=*v;body.prooftype=i?v->localtype[i-1]:0;body.frame=type.bytes;body.count=end-start;body.calls=body.localused=0;
  for(j=0;j<body.count;j++){
   body.decoded[j]=v->decoded[start+j];
   if(dis64clientbranch(body.decoded[j].instruction.op)){
    long target=body.decoded[j].instruction.destination;
    if(target<(long)start || target>=(long)end)return 0;
    body.decoded[j].instruction.destination=target-start;
   }
  }
  if(!dis64clientbodyprove(&body,1,1))return 0;edges[i]=body.localused;
 }
 /* Transitive closure detects cycles even in otherwise unreachable helpers. */
 for(k=0;k<v->localcount;k++)for(i=0;i<=v->localcount;i++)if(edges[i]&(1U<<k))edges[i]|=edges[k+1];
 for(i=0;i<v->localcount;i++)if(edges[i+1]&(1U<<i))return 0;
 expected=(1U<<v->localcount)-1;reach=edges[0];if(reach!=expected)return 0;
 return 1;
}
