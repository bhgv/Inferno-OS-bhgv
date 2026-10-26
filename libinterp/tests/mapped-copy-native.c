#include "comp-amd64.c"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>
REG R;
int mutator,nprop;
char exModule[]="module",exNilref[]="nil",exHeap[]="heap";
static jmp_buf boundary;
static char *raised;
static int retained,released;

/* Allocate only production compiler descriptors; executable ownership stays in the adapter. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Compilation is valid and does not inject exceptions in this differential fixture. */
int kprotect(int (*f)(void*),void *arg) {return f(arg);}
/* Keep an exact bounded exception observation around the extracted C handlers. */
void error(char *message) {raised=message;longjmp(boundary,1);}
/* Non-pointer records have no mapped owners; reject accidental fixture broadening. */
void incmem(void *value,Type *t) {(void)value;assert(t->np==0);retained++;}
void freeptrs(void *value,Type *t) {(void)value;(void)t;assert(0);}
/* The C head copy releases only its temporary source-list retention. */
void destroy(void *value) {assert(value!=nil && value!=H && D2H(value)->ref>1);D2H(value)->ref--;released++;}
/* Keep sanitizer stack checks without unavailable process-based leak inspection. */
int __lsan_is_turned_off(void) {return 1;}
#define OP(name) void name(void)
IMPLEMENTATION

/* Observe exact C failures before any data, reference, operand or propagation change. */
static unsigned long
mappederrors(Module *code)
{
	Module module={0};Modlink link={0};Type type={0};Type *table[2]={&type,&type};
	union {uvlong align;uchar bytes[512];} slots;
	uchar expected[512],block[sizeof(Heap)+sizeof(List)+256],before[sizeof(block)];
	Heap *heap;List *list,*owner;
	REG saved;
	WORD index;
	char *failure,*selection=getenv("MAPPED_ERROR_CASE");
	int op,profile,shift,pass,status;
	unsigned long checks=0;
	void (*handlers[])(void)={movmp,headmp};int ops[]={IMOVMP,IHEADMP};

	/* The union's alignment is also required for the source heap header. */
	union {uvlong align;uchar bytes[sizeof(block)];} aligned;
	heap=(Heap*)aligned.bytes;list=H2D(List*,heap);
	for(op=0;op<2;op++)for(profile=0;profile<19;profile++)for(shift=0;shift<8;shift++){
		if(selection && atoi(selection)!=profile)continue;
		if(!op && profile>=14 && profile<=17)continue;
		memset(aligned.bytes,0xa5,sizeof(aligned.bytes));heap->ref=10;heap->color=2;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));memset(&R,0,sizeof(R));
		type.size=1;type.np=0;table[1]=&type;module.type=table;module.ntype=2;link.m=&module;
		owner=list;index=1;failure="invalid type";
		R.M=&link;R.s=op ? slots.bytes+8+shift : (void*)list->data;R.m=slots.bytes+128+shift;R.d=slots.bytes+256+shift;
		if(profile==0){type.size=0;type.np=1;heap->ref=0;}
		if(profile==1){R.M=nil;failure=exModule;}
		if(profile==2){R.M=H;failure=exModule;}
		if(profile==3){link.m=nil;failure=exModule;}
		if(profile==4){link.m=H;failure=exModule;}
		if(profile==5)module.type=nil;
		if(profile==6)module.type=H;
		if(profile==7)module.ntype=0;
		if(profile==8)module.ntype=-1;
		if(profile==9)index=-1;
		if(profile==10)index=2;
		if(profile==11)table[1]=nil;
		if(profile==12)table[1]=H;
		if(profile==13)type.size=-1;
		if(profile==14){owner=H;failure=exNilref;}
		if(profile==15){owner=nil;failure=exNilref;}
		if(profile==16){heap->ref=0;failure=exHeap;}
		if(profile==17){heap->ref=(ulong)-1;failure=exHeap;}
		if(profile==18)type.np=1;
		if(op)memmove(R.s,&owner,sizeof(owner));
		memmove(R.m,&index,sizeof(index));saved=R;
		memmove(expected,slots.bytes,sizeof(expected));memmove(before,aligned.bytes,sizeof(before));
		for(pass=0;pass<2;pass++){
			R=saved;nprop=1;retained=released=0;raised=nil;
			if(pass==0){
				status=code->jit->entry[ops[op]](&R);assert(status==(profile==0));
			}else if(profile!=18){
				if(setjmp(boundary)==0){handlers[op]();assert(profile==0);}
				else assert(profile!=0 && !strcmp(raised,failure));
			}
			assert(!memcmp(&saved,&R,sizeof(R)) && !memcmp(expected,slots.bytes,sizeof(expected)));
			assert(!memcmp(before,aligned.bytes,sizeof(before)) && retained==0 && released==0 && nprop==1);
		}
		checks++;
	}
	return checks;
}

/* Check whole memory images, aliases and moving live tables against libc memmove and C. */
int
main(void)
{
	Module module={0},owner={0};Modlink link={0};Inst instructions[2]={{0}},sentinel;
	Type record={0},stale={0};Type *first[2]={&stale,&record},*second[2]={&stale,&record};
	int ops[]={IMOVMP,IHEADMP};void (*handlers[])(void)={movmp,headmp};
	int sizes[]={0,1,2,7,8,15,16,31,32,63,64,127,128,129,255,256,257};
	int deltas[]={-8,-1,0,1,7,8,31,63,127,128,256};
	union {uvlong align;uchar bytes[512];} slots;
	uchar before[512],expectedslots[512],*block,*expected,*source,*destination,*wanted;
	Heap *heap;List *list;
	WORD index=1;
	int op,sa,da,ma,si,di,pass,n,i,valid;
	unsigned long checks=0;
	ulong length=sizeof(Heap)+sizeof(List)+1024;

	block=malloc(length);expected=malloc(length);assert(block && expected);
	heap=(Heap*)block;list=H2D(List*,heap);
	instructions[0].op=IMOVMP;instructions[1].op=IHEADMP;module.prog=instructions;module.nprog=2;
	assert(compile(&module,2,nil) && module.jit->entry[IMOVMP] && module.jit->entry[IHEADMP]);
	link.m=&owner;link.type=first;owner.ntype=2;
	if(getenv("MAPPED_ERROR_ONLY"))goto errors;
	for(op=0;op<2;op++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(ma=0;ma<8;ma++)
	for(si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++)for(di=0;di<13;di++){
		n=sizes[si];record.size=n;record.np=0;stale.size=257;
		owner.type=(checks&1) ? first : second;
		/* The cached link table points at an incompatible descriptor on every call. */
		link.type=(checks&1) ? second : first;link.type[1]=&stale;owner.type[1]=&record;
		for(i=0;i<length;i++)block[i]=(i*37+si*11)&255;
		heap->ref=10;heap->color=2;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));
		source=(uchar*)list->data+(op ? 0 : 128+sa);
		destination=di<11 ? source+deltas[di]+da : di==11 ? slots.bytes+128+ma : op ? slots.bytes+8+sa : source;
		R.s=op ? slots.bytes+8+sa : source;R.m=slots.bytes+128+ma;R.d=destination;
		if(op)memmove(R.s,&list,sizeof(list));
		memmove(R.m,&index,sizeof(index));memmove(before,slots.bytes,sizeof(before));
		memmove(expectedslots,before,sizeof(before));memmove(expected,block,length);
		wanted=op && n ? (uchar*)list->data : R.s;
		if((uintptr)destination>=(uintptr)block && (uintptr)destination<(uintptr)block+length)
			memmove(expected+(destination-block),source,n);
		else
			memmove(expectedslots+(destination-slots.bytes),source,n);
		for(pass=0;pass<2;pass++){
			for(i=0;i<length;i++)block[i]=(i*37+si*11)&255;
			heap->ref=10;heap->color=2;memmove(slots.bytes,before,sizeof(before));
			R.M=&link;R.PC=&sentinel;R.s=op ? slots.bytes+8+sa : source;R.d=destination;R.m=slots.bytes+128+ma;
			retained=released=0;nprop=1;raised=nil;
			feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);
			if(pass==0){
				valid=n<=256;
				if(module.jit->entry[ops[op]](&R)!=valid)abort();
				if(!valid){assert(!memcmp(before,slots.bytes,sizeof(before)) && heap->ref==10 && R.s==(op ? slots.bytes+8+sa : source));continue;}
			}else handlers[op]();
			assert(!memcmp(expected,block,length) && !memcmp(expectedslots,slots.bytes,sizeof(before)));
			assert(R.s==wanted && R.M==&link && R.PC==&sentinel && R.d==destination && R.m==slots.bytes+128+ma);
			assert(retained==(pass && n!=0) && released==(pass && op && n!=0) && nprop==1);
			assert(fetestexcept(FE_ALL_EXCEPT)==FE_DIVBYZERO);
		}
		checks++;
	}
	errors:
	checks+=mappederrors(&module);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);free(block);free(expected);
	printf("PASS mapped copy native: %lu live-type/size/alias/overlap/FP cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
