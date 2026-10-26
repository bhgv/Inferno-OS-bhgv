#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <stdlib.h>
#include <stdio.h>

REG R;
#define OP(name) void name(void)
#define B(r) (*(BYTE*)R.r)
#define W(r) (*(WORD*)R.r)
#define V(r) (*(LONG*)R.r)

/* Keep address instrumentation enabled where process inspection is unavailable. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

char exBounds[] = "bounds"; /* Used decoded-slot bounds error. */


/* condition is a once-evaluated fixture invariant; failure terminates the test.
 * No ownership or blocking. Explicit checks remain active with NDEBUG.
 */
static void branchpredicate(int condition)
{
    if(!condition){
        abort();
    }
}
#define branchcheck(condition) branchpredicate(!!(condition))


/* Valid old differential inputs must never raise the borrowed VM error message. */
void error(char *message)
{
    (void)message;
    abort();
}

IMPLEMENTATION

/* Exercise signed boundaries, independent byte offsets and unread false targets. */
int
main(void)
{
	void (*ops[])(void) = {beqb,bneb,bltb,bleb,bgtb,bgeb,
		beqw,bnew,bltw,blew,bgtw,bgew,beql,bnel,bltl,blel,bgtl,bgel};
	uvlong values[] = {0,1,127,128,255,256,0x7fffffffULL,0x80000000ULL,
		0xffffffffULL,0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,~0ULL};
	union { uvlong align; uchar bytes[128]; } slots;
	uchar before[128];
	Inst instructions[3], *target = instructions+2;
	BYTE ba,bb;
	WORD wa,wb;
	LONG a,b;
	int op,i,j,x,y,z,take;
	unsigned long checks = 0;

	for(op=0; op<nelem(ops); op++)
	for(i=0; i<nelem(values); i++)
	for(j=0; j<nelem(values); j++)
	for(x=0; x<8; x++)
	for(y=0; y<8; y++)
	for(z=0; z<8; z++){
		memset(slots.bytes,0xa5,sizeof(slots.bytes));
		R.s=slots.bytes+8+x;
		R.m=slots.bytes+40+y;
		R.d=slots.bytes+80+z;
		if(op<6){
			ba=values[i]; bb=values[j]; a=ba; b=bb;
			memmove(R.s,&ba,sizeof(ba)); memmove(R.m,&bb,sizeof(bb));
		}else if(op<12){
			wa=values[i]; wb=values[j]; a=wa; b=wb;
			memmove(R.s,&wa,sizeof(wa)); memmove(R.m,&wb,sizeof(wb));
		}else{
			memmove(&a,&values[i],sizeof(a)); memmove(&b,&values[j],sizeof(b));
			memmove(R.s,&a,sizeof(a)); memmove(R.m,&b,sizeof(b));
		}
		switch(op%6){
		case 0: take=a==b; break;
		case 1: take=a!=b; break;
		case 2: take=a<b; break;
		case 3: take=a<=b; break;
		case 4: take=a>b; break;
		default: take=a>=b; break;
		}
		memmove(R.d,&target,sizeof(target));
		memmove(before,slots.bytes,sizeof(before));
		R.PC=instructions;
		if(!take) R.d=(void*)1;
		ops[op]();
		branchcheck(R.PC==(take ? target : instructions));
		branchcheck(!memcmp(before,slots.bytes,sizeof(before)));
		checks++;
	}
	for(z=0; z<8; z++){
		R.d=slots.bytes+z;
		memmove(R.d,&target,sizeof(target));
		R.PC=instructions;
		jmp();
		branchcheck(R.PC==target);
		checks++;
	}
	printf("PASS integer branches: %lu checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
