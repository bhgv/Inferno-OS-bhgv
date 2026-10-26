#include "comp-amd64.c"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1};
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

REG R;
/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;
char exModule[]="module",exBounds[]="bounds";
static char *raised;
static jmp_buf boundary;
static void (*operation)(void);

/* Compile owned metadata without starting a full emulator process. */
void*
mallocz(ulong size,int clear)
{
	void *p=malloc(size);
	if(p && clear)
		memset(p,0,size);
	return p;
}

/* Valid module compilation uses the real callback without injected failures here. */
int
kprotect(int (*callback)(void*),void *argument)
{
	return callback(argument);
}

/* Observe the exact VM exception while preserving all register and data images. */
void
error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}

/* Retain stack/address instrumentation without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define V(r) (*(LONG*)R.r)
IMPLEMENTATION

/* Isolate nonlocal errors from the fixture's automatic loop variables. */
static int
invoke(void)
{
	raised=nil;
	if(setjmp(boundary))
		return 0;
	operation();
	return 1;
}

/* Validate integer/big table searches with a linear oracle and both relocated representations. */
static void
casechecks(Module *module,Modlink *link,Inst *code)
{
	LONG values[]={-2147483648LL,2147483647LL,-129,-128,-127,-121,-120,-1,0,1,7,8,15,16,31,127,
		(vlong)1<<40,-((vlong)1<<40),9223372036854775807LL,(-9223372036854775807LL-1)};
	int defaults[]={-1,0,31,32};
	union { uvlong align; uchar bytes[1024]; } memory;
	uchar before[1024];
	Inst sentinel;
	REG saved,expected;
	LONG value,low,high,base;
	WORD word;
	int family,compiled,pattern,count,vi,di,sa,ta,k,header,stride,destination,entry,valid,accepted;
	unsigned long checks=0;

	module->prog=code;module->nprog=32;link->m=module;link->prog=code;
	for(family=0;family<2;family++)for(compiled=0;compiled<2;compiled++)
	for(pattern=0;pattern<3;pattern++)for(count=-1;count<=8;count++)
	for(vi=0;vi<nelem(values);vi++)for(di=0;di<nelem(defaults);di++)
	for(sa=0;sa<8;sa++)for(ta=0;ta<8;ta++){
		if(getenv("GOTO_CASE_FAMILY") && family!=atoi(getenv("GOTO_CASE_FAMILY")))continue;
		memset(memory.bytes,0xa5,sizeof(memory.bytes));memset(&R,0,sizeof(R));
		link->compiled=compiled;R.M=link;R.PC=&sentinel;
		R.s=memory.bytes+16+sa;R.d=memory.bytes+128+ta;
		value=values[vi];word=(WORD)value;
		if(!family)value=(LONG)word;
		memmove(R.s,family ? (void*)&value : (void*)&word,family ? sizeof(value) : sizeof(word));
		header=family ? 8 : 4;stride=family ? 24 : 12;
		base=pattern==0 ? -128 : pattern==1 ? 0 : family ? (vlong)1<<40 : -2147483648LL;
		destination=defaults[di];
		memmove(R.d,&count,4);
		for(k=0;k<(count>0 ? count : 0);k++){
			low=base+k*16;high=low+8;entry=(k*7+di)%35-1;
			if(compiled)entry=(int)(0x80000000U+(u32int)entry);
			if(family){
				memmove((uchar*)R.d+header+k*stride,&low,8);
				memmove((uchar*)R.d+header+k*stride+8,&high,8);
			}else{
				int lo=(int)low,hi=(int)high;
				memmove((uchar*)R.d+header+k*stride,&lo,4);
				memmove((uchar*)R.d+header+k*stride+4,&hi,4);
			}
			memmove((uchar*)R.d+header+k*stride+(family ? 16 : 8),&entry,4);
			if(value>=low && value<high)destination=entry;
		}
		entry=compiled ? (int)(0x80000000U+(u32int)defaults[di]) : defaults[di];
		memmove((uchar*)R.d+header+(count>0 ? count : 0)*stride,&entry,4);
		if(count>=0){
			int found=0;
			for(k=0;k<count;k++)if(value>=base+k*16 && value<base+k*16+8)found=1;
			if(!found)destination=entry;
		}
		valid=count>=0 && (compiled || (destination>=0 && destination<32));
		saved=R;expected=R;
		if(valid)expected.PC=compiled ? (Inst*)(uintptr)(u32int)destination : code+destination;
		if(getenv("GOTO_VALID_ONLY") && !valid)continue;
		memmove(before,memory.bytes,sizeof(before));operation=family ? casel : icase;
		assert(invoke()==valid && (valid || raised==exBounds));
		assert(!memcmp(&R,&expected,sizeof(R)) && !memcmp(before,memory.bytes,sizeof(before)));
		R=saved;accepted=valid && !compiled;
		assert(module->jit->entry[family ? ICASEL : ICASE](&R)==accepted);
		assert(!memcmp(&R,accepted ? &expected : &saved,sizeof(R)) && !memcmp(before,memory.bytes,sizeof(before)));
		checks++;
	}
	/* Invalid headers and metadata must fail before target formation or poisoned reads. */
	for(family=0;family<2;family++)for(k=0;k<14;k++){
		if(getenv("GOTO_CASE_FAMILY") && family!=atoi(getenv("GOTO_CASE_FAMILY")))continue;
		memset(memory.bytes,0xa5,sizeof(memory.bytes));memset(&R,0,sizeof(R));
		link->m=module;link->compiled=0;module->prog=code;module->nprog=32;
		R.M=link;R.PC=&sentinel;R.s=memory.bytes+16;R.d=memory.bytes+128;
		value=0;word=0;count=0;entry=1;header=family ? 8 : 4;
		memmove(R.s,family ? (void*)&value : (void*)&word,family ? sizeof(value) : sizeof(word));
		memmove(R.d,&count,4);memmove((uchar*)R.d+header,&entry,4);
		if(k==0)R.M=nil;
		if(k==1)R.M=H;
		if(k==2)link->m=nil;
		if(k==3)link->m=H;
		if(k<4)R.s=R.d=(void*)1;
		if(k==4){count=-1;memmove(R.d,&count,4);}
		if(k==5){count=0x7fffffff;memmove(R.d,&count,4);}
		if(k==6)R.d=nil;
		if(k==7)R.d=H;
		if(k==8)R.d=(void*)((uintptr)-1-1);
		if(k==9)module->prog=nil;
		if(k==10)module->prog=H;
		if(k==11)module->prog=(Inst*)((uintptr)-1-8);
		if(k==12)module->nprog=0;
		if(k==13)module->nprog=-1;
		saved=R;operation=family ? casel : icase;
		assert(!invoke() && raised==(k<4 ? exModule : exBounds));
		assert(!memcmp(&R,&saved,sizeof(R)));
		assert(!module->jit->entry[family ? ICASEL : ICASE](&R) && !memcmp(&R,&saved,sizeof(R)));
		checks++;
	}
	printf("PASS case wire tables: %lu width/alignment/metadata/relocation checks WORD%zu\n",checks,sizeof(WORD)*8);
}

/* Exercise the native table limit and retain C dispatch for larger valid tables. */
static void
largecases(Module *module,Modlink *link,Inst *code)
{
	uchar *table,*before;
	union { uvlong align; uchar bytes[24]; } source;
	LONG value,low,high;
	WORD word;
	Inst sentinel;
	REG saved,expected;
	int family,count,align,k,which,header,stride,destination,entry,accepted;
	unsigned long checks=0;

	module->prog=code;module->nprog=32;link->m=module;link->prog=code;link->compiled=0;
	table=malloc(8+4096*24+32);before=malloc(8+4096*24+32);assert(table && before);
	for(family=0;family<2;family++)for(count=4095;count<=4096;count++)for(align=1;align<=4;align+=3){
		memset(table,0xa5,8+4096*24+32);
		header=family ? 8 : 4;stride=family ? 24 : 12;
		R.d=table+align;memmove(R.d,&count,4);
		for(k=0;k<count;k++){
			low=k*2;high=low+1;entry=k%32;
			if(family){memmove((uchar*)R.d+header+k*stride,&low,8);memmove((uchar*)R.d+header+k*stride+8,&high,8);}
			else{int lo=low,hi=high;memmove((uchar*)R.d+header+k*stride,&lo,4);memmove((uchar*)R.d+header+k*stride+4,&hi,4);}
			memmove((uchar*)R.d+header+k*stride+(family ? 16 : 8),&entry,4);
		}
		entry=31;memmove((uchar*)R.d+header+count*stride,&entry,4);
		memmove(before,table,8+4096*24+32);
		for(which=0;which<8;which++){
			value=which==0 ? -1 : which==1 ? 0 : which==2 ? 1 : which==3 ? 2 :
				which==4 ? (count-1)*2 : which==5 ? (count-1)*2+1 : which==6 ? count*2 : 127;
			word=value;R.s=source.bytes+1;R.M=link;R.PC=&sentinel;
			memmove(R.s,family ? (void*)&value : (void*)&word,family ? sizeof(value) : sizeof(word));
			destination=value>=0 && value<count*2 && value%2==0 ? (value/2)%32 : 31;
			saved=R;expected=R;expected.PC=code+destination;operation=family ? casel : icase;
			assert(invoke() && !memcmp(&R,&expected,sizeof(R)));
			R=saved;accepted=count<=4095;
			assert(module->jit->entry[family ? ICASEL : ICASE](&R)==accepted);
			assert(!memcmp(&R,accepted ? &expected : &saved,sizeof(R)));
			assert(!memcmp(before,table,8+4096*24+32));checks++;
		}
	}
	free(before);free(table);
	printf("PASS large native cases: %lu boundary/target/fallback checks WORD%zu\n",checks,sizeof(WORD)*8);
}

/* Compare real C/native table dispatch against a separate fixed-width table oracle. */
int
main(void)
{
	WORD indices[]={-1,0,1,6,15,16,17,0x7fffffff,
		(WORD)((UWORD)1<<(sizeof(WORD)*8-1)),(WORD)(((UWORD)1<<(sizeof(WORD)*8-1))-1),
		(WORD)((uvlong)1<<32)};
	int counts[]={-1,0,1,2,7,16};
	int destinations[]={-1,0,1,31,32,(-0x7fffffff-1),0x7fffffff};
	union { uvlong align; uchar bytes[512]; } memory;
	uchar before[512];
	Module module={0};
	Modlink link={0};
	Inst code[32]={{0}},sentinel;
	REG saved,expected;
	WORD index;
	u32int bits;
	int c,i,d,sa,ta,alias,k,count,destination,valid,ok,kind;
	unsigned long checks=0;

	operation=igoto;
	for(i=0;i<32;i++)
		code[i].op=IGOTO;
	code[1].op=ICASE;code[2].op=ICASEL;
	module.prog=code;module.nprog=32;
	assert(compile(&module,32,nil) && module.jit->entry[IGOTO] && module.jit->entry[ICASE] && module.jit->entry[ICASEL]);
	link.m=&module;link.prog=code;
	if(getenv("GOTO_CASE_ONLY")){
		casechecks(&module,&link,code);
		module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
		return 0;
	}
	for(c=0;c<nelem(counts);c++)for(i=0;i<nelem(indices);i++)for(d=0;d<nelem(destinations);d++)
	for(sa=0;sa<8;sa++)for(ta=0;ta<8;ta++)for(alias=0;alias<3;alias++){
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		memset(&R,0,sizeof(R));
		R.M=&link;R.PC=&sentinel;
		R.d=memory.bytes+128+ta;
		R.s=alias==1 ? (uchar*)R.d-4 : alias==2 ? R.d : memory.bytes+16+sa;
		for(k=0;k<16;k++){
			bits=(u32int)destinations[d]+(u32int)(k%3);
			memmove((uchar*)R.d+k*4,&bits,4);
		}
		memmove((uchar*)R.d-4,&counts[c],4);
		if(!alias)
			memmove(R.s,&indices[i],sizeof(WORD));
		memmove(&index,R.s,sizeof(index));
		memmove(&count,(uchar*)R.d-4,4);
		valid=count>0 && index>=0 && index<count;
		if(valid){
			memmove(&destination,(uchar*)R.d+index*4,4);
			valid=destination>=0 && destination<32;
		}
		if(getenv("GOTO_VALID_ONLY") && !valid)continue;
		memmove(before,memory.bytes,sizeof(before));
		saved=R;expected=R;
		if(valid)
			expected.PC=code+destination;
		ok=invoke();
		assert(ok==valid && (ok || raised==exBounds));
		assert(!memcmp(&R,&expected,sizeof(R)) && !memcmp(before,memory.bytes,sizeof(before)));
		R=saved;
		assert(module.jit->entry[IGOTO](&R)==valid);
		assert(!memcmp(&R,valid ? &expected : &saved,sizeof(R)) && !memcmp(before,memory.bytes,sizeof(before)));
		checks++;
	}
	/* Reject bad metadata without reading poisoned source/table addresses. */
	for(kind=0;kind<13;kind++){
		memset(&R,0,sizeof(R));
		link.m=&module;link.compiled=0;module.prog=code;module.nprog=32;
		R.M=&link;R.s=memory.bytes+16;R.d=memory.bytes+128;R.PC=&sentinel;
		index=0;count=1;destination=1;
		memmove(R.s,&index,sizeof(index));memmove((uchar*)R.d-4,&count,4);memmove(R.d,&destination,4);
		if(kind==0)R.M=nil;
		if(kind==1)R.M=H;
		if(kind==2)link.m=nil;
		if(kind==3)link.m=H;
		if(kind<4)R.s=R.d=(void*)1;
		if(kind==4)module.nprog=-1;
		if(kind==5)module.nprog=0;
		if(kind==6)module.prog=nil;
		if(kind==7)module.prog=H;
		if(kind==8)module.prog=(Inst*)((uintptr)-1-8);
		if(kind>=9)R.d=kind==12 ? H : (void*)(uintptr)(kind-9);
		saved=R;
		assert(!invoke() && raised==(kind<4 ? exModule : exBounds));
		assert(!memcmp(&R,&saved,sizeof(R)));
		assert(!module.jit->entry[IGOTO](&R) && !memcmp(&R,&saved,sizeof(R)));
		checks++;
	}
	/* Already relocated legacy tables have a zero count and retain four-byte raw targets. */
	link.m=&module;link.compiled=1;module.prog=code;module.nprog=32;
	for(i=0;i<16;i++){
		index=i;count=0;bits=0x80000000U+(u32int)i*17;
		R.M=&link;R.s=memory.bytes+17;R.d=memory.bytes+129;R.PC=&sentinel;
		memmove(R.s,&index,sizeof(index));memmove((uchar*)R.d-4,&count,4);memmove((uchar*)R.d+i*4,&bits,4);
		saved=R;
		assert(!module.jit->entry[IGOTO](&R) && !memcmp(&R,&saved,sizeof(R)));
		assert(invoke() && R.PC==(Inst*)(uintptr)bits);
		checks++;
	}
	link.compiled=1;link.m=(Module*)1;R.M=&link;R.s=R.d=(void*)1;R.PC=&sentinel;saved=R;
	assert(!module.jit->entry[IGOTO](&R) && !memcmp(&R,&saved,sizeof(R)));
	casechecks(&module,&link,code);
	largecases(&module,&link,code);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS native goto: %lu table-width/alignment/alias/error/legacy checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
