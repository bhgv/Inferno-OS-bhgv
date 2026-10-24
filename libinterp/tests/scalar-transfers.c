#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>

REG R;
#define OP(name) void name(void)
#define B(r) (*(BYTE*)R.r)
#define W(r) (*(WORD*)R.r)
#define V(r) (*(LONG*)R.r)
#define F(r) (*(REAL*)R.r)
#define SH(r) (*(SHORT*)R.r)

/* Keep stack instrumentation while avoiding unsupported leak process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Construct independent byte oracles before any overlapping store can change input. */
int
main(void)
{
	void (*ops[])(void) = {movb,movw,movl,movf,cvtbw,cvtwb,cvtws,cvtsw,cvtwl,cvtlw};
	int sizes[] = {1,sizeof(WORD),8,8,sizeof(WORD),1,2,sizeof(WORD),8,sizeof(WORD)};
	uvlong patterns[] = {0,1,127,128,255,256,0x8000,0xffff,0x7fffffff,0x80000000,
		0xffffffff,0x100000000ULL,0x8000000000000000ULL,~0ULL,
		0x7ff0000000000042ULL,0xfff8000000000042ULL};
	union { uvlong align; uchar bytes[128]; } memory;
	uchar expected[128], output[8], byte;
	WORD word, convertedword;
	SHORT shortword, convertedshort;
	LONG big, convertedbig;
	Inst sentinel;
	int op,i,s,d,k;
	unsigned long checks=0;

	for(op=0; op<nelem(ops); op++)
	for(i=0; i<nelem(patterns); i++)
	for(s=24; s<32; s++)
	for(d=16; d<48; d++){
		for(k=0; k<sizeof(memory.bytes); k++) memory.bytes[k]=(uchar)(k*37+11);
		memmove(memory.bytes+s,&patterns[i],8);
		memmove(expected,memory.bytes,sizeof(expected));
		memmove(&word,expected+s,sizeof(word));
		memmove(&shortword,expected+s,sizeof(shortword));
		memmove(&big,expected+s,sizeof(big));
		switch(op){
		case 4: convertedword=expected[s]; memmove(output,&convertedword,sizeof(convertedword)); break;
		case 5: byte=word; memmove(output,&byte,1); break;
		case 6: convertedshort=word; memmove(output,&convertedshort,2); break;
		case 7: convertedword=shortword; memmove(output,&convertedword,sizeof(convertedword)); break;
		case 8: convertedbig=word; memmove(output,&convertedbig,8); break;
		case 9: convertedword=big; memmove(output,&convertedword,sizeof(convertedword)); break;
		default: memmove(output,expected+s,sizes[op]); break;
		}
		memmove(expected+d,output,sizes[op]);
		R.s=memory.bytes+s; R.d=memory.bytes+d; R.m=(void*)1; R.PC=&sentinel;
		ops[op]();
		assert(!memcmp(memory.bytes,expected,sizeof(expected)));
		assert(R.PC==&sentinel && R.s==memory.bytes+s && R.d==memory.bytes+d && R.m==(void*)1);
		checks++;
	}
	printf("PASS scalar transfers: %lu alignment/overlap checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
