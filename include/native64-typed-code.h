#ifndef NATIVE64_TYPED_CODE_H
#define NATIVE64_TYPED_CODE_H
/* Trusted checked-IR materialization only; no bytecode/file admission. */
#include "native64-model.h"
typedef struct Native64TypedStep Native64TypedStep;
struct Native64TypedStep {
	unsigned int op,mode;
	long s,m,d;
	unsigned int returned;
};

static int
native64typedbranch(unsigned int op)
{
	switch(op){
	case IJMP:
	case IBEQW:case IBNEW:case IBLTW:case IBLEW:case IBGTW:case IBGEW:
	case IBEQL:case IBNEL:case IBLTL:case IBLEL:case IBGTL:case IBGEL:
	case IBEQF:case IBNEF:case IBLTF:case IBLEF:case IBGTF:case IBGEF:return 1;
	}
	return 0;
}

static int
native64typedcodeoperand(unsigned int mode,long offset,unsigned int fpbytes,unsigned int mpbytes)
{
	if(mode==AXXX)return offset==0;
	if(mode==AIMM)return offset>=-536870912L && offset<=536870911L;
	if(offset<0 || (mode!=AFP && mode!=AMP))return 0;
	return (unsigned long)offset<(mode==AFP?fpbytes:mpbytes) && (mode!=AFP || offset>=64);
}

/* Input, layouts and semantic operand roles were proved by the compiler and
 * remain immutable. This bridge checks native encoding and local branch ownership.
 * Output must be disjoint from input. Refusal leaves all output bytes untouched.
 * No frame addresses are captured: result stores resolve FP[32] at execution. */
static int
native64typedcode(Inst *out,unsigned int capacity,const Native64TypedStep *steps,unsigned int count,unsigned int fpbytes,unsigned int mpbytes)
{
	unsigned int i,sm,dm,mm,width,reference;
	const Native64TypedStep *s;
	Inst *p;
	if(!native64model() || out==nil || steps==nil || !count || count>65536 || capacity<count ||
	   fpbytes<64 || fpbytes>1024*1024 || fpbytes%8 || mpbytes>1024*1024 || mpbytes%8)return 0;
	for(i=0;i<count;i++){
		s=steps+i;sm=USRC(s->mode);dm=UDST(s->mode);mm=s->mode&ARM;
		if(s->mode>255 || s->op>255)return 0;
		switch(s->op){
		case INOP:case IRET:
			if(s->mode!=(SRC(AXXX)|DST(AXXX)) || s->s || s->m || s->d || s->returned)return 0;
			continue;
		case IMOVB:case IMOVW:case IMOVL:case IMOVF:case IMOVP:
		case ICVTBW:case ICVTWB:case ICVTWL:case ICVTLW:case ICVTWF:case ICVTFW:case ICVTLF:case ICVTFL:
		case INEGF:case ILENC:case ILENL:case ILENA:
		case IADDW:case ISUBW:case IMULW:case IDIVW:case IMODW:case IANDW:case IORW:case IXORW:case ISHLW:case ISHRW:
		case IADDL:case ISUBL:case IMULL:case IDIVL:case IMODL:case IANDL:case IORL:case IXORL:case ISHLL:case ISHRL:
		case IADDF:case ISUBF:case IMULF:case IDIVF:break;
		default:if(!native64typedbranch(s->op))return 0;
		}
		if(!native64typedcodeoperand(sm,s->s,fpbytes,mpbytes))return 0;
		if(native64typedbranch(s->op)){
			if(dm!=AIMM || s->d<0 || (unsigned long)s->d>=count || s->returned)return 0;
		}else if(s->returned){
			width=s->returned&15;reference=!!(s->returned&16);
			if(dm!=(AFP|AIND) || s->d!=32 || (s->returned&~31) ||
			   (width!=1 && width!=4 && width!=8) || (reference && width!=8))return 0;
		}else if(!native64typedcodeoperand(dm,s->d,fpbytes,mpbytes) || dm==AIMM || dm==AXXX)return 0;
		if(mm==AXNON){if(s->m)return 0;}
		else if(mm==AXIMM){if(s->m < -32768 || s->m>32767)return 0;}
		else if(s->m>65535 || !native64typedcodeoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes))return 0;
	}
	for(i=0;i<count;i++){
		s=steps+i;p=out+i;memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;p->reg=s->m;
		p->s.imm=s->s;
		if(native64typedbranch(s->op))p->d.ins=out+s->d;
		else if(s->returned){p->d.i.f=32;p->d.i.s=0;}
		else p->d.ind=s->d;
	}
	return 1;
}
#endif
