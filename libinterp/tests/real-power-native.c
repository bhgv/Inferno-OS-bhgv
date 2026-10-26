#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#include <assert.h>
#include <stdio.h>
#include <fenv.h>
#include <math.h>
REG R;
/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;
/* Compile real module descriptors using owned ordinary allocations. */
void *mallocz(ulong n,int clear){void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Valid compilation uses the production compiler's protected callback. */
int kprotect(int (*fn)(void*),void *arg){return fn(arg);}
/* Real IEEE power arithmetic does not raise an interpreter exception. */
void error(char *message){(void)message;assert(0);}
/* Keep stack/address instrumentation without unavailable leak-process inspection. */
int __lsan_is_turned_off(void){return 1;}
#define OP(name) void name(void)
IMPLEMENTATION
/* Classify a binary64 NaN without causing floating exceptions. */
static int realnan(uchar *bytes)
{
	uvlong bits;
	memmove(&bits,bytes,8);
	return (bits&0x7ff0000000000000ULL)==0x7ff0000000000000ULL && (bits&0xfffffffffffffULL)!=0;
}
/* Compare actual C/native squaring, sticky flags and all overlapping byte spans. */
int main(void)
{
	union { uvlong align; uchar bytes[160]; } memory;
	uchar before[160],expected[160];
	uvlong values[]={0,0x8000000000000000ULL,1,0x8000000000000001ULL,
		0x3ff0000000000000ULL,0xbff0000000000000ULL,0x4000000000000000ULL,0xc000000000000000ULL,
		0x3ff0000000000001ULL,0xbff0000000000001ULL,0x3fe0000000000000ULL,0xbfe0000000000000ULL,
		0x0010000000000000ULL,0x7fefffffffffffffULL,0x7ff0000000000000ULL,0xfff0000000000000ULL,
		0x7ff8000000000042ULL,0xfff8000000000042ULL,0x7ff0000000000042ULL,0xfff0000000000042ULL};
	WORD exponents[]={0,1,-1,2,-2,3,-3,7,31,32,63,64,127,-127,
		(WORD)((UWORD)1<<(sizeof(WORD)*8-1)),(WORD)(((UWORD)1<<(sizeof(WORD)*8-1))-1)};
	int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	Module module={0};Inst code={0};REG savedreg;
	fenv_t saved;
	int i,n,sa,ma,alias,mode,seed,flags;
	unsigned long checks=0;

	code.op=IEXPF;module.prog=&code;module.nprog=1;
	assert(compile(&module,1,nil) && module.jit->entry[IEXPF]);assert(fegetenv(&saved)==0);
	for(i=0;i<nelem(values);i++)for(n=0;n<nelem(exponents);n++)
	for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(alias=0;alias<6;alias++)
	for(mode=0;mode<4;mode++)for(seed=0;seed<2;seed++){
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		R.s=memory.bytes+16+sa;R.m=memory.bytes+48+ma;
		R.d=alias==0 ? memory.bytes+96 : alias==1 ? R.s : alias==2 ? R.m :
			alias==3 ? (uchar*)R.s+1 : alias==4 ? (uchar*)R.m-1 : (uchar*)R.m+1;
		R.PC=&code;savedreg=R;
		memmove(R.s,&exponents[n],sizeof(WORD));memmove(R.m,&values[i],8);
		memmove(before,memory.bytes,sizeof(before));
		assert(fesetround(modes[mode])==0);feclearexcept(FE_ALL_EXCEPT);if(seed)feraiseexcept(FE_DIVBYZERO);
		iexpf();flags=fetestexcept(FE_ALL_EXCEPT);assert(!memcmp(&R,&savedreg,sizeof(R)));
		memmove(expected,memory.bytes,sizeof(expected));memmove(memory.bytes,before,sizeof(before));
		feclearexcept(FE_ALL_EXCEPT);if(seed)feraiseexcept(FE_DIVBYZERO);
		assert(module.jit->entry[IEXPF](&R)==1);
		if(realnan(expected+((uchar*)R.d-memory.bytes))){
			assert(realnan(R.d));memmove(expected+((uchar*)R.d-memory.bytes),R.d,8);
		}
		assert(!memcmp(expected,memory.bytes,sizeof(expected)) && !memcmp(&R,&savedreg,sizeof(R)));
		assert(fegetround()==modes[mode] && fetestexcept(FE_ALL_EXCEPT)==flags);checks++;
	}
	assert(fesetenv(&saved)==0);module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS native real power: %lu FP-mode/flags/alignment/alias cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
