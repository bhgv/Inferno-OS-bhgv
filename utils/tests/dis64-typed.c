#include "limbo.h"
#include "dis64-typed.h"
#include <assert.h>
static unsigned int owners;
void *allocmem(ulong bytes) { void *p=calloc(1,bytes?bytes:1); assert(p!=nil); owners++; return p; }
#undef free
extern void free(void*);
void typedfree(void *p) { if(p){assert(owners);owners--;free(p);} }
#undef qsort
static int (*order)(void*,void*);
static int hostorder(const void *a,const void *b) {return order((void*)a,(void*)b);}
void infqsort(void *p,long n,long size,int (*compare)(void*,void*)) {order=compare;qsort(p,n,size,hostorder);}
Desc *descriptors;

int main(void)
{
	Type word={0},string={0},reference={0},none={0},fn={0};
	Decl args[2]={{0}},globals[2]={{0}},local={0},function={0};
	Desc frame={0},data={0}; uchar fmap[]={0,0x80},dmap[]={0x40};
	Inst code[3]={{0}},outside={0}; Dis64TypedBody p={0};
	unsigned int i;
	word.kind=Tint; word.size=word.align=4; string.kind=Tstring; string.size=string.align=4;
	reference=string; reference.kind=Tref; none.kind=Tnone;
	args[0].store=args[1].store=Darg; args[0].ty=&string;args[0].offset=32;args[0].next=args+1;
	args[1].ty=&word;args[1].offset=36;
	local.store=Dlocal;local.ty=&word;local.offset=40;
	globals[0].store=globals[1].store=Dglobal;globals[0].ty=&word;globals[0].next=globals+1;
	globals[1].ty=&string;globals[1].offset=4;
	frame.size=48;frame.map=fmap;frame.nmap=2;data.size=8;data.map=dmap;data.nmap=1;descriptors=&data;
	fn.kind=Tfn;fn.tof=&none;fn.ids=args;function.ty=&fn;function.locals=&local;function.desc=&frame;function.pc=code;
	for(i=0;i<3;i++){code[i].pc=i;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<2?code+i+1:nil;}
	code[0].op=IMOVW;code[0].sm=Aimm;code[0].s.offset=7;code[0].dm=Afp;code[0].d.reg=36;
	code[1].op=ILENC;code[1].sm=Afp;code[1].s.reg=32;code[1].dm=Amp;code[2].op=IRET;
	assert(dis64typedlower(&function,globals,code,3,3,&p));
	assert(p.count==3 && p.code[0].destination==72 && p.code[1].source==64 && p.frame.bytes==80);
	dis64typedfree(&p);
	code[2].op=INEW;
	assert(!dis64typedlower(&function,globals,code,3,3,&p) && p.badpc==2);dis64typedfree(&p);code[2].op=IRET;
	function.handler=1;assert(!dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p);function.handler=0;
	code[0].sm=Afp;code[0].s.reg=32;
	assert(!dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p);
	code[0].op=IMOVP;code[0].sm=Amp;code[0].s.reg=4;code[0].d.reg=32;globals[1].ty=&reference;
	assert(!dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p);globals[1].ty=&string; /* REF_CLASS */
	code[0].op=IMOVL;code[0].sm=Aimm;code[0].s.offset=-1;code[0].d.reg=32;
	/* A scalar big destination is needed to isolate the eight-byte immediate refusal. */
	args[0].ty=&string; args[1].ty=&word;
	{
		Type big={0}; Decl save=local; big.kind=Tbig;big.size=big.align=8;local.ty=&big;local.offset=40;
		code[0].d.reg=40;
		assert(!dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p);local=save; /* WIDE_IMMEDIATE */
	}
	args[0].ty=&string;
	code[0].op=IJMP;code[0].sm=Anone;code[0].dm=Apc;code[0].branch=&outside;outside.pc=3;code[0].d.offset=3;
	assert(!dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p); /* BRANCH_OWNER */
	code[0].branch=code+2;code[0].d.offset=2;
	assert(dis64typedlower(&function,globals,code,3,3,&p));assert(p.code[0].destination==2);dis64typedfree(&p);
	for(i=0;i<3;i++)code[i].pc+=100;code[0].d.offset=102;
	assert(dis64typedlower(&function,globals,code,103,103,&p));assert(p.first==100 && p.code[0].destination==2);dis64typedfree(&p);
	assert(!dis64typedlower(&function,globals,code,65537,103,&p));dis64typedfree(&p);
	assert(owners==0 && p.code==nil && p.reason==nil);
	return 0;
}
