#ifdef NATIVE_STRING_BRANCH
#include "comp-amd64.c"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1};
#else
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#endif
#include <setjmp.h>
#include <assert.h>
#include <stdio.h>

REG R;
/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;
char exNilref[]="nil",exHeap[]="heap";
static jmp_buf boundary;
static char *raised;
/* Invalid owners are checked before changing the branch PC. */
void error(char *message) {raised=message;longjmp(boundary,1);}
#define OP(name) void name(void)
#define S(r) (*(String**)R.r)
/* Retain stack/address instrumentation without unsupported leak inspection. */
int __lsan_is_turned_off(void) {return 1;}

#ifdef NATIVE_STRING_BRANCH
/* Publish the real leaf compiler with ordinary descriptor allocation. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Valid publication in this fixture does not raise a VM error. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
#endif
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
#ifdef NATIVE_STRING_BRANCH
	Module module={0};Inst native[6]={{0}};
	int opcodes[]={IBEQC,IBNEC,IBLTC,IBLEC,IBGTC,IBGEC};
	for(i=0;i<6;i++)native[i].op=opcodes[i];
	module.prog=native;module.nprog=6;
	assert(compile(&module,6,nil));
	for(i=0;i<6;i++)assert(module.jit->entry[opcodes[i]]);
#endif

	strings[0]=H;
	for(i=0;i<9;i++){
		strings[i+1]=calloc(1,sizeof(String)+64);assert(strings[i+1]);
		strings[i+1]->len=i<5 ? lengths[i] : -lengths[i];
		strings[i+1]->max=16;
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
#ifdef NATIVE_STRING_BRANCH
			{
				int handled=1;
				assert(module.jit->entry[opcodes[op]](&R)==handled);
				assert(R.PC==(handled && take ? target : instructions) && !memcmp(slots.bytes,before,sizeof(before)));
				R.PC=instructions;
			}
#endif
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
#ifdef NATIVE_STRING_BRANCH
	/* All high byte values use unsigned lexicographic ordering in native code. */
	for(i=0;i<256;i++)for(j=0;j<256;j++){
		left=strings[1];right=strings[2];left->len=right->len=1;
		left->Sascii[0]=i;right->Sascii[0]=j;
		R.s=&left;R.m=&right;
		for(op=0;op<6;op++){
			take=op==0 ? i==j : op==1 ? i!=j : op==2 ? i<j : op==3 ? i<=j : op==4 ? i>j : i>=j;
			R.PC=instructions;R.d=take ? (void*)&target : (void*)1;
			assert(module.jit->entry[opcodes[op]](&R)==1 && R.PC==(take ? target : instructions));
			checks++;
		}
	}
#endif
#ifdef NATIVE_STRING_BRANCH
	/* Compare both representations, including the C comparator's signed Rune keys. */
	{
		uint points[]={0,1,127,128,255,256,0x10000,0x10ffff,0x7fffffff,0x80000000U,0xffffffffU};
		int a,b,aw,bw,ak,bk;
		for(a=0;a<nelem(points);a++)for(b=0;b<nelem(points);b++)
		for(aw=0;aw<2;aw++)for(bw=0;bw<2;bw++){
			if(!aw && points[a]>255 || !bw && points[b]>255)continue;
			left=strings[6];right=strings[7];left->len=aw ? -1 : 1;right->len=bw ? -1 : 1;
			if(aw)left->Srune[0]=points[a];else left->Sascii[0]=points[a];
			if(bw)right->Srune[0]=points[b];else right->Sascii[0]=points[b];
			ak=(int)points[a];bk=(int)points[b];R.s=&left;R.m=&right;
			for(op=0;op<6;op++){
				take=op==0 ? ak==bk : op==1 ? ak!=bk : op==2 ? ak<bk : op==3 ? ak<=bk : op==4 ? ak>bk : ak>=bk;
				R.PC=instructions;R.d=take ? (void*)&target : (void*)1;
				assert(module.jit->entry[opcodes[op]](&R)==1 && R.PC==(take ? target : instructions));
				R.PC=instructions;ops[op]();assert(R.PC==(take ? target : instructions));checks++;
			}
		}
	}
#endif
	/* Every comparison rejects malformed storage before reading a branch target. */
	for(i=0;i<5;i++)for(j=0;j<3;j++)for(op=0;op<6;op++){
		String *invalid = strings[1];
		invalid->len = i == 0 ? 0 : i == 1 ? 1 : i == 2 ? -1 : i == 3 ? 8 : -8;
		invalid->max = i < 3 ? -1 : 7;
		left = j == 1 ? strings[2] : invalid;
		right = j == 0 ? strings[2] : invalid;
		R.s = &left; R.m = &right; R.d = (void*)1; R.PC = instructions;
#ifdef NATIVE_STRING_BRANCH
		if(module.jit->entry[opcodes[op]](&R) != 0 || R.PC != instructions)
			abort();
#endif
		raised = nil;
		if(setjmp(boundary) == 0){
			ops[op]();
			abort();
		}
		if(raised != exHeap || R.PC != instructions)
			abort();
		invalid->max = 16;
		checks++;
	}
	for(i=0;i<4;i++){
		String *invalid=i<2 ? nil : strings[1];
		if(i>=2)invalid->len=(-0x7fffffff-1);
		left=invalid;right=(i&1) ? invalid : strings[2];R.s=&left;R.m=&right;R.PC=instructions;
#ifdef NATIVE_STRING_BRANCH
		R.d=(void*)1;assert(module.jit->entry[IBEQC](&R)==0 && R.PC==instructions);
#endif
		raised=nil;if(setjmp(boundary)==0){beqc();assert(0);}
		assert(raised==(i<2 ? exNilref : exHeap) && R.PC==instructions);checks++;
	}
#ifdef NATIVE_STRING_BRANCH
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
#endif
	for(i=1;i<10;i++)free(strings[i]);
	printf("PASS string branches: %lu Unicode/alignment/PC checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
