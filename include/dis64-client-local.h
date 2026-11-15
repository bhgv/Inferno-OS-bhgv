/* Local calls derive exact parameter and result classes. */
static int dis64clientbodyprove(Dis64ScalarClientView*,int,int);
/* A result kind starts unknown; numeric classes never merge with references. */
static unsigned int
dis64clientresultkind(unsigned int a,unsigned int b)
{
 if(!a)return b;
 return dis64clientisroot(a) && dis64clientisroot(b)?dis64clientmergekind(a,b):a==b?a:0;
}
static int
dis64clientresultstore(const Dis64CallDecoded *d)
{
 unsigned int source,kind;int binary;
 return UDST(d->instruction.mode)==(AFP|AIND) && d->instruction.destination==32 &&
  (d->instruction.op==ILENC || d->instruction.op==ILENL || d->instruction.op==ILENA || d->instruction.op==IMOVP || d->instruction.op==IHEADP || d->instruction.op==ITAIL || dis64clientoperation(d->instruction.op,&source,&kind,&binary));
}
static int
dis64clientreturn(Dis64ScalarClientView *v,const Dis64CallDecoded *d,Dis64ClientSlot *slots,unsigned int *count,unsigned int need[2])
{
 const Dis64Instruction *in=&d->instruction;unsigned int kind,source,mm=in->mode&ARM;int binary;
 if(!v->returning || d->secondary)return 0;
 if(dis64clientoperation(in->op,&source,&kind,&binary)){
  if(!dis64clientbit(slots,count,v->frame,USRC(in->mode),in->source,source,need))return 0;
  if(binary){
   if(mm!=AXINF && mm!=AXIMM)return 0;
   if(mm==AXIMM && (kind!=Dis64Int || in->middle < -32768 || in->middle > 32767))return 0;
   if(!dis64clientbit(slots,count,v->frame,mm==AXIMM?AIMM:AFP,in->middle,kind,need))return 0;
  }else if(mm!=AXNON)return 0;
 }else if(in->op==ILENC || in->op==ILENL || in->op==ILENA){
  source=in->op==ILENC?Dis64ClientString:in->op==ILENL?Dis64ClientListString:Dis64ClientArray;kind=Dis64Int;
  if(mm!=AXNON || !dis64clientrootoperand(slots,*count,USRC(in->mode),in->source,source,need))return 0;
 }else{
  if(mm!=AXNON || !dis64clientrootoperand(slots,*count,USRC(in->mode),in->source,in->op==IMOVP?Dis64ClientRoot:Dis64ClientListString,need))return 0;
  kind=in->op==IHEADP?Dis64ClientString:in->op==ITAIL?Dis64ClientListString:dis64clientrootkind(slots,*count,USRC(in->mode),in->source);
 }
 kind=dis64clientresultkind(v->returnkind,kind);if(!kind)return 0;v->returnkind=kind;return 1;
}
/* Binder presence determines result arity. Its destination is an exact caller
 * scalar/root span, or the checked inherited address in current FP32. */
static unsigned int
dis64clientargkind(unsigned int op)
{
 return op==IMOVP?Dis64ClientRoot:op==IMOVB?Dis64ClientByte:op==IMOVW?Dis64Int:op==IMOVL?Dis64Big:op==IMOVF?Dis64Real:0;
}
/* Parameters are densely aligned stores before the binder. Two leading MOVPs
 * retain the existing Context/list prefix; other MOVPs need inferred classes. */
static unsigned int
dis64clientlocalscan(const Dis64ScalarClientView *v,unsigned int pc,unsigned int *prefix,unsigned int *argc,unsigned int kinds[8],unsigned int *value)
{
 unsigned int at=pc+1,j,offset=64,kind,width;const Dis64Instruction *in;const Dis64CallDecoded *d;
 *prefix=*argc=*value=0;if(pc>=v->count || v->count-pc<2)return 0;
 if(v->count-at>=2 && v->decoded[at].instruction.op==IMOVP && v->decoded[at+1].instruction.op==IMOVP){
  if(v->count-at<3)return 0;
  for(j=0;j<2;j++){
   d=v->decoded+at+j;in=&d->instruction;
   if(in->op!=IMOVP || (in->mode&ARM)!=AXNON || UDST(in->mode)!=(AFP|AIND) || in->destination!=40)return 0;
  }
  *prefix=2;at+=2;offset=80;
 }
 while(at<v->count){
  d=v->decoded+at;in=&d->instruction;kind=dis64clientargkind(in->op);
  if(!kind || UDST(in->mode)!=(AFP|AIND) || d->secondary<64)break;
  width=dis64clientwidth(kind);offset=(offset+width-1)&~(width-1);
  if(*argc==8 || (in->mode&ARM)!=AXNON || in->destination!=40 || d->secondary!=offset)return 0;
  kinds[(*argc)++]=kind;offset+=width;at++;
 }
 if(at<v->count && (v->decoded[at].instruction.op==ILEA || v->decoded[at].instruction.op==IMOVL)){*value=1;at++;}
 if(at>=v->count || v->decoded[at].instruction.op!=ICALL)return 0;
 return at-pc;
}
static unsigned int
dis64clientlocalend(const Dis64ScalarClientView *v,unsigned int pc)
{
 unsigned int prefix,argc,kinds[8],value;return dis64clientlocalscan(v,pc,&prefix,&argc,kinds,&value);
}
static int
dis64clientlocalcall(Dis64ScalarClientView *v,unsigned int pc,Dis64ClientSlot *slots,unsigned int *count,unsigned int need[2],unsigned int give[2],unsigned int *assigned)
{
 Dis64Instruction *frame,*call,*in;Dis64CallDecoded *d;unsigned int i,j,prefix,argc,kinds[8],value,end=dis64clientlocalscan(v,pc,&prefix,&argc,kinds,&value),kind,offset,width;
 if(!end || v->count-pc<=end)return 0;
 frame=&v->decoded[pc].instruction;call=&v->decoded[pc+end].instruction;
 if(frame->op!=IFRAME || frame->mode!=(SRC(AIMM)|DST(AFP)) || frame->destination!=40 ||
    call->op!=ICALL || call->mode!=(SRC(AFP)|DST(AIMM)) || call->source!=40)return 0;
 for(i=0;i<v->localcount;i++)if(frame->source==v->localtype[i] && call->destination==v->localentry[i])break;
 if(i==v->localcount || v->localvalue[i]!=value || v->localprefix[i]!=prefix || v->localargc[i]!=argc)return 0;
 for(j=0;j<prefix;j++){
  d=v->decoded+pc+1+j;in=&d->instruction;
  if(in->op!=IMOVP || (in->mode&ARM)!=AXNON || UDST(in->mode)!=(AFP|AIND) || in->destination!=40 || d->secondary!=64+8*j ||
     !dis64clientrootoperand(slots,*count,USRC(in->mode),in->source,j?Dis64ClientListString:Dis64ClientOpaque,need))return 0;
 }
 offset=prefix?80:64;
 for(j=0;j<argc;j++){
  d=v->decoded+pc+1+prefix+j;in=&d->instruction;kind=v->localargs[i][j];width=dis64clientwidth(kind);
  offset=(offset+width-1)&~(width-1);
  if((kinds[j]==Dis64ClientRoot?!dis64clientisroot(kind):kinds[j]!=kind) || d->secondary!=offset)return 0;
  if(dis64clientisroot(kind)){
   if(kind==Dis64ClientRoot || !dis64clientrootoperand(slots,*count,USRC(in->mode),in->source,kind,need))return 0;
  }else if(!dis64clientbit(slots,count,v->frame,USRC(in->mode),in->source,kind,need))return 0;
  offset+=width;
 }
 if(value){
  d=v->decoded+pc+1+prefix+argc;in=&d->instruction;kind=v->localresult[i];
  if(!kind || in->mode!=(SRC(AFP)|DST(AFP|AIND)) || in->destination!=40 || d->secondary!=32)return 0;
  if(in->op==ILEA){unsigned int bits[2]={0};
   if(dis64clientisroot(kind)){
    if(!dis64clientrootoperand(slots,*count,AFP,in->source,kind,bits))return 0;
   }else if(!dis64clientbit(slots,count,v->frame,AFP,in->source,kind,give))return 0;
  }else{
   if(in->op!=IMOVL || in->source!=32 || !v->returning || !(kind=dis64clientresultkind(v->returnkind,kind)))return 0;
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
 unsigned int at=4,used,i,j,k,start,end,step,edges[9]={0},reach,expected,done=0,progress,owner,resultmask=0,prefix,argc,kinds[8],value,n;
 for(i=0;i<v->count;i++){
  if(at>=v->code.bytes || !dis64clientinstructionread(v->envelope.data+v->code.offset+at,v->code.bytes-at,v->decoded+i,&used))return 0;at+=used;
 }
 if(at!=v->code.bytes || !dis64clientliteraluses(v))return 0;
 for(i=0;i<v->count;i++)if(v->decoded[i].instruction.op==IFRAME){
  Dis64Instruction *f=&v->decoded[i].instruction,*call;
  step=dis64clientlocalscan(v,i,&prefix,&argc,kinds,&value);if(!step || v->count-i<=step)return 0;call=&v->decoded[i+step].instruction;
  for(j=0;j<v->localcount;j++)if(f->source==v->localtype[j])break;
  if(j==v->localcount || call->op!=ICALL || call->destination<=0 || (unsigned long)call->destination>=v->count ||
     (v->localentry[j] && (v->localentry[j]!=(unsigned long)call->destination || v->localvalue[j]!=value || v->localprefix[j]!=prefix || v->localargc[j]!=argc)))return 0;
  if(v->localentry[j])for(n=0;n<argc;n++)if(v->localargs[j][n]!=kinds[n])return 0;
  v->localentry[j]=call->destination;v->localvalue[j]=value;v->localprefix[j]=prefix;v->localargc[j]=argc;
  for(n=0;n<argc;n++)v->localargs[j][n]=kinds[n];
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
 for(i=0;i<v->localcount;i++){
  if(v->localvalue[i])resultmask|=1U<<i;
  for(j=0;j<v->localargc[i];j++)if(dis64clientisroot(v->localargs[i][j]))resultmask|=1U<<i;
 }
 for(k=0;k<=v->localcount;k++){
  progress=0;
  for(i=0;i<=v->localcount;i++)if(!(done&(1U<<i)) && !(edges[i]&resultmask&~(done>>1))){
   start=i?v->localentry[i-1]:0;end=i<v->localcount?v->localentry[i]:v->count;
   if(start>=end || !dis64typeget(&v->types,i?v->localtype[i-1]:0,&type))return 0;
   body=*v;body.prooftype=i?v->localtype[i-1]:0;body.frame=type.bytes;body.count=end-start;body.calls=body.localused=0;
   body.argprefix=i?v->localprefix[i-1]:2;body.argcount=i?v->localargc[i-1]:0;
   for(n=0;n<body.argcount;n++)body.argkinds[n]=v->localargs[i-1][n];
   body.returning=i?v->localvalue[i-1]:0;body.returnkind=0;
   for(j=0;j<body.count;j++){
    body.decoded[j]=v->decoded[start+j];
    if(dis64clientbranch(body.decoded[j].instruction.op)){
     long target=body.decoded[j].instruction.destination;
     if(target<(long)start || target>=(long)end)return 0;
     body.decoded[j].instruction.destination=target-start;
    }
   }
   if(!dis64clientbodyprove(&body,i || !v->module.profile,1))return 0;
   if(i){
    if(body.returnkind==Dis64ClientArray)return 0;
    v->localresult[i-1]=body.returnkind;
    for(n=0;n<body.argcount;n++)v->localargs[i-1][n]=body.argkinds[n];
   }
   done|=1U<<i;progress=1;
  }
  if(done==((1U<<(v->localcount+1))-1))return 1;
  if(!progress)break;
 }
 return 0;
}

/* Recover the proved local binder contract without trusting a wire annotation. */
static unsigned int
dis64clientlocalcontract(const Dis64ScalarClientView *v,unsigned int pc)
{
 unsigned int at,end,prefix,argc,kinds[8],value,i;
 for(at=0;at<pc;at++)if(v->decoded[at].instruction.op==IFRAME){
  end=dis64clientlocalscan(v,at,&prefix,&argc,kinds,&value);
  if(!end || !value || at+1+prefix+argc!=pc)continue;
  for(i=0;i<v->localcount;i++)if(v->decoded[at].instruction.source==v->localtype[i])
   return dis64clientisroot(v->localresult[i])?24:dis64clientwidth(v->localresult[i]);
 }
 return v->module.profile?8:0;
}
