#include "comp-amd64.c"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include <fenv.h>
REG R;
int mutator,nprop;
char exBounds[]="bounds",exNilref[]="nil",exHeap[]="heap";
static jmp_buf boundary;
static char *raised;
static int retains,releases,destroys;
/* Keep descriptor allocation in the real compiler and executable mapping owner. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Valid code publication does not inject compiler exceptions in this fixture. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
/* Observe production VM exception identity before comparing complete memory images. */
void error(char *text) {raised=text;longjmp(boundary,1);}
/* Zero-bit pointer maps exercise C fallback traversal without owning child objects. */
void incmem(void *p,Type *t) {(void)p;assert(t->np==1 && t->map[0]==0);retains++;}
void freeptrs(void *p,Type *t) {(void)p;assert(t->np==1 && t->map[0]==0);releases++;}
/* C retains only the source array temporarily for the mapped-copy branch. */
void destroy(void *p) {assert(p!=nil && p!=H && D2H(p)->ref==11);D2H(p)->ref--;destroys++;}
/* Keep ASan stack checking without unavailable process-based leak inspection. */
int __lsan_is_turned_off(void) {return 1;}
#define OP(name) void name(void)
IMPLEMENTATION

/* Compare prefix acceptance and all C fallback variants against independent memmove images. */
int
main(void)
{
	Module module={0};Inst instruction={0},sentinel;
	struct {Heap heap;Array array;} source,dest;
	Type type={0};Array *s=&source.array,*d=&dest.array;
	union {uvlong align;uchar bytes[2048];} data;
	union {uvlong align;uchar bytes[1024];} slots;
	uchar expected[2048],saved[1024],expectedslots[1024];
	int sizes[]={0,1,2,7,8,15,16,31,32,63,64,127,128,129,255};
	int deltas[]={-32,-1,0,1,7,8,31,63,127,128,256};
	int sa,ma,da,si,di,width,mapped,index,pass,i,n,bytes,native;
	unsigned long checks=0;
	WORD start;
	uchar *sp,*dp;

	instruction.op=ISLICELA;module.prog=&instruction;module.nprog=1;
	assert(compile(&module,1,nil) && module.jit->entry[ISLICELA]);
	for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++)
	for(si=0;si<15;si++)for(di=0;di<13;di++)for(width=0;width<3;width++)
	for(mapped=0;mapped<2;mapped++)for(index=0;index<2;index++){
		n=sizes[si];bytes=n*width;start=index;
		memset(&source,0,sizeof(source));memset(&dest,0,sizeof(dest));memset(&type,0,sizeof(type));
		source.heap.ref=dest.heap.ref=10;source.heap.color=dest.heap.color=2;
		s->len=n;s->t=&type;s->root=H;d->len=300;d->t=&type;d->root=H;
		type.size=width;type.np=mapped;
		for(i=0;i<sizeof(data.bytes);i++)data.bytes[i]=(i*37+si*11)&255;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));
		sp=data.bytes+256+sa;
		dp=di<11 ? sp+deltas[di]+da : di==11 ? slots.bytes+128+ma : slots.bytes+8+sa;
		s->data=sp;d->data=dp;
		R.s=slots.bytes+8+sa;R.m=slots.bytes+128+ma;R.d=slots.bytes+768+da;
		memmove(R.s,&s,sizeof(s));memmove(R.m,&start,sizeof(start));memmove(R.d,&d,sizeof(d));
		memmove(saved,slots.bytes,sizeof(saved));memmove(expectedslots,saved,sizeof(saved));memmove(expected,data.bytes,sizeof(expected));
		if(di<11)memmove(expected+(dp-data.bytes)+index*width,sp,bytes);
		else memmove(expectedslots+(dp-slots.bytes)+index*width,sp,bytes);
		native=index==0 && n<=128 && (n==0 || (width==1 && mapped==0));
		for(pass=0;pass<2;pass++){
			for(i=0;i<sizeof(data.bytes);i++)data.bytes[i]=(i*37+si*11)&255;
			memmove(slots.bytes,saved,sizeof(saved));source.heap.ref=10;
			R.s=slots.bytes+8+sa;R.m=slots.bytes+128+ma;R.d=slots.bytes+768+da;R.PC=&sentinel;
			retains=releases=destroys=0;nprop=1;raised=nil;
			feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);
			if(pass==0){
				assert(module.jit->entry[ISLICELA](&R)==native);
				if(!native){assert(!memcmp(saved,slots.bytes,sizeof(saved)) && source.heap.ref==10 && nprop==1);continue;}
			}else slicela();
			assert(!memcmp(expected,data.bytes,sizeof(expected)) && !memcmp(expectedslots,slots.bytes,sizeof(saved)));
			assert(source.heap.ref==10 && dest.heap.ref==10 && source.heap.color==2 && dest.heap.color==2 && nprop==1);
			assert(retains==(pass && mapped && width && n ? n : 0) && releases==retains && destroys==(pass && mapped && width && n ? 1 : 0));
			assert(R.s==slots.bytes+8+sa && R.m==slots.bytes+128+ma && R.d==slots.bytes+768+da && R.PC==&sentinel);
			assert(fetestexcept(FE_ALL_EXCEPT)==FE_DIVBYZERO);
		}
		checks++;
	}
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS slice copy native: %lu byte-prefix/type/offset/overlap/alias/FP cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
