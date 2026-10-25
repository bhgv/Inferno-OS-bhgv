#include "dat.h"
#include "fns.h"
#include "interp.h"
#include "isa.h"
#include "raise.h"
#include "srv.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
REG R;
int mutator, nprop;
Type Tlist={1}, Tptr={1}, Tstring={1};
char exHeap[]="heap", exNilref[]="nil", exNegsize[]="negative";
static char *raised;
static int vmheld=1, dbheld, live, native_live, allocations, failat;
static int api, answers, nativeerror, missing, oldreleased, vectorfail;
static void *vector;
static void *oldroot, *returnslot;
static struct { Heap *heap; int size; } blocks[64];
static void *native[10];
static char *names[]={"", "1", "192.0.2.1", "host.example", "a", "bb", "ccc", "dddd", "eeeee", "ffffff"};

/* Keep actual emulator exception frames and inspect every ownership unwind. */
Proc *getup(void) {return &worker;}
void nexterror(void) {assert(worker.nerr>0);siglongjmp(worker.estack[--worker.nerr],1);}
void error(char *message) {raised=message;nexterror();}
int __lsan_is_turned_off(void) {return 1;}

/* Require native resolver calls outside the VM lock and allocations inside it. */
void release(void) {assert(vmheld && !dbheld);vmheld=0;}
void acquire(void) {assert(!vmheld && !dbheld);vmheld=1;}
void qlock(QLock *q) {(void)q;assert(!vmheld && !dbheld);dbheld=1;}
void qunlock(QLock *q) {(void)q;assert(!vmheld && dbheld);dbheld=0;}
char *string2c(String *text) {(void)text;assert(!vmheld && dbheld);return "input";}

/* Preserve exact allocation sizes, native reference widths and trailing guards. */
Heap*
nheap(int size)
{
	Heap *h;
	int i;
	assert(vmheld && !dbheld && size>=0);
	allocations++;
	if(failat && allocations==failat)error(exHeap);
	for(i=0;i<64 && blocks[i].heap;i++);
	assert(i<64);
	h=calloc(1,sizeof(Heap)+size+16);assert(h);h->ref=1;h->color=mutator;
	blocks[i].heap=h;blocks[i].size=size;memset((uchar*)(h+1)+size,0xa5,16);live++;
	return h;
}

/* Return the real requested geometry to the production string allocator. */
int
hmsize(void *heap)
{
	int i;
	for(i=0;i<64 && blocks[i].heap!=heap;i++);
	assert(i<64);return sizeof(Heap)+blocks[i].size;
}

/* Destroy actual list/string ownership graphs and verify publication before release. */
void
destroy(void *value)
{
	Heap *h;
	List *list;
	void *child, *published;
	int i,j;
	if(value==H)return;
	assert(vmheld && !dbheld && value!=nil);
	if(value==oldroot && !oldreleased){
		memmove(&published,returnslot,sizeof(published));assert(published==H);oldreleased=1;
	}
	h=D2H(value);assert(h->ref>0);if(--h->ref)return;
	for(i=0;i<64 && blocks[i].heap!=h;i++);
	assert(i<64);
	for(j=0;j<16;j++)assert(((uchar*)(h+1))[blocks[i].size+j]==0xa5);
	if(h->t==&Tlist){
		list=value;
		if(list->t){assert(list->t==&Tptr);memmove(&child,list->data,sizeof(child));destroy(child);Tptr.ref--;}
		destroy(list->tail);Tlist.ref--;
	}else{assert(h->t==&Tstring);Tstring.ref--;}
	blocks[i].heap=nil;free(h);live--;
}

/* Keep the resolver ownership vector outside automatic storage across longjmp. */
static void*
srvfixturemalloc(ulong size)
{
	assert(vmheld && !dbheld && size==10*sizeof(char*) && !vector);
	if(vectorfail)return nil;
	vector=malloc(size);assert(vector);native_live++;
	return vector;
}

/* Track resolver buffers independently from the VM heap and reject double release. */
static void
srvfixturefree(void *value)
{
	int i;
	if(!value)return;
	if(value==vector){free(vector);vector=nil;native_live--;return;}
	for(i=0;i<10 && native[i]!=value;i++);
	assert(i<10);native[i]=nil;free(value);native_live--;
}

/* Model zero, multiple, missing and partially failing native answer vectors. */
static int
resolve(char **hosts, int capacity)
{
	int i,n=answers<0 || answers>10 ? 3 : answers;
	assert(!vmheld && dbheld && capacity==10);
	for(i=0;i<n;i++){
		if(missing && i==1)continue;
		native[i]=malloc(strlen(names[i])+1);assert(native[i]);native_live++;
		strcpy(native[i],names[i]);hosts[i]=native[i];
		if(nativeerror && i==n/2)error(exHeap);
	}
	if(nativeerror)error(exHeap);
	return answers;
}

/* Both native resolver entry points preserve ordered answer ownership. */
int so_gethostbyname(char *name,char **hosts,int n) {(void)name;return resolve(hosts,n);}
int so_gethostbyaddr(char *name,char **hosts,int n) {(void)name;return resolve(hosts,n);}
int so_getservbyname(char *service,char *net,char *port) {
	(void)service;(void)net;assert(!vmheld && dbheld);
	if(nativeerror)error(exHeap);memmove(port,"443",4);return answers ? 0 : -1;
}

/* Transfer a freshly allocated string directly into its unaligned VM owner slot. */
void
retstr(char *text,void *slot)
{
	String *result=newstring(strlen(text));
	memmove(result->Sascii,text,strlen(text));memmove(slot,&result,sizeof(result));
}

#define malloc srvfixturemalloc
#define free srvfixturefree
IMPLEMENTATION
#undef free
#undef malloc

/* Enter the real wrapper through the real outer emulator exception protector. */
static int
invoke(void *frame)
{
	if(api==0)Srv_iph2a(frame);
	else if(api==1)Srv_ipa2h(frame);
	else Srv_ipn2p(frame);
	return 1;
}

/* Isolate parent regressions without changing the ordinary exhaustive matrix. */
static int
selected(char *name, int value)
{
	char *text=getenv(name);
	return text==nil || atoi(text)==value;
}

/* Check every allocation failure, return alignment, native unwind and ordered payload. */
int
main(void)
{
	union {uvlong align;uchar bytes[48];} slots;
	uchar expected[48];
	F_Srv_iph2a forward={0};F_Srv_ipa2h reverse={0};F_Srv_ipn2p service={0};
	void *frame,*empty=H,*result,*child;
	List *list;
	String *text;
	int counts[]={0,1,3,10,-1,11},ci,offset,fail,ok,bad,i,expectedalloc;
	unsigned long checks=0;

	for(api=0;api<3;api++)for(ci=0;ci<(api==2 ? 2 : 6);ci++)
	for(offset=0;offset<8;offset++)for(nativeerror=0;nativeerror<2;nativeerror++)
	for(missing=0;missing<(api==2 ? 1 : 2);missing++)for(vectorfail=0;vectorfail<(api==2 ? 1 : 2);vectorfail++)
	for(fail=0;fail<=21;fail++){
		if(!selected("SRV_API",api) || !selected("SRV_OFFSET",offset) ||
		   !selected("SRV_VECTOR_FAIL",vectorfail) || !selected("SRV_COUNT",counts[ci]) || !selected("SRV_FAIL",fail) ||
		   !selected("SRV_NATIVE_ERROR",nativeerror) || !selected("SRV_MISSING",missing))continue;
		answers=counts[ci];failat=0;allocations=0;raised=nil;oldreleased=1;
		returnslot=slots.bytes+8+offset;memset(slots.bytes,0xa5,sizeof(slots.bytes));
		memmove(returnslot,&empty,sizeof(empty));
		if(api==2){oldroot=newstring(3);memmove(returnslot,&oldroot,sizeof(oldroot));}
		else oldroot=cons(0,returnslot);
		oldreleased=0;memmove(expected,slots.bytes,sizeof(expected));
		memmove(expected+8+offset,&empty,sizeof(empty));
		allocations=0;failat=fail;
		if(api==0){forward.ret=returnslot;frame=&forward;}
		else if(api==1){reverse.ret=returnslot;frame=&reverse;}
		else{service.ret=returnslot;frame=&service;}
		expectedalloc=api==2 ? (answers!=0) : answers*2;
		bad=vectorfail || (!nativeerror && (answers<0 || answers>10 || (missing && answers>=3) || (fail && fail<=expectedalloc)));
		ok=kprotect(invoke,frame);
		assert(ok==!bad && oldreleased && !native_live && vmheld && !dbheld && worker.nerr==0);
		memmove(&result,returnslot,sizeof(result));
		if(bad || nativeerror || answers==0)assert(result==H && !live);
		else if(api==2){text=result;assert(text->len==3 && !memcmp(text->Sascii,"443",3));}
		else{
			list=result;
			for(i=0;i<answers;i++){
				assert(list!=H && list->t==&Tptr);memmove(&child,list->data,sizeof(child));
				text=child;assert(text->len==strlen(names[i]) && !memcmp(text->Sascii,names[i],text->len));
				list=list->tail;
			}
			assert(list==H);
		}
		memmove(expected+8+offset,&result,sizeof(result));assert(!memcmp(expected,slots.bytes,sizeof(expected)));
		oldreleased=1;destroy(result);failat=0;
		assert(!live && Tlist.ref==1 && Tptr.ref==1 && Tstring.ref==1);
		checks++;
	}
	printf("PASS Srv results: %lu slot/answer/allocation/native-buffer/error cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
