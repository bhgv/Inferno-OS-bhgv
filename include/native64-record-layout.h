#ifndef NATIVE64_RECORD_LAYOUT_H
#define NATIVE64_RECORD_LAYOUT_H
/* Private checked-IR annotation; never a serialized Dis opcode or descriptor. */
enum { Native64TypedRecordArg=33, Native64TypedRecordVararg=34, Native64TypedRecordArray=35,
       Native64TypedRecordNew=38, Native64TypedRecordCopy=39, Native64TypedRecordResult=41,
       Native64TypedRecordPair=42,
       Native64TypedRecord=72 };
/* Private descriptor: 24 native GC slots and a size in four-byte units. */
enum { Native64RecordBytes=192, Native64RecordMapBits=24, Native64RecordResultFlag=16384,
       Native64RecordPath=6 };
static unsigned int
native64recordbytes(unsigned long shape)
{
 return (shape>>Native64RecordMapBits)*4;
}
static unsigned int
native64recordmask(unsigned long shape)
{
 return shape&0xffffffUL;
}
static int
native64recorddescriptor(long shape)
{
 unsigned int bytes,mask;
 if(shape<=0||(unsigned long)shape>0x30ffffffUL)return 0;
 bytes=native64recordbytes(shape);mask=native64recordmask(shape);
 return bytes&&bytes<=Native64RecordBytes&&(!mask||bytes%8==0)&&!(mask>>(bytes/8));
}
static int
native64recordshape(unsigned int op,unsigned int*w,unsigned int*r)
{
 w[0]=w[1]=4;r[0]=r[1]=0;
 switch(op){
 case IMOVP:case IADDC:case IBEQL:case IBNEL:case IBEQC:case IBNEC:w[0]=w[1]=8;r[0]=r[1]=1;return 1;
 case IINDC:case ILENC:case ILENA:case ILENL:w[0]=8;r[0]=1;return 1;
 case IMOVW:case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:
 case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:
 case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:return 1;
 }return 0;
}
#endif
