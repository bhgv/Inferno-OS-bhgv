#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#include <assert.h>
#include <fenv.h>
#include <stdio.h>

REG R;
/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;

/* Allocate ordinary module metadata without starting a VM process. */
void*
mallocz(ulong size, int clear)
{
	void *p;

	p=malloc(size);
	if(p && clear)
		memset(p,0,size);
	return p;
}

/* Execute the production compiler's protected callback for valid modules. */
int
kprotect(int (*callback)(void*), void *argument)
{
	return callback(argument);
}

/* A real comparison must never raise a VM exception. */
void
error(char *message)
{
	USED(message);
	assert(0);
}

/* Preserve address/stack instrumentation without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define OP(name) void name(void)
IMPLEMENTATION

/* Inspect the full hardware status, including the x86 denormal operand sticky bit. */
static unsigned int
readmxcsr(void)
{
	unsigned int value;

	__asm__ volatile("stmxcsr %0" : "=m"(value));
	return value;
}

/* Install only supported DAZ/FTZ controls, standard rounding and masked exceptions. */
static void
writemxcsr(unsigned int value)
{
	__asm__ volatile("ldmxcsr %0" : : "m"(value));
}

/* Classify NaNs by bits before any host FP operation can quiet a signaling NaN. */
static int
isnanbits(uvlong bits)
{
	return (bits&0x7ff0000000000000ULL)==0x7ff0000000000000ULL &&
		(bits&0xfffffffffffffULL)!=0;
}

/* Compare actual C/native predicates, PC effects, byte guards and complete MXCSR state. */
int
main(void)
{
	uvlong values[]={0,0x8000000000000000ULL,1,0x8000000000000001ULL,
		0xfffffffffffffULL,0x800fffffffffffffULL,0x0010000000000000ULL,0x8010000000000000ULL,
		0x3ff0000000000000ULL,0xbff0000000000000ULL,0x7fefffffffffffffULL,0xffefffffffffffffULL,
		0x7ff0000000000000ULL,0xfff0000000000000ULL,
		0x7ff8000000000042ULL,0xfff8000000000042ULL,0x7ff0000000000042ULL,0xfff0000000000042ULL};
	void (*operations[])(void)={beqf,bnef,bltf,blef,bgtf,bgef};
	int ids[]={IBEQF,IBNEF,IBLTF,IBLEF,IBGTF,IBGEF};
	union { uvlong align; uchar bytes[128]; } memory;
	uchar before[128];
	Module module={0};
	Inst code[6]={{0}},target;
	REG saved,expected;
	unsigned int original,initial,wanted;
	int op,i,j,align,mode,flush,seed,accepted,taken;
	unsigned long checks=0;

	for(op=0;op<6;op++)
		code[op].op=ids[op];
	module.prog=code;
	module.nprog=6;
	assert(compile(&module,6,nil));
	original=readmxcsr();
	for(op=0;op<6;op++)
	for(i=0;i<nelem(values);i++)for(j=0;j<nelem(values);j++)
	for(align=0;align<8;align++)
	for(mode=0;mode<4;mode++)for(flush=0;flush<4;flush++)for(seed=0;seed<2;seed++){
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		memset(&R,0,sizeof(R));
		R.s=memory.bytes+16+align;
		R.m=memory.bytes+48+(align*3)%8;
		R.d=memory.bytes+80+(align*5)%8;
		R.PC=code;
		memmove(R.s,&values[i],8);
		memmove(R.m,&values[j],8);
		{ Inst *pointer=&target; memmove(R.d,&pointer,sizeof(pointer)); }
		memmove(before,memory.bytes,sizeof(before));
		saved=R;
		initial=(original&~0xffffU)|0x1f80U|(mode<<13)|((flush&1)<<6)|((flush>>1)<<15)|(seed ? 0x24 : 0);
		writemxcsr(initial);
		operations[op]();
		wanted=readmxcsr();
		expected=R;
		taken=R.PC==&target;
		assert(!memcmp(before,memory.bytes,sizeof(before)));
		R=saved;
		accepted=!isnanbits(values[i]) && !isnanbits(values[j]);
		writemxcsr(initial);
		assert(module.jit->entry[ids[op]](&R)==accepted);
		assert(readmxcsr()==(accepted ? wanted : initial));
		assert(!memcmp(&R,accepted ? &expected : &saved,sizeof(R)));
		assert(!memcmp(before,memory.bytes,sizeof(before)));
		if(accepted && !taken){
			R.d=(void*)1;
			assert(module.jit->entry[ids[op]](&R)==1 && R.PC==code);
			assert(readmxcsr()==wanted && !memcmp(before,memory.bytes,sizeof(before)));
		}
		checks++;
	}
	writemxcsr(original);
	module.jit->freecode(module.jit->code,module.jit->size);
	free(module.jit);
	printf("PASS native real branches: %lu FP-controls/status/PC/alignment cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
