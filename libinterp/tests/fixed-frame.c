#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>

REG R;
#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define Dtmp (*(WORD*)(R.FP+(NREG+2)*IBY2WD))
char exInval[]="invalid",exZdiv[]="zero";

/* Valid fixed-frame cases must not raise VM exceptions. */
void
error(char *message)
{
	(void)message;assert(0);
}
/* Keep stack/address checking without unsupported leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Compare frame-byte snapshots before every aliased publication or native fallback. */
int
main(void)
{
	void (*ops[])(void)={mulx,divx};
	WORD values[]={0,1,-1,127,-128,0x7fffffff,(WORD)0x80000000ULL,
		(WORD)0x7fffffffffffffffULL,(WORD)0x8000000000000000ULL};
	WORD counts[]={-63,-33,-32,-1,0,1,31,32,62,63};
	union {uvlong align;uchar bytes[256];} memory;
	uchar initial[256],expected[256];
	WORD x,y,n,result;
	LONG intermediate;
	uvlong magnitude;
	Inst sentinel;
	REG saved=R;
	int op,i,j,c,s,m,f,p,d,k,completed;
	unsigned long checks=0;
#ifdef NATIVE_FIXED_FRAME
	Module module;
	Inst code[2];
	int ids[]={IMULX,IDIVX};

	memset(&module,0,sizeof(module));memset(code,0,sizeof(code));
	for(k=0;k<2;k++)code[k].op=ids[k];
	module.prog=code;module.nprog=2;assert(compile(&module,2,nil));
#endif
	for(op=0;op<2;op++)for(i=0;i<nelem(values);i++)for(j=0;j<nelem(values);j++)for(c=0;c<nelem(counts);c++){
		x=values[i];y=values[j];n=counts[c];
		if(op==1 && y==0)continue;
		intermediate=op==0 ? (LONG)((uvlong)(LONG)x*(uvlong)(LONG)y) : (LONG)x;
		intermediate=n>=0 ? (LONG)((uvlong)intermediate<<n) : intermediate>>-n;
		if(op==1){
			magnitude=intermediate<0 ? (uvlong)0-(uvlong)intermediate : (uvlong)intermediate;
			magnitude /= y<0 ? (uvlong)0-(uvlong)(LONG)y : (uvlong)y;
			intermediate=(LONG)(((intermediate<0)!=(y<0)) ? (uvlong)0-magnitude : magnitude);
		}
		result=(WORD)intermediate;
		for(s=0;s<8;s++)for(m=0;m<8;m++)for(f=0;f<8;f++)for(p=0;p<12;p++){
			for(k=0;k<sizeof(memory.bytes);k++)memory.bytes[k]=(uchar)(k*37+11);
			R.s=memory.bytes+16+s;R.m=memory.bytes+48+m;R.FP=memory.bytes+128+f;
			d=p<6 ? 88+p : p==6 ? 16+s : p==7 ? 48+m : p==8 ? 17+s : p==9 ? 47+m : p==10 ? 128+f+(NREG+2)*IBY2WD : 129+f+(NREG+2)*IBY2WD;
			R.d=memory.bytes+d;R.PC=&sentinel;
			memmove(R.s,&y,sizeof(y));memmove(R.m,&x,sizeof(x));memmove(R.FP+(NREG+2)*IBY2WD,&n,sizeof(n));
			memmove(initial,memory.bytes,sizeof(initial));memmove(expected,initial,sizeof(expected));
			memmove(expected+d,&result,sizeof(result));
#ifdef NATIVE_FIXED_FRAME
			completed=op==0 || y!=-1;
			assert(module.jit->entry[ids[op]](&R)==completed);
			assert(!memcmp(memory.bytes,completed ? expected : initial,sizeof(expected)));
			assert(R.PC==&sentinel && R.s==memory.bytes+16+s && R.m==memory.bytes+48+m && R.FP==memory.bytes+128+f && R.d==memory.bytes+d);
			memmove(memory.bytes,initial,sizeof(initial));
#endif
			ops[op]();
			assert(!memcmp(memory.bytes,expected,sizeof(expected)));
			assert(R.PC==&sentinel && R.s==memory.bytes+16+s && R.m==memory.bytes+48+m && R.FP==memory.bytes+128+f && R.d==memory.bytes+d);
			checks++;
		}
	}
#ifdef NATIVE_FIXED_FRAME
	dropmodule(&module);
#endif
	R=saved;
	printf("PASS fixed frame: %lu alignment/alias/temporary checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
