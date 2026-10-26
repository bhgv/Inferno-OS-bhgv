#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
#define transfercheck(condition) transferpredicate(!!(condition)) // Keep RX/FP oracles active under NDEBUG.


/* Check native execution and FP results even in optimized fixture builds. */
static void
transferpredicate(int condition)
{
	if(!condition)
		abort();
}


/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#include <stdio.h>
#include <fenv.h>
#include <math.h>
REG R;


/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;
char exInval[]="invalid", exBounds[]="bounds";


/* Native compilation uses ordinary owned allocations. */
void*
mallocz(ulong n,int clear)
{
	void *p;

	p=malloc(n);
	if(p && clear)
		memset(p,0,n);
	return p;
}


/* Valid descriptor compilation must not raise a VM error. */
int
kprotect(int (*fn)(void*),void *arg)
{
	return fn(arg);
}


/* Only accepted, in-range integers reach the C comparison path. */
void error(char *message)
{
	(void)message;
	abort();
}


/* Keep stack instrumentation without process-inspection leak checks. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
#define OP(name) void name(void)
IMPLEMENTATION


/* Independently classify exact powers through frexp rather than emitter bit encodings. */
static int
fixedaccepted(REAL value,REAL scale,int width)
{
	int se,ve;
	REAL product,bound;

	bound=width==4 ? 2147483647.5 : 0x1.fffffffffffffp62;
	if(!isfinite(scale) || scale==0 || frexp(fabs(scale),&se)!=0.5 || se < -1021 || se > 1024)
		return 0;
	if(fabs(scale)==1)
		return fabs(value)<bound;
	if(!isnormal(value))
		return 0;
	(void)frexp(fabs(value),&ve);
	if(ve+se < -1020 || ve+se > 1025)
		return 0;
	product=value*scale;
	return fabs(product)<bound;
}


/* Compare native and C memory plus all rounding modes and sticky FP exceptions. */
int
main(void)
{
	union {uvlong align;uchar bytes[128];} slots;
	uchar before[128],expected[128];
	Module module={0};Inst code[3]={{0}};
	union {uvlong bits;REAL value;} input;
	REAL edges[]={0.,-0.,1.,-1.,.5,-.5,1.5,-1.5,2147483647.,-2147483648.,2147483648.,-2147483649.,0x1p52,-0x1p52,0x1.ffffffffffffep51,0x1p63,-0x1p63,INFINITY,-INFINITY,NAN,0x1p-1074,
		0x1p30,-0x1p30,2147483647.4999998,-2147483647.4999998,2147483647.5,-2147483647.5,
		0x1p62,-0x1p62,0x1.ffffffffffffep62,-0x1.ffffffffffffep62,
		0x1.fffffffffffffp62,-0x1.fffffffffffffp62,0x1p-1022,0x1.fffffffffffffp-1022,
		0x1p-1021,0x1p-1006,0x1p46,0x1p78,0x1p15,0x1p-16,0x1.fffffffffffffp1023,-0x1.fffffffffffffp1023,0x0.fffffffffffffp-1022};
	int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	uvlong state=0x7265616c696e74ULL;
	int i,op,sa,da,alias,round,flags,valid,width,native,expectedflags,co,accepted;
	REAL scale;
	uvlong base[]={0x3ff0000000000000ULL,0xbff0000000000000ULL,0,0x4000000000000000ULL,0x7ff0000000000042ULL,
		0xc000000000000000ULL,0x3fe0000000000000ULL,0xbfe0000000000000ULL,
		0x40f0000000000000ULL,0xc0f0000000000000ULL,0x3ef0000000000000ULL,0xbef0000000000000ULL,
		0x4100000000000000ULL,0x3ff8000000000000ULL,0x3ee0000000000000ULL,0x7ff0000000000000ULL,1,
		0x0010000000000000ULL,0x0170000000000000ULL,0x3bf0000000000000ULL,
		0x43f0000000000000ULL,0x7e70000000000000ULL,0x7fe0000000000000ULL};
	uvlong scales[128],bits;
	int nscale,k,negative,j;
	fenv_t saved;
	REG savedreg;
	unsigned long checks=0;

	nscale=nelem(base);
	memmove(scales,base,sizeof(base));
	for(k=-16;k<=16;k++)for(negative=0;negative<2;negative++){
		bits=((uvlong)(1023+k)<<52)|((uvlong)negative<<63);
		for(j=0;j<nscale && scales[j]!=bits;j++);
		if(j==nscale)scales[nscale++]=bits;
	}
	transfercheck(nscale<=nelem(scales));
	code[0].op=ICVTFW;code[1].op=ICVTFL;code[2].op=ICVTFX;module.prog=code;module.nprog=3;
	transfercheck(compile(&module,3,nil) && module.jit->entry[ICVTFW] && module.jit->entry[ICVTFL] && module.jit->entry[ICVTFX]);
	transfercheck(fegetenv(&saved)==0);
	for(i=0;i<256;i++){
		state=state*6364136223846793005ULL+1;
		if(i==250)input.bits=0x7ff0000000000042ULL;
		else if(i==251)input.bits=0xfff0000000000042ULL;
		else if(i<(int)(sizeof(edges)/sizeof(edges[0])))input.value=edges[i];
		else if(i&1)input.bits=state;
		else input.value=(vlong)(state&((1ULL<<53)-1))-(vlong)(1ULL<<52);
		for(op=0;op<3;op++){
			width=op==1 ? 8 : sizeof(WORD);
			valid=isfinite(input.value) && fabs(input.value)<(width==4 ? 2147483647.5 : 0x1.fffffffffffffp62);
			for(co=0;co<(op==2 ? nscale : 1);co++)
			for(sa=0;sa<(co<nelem(base) ? 8 : 2);sa++)for(da=0;da<(co<nelem(base) ? 8 : 2);da++)for(alias=0;alias<6;alias++)
			for(round=0;round<4;round++)for(flags=0;flags<2;flags++){
				memmove(&scale,&scales[co],sizeof(scale));
				accepted=op==2 ? fixedaccepted(input.value,scale,width) : valid;
				memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+24+sa;
				R.m=op==2 ? slots.bytes+56+da : (void*)1;
				R.d=alias==0 ? slots.bytes+80+da : alias==1 ? R.s : alias<4 ? (uchar*)R.s+(alias==2 ? 1 : -1) : slots.bytes+56+da+(alias==5);
				memmove(slots.bytes+56+da,&scales[co],8);memmove(R.s,&input.bits,8);memmove(before,slots.bytes,sizeof(before));
				R.PC=code;savedreg=R;
				transfercheck(fesetround(rounds[round])==0);feclearexcept(FE_ALL_EXCEPT);
				if(flags)feraiseexcept(FE_DIVBYZERO);
				if(accepted){
					if(op==1)cvtfl();else if(op==2)cvtfx();else cvtfw();
					expectedflags=fetestexcept(FE_ALL_EXCEPT);
					memmove(expected,slots.bytes,sizeof(expected));memmove(slots.bytes,before,sizeof(before));
				}else expectedflags=flags ? FE_DIVBYZERO : 0;
				feclearexcept(FE_ALL_EXCEPT);if(flags)feraiseexcept(FE_DIVBYZERO);
				native=module.jit->entry[code[op].op](&R);transfercheck(native==(accepted));
				transfercheck(!memcmp(&R,&savedreg,sizeof(R)));
				transfercheck(fegetround()==rounds[round] && fetestexcept(FE_ALL_EXCEPT)==expectedflags);
				transfercheck(!memcmp(slots.bytes,native ? expected : before,sizeof(before)));

				checks++;
			}
		}
	}
	transfercheck(fesetenv(&saved)==0);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS native real-integer leaves: %lu rounding/FP-status/alignment/alias/fallback checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
