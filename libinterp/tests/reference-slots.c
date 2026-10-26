#ifdef NATIVE_REFERENCE
#include "comp-amd64.c"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
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
#define OP(name) void name(void)
#define P(r) (*(WORD**)R.r)
#define T(r) (*(void**)R.r)
int mutator=1,nprop;
char exHeap[]="heap";
char exNilref[]="nil",exTcheck[]="type",exModule[]="module";
static jmp_buf boundary;
static char *raised;
/* Preserve exact VM error identities at a bounded fixture exception boundary. */
void error(char *message) {raised=message;longjmp(boundary,1);}
/* References stay nonzero; accounting is checked before releasing fixture blocks. */
void destroy(void *value) {if(value!=H){assert(value!=nil && D2H(value)->ref>1);D2H(value)->ref--;}}
/* Keep stack checking without unsupported leak-process inspection. */
int __lsan_is_turned_off(void) {return 1;}

#ifdef NATIVE_REFERENCE
/* Use the production compiler with ordinary owned descriptor allocation. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Valid compiler publication does not raise in this fixture. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
#endif
IMPLEMENTATION

/* Check reference and marking deltas separately from destination byte effects. */
int
main(void)
{
	Heap *heaps[3];Type types[2]={{0},{0}};
	void *values[5]={H,nil,nil,nil,nil},*source,*old,*wanted;
	void (*ops[])(void)={movp,tcmp,self};
	union {uvlong align;uchar bytes[96];} slots;
	uchar expected[96];
	Inst sentinel;
	ulong refs[3];
	int op,a,b,alias,sa,da,color,i,valid,src,dst,marked;
	char *failure;
	unsigned long checks=0;
#ifdef NATIVE_REFERENCE
	Module module={0};Inst instruction={0};
	instruction.op=ITCMP;module.prog=&instruction;module.nprog=1;
	assert(compile(&module,1,nil) && module.jit->entry[ITCMP]);
#endif

	for(i=0;i<3;i++){heaps[i]=calloc(1,sizeof(Heap)+sizeof(Modlink));assert(heaps[i]);heaps[i]->t=&types[i==2];values[i+1]=H2D(void*,heaps[i]);}
	for(op=0;op<3;op++)for(a=0;a<5;a++)for(b=0;b<5;b++)for(alias=0;alias<2;alias++)
	for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(color=0;color<3;color++){
		source=values[a];old=alias ? source : values[b];
		if(op!=1 && old==nil && !(op==0 && source==nil) && !(op==2 && (source==nil || source==H)))continue;
		valid=1;failure=nil;src=dst=-1;marked=0;
		for(i=0;i<3;i++){heaps[i]->ref=refs[i]=10;heaps[i]->color=color;if(source==values[i+1])src=i;if(old==values[i+1])dst=i;}
		if(op==0 && source==nil){valid=0;failure=exNilref;}
		if(op==1 && source!=H && (source==nil || old==nil || old==H || D2H(source)->t!=D2H(old)->t)){valid=0;failure=exTcheck;}
		if(op==2 && (source==nil || source==H)){valid=0;failure=exModule;}
		wanted=source;
		if(valid && op!=1){if(src>=0){refs[src]++;marked=color!=mutator;}if(dst>=0)refs[dst]--;}
		memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+8+sa;R.d=alias ? R.s : slots.bytes+48+da;
		memmove(R.s,&source,sizeof(source));memmove(R.d,&old,sizeof(old));R.M=source;R.PC=&sentinel;
		memmove(expected,slots.bytes,sizeof(expected));if(valid && op!=1)memmove(expected+((uchar*)R.d-slots.bytes),&wanted,sizeof(wanted));
		raised=nil;nprop=0;
#ifdef NATIVE_REFERENCE
		if(op==1){
			assert(module.jit->entry[ITCMP](&R)==valid);
			assert(!memcmp(expected,slots.bytes,sizeof(expected)) && R.PC==&sentinel && R.M==source);
			assert(nprop==0);
			for(i=0;i<3;i++)assert(heaps[i]->ref==refs[i] && heaps[i]->color==color);
		}
#endif
		if(setjmp(boundary)==0){ops[op]();assert(valid);}else assert(!valid && raised==failure);
		assert(!memcmp(expected,slots.bytes,sizeof(expected)) && R.PC==&sentinel && R.M==source);
		assert(nprop==marked);
		for(i=0;i<3;i++){assert(heaps[i]->ref==refs[i]);assert(heaps[i]->color==(valid && op!=1 && i==src && color!=mutator ? propagator : color));}
		checks++;
	}
#ifdef NATIVE_REFERENCE
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
#endif
	for(i=0;i<3;i++)free(heaps[i]);
	printf("PASS reference slots: %lu ownership/mark/alignment/error checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
