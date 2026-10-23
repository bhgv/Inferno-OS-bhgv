#include "dat.h"
#include "fns.h"
#include "kernel.h"
#include "isa.h"
#include "interp.h"
#include "keyringif.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

Type Tarray={1}, Tbyte={1}, Tstring={1}, wrong={1};
char exBounds[]="bounds", exNilref[]="nil", exType[]="type";
char exInval[]="invalid", exHeap[]="heap", exNomem[]="nomem";
static Proc worker;
static int released, readcalls, writecalls, chunk, failat, zeroat, position;
static int inputlen, outputlen, native_live, vm_live, failnative, failvm, checks;
static uchar input[12000], output[12000];
static char syserror[128];
static Sys_FD fd={17};
enum { Maxmsg=4096 };

/* Use production emulator error frames, including rethrow after protected cleanup. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr>0); siglongjmp(worker.estack[--worker.nerr],1); }
void error(char *text) { (void)text; nexterror(); }
int __lsan_is_turned_off(void) { return 1; }
/* Require every kernel I/O call to run outside the VM lock. */
void release(void) { assert(!released); released=1; }
void acquire(void) { assert(released); released=0; }
void kwerrstr(char *format, ...) {
	va_list args;
	va_start(args,format); vsnprintf(syserror,sizeof(syserror),format,args); va_end(args);
}
/* Host formatting stands in for fmt; protocol bytes are asserted independently. */
int snprint(char *buf,int size,char *format,...) {
	va_list args; int n;
	va_start(args,format);n=vsnprintf(buf,size,format,args);va_end(args);return n;
}
/* Inject short I/O, zero progress and native errors at exact byte positions. */
long kread(int descriptor,void *buf,long n) {
	assert(released && descriptor==17 && n>0); readcalls++;
	if(failat>=0 && position>=failat)return -1;
	if(zeroat>=0 && position>=zeroat)return 0;
	if(n>inputlen-position)n=inputlen-position;
	if(chunk>0 && n>chunk)n=chunk;
	if(failat>=0 && position+n>failat)n=failat-position;
	if(zeroat>=0 && position+n>zeroat)n=zeroat-position;
	memmove(buf,input+position,n);position+=n;return n;
}
long kwrite(int descriptor,void *buf,long n) {
	assert(released && descriptor==17 && n>0);writecalls++;
	if(failat>=0 && outputlen>=failat)return -1;
	if(zeroat>=0 && outputlen>=zeroat)return 0;
	if(chunk>0 && n>chunk)n=chunk;
	if(failat>=0 && outputlen+n>failat)n=failat-outputlen;
	if(zeroat>=0 && outputlen+n>zeroat)n=zeroat-outputlen;
	assert(outputlen+n<=(int)sizeof(output));
	memmove(output+outputlen,buf,n);outputlen+=n;return n;
}
/* Track native buffers separately from VM objects across every throw boundary. */
static void *bufferalloc(size_t n) {
	void *p;
	if(failnative)return nil;
	p=malloc(n);assert(p);native_live++;return p;
}
static void bufferfree(void *p) { if(p) {assert(native_live>0);native_live--;free(p);} }
/* Construct real heap headers; only the allocator/GC itself is simulated here. */
static void *vmalloc(Type *type,int size) {
	Heap *h;
	if(failvm)error(exHeap);
	h=calloc(1,sizeof(Heap)+size);assert(h);h->ref=1;h->t=type;vm_live++;
	return H2D(void*,h);
}
void destroy(void *p) {
	Heap *h;
	if(p==H)return;
	assert(p);h=D2H(p);assert(h->ref>0);
	if(--h->ref==0) {vm_live--;free(h);}
}
Array *mem2array(void *data,int n) {
	Array *a=vmalloc(&Tarray,sizeof(Array)+n);
	a->len=n;a->t=&Tbyte;a->root=H;a->data=(uchar*)(a+1);
	if(n)memmove(a->data,data,n);return a;
}
String *c2string(char *data,int n) {
	String *s=vmalloc(&Tstring,sizeof(String)+n+1);
	s->len=n;memmove(s->Sascii,data,n);s->Sascii[n]=0;return s;
}
void retnstr(char *data,int n,String **result) {
	String *s=n ? c2string(data,n) : H;
	destroy(*result);*result=s;
}
char *string2c(String *s) { return s==H ? "" : s->Sascii; }

#define malloc bufferalloc
#define free bufferfree
IMPLEMENTATION
#undef malloc
#undef free

/* Reset I/O state without hiding leaked native buffers or lost VM error frames. */
static void reset(const void *data,int n) {
	assert(!released && native_live==0 && worker.nerr==0);
	if(n)memmove(input,data,n);
	inputlen=n;outputlen=position=readcalls=writecalls=chunk=0;
	failat=zeroat=-1;failnative=failvm=0;syserror[0]=0;
}
/* Read each legal payload through every short-read size surrounding the header. */
static void streamreads(void) {
	char frame[4110], result[4100];
	int sizes[]={0,1,3,63,4095},i,j,n;
	for(i=0;i<5;i++) {
		n=sizes[i];snprintf(frame,sizeof(frame),"%04d\n",n);memset(frame+5,'x',n);
		for(j=1;j<=7;j++) {
			reset(frame,n+5);chunk=j;
			assert(getmsg(17,result,sizeof(result))==n);
			assert(position==n+5 && result[n]==0 && !released);checks++;
		}
	}
	reset("!000\n",5);assert(getmsg(17,result,sizeof(result))<0 && result[0]==0);checks++;
	reset("!003\nbad",8);chunk=1;assert(getmsg(17,result,sizeof(result))==-3 && !strcmp(result,"bad"));checks++;
	for(i=0;i<8;i++) {
		reset("0003\nabc",8);inputlen=i;memset(result,0xa5,sizeof(result));
		assert(getmsg(17,result,sizeof(result))<0 && !released);checks++;
	}
	reset("4096\n",5);assert(getmsg(17,result,4096)<0 && position==5);checks++;
	reset(nil,0);assert(getmsg(17,result,0)<0 && readcalls==0);checks++;
}
/* Mutate every header byte; reject all nondecimal characters before reading a body. */
static void headers(void) {
	char frame[6]="0000\n",result[32];
	int i,b;
	for(i=0;i<5;i++)for(b=0;b<256;b++) {
		memcpy(frame,"0000\n",5);frame[i]=b;
		if(i<4 && b>='0' && b<='9')continue;
		if(i==0 && b=='!')continue;
		if(i==4 && b=='\n')continue;
		reset(frame,5);assert(getmsg(17,result,sizeof(result))<0);
		assert(position==5 && !strcmp(result,MSG));checks++;
	}
}
/* Check maximum counts, payload identity, zero progress and partially sent frames. */
static void streamwrites(void) {
	char payload[10001];int n,j,k,result;
	int sizes[]={0,1,4095,9999};
	memset(payload,'q',sizeof(payload));payload[sizeof(payload)-1]=0;
	for(k=0;k<4;k++)for(j=1;j<=7;j++) {
		n=sizes[k];reset(nil,0);chunk=j;
		assert(sendmsg(17,payload,n)==n && outputlen==n+5);
		assert(output[4]=='\n' && !memcmp(output+5,payload,n));checks++;
	}
	for(j=0;j<2;j++)for(k=0;k<9;k++) {
		reset(nil,0);chunk=2;if(j)zeroat=k;else failat=k;
		result=sendmsg(17,payload,4);assert(result==-1 && !released && outputlen==k);checks++;
	}
	reset(nil,0);assert(sendmsg(17,payload,10000)==-1 && writecalls==0);checks++;
	reset(nil,0);assert(sendmsg(17,payload,-1)==-1 && writecalls==0);checks++;
	payload[999]=0;reset(nil,0);chunk=1;
	assert(senderr(17,payload,0)==999 && !memcmp(output,"!999\n",5));checks++;
	payload[999]='q';payload[1000]=0;reset(nil,0);
	assert(senderr(17,payload,0)==-1 && writecalls==0);checks++;
	payload[991]=0;reset(nil,0);chunk=3;
	assert(senderr(17,payload,1)==991 && !memcmp(output,"!999\nremote: ",13));checks++;
	payload[991]='q';payload[992]=0;reset(nil,0);
	assert(senderr(17,payload,1)==-1 && writecalls==0);checks++;
	reset(nil,0);assert(senderr(17,"",0)==0 && !memcmp(output,"!000\n",5));checks++;
}
/* The hangup terminator belongs to err, and neither output may exceed its span. */
static void records(void) {
	uchar data[9],errorbuf[82],payload[4100];int n,i;
	for(n=1;n<=80;n++) {
		reset(nil,0);memset(data,0xa5,sizeof(data));memset(errorbuf,0xa5,sizeof(errorbuf));
		assert(getbuf(17,data,1,(char*)errorbuf,n)<0);
		assert(errorbuf[n-1]==0 && errorbuf[n]==0xa5 && data[1]==0xa5);checks++;
	}
	reset(nil,0);assert(getbuf(17,data,0,(char*)errorbuf,1)<0 && readcalls==0);checks++;
	reset(nil,0);assert(getbuf(17,data,1,(char*)errorbuf,0)<0 && readcalls==0);checks++;
	reset("\0abc",4);assert(getbuf(17,data,9,(char*)errorbuf,80)==3);checks++;
	reset("\xff",1);assert(getbuf(17,data,9,(char*)errorbuf,80)<0 && !strcmp((char*)errorbuf,"unknown"));checks++;
	reset("\x7f",1);assert(getbuf(17,data,9,(char*)errorbuf,80)<0 && !strcmp((char*)errorbuf,"failure"));checks++;
	reset("\xfflong error",11);assert(getbuf(17,data,9,(char*)errorbuf,4)<0 && !strcmp((char*)errorbuf,"lon"));checks++;
	memset(payload,'z',sizeof(payload));
	for(i=0;i<3;i++) {
		reset(nil,0);n=i==0?0:i==1?4095:4100;
		assert(putbuf(17,payload,n,1)==(n<4096?n:4095)+1);
		assert(writecalls==1 && output[0]==0xff && native_live==0);checks++;
	}
	reset(nil,0);chunk=2;assert(putbuf(17,payload,10,0)==-1 && writecalls==1);checks++;
	reset(nil,0);failnative=1;assert(putbuf(17,payload,1,0)==-1 && writecalls==0);checks++;
}
/* Dispatch with real generated frames; no shortened substitute parameter structs. */
static void wrappers(void) {
	F_Keyring_sendmsg s={0};F_Keyring_putbytearray p={0};F_Keyring_getmsg g={0};
	F_Keyring_senderrmsg e={0};F_Keyring_puterror pe={0};
	F_Keyring_getstring gs={0};F_Keyring_getbytearray ga={0};
	Array *a,*result;String *text;WORD value;int op,mode;
	struct { String *t0,*t1; } strings;
	struct { Array *t0;String *t1; } arrays;
	reset(nil,0);a=mem2array("abc",3);text=c2string("",0);
	s.fd=p.fd=e.fd=pe.fd=&fd;s.buf=p.a=a;s.ret=p.ret=e.ret=pe.ret=&value;
	e.s=pe.s=text;
	Keyring_senderrmsg(&e);assert(value==0 && !memcmp(output,"!000\n",5));checks++;
	reset(nil,0);Keyring_puterror(&pe);assert(value==1 && output[0]==0xff);checks++;
	for(op=0;op<2;op++)for(mode=0;mode<5;mode++) {
		reset(nil,0);s.n=p.n=mode==0?-1:mode==1?4:mode==2?0:mode==3?3:(WORD)0x7fffffff;
		if(waserror()) {assert(mode==1 || mode==4 || (op==1 && mode==0));checks++;continue;}
		if(op)Keyring_putbytearray(&p);else Keyring_sendmsg(&s);
		poperror();assert(mode==0 || mode==2 || mode==3);checks++;
	}
	for(op=0;op<2;op++) {
		reset(nil,0);s.n=p.n=1;a->t=&wrong;
		if(waserror()) {a->t=&Tbyte;assert(writecalls==0);checks++;continue;}
		if(op)Keyring_putbytearray(&p);else Keyring_sendmsg(&s);assert(0);
	}
	if(sizeof(WORD)>4) {
		reset(nil,0);s.n=p.n=(WORD)((uvlong)1<<32)+3;
		if(waserror()) {assert(writecalls==0);checks++;}
		else {Keyring_putbytearray(&p);assert(0);}
		reset(nil,0);a->len=(WORD)((uvlong)1<<32)+3;s.n=p.n=a->len;
		if(waserror()) {assert(writecalls==0);checks++;}
		else {Keyring_putbytearray(&p);assert(0);}
		a->len=3;
	}
	destroy(a);destroy(text);
	g.fd=gs.fd=ga.fd=&fd;g.ret=&result;
	gs.ret=(void*)&strings;ga.ret=(void*)&arrays;
	for(op=0;op<3;op++)for(mode=0;mode<3;mode++) {
		reset(mode==2?"!003\nbad":"0003\nabc",8);
		if(op) {input[0]=mode==2?0xff:0;memmove(input+1,mode==2?"bad":"abc",3);inputlen=4;}
		result=H;strings.t0=strings.t1=H;arrays.t0=H;arrays.t1=H;
		failvm=mode==1;
		if(waserror()) {
			assert(mode==1 && native_live==0 && !released);failvm=0;checks++;continue;
		}
		if(op==0)Keyring_getmsg(&g);else if(op==1)Keyring_getstring(&gs);else Keyring_getbytearray(&ga);
		poperror();assert(mode!=1 && native_live==0);
		if(op==0) {if(mode==2)assert(result==H && !strcmp(syserror,"bad"));else assert(result->len==3);destroy(result);}
		else if(op==1) {assert(mode==2?strings.t0==H:strings.t1==H);destroy(strings.t0);destroy(strings.t1);}
		else {assert(mode==2?arrays.t0==H:arrays.t1==H);destroy(arrays.t0);destroy(arrays.t1);}
		checks++;
	}
	/* Diagnostic conversion must release the receive buffer on VM allocation errors too. */
	for(op=1;op<=2;op++) {
		reset("\xff" "bad",4);strings.t0=strings.t1=H;arrays.t0=H;arrays.t1=H;failvm=1;
		if(waserror()) {assert(native_live==0);failvm=0;checks++;continue;}
		if(op==1)Keyring_getstring(&gs);else Keyring_getbytearray(&ga);assert(0);
	}
	/* Allocation failure preserves the old single result while freeing native storage. */
	reset("0003\nabc",8);result=mem2array("old",3);a=result;failvm=1;
	if(waserror()) {assert(result==a && native_live==0);failvm=0;destroy(a);checks++;}
	else {Keyring_getmsg(&g);assert(0);}
	assert(vm_live==0 && native_live==0 && worker.nerr==0);
}
/* Run boundary families with independent byte expectations and ownership accounting. */
int main(void) {
	streamreads();headers();streamwrites();records();wrappers();
	assert(!released && native_live==0 && vm_live==0 && worker.nerr==0);
	printf("Keyring messages: %d framing/I/O/ownership checks passed\n",checks);return 0;
}
