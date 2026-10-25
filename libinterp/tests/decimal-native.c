#include "comp-amd64.c"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>
REG R;
int mutator,nprop;
char exNilref[]="nil",exNomem[]="memory",exHeap[]="heap";
static jmp_buf boundary;
static char *raised;
/* Exercise the actual compiler and its executable mapping ownership. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Descriptor publication does not inject exceptions in this matrix. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
/* Observe the exact production C error identity at a fixture boundary. */
void error(char *text) {raised=text;longjmp(boundary,1);}
/* Rune conversion belongs to the C fallback; ASCII cases cannot call it. */
int runetochar(char *p,Rune *r) {(void)p;(void)r;assert(0);return 0;}
/* No Rune cache is read or allocated in this ASCII/nil comparison. */
ulong msize(void *p) {(void)p;assert(0);return 0;}
/* Preserve ASan stack checking without process-based leak inspection. */
int __lsan_is_turned_off(void) {return 1;}
#define OP(name) void name(void)
IMPLEMENTATION


/* Compare actual C conversion and published AMD64 code at every operand alignment. */
int
main(void)
{
	static char *texts[]={"","+","-","0","-0","+0","1","-1","999999999",
		"-999999999","+999999999","000000000","1000000000","2147483647",
		"-2147483648","9223372036854775807","-9223372036854775808"," 12",
		"12 ","12x","++1","--1","\xff","\t1","0x1","123456789",
		"1.5","-0.0","1e2","1e-300","1e-999","1e999","Inf","nan"};
	Module module={0};Inst instructions[3]={{0},{0},{0}},sentinel;
	union {uvlong align;uchar bytes[128];} object,slots;
	uchar expectedobject[128],expectedslots[128],savedobject[128],savedslots[128];
	String *s;
	REG saved;
	LONG value,big;
	WORD word;
	REAL real;
	int i,op,sa,da,alias,kind,pass,valid,length,j,round,flags,seed;
	int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	unsigned long checks=0;

	instructions[0].op=ICVTCW;instructions[1].op=ICVTCL;instructions[2].op=ICVTCF;
	module.prog=instructions;module.nprog=3;
	if(compile(&module,3,nil) != 1 || !module.jit || module.compiled)
		abort();
	if(!module.jit->entry[ICVTCW] || !module.jit->entry[ICVTCL] || !module.jit->entry[ICVTCF])
		abort();
	for(seed=0;seed<2;seed++)for(round=0;round<4;round++)for(i=0;i<sizeof(texts)/sizeof(texts[0]);i++)for(kind=0;kind<4;kind++)
	for(op=0;op<3;op++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(alias=0;alias<3;alias++){
		memset(object.bytes,0xa5,sizeof(object.bytes));s=(String*)object.bytes;
		length=strlen(texts[i]);s->len=length;s->max=length;s->tmp=nil;
		memmove(s->Sascii,texts[i],length+1);
		valid=length-(texts[i][0]=='+' || texts[i][0]=='-')<=9;
		for(j=(texts[i][0]=='+' || texts[i][0]=='-');j<length;j++)
			if(texts[i][j]<'0' || texts[i][j]>'9')valid=0;
		value=valid && length ? strtoll(texts[i],nil,10) : 0;
		real = strtod(texts[i], nil);
		if(op == 2 && (strcmp(texts[i], "+") == 0 || strcmp(texts[i], "-") == 0))
			valid = 0;
		if(kind==1){s=H;valid=1;value=0;real=0;}
		if(kind==2){s=nil;valid=0;}
		if(kind==3){((String*)object.bytes)->Sascii[length]='x';valid=0;}
		memset(slots.bytes,0xa5,sizeof(slots.bytes));memset(&R,0,sizeof(R));
		R.s=slots.bytes+8+sa;R.d=alias==1 ? R.s : alias==2 ? object.bytes+64+da : slots.bytes+80+da;
		R.PC=&sentinel;memmove(R.s,&s,sizeof(s));saved=R;
		memmove(savedobject,object.bytes,sizeof(object.bytes));memmove(savedslots,slots.bytes,sizeof(slots.bytes));
		for(pass=0;pass<2;pass++){
			memmove(object.bytes,savedobject,sizeof(object.bytes));memmove(slots.bytes,savedslots,sizeof(slots.bytes));
			memmove(expectedobject,object.bytes,sizeof(object.bytes));memmove(expectedslots,slots.bytes,sizeof(slots.bytes));
			R=saved;raised=nil;
			assert(!fesetround(rounds[round]));feclearexcept(FE_ALL_EXCEPT);
			if(seed)feraiseexcept(FE_INVALID|FE_INEXACT);
			flags=fetestexcept(FE_ALL_EXCEPT);
			if(valid || (pass && kind!=2)){
				if(pass && kind!=1){((String*)expectedobject)->Sascii[length]=0;value=strtoll(texts[i],nil,10);}
				big=value;word=(WORD)value;
				memmove(alias==2 ? expectedobject+64+da : expectedslots+(alias==1 ? 8+sa : 80+da),op==2 ? (void*)&real : op ? (void*)&big : (void*)&word,op ? sizeof(big) : sizeof(word));
			}
			/* Expected-value parsing above must not seed flags for the tested operation. */
			feclearexcept(FE_ALL_EXCEPT);
			if(seed)feraiseexcept(FE_INVALID|FE_INEXACT);
			if(op == 2 && pass && kind != 2){
				volatile REAL expected = strtod(kind == 1 ? "" : texts[i], nil);
				(void)expected;
				flags = fetestexcept(FE_ALL_EXCEPT);
				feclearexcept(FE_ALL_EXCEPT);
				if(seed)feraiseexcept(FE_INVALID|FE_INEXACT);
			}
			if(pass==0){
				if(module.jit->entry[instructions[op].op](&R) != valid)abort();
			}
			else if(setjmp(boundary)==0){
				if(op==2)cvtcf();else if(op)cvtcl();else cvtcw();
				if(kind==2)abort();
			}
			else assert(kind==2 && raised==exNilref);
			assert(!memcmp(object.bytes,expectedobject,sizeof(object.bytes)) && !memcmp(slots.bytes,expectedslots,sizeof(slots.bytes)));
			assert(!memcmp(&saved,&R,sizeof(R)));
			assert(fegetround()==rounds[round] && fetestexcept(FE_ALL_EXCEPT)==flags);
		}
		checks++;
	}
	assert(!fesetround(FE_TONEAREST));feclearexcept(FE_ALL_EXCEPT);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS decimal native: %lu actual C/JIT/alias/error cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
