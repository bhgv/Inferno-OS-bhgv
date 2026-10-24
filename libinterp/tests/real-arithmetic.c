#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>
#include <fenv.h>

REG R;
#define OP(name) void name(void)
#define F(r) (*(REAL*)R.r)
#define SR(r) (*(SREAL*)R.r)
#define W(r) (*(WORD*)R.r)

/* Retain stack/address instrumentation without unsupported leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Use volatile scalar inputs so every oracle observes the active FP environment. */
static void
realoracle(int op, uvlong a, uvlong b, uchar *output)
{
	REAL x,y,result;
	SREAL single,small;
	WORD integer;
	volatile REAL left,right;
	volatile SREAL shortreal;
	volatile WORD fixed;

	memmove(&x,&a,sizeof(x));memmove(&y,&b,sizeof(y));
	left=x;right=y;
	if(op==5){memmove(&single,&a,sizeof(single));shortreal=single;result=shortreal;}
	else if(op==6){small=left;memmove(output,&small,sizeof(small));return;}
	else if(op==7){memmove(&integer,&a,sizeof(integer));fixed=integer;result=(REAL)fixed*right;}
	else switch(op){
	case 0:result=left+right;break;
	case 1:result=right-left;break;
	case 2:result=left*right;break;
	case 3:result=right/left;break;
	default:result=-left;break;
	}
	memmove(output,&result,sizeof(result));
}

/* Classify NaNs without raising flags or quieting signaling payloads. */
static int
realnan(uchar *value, int width)
{
	uvlong bits=0;
	memmove(&bits,value,width);
	if(width==4)return (bits&0x7f800000ULL)==0x7f800000ULL && (bits&0x7fffffULL)!=0;
	return (bits&0x7ff0000000000000ULL)==0x7ff0000000000000ULL && (bits&0xfffffffffffffULL)!=0;
}

/* Native resizing accepts only signed zero and exact normal-to-normal conversions. */
static int
nativeresize(uvlong bits, int input)
{
	uvlong magnitude,exponent;

	magnitude=bits&(input==4 ? 0x7fffffffULL : 0x7fffffffffffffffULL);
	exponent=magnitude>>(input==4 ? 23 : 52);
	return magnitude==0 || (input==4 ? exponent>0 && exponent<255 :
		exponent>=897 && exponent<=1150 && (magnitude&0x1fffffffULL)==0);
}

/* Compare FP effects and all byte placements; NaN arithmetic payload choice is host-defined. */
int
main(void)
{
	void (*ops[])(void)={addf,subf,mulf,divf,negf,cvtrf,cvtfr,cvtxf};
	void (*volatile reference)(int,uvlong,uvlong,uchar*)=realoracle;
	uvlong values[]={0,0x8000000000000000ULL,0x3ff0000000000000ULL,0xbff0000000000000ULL,
		0x0010000000000000ULL,0x8010000000000000ULL,0x7fefffffffffffffULL,0xffefffffffffffffULL,
		0x7ff0000000000000ULL,0xfff0000000000000ULL,1,0x8000000000000001ULL,
		0x7ff8000000000042ULL,0xfff8000000000042ULL,0x7ff0000000000042ULL,0xfff0000000000042ULL,
		0x3810000000000000ULL,0x47efffffe0000000ULL};
	uvlong singles[]={0,0x80000000,1,0x80000001,0x7fffff,0x800000,0x80800000,
		0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,
		0x7fc00042,0xffc00042,0x7f800042,0xff800042,0x3f000001};
	int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	union { uvlong align; uchar bytes[144]; } memory;
	uchar wanted[8],initial[144],expected[144];
	uvlong a,b;
	fenv_t saved;
	Inst sentinel;
	int op,i,j,mode,seed,s,m,d,p,k,insize,outsize,middle,expectedflags,initialflags,take,nan;
	unsigned long checks=0;
#ifdef NATIVE_REAL_ARITH
	Module module;
	Inst code[8];
	int ids[]={IADDF,ISUBF,IMULF,IDIVF,INEGF,ICVTRF,ICVTFR,ICVTXF};

	memset(&module,0,sizeof(module));memset(code,0,sizeof(code));
	for(k=0;k<8;k++)code[k].op=ids[k];
	module.prog=code;module.nprog=8;assert(compile(&module,8,nil));
#endif
	assert(fegetenv(&saved)==0);
	for(op=0;op<nelem(ops);op++){
		insize=op==5 ? 4 : op==7 ? sizeof(WORD) : 8;
		outsize=op==6 ? 4 : 8;
		middle=op<4 || op==7;
		for(mode=0;mode<nelem(modes);mode++)
		for(seed=0;seed<2;seed++)
		for(i=0;i<nelem(values);i++)
		for(j=0;j<(middle ? nelem(values) : 1);j++){
			a=op==5 ? singles[i] : values[i];b=values[j];
			assert(fesetround(modes[mode])==0);
			feclearexcept(FE_ALL_EXCEPT);feraiseexcept(seed ? FE_ALL_EXCEPT : FE_DIVBYZERO);
			initialflags=fetestexcept(FE_ALL_EXCEPT);
			reference(op,a,b,wanted);expectedflags=fetestexcept(FE_ALL_EXCEPT);
			nan=realnan(wanted,outsize);
			take=op==4 || ((op==5 || op==6) && nativeresize(a,insize));
			for(s=16;s<24;s++)for(m=48;m<56;m++)for(p=0;p<12;p++){
				d=p<8 ? 96+p : p==8 ? s : p==9 ? m : p==10 ? s+1 : m-1;
				for(k=0;k<sizeof(memory.bytes);k++)memory.bytes[k]=(uchar)(k*37+11);
				memmove(memory.bytes+s,&a,insize);memmove(memory.bytes+m,&b,8);
				memmove(initial,memory.bytes,sizeof(initial));memmove(expected,initial,sizeof(expected));
				memmove(expected+d,wanted,outsize);
				R.s=memory.bytes+s;R.m=middle ? memory.bytes+m : (void*)1;R.d=memory.bytes+d;R.PC=&sentinel;
				feclearexcept(FE_ALL_EXCEPT);feraiseexcept(seed ? FE_ALL_EXCEPT : FE_DIVBYZERO);
#ifdef NATIVE_REAL_ARITH
				if(op==4 || op==5 || op==6){
					assert(module.jit->entry[ids[op]](&R)==take);
					assert(!memcmp(memory.bytes,take ? expected : initial,sizeof(expected)));
					assert(fetestexcept(FE_ALL_EXCEPT)==initialflags && fegetround()==modes[mode]);
					assert(R.PC==&sentinel && R.s==memory.bytes+s && R.d==memory.bytes+d);
					memmove(memory.bytes,initial,sizeof(initial));
				}else assert(!module.jit->entry[ids[op]]);
#endif
				ops[op]();
				if(nan){assert(realnan(memory.bytes+d,outsize));memmove(expected+d,memory.bytes+d,outsize);}
				if(memcmp(memory.bytes,expected,sizeof(expected))){
					uvlong actual=0,oracle=0;
					memmove(&actual,memory.bytes+d,outsize);memmove(&oracle,wanted,outsize);
					printf("MISMATCH op=%d i=%d j=%d mode=%d seed=%d s=%d m=%d d=%d got=%llx wanted=%llx\n",op,i,j,mode,seed,s,m,d,(unsigned long long)actual,(unsigned long long)oracle);fflush(stdout);
				}
				assert(!memcmp(memory.bytes,expected,sizeof(expected)));
				assert(fetestexcept(FE_ALL_EXCEPT)==expectedflags && fegetround()==modes[mode]);
				assert(R.PC==&sentinel && R.s==memory.bytes+s && R.d==memory.bytes+d && R.m==(middle ? memory.bytes+m : (void*)1));
				checks++;
			}
		}
	}
	assert(fesetenv(&saved)==0);
#ifdef NATIVE_REAL_ARITH
	dropmodule(&module);
#endif
	printf("PASS real arithmetic: %lu alignment/overlap/FP-state checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
