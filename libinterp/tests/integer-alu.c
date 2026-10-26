#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <stdio.h>

REG R;
#define OP(name) void name(void)
#define B(r) (*(BYTE*)R.r)
#define W(r) (*(WORD*)R.r)
#define UW(r) (*(UWORD*)R.r)
#define V(r) (*(LONG*)R.r)
#define UV(r) (*(ULONG*)R.r)


/* Preserve address instrumentation without unavailable process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

char exBounds[]="bounds";
#define integercheck(condition) integerpredicate(!!(condition)) // Keep modular/alias oracles active under NDEBUG.


/* Fail explicit predicates even when the compiler disables assertions. */
static void
integerpredicate(int condition)
{
	if(!condition)
		abort();
}


/* This positive arithmetic matrix supplies valid slots and expects no VM errors. */
void error(char *message)
{
	(void)message;
	abort();
}


IMPLEMENTATION


/* Compare modular integer arithmetic against byte snapshots before aliased writes. */
int
main(void)
{
	void (*ops[])(void) = {addb,subb,mulb,andb,orb,xorb,addw,subw,mulw,andw,orw,xorw,
		addl,subl,mull,andl,orl,xorl};
	uvlong patterns[] = {0,1,127,128,255,256,0x7fffffff,0x80000000,0xffffffff,
		0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,~0ULL};
	union { uvlong align; uchar bytes[160]; } memory;
	uchar expected[160];
	uvlong left,right,result,mask;
	Inst sentinel;
	int op,i,j,s,m,d,k,width,placement;
	unsigned long checks=0;

	for(op=0;op<nelem(ops);op++)
	for(i=0;i<nelem(patterns);i++)
	for(j=0;j<nelem(patterns);j++)
	for(s=24;s<32;s++)
	for(m=64;m<72;m++)
	for(placement=0;placement<12;placement++){
		d=placement<8 ? 104+placement : placement==8 ? s : placement==9 ? m : placement==10 ? s+1 : m-1;
		width=op<6 ? 1 : op<12 ? sizeof(WORD) : 8;
		for(k=0;k<sizeof(memory.bytes);k++) memory.bytes[k]=(uchar)(k*37+11);
		memmove(memory.bytes+s,&patterns[i],8);
		memmove(memory.bytes+m,&patterns[j],8);
		memmove(expected,memory.bytes,sizeof(expected));
		left=right=0;
		memmove(&left,expected+m,width); memmove(&right,expected+s,width);
		mask=width==8 ? ~(uvlong)0 : ((uvlong)1<<(width*8))-1;
		switch(op%6){
		case 0: result=left+right; break;
		case 1: result=left-right; break;
		case 2: result=left*right; break;
		case 3: result=left&right; break;
		case 4: result=left|right; break;
		default: result=left^right; break;
		}
		result &= mask;
		memmove(expected+d,&result,width);
		R.s=memory.bytes+s;R.m=memory.bytes+m;R.d=memory.bytes+d;R.PC=&sentinel;
		ops[op]();
		integercheck(!memcmp(expected,memory.bytes,sizeof(expected)));
		integercheck(R.PC==&sentinel && R.s==memory.bytes+s && R.m==memory.bytes+m && R.d==memory.bytes+d);
		checks++;
	}
	printf("PASS integer ALU: %lu boundary/alignment/alias checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
