#include "comp-amd64.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
/* Supply the builtin identity used by guarded numeric formatting leaves. */
Type Tstring = {1}, Tarray = {1};
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

/* Execute the exact arrayspan INT_MAX boundary, wide empty indexes and nil-source rules. */
static unsigned long
indexedges(Module *module)
{
    struct {Heap heap;Array array;} src,dst;
    Type type={0};Array *source,*dest;
    union {uvlong align;uchar bytes[96];} slots;
    uchar savedslots[96],data[1024],expected[1024];
    WORD index;
    REG saved;
    char *failure;
    int profile,shift,pass,i,valid;
    unsigned long checks=0;

    for(profile=0;profile<12;profile++)for(shift=0;shift<8;shift++){
        memset(&src,0,sizeof(src));memset(&dst,0,sizeof(dst));
        src.heap.ref=dst.heap.ref=10;type.size=1;type.np=0;
        source=&src.array;dest=&dst.array;index=0;
        src.array.len=1;src.array.t=&type;src.array.root=H;src.array.data=data+128;
        dst.array.len=300;dst.array.t=&type;dst.array.root=H;dst.array.data=data+256;
        valid=profile==0 || profile==2 || profile==4 || profile==5 || profile==10 || profile==11;
        failure=exBounds;
        if(profile==0){index=0x7ffffffe;dst.array.len=0x7fffffff;}
        if(profile==1){index=0x7fffffff;dst.array.len=(WORD)((uvlong)0x7fffffff+1);}
        if(profile==2){index=sizeof(WORD)==8 ? (WORD)((uvlong)1<<32) : (WORD)0x7fffffff;dst.array.len=index;src.array.len=0;}
        if(profile==3){index=0x7fffffff-127;src.array.len=128;dst.array.len=(WORD)((uvlong)0x7fffffff+1);}
        if(profile==4){index=0x7fffffff-128;src.array.len=128;dst.array.len=0x7fffffff;}
        if(profile<=4)dst.array.data=(uchar*)((uintptr)data+256-(uintptr)index);
        if(profile==5){index=0x7fffffff;dst.array.len=index;src.array.len=0;dst.array.t=H;dst.array.data=H;}
        if(profile==6)index=-1;
        if(profile==7)src.array.len=-1;
        if(profile==8)dst.array.t=H;
        if(profile==9){index=1;dst.array.data=H;}
        if(profile==10 || profile==11){source=profile==10 ? nil : H;dest=H;index=-1;}
        memset(slots.bytes,0xa5,sizeof(slots.bytes));memset(&R,0,sizeof(R));
        R.s=slots.bytes+8+shift;R.m=slots.bytes+40+shift;R.d=slots.bytes+72+shift;
        memmove(R.s,&source,sizeof(source));memmove(R.m,&index,sizeof(index));memmove(R.d,&dest,sizeof(dest));
        saved=R;memmove(savedslots,slots.bytes,sizeof(savedslots));
        for(i=0;i<sizeof(data);i++)data[i]=(i*37+profile*11)&255;
        memmove(expected,data,sizeof(data));
        if(valid && source!=nil && source!=H && src.array.len)memmove(expected+256,data+128,src.array.len);
        for(pass=0;pass<2;pass++){
            for(i=0;i<sizeof(data);i++)data[i]=(i*37+profile*11)&255;
            R=saved;retains=releases=destroys=0;nprop=1;raised=nil;
            if(pass==0)assert(module->jit->entry[ISLICELA](&R)==valid);
            else if(setjmp(boundary)==0){slicela();assert(valid);}
            else assert(!valid && raised==failure);
            assert(!memcmp(data,expected,sizeof(data)) && !memcmp(savedslots,slots.bytes,sizeof(savedslots)));
            assert(!memcmp(&saved,&R,sizeof(R)) && src.heap.ref==10 && dst.heap.ref==10 && !retains && !releases && !destroys && nprop==1);
        }
        checks++;
    }
    return checks;
}

/* Compare slice acceptance and all C fallback variants against independent memmove images. */
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
	for(mapped=0;mapped<2;mapped++)for(index=0;index<8;index++){
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
		native=n<=128 && (n==0 || (width==1 && mapped==0));
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
	checks+=indexedges(&module);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS slice copy native: %lu byte-prefix/type/offset/overlap/alias/FP cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
