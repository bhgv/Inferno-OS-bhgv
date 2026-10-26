#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#include <stdio.h>
#include <setjmp.h>

REG R;
int mutator, nprop;
char exHeap[]="heap", exModule[]="module";
static jmp_buf boundary;
static char *raised;
static int extensions, initialized;


/* Keep sanitizer instrumentation without unavailable process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Preserve VM failures for exact before/after comparisons. */
void
error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}


/* Provide ordinary ownership for native publication metadata. */
void*
mallocz(ulong size,int clear)
{
	void *p;

	p=malloc(size);
	if(p && clear)memset(p,0,size);
	return p;
}


/* Compilation uses valid input and needs no simulated allocation failures. */
int
kprotect(int (*fn)(void*),void *arg)
{
	return fn(arg);
}


/* Detect pointer-map fallback without replacing its separate VM regression suite. */
void
initmem(Type *type,void *data)
{
	(void)type; (void)data;
	initialized++;
}


/* Detect extent fallback; real copying and unwind are checked by stack-lifecycle.py. */
void
extend(void)
{
	extensions++;
	R.s=R.SP;
}

#define OP(name) void name(void) // Interpreter instruction entry point.
IMPLEMENTATION


/* Compare exact stack/slot/REG images and execute real published AMD64 leaves. */
int
main(void)
{
	union {uvlong align;uchar bytes[2048];} memory;
	uchar expected[2048],before[2048];
	REG saved;
	Module compiled={0},owner={0};
	Modlink link={0};
	Inst instruction={0};
	Type descriptor={1}, *types[]={&descriptor};
	Frame *result;
	WORD index=0;
	int mode,sa,da,alias,pass,valid;
	ulong total=0;

	instruction.op=IFRAME;compiled.prog=&instruction;compiled.nprog=1;
	if(!compile(&compiled,1,nil) || !compiled.jit || !compiled.jit->entry[IFRAME])abort();
	owner.type=types;owner.ntype=1;link.m=&owner;
	for(mode=0;mode<9;mode++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)
	for(alias=0;alias<3;alias++)for(pass=0;pass<2;pass++){
		memset(memory.bytes,0xa5,sizeof(memory.bytes));memset(&R,0,sizeof(R));
		descriptor.size=sizeof(Frame)+32;descriptor.np=0;
		R.M=&link;R.s=memory.bytes+sa;R.d=memory.bytes+128+da;
		R.SP=memory.bytes+512;R.TS=memory.bytes+1536;
		if(alias==1)R.d=R.SP+offsetof(Frame,t);
		if(alias==2)R.s=R.SP+offsetof(Frame,mr);
		memmove(R.s,&index,sizeof(index));
		if(mode==1)R.TS=R.SP+descriptor.size;
		if(mode==2)R.TS=R.SP+descriptor.size+1;
		if(mode==3)descriptor.np=1;
		if(mode==4)R.SP=H;
		if(mode==5)R.TS=H;
		if(mode==6)R.SP=nil;
		if(mode==7)R.TS=nil;
		if(mode==8)descriptor.np=-1;
		memmove(before,memory.bytes,sizeof(before));memmove(expected,before,sizeof(before));memmove(&saved,&R,sizeof(R));
		extensions=initialized=0;raised=nil;valid=mode==0 || mode==2;
		if(pass==0){
			if(compiled.jit->entry[IFRAME](&R)!=valid)abort();
			if(!valid && (memcmp(&saved,&R,sizeof(R)) || memcmp(before,memory.bytes,sizeof(before))))abort();
		}
		if(pass!=0 || !valid){
			if(setjmp(boundary)==0)
				frame();
			else{
				if(mode<4 || raised!=exHeap || memcmp(&saved,&R,sizeof(R)) ||
				   memcmp(before,memory.bytes,sizeof(before)))abort();
				total++;
				continue;
			}
		}
		if(mode>=4)abort();
		if(valid){
			result=(Frame*)(memory.bytes+512);
			memmove(expected+512+offsetof(Frame,t),&types[0],sizeof(void*));
			memset(expected+512+offsetof(Frame,mr),0,sizeof(void*));
			memmove(expected+((uchar*)saved.d-memory.bytes),&result,sizeof(result));
			saved.SP+=descriptor.size;
			if(memcmp(expected,memory.bytes,sizeof(expected)) || memcmp(&saved,&R,sizeof(R)) || extensions || initialized)abort();
		}else if(mode==1 ? extensions!=1 : initialized!=1)abort();
		total++;
	}
	compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);
	printf("PASS FRAME C/RX: %lu stack/alias/fallback/owner cases WORD%zu\n",total,sizeof(WORD)*8);
	return 0;
}
