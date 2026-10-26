#include <lib9.h>
#include "isa.h"
#include "interp.h"
#include <setjmp.h>
#include <stdio.h>
#include <fenv.h>

#define OP(name) static void name(void) // Bind actual production handlers locally.
REG R;
static jmp_buf boundary; // Owned error boundary for one handler invocation.
static char *raised; // Exact borrowed VM error identity.
char exBounds[]="bounds", exZdiv[]="zero";


/* Preserve caller snapshots while delivering the production VM error identity. */
void error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}


/* Keep address/undefined checks active without unsupported leak-process scanning. */
int __lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION


/* Keep longjmp local so no caller loop variable becomes indeterminate. */
static char*
invoke(void (*operation)(void))
{
	raised=nil;
	if(setjmp(boundary)==0)
		operation();
	return raised;
}


/* Check error priority and complete byte/register/FP preservation on failure. */
static void
failure(void (*operation)(void), char *expected, uchar *image)
{
	uchar before[512];
	REG saved;
	char *actual;
	int mode, flags;

	memmove(before,image,sizeof(before)); memmove(&saved,&R,sizeof(saved));
	mode=fegetround(); flags=fetestexcept(FE_ALL_EXCEPT);
	actual=invoke(operation);
	if(actual!=expected || memcmp(before,image,sizeof(before)) || memcmp(&saved,&R,sizeof(R)) ||
	   fegetround()!=mode || fetestexcept(FE_ALL_EXCEPT)!=flags)
		abort();
}


/* Check 24 production handlers with native-pointer slots in both WORD models. */
int main(void)
{
	void (*ops[])(void)={addb,subb,mulb,andb,orb,xorb,addw,subw,mulw,andw,orw,xorw,
		addl,subl,mull,andl,orl,xorl,divb,modb,divw,modw,divl,modl};
	uchar image[512];
	uintptr address;
	uvlong one,zero;
	int op,width,side,fault,shift,checks,profile;
	fenv_t original;

	checks=0; one=1; zero=0;
	if(fegetenv(&original) || feclearexcept(FE_ALL_EXCEPT) || feraiseexcept(FE_DIVBYZERO)) abort();
	for(op=0;op<24;op++){
		if(getenv("INTEGER_SLOT_OP") && op!=atoi(getenv("INTEGER_SLOT_OP"))) continue;
		width=op<6 || op==18 || op==19 ? 1 : op<12 || op==20 || op==21 ? sizeof(WORD) : 8;
		if(!getenv("INTEGER_SLOT_ZERO_ONLY")){
			for(side=0;side<3;side++) for(fault=0;fault<=width;fault++) for(shift=0;shift<8;shift++){
				memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
				R.s=image+64+shift; R.m=image+160+shift; R.d=image+256+shift;
				memmove(R.s,&one,width); memmove(R.m,&one,width);
				address=fault==0 ? 0 : (uintptr)-1-(fault-1);
				if(side==0) R.s=(void*)address;
				if(side==1) R.m=(void*)address;
				if(side==2) R.d=(void*)address;
				failure(ops[op],exBounds,image); checks++;
			}
			/* Invalid results must precede unused, unmapped input reads. */
			for(shift=0;shift<8;shift++){
				memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
				R.s=image+64+shift; memmove(R.s,&one,width);
				R.m=(void*)1; R.d=H;
				failure(ops[op],exBounds,image); checks++;
			}
		}
		if(op<18) continue;
		/* Zero must win even if neither dividend nor output address can be used. */
		for(shift=0;shift<8;shift++) for(profile=0;profile<4;profile++){
			memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
			R.s=image+64+shift; memmove(R.s,&zero,width);
			R.m=profile==0 ? nil : profile==1 ? H : (void*)1;
			R.d=profile==2 ? nil : profile==3 ? H : (void*)1;
			failure(ops[op],exZdiv,image); checks++;
		}
	}
	if(fesetenv(&original)) abort();
	printf("PASS C integer slots/zero-priority/REG/image/FP: %d cases WORD%d\n",checks,(int)sizeof(WORD)*8);
	return 0;
}
