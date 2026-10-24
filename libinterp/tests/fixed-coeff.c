#ifdef NATIVE_COEFFICIENT
#include "comp-amd64.c"
#else
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
REG R;
char exInval[]="invalid",exZdiv[]="zero",exNilref[]="nil";
static jmp_buf boundary;
static char *raised;
/* Preserve VM error identity and all destination bytes. */
void error(char *text) {raised=text;longjmp(boundary,1);}
/* Keep native stack instrumentation without leak-process inspection. */
int __lsan_is_turned_off(void) {return 1;}
#ifdef NATIVE_COEFFICIENT
/* Compile only valid module descriptors with ordinary owned allocations. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* The valid compiler fixture executes its protected allocation callback directly. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
#endif
#define OP(name) void name(void)
IMPLEMENTATION

/* Independently compute modular intermediates and unsigned quotient magnitudes. */
int
main(void)
{
	union {uvlong align;uchar bytes[256];} memory;
	uchar before[256],expected[256];
	WORD values[]={0,1,-1,(WORD)((UWORD)1<<(sizeof(WORD)*8-1)),(WORD)(((UWORD)1<<(sizeof(WORD)*8-1))-1)};
	WORD coefficients[]={1,-1,2,-2};
	WORD counts[]={-63,-1,0,1,32,63};
	WORD x,y,n,a,result,source,middle,encoded;
	LONG intermediate;
	uvlong magnitude,denominator;
	void (*ops[])(void)={mulx0,divx0,cvtxx0,mulx1,divx1,cvtxx1};
	void (*frameops[])(void)={mulx,divx,mulx0,divx0,cvtxx0,mulx1,divx1,cvtxx1};
	Inst sentinel;REG saved;
	int op,kind,c,sa,ma,fa,alias,d,mode,accepted,identity;
	unsigned long checks=0;
#ifdef NATIVE_COEFFICIENT
	Module module={0};Inst code[6]={{0}};int ids[]={IMULX0,IDIVX0,ICVTXX0,IMULX1,IDIVX1,ICVTXX1};
	for(op=0;op<6;op++)code[op].op=ids[op];module.prog=code;module.nprog=6;
	assert(compile(&module,6,nil));for(op=0;op<6;op++)assert(module.jit->entry[ids[op]]);
#endif
	for(mode=0;mode<2;mode++)for(op=0;op<6;op++)for(c=0;c<96;c++){
#ifndef NATIVE_COEFFICIENT
		if(mode)continue;
#endif
		kind=op%3;
		x=values[c%5];y=values[(c/5)%5];n=counts[c%6];a=coefficients[(c/25)%4];
		if(kind==1 && y==0)continue;
		intermediate=kind==0 ? (LONG)((uvlong)(LONG)x*(uvlong)(LONG)y) : kind==1 ? (LONG)((uvlong)(LONG)a*(uvlong)(LONG)x) : (LONG)x;
		intermediate=n>=0 ? (LONG)((uvlong)intermediate<<n) : intermediate>>-n;
		denominator=(uvlong)(LONG)(kind==1 ? y : a);
		if((LONG)denominator<0)denominator=(uvlong)0-denominator;
		magnitude=intermediate<0 ? (uvlong)0-(uvlong)intermediate : (uvlong)intermediate;magnitude/=denominator;
		result=(WORD)(((intermediate<0)!=((kind==1 ? y : a)<0)) ? (uvlong)0-magnitude : magnitude);
		accepted=(kind==1 ? y : a)!=-1;
		encoded=op>=3 ? n*4 : n;
		source=kind==2 ? x : y;middle=kind==2 ? encoded : x;
		for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(fa=0;fa<8;fa++)for(alias=0;alias<8;alias++){
			memset(memory.bytes,0xa5,sizeof(memory.bytes));R.s=memory.bytes+16+sa;R.m=memory.bytes+48+ma;R.FP=memory.bytes+128+fa;
			d=alias==0 ? 88 : alias==1 ? 16+sa : alias==2 ? 48+ma : alias==3 ? 17+sa : alias==4 ? 47+ma : alias==5 ? 128+fa+NREG*IBY2WD : alias==6 ? 128+fa+(NREG+2)*IBY2WD : 129+fa+NREG*IBY2WD;
			R.d=memory.bytes+d;R.PC=&sentinel;
			memmove(R.s,&source,sizeof(source));memmove(R.m,&middle,sizeof(middle));memmove(R.FP+NREG*IBY2WD,&a,sizeof(a));memmove(R.FP+(NREG+2)*IBY2WD,&encoded,sizeof(encoded));
			memmove(before,memory.bytes,sizeof(before));memmove(expected,before,sizeof(expected));memmove(expected+d,&result,sizeof(result));saved=R;
#ifdef NATIVE_COEFFICIENT
			if(mode){assert(module.jit->entry[ids[op]](&R)==accepted);assert(!memcmp(memory.bytes,accepted ? expected : before,sizeof(before)));memmove(memory.bytes,before,sizeof(before));}
#endif
			ops[op]();assert(!memcmp(memory.bytes,expected,sizeof(expected)) && !memcmp(&R,&saved,sizeof(R)));checks++;
		}
	}
	for(identity=0;identity<2;identity++)for(op=0;op<8;op++){
		x=y=n=1;result=77;R.s=&y;R.m=&x;R.d=&result;R.FP=identity ? H : nil;saved=R;raised=nil;
		if(setjmp(boundary)==0){frameops[op]();assert(0);}
		assert(raised==exNilref && result==77 && !memcmp(&R,&saved,sizeof(R)));checks++;
	}
#ifdef NATIVE_COEFFICIENT
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
#endif
	printf("PASS fixed coefficients: %lu frame/operand/alias/native-fallback checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
