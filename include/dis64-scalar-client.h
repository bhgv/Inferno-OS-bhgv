/* Bounded linear importing command. No new wire IDs or serialized native facts. */
#ifndef DIS64_SCALAR_CLIENT_H
#define DIS64_SCALAR_CLIENT_H
#include "dis64-call-library.h"
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
 unsigned int count,frame,calls;
 Dis64CallDecoded decoded[Dis64ClientCode];
};

typedef struct Dis64ClientSlot Dis64ClientSlot;
struct Dis64ClientSlot {unsigned int offset,kind,initialized;};
/* Numeric spans have one immutable kind; reads additionally require a definition. */
static int
dis64clientslot(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,long offset,unsigned int kind,int write)
{
 unsigned int i,width=dis64scalarwidth(kind),at;
 if(!width || kind==Dis64Bool || offset<64 || (unsigned long)offset>frame-16)return 0;
 at=offset;if(at%width || width>frame-16-at)return 0;
 for(i=0;i<*count;i++){
  if(at==slots[i].offset){if(slots[i].kind!=kind || (!write && !slots[i].initialized))return 0;slots[i].initialized|=write;return 1;}
  if(at<slots[i].offset+dis64scalarwidth(slots[i].kind) && slots[i].offset<at+width)return 0;
 }
 if(!write || *count==Dis64ClientSlots)return 0;
 slots[*count]=(Dis64ClientSlot){at,kind,1};(*count)++;return 1;
}
static int
dis64clientread(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,unsigned int mode,long value,unsigned int kind)
{
 return mode==AIMM?kind==Dis64Int:mode==AFP && dis64clientslot(slots,count,frame,value,kind,0);
}
/* No branches, hidden MP access, references or prepared-frame access here. */
static int
dis64clientnumeric(Dis64ClientSlot *slots,unsigned int *count,unsigned int frame,const Dis64Instruction *in)
{
 unsigned int kind=Dis64Int,sourcekind,mm=in->mode&ARM,lm;long lhs;int binary=0;
 switch(in->op){
 case IMOVW:break;case IMOVL:kind=Dis64Big;break;case IMOVF:kind=Dis64Real;break;
 case ICVTWL:case ICVTWF:kind=in->op==ICVTWL?Dis64Big:Dis64Real;break;
 case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:binary=1;break;
 case IADDL:case ISUBL:case IMULL:case IDIVL:case IMODL:case IANDL:case IORL:case IXORL:case ISHLL:case ISHRL:kind=Dis64Big;binary=1;break;
 default:return 0;
 }
 /* Native Inst.reg holds a signed16 middle immediate; wire signed30 is wider. */
 if(mm==AXIMM && (kind!=Dis64Int || in->middle < -32768 || in->middle > 32767))return 0;
 sourcekind=in->op==ICVTWL || in->op==ICVTWF || in->op==ISHLL || in->op==ISHRL?Dis64Int:kind;
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
    v.module.entry || v.module.frametype || v.module.databytes!=24 || v.module.datatype!=1 ||
    !dis64typescheck(data+types.offset,types.bytes,2,4096,&v.types) || v.types.count!=2 ||
    !dis64typeget(&v.types,0,&frame) || !dis64typeget(&v.types,1,&mp) || frame.bytes<80 || frame.align!=8 ||
    mp.bytes!=24 || mp.align!=8 || mp.mapbytes!=1 || mp.map[0]!=0xe0)return 0;
 for(i=0;i<frame.mapbytes;i++)if(frame.map[i])return 0;
 if(!disstringpoolcheck(data+v.names.offset,v.names.bytes,65536,&v.strings) ||
    !dis64bindingscheck(data+v.requests.offset,v.requests.bytes,0,&v.strings,0,0,64,64,&v.imports) ||
    v.requests.bytes>Dis64LocalBytes || v.names.bytes>Dis64LocalBytes || !v.imports.count || v.strings.count!=v.imports.count+3 || v.code.bytes<4)return 0;
 for(i=0;i<v.strings.count;i++){
  if(!disstringpoolget(&v.strings,i,&name) || !name.count || name.count>(i==1?1023U:255U))return 0;
  for(j=0;j<name.count;j++)if(!disruneat(&name,j,&rune) || !rune || rune>127 || (i==1 && j==0 && rune=='$'))return 0;
 }
 for(i=0;i<v.imports.count;i++){Dis64Binding b;if(!dis64bindingget(&v.imports,i,&b) || b.module!=2 || b.symbol!=i+3 || b.signature.result>Dis64Real)return 0;for(j=0;j<b.signature.parameters;j++)if(dis64u32(b.signature.data+12+4*j)>Dis64Real)return 0;}
 v.count=dis64u32(data+v.code.offset);if(v.count<4 || v.count>Dis64ClientCode)return 0;
 v.frame=frame.bytes;*out=v;return 1;
}
/* Prove every decoded instruction from an empty entry state, publishing atomically.
 * No branch/call/function manifest can bypass the complete linear role proof. */
static int
dis64clientcheck(const unsigned char *data,unsigned int bytes,Dis64ScalarClientView *out)
{
 Dis64ScalarClientView v;Dis64ClientSlot slots[Dis64ClientSlots]={{0}};unsigned int nslots=0,i,j,at=4,used,offset,kind,width,result,ordinal;int loaded=0;
 Dis64CallDecoded *d;Dis64Instruction *in;Dis64Binding b;
 if(!out || !dis64clientopen(data,bytes,&v))return 0;
 for(i=0;i<v.count;i++){if(at>=v.code.bytes || !dis64callread(data+v.code.offset+at,v.code.bytes-at,v.decoded+i,&used))return 0;at+=used;}
 if(at!=v.code.bytes)return 0;
 for(i=0;i<v.count;i++){
  d=v.decoded+i;in=&d->instruction;
  if(in->op==ILOAD){if(in->mode!=(SRC(AMP)|AXIMM|DST(AMP)) || in->middle || in->source!=16 || in->destination)return 0;loaded=1;continue;}
  if(in->op==IEXIT){if(i!=v.count-1 || in->mode!=(SRC(AXXX)|DST(AXXX)) || !v.calls)return 0;continue;}
  if(i==v.count-1)return 0;
  if(in->op!=IMFRAME){if(!dis64clientnumeric(slots,&nslots,v.frame,in))return 0;continue;}
  if(!loaded || in->mode!=(SRC(AMP)|AXIMM|DST(AFP)) || in->source || in->destination!=40 || in->middle<0 ||
     (unsigned long)in->middle>=v.imports.count || !dis64bindingget(&v.imports,in->middle,&b))return 0;
  ordinal=in->middle;offset=64;
  for(j=0;j<b.signature.parameters;j++){
   if(++i>=v.count)return 0;d=v.decoded+i;in=&d->instruction;kind=dis64u32(b.signature.data+12+4*j);width=dis64scalarwidth(kind);offset=(offset+width-1)&~(width-1);
   if(in->op!=(kind==Dis64Int?IMOVW:kind==Dis64Big?IMOVL:IMOVF) || (in->mode&ARM)!=AXNON ||
      UDST(in->mode)!=(AFP|AIND) || in->destination!=40 || d->secondary!=offset ||
      !dis64clientread(slots,&nslots,v.frame,USRC(in->mode),in->source,kind))return 0;
   offset+=width;
  }
  result=b.signature.result;
  if(result){if(++i>=v.count)return 0;d=v.decoded+i;in=&d->instruction;
   if(in->op!=ILEA || in->mode!=(SRC(AFP)|DST(AFP|AIND)) || in->source!=v.frame-8 || in->destination!=40 || d->secondary!=32)return 0;
  }
  if(++i>=v.count)return 0;in=&v.decoded[i].instruction;
  if(in->op!=IMCALL || in->mode!=(SRC(AFP)|AXIMM|DST(AMP)) || in->source!=40 || in->destination || in->middle!=ordinal)return 0;
  v.calls++;
  if(result){if(++i>=v.count)return 0;in=&v.decoded[i].instruction;
   if(in->op!=(result==Dis64Int?IMOVW:result==Dis64Big?IMOVL:IMOVF) || in->mode!=(SRC(AFP)|DST(AFP)) || in->source!=v.frame-8 ||
      !dis64clientslot(slots,&nslots,v.frame,in->destination,result,1))return 0;
  }
 }
 if(v.decoded[v.count-1].instruction.op!=IEXIT)return 0;*out=v;return 1;
}
#endif
