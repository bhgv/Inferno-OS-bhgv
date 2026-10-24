#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
REG R;
char exNilref[]="nil";
static jmp_buf boundary;
static int released,extended,mapped;
static Frame *expectedframe;
static Type *expectedtype;
static Modlink *expectedmodule;
/* Keep native stack checks enabled without process-inspection leak checks. */
int __lsan_is_turned_off(void) {return 1;}
/* Invalid return frames must preserve REG before raising. */
void error(char *text) {(void)text;longjmp(boundary,1);}
/* Stack teardown may invalidate the frame immediately. */
void unextend(Frame *frame) {assert(frame==expectedframe);extended++;memset(frame,0x5a,sizeof(Frame));}
/* Mapped cleanup receives the captured descriptor and original frame address. */
void freeptrs(void *frame,Type *type) {assert(frame==expectedframe && type==expectedtype);mapped++;memset(frame,0x5a,sizeof(Frame));}
/* Module release follows frame teardown and occurs before the caller is restored. */
void destroy(void *module) {assert(module==expectedmodule);released++;}
#define OP(name) void name(void)
IMPLEMENTATION

/* Cover normal returns and builtin stack extensions with independent frame bytes. */
int
main(void)
{
	union {uvlong align;uchar bytes[512];} arena;
	Frame links;
	Type type={0};
	Modlink caller={0},callee={0};
	Inst code[3];
	REG before;
	int align,kind,restore,oldcompiled,newcompiled,invalid;
	unsigned long checks=0;

	for(align=0;align<8;align++)for(kind=0;kind<3;kind++)for(restore=0;restore<2;restore++)
	for(oldcompiled=0;oldcompiled<2;oldcompiled++)for(newcompiled=0;newcompiled<2;newcompiled++){
		memset(arena.bytes,0xa5,sizeof(arena.bytes));memset(&R,0,sizeof(R));
		expectedframe=(Frame*)(arena.bytes+128+align);expectedtype=&type;expectedmodule=&callee;
		links.lr=code+2;links.fp=arena.bytes+320;links.mr=restore ? &caller : nil;links.t=kind==0 ? nil : &type;
		type.np=kind==2;caller.compiled=newcompiled;callee.compiled=oldcompiled;caller.MP=arena.bytes+400;
		memmove(expectedframe,&links,sizeof(links));R.FP=(uchar*)expectedframe;R.PC=code;R.M=&callee;R.MP=arena.bytes+448;R.IC=12;R.t=9;
		released=extended=mapped=0;ret();
		assert(R.FP==links.fp && R.PC==links.lr && R.SP==(uchar*)expectedframe);
		assert(extended==(kind==0) && mapped==(kind==2) && released==restore);
		assert(R.M==(restore ? &caller : &callee));
		assert(R.MP==(restore ? caller.MP : arena.bytes+448));
		assert(R.IC==((restore && oldcompiled!=newcompiled) ? 1 : 12));
		assert(R.t==((restore && oldcompiled!=newcompiled) ? 1 : 9));checks++;
	}
	for(invalid=0;invalid<3;invalid++){
		R.FP=invalid==0 ? nil : invalid==1 ? H : arena.bytes+128;
		memset(&links,0,sizeof(links));memmove(arena.bytes+128,&links,sizeof(links));before=R;
		released=extended=mapped=0;
		if(setjmp(boundary)==0){ret();assert(0);}
		assert(!memcmp(&R,&before,sizeof(R)) && !released && !extended && !mapped);checks++;
	}
	printf("PASS return slots: %lu frame alignment/cleanup/module/PC checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
