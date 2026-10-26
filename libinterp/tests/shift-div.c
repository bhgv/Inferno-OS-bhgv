#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>

#ifndef SHIFT_DIV_FIRST
#define SHIFT_DIV_FIRST 0
#endif
REG R;
#define OP(name) void name(void)
#define B(r) (*(BYTE*)R.r)
#define W(r) (*(WORD*)R.r)
#define UW(r) (*(UWORD*)R.r)
#define V(r) (*(LONG*)R.r)
#define UV(r) (*(ULONG*)R.r)
char exBounds[] = "bounds"; /* Decoded shift-slot exception identity. */
char exInval[]="invalid", exZdiv[]="zero";
static jmp_buf trap;
static int armed;
static char *raised;

/* Isolate each failing operation in its own process and exception boundary. */
void
error(char *message)
{
	assert(armed);raised=message;longjmp(trap,1);
}
/* Keep address and stack instrumentation without unsupported leak inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Test every operand alignment and both directions of destination overlap. */
int
main(int argc, char **argv)
{
	void (*ops[])(void)={shlb,shrb,shlw,shrw,lsrw,shll,shrl,lsrl,divb,modb,divw,modw,divl,modl};
	uvlong patterns[]={0,1,127,128,255,256,0x7fffffff,0x80000000,0xffffffff,
		0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,~0ULL};
	union { uvlong align; uchar bytes[144]; } memory;
	uchar expected[144], initial[144];
	WORD count;
	WORD word;
	LONG value,divisor,result,minimum;
	uvlong bits,dbits,out,mask;
	Inst sentinel;
	int op,i,j,s,m,d,p,k,width,limit;
	unsigned long checks=0;

#ifndef NATIVE_SHIFT_DIV
	if(argc>1){
		op=atoi(argv[1]);assert(op>=0 && op<nelem(ops));
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		R.s=memory.bytes+17;R.m=memory.bytes+49;R.d=memory.bytes+97;R.PC=&sentinel;
		count=-1;
		if(op<8){memmove(R.s,&count,sizeof(count));R.m=(void*)1;}
		else{memset(R.s,0,8);if(op<10) R.m=(void*)1;}
		memmove(expected,memory.bytes,sizeof(expected));armed=1;
		if(setjmp(trap)==0){ops[op]();assert(0);}
		assert(raised==(op<8 ? exInval : exZdiv));
		assert(R.PC==&sentinel && !memcmp(expected,memory.bytes,sizeof(expected)));
		printf("PASS shift/div failure: op %d WORD%zu\n",op,sizeof(WORD)*8);
		return 0;
	}
#endif
#ifdef NATIVE_SHIFT_DIV
	Module module;
	Inst code[14];
	int ids[]={ISHLB,ISHRB,ISHLW,ISHRW,ILSRW,ISHLL,ISHRL,ILSRL,IDIVB,IMODB,IDIVW,IMODW,IDIVL,IMODL};
	int completed;

	memset(&module,0,sizeof(module));memset(code,0,sizeof(code));
	for(k=0;k<14;k++) code[k].op=ids[k];
	module.prog=code;module.nprog=14;assert(compile(&module,14,nil));
#endif
	for(op=SHIFT_DIV_FIRST;op<nelem(ops);op++){
		width=op<2 || (op>=8 && op<10) ? 1 : (op<5 || (op>=10 && op<12)) ? sizeof(WORD) : 8;
		mask=width==8 ? ~(uvlong)0 : ((uvlong)1<<(width*8))-1;
		minimum=(LONG)((uvlong)1<<(width*8-1));
		if(width<8)minimum=-(LONG)((uvlong)1<<(width*8-1));
		limit=op<8 ? width*8 : nelem(patterns);
		for(i=0;i<nelem(patterns);i++)
		for(j=0;j<limit;j++)
		for(s=16;s<24;s++)
		for(m=48;m<56;m++)
		for(p=0;p<12;p++){
			d=p<8 ? 96+p : p==8 ? s : p==9 ? m : p==10 ? s+1 : m-1;
			bits=patterns[i]&mask;dbits=op<8 ? 0 : patterns[j]&mask;
			if(op>=8 && dbits==0)continue;
			value=(LONG)bits;divisor=(LONG)dbits;
			if(width==sizeof(WORD) && width!=1){word=(WORD)bits;value=word;word=(WORD)dbits;divisor=word;}
			if(op<8){
				count=j;
				if(op==0 || op==2 || op==5)out=bits<<j;
				else if(op==3 || op==6)out=(uvlong)(value>>j);
				else out=bits>>j;
			}else{
				if(width>1 && value==minimum && divisor==-1)result=(op%2)==0 ? minimum : 0;
				else result=(op%2)==0 ? value/divisor : value%divisor;
				out=(uvlong)result;
			}
			out &= mask;
			for(k=0;k<sizeof(memory.bytes);k++)memory.bytes[k]=(uchar)(k*37+11);
			memmove(memory.bytes+m,&bits,width);
			if(op<8)memmove(memory.bytes+s,&count,sizeof(count));
			else memmove(memory.bytes+s,&dbits,width);
			memmove(initial,memory.bytes,sizeof(initial));memmove(expected,initial,sizeof(expected));memmove(expected+d,&out,width);
			R.s=memory.bytes+s;R.m=memory.bytes+m;R.d=memory.bytes+d;R.PC=&sentinel;
#ifdef NATIVE_SHIFT_DIV
			completed=1;
			assert(module.jit->entry[ids[op]](&R)==completed);
			if(completed)assert(!memcmp(memory.bytes,expected,sizeof(expected)));
			else assert(!memcmp(memory.bytes,initial,sizeof(initial)));
			assert(R.PC==&sentinel && R.s==memory.bytes+s && R.m==memory.bytes+m && R.d==memory.bytes+d);
			for(k=0;k<sizeof(memory.bytes);k++)memory.bytes[k]=(uchar)(k*37+11);
			memmove(memory.bytes+m,&bits,width);
			if(op<8)memmove(memory.bytes+s,&count,sizeof(count));else memmove(memory.bytes+s,&dbits,width);
#endif
			ops[op]();
			assert(!memcmp(expected,memory.bytes,sizeof(expected)));
			assert(R.PC==&sentinel && R.s==memory.bytes+s && R.m==memory.bytes+m && R.d==memory.bytes+d);
			checks++;
		}
	}
#ifdef NATIVE_SHIFT_DIV
	dropmodule(&module);
#endif
	printf("PASS shift/div: %lu boundary/alignment/alias checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
