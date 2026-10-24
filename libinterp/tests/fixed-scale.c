#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>

REG R;
#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
char exInval[]="invalid";

/* Valid scale cases must never enter the VM error path. */
void
error(char *message)
{
	(void)message;
	assert(0);
}
/* Preserve stack checking without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Exercise fixed intermediates before truncation and all byte/overlap placements. */
int
main(void)
{
	uvlong patterns[]={0,1,127,128,255,0x7fffffff,0x80000000,0xffffffff,
		0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,~0ULL};
	union { uvlong align; uchar bytes[144]; } memory;
	uchar expected[144], initial[144];
	WORD value,count,result;
	LONG wide;
	Inst sentinel;
	int i,n,s,m,d,placement,k;
	unsigned long checks=0;
#ifdef NATIVE_FIXED
	Module module;
	Inst instruction;
	WORD invalid[]={64,-64,(WORD)((UWORD)1<<(sizeof(WORD)*8-1)),(WORD)(((UWORD)1<<(sizeof(WORD)*8-1))-1)};

	memset(&module,0,sizeof(module));memset(&instruction,0,sizeof(instruction));
	instruction.op=ICVTXX;module.prog=&instruction;module.nprog=1;
	assert(compile(&module,1,nil) && module.jit->entry[ICVTXX]);
#endif
	for(i=0;i<nelem(patterns);i++)
	for(n=-63;n<=63;n++)
	for(s=16;s<24;s++)
	for(m=48;m<56;m++)
	for(placement=0;placement<12;placement++){
		d=placement<8 ? 96+placement : placement==8 ? s : placement==9 ? m : placement==10 ? s+1 : m-1;
		value=(WORD)patterns[i];count=n;
		wide=(LONG)value;
		result=n>=0 ? (WORD)((uvlong)wide<<n) : (WORD)(wide>>-n);
		for(k=0;k<sizeof(memory.bytes);k++) memory.bytes[k]=(uchar)(k*37+11);
		memmove(memory.bytes+s,&value,sizeof(value));memmove(memory.bytes+m,&count,sizeof(count));
		memmove(initial,memory.bytes,sizeof(initial));memmove(expected,initial,sizeof(expected));
		memmove(expected+d,&result,sizeof(result));
		R.s=memory.bytes+s;R.m=memory.bytes+m;R.d=memory.bytes+d;R.PC=&sentinel;
#ifdef NATIVE_FIXED
		assert(module.jit->entry[ICVTXX](&R)==1);
		assert(!memcmp(memory.bytes,expected,sizeof(expected)));
		assert(R.PC==&sentinel && R.s==memory.bytes+s && R.m==memory.bytes+m && R.d==memory.bytes+d);
		memmove(memory.bytes,initial,sizeof(initial));
#endif
		cvtxx();
		assert(!memcmp(memory.bytes,expected,sizeof(expected)));
		assert(R.PC==&sentinel && R.s==memory.bytes+s && R.m==memory.bytes+m && R.d==memory.bytes+d);
		checks++;
	}
#ifdef NATIVE_FIXED
	/* Invalid scales must decline without publishing, then retain the C exception. */
	for(i=0;i<nelem(invalid);i++){
		value=0;count=invalid[i];
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		R.s=memory.bytes+16;R.m=memory.bytes+48;R.d=memory.bytes+96;
		memmove(R.s,&value,sizeof(value));memmove(R.m,&count,sizeof(count));
		memmove(initial,memory.bytes,sizeof(initial));R.PC=&sentinel;
		assert(module.jit->entry[ICVTXX](&R)==0 && !memcmp(initial,memory.bytes,sizeof(initial)));
		assert(!kprotect(fixedscaleerror,nil));
		assert(!memcmp(initial,memory.bytes,sizeof(initial)) && R.PC==&sentinel);
	}
	dropmodule(&module);
#endif
	printf("PASS fixed scale: %lu alignment/overlap checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
