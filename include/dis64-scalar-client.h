/* Bounded importing command. Private Command trial; no serialized native facts. */
#ifndef DIS64_SCALAR_CLIENT_H
#define DIS64_SCALAR_CLIENT_H
#include "dis64-call-library.h"
#include "dis64-command-binding.h"
#ifndef Dis64ScalarClient
#define Dis64ScalarClient (1U<<29) /* Native-only constructor/cache identity. */
#endif
#define Dis64ClientCode 256U
#define Dis64ClientSlots 64U

typedef struct Dis64ScalarClientView Dis64ScalarClientView;
struct Dis64ScalarClientView {
 Dis64View envelope;
 Dis64ModuleInfo module;
 Dis64TypeTable types;
 DisStringPoolView strings;
 Dis64Bindings imports;
 Dis64Section code,requests,names;
 unsigned int count,frame,calls,literals,contexttype,bytearrays,bytetype,intarrays,inttype;
 unsigned int prooftype,localcount,localused,localtype[8],localentry[8];
 unsigned int returning,returnkind,localvalue[8],localresult[8];
 Dis64CallDecoded decoded[Dis64ClientCode];
};

#include "dis64-client-literals.h"

/* Local indirect framing for checked managed arguments and Context fields.
 * Exactly one operand is indirect; secondary holds its displacement. The
 * general scalar decoder stays unchanged. */
static int
dis64clientinstructionread(const unsigned char *data,unsigned int bytes,Dis64CallDecoded *out,unsigned int *used)
{
 Dis64CallDecoded d={0};unsigned int at=2,n;
 if(data && bytes>=2 && (data[0]==IMOVB || data[0]==ICVTWB || data[0]==IMOVW) &&
    (data[1]==(SRC(AFP)|DST(AFP|AIND)) || data[1]==(SRC(AIMM)|DST(AFP|AIND)))){
  if(!out || !used || !dis64operandread(data+at,bytes-at,&d.instruction.source,&n))return 0;at+=n;
  if(!dis64operandread(data+at,bytes-at,&d.instruction.destination,&n))return 0;at+=n;
  if(!dis64operandread(data+at,bytes-at,&d.secondary,&n))return 0;at+=n;
  d.instruction.op=data[0];d.instruction.mode=data[1];*out=d;*used=at;return 1;
 }

 if(data && bytes>=2 && (((data[0]==IMOVP || data[0]==IMOVB || data[0]==ICVTBW || data[0]==IMOVW) && data[1]==(SRC(AFP|AIND)|DST(AFP))) ||
   ((data[0]==IBEQL || data[0]==IBNEL) && data[1]==(SRC(AFP|AIND)|AXINM|DST(AIMM))))){
  if(data[1]&ARM){if(!dis64operandread(data+at,bytes-at,&d.instruction.middle,&n))return 0;at+=n;}
  if(!out || !used || !dis64operandread(data+at,bytes-at,&d.instruction.source,&n))return 0;at+=n;
  if(!dis64operandread(data+at,bytes-at,&d.secondary,&n))return 0;at+=n;
  if(!dis64operandread(data+at,bytes-at,&d.instruction.destination,&n))return 0;at+=n;
  d.instruction.op=data[0];d.instruction.mode=data[1];*out=d;*used=at;return 1;
 }
 if(data && bytes>=2 && data[0]==IMOVP && data[1]==(SRC(AMP)|DST(AFP|AIND))){
  if(!out || !used || !dis64operandread(data+at,bytes-at,&d.instruction.source,&n))return 0;at+=n;
  if(!dis64operandread(data+at,bytes-at,&d.instruction.destination,&n))return 0;at+=n;
  if(!dis64operandread(data+at,bytes-at,&d.secondary,&n))return 0;at+=n;
  d.instruction.op=IMOVP;d.instruction.mode=data[1];*out=d;*used=at;return 1;
 }
 return dis64callread(data,bytes,out,used);
}

/* Internal proof classes only; never signature kinds or serialized type IDs. */
enum { Dis64ClientRoot=0x100,Dis64ClientString,Dis64ClientListString,Dis64ClientOpaque,Dis64ClientDisplay,Dis64ClientScreen,Dis64ClientWm,Dis64ClientByteArray,Dis64ClientIntArray,Dis64ClientArray };
static int dis64clientisroot(unsigned int kind){return kind>=Dis64ClientRoot && kind<=Dis64ClientArray;}
enum { Dis64ClientByte=0x200 };
static unsigned int dis64clientwidth(unsigned int kind){return kind==Dis64ClientByte?1:dis64clientisroot(kind)?8:dis64scalarwidth(kind);}

typedef struct Dis64ClientSlot Dis64ClientSlot;
struct Dis64ClientSlot {unsigned int offset,kind,initialized;};
/* Numeric spans have one immutable kind; reads additionally require a definition. */
static int
dis64clientslot(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,long offset,unsigned int kind,int write)
{
 unsigned int i,width=dis64clientwidth(kind),at;
 if(!width || kind==Dis64Bool || offset<64 || (unsigned long)offset>frame-16)return 0;
 at=offset;if(at%width || width>frame-16-at)return 0;
 for(i=0;i<*count;i++){
  if(at==slots[i].offset){if(slots[i].kind!=kind || (!write && !slots[i].initialized))return 0;slots[i].initialized|=write;return 1;}
  if(at<slots[i].offset+dis64clientwidth(slots[i].kind) && slots[i].offset<at+width)return 0;
 }
 if(!write || *count==Dis64ClientSlots)return 0;
 slots[*count]=(Dis64ClientSlot){at,kind,1};(*count)++;return 1;
}
static int
dis64clientread(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,unsigned int mode,long value,unsigned int kind)
{
 return mode==AIMM?(kind==Dis64Int || (kind==Dis64ClientByte && value>=0 && value<=255)):mode==AFP && dis64clientslot(slots,count,frame,value,kind,0);
}
/* Derive exact numeric operand kinds; equal widths do not imply equal kinds. */
static int
dis64clientoperation(unsigned int op,unsigned int *source,unsigned int *destination,int *binary)
{
 *source=*destination=Dis64Int;*binary=0;
 switch(op){
 case IMOVB:*source=*destination=Dis64ClientByte;break;
 case ICVTBW:*source=Dis64ClientByte;break;
 case ICVTWB:*destination=Dis64ClientByte;break;
 case IMOVW:break;
 case IMOVL:*source=*destination=Dis64Big;break;
 case IMOVF:case INEGF:*source=*destination=Dis64Real;break;
 case ICVTWL:*destination=Dis64Big;break;
 case ICVTWF:*destination=Dis64Real;break;
 case ICVTLW:*source=Dis64Big;break;
 case ICVTFW:*source=Dis64Real;break;
 case ICVTLF:*source=Dis64Big;*destination=Dis64Real;break;
 case ICVTFL:*source=Dis64Real;*destination=Dis64Big;break;
 case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:*binary=1;break;
 case IADDL:case ISUBL:case IMULL:case IDIVL:case IMODL:case IANDL:case IORL:case IXORL:*source=*destination=Dis64Big;*binary=1;break;
 case ISHLL:case ISHRL:*destination=Dis64Big;*binary=1;break;
 case IADDF:case ISUBF:case IMULF:case IDIVF:*source=*destination=Dis64Real;*binary=1;break;
 default:return 0;
 }
 return 1;
}
/* No branches, hidden MP access, references or prepared-frame access here. */
static int
dis64clientnumeric(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,const Dis64Instruction *in)
{
 unsigned int kind,sourcekind,mm=in->mode&ARM,lm;long lhs;int binary;
 if(!dis64clientoperation(in->op,&sourcekind,&kind,&binary))return 0;
 /* Native Inst.reg holds a signed16 WORD middle immediate, never a REAL/LONG. */
 if(mm==AXIMM && (kind!=Dis64Int || in->middle < -32768 || in->middle > 32767))return 0;
 if(!dis64clientread(slots,count,frame,USRC(in->mode),in->source,sourcekind) || UDST(in->mode)!=AFP)return 0;
 if(binary){lm=mm==AXNON?AFP:mm==AXIMM?AIMM:mm==AXINF?AFP:AMP;lhs=mm==AXNON?in->destination:in->middle;
  if(!dis64clientread(slots,count,frame,lm,lhs,kind))return 0;
 }else if(mm!=AXNON)return 0;
 return dis64clientslot(slots,count,frame,in->destination,kind,1);
}
/* Structural discriminator only: no VM owner and no opcode permission. */
static int
dis64clientopen(const unsigned char *data,unsigned int bytes,Dis64ScalarClientView *out)
{
 Dis64ScalarClientView v={0};Dis64Section s,module={0},types={0};Dis64Type frame,mp;DisRuneView name;unsigned int i,j,seen=0,rune;
 if(!out || bytes>Dis64CallImageBytes || !dis64check(data,bytes,0,5,0,0,&v.envelope) || v.envelope.sections!=5)return 0;
 for(i=0;i<5;i++){
  if(!dis64section(&v.envelope,i,&s) || s.kind<1 || s.kind>7 || (seen&(1U<<s.kind)))return 0;
  seen|=1U<<s.kind;
  switch(s.kind){case Dis64Module:module=s;break;case Dis64Code:v.code=s;break;case Dis64Types:types=s;break;case Dis64Imports:v.requests=s;break;case Dis64Strings:v.names=s;break;default:return 0;}
 }
 if(!dis64modulecheck(data+module.offset,module.bytes,&v.module) || v.module.profile!=1 || v.module.name!=0 ||
    v.module.entry || v.module.frametype || v.module.datatype!=1 ||
    !dis64typescheck(data+types.offset,types.bytes,5,4096,&v.types) ||
    !dis64typeget(&v.types,0,&frame) || !dis64typeget(&v.types,1,&mp) || frame.bytes<80 || frame.align!=8 ||
    mp.align!=8)return 0;

 if(!disstringpoolcheck(data+v.names.offset,v.names.bytes,65536,&v.strings) ||
    !dis64clientbindingscheck(data+v.requests.offset,v.requests.bytes,0,&v.strings,0,0,64,64,&v.imports) ||
    v.requests.bytes>Dis64LocalBytes || v.names.bytes>Dis64LocalBytes || !v.imports.count || !dis64clientdatacheck(&v) || v.localcount || v.code.bytes<4)return 0;
 for(i=0;i<v.imports.count+3;i++){
  if(!disstringpoolget(&v.strings,i,&name) || !name.count || name.count>(i==1?1023U:255U))return 0;
  for(j=0;j<name.count;j++)if(!disruneat(&name,j,&rune) || !rune || rune>127 || (i==1 && j==0 && rune=='$'))return 0;
 }
 for(i=0;i<v.imports.count;i++){Dis64Binding b;if(!dis64clientbindingget(&v.imports,i,&b) || b.module!=2 || b.symbol!=i+3 || b.signature.result>Dis64Real)return 0;if(!dis64commandsignature(&b.signature))for(j=0;j<b.signature.parameters;j++)if(dis64u32(b.signature.data+12+4*j)>Dis64Real)return 0;}
 v.count=dis64u32(data+v.code.offset);if(v.count<4 || v.count>Dis64ClientCode)return 0;
 v.frame=frame.bytes;*out=v;return 1;
}
/* Complete call islands are atomic CFG nodes: branch targets never enter setup. */
typedef struct Dis64ClientNode Dis64ClientNode;
struct Dis64ClientNode {
 unsigned int need[2],give[2],next,target,load,needload,boundary,seen,queued;
 unsigned int in[2],out[2],inload,outload;
};
static int
dis64clientbit(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,
 unsigned int mode,long value,unsigned int kind,unsigned int bits[2])
{
 unsigned int i;
 if(mode==AIMM)return kind==Dis64Int || (kind==Dis64ClientByte && value>=0 && value<=255);
 if(mode!=AFP || !dis64clientslot(slots,count,frame,value,kind,1))return 0;
 for(i=0;i<*count;i++)if(slots[i].offset==(unsigned long)value){bits[i/32]|=1U<<(i%32);return 1;}
 return 0;
}
#include "dis64-client-managed.h"
#include "dis64-client-index.h"
static int
dis64clientbranch(unsigned int op)
{
 switch(op){case IJMP:case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:
 case IBEQL:case IBNEL:case IBLTL:case IBLEL:case IBGTL:case IBGEL:
 case IBEQC:case IBNEC:case IBLTC:case IBLEC:case IBGTC:case IBGEC:
 case IBEQF:case IBNEF:case IBLTF:case IBLEF:case IBGTF:case IBGEF:return 1;}
 return 0;
}
/* Derive reads/writes from opcodes and full typed spans, never a file manifest. */
static int
dis64clienteffect(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,
 const Dis64Instruction *in,Dis64ClientNode *node,const Dis64ScalarClientView *v)
{
 unsigned int kind=Dis64Int,sourcekind,mm=in->mode&ARM,lm,branch=dis64clientbranch(in->op);long lhs;int binary=0;
 if(dis64clientmanagedop(in) || dis64clientreferencecompare(slots,*count,in))return dis64clientmanagedroles(slots,count,frame,in,node->need,node->give,v);
 if(in->op==IJMP)return in->mode==(SRC(AXXX)|DST(AIMM));
 if((in->op==IBEQL || in->op==IBNEL) && in->mode==(SRC(AMP)|AXINM|DST(AIMM)))return in->source==0 && in->middle==8;
 if(branch){
  switch(in->op){
  case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:break;
  case IBEQL:case IBNEL:case IBLTL:case IBLEL:case IBGTL:case IBGEL:kind=Dis64Big;break;
  case IBEQF:case IBNEF:case IBLTF:case IBLEF:case IBGTF:case IBGEF:kind=Dis64Real;break;
  default:return 0;
  }
  sourcekind=kind;
 }else if(!dis64clientoperation(in->op,&sourcekind,&kind,&binary))return 0;
 if(!dis64clientbit(slots,count,frame,USRC(in->mode),in->source,sourcekind,node->need))return 0;
 if(binary || branch){
  if(branch && mm==AXNON)return 0;
  if(mm==AXIMM && (kind!=Dis64Int || in->middle < -32768 || in->middle > 32767))return 0;
  lm=mm==AXNON?UDST(in->mode):mm==AXIMM?AIMM:mm==AXINF?AFP:AMP;
  lhs=mm==AXNON?in->destination:in->middle;
  if(!dis64clientbit(slots,count,frame,lm,lhs,kind,node->need))return 0;
 }
 if(branch)return UDST(in->mode)==AIMM;
 if(!dis64clientbit(slots,count,frame,UDST(in->mode),in->destination,kind,node->give))return 0;
 /* All spans are declared above; reuse the existing exact numeric mode checks. */
 return dis64clientnumeric(slots,count,frame,in);
}

/* Descending intersection: entry has no numeric definitions and no LOAD attempt.
 * Every boundary is structurally reachable; loops never invent first-entry facts. */
static int
dis64clientflow(Dis64ClientNode *nodes,unsigned int count,const unsigned int seed[2])
{
 unsigned int queue[Dis64ClientCode],head=0,tail=0,pending=0,i,j,k,at,budget=132*count+1,changed,edge[2],incoming[2],loaded;
 queue[tail++]=0;nodes[0].seen=1;
 while(head<tail){at=queue[head++];edge[0]=nodes[at].next;edge[1]=nodes[at].target;
  for(k=0;k<2;k++)if(edge[k]<count && !nodes[edge[k]].seen){nodes[edge[k]].seen=1;queue[tail++]=edge[k];}
 }
 head=tail=pending=0;
 for(i=0;i<count;i++)if(nodes[i].boundary){
  if(!nodes[i].seen)return 0;
  nodes[i].in[0]=nodes[i].in[1]=nodes[i].out[0]=nodes[i].out[1]=~0U;nodes[i].inload=nodes[i].outload=1;
  queue[tail]=i;tail=(tail+1)%count;pending++;nodes[i].queued=1;
 }
 while(pending){
  if(!budget--)return 0;at=queue[head];head=(head+1)%count;pending--;nodes[at].queued=0;
  incoming[0]=at==0?seed[0]:~0U;incoming[1]=at==0?seed[1]:~0U;loaded=at==0?0:1;
  for(j=0;j<count;j++)if(nodes[j].boundary && (nodes[j].next==at || nodes[j].target==at)){
   incoming[0]&=nodes[j].out[0];incoming[1]&=nodes[j].out[1];loaded&=nodes[j].outload;
  }
  nodes[at].in[0]=incoming[0];nodes[at].in[1]=incoming[1];nodes[at].inload=loaded;
  incoming[0]|=nodes[at].give[0];incoming[1]|=nodes[at].give[1];loaded|=nodes[at].load;
  changed=incoming[0]!=nodes[at].out[0] || incoming[1]!=nodes[at].out[1] || loaded!=nodes[at].outload;
  if(changed){
   nodes[at].out[0]=incoming[0];nodes[at].out[1]=incoming[1];nodes[at].outload=loaded;edge[0]=nodes[at].next;edge[1]=nodes[at].target;
   for(k=0;k<2;k++)if(edge[k]<count && !nodes[edge[k]].queued){i=edge[k];queue[tail]=i;tail=(tail+1)%count;pending++;nodes[i].queued=1;}
  }
 }
 for(i=0;i<count;i++)if(nodes[i].boundary && ((nodes[i].need[0]&~nodes[i].in[0]) ||
    (nodes[i].need[1]&~nodes[i].in[1]) || (nodes[i].needload && !nodes[i].inload)))return 0;
 return 1;
}

/* The fixed ordinary init island has two mapped arguments and no result. */
static int
dis64commandarguments(Dis64ScalarClientView *v,unsigned int pc,Dis64ClientSlot *slots,unsigned int count,unsigned int need[2])
{
 unsigned int j;Dis64CallDecoded *d;Dis64Instruction *in;
 if(pc>v->count || v->count-pc<4)return 0;
 for(j=0;j<2;j++){
  d=v->decoded+pc+1+j;in=&d->instruction;
  if(in->op!=IMOVP || (in->mode&ARM)!=AXNON || UDST(in->mode)!=(AFP|AIND) || in->destination!=40 || d->secondary!=64+8*j ||
     !dis64clientrootoperand(slots,count,USRC(in->mode),in->source,j?Dis64ClientListString:Dis64ClientOpaque,need))return 0;
 }
 in=&v->decoded[pc+3].instruction;
 return in->op==IMCALL && in->mode==(SRC(AFP)|AXIMM|DST(AMP)) && in->source==40 && !in->destination && !in->middle;
}

#include "dis64-client-local.h"
/* All instructions and islands are proved before CFG facts are solved. */
static int
dis64clientbodyprove(Dis64ScalarClientView *v,int library,int ready)
{
 Dis64ClientSlot slots[Dis64ClientSlots]={{0}};Dis64ClientNode nodes[Dis64ClientCode]={{0}},*node;
 unsigned int nslots=0,i,j,k,changed,seed[2]={0},previous[Dis64ClientSlots],at=4,used,offset,kind,width,result,ordinal,start;Dis64Type frame;
 Dis64CallDecoded *d;Dis64Instruction *in;Dis64Binding b;
 const unsigned char *data=v->envelope.data;
 if(!ready){for(i=0;i<v->count;i++){if(at>=v->code.bytes || !dis64clientinstructionread(data+v->code.offset+at,v->code.bytes-at,v->decoded+i,&used))return 0;at+=used;}
 if(at!=v->code.bytes || !dis64clientliteraluses(v))return 0;}
 if(!dis64typeget(&v->types,v->prooftype,&frame))return 0;
 for(i=0;i<frame.mapbytes*8;i++)if(frame.map[i/8]&(0x80>>(i%8))){
  if(i<8)return 0;
  if(8*i>v->frame-24 || nslots==Dis64ClientSlots)return 0;
  slots[nslots++]=(Dis64ClientSlot){8*i,Dis64ClientRoot,1};
 }
 if(library){unsigned int bits[2]={0};
  if(!dis64clientrootoperand(slots,nslots,AFP,64,Dis64ClientOpaque,bits) ||
     !dis64clientrootoperand(slots,nslots,AFP,72,Dis64ClientListString,bits))return 0;
 }
 /* Solve MOVP class equalities before deriving executable CFG facts. */
 for(k=0;k<=Dis64ClientSlots;k++){
  changed=nslots;for(i=0;i<nslots;i++)previous[i]=slots[i].kind;
  for(i=0;i<v->count;i++)if(dis64clientresultstore(v->decoded+i) || dis64clientmanagedop(&v->decoded[i].instruction) || dis64clientfieldop(v->decoded+i) || dis64clientreferencecompare(slots,nslots,&v->decoded[i].instruction)){
   unsigned int reads[2]={0},writes[2]={0};
   if(dis64clientresultstore(v->decoded+i)){if(!dis64clientreturn(v,v->decoded+i,slots,nslots,reads))return 0;}
   else if(dis64clientfieldop(v->decoded+i)){if(!dis64clientfieldroles(slots,nslots,v->decoded+i,reads,writes))return 0;}
   else if(!dis64clientmanagedroles(slots,&nslots,v->frame,&v->decoded[i].instruction,reads,writes,v))return 0;
  }
  for(i=0;i<v->count;i++)if((v->decoded[i].instruction.op==IINDB || v->decoded[i].instruction.op==IINDW)){unsigned int reads[2]={0},writes[2]={0};
   if(!dis64clientindex(v,i,slots,&nslots,reads,writes))return 0;
  }
  if(library)for(i=0;i<v->count;i++)if(v->decoded[i].instruction.op==IFRAME){unsigned int bits[2]={0},assigned=0;
   if(!dis64clientlocalcall(v,i,slots,nslots,bits,&assigned))return 0;
  }
  if(!library)for(i=0;i<v->count;i++)if(v->decoded[i].instruction.op==IMFRAME){
   Dis64Binding binding;unsigned int bits[2]={0};long ordinal=v->decoded[i].instruction.middle;
   if(ordinal>=0 && (unsigned long)ordinal<v->imports.count && dis64clientbindingget(&v->imports,ordinal,&binding) && dis64commandsignature(&binding.signature))
    if(!dis64commandarguments(v,i,slots,nslots,bits))return 0;
  }
  j=changed;changed=j!=nslots;for(i=0;i<j;i++)changed|=previous[i]!=slots[i].kind;
  if(!changed)break;
 }
 if(k>Dis64ClientSlots)return 0;
 for(i=0;i<nslots;i++)if(dis64clientisroot(slots[i].kind))seed[i/32]|=1U<<(i%32);

 for(i=0;i<v->count;i++){
  start=i;node=nodes+i;node->boundary=1;node->target=v->count;
  d=v->decoded+i;in=&d->instruction;
  if(in->op==ILOAD){if(library || in->mode!=(SRC(AMP)|AXIMM|DST(AMP)) || in->middle || in->source!=16 || in->destination)return 0;node->load=1;node->next=i+1;continue;}
  if(in->op==IEXIT || (library && in->op==IRET)){if((library && in->op!=IRET) || in->mode!=(SRC(AXXX)|DST(AXXX)))return 0;node->needload=library && v->returning;node->next=v->count;continue;}
  if(i==v->count-1)return 0;
  if(in->op==IINDB || in->op==IINDW){
   if(!dis64clientindex(v,i,slots,&nslots,node->need,node->give))return 0;
   node->next=i+2;i++;continue;
  }
  if(dis64clientresultstore(d)){
   if(!library || !dis64clientreturn(v,d,slots,nslots,node->need))return 0;
   node->load=1;node->next=i+1;continue;
  }
  if(in->op==IFRAME){unsigned int assigned=0;
   if(!library || !dis64clientlocalcall(v,i,slots,nslots,node->need,&assigned))return 0;
   node->load=assigned;i+=dis64clientlocalend(v,i);node->next=i+1;continue;
  }
  if(in->op!=IMFRAME){
   if(dis64clientfieldop(d)){if(!dis64clientfieldroles(slots,nslots,d,node->need,node->give))return 0;}
   else if(!dis64clienteffect(slots,&nslots,v->frame,in,node,v))return 0;
   node->next=in->op==IJMP?v->count:i+1;
   if(dis64clientbranch(in->op)){if(in->destination<0 || (unsigned long)in->destination>=v->count)return 0;node->target=in->destination;}
   continue;
  }
  if(library)return 0;
  if(in->mode!=(SRC(AMP)|AXIMM|DST(AFP)) || in->source || in->destination!=40 || in->middle<0 ||
     (unsigned long)in->middle>=v->imports.count || !dis64clientbindingget(&v->imports,in->middle,&b))return 0;
  node->needload=1;ordinal=in->middle;offset=64;
  if(dis64commandsignature(&b.signature)){
   if(!dis64commandarguments(v,i,slots,nslots,node->need))return 0;
   i+=3;v->calls++;nodes[start].next=i+1;continue;
  }
  for(j=0;j<b.signature.parameters;j++){
   if(++i>=v->count)return 0;d=v->decoded+i;in=&d->instruction;kind=dis64u32(b.signature.data+12+4*j);width=dis64scalarwidth(kind);offset=(offset+width-1)&~(width-1);
   if(in->op!=(kind==Dis64Int?IMOVW:kind==Dis64Big?IMOVL:IMOVF) || (in->mode&ARM)!=AXNON ||
      UDST(in->mode)!=(AFP|AIND) || in->destination!=40 || d->secondary!=offset ||
      !dis64clientbit(slots,&nslots,v->frame,USRC(in->mode),in->source,kind,node->need))return 0;
   offset+=width;
  }
  result=b.signature.result;
  if(result){if(++i>=v->count)return 0;d=v->decoded+i;in=&d->instruction;
   if(in->op!=ILEA || in->mode!=(SRC(AFP)|DST(AFP|AIND)) || in->source!=v->frame-8 || in->destination!=40 || d->secondary!=32)return 0;
  }
  if(++i>=v->count)return 0;in=&v->decoded[i].instruction;
  if(in->op!=IMCALL || in->mode!=(SRC(AFP)|AXIMM|DST(AMP)) || in->source!=40 || in->destination || in->middle!=ordinal)return 0;
  v->calls++;
  if(result){if(++i>=v->count)return 0;in=&v->decoded[i].instruction;
   if(in->op!=(result==Dis64Int?IMOVW:result==Dis64Big?IMOVL:IMOVF) || in->mode!=(SRC(AFP)|DST(AFP)) || in->source!=v->frame-8 ||
      !dis64clientbit(slots,&nslots,v->frame,UDST(in->mode),in->destination,result,node->give))return 0;
  }
  nodes[start].next=i+1;
 }
 if((library?v->calls!=0:!v->calls) || v->decoded[v->count-1].instruction.op!=(unsigned int)(library?IRET:IEXIT))return 0;
 for(i=0;i<v->count;i++)if(nodes[i].boundary && ((nodes[i].next<v->count && !nodes[nodes[i].next].boundary) ||
    (nodes[i].target<v->count && !nodes[nodes[i].target].boundary)))return 0;
 if(!dis64clientflow(nodes,v->count,seed))return 0;return 1;
}
static int
dis64clientprove(Dis64ScalarClientView *v,int library){return dis64clientbodyprove(v,library,0);}
static int
dis64clientcheck(const unsigned char *data,unsigned int bytes,Dis64ScalarClientView *out)
{
 Dis64ScalarClientView v;
 if(!out || !dis64clientopen(data,bytes,&v) || !dis64clientprove(&v,0))return 0;
 *out=v;return 1;
}
#include "dis64-command-image.h"

#endif
