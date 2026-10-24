#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <setjmp.h>
#include <assert.h>
#include <stdio.h>

REG R;
char exNilref[]="nil",exHeap[]="heap";
static jmp_buf boundary;
static char *raised;
/* Invalid owners are checked before changing the branch PC. */
void error(char *message) {raised=message;longjmp(boundary,1);}
#define OP(name) void name(void)
#define S(r) (*(String**)R.r)
/* Retain stack/address instrumentation without unsupported leak inspection. */
int __lsan_is_turned_off(void) {return 1;}

IMPLEMENTATION

/* Compare code points rather than using the production string comparison oracle. */
int
main(void)
{
	Rune text[9][4]={{0},{'a'},{'a','b'},{'z'},{0},{'a'},{'a','b'},{'a',0x100},{0x100}};
	int lengths[]={0,1,2,1,0,1,2,2,1};
	String *strings[10],*left,*right;
	void (*ops[])(void)={beqc,bnec,bltc,blec,bgtc,bgec};
	union {uvlong align;uchar bytes[128];} slots;
	uchar before[128];
	Inst instructions[3],*target=instructions+2;
	int i,j,k,sa,ma,da,op,order,n1,n2,n,take;
	unsigned long checks=0;

	strings[0]=H;
	for(i=0;i<9;i++){
		strings[i+1]=calloc(1,sizeof(String)+64);assert(strings[i+1]);
		strings[i+1]->len=i<5 ? lengths[i] : -lengths[i];
		if(i<5)for(j=0;j<lengths[i];j++)strings[i+1]->Sascii[j]=text[i][j];
		else memmove(strings[i+1]->Srune,text[i],lengths[i]*sizeof(Rune));
	}
	for(i=0;i<10;i++)for(j=0;j<10;j++){
		n1=i ? lengths[i-1] : 0;n2=j ? lengths[j-1] : 0;n=n1<n2 ? n1 : n2;order=0;
		for(k=0;k<n;k++)if(text[i-1][k]!=text[j-1][k]){order=text[i-1][k]<text[j-1][k] ? -1 : 1;break;}
		if(order==0)order=(n1>n2)-(n1<n2);
		left=strings[i];right=strings[j];
		for(op=0;op<6;op++)for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++){
			switch(op){
			case 0:take=order==0;break;
			case 1:take=order!=0;break;
			case 2:take=order<0;break;
			case 3:take=order<=0;break;
			case 4:take=order>0;break;
			default:take=order>=0;break;
			}
			memset(slots.bytes,0xa5,sizeof(slots.bytes));
			R.s=slots.bytes+16+sa;R.m=slots.bytes+48+ma;R.d=slots.bytes+80+da;R.PC=instructions;
			memmove(R.s,&left,sizeof(left));memmove(R.m,&right,sizeof(right));memmove(R.d,&target,sizeof(target));
			memmove(before,slots.bytes,sizeof(before));if(!take)R.d=(void*)1;
			ops[op]();
			assert(R.PC==(take ? target : instructions) && !memcmp(slots.bytes,before,sizeof(before)));
			checks++;
		}
	}
	for(i=0;i<256;i++)for(j=0;j<256;j++){
		String *narrow=strings[1],*wide=strings[6];
		int expected=(i>j)-(i<j),forward,reverse;
		narrow->len=1;wide->len=-1;narrow->Sascii[0]=i;wide->Srune[0]=j;
		forward=stringcmp(narrow,wide);reverse=stringcmp(wide,narrow);
		assert((forward>0)-(forward<0)==expected && (reverse>0)-(reverse<0)==-expected);
		checks+=2;
	}
	for(i=0;i<4;i++){
		String *invalid=i<2 ? nil : strings[1];
		if(i>=2)invalid->len=(-0x7fffffff-1);
		left=invalid;right=(i&1) ? invalid : strings[2];R.s=&left;R.m=&right;R.PC=instructions;
		raised=nil;if(setjmp(boundary)==0){beqc();assert(0);}
		assert(raised==(i<2 ? exNilref : exHeap) && R.PC==instructions);checks++;
	}
	for(i=1;i<10;i++)free(strings[i]);
	printf("PASS string branches: %lu Unicode/alignment/PC checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
