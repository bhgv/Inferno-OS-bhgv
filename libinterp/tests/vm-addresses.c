#ifdef NATIVE_PC
#include "comp-amd64.c"
#else
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>

REG R;
/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;
#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define T(r) (*(void**)R.r)
char exModule[]="module",exBounds[]="bounds";
static jmp_buf boundary;
static char *raised;

/* Keep the VM exception identity and return to the fixture boundary. */
void
error(char *message)
{
	raised=message;longjmp(boundary,1);
}
/* Retain stack/address instrumentation without unsupported leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

#ifdef NATIVE_PC
/* Native compiler fixtures use ordinary owned allocation and valid descriptors. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Compilation does not raise in these valid publication cases. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
#endif
IMPLEMENTATION

/* Exercise pointer-width stores separately from VM-word address representations. */
int
main(int argc, char **argv)
{
	union {uvlong align;uchar bytes[128];} memory;
	uchar expected[128],initial[128];
	Module module;
	Modlink link;
	Inst program[4],sentinel,*target;
	WORD index,address;
	void *sources[]={nil,(void*)1,H,memory.bytes,(void*)(uintptr)0x123456789abcULL};
	int i,s,d,p,k,which;
	unsigned long checks=0;

	memset(&module,0,sizeof(module));memset(&link,0,sizeof(link));
	memset(program,0,sizeof(program));for(i=0;i<4;i++)program[i].op=IMOVPC;
	program[3].op=ILEA;
	module.prog=program;module.nprog=4;link.m=&module;R.M=&link;R.PC=&sentinel;
#ifdef NATIVE_PC
	assert(compile(&module,4,nil) && module.jit->entry[IMOVPC] && module.jit->entry[ILEA]);
#endif
	if(argc>1){
		which=atoi(argv[1]);assert(which>=0 && which<11);
		for(s=0;s<8;s++)for(d=0;d<8;d++){
			R.M=&link;link.m=&module;module.prog=program;module.nprog=4;index=0;
			memset(memory.bytes,0xa5,sizeof(memory.bytes));
			R.s=memory.bytes+16+s;R.d=memory.bytes+64+d;
			if(which==0)R.M=nil;
			if(which==1)R.M=H;
			if(which==2)link.m=nil;
			if(which==3)link.m=H;
			if(which==4)index=-1;
			if(which==5)index=4;
			if(which==6)module.prog=nil;
			if(which==7)module.nprog=-1;
			if(which==8)module.prog=H;
			if(which==9){module.prog=(Inst*)((uintptr)-1-8);index=1;}
			if(which==10){index=(WORD)((uvlong)1<<(sizeof(WORD)*8-1));}
			memmove(R.s,&index,sizeof(index));memmove(expected,memory.bytes,sizeof(expected));
			if(which<4)R.s=(void*)1;
			R.d=(void*)1;
			raised=nil;
#ifdef NATIVE_PC
			assert(module.jit->entry[IMOVPC](&R)==0);
#endif
			if(setjmp(boundary)==0){movpc();assert(0);}
			assert(raised==(which<4 ? exModule : exBounds));
			assert(R.PC==&sentinel && !memcmp(memory.bytes,expected,sizeof(expected)));
			checks++;
		}
	}else{
		for(i=0;i<4;i++)for(s=0;s<8;s++)for(d=0;d<8;d++)for(p=0;p<4;p++){
			index=i;target=program+i;
			memset(memory.bytes,0xa5,sizeof(memory.bytes));
			R.s=memory.bytes+16+s;R.m=(void*)1;
			R.d=p==0 ? memory.bytes+64+d : p==1 ? R.s : p==2 ? (uchar*)R.s+1 : (uchar*)R.s-1;
			memmove(R.s,&index,sizeof(index));memmove(expected,memory.bytes,sizeof(expected));memmove(initial,memory.bytes,sizeof(initial));
			memmove(expected+((uchar*)R.d-memory.bytes),&target,sizeof(target));

#ifdef NATIVE_PC
			assert(module.jit->entry[IMOVPC](&R)==1 && !memcmp(memory.bytes,expected,sizeof(expected)));
			memmove(memory.bytes,initial,sizeof(initial));
#endif
			movpc();
			assert(!memcmp(memory.bytes,expected,sizeof(expected)) && R.PC==&sentinel && R.M==&link);
			checks++;
		}
		for(i=0;i<nelem(sources);i++)for(d=0;d<8;d++){
			memset(memory.bytes,0xa5,sizeof(memory.bytes));
			R.s=sources[i];R.m=(void*)1;R.d=memory.bytes+64+d;
			address=(WORD)R.s;
			memmove(expected,memory.bytes,sizeof(expected));memmove(expected+64+d,&address,sizeof(address));
#ifdef NATIVE_PC
			assert(module.jit->entry[ILEA](&R)==1);
			assert(!memcmp(memory.bytes,expected,sizeof(expected)) && R.PC==&sentinel && R.s==sources[i]);
			memset(memory.bytes,0xa5,sizeof(memory.bytes));
#endif
			lea();
			assert(!memcmp(memory.bytes,expected,sizeof(expected)) && R.PC==&sentinel && R.s==sources[i]);
			checks++;
		}
	}
#ifdef NATIVE_PC
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
#endif
	printf("PASS VM addresses: %lu alignment/alias/bounds checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
