#ifdef NATIVE_CALL
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
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
REG R;
/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;
char exNilref[]="nil";
static jmp_buf boundary;
static int errors;
/* Retain stack instrumentation without sandbox process inspection. */
int __lsan_is_turned_off(void) {return 1;}
/* Deliver VM nil-frame failures to a test boundary. */
void error(char *message) {assert(message==exNilref);errors++;longjmp(boundary,1);}
#ifdef NATIVE_CALL
/* Keep normal compiler allocation ownership in the native fixture. */
void *mallocz(ulong size,int clear) {void *p=malloc(size);if(p && clear)memset(p,0,size);return p;}
/* Compilation uses valid descriptors and must not raise errors. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
#endif
#define OP(name) void name(void)
#define T(r) (*(void**)R.r)
#define JMP(r) memmove(&R.PC,R.r,sizeof(R.PC))
IMPLEMENTATION

/* Compare the complete frame image and REG, including overlapping operand slots. */
int
main(void)
{
	union {uvlong align;uchar bytes[1024];} memory;
	uchar expected[1024];
	Frame *frame;
	Inst instructions[3],*target=instructions+2,*previous=instructions;
	uchar *oldfp=memory.bytes+800;
	int sa,da,fa,alias,mode,offset;
	unsigned long checks=0;
#ifdef NATIVE_CALL
	Module module={0};Inst instruction={0};
	instruction.op=ICALL;module.prog=&instruction;module.nprog=1;
	assert(compile(&module,1,nil) && module.jit->entry[ICALL]);
#endif
	for(mode=0;mode<2;mode++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(fa=0;fa<8;fa++)for(alias=0;alias<5;alias++){
#ifndef NATIVE_CALL
		if(mode)continue;
#endif
		memset(memory.bytes,0xa5,sizeof(memory.bytes));
		frame=(Frame*)(memory.bytes+512+fa);
		memset(&R,0,sizeof(R));R.s=memory.bytes+64+sa;R.d=memory.bytes+128+da;
		if(alias==1)R.d=(uchar*)frame+offsetof(Frame,lr);
		if(alias==2)R.d=(uchar*)frame+offsetof(Frame,fp);
		if(alias==3)R.s=(uchar*)frame+offsetof(Frame,lr);
		if(alias==4)R.s=(uchar*)frame+offsetof(Frame,fp);
		memmove(R.s,&frame,sizeof(frame));memmove(R.d,&target,sizeof(target));
		R.PC=previous;R.FP=oldfp;R.M=(void*)0x1234;R.MP=(void*)0x5678;R.t=17;
		memmove(expected,memory.bytes,sizeof(expected));
		offset=(uchar*)frame-memory.bytes;
		memmove(expected+offset+offsetof(Frame,lr),&previous,sizeof(previous));
		memmove(expected+offset+offsetof(Frame,fp),&oldfp,sizeof(oldfp));
#ifdef NATIVE_CALL
		if(mode)assert(module.jit->entry[ICALL](&R)==1);else
#endif
		call();
		assert(R.PC==target && R.FP==(uchar*)frame && R.M==(void*)0x1234 && R.MP==(void*)0x5678 && R.t==17);
		assert(!memcmp(memory.bytes,expected,sizeof(expected)));checks++;
	}
	for(sa=0;sa<8;sa++)for(alias=0;alias<2;alias++){
		REG saved;
		frame=alias ? H : nil;R.s=memory.bytes+64+sa;R.d=(void*)1;
		memmove(R.s,&frame,sizeof(frame));saved=R;
#ifdef NATIVE_CALL
		assert(module.jit->entry[ICALL](&R)==0 && !memcmp(&R,&saved,sizeof(R)));
#endif
		if(setjmp(boundary)==0){call();assert(0);}
		assert(!memcmp(&R,&saved,sizeof(R)));checks++;
	}
#ifdef NATIVE_CALL
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
#endif
	assert(errors==16);
	printf("PASS call slots: %lu frame/target/operand alias and alignment checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
