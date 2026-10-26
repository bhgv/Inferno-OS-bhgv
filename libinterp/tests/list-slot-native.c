#include <lib9.h>
#include "isa.h"
#include "interp.h"
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>

#define OP(name) static void name(void) // Bind extracted production handlers locally.
REG R;
static jmp_buf boundary; // Owned error boundary for one production call.
static char *raised; // Exact VM error delivered by the production handler.
char exHeap[]="heap", exBounds[]="bounds", exNilref[]="nil", exTcheck[]="type";


/* Report VM errors through the current fixture boundary without disabling checks. */
void error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}


/* No fixture allocations survive a check or require process-based leak inspection. */
int __lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION


/* Keep longjmp within this frame, preserving caller loop values and snapshots. */
static char*
invoke(void (*operation)(void))
{
	raised=nil;
	if(setjmp(boundary)==0)
		operation();
	return raised;
}


/* Check exact errors and byte-for-byte failure atomicity, including decoded slots. */
static void
check(void (*operation)(void), char *expected, uchar *image, int length)
{
	uchar before[1024];
	REG saved;
	char *actual;

	if(length > sizeof(before))
		abort();
	memmove(before,image,length);
	memmove(&saved,&R,sizeof(R));
	actual=invoke(operation);
	if(actual!=expected || memcmp(before,image,length) || memcmp(&saved,&R,sizeof(R))){
		fprintf(stderr,"list-slot error mismatch: expected %s got %s\n",
			expected ? expected : "success",actual ? actual : "success");
		abort();
	}
}


/* Compare successful byte copies against a captured pre-execution source image. */
static void
checkresult(void (*operation)(void), uchar *image, uchar *expected, int length)
{
	REG saved;

	memmove(&saved,&R,sizeof(R));
	if(invoke(operation)!=nil || memcmp(image,expected,length) || memcmp(&saved,&R,sizeof(R)))
		abort();
}


/* Exercise malformed decoded slots, unused zero-copy operands and List spans.
 * Stack storage is deliberately unaligned; no guessed address is dereferenced. */
int main(void)
{
	void (*operations[])(void)={headb,headw,headl,headf,lenl,headm,movm,tcmp};
	uchar image[1024], expectedimage[1024];
	List *list;
	void *owner;
	WORD count;
	uintptr address;
	int op,side,width,i,shift,profile,checks,alignment,position;
	uvlong value;
	char *expected;

	checks=0;
	for(op=0;op<8;op++) for(side=0;side<3;side++){
		if(getenv("LIST_SLOT_OP") && op!=atoi(getenv("LIST_SLOT_OP"))) continue;
		if(side==0 && op==6) continue; // MOVM uses source bytes, not an owner slot.
		if(side==1 && (op==5 || op==6)) continue; // Copy destinations depend on count.
		if(side==2 && op!=5 && op!=6) continue; // Other operations ignore REG.m.
		width=side==2 ? sizeof(WORD) : side==0 || op==7 ? sizeof(void*) :
			op==0 ? 1 : op==1 || op==4 ? sizeof(WORD) : 8;
		for(i=0;i<width+1;i++) for(shift=0;shift<8;shift++){
			memset(image,0xa5,sizeof(image));
			memset(&R,0,sizeof(R));
			R.s=image+32+shift; R.d=image+96+shift; R.m=image+160+shift;
			list=(List*)(image+512+shift);
			owner=op==7 ? H : (void*)list;
			memmove(R.s,&owner,sizeof(owner));
			count=0; memmove(R.m,&count,sizeof(count));
			address=i==0 ? 0 : (uintptr)-1-(i-1);
			if(side==0) R.s=(void*)address;
			if(side==1) R.d=(void*)address;
			if(side==2) R.m=(void*)address;
			expected=op==7 ? exTcheck : side==0 && op!=4 ? exHeap : exBounds;
			check(operations[op],expected,image,sizeof(image));
			checks++;
		}
	}
	/* Invalid results must be rejected without reading an otherwise unused address 1. */
	for(op=0;op<5;op++) for(i=0;i<2;i++){
		memset(&R,0,sizeof(R)); R.s=(void*)1; R.d=i ? H : nil;
		check(operations[op],exBounds,image,sizeof(image)); checks++;
	}
	/* Empty raw copies ignore data/destination identities; HEADM still captures its owner. */
	for(op=5;op<=6;op++) for(profile=0;profile<4;profile++) for(shift=0;shift<8;shift++){
		memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
		R.s=image+32+shift; R.m=image+160+shift;
		owner=(void*)1; memmove(R.s,&owner,sizeof(owner));
		count=profile<2 ? 0 : -1; memmove(R.m,&count,sizeof(count));
		if(op==6) R.s=profile&1 ? H : nil;
		R.d=profile&1 ? H : nil;
		check(operations[op],count==0 ? nil : exBounds,image,sizeof(image)); checks++;
	}
	/* Bound the entire selected scalar payload before forming or reading its address. */
	for(op=0;op<4;op++) for(shift=0;shift<8;shift++){
		width=op==0 ? 1 : op==1 ? sizeof(WORD) : 8;
		for(i=0;i<offsetof(List,data)+width;i++){
			memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
			R.s=image+32+shift; R.d=image+96+shift;
			owner=(void*)((uintptr)-1-i); memmove(R.s,&owner,sizeof(owner));
			check(operations[op],i==0 ? exNilref : exBounds,image,sizeof(image)); checks++;
		}
	}
	/* Preserve exact source bits across unaligned and overlapping scalar head copies. */
	for(op=0;op<4;op++) for(shift=0;shift<8;shift++)
	for(alignment=0;alignment<8;alignment++) for(position=0;position<3;position++)
	for(profile=0;profile<4;profile++){
		memset(image,0xa5,sizeof(image)); memset(&R,0,sizeof(R));
		for(i=0;i<sizeof(image);i++) image[i]=i*37+11;
		R.s=image+32+shift; R.m=(void*)1;
		list=(List*)(image+512+shift); owner=list;
		memmove(R.s,&owner,sizeof(owner));
		width=op==0 ? 1 : op==1 ? sizeof(WORD) : 8;
		value=profile==0 ? 0 : profile==1 ? (uvlong)-1 :
			profile==2 ? 0x8000000080000080ULL : 0x0123456789abcdefULL;
		memmove(image+512+shift+offsetof(List,data),&value,width);
		R.d=image+(position==0 ? 96 : position==1 ? 32 : 512+shift+offsetof(List,data)-8)+alignment;
		memmove(expectedimage,image,sizeof(image));
		memmove(expectedimage+((uchar*)R.d-image),image+512+shift+offsetof(List,data),width);
		checkresult(operations[op],image,expectedimage,sizeof(image)); checks++;
	}
	printf("PASS C list/copy/type slot preflight: %d cases WORD%d\n",checks,(int)sizeof(WORD)*8);
	return 0;
}
