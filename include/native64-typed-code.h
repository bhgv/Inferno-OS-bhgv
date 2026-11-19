#ifndef NATIVE64_TYPED_CODE_H
#define NATIVE64_TYPED_CODE_H
/* Trusted checked-IR materialization only; no bytecode/file admission. */
#include "native64-model.h"
/* Private compiler annotations: exact selected Draw/Wmclient root fields. */
enum { Native64TypedContext = 96, Native64TypedContextNil = 97, Native64TypedWindow = 100, Native64TypedWindowNil = 101, Native64TypedDraw = 113, Native64TypedDrawNil = 114, Native64TypedFieldArg = 125 };
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
	case IBEQF:case IBNEF:case IBLTF:case IBLEF:case IBGTF:case IBGEF:
    case IBEQC:case IBNEC:case IBLTC:case IBLEC:case IBGTC:case IBGEC:return 1;
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

static int
native64typedrootoperand(unsigned int mode,long offset,unsigned int fpbytes,unsigned int mpbytes)
{
 return (mode==AFP || mode==AMP) && native64typedcodeoperand(mode,offset,fpbytes,mpbytes) &&
  offset%8==0 && 8<=(mode==AFP?fpbytes:mpbytes)-offset;
}

static int
native64typedcontextcopy(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 return s->returned==173 && s->op==IMOVP && s->mode==(SRC(sm)|DST(dm)) &&
  (sm==(AFP|AIND) || sm==(AMP|AIND)) && (dm==(AFP|AIND) || dm==(AMP|AIND)) && (s->m==0 || s->m==8 || s->m==16) &&
  native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(dm&~AIND,s->d,fp,mp);
}

static int
native64typedcontextnew(const Native64TypedStep*s,unsigned int fp)
{
 return (s->op==INEW || s->op==INEWZ) && !s->s && !s->m &&
  ((s->returned==172 && s->mode==(SRC(AIMM)|DST(AFP)) && native64typedrootoperand(AFP,s->d,fp,0)) ||
   (s->returned==174 && s->mode==(SRC(AIMM)|DST(AFP|AIND)) && s->d==32));
}

static int
native64typedtriplecopy(const Native64TypedStep*s,unsigned int fp)
{
 return s->returned==171 && s->op==IMOVMP && s->mode==(SRC(AFP)|AXIMM|DST(AFP|AIND)) && !s->m && s->d==32 && s->s>=64 && s->s%8==0 && (unsigned long)s->s<=fp && 24<=fp-s->s;
}

static int
native64typedwordoperand(unsigned int mode,long offset,unsigned int fpbytes,unsigned int mpbytes)
{
 return native64typedcodeoperand(mode,offset,fpbytes,mpbytes) &&
  (mode==AIMM || ((mode==AFP || mode==AMP) && offset%4==0 && 4<=(mode==AFP?fpbytes:mpbytes)-offset));
}

static int
native64typedchannelop(unsigned int op)
{
 return op==INEWCB || op==INEWCW || op==INEWCL || op==INEWCF || op==INEWCP;
}
static int
native64typedchannel(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int mm=s->mode&ARM,dm=UDST(s->mode),mode=mm==AXIMM?AIMM:mm==AXINF?AFP:AMP;
 return native64typedchannelop(s->op) && s->mode<=255 && USRC(s->mode)==AXXX && !s->s &&
  ((s->returned==0 && native64typedrootoperand(dm,s->d,fp,mp)) || (s->returned==24 && dm==(AFP|AIND) && s->d==32)) &&
  (mm==AXNON?!s->m:native64typedwordoperand(mode,s->m,fp,mp) && (mm!=AXIMM || (s->m>=-32768 && s->m<=32767)));
}

static int
native64typedtransfer(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int cm=s->op==ISEND?UDST(s->mode):USRC(s->mode),vm=s->op==ISEND?USRC(s->mode):UDST(s->mode),width=s->m&15;
 long channel=s->op==ISEND?s->d:s->s,value=s->op==ISEND?s->s:s->d;
 if(s->returned!=188 || (s->op!=ISEND && s->op!=IRECV) || s->mode>255 || (s->mode&ARM)!=AXNON ||
    (s->m!=1 && s->m!=4 && s->m!=8 && s->m!=24) || !native64typedrootoperand(cm,channel,fp,mp))return 0;
 if(s->m==24)return native64typedrootoperand(vm,value,fp,mp);
 if(vm==AIMM)return s->op==ISEND && width<8 && value>=-536870912L && value<=536870911L && (width!=1 || (value>=0 && value<=255));
 return (vm==AFP || vm==AMP) && native64typedcodeoperand(vm,value,fp,mp) && value%width==0 && width<=(vm==AFP?fp:mp)-value;
}

/* Payload root-count is a checked role, relocated to a private native Type. */
static int
native64typedmappedchannel(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 Native64TypedStep copy=*s;
 if(s->op!=INEWCMP || USRC(s->mode)!=AIMM || (s->s!=2 && s->s!=3) || (s->returned!=183 && s->returned!=185))return 0;
 copy.op=INEWCP;copy.mode=(s->mode&~(7<<3))|SRC(AXXX);copy.s=0;copy.returned=s->returned==185?24:0;
 return native64typedchannel(&copy,fp,mp);
}

static int
native64typedstringstep(const Native64TypedStep *s,unsigned int fpbytes,unsigned int mpbytes)
{
 unsigned int mm=s->mode&ARM;
 if(s->op==IADDC && s->returned==24){
  return s->mode<=255 && UDST(s->mode)==(AFP|AIND) && s->d==32 && (mm==AXINF || mm==AXINM) && s->m<=65535 &&
   native64typedrootoperand(USRC(s->mode),s->s,fpbytes,mpbytes) && native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes);
 }
 if(s->mode>255 || s->returned || (UDST(s->mode)!=AFP && !(s->op==ISLICEC && UDST(s->mode)==AMP)))return 0;
 if(s->op==IADDC)return (mm==AXINF || mm==AXINM) &&
  native64typedrootoperand(USRC(s->mode),s->s,fpbytes,mpbytes) &&
  native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes) && s->m<=65535 &&
  native64typedrootoperand(AFP,s->d,fpbytes,mpbytes);
 if(s->op!=IINDC && s->op!=ISLICEC)return 0;
 if(mm!=AXIMM && mm!=AXINF)return 0;
 if(mm==AXIMM){if(s->m< -32768 || s->m>32767)return 0;}
 else if(s->m>65535 || !native64typedwordoperand(AFP,s->m,fpbytes,mpbytes))return 0;
 return s->op==IINDC?
  native64typedrootoperand(USRC(s->mode),s->s,fpbytes,mpbytes) && native64typedwordoperand(AFP,s->d,fpbytes,mpbytes):
  native64typedwordoperand(USRC(s->mode),s->s,fpbytes,mpbytes) && native64typedrootoperand(UDST(s->mode),s->d,fpbytes,mpbytes);
}

/* Byte arrays only. NEWAZ's zero is a private byte-Type token; the owning
 * module builder resolves it to its separately retained primitive descriptor. */
static int
native64typedbytestep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int mm=s->mode&ARM;
 if(s->mode>255)return 0;
 if(s->returned){if(s->op!=ICVTAC || s->returned!=24 || UDST(s->mode)!=(AFP|AIND) || s->d!=32)return 0;}
 else if(UDST(s->mode)!=AFP || !native64typedrootoperand(AFP,s->d,fp,mp))return 0;
 if(s->op==ICVTAC)return mm==AXNON && !s->m && native64typedrootoperand(USRC(s->mode),s->s,fp,mp);
 if(!native64typedwordoperand(USRC(s->mode),s->s,fp,mp))return 0;
 if(s->op==INEWAZ)return mm==AXIMM && s->m==0;
 if(s->op!=ISLICEA)return 0;
 return mm==AXIMM?(s->m>=-32768 && s->m<=32767):mm==AXINF && s->m<=65535 && native64typedwordoperand(AFP,s->m,fp,mp);
}

/* Context has exactly three pointer fields (Display, Screen, wm channel).
 * The compiler proves the nominal owner and field class; no generic ADT ABI. */
static int
native64typedcontextstep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),base,value;
 if(s->returned>=Native64TypedContextNil && s->returned<Native64TypedContextNil+3){
  return (s->op==IBEQL || s->op==IBNEL) && s->mode==(SRC(sm)|AXINM|DST(AIMM)) &&
   (sm==(AFP|AIND) || sm==(AMP|AIND)) && s->m<=65535 &&
   native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(AMP,s->m,fp,mp);
 }
 if(s->op!=IMOVP || s->returned!=Native64TypedContext || s->mode>255 || (s->mode&ARM)!=AXNON ||
    (s->m!=0 && s->m!=8 && s->m!=16))return 0;
 if(sm==(AFP|AIND) || sm==(AMP|AIND)){base=sm&~AIND;value=dm;}
 else if(dm==(AFP|AIND) || dm==(AMP|AIND)){base=dm&~AIND;value=sm;}
 else return 0;
 return native64typedrootoperand(base,(sm&AIND)?s->s:s->d,fp,mp) &&
  native64typedrootoperand(value,(sm&AIND)?s->d:s->s,fp,mp);
}

static unsigned int
native64windowfieldoffset(unsigned int index)
{
 static const unsigned int offsets[]={0,24,48,64,72,104};
 return index<6?offsets[index]:~0U;
}
static int
native64typedwindowstep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),i;
 if(s->returned>=Native64TypedWindowNil && s->returned<Native64TypedWindowNil+6){
  return (s->op==IBEQL || s->op==IBNEL) && s->mode==(SRC(sm)|AXINM|DST(AIMM)) &&
   (sm==(AFP|AIND) || sm==(AMP|AIND)) && s->m<=65535 &&
   native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(AMP,s->m,fp,mp);
 }
 if(s->returned!=Native64TypedWindow || s->m<0)return 0;
 for(i=0;i<6;i++)if(s->m==native64windowfieldoffset(i))break;
 if(i==6)return 0;
 return s->op==IMOVP && s->mode<=255 && (s->mode&ARM)==AXNON &&
  ((sm==(AFP|AIND) || sm==(AMP|AIND))?
    native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(dm,s->d,fp,mp):
    (dm==(AFP|AIND) || dm==(AMP|AIND)) && native64typedrootoperand(sm,s->s,fp,mp) && native64typedrootoperand(dm&~AIND,s->d,fp,mp));
}

static unsigned int
native64drawfieldoffset(unsigned int index)
{
 static const unsigned int offsets[]={48,56,64,8,16,24,0,8,16,24,32};return index<11?offsets[index]:~0U;
}
static int
native64typedfieldoffset(long offset,int wide)
{
 unsigned int i;if(wide && (offset==72 || offset==104))return 1;
 for(i=0;i<11;i++)if(offset==native64drawfieldoffset(i))return 1;return 0;
}
static int
native64typeddrawstep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode);
 if(s->returned>=Native64TypedDrawNil && s->returned<Native64TypedDrawNil+11){
  return s->returned!=Native64TypedDrawNil+2 && (s->op==IBEQL || s->op==IBNEL) && s->mode==(SRC(sm)|AXINM|DST(AIMM)) &&
   (sm==(AFP|AIND) || sm==(AMP|AIND)) && s->m<=65535 && native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(AMP,s->m,fp,mp);
 }
 if(s->returned!=Native64TypedDraw || !native64typedfieldoffset(s->m,0))return 0;
 return s->op==IMOVP && s->mode<=255 && (s->mode&ARM)==AXNON &&
  ((sm==(AFP|AIND) || sm==(AMP|AIND))?
   native64typedrootoperand(sm&~AIND,s->s,fp,mp) && native64typedrootoperand(dm,s->d,fp,mp):
   (dm==(AFP|AIND) || dm==(AMP|AIND)) && native64typedrootoperand(sm,s->s,fp,mp) && native64typedrootoperand(dm&~AIND,s->d,fp,mp));
}

/* Input, layouts and semantic operand roles were proved by the compiler and
 * remain immutable. This bridge checks native encoding and local branch ownership.
 * Output must be disjoint from input. Refusal leaves all output bytes untouched.
 * No frame addresses are captured: result stores resolve FP[32] at execution. */
static int
native64typedaltstep(const Native64TypedStep*s,unsigned int fp,unsigned int mp)
{
 unsigned int sm=USRC(s->mode),dm=UDST(s->mode),w=s->m&15;
 if(s->mode>255 || (s->mode&ARM)!=AXNON || dm!=AFP)return 0;
 if(s->returned==189 || s->returned==192 || s->returned==193)return s->op==IMOVL && native64typedrootoperand(AFP,s->d,fp,mp) &&
  (s->returned==189?(!s->m && native64typedrootoperand(sm,s->s,fp,mp)):((s->returned==192?s->m==64:(s->m>=0 && s->m<=32 && s->m%8==0)) && (sm==(AFP|AIND) || sm==(AMP|AIND)) && native64typedrootoperand(sm&~AIND,s->s,fp,mp)));
 if(s->returned==190)return s->op==ILEA && (s->m==4 || s->m==24) && native64typedrootoperand(AFP,s->d,fp,mp) &&
  (sm==AFP || sm==AMP) && s->s>=0 && !(s->s%w) && native64typedcodeoperand(sm,s->s,fp,mp) && w<=(sm==AFP?fp:mp)-s->s;
 return s->returned==191 && s->op==IALT && sm==AFP && s->m>=1 && s->m<=32 &&
  native64typedrootoperand(AFP,s->s,fp,mp) && 8+16*s->m<=fp-s->s && native64typedwordoperand(AFP,s->d,fp,mp);
}

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
        if(s->returned>=Native64TypedContext && s->returned<Native64TypedContextNil+3){
            if(!native64typedcontextstep(s,fpbytes,mpbytes) ||
               (s->returned!=Native64TypedContext && (s->d<0 || (unsigned long)s->d>=count)))return 0;
            continue;
        }
        if(s->returned>=Native64TypedWindow && s->returned<Native64TypedWindowNil+6){
            if(!native64typedwindowstep(s,fpbytes,mpbytes) ||
               (s->returned!=Native64TypedWindow && (s->d<0 || (unsigned long)s->d>=count)))return 0;
            continue;
        }
        if(s->returned>=Native64TypedDraw && s->returned<Native64TypedDrawNil+11){
            if(!native64typeddrawstep(s,fpbytes,mpbytes) || (s->returned!=Native64TypedDraw && (s->d<0 || (unsigned long)s->d>=count)))return 0;continue;
        }
        if(s->returned==173){if(!native64typedcontextcopy(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==172 || s->returned==174){if(!native64typedcontextnew(s,fpbytes))return 0;continue;}
        if(s->returned==171){if(!native64typedtriplecopy(s,fpbytes))return 0;continue;}
        if(s->returned>=189 && s->returned<=193){if(!native64typedaltstep(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==188){if(!native64typedtransfer(s,fpbytes,mpbytes))return 0;continue;}
        if(s->returned==183 || s->returned==185){if(!native64typedmappedchannel(s,fpbytes,mpbytes))return 0;continue;}
        if(native64typedchannelop(s->op)){if(!native64typedchannel(s,fpbytes,mpbytes))return 0;continue;}
		switch(s->op){
		case INEWAZ:case ISLICEA:case ICVTAC:
            if(!native64typedbytestep(s,fpbytes,mpbytes))return 0;
            continue;
        case ICVTWC:
            if(s->returned || dm!=AFP || mm!=AXNON || s->m || !native64typedwordoperand(sm,s->s,fpbytes,mpbytes) ||
               !native64typedrootoperand(AFP,s->d,fpbytes,mpbytes))return 0;
            continue;
		case IADDC:case IINDC:case ISLICEC:
            if(!native64typedstringstep(s,fpbytes,mpbytes))return 0;
            continue;
        case IBEQC:case IBNEC:case IBLTC:case IBLEC:case IBGTC:case IBGEC:
            if(s->returned || dm!=AIMM || s->d<0 || (unsigned long)s->d>=count || (mm!=AXINF && mm!=AXINM) || s->m>65535 ||
               !native64typedrootoperand(sm,s->s,fpbytes,mpbytes) || !native64typedrootoperand(mm==AXINF?AFP:AMP,s->m,fpbytes,mpbytes))return 0;
            continue;
		case IRAISE:
            if(dm!=AXXX || mm!=AXNON || s->m || s->d || s->returned ||
               !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
            continue;
        case ICONSW:
            if(s->returned || mm!=AXNON || s->m || !native64typedwordoperand(sm,s->s,fpbytes,mpbytes) || !native64typedrootoperand(dm,s->d,fpbytes,mpbytes))return 0;
            continue;
        case IHEADW:
            if(mm!=AXNON || s->m || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
            if(s->returned){if(s->returned!=4 || dm!=(AFP|AIND) || s->d!=32)return 0;}
            else if(dm==AIMM || !native64typedwordoperand(dm,s->d,fpbytes,mpbytes))return 0;
            continue;
		case ICONSP:case IHEADP:case ITAIL:
            if(mm!=AXNON || s->m || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
            if(s->returned){
                if(s->op==ICONSP || s->returned!=24 || dm!=(AFP|AIND) || s->d!=32)return 0;
            }else if(!native64typedrootoperand(dm,s->d,fpbytes,mpbytes))return 0;
            continue;
		case ILOAD:
            if((sm!=AFP && sm!=AMP) || (dm!=AFP && dm!=AMP) || mm!=AXIMM || s->m<0 || s->m>=64 ||
               s->returned || !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) ||
               !native64typedcodeoperand(dm,s->d,fpbytes,mpbytes) || s->s%8 || s->d%8 ||
               8>(sm==AFP?fpbytes:mpbytes)-s->s || 8>(dm==AFP?fpbytes:mpbytes)-s->d)return 0;
            continue;
		case ICASE:case ICASEL:case IGOTO:case ICASEC:
            width=s->op==ICASEL || s->op==ICASEC?8:4;
            if((sm!=AFP && sm!=AMP) || dm!=AMP || mm!=AXNON || s->m || s->returned ||
               !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || s->s%width ||
               width>(sm==AFP?fpbytes:mpbytes)-s->s || s->d<0 || s->d%4 ||
               (unsigned long)s->d>mpbytes || 4>mpbytes-s->d || (s->op==IGOTO && s->d<4))return 0;
            continue;
		case INOP:case IRET:case IEXIT:
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
        }else if(s->returned>=168 && s->returned<=170){
            if(s->op!=IMOVP || dm!=(AFP|AIND) || mm!=AXNON || s->m || s->d!=32+8*(s->returned-168) || !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
		}else if(s->returned==40 || s->returned==56){
            if(s->op!=IMOVP || dm!=(AFP|AIND) || mm!=AXNON || s->m || s->d!=(s->returned==40?32:40) ||
               !native64typedrootoperand(sm,s->s,fpbytes,mpbytes))return 0;
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
        if(s->returned>=189 && s->returned<=193){p->reg=s->returned==190?256|s->m:0;p->d.ind=s->d;if(s->returned==192 || s->returned==193){p->s.i.f=s->s;p->s.i.s=s->returned==192?64:s->m;}continue;}
        if(s->returned==188){p->reg=0;p->d.ind=s->d;continue;}
        if(s->returned==183){p->d.ind=s->d;continue;}
        if(s->returned==185){p->d.i.f=32;p->d.i.s=0;continue;}
        if(s->returned==172){p->d.ind=s->d;continue;}
        if(s->returned==174){p->d.i.f=32;p->d.i.s=0;continue;}
        if(s->returned==173){p->reg=0;p->s.i.f=s->s;p->s.i.s = s->m;p->d.i.f=s->d;p->d.i.s=s->m;continue;}
        if(s->returned>=Native64TypedContextNil && s->returned<Native64TypedContextNil+3){
            p->s.i.f=s->s;p->s.i.s=8*(s->returned-Native64TypedContextNil);p->d.ins=out+s->d;continue;
        }
        if(s->returned>=Native64TypedWindowNil && s->returned<Native64TypedWindowNil+6){
            p->s.i.f=s->s;p->s.i.s=native64windowfieldoffset(s->returned-Native64TypedWindowNil);p->d.ins=out+s->d;continue;
        }
        if(s->returned>=Native64TypedDrawNil && s->returned<Native64TypedDrawNil+11){
            p->s.i.f=s->s;p->s.i.s=native64drawfieldoffset(s->returned-Native64TypedDrawNil);p->d.ins=out+s->d;continue;
        }
        if(s->returned==Native64TypedContext || s->returned==Native64TypedWindow || s->returned==Native64TypedDraw){
            p->reg=0;
            if(USRC(s->mode)&AIND){p->s.i.f=s->s;p->s.i.s=s->m;p->d.ind=s->d;}
            else{p->s.ind=s->s;p->d.i.f=s->d;p->d.i.s=s->m;}
            continue;
        }
		if(native64typedbranch(s->op))p->d.ins=out+s->d;
        else if(s->returned>=168 && s->returned<=170){p->d.i.f=32;p->d.i.s=8*(s->returned-168);}
		else if(s->returned){p->d.i.f=32;p->d.i.s=s->returned==56?8:0;}
		else p->d.ind=s->d;
	}
	return 1;
}

/* Canonical compiler-proved forwarding wrapper. target/type belong to the same
 * unpublished trusted module; no native pointers are supplied by a file. */
static int
native64typedtailcode(Inst *out,unsigned int capacity,const Native64TypedStep *steps,unsigned int count,unsigned int fpbytes,unsigned int mpbytes,Type *callee,Inst *target,unsigned int ordinal)
{
    unsigned int i,sm;
    const Native64TypedStep *s;
    Inst *p;
    if(!native64model() || out==nil || steps==nil || count<4 || count>65536 || capacity<count ||
       callee==nil || callee==H || target==nil || target==H || ordinal>536870911 ||
       fpbytes<64 || fpbytes>1024*1024 || fpbytes%8 || mpbytes>1024*1024 || mpbytes%8 ||
       callee->size<64 || callee->size>65536 || callee->size%8)return 0;
    s=steps;
    if(s->op!=IFRAME || s->mode!=(SRC(AIMM)|DST(AFP)) || s->m || s->d!=40 || s->returned)return 0;
    for(i=1;i<count-3;i++){
        s=steps+i;sm=USRC(s->mode);
        if((s->op!=IMOVB && s->op!=IMOVW && s->op!=IMOVL && s->op!=IMOVF && s->op!=IMOVP) ||
           s->mode!=(SRC(sm)|DST(AFP|AIND)) || s->m || s->returned!=64 ||
           !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || sm==AXXX ||
           s->d<64 || (unsigned long)s->d>=(unsigned int)callee->size || s->d>65535)return 0;
    }
    s=steps+count-3;
    if(s->op!=IMOVL || s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->s!=32 || s->m || s->d!=32 || s->returned!=64)return 0;
    s=steps+count-2;
    if(s->op!=ICALL || s->mode!=(SRC(AFP)|DST(AIMM)) || s->s!=40 || s->m || s->returned)return 0;
    s=steps+count-1;
    if(s->op!=IRET || s->mode!=(SRC(AXXX)|DST(AXXX)) || s->s || s->m || s->d || s->returned)return 0;
    for(i=0;i<count;i++){
        s=steps+i;p=out+i;memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;
        if(i==0){p->s.imm=ordinal;p->d.ind=40;}
        else if(i==count-2){p->s.ind=40;p->d.ins=target;}
        else if(i<count-1){p->s.imm=s->s;p->d.i.f=40;p->d.i.s=s->d;}
    }
    return 1;
}

/* One proved call island at entry followed by ordinary typed instructions.
 * Target/Type/ordinal identity is supplied by the trusted unpublished builder. */
static int
native64typedlocalcode(Inst *out,unsigned int capacity,const Native64TypedStep *steps,unsigned int count,unsigned int end,unsigned int fpbytes,unsigned int mpbytes,Type *callee,Inst *target,unsigned int ordinal)
{
    unsigned int i,sm,width,contract;Inst probe,*p;Native64TypedStep checkstep;const Native64TypedStep *s;
    if(!native64model() || out==nil || steps==nil || count<4 || count>65536 || end<2 || end>=count-1 || capacity<count ||
       callee==nil || callee==H || target==nil || target==H || ordinal>536870911 ||
       fpbytes<64 || fpbytes>1024*1024 || fpbytes%8 || mpbytes>1024*1024 || mpbytes%8 ||
       callee->size<64 || callee->size>65536 || callee->size%8)return 0;
    s=steps;
    if(s->op!=IFRAME || s->mode!=(SRC(AIMM)|DST(AFP)) || s->m || s->d!=40 || s->returned)return 0;
    for(i=1;i<end-1;i++){
        s=steps+i;sm=USRC(s->mode);
        if((s->op!=IMOVB && s->op!=IMOVW && s->op!=IMOVL && s->op!=IMOVF && s->op!=IMOVP) ||
           s->mode!=(SRC(sm)|DST(AFP|AIND)) || s->m || s->returned!=64 ||
           !native64typedcodeoperand(sm,s->s,fpbytes,mpbytes) || sm==AXXX ||
           s->d<64 || (unsigned long)(s->d)>=(unsigned int)callee->size || s->d>65535)return 0;
    }
    s=steps+end-1;contract=s->returned&31;width=contract&15;
    if(s->op!=ILEA || s->mode!=(SRC(AFP)|DST(AFP|AIND)) || s->m || s->d!=32 || s->returned!=(128|contract) ||
       (width!=1 && width!=4 && width!=8) || ((contract&16) && width!=8) || s->s<64 ||
       (unsigned long)s->s>fpbytes || width>fpbytes-s->s || s->s%width)return 0;
    s=steps+end;
    if(s->op!=ICALL || s->mode!=(SRC(AFP)|DST(AIMM)) || s->m || s->s!=40 || s->returned)return 0;
    for(i=end+1;i<count;i++){
        checkstep=steps[i];
        if(native64typedbranch(checkstep.op)){
            if(checkstep.d<0 || (unsigned long)checkstep.d>=count || ((unsigned long)checkstep.d<=end))return 0;
            checkstep.d=0;
        }
        if(!native64typedcode(&probe,1,&checkstep,1,fpbytes,mpbytes))return 0;
    }
    for(i=0;i<count;i++){
        s=steps+i;p=out+i;
        if(i>end){
            checkstep=*s;if(native64typedbranch(s->op))checkstep.d=0;
            native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);
            if(native64typedbranch(s->op))p->d.ins=out+s->d;
            continue;
        }
        memset(p,0,sizeof(*p));p->op=s->op;p->add=s->mode;
        if(i==0){p->s.imm=ordinal;p->d.ind=40;}
        else if(i==end){p->s.ind=40;p->d.ins=target;}
        else{p->s.imm=s->s;p->d.i.f=40;p->d.i.s=s->d;if(i==end-1)p->reg=contract;}
    }
    return 1;
}
#endif
