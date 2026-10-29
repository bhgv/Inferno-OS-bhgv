#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <stdio.h>
#include <setjmp.h>
REG R;
char exNilref[]="nil",exHeap[]="heap",exModule[]="module";
static jmp_buf boundary;
static int released,extended,mapped;
static Frame *expectedframe;
static Type *expectedtype;
static Modlink *expectedmodule;
static int returnborrow; // One borrows REG during mapped cleanup, two during module destruction.
typedef struct ReturnOwner ReturnOwner;

struct ReturnOwner {
	Heap heap; // Real native reference header immediately before the module payload.
	Modlink link; // Module-link payload shared with published RET leaves.
};


/* Keep native stack checks enabled without process-inspection leak checks. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Invalid return frames must preserve REG before raising. */
void
error(char *text)
{
	(void)text;
	longjmp(boundary,1);
}


/* Stack teardown may invalidate the frame immediately. */
void
unextend(Frame *frame)
{
	if(frame!=expectedframe)abort();
	extended++;
	memset(frame,0x5a,sizeof(Frame));
}


/* Mapped cleanup receives the captured descriptor and original frame address. */
void
freeptrs(void *frame,Type *type)
{
	if(frame!=expectedframe || type!=expectedtype)abort();
	mapped++;
	memset(frame,0x5a,sizeof(Frame));
	if(returnborrow & 1) memset(&R, 0x66, sizeof(R));
}


/* Observe the same reference decrement as destroy's non-destructive fast path. */
void
destroy(void *module)
{
	if(module!=expectedmodule || D2H(module)->ref==0)abort();
	D2H(module)->ref--;
	released++;
	if(returnborrow & 2) memset(&R, 0x66, sizeof(R));
}

#define OP(name) void name(void) // Production interpreter instruction entry.
IMPLEMENTATION


/* Cover normal returns and builtin stack extensions with independent frame bytes. */
int
main(void)
{
	union {uvlong align;uchar bytes[512];} arena;
	Frame links;
	Type type={0};
	ReturnOwner calleeowner={0};
	Modlink caller={0},*callee=&calleeowner.link;
	Inst code[3];
	REG before;
	int align,kind,restore,oldcompiled,newcompiled,invalid;
	unsigned long checks=0;

	for(align=0;align<8;align++)for(kind=0;kind<3;kind++)for(restore=0;restore<2;restore++)
	for(oldcompiled=0;oldcompiled<2;oldcompiled++)for(newcompiled=0;newcompiled<2;newcompiled++){
		memset(arena.bytes,0xa5,sizeof(arena.bytes));memset(&R,0,sizeof(R));
		expectedframe=(Frame*)(arena.bytes+128+align);expectedtype=&type;expectedmodule=callee;
		links.lr=code+2;links.fp=arena.bytes+320;links.mr=restore ? &caller : nil;links.t=kind==0 ? nil : &type;
		calleeowner.heap.ref=3;type.np=kind==2;caller.compiled=newcompiled;callee->compiled=oldcompiled;caller.MP=arena.bytes+400;
		memmove(expectedframe,&links,sizeof(links));
		if(kind==0)memmove((uchar*)SEXTYPE(expectedframe)+offsetof(Stkext,reg.TR),&expectedtype,sizeof(expectedtype));
		R.FP=(uchar*)expectedframe;R.PC=code;R.M=callee;R.MP=arena.bytes+448;R.IC=12;R.t=9;
		released=extended=mapped=0;ret();
		if(!(R.FP==links.fp && R.PC==links.lr && R.SP==(uchar*)expectedframe))abort();
		if(!(extended==(kind==0) && mapped==(kind==2) && released==restore))abort();
		if(!(R.M==(restore ? &caller : callee)))abort();
		if(!(R.MP==(restore ? caller.MP : arena.bytes+448)))abort();
		if(!(R.IC==((restore && oldcompiled!=newcompiled) ? 1 : 12)))abort();
		if(!(R.t==((restore && oldcompiled!=newcompiled) ? 1 : 9)))abort();checks++;
	}
	for(invalid=0;invalid<3;invalid++){
		R.FP=invalid==0 ? nil : invalid==1 ? H : arena.bytes+128;
		memset(&links,0,sizeof(links));memmove(arena.bytes+128,&links,sizeof(links));before=R;
		released=extended=mapped=0;
		if(setjmp(boundary)==0){ret();if(!(0))abort();}
		if(!(!memcmp(&R,&before,sizeof(R)) && !released && !extended && !mapped))abort();checks++;
	}
	printf("PASS return slots: %lu frame alignment/cleanup/module/PC checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
