/* Fixture predicates must remain active with release-mode production flags. */
#undef NDEBUG
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
/* Raw IR fixtures supply test signatures; real source gates use types.c sign. */
ulong sign(Decl*d){return d&&d->ty?(ulong)d->ty->sig:0;}

static void tabletests(void)
{
    Type word={0},fn={0},tabletype={0};Decl arg={0},function={0},global={0};
    Desc frame={0},data={0};Inst code[4]={{0}},outside={0};Case table={0};Label label={0};
    Node low={0},high={0},init={0};Sym name={0};Dis64TypedBody p={0};unsigned i;
    word.kind=Tint;word.size=word.align=4;fn.kind=Tfn;fn.tof=&word;fn.ids=&arg;
    arg.store=Darg;arg.ty=&word;arg.offset=32;function.ty=&fn;function.desc=&frame;function.pc=code;
    frame.size=40;data.size=24;descriptors=&data;
    name.name=".case_test";name.len=10;global.sym=&name;global.store=Dglobal;global.ty=&tabletype;global.init=&init;
    init.op=Oname;init.decl=&global;tabletype.kind=Tcase;tabletype.size=20;tabletype.align=4;tabletype.cse=&table;
    low.op=high.op=Oconst;low.val=high.val=1;label.start=&low;label.stop=&high;label.inst=code+1;
    table.nlab=1;table.labs=&label;table.iwild=code+3;
    for(i=0;i<4;i++){code[i].pc=i;code[i].op=IRET;code[i].sm=code[i].mm=code[i].dm=Anone;code[i].next=i<3?code+i+1:nil;}
    code[0].op=ICASE;code[0].sm=Afp;code[0].s.reg=32;code[0].dm=Amp;code[0].d.decl=&global;
    code[1].op=IMOVW;code[1].sm=Aimm;code[1].s.offset=7;code[1].dm=Afpind;code[1].d.reg=16;
    assert(!dis64typedlower(&function,&global,code,4,4,&p) && strcmp(p.reason,"undefined-result")==0);dis64typedfree(&p); /* TABLE_RESULT */
    table.iwild=code+1;
    assert(dis64typedlower(&function,&global,code,4,4,&p) && p.ntables==1);dis64typedfree(&p);
    outside.pc=4;table.iwild=&outside;
    assert(!dis64typedlower(&function,&global,code,4,4,&p) && strcmp(p.reason,"table-default-owner")==0);dis64typedfree(&p);
    table.iwild=code+1;label.inst=&outside;
    assert(!dis64typedlower(&function,&global,code,4,4,&p) && strcmp(p.reason,"table-row-owner-or-order")==0);dis64typedfree(&p); /* TABLE_OWNER */
    label.inst=code+1;name.name="public";
    assert(!dis64typedlower(&function,&global,code,4,4,&p) && strcmp(p.reason,"table-shape")==0);dis64typedfree(&p);
    {
        Type string={0},list={0};Decl a={0},b={0};Sym sa={0},sb={0};uchar dm[]={0x60},fm[]={0,0x80};
        name.name=".case_test";string.kind=Tstring;string.size=string.align=4;list=string;list.kind=Tlist;
        arg.ty=&string;frame.map=fm;frame.nmap=2;data.map=dm;data.nmap=1;tabletype.kind=Tcasec;
        sa.name="a";sa.len=1;sb.name="z";sb.len=1;a.sym=&sa;b.sym=&sb;low.decl=&a;high.decl=&b;low.ty=high.ty=&string;
        code[0].op=ICASEC;
        assert(dis64typedlower(&function,&global,code,4,4,&p) && p.ntables==1 && p.tables[0].op==ICASEC);dis64typedfree(&p);
        table.iwild=code+3;
        assert(!dis64typedlower(&function,&global,code,4,4,&p) && strcmp(p.reason,"undefined-result")==0);dis64typedfree(&p);
        table.iwild=code+1;sa.name="zz";sa.len=2;
        assert(!dis64typedlower(&function,&global,code,4,4,&p) && strcmp(p.reason,"string-table-row-owner-or-order")==0);dis64typedfree(&p);
        sa.name="a";sa.len=1;arg.ty=&list;
        assert(!dis64typedlower(&function,&global,code,4,4,&p) && strcmp(p.reason,"opcode-type")==0);dis64typedfree(&p);
    }
    assert(!owners);
}

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
	fn.tof=&word;code[0].op=IMOVW;code[0].sm=Aimm;code[0].s.offset=7;code[0].dm=Afpind;code[0].d.reg=16;code[0].d.offset=0;
	assert(dis64typedlower(&function,globals,code,3,3,&p) && p.resultbytes==4 && p.returns[0] && p.code[0].destination==32);dis64typedfree(&p);
	code[0].d.offset=4;assert(!dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p);code[0].d.offset=0;
	code[0].d.reg=20;assert(!dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p);
	code[0].dm=Afp;code[0].d.reg=36;
	assert(!dis64typedlower(&function,globals,code,3,3,&p) && strcmp(p.reason,"undefined-result")==0);dis64typedfree(&p); /* RESULT_DEFINED */
	{
		Inst a=code[0],b=code[1];
		code[0].op=IBEQW;code[0].mm=Afp;code[0].m.reg=36;code[0].dm=Apc;code[0].branch=code+2;code[0].d.offset=2;
		code[1].op=IMOVW;code[1].sm=Aimm;code[1].s.offset=7;code[1].dm=Afpind;code[1].d.reg=16;code[1].d.offset=0;
		assert(!dis64typedlower(&function,globals,code,3,3,&p) && strcmp(p.reason,"undefined-result")==0);dis64typedfree(&p); /* RESULT_JOIN */
		code[0].branch=code+1;code[0].d.offset=1;
		assert(dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p);code[0]=a;code[1]=b;
	}
	fn.tof=&none;
	code[0].op=IJMP;code[0].sm=Anone;code[0].dm=Apc;code[0].branch=&outside;outside.pc=3;code[0].d.offset=3;
	assert(!dis64typedlower(&function,globals,code,3,3,&p));dis64typedfree(&p); /* BRANCH_OWNER */
	code[0].branch=code+2;code[0].d.offset=2;
	assert(dis64typedlower(&function,globals,code,3,3,&p));assert(p.code[0].destination==2);dis64typedfree(&p);
	for(i=0;i<3;i++)code[i].pc+=100;code[0].d.offset=102;
	assert(dis64typedlower(&function,globals,code,103,103,&p));assert(p.first==100 && p.code[0].destination==2);dis64typedfree(&p);
	assert(!dis64typedlower(&function,globals,code,65537,103,&p));dis64typedfree(&p);
	assert(owners==0 && p.code==nil && p.reason==nil);
    tabletests();
	return 0;
}
