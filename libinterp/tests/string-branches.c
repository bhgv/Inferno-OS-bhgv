#ifdef NATIVE_STRING_BRANCH
#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#else
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#endif
#include <setjmp.h>
#include <stdio.h>

#define branchcheck(condition) branchpredicate(!!(condition)) // Keep scalar/pointer checks active under NDEBUG.


/* Preserve every fixture predicate in optimized production-mode builds. */
static void
branchpredicate(int condition)
{
	if(!condition)
		abort();
}


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


/* Keep VM exceptions outside the caller's loop variables. */
static int
branchchecked(void (*operation)(void))
{
	raised = nil;
	if(setjmp(boundary))
		return 0;
	operation();
	return 1;
}


/* Invalid targets matter only for true predicates; invalid source slots always decline. */
static void
checkpreflight(Module *module)
{
	union {
		uvlong align; // Natural alignment for both synthetic String headers.
		uchar bytes[2][96]; // Headers, character storage and surrounding canaries.
	} objects;
	uchar slots[128], before[128], image[sizeof(objects)];
	String *left, *right;
	REG saved;
	Inst code[2];
	void (*ops[])(void) = {beqc, bnec, bltc, blec, bgtc, bgec};
#ifdef NATIVE_STRING_BRANCH
	int codes[] = {IBEQC, IBNEC, IBLTC, IBLEC, IBGTC, IBGEC};
#endif
	uintptr pointer;
	int op, kind, mode, alignment, order, take, success, native, total = 0;

	(void)module;
	memset(&objects, 0xa5, sizeof(objects));
	for(op = 0; op < 6; op++)
	for(kind = 0; kind < 3; kind++)
	for(mode = 0; mode < (kind == 2 ? 5 : 4); mode++)
	for(alignment = 0; alignment < 8; alignment++)
	for(order = -1; order <= 1; order++){
		if(getenv("BRANCH_PROBE_KIND") && kind != atoi(getenv("BRANCH_PROBE_KIND")))
			continue;
		if(kind < 2 && order != 0)
			continue;
		take = op == 0 ? order == 0 : op == 1 ? order != 0 : op == 2 ? order < 0 :
			op == 3 ? order <= 0 : op == 4 ? order > 0 : order >= 0;
		if(mode == 4 && take)
			continue; // An arbitrary unmapped slot is a false-target nonaccess probe only.
		pointer = mode == 0 ? 0 : mode == 1 ? (uintptr)H : mode == 4 ? 1 : (uintptr)-1-(mode-1);
		memset(&objects, 0xa5, sizeof(objects));
		left = (String*)objects.bytes[0]; right = (String*)objects.bytes[1];
		left->len = left->max = right->len = right->max = 1;
		left->Sascii[0] = 'b';
		right->Sascii[0] = order < 0 ? 'c' : order == 0 ? 'b' : 'a';
		memset(slots, 0x5a, sizeof(slots));
		memset(&R, 0, sizeof(R));
		R.s = slots+alignment; R.m = slots+32+alignment; R.d = (void*)pointer;
		memmove(R.s, &left, sizeof(left)); memmove(R.m, &right, sizeof(right));
		if(kind == 0)R.s = (void*)pointer;
		if(kind == 1)R.m = (void*)pointer;
		R.PC = code;
		saved = R;
		memmove(before, slots, sizeof(before)); memmove(image, &objects, sizeof(objects));
		success = kind == 2 && !take;
		native = success;
		(void)native;
#ifdef NATIVE_STRING_BRANCH
		branchcheck(module->jit->entry[codes[op]](&R) == native);
		branchcheck(!memcmp(&R, &saved, sizeof(R)));
#endif
		branchcheck(branchchecked(ops[op]) == success);
		branchcheck((success || raised == exHeap) && !memcmp(&R, &saved, sizeof(R)) &&
			!memcmp(slots, before, sizeof(slots)) && !memcmp(&objects, image, sizeof(objects)));
		total++;
	}
	if(!getenv("BRANCH_PROBE_KIND") || atoi(getenv("BRANCH_PROBE_KIND")) == 3)
	for(op = 0; op < 6; op++)
	for(kind = 0; kind < 2; kind++)
	for(mode = 0; mode < 3; mode++)
	for(alignment = 0; alignment < 8; alignment++){
		/* The other owner may be valid, unmapped or H; nil still wins before metadata reads. */
		memset(slots, 0xa5, sizeof(slots));
		left = mode == 0 ? (String*)objects.bytes[0] : mode == 1 ? (String*)1 : H;
		right = left;
		if(kind == 0)left = nil;
		else right = nil;
		memset(&R, 0, sizeof(R));
		R.s = slots+alignment; R.m = slots+32+alignment; R.d = (void*)1; R.PC = code;
		memmove(R.s, &left, sizeof(left)); memmove(R.m, &right, sizeof(right));
		saved = R;
		memmove(before, slots, sizeof(before)); memmove(image, &objects, sizeof(objects));
#ifdef NATIVE_STRING_BRANCH
		branchcheck(module->jit->entry[codes[op]](&R) == 0);
		branchcheck(!memcmp(&R, &saved, sizeof(R)));
#endif
		branchcheck(!branchchecked(ops[op]) && raised == exNilref &&
			!memcmp(&R, &saved, sizeof(R)) && !memcmp(slots, before, sizeof(slots)) &&
			!memcmp(&objects, image, sizeof(objects)));
		total++;
	}
	printf("PASS string branch preflight: %d slot/conditional-target/image cases WORD%zu\n", total, sizeof(WORD)*8);
}


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
	branchcheck(compile(&module,6,nil));
	for(i=0;i<6;i++)branchcheck(module.jit->entry[opcodes[i]]);
#endif

#ifdef NATIVE_STRING_BRANCH
	checkpreflight(&module);
#else
	checkpreflight(nil);
#endif
	if(getenv("BRANCH_PREFLIGHT_ONLY")){
#ifdef NATIVE_STRING_BRANCH
		module.jit->freecode(module.jit->code,module.jit->size); free(module.jit);
#endif
		return 0;
	}
	strings[0]=H;
	for(i=0;i<9;i++){
		strings[i+1]=calloc(1,sizeof(String)+64);branchcheck(strings[i+1]);
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
				branchcheck(module.jit->entry[opcodes[op]](&R)==handled);
				branchcheck(R.PC==(handled && take ? target : instructions) && !memcmp(slots.bytes,before,sizeof(before)));
				R.PC=instructions;
			}
#endif
			ops[op]();
			branchcheck(R.PC==(take ? target : instructions) && !memcmp(slots.bytes,before,sizeof(before)));
			checks++;
		}
	}
	for(i=0;i<256;i++)for(j=0;j<256;j++){
		String *narrow=strings[1],*wide=strings[6];
		int expected=(i>j)-(i<j),forward,reverse;
		narrow->len=1;wide->len=-1;narrow->Sascii[0]=i;wide->Srune[0]=j;
		forward=stringcmp(narrow,wide);reverse=stringcmp(wide,narrow);
		branchcheck((forward>0)-(forward<0)==expected && (reverse>0)-(reverse<0)==-expected);
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
			branchcheck(module.jit->entry[opcodes[op]](&R)==1 && R.PC==(take ? target : instructions));
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
				branchcheck(module.jit->entry[opcodes[op]](&R)==1 && R.PC==(take ? target : instructions));
				R.PC=instructions;ops[op]();branchcheck(R.PC==(take ? target : instructions));checks++;
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
		R.d=(void*)1;branchcheck(module.jit->entry[IBEQC](&R)==0 && R.PC==instructions);
#endif
		raised=nil;if(setjmp(boundary)==0){beqc();branchcheck(0);}
		branchcheck(raised==(i<2 ? exNilref : exHeap) && R.PC==instructions);checks++;
	}
#ifdef NATIVE_STRING_BRANCH
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
#endif
	for(i=1;i<10;i++)free(strings[i]);
	printf("PASS string branches: %lu Unicode/alignment/PC checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
