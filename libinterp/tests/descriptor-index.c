#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>

REG R;
#define OP(name) void name(void)
char exBounds[]="bounds";
/* These descriptor cases have already passed the independent bounds oracle. */
void error(char *message) { (void)message;assert(0); }
/* Retain stack instrumentation without unsupported leak-process inspection. */
int __lsan_is_turned_off(void) {return 1;}

IMPLEMENTATION

/* Check exact byte effects for arbitrary descriptor strides and zero-width identities. */
int
main(void)
{
	Type type={0};Array array={0},*owner=&array;
	union {uvlong align;uchar bytes[160];} slots;
	uchar expected[160],initial[160],backing[256];
	uintptr bases[]={(uintptr)backing,0,(uintptr)H,(uintptr)-32};
	int widths[]={0,1,3,7,8,16,255,0x7fffffff};
	WORD index,wanted;
	Inst sentinel;
	REG saved=R;
	int w,i,b,sa,ia,placement,d,k;
	unsigned long checks=0;
#ifdef NATIVE_DESCRIPTOR
	Module module;Inst code[2];
	memset(&module,0,sizeof(module));memset(code,0,sizeof(code));
	code[0].op=IINDX;code[1].op=IRUNT;module.prog=code;module.nprog=2;
	assert(compile(&module,2,nil));
	assert(module.jit->entry[IINDX]);
#endif
	array.t=&type;array.len=8;
	for(w=0;w<nelem(widths);w++)for(i=0;i<8;i++)for(b=0;b<nelem(bases);b++){
		type.size=widths[w];index=i;array.data=(uchar*)bases[b];
		if(type.size && (bases[b]==0 || bases[b]==(uintptr)H ||
		   (uvlong)i*type.size>0x7fffffff || bases[b]>(uintptr)-1-(uintptr)i*type.size))continue;
		wanted=(WORD)(bases[b]+(uintptr)i*type.size);
		for(sa=0;sa<8;sa++)for(ia=0;ia<8;ia++)for(placement=0;placement<12;placement++){
			for(k=0;k<sizeof(slots.bytes);k++)slots.bytes[k]=(uchar)(k*37+11);
			R.s=slots.bytes+16+sa;R.d=slots.bytes+56+ia;
			d=placement<8 ? 104+placement : placement==8 ? 16+sa : placement==9 ? 56+ia : placement==10 ? 17+sa : 55+ia;
			R.m=slots.bytes+d;R.PC=&sentinel;
			memmove(R.s,&owner,sizeof(owner));memmove(R.d,&index,sizeof(index));
			memmove(initial,slots.bytes,sizeof(initial));memmove(expected,initial,sizeof(expected));
			memmove(expected+d,&wanted,sizeof(wanted));
#ifdef NATIVE_DESCRIPTOR
			{
				assert(module.jit->entry[IINDX](&R)==1 && !memcmp(slots.bytes,expected,sizeof(expected)));
				assert(R.PC==&sentinel && R.s==slots.bytes+16+sa && R.d==slots.bytes+56+ia && R.m==slots.bytes+d);
				memmove(slots.bytes,initial,sizeof(initial));
			}
#endif
			indx();
			assert(!memcmp(slots.bytes,expected,sizeof(expected)));
			assert(R.PC==&sentinel && R.s==slots.bytes+16+sa && R.d==slots.bytes+56+ia && R.m==slots.bytes+d);
			checks++;
		}
	}
#ifdef NATIVE_DESCRIPTOR
	dropmodule(&module);
#endif
	R=saved;
	printf("PASS descriptor index: %lu stride/alignment/alias/empty checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
