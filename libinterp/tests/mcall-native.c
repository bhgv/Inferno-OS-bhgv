#include "comp-amd64.c"
#include <stdio.h>
#include <setjmp.h>

REG R;
int mutator,nprop;
char exHeap[]="heap",exModule[]="module",exNilref[]="nil";
static jmp_buf boundary;
static char *raised;
static Prog running;
static int callbacks;


/* Keep sanitizer checks without unavailable leak-process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Capture VM errors without publishing partial test results. */
void
error(char *message)
{
	raised=message;
	longjmp(boundary,1);
}


/* Publish native code metadata using ordinary host allocations. */
void*
mallocz(ulong size,int clear)
{
	void *p;

	p=malloc(size);
	if(p && clear)memset(p,0,size);
	return p;
}


/* Compilation is given valid input and does not raise VM errors. */
int
kprotect(int (*fn)(void*),void *arg)
{
	return fn(arg);
}


/* Runtime calls consult only the pending-kill field of this process. */
Prog*
currun(void)
{
	return &running;
}


/* This fixture uses ordinary pointer-free frames for builtin fallback. */
void
unextend(Frame *frame)
{
	(void)frame;
	abort();
}


/* Pointer-map cleanup remains covered by the separate return and stack suites. */
void
freeptrs(void *frame,Type *type)
{
	(void)frame;(void)type;
	abort();
}


/* Observe that builtin calls still execute through the interpreter fallback. */
static void
builtin(void *frame)
{
	if(R.FP!=frame)abort();
	callbacks++;
}

#define OP(name) void name(void) // Production interpreter instruction entry.
IMPLEMENTATION


/* Compare C, published RX code, full register images and refcount failure atomicity. */
int
main(void)
{
	Module compiled={0},owner={0};
	Modlink other={0},*target,*destination,*caller;
	Heap *header;
	Inst instruction={0},code[3];
	Link exports[2]={0};
	Type type={0};
	Frame links={0},*frame;
	uchar slots[128],before[128],bytes[sizeof(Frame)+8],savedbytes[sizeof(Frame)+8],expectedbytes[sizeof(Frame)+8];
	REG saved,expected;
	WORD index,operand;
	int alignment,fa,kind,bad,pass,k,native,valid,failed;
	ulong reference,total=0;

	header=calloc(1,sizeof(Heap)+sizeof(Modlink)+2*sizeof(Modl));
	if(!header)abort();
	target=H2D(Modlink*,header);target->m=&owner;owner.ext=exports;
	exports[0].name="entry";exports[0].u.pc=code+1;
	instruction.op=IMCALL;compiled.prog=&instruction;compiled.nprog=1;
	if(!compile(&compiled,1,nil) || !compiled.jit || !compiled.jit->entry[IMCALL])abort();
	for(index=0;index<3;index++)for(alignment=0;alignment<8;alignment++)for(fa=0;fa<8;fa++)
	for(kind=0;kind<4;kind++)for(bad=0;bad<16;bad++)for(pass=0;pass<2;pass++){
		memset(&R,0,sizeof(R));memset(bytes,0xa5,sizeof(bytes));memset(slots,0xa5,sizeof(slots));
		frame=(Frame*)(bytes+fa);links.t=&type;
		memmove(frame,&links,sizeof(links));
		destination=target;caller=kind==0 ? target : &other;
		header->ref=1;target->nlinks=3;target->prog=kind==2 ? nil : code;target->MP=(uchar*)0x12345678;
		target->compiled=kind==1;other.compiled=0;operand=index;
		for(k=0;k<3;k++){
			target->links[k].u.pc=H;
			if(kind==2)target->links[k].u.runt=builtin;
		}
		target->links[index].u.pc=code+1;
		if(kind==2){target->links[index].u.runt=builtin;exports[0].u.runt=builtin;}
		else exports[0].u.pc=code+1;
		if(bad==1)header->ref=0;
		if(bad==2)header->ref=(ulong)-1;
		if(bad==3)frame=nil;
		if(bad==4)frame=H;
		if(bad==5)destination=nil;
		if(bad==6)destination=H;
		if(bad==7)target->prog=H;
		if(bad==8){target->links[index].u.pc=nil;}
		if(bad==9){target->links[index].u.pc=H;}
		if(bad==10)caller=nil;
		if(bad==11)caller=H;
		if(bad==12)operand=-1;
		if(bad==13)header->ref=(ulong)-2;
		if(bad==14)target->nlinks=-1;
		if(bad==15)operand=3;
		R.M=caller;R.PC=code;R.FP=(uchar*)0x24681000;R.SP=(uchar*)0x13572468;R.MP=(uchar*)0x10203040;R.IC=17;R.t=23;
		R.s=slots+alignment;R.m=slots+32+alignment;R.d=slots+64+alignment;
		memmove(R.s,&frame,sizeof(frame));memmove(R.m,&operand,sizeof(operand));memmove(R.d,&destination,sizeof(destination));
		memmove(before,slots,sizeof(before));memmove(savedbytes,bytes,sizeof(bytes));memmove(expectedbytes,bytes,sizeof(bytes));
		memmove(&saved,&R,sizeof(R));reference=header->ref;callbacks=0;raised=nil;
		valid=bad==0 || bad==12 || bad==13;native=valid && (kind==0 || kind==3) && operand>=0;
		if(pass==0){
			if(compiled.jit->entry[IMCALL](&R)!=native)abort();
			if(!native && (memcmp(&saved,&R,sizeof(R)) || memcmp(bytes,savedbytes,sizeof(bytes)) || header->ref!=reference))abort();
		}
		failed=0;
		if(pass!=0 || !native){if(setjmp(boundary)==0)mcall();else failed=1;}
		if(valid){
			memmove(&expected,&saved,sizeof(expected));
			memmove(expectedbytes+fa+offsetof(Frame,lr),&saved.PC,sizeof(void*));
			memmove(expectedbytes+fa+offsetof(Frame,fp),&saved.FP,sizeof(void*));
			memmove(expectedbytes+fa+offsetof(Frame,mr),&saved.M,sizeof(void*));
			expected.FP=(uchar*)frame;expected.M=target;
			if(kind==2){expected.FP=saved.FP;expected.M=saved.M;expected.SP=(uchar*)frame;expected.t=0;}
			else{expected.PC=code+1;expected.MP=target->MP;expected.t=1;if(kind==1)expected.IC=1;}
			if(failed || memcmp(&expected,&R,sizeof(R)) || memcmp(bytes,expectedbytes,sizeof(bytes)) ||
			   header->ref!=reference+(kind!=2) || callbacks!=(kind==2))abort();
		}else if(!failed || memcmp(&saved,&R,sizeof(R)) || memcmp(bytes,savedbytes,sizeof(bytes)) || header->ref!=reference || callbacks)abort();
		if(memcmp(before,slots,sizeof(slots)))abort();
		total++;
	}
	compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);free(header);
	printf("PASS MCALL C/RX: %lu owner/entry/frame/refcount/fallback cases WORD%zu\n",total,sizeof(WORD)*8);
	return 0;
}
