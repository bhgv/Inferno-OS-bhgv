#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>

REG R;
char exBounds[]="bounds";
static jmp_buf boundary;
static char *raised;
static int checks;

/* Preserve the production VM error choice at a bounded host exception boundary. */
void
error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}

/* No fixture heap objects need LeakSanitizer process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}
#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define A(r) (*(Array**)R.r)
IMPLEMENTATION

/* Check success and rejection for separate or aliased source/index/result slots. */
static void
probe(Array *array, WORD index, int operation, int alias, int valid, uchar *expected)
{
	union {uvlong align;uchar bytes[128];} slots;
	uchar before[128];
	WORD wanted=(WORD)(uintptr)expected;
	Inst sentinel;
	void (*functions[])(void)={indx,indb,indw,indl,indf};
	int sa,ia,oa,offset;

	for(sa=0;sa<8;sa++)for(ia=0;ia<8;ia++)for(oa=0;oa<8;oa++){
		memset(slots.bytes,0xa5,sizeof(slots.bytes));
		R.s=slots.bytes+16+sa;R.d=slots.bytes+48+ia;
		offset=alias==0 ? 88+oa : alias==1 ? 48+ia : 16+sa;
		R.m=slots.bytes+offset;R.PC=&sentinel;
		memmove(R.s,&array,sizeof(array));memmove(R.d,&index,sizeof(index));
		memmove(before,slots.bytes,sizeof(before));raised=nil;
		if(setjmp(boundary)==0){
			functions[operation]();assert(valid);
			memmove(before+offset,&wanted,sizeof(wanted));
		}else assert(!valid && raised==exBounds);
		assert(!memcmp(slots.bytes,before,sizeof(before)) && R.PC==&sentinel);
		assert(R.s==slots.bytes+16+sa && R.d==slots.bytes+48+ia && R.m==slots.bytes+offset);
		checks++;
	}
}

/* Compute ordinary addresses and zero-width identities against a byte-array oracle. */
static void
matrix(int zero)
{
	Type type={0};
	Array array={0};
	uchar bytes[512];
	int widths[]={0,1,3,7,8,16};
	int op,w,length,index,alias,width,i;

	memset(bytes,0x5a,sizeof(bytes));array.t=&type;
	for(op=0;op<5;op++)for(w=0;w<6;w++){
		width=op==0?widths[w]:op==1?1:op==2?sizeof(WORD):8;
		if((width==0)!=zero)continue;
		type.size=widths[w];
		for(length=0;length<=16;length++)for(index=-2;index<=length+2;index++)
		for(alias=0;alias<3;alias++){
			array.len=length;array.data=zero?nil:bytes;
			probe(&array,index,op,alias,index>=0 && index<length,
				zero?nil:bytes+(index>=0 && index<length?index*width:0));
		}
	}
	for(i=0;i<sizeof(bytes);i++)assert(bytes[i]==0x5a);
}

/* Reject invalid owners, descriptors, spans, large offsets and native address wrap. */
static void
invalid(void)
{
	Type type={0};
	Array array={0};
	uchar bytes[32];
	int op,alias;

	type.size=8;array.t=&type;array.len=4;array.data=bytes;
	for(op=0;op<5;op++)for(alias=0;alias<3;alias++){
		probe(H,0,op,alias,0,nil);probe(nil,0,op,alias,0,nil);
		array.data=nil;probe(&array,0,op,alias,0,nil);
		array.data=H;probe(&array,0,op,alias,0,nil);
		array.data=bytes;array.len=-1;probe(&array,0,op,alias,0,nil);array.len=4;
		probe(&array,-1,op,alias,0,nil);
		if(sizeof(WORD)==8){
			array.len=(WORD)((uvlong)1<<40);
			probe(&array,(WORD)((uvlong)1<<39),op,alias,0,nil);
			array.len=4;
		}
		array.data=(uchar*)((uintptr)-2);
		probe(&array,3,op,alias,0,nil);
		array.data=bytes;
	}
	array.t=nil;probe(&array,0,0,0,0,nil);
	array.t=H;probe(&array,0,0,0,0,nil);
	array.t=&type;type.size=-1;probe(&array,0,0,0,0,nil);
}

/* Keep descriptor and native-wrap parent counterexamples independently selectable. */
static void
rejection(int descriptor)
{
	Array array={0};
	Type type={0};
	uchar bytes[32];

	array.len=4;array.data=bytes;array.t=&type;type.size=8;
	if(descriptor){
		array.t=nil;
		probe(&array,0,0,0,0,nil);
	}else{
		array.data=(uchar*)((uintptr)-2);
		probe(&array,3,1,0,0,nil);
	}
}

/* Length reads allow nil owners and preserve signed VM length bits exactly. */
static void
lengths(void)
{
	Array array={0},*slot;
	WORD wanted,values[]={0,1,-1,17};
	union {uvlong align;uchar bytes[80];} slots;
	uchar expected[80];
	int i,sa,da;

	for(i=0;i<6;i++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++){
		slot=i==0?H:i==1?nil:&array;
		if(i>=2)array.len=values[i-2];wanted=i>=2 ? values[i-2] : 0;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+16+sa;R.d=slots.bytes+48+da;
		memmove(R.s,&slot,sizeof(slot));memmove(expected,slots.bytes,sizeof(expected));
		memmove(expected+48+da,&wanted,sizeof(wanted));lena();
		assert(!memcmp(slots.bytes,expected,sizeof(expected)));checks++;
	}
}

/* Keep faulting metadata separate from valid zero-width and ordinary programs. */
int
main(int argc,char **argv)
{
	if(argc==1 || !strcmp(argv[1],"valid"))matrix(0);
	if(argc==1 || !strcmp(argv[1],"zero"))matrix(1);
	if(argc==1 || !strcmp(argv[1],"invalid"))invalid();
	if(argc==1 || !strcmp(argv[1],"length"))lengths();
	if(argc>1 && !strcmp(argv[1],"descriptor"))rejection(1);
	if(argc>1 && !strcmp(argv[1],"wrap"))rejection(0);
	printf("PASS array indices: %d bounds/address/zero/alias checks, WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
