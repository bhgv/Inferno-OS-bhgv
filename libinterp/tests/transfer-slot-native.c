#include <lib9.h>
#include "isa.h"
#include "interp.h"
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <fenv.h>

#define OP(name) static void name(void) // Bind extracted production handlers locally.
REG R;
static jmp_buf boundary; // Owned VM error boundary for one handler call.
static char *raised; // Exact error identity raised by the extracted handler.
char exBounds[]="bounds", exInval[]="invalid";


/* Capture VM errors without unwinding past the caller's loop and snapshots. */
void error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}


/* Stack checks remain enabled; this fixture needs no process-based leak scan. */
int __lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION


/* Keep longjmp within a dedicated frame so caller state stays defined. */
static char*
invoke(void (*operation)(void))
{
	raised=nil;
	if(setjmp(boundary)==0)
		operation();
	return raised;
}


/* Check memory, decoded registers, exact error identity and the full FP contract. */
static void
verify(void (*operation)(void), char *expectederror, uchar *image, uchar *expected,
	int mode, int flags)
{
	REG saved;
	char *actual;

	memmove(&saved,&R,sizeof(R));
	actual=invoke(operation);
	if(actual!=expectederror || memcmp(image,expected,512) || memcmp(&R,&saved,sizeof(R)) ||
	   fegetround()!=mode || fetestexcept(FE_ALL_EXCEPT)!=flags){
		fprintf(stderr,"transfer-slot VM/image/register/FP mismatch\n");
		abort();
	}
}


/* Check 18 transfers in both WORD models without aligned typed-slot accesses. */
int main(void)
{
	void (*ops[])(void)={movb,movw,movl,movf,cvtbw,cvtwb,cvtws,cvtsw,cvtwl,cvtlw,
		lea,negf,cvtrf,cvtfr,cvtwf,cvtlf,cvtfl,cvtfw};
	int inputs[]={1,sizeof(WORD),8,8,1,sizeof(WORD),sizeof(WORD),2,sizeof(WORD),8,
		0,8,4,8,sizeof(WORD),8,8,8};
	int outputs[]={1,sizeof(WORD),8,8,sizeof(WORD),1,2,sizeof(WORD),8,sizeof(WORD),
		sizeof(WORD),8,8,4,8,8,8,sizeof(WORD)};
	int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	uchar image[512],expected[512];
	uintptr address;
	uvlong raw;
	int op,side,width,i,shift,mode,checks,flags;
	fenv_t original;

	checks=0;
	if(fegetenv(&original)) abort();
	for(op=0;op<18;op++) for(side=0;side<2;side++){
		if(getenv("TRANSFER_SLOT_OP") && op!=atoi(getenv("TRANSFER_SLOT_OP"))) continue;
		if(op==10 && side==0) continue; // LEA materializes an address without reading it.
		width=side ? outputs[op] : inputs[op];
		for(i=0;i<=width;i++) for(shift=0;shift<8;shift++) for(mode=0;mode<4;mode++){
			memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
			R.s=image+64+shift; R.d=image+160+shift; R.m=(void*)1;
			raw=0x7ff0000000000042ULL; memmove(R.s,&raw,8);
			address=i==0 ? 0 : (uintptr)-1-(i-1);
			if(side) R.d=(void*)address; else R.s=(void*)address;
			memmove(expected,image,sizeof(image));
			if(fesetround(modes[mode]) || feclearexcept(FE_ALL_EXCEPT) || feraiseexcept(FE_DIVBYZERO)) abort();
			flags=fetestexcept(FE_ALL_EXCEPT);
			verify(ops[op],exBounds,image,expected,modes[mode],flags); checks++;
		}
	}
	/* An invalid destination must not consult an inaccessible source at address 1. */
	for(op=0;op<18;op++) for(mode=0;mode<4;mode++){
		memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
		R.s=(void*)1; R.d=H; R.m=H; memmove(expected,image,sizeof(image));
		if(fesetround(modes[mode]) || feclearexcept(FE_ALL_EXCEPT)) abort();
		verify(ops[op],exBounds,image,expected,modes[mode],0); checks++;
	}
	/* LEA accepts every source identity and writes only the active WORD width. */
	for(i=0;i<12;i++) for(shift=0;shift<8;shift++){
		WORD wanted;

		memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
		address=i==0 ? 0 : i==1 ? 1 : i==2 ? 0x100000123ULL : (uintptr)-1-(i-3);
		R.s=(void*)address; R.d=image+160+shift; R.m=H;
		wanted=(WORD)address; memmove(expected,image,sizeof(image));
		memmove(expected+160+shift,&wanted,sizeof(wanted));
		if(feclearexcept(FE_ALL_EXCEPT)) abort();
		verify(lea,nil,image,expected,fegetround(),0); checks++;
	}
	if(fesetenv(&original)) abort();
	printf("PASS C transfer-slot preflight: %d cases WORD%d\n",checks,(int)sizeof(WORD)*8);
	return 0;
}
