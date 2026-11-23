#ifndef NATIVE64_RECORD_LAYOUT_H
#define NATIVE64_RECORD_LAYOUT_H
/* Private checked-IR annotation; never a serialized Dis opcode or descriptor. */
enum { Native64TypedRecordArg=33, Native64TypedRecordVararg=34, Native64TypedRecord=72 };
static int
native64recordshape(unsigned int op,unsigned int*w,unsigned int*r)
{
 w[0]=w[1]=4;r[0]=r[1]=0;
 switch(op){
 case IMOVP:case IBEQL:case IBNEL:case IBEQC:case IBNEC:w[0]=w[1]=8;r[0]=r[1]=1;return 1;
 case IINDC:case ILENC:case ILENA:case ILENL:w[0]=8;r[0]=1;return 1;
 case IMOVW:case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:
 case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:
 case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:return 1;
 }return 0;
}
#endif
