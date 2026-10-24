#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
REG R;
Type Tstring={1};
char exNilref[]="nil",exType[]="type",exNomem[]="memory";
static Prog current;
static jmp_buf boundary;
static char *raised;
static int allocationfailure;
/* Preserve the actual VM error or string-message pointer for ownership assertions. */
void error(char *text) {raised=text;longjmp(boundary,1);}
/* Use one stable VM process so publication can be checked after every error. */
Prog *currun(void) {return &current;}
/* Force refreshes to exercise replacement and failed cache allocations. */
ulong msize(void *p) {(void)p;return 0;}
/* Fail UTF-8 cache preparation before any exception owner can be published. */
static void *cachemalloc(ulong n) {return allocationfailure ? nil : malloc(n);}
/* Keep stack checking enabled while avoiding unsupported leak inspection. */
int __lsan_is_turned_off(void) {return 1;}
#define OP(name) void name(void)
#define T(r) (*(void**)R.r)
#define malloc cachemalloc
IMPLEMENTATION
#undef malloc

/* Construct real heap-prefixed strings without replacing production conversion logic. */
static String*
textstring(int wide)
{
	Heap *h=calloc(1,sizeof(Heap)+sizeof(String)+64);
	String *s;
	assert(h);h->ref=1;h->t=&Tstring;s=H2D(String*,h);
	if(wide){s->len=-2;s->Srune[0]='e';s->Srune[1]='r';}
	else{s->len=2;memmove(s->Sascii,"er",2);}
	return s;
}

/* Validate tuple descriptors, owner slots, preparation failures and retained references. */
int
main(void)
{
	uchar slot[32];
	String *message,*s;
	Heap *tuple;
	Type type={0};
	void *value;
	REG saved;
	int alignment,mode,fail,accepted;
	unsigned long checks=0;

	for(alignment=0;alignment<8;alignment++)for(mode=0;mode<12;mode++)for(fail=0;fail<2;fail++){
		s=textstring(mode==1 || mode==3);message=s;
		tuple=calloc(1,sizeof(Heap)+sizeof(void*));assert(tuple);tuple->ref=1;type.size=sizeof(void*);tuple->t=&type;
		value=mode<2 ? (void*)s : H2D(void*,tuple);
		if(mode==4)value=nil;
		if(mode==5)value=H;
		if(mode==6)tuple->t=nil;
		if(mode==7)tuple->t=H;
		if(mode==8)type.size=sizeof(void*)-1;
		if(mode==9)message=nil;
		if(mode==10)message=H;
		if(mode==11)s->len=(-0x7fffffff-1);
		memmove(H2D(void*,tuple),&message,sizeof(message));
		memset(&R,0,sizeof(R));R.s=slot+alignment;memmove(R.s,&value,sizeof(value));saved=R;
		current.exval=H;allocationfailure=fail;raised=nil;
		if(setjmp(boundary)==0){iraise();assert(0);}
		accepted=mode<4 && !(fail && (mode==1 || mode==3));
		if(mode==10)accepted=1;
		assert(!memcmp(&R,&saved,sizeof(R)));
		if(accepted){
			assert(current.exval==value && D2H(value)->ref==2);
			assert(!strcmp(raised,mode==10 ? "" : "er"));D2H(value)->ref--;
		}else{
			assert(current.exval==H && D2H(s)->ref==1 && tuple->ref==1);
			assert(raised==((mode==4 || mode==5 || mode==9) ? exNilref : mode>=6 && mode<=8 ? exType : exNomem));
		}
		free(s->tmp);free(D2H(s));free(tuple);checks++;
	}
	printf("PASS raise slots: %lu descriptor/message/allocation/owner checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
