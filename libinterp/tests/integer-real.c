#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <stdio.h>
#include <fenv.h>

REG R;
#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define V(r) (*(LONG*)R.r)
#define F(r) (*(REAL*)R.r)


/* Keep address and stack checking without unsupported leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

char exBounds[]="bounds";
#define transfercheck(condition) transferpredicate(!!(condition)) // Keep every oracle active under NDEBUG.


/* Fail fixture invariants explicitly so optimization cannot remove the oracles. */
static void
transferpredicate(int condition)
{
	if(!condition)
		abort();
}


/* These matrices supply valid spans; an unexpected VM error fails the case. */
void error(char *message)
{
	(void)message;
	abort();
}


IMPLEMENTATION


/* Force the host WORD conversion to observe the current rounding environment. */
static REAL
wordrealoracle(WORD value)
{
	volatile WORD input=value;
	volatile REAL output=input;
	return output;
}


/* Keep the LONG oracle separate so WORD32 does not narrow its input. */
static REAL
longrealoracle(LONG value)
{
	volatile LONG input=value;
	volatile REAL output=input;
	return output;
}


/* Compare exact bits, exception flags and byte effects in all four rounding modes. */
int
main(void)
{
	uvlong patterns[512]={0,1,~(uvlong)0,0x7fffffffffffffffULL,0x8000000000000000ULL,
		0x1fffffffffffffULL,0x20000000000000ULL,0x20000000000001ULL};
	void (*ops[])(void)={cvtwf,cvtlf};
	REAL (*volatile wordoracle)(WORD)=wordrealoracle;
	REAL (*volatile longoracle)(LONG)=longrealoracle;
	int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	union { uvlong align; uchar bytes[128]; } memory;
	uchar initial[128],expected[128];
	uvlong state=0x51ba936d77ULL,mag,raw;
	WORD word;
	LONG value;
	REAL wanted;
	fenv_t saved;
	Inst sentinel;
	int op,i,e,delta,np=8,mode,seed,s,d,k,initialflags,expectedflags,exact;
	unsigned long checks=0;
#ifdef NATIVE_INTEGER_REAL
	Module module;
	Inst code[2];
	int ids[]={ICVTWF,ICVTLF};

	memset(&module,0,sizeof(module));memset(code,0,sizeof(code));
	for(k=0;k<2;k++)code[k].op=ids[k];
	module.prog=code;module.nprog=2;transfercheck(compile(&module,2,nil));
#endif
	transfercheck(fegetenv(&saved)==0);
	for(e=0;e<63;e++)for(delta=-1;delta<=1;delta++){
		raw=((uvlong)1<<e)+delta;
		patterns[np++]=raw;patterns[np++]=(uvlong)0-raw;
	}
	for(i=0;i<64;i++){
		state^=state<<13;state^=state>>7;state^=state<<17;
		patterns[np++]=state;
	}
	transfercheck(np<=nelem(patterns));
	for(op=0;op<2;op++)
	for(mode=0;mode<nelem(modes);mode++)
	for(seed=0;seed<2;seed++)
	for(i=0;i<np;i++){
		memmove(&value,&patterns[i],sizeof(value));
		word=(WORD)value;
		if(op==0)value=word;
		mag=value<0 ? (uvlong)0-(uvlong)value : (uvlong)value;
		exact=mag<((uvlong)1<<53);
		transfercheck(fesetround(modes[mode])==0);
		feclearexcept(FE_ALL_EXCEPT);feraiseexcept(seed ? FE_ALL_EXCEPT : FE_DIVBYZERO);
		initialflags=fetestexcept(FE_ALL_EXCEPT);
		wanted=op==0 ? wordoracle(word) : longoracle(value);
		expectedflags=fetestexcept(FE_ALL_EXCEPT);
		if(exact)transfercheck(expectedflags==initialflags);
		for(s=24;s<32;s++)for(d=16;d<48;d++){
			for(k=0;k<sizeof(memory.bytes);k++)memory.bytes[k]=(uchar)(k*37+11);
			if(op==0)memmove(memory.bytes+s,&word,sizeof(word));
			else memmove(memory.bytes+s,&value,sizeof(value));
			memmove(initial,memory.bytes,sizeof(initial));memmove(expected,initial,sizeof(expected));
			memmove(expected+d,&wanted,sizeof(wanted));
			R.s=memory.bytes+s;R.d=memory.bytes+d;R.m=(void*)1;R.PC=&sentinel;
			feclearexcept(FE_ALL_EXCEPT);feraiseexcept(seed ? FE_ALL_EXCEPT : FE_DIVBYZERO);
#ifdef NATIVE_INTEGER_REAL
			transfercheck(module.jit->entry[ids[op]](&R)==1);
			transfercheck(!memcmp(memory.bytes,expected,sizeof(expected)));
			transfercheck(fetestexcept(FE_ALL_EXCEPT)==expectedflags && fegetround()==modes[mode]);
			transfercheck(R.PC==&sentinel && R.s==memory.bytes+s && R.d==memory.bytes+d && R.m==(void*)1);
			memmove(memory.bytes,initial,sizeof(initial));
#endif
			ops[op]();
			transfercheck(!memcmp(memory.bytes,expected,sizeof(expected)));
			transfercheck(fetestexcept(FE_ALL_EXCEPT)==expectedflags && fegetround()==modes[mode]);
			transfercheck(R.PC==&sentinel && R.s==memory.bytes+s && R.d==memory.bytes+d && R.m==(void*)1);
			checks++;
		}
	}
	transfercheck(fesetenv(&saved)==0);
#ifdef NATIVE_INTEGER_REAL
	dropmodule(&module);
#endif
	printf("PASS integer real: %lu alignment/overlap/FP-state checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
