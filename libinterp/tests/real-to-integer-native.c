#include "comp-amd64.c"
#include <assert.h>
#include <stdio.h>
#include <fenv.h>
#include <math.h>
REG R;
char exInval[]="invalid";
/* Native compilation uses ordinary owned allocations. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Valid descriptor compilation must not raise a VM error. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
/* Only accepted, in-range integers reach the C comparison path. */
void error(char *message) {(void)message;assert(0);}
/* Keep stack instrumentation without process-inspection leak checks. */
int __lsan_is_turned_off(void) {return 1;}
#define OP(name) void name(void)
IMPLEMENTATION

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
		0x1.fffffffffffffp62,-0x1.fffffffffffffp62};
	int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	uvlong state=0x7265616c696e74ULL;
	int i,op,sa,da,alias,round,flags,valid,width,native,expectedflags,co;
	uvlong scales[]={0x3ff0000000000000ULL,0xbff0000000000000ULL,0,0x4000000000000000ULL,0x7ff0000000000042ULL};
	fenv_t saved;
	REG savedreg;
	unsigned long checks=0;

	code[0].op=ICVTFW;code[1].op=ICVTFL;code[2].op=ICVTFX;module.prog=code;module.nprog=3;
	assert(compile(&module,3,nil) && module.jit->entry[ICVTFW] && module.jit->entry[ICVTFL] && module.jit->entry[ICVTFX]);
	assert(fegetenv(&saved)==0);
	for(i=0;i<256;i++){
		state=state*6364136223846793005ULL+1;
		if(i<(int)(sizeof(edges)/sizeof(edges[0])))input.value=edges[i];
		else if(i&1)input.bits=state;
		else input.value=(vlong)(state&((1ULL<<53)-1))-(vlong)(1ULL<<52);
		for(op=0;op<3;op++){
			width=op==1 ? 8 : sizeof(WORD);
			valid=isfinite(input.value) && fabs(input.value)<(width==4 ? 2147483647.5 : 0x1.fffffffffffffp62);
			for(co=0;co<(op==2 ? nelem(scales) : 1);co++)
			for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(alias=0;alias<6;alias++)
			for(round=0;round<4;round++)for(flags=0;flags<2;flags++){
				memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+24+sa;
				R.m=op==2 ? slots.bytes+56+da : (void*)1;
				R.d=alias==0 ? slots.bytes+80+da : alias==1 ? R.s : alias<4 ? (uchar*)R.s+(alias==2 ? 1 : -1) : slots.bytes+56+da+(alias==5);
				memmove(slots.bytes+56+da,&scales[co],8);memmove(R.s,&input.bits,8);memmove(before,slots.bytes,sizeof(before));
				R.PC=code;savedreg=R;
				assert(fesetround(rounds[round])==0);feclearexcept(FE_ALL_EXCEPT);
				if(flags)feraiseexcept(FE_DIVBYZERO);
				if(valid && (op!=2 || co<=1)){
					if(op==1)cvtfl();else if(op==2)cvtfx();else cvtfw();
					expectedflags=fetestexcept(FE_ALL_EXCEPT);
					memmove(expected,slots.bytes,sizeof(expected));memmove(slots.bytes,before,sizeof(before));
				}else expectedflags=flags ? FE_DIVBYZERO : 0;
				feclearexcept(FE_ALL_EXCEPT);if(flags)feraiseexcept(FE_DIVBYZERO);
				native=module.jit->entry[code[op].op](&R);assert(native==(valid && (op!=2 || co<=1)));
				assert(!memcmp(&R,&savedreg,sizeof(R)));
				assert(fegetround()==rounds[round] && fetestexcept(FE_ALL_EXCEPT)==expectedflags);
				assert(!memcmp(slots.bytes,native ? expected : before,sizeof(before)));

				checks++;
			}
		}
	}
	assert(fesetenv(&saved)==0);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS native real-integer leaves: %lu rounding/FP-status/alignment/alias/fallback checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
