#include "limbo.h"
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

Biobuf *bout;
char debug[256];
Inst *lastinst;
int nlabel;
static Type inttype = {.kind=Tint,.size=4}, bigtype = {.kind=Tbig,.size=8};
static Type stringtype = {.kind=Tstring};
Type *tint = &inttype, *tbig = &bigtype, *tstring = &stringtype;
static Inst code[256];
static struct { Node s,m,d; int hs,hm,hd; } operands[256];
static Decl decls[64], inputdecl;
static Node nodepool[512];
static Sym symbols[64];
static char names[64][32];
static void *allocations[64];
static int ncode, ndecl, nnode, nsymbol, nalloc, bodyop = MAXDIS+50;
static Decl *installed;
static Long values[64], initial, later;
static int reads, emitted, checks, textmode, textrows, textpending;
static uchar wire[4096];
static Long textstarts[32], textends[32], texttargets[32];

/* Keep compiler and VM oracles active when NDEBUG is selected. */
static void
testcheck(int condition)
{
	if(!condition)
		abort();
}


/* Keep all compiler-owned fixture storage bounded and releasable. */
void*
allocmem(ulong size)
{
	void *p;

	testcheck(nalloc < 64 && size < 65536);
	p = calloc(1,size);
	testcheck(p != nil);
	allocations[nalloc++] = p;
	return p;
}


/* Preserve normal realloc ownership for the production data cache. */
void*
reallocmem(void *p, ulong size)
{
	testcheck(size <= 65536);
	p = realloc(p,size);
	testcheck(p != nil);
	return p;
}


/* Valid scenarios must never escape through a compiler diagnostic. */
void
fatal(char *message, ...)
{
	fprintf(stderr,"compiler failure: %s\n",message);
	abort();
}


/* Unexpected source errors identify an unsupported fixture AST. */
void
nerror(Node *n, char *message, ...)
{
	(void)n;
	fatal(message);
}


/* Allocate stable nodepool so compiler operands survive local temporary lifetimes. */
static Node*
node(void)
{
	testcheck(nnode < 512);
	return &nodepool[nnode++];
}


/* Preserve typed integer constants used by range expansion and bounds checks. */
Node*
mkconst(Src *src, Long value)
{
	Node *n = node();
	n->src = *src;
	n->op = Oconst;
	n->ty = tint;
	n->val = value;
	n->addable = Rconst;
	return n;
}


/* Intern the generated table name without depending on frontend symbol hashing. */
Sym*
enter(char *name, int token)
{
	Sym *s;

	testcheck(nsymbol < 64 && strlen(name) < sizeof(names[0]));
	s = &symbols[nsymbol];
	strcpy(names[nsymbol],name);
	s->name = names[nsymbol++];
	s->len = strlen(name);
	s->token = token;
	return s;
}


/* Record the generated global with actual compiler declarations. */
Decl*
mkids(Src *src, Sym *sym, Type *ty, Decl *next)
{
	Decl *d;

	testcheck(ndecl < 64);
	d = &decls[ndecl];
	d->offset = ndecl++;
	d->src = *src;
	d->sym = sym;
	d->ty = ty;
	d->next = next;
	return d;
}


/* Build the interface node expected by production disvar/disdatum. */
Node*
mkdeclname(Src *src, Decl *d)
{
	Node *n = node();

	n->src = *src;
	n->op = Oname;
	n->ty = d->ty;
	n->decl = d;
	return n;
}


/* These fixtures begin with already-simplified typed case expressions. */
Node*
simplify(Node *n)
{
	return n;
}


/* Represent addressable constants without testing unrelated constant interning. */
Node*
sumark(Node *n)
{
	if(n != nil && n->op == Oconst)
		n->addable = Rmreg;
	return n;
}


/* Assign a stable local cell to each temporary requested by casecom. */
Node*
talloc(Node *n, Type *t, Node *unused)
{
	(void)unused;
	memset(n,0,sizeof(*n));
	n->op = Oname;
	n->ty = t;
	n->addable = Rreg;
	n->decl = mkids(&n->src,nil,t,nil);
	n->decl->store = Dlocal;
	return n;
}


/* Keep instruction addresses stable before their operands are emitted. */
Inst*
nextinst(void)
{
	testcheck(ncode < 256);
	code[ncode].pc = ncode;
	return &code[ncode];
}


/* Observe source operands by value while retaining their stable declaration IDs. */
Inst*
genrawop(Src *src, int op, Node *s, Node *m, Node *d)
{
	Inst *in = nextinst();
	int at = ncode++;

	in->src = *src;
	in->op = op;
	if(s){operands[at].s=*s;operands[at].hs=1;}
	if(m){operands[at].m=*m;operands[at].hm=1;}
	if(d){operands[at].d=*d;operands[at].hd=1;}
	lastinst = in;
	return in;
}


/* Model one evaluation of an otherwise nonaddressable numeric expression. */
Node*
eacom(Node *n, Node *temporary, Node *unused)
{
	(void)unused;
	talloc(temporary,n->ty,nil);
	temporary->src=n->src;
	genrawop(&n->src,n->ty==tbig?IMOVL:IMOVW,n,nil,temporary);
	return temporary;
}


/* Temporary declaration IDs remain available for execution after code generation. */
void
tfree(Node *n)
{
	(void)n;
}


/* No other expression owns deferred temporaries in the fixture. */
void
tfreenow(void)
{
}


/* Numeric range comparisons avoid subtracting potentially extreme values. */
int
casecmp(Type *t, Node *a, Node *b)
{
	(void)t;
	return (a->val>b->val)-(a->val<b->val);
}


/* Emit a body marker and continue, so production common-exit jumps are checked. */
void
scom(Node *n)
{
	genrawop(&n->src,bodyop,n,nil,nil);
}


/* Assign the table sizes used by the current Dis layout. */
Type*
usetype(Type *t)
{
	int rows = t->cse->nlab;

	if(t->kind == Tcase)
		t->size = 8+12*rows;
	else if(t->kind == Tcasel)
		t->size = 12+24*rows;
	else if(t->kind == Tgoto)
		t->size = 4+4*rows+(t->cse->iwild?4:0);
	return t;
}


/* Retain the table global that the real serializer will consume. */
void
installids(int store, Decl *d)
{
	testcheck(store == Dglobal);
	d->store = store;
	d->offset = 0;
	installed = d;
}


/* Pick-only helpers must not be reached by numeric case fixtures. */
Node*
ecom(Src *s, Node *a, Node *b)
{
	(void)s;(void)a;(void)b;
	testcheck(0);
	return nil;
}


/* Mark unsupported pick-only node cloning explicitly. */
Node*
dupn(int count, Src *s, Node *n)
{
	(void)count;(void)s;(void)n;
	testcheck(0);
	return nil;
}


/* Mark unsupported pick-only indirection explicitly. */
Node*
mkunary(int op, Node *n)
{
	(void)op;(void)n;
	testcheck(0);
	return nil;
}


/* Format only generated table names using the compatible numeric format. */
char*
seprint(char *p, char *end, char *format, ...)
{
	va_list ap;
	int n;

	va_start(ap,format);
	n = vsnprintf(p,end-p,format,ap);
	va_end(ap);
	testcheck(n >= 0 && n < end-p);
	return p+n;
}


/* Compiler debug output is disabled by every scenario. */
int
print(char *format, ...)
{
	(void)format;
	testcheck(0);
	return 0;
}

COMPILER_IMPLEMENTATION

/* Byte output and offsets preserve the real compiler writer signatures. */
int
Bputc(Biobuf *b, int c)
{
	(void)b;
	testcheck(emitted < sizeof(wire));
	wire[emitted++] = c;
	return 0;
}


/* Record canonical table bytes without substituting the data writer. */
long
Bwrite(Biobuf *b, void *p, long n)
{
	(void)b;
	testcheck(n >= 0 && emitted+n <= sizeof(wire));
	if(n)memcpy(wire+emitted,p,n);
	emitted += n;
	return n;
}


/* Expose only the accumulated wire length to diagnostic code. */
vlong
Boffset(Biobuf *b)
{
	(void)b;
	return emitted;
}


/* Type signatures do not participate in numeric case-table serialization. */
ulong
sign(Decl *d)
{
	(void)d;
	testcheck(0);
	return 0;
}


/* Interface alignment is outside numeric table fixtures. */
long
align(long n, int width)
{
	return (n+width-1)/width*width;
}


/* String initializer references must remain unexecuted in numeric table tests. */
void
asmstring(long off, Sym *s)
{
	(void)off;(void)s;
	testcheck(0);
}


/* Capture text-case row arguments independently of binary byte decoding. */
int
Bprint(Biobuf *b, char *format, ...)
{
	va_list ap;

	(void)b;
	if(!textmode)
		abort();
	va_start(ap,format);
	if(strcmp(format,",%ld,%ld,%ld") == 0){
		textstarts[textrows]=va_arg(ap,long);
		textends[textrows]=va_arg(ap,long);
		texttargets[textrows++]=va_arg(ap,long);
	}else if(strcmp(format,"\tlong\t@mp+%ld,%lld,%lld\n") == 0){
		(void)va_arg(ap,long);
		textstarts[textrows] = va_arg(ap,Long);
		textends[textrows] = va_arg(ap,Long);
		textpending = 1;
	}else if(textpending && strcmp(format,"\tword\t@mp+%ld,%ld\n") == 0){
		(void)va_arg(ap,long);
		texttargets[textrows++] = va_arg(ap,long);
		textpending = 0;
	}
	va_end(ap);
	return 0;
}

DATA_IMPLEMENTATION

typedef struct VmInst { int index; } VmInst;
static struct VmModule { int compiled; VmInst *prog; int nprog; struct VmModule *m; } vmmodule;
char exModule[]="module",exBounds[]="bounds";
#define Modlink struct VmModule
#define H ((void*)-1)
static struct VmRegs { void *s,*m,*d; VmInst *PC; struct VmModule *M; } vmregs;
#define WORD int32_t
#define UWORD uint32_t
#define LONG Long
#define Inst VmInst
#define OP(name) static void vm_##name(void)
#define R vmregs
#define W(r) (*(int32_t*)R.r)
#define UW(r) (*(uint32_t*)R.r)
#define V(r) (*(Long*)R.r)
#define JMP(r) (R.PC=(VmInst*)R.r)
/* Valid compiler-produced dispatch must not raise a VM bounds exception. */
static void vmerror(char *message) { fprintf(stderr,"VM error: %s\n",message); abort(); }
#define error vmerror
VM_IMPLEMENTATION
#undef error
#undef H
#undef Modlink
#undef WORD
#undef UWORD
#undef LONG
#undef Inst
#undef OP
#undef R
#undef W
#undef UW
#undef V
#undef JMP

static uchar *table;
static VmInst *vmprogram;

/* Decode signed Dis operands without assuming host endianness. */
static long
decode(int *at)
{
	uint32_t bits, sign;
	int c, n;

	testcheck(*at < emitted);
	c = wire[(*at)++];
	n = c < 128 ? 1 : c < 192 ? 2 : 4;
	bits = c&(n==1?127:63);
	sign = (uint32_t)1<<(n==1?6:n==2?13:29);
	while(--n > 0){testcheck(*at < emitted);bits=bits*256+wire[(*at)++];}
	return bits&sign ? (long)bits-(long)(sign*2) : bits;
}


/* Materialize fixed-width word/long records into a genuine 32-bit VM table. */
static void
loadtable(void)
{
	int at = 0, tag, kind, count, offset, width, i, j;
	uint64_t bits;

	memset(table,0,4096);
	while((tag=wire[at++]) != 0){
		kind = tag>>4;
		count = tag&15;
		if(count == 0)count = decode(&at);
		offset = decode(&at);
		width = kind==DEFW?4:8;
		testcheck((kind==DEFW || kind==DEFL) && offset >= 0 && offset+count*width <= 4096);
		for(i = 0; i < count; i++){
			bits = 0;
			for(j = 0; j < width; j++)bits=bits*256+wire[at++];
			if(width == 4){uint32_t word=bits;memcpy(table+offset+4*i,&word,4);}
			else memcpy(table+offset+8*i,&bits,8);
		}
	}
	testcheck(at == emitted);
}


/* Read an original operand observably, simulating mutation after its first read. */
static Long
value(Node *n)
{
	if(n->op == Oconst)
		return n->val;
	if(n->decl == &inputdecl)
		return reads++ == 0 ? initial : later;
	testcheck(n->decl >= decls && n->decl < decls+ndecl);
	return values[n->decl->offset];
}


/* Dispatch compiler-generated numeric operations through production VM bodies. */
static int
execute(Long input, Long changed, int compiled)
{
	int pc = 0, steps = 0, answer = -1, bodies = 0, row, target;
	Long s, m;
	int32_t sw, mw;
	int32_t *words = (int32_t*)table;

	initial=input;later=changed;reads=0;
	memset(values,0,sizeof(values));
	loadtable();
	if(compiled && installed->ty->kind != Tgoto){
		int stride=installed->ty->kind==Tcasel?6:3;
		int header=installed->ty->kind==Tcasel?2:1;
		for(row=0;row<words[0];row++){
			int at=header+row*stride+(stride==6?4:2);
			target=words[at];words[at]=(uintptr)&vmprogram[target];
		}
		row=header+words[0]*stride;
		if(words[row] != -1)words[row]=(uintptr)&vmprogram[words[row]];
	}
	vmmodule.compiled=compiled;vmmodule.prog=vmprogram;vmmodule.m=&vmmodule;vmmodule.nprog=256;
	vmregs.M=&vmmodule;
	while(pc < ncode){
		testcheck(pc >= 0 && ++steps < 512);
		Inst *in=&code[pc];
		if(in->op == bodyop){answer=operands[pc].s.val;bodies++;pc++;continue;}
		s=operands[pc].hs?value(&operands[pc].s):0;
		m=operands[pc].hm?value(&operands[pc].m):0;
		sw=s;mw=m;
		vmregs.s=(in->op==IMOVL || in->op==IBEQL || in->op==ICASEL)?(void*)&s:&sw;
		vmregs.m=in->op==IBEQL?(void*)&m:&mw;
		vmregs.PC=&vmprogram[pc+1];
		if(in->branch != nil)vmregs.d=&vmprogram[in->branch->pc];
		if(in->op==IMOVW || in->op==IMOVL || in->op==ISUBW)
			vmregs.d=&values[operands[pc].d.decl->offset];
		switch(in->op){
		case IMOVW:vm_movw();break;
		case IMOVL:vm_movl();break;
		case ISUBW:vm_subw();break;
		case IBEQW:vm_beqw();break;
		case IBEQL:vm_beql();break;
		case IBLTW:vm_bltw();break;
		case IBGTW:vm_bgtw();break;
		case IJMP:vm_jmp();break;
		case ICASE:vmregs.d=table;vm_icase();break;
		case ICASEL:vmregs.d=table;vm_casel();break;
		case IGOTO:
			/* IGOTO uses instruction PCs here; native macro lowering is not emulated. */
			vmmodule.compiled=0;vmregs.d=table+4;vm_igoto();vmmodule.compiled=compiled;
			break;
		default:testcheck(0);
		}
		pc=vmregs.PC-vmprogram;
	}
	testcheck(pc==ncode && bodies <= 1 && reads==1);
	return answer;
}


/* Compile typed range/wildcard ASTs, serialize, then check both target-PC forms. */
static void
scenario(int big, int maximum, int full, int wild, int addressable, int dense)
{
	Type type={0};
	Case cases={0};
	Label labels[2]={{0}};
	Node expression={0}, cn={0}, starts[2]={{0}}, stops[2]={{0}};
	Node clauses[3]={{0}}, branches[3]={{0}}, quals[3]={{0}}, bodies[3]={{0}}, ranges[2]={{0}}, wildcard={0};
	Long limit=big?(Long)0x7fffffffffffffffLL:0x7fffffffL;
	Long minimum=big?(-limit-1):-2147483647L-1;
	Long probes[16], low, high, v;
	int i,j,nprobes=0,nlabels=2,expected,guard=0,moves=0;

	for(i=0;i<nalloc;i++)free(allocations[i]);
	nalloc=ncode=ndecl=nnode=nsymbol=nlabel=0;
	memset(code,0,sizeof(code));memset(operands,0,sizeof(operands));
	memset(decls,0,sizeof(decls));memset(nodepool,0,sizeof(nodepool));memset(symbols,0,sizeof(symbols));
	expression.src.start.line=73;expression.src.start.pos=4;
	expression.src.stop.line=73;expression.src.stop.pos=9;
	expression.op=Oname;expression.ty=big?tbig:tint;expression.decl=&inputdecl;
	expression.addable=addressable?Rmreg:Rcant;
	cn.op=Ocase;cn.left=&expression;cn.ty=&type;cn.right=&clauses[0];
	type.kind=big?Tcasel:Tcase;type.cse=&cases;
	cases.labs=labels;cases.nlab=nlabels;
	low=full?minimum:(maximum?limit-7:-17);
	high=maximum?limit:23;
	if(full){nlabels=1;cases.nlab=1;}
	if(full==2)low=high;
	if(dense){low=limit-2;high=limit;nlabels=1;cases.nlab=1;}
	for(i=0;i<nlabels;i++){
		starts[i].op=stops[i].op=Oconst;
		starts[i].ty=stops[i].ty=expression.ty;
		starts[i].val=i?high-1:low;
		stops[i].val=(full || dense)?high:(i?high:low+1);
		labels[i].start=&starts[i];labels[i].stop=&stops[i];
		ranges[i].op=Orange;ranges[i].left=&starts[i];ranges[i].right=&stops[i];
		quals[i].left=&ranges[i];branches[i].left=&quals[i];branches[i].right=&bodies[i];
		bodies[i].val=i+1;clauses[i].left=&branches[i];
		clauses[i].right=i+1<nlabels?&clauses[i+1]:nil;
	}
	if(wild){
		wildcard.op=Owild;quals[nlabels].left=&wildcard;
		branches[nlabels].left=&quals[nlabels];branches[nlabels].right=&bodies[nlabels];
		bodies[nlabels].val=9;clauses[nlabels].left=&branches[nlabels];
		clauses[nlabels-1].right=&clauses[nlabels];
	}
	lastinst=nil;installed=nil;
	casecom(&cn);
	testcheck(installed != nil);
	for(i=0;i<ncode;i++){
		if(code[i].op==IBEQW || code[i].op==IBEQL)guard++;
		if(maximum && !dense && (code[i].op==IBEQW || code[i].op==IBEQL ||
		   code[i].op==ICASE || code[i].op==ICASEL)){
			testcheck(code[i].src.start.line==73 && code[i].src.start.pos==4);
			testcheck(code[i].src.stop.line==73 && code[i].src.stop.pos==9);
		}
		if(code[i].op==IMOVW || code[i].op==IMOVL)moves++;
	}
	emitted=0;disvar(installed->ty->size,installed);
	textmode=1;textrows=0;textpending=0;asminitializer(0,installed->init);textmode=0;
	if(!dense){
		testcheck(textrows==nlabels);
		for(i=0;i<nlabels;i++){
			testcheck(textstarts[i]==starts[i].val);
			testcheck(textends[i]==(stops[i].val==limit?limit:stops[i].val+1));
			testcheck(texttargets[i]==labels[i].inst->pc);
		}
	}
	probes[nprobes++]=minimum;probes[nprobes++]=minimum+1;
	probes[nprobes++]=limit;probes[nprobes++]=limit-1;probes[nprobes++]=limit-2;
	probes[nprobes++]=0;probes[nprobes++]=-1;
	for(i=0;i<nlabels;i++){
		probes[nprobes++]=starts[i].val;probes[nprobes++]=stops[i].val;
		if(starts[i].val>minimum)probes[nprobes++]=starts[i].val-1;
		if(stops[i].val<limit)probes[nprobes++]=stops[i].val+1;
	}
	for(j=0;j<2;j++)for(i=0;i<nprobes;i++){
		v=probes[i];expected=wild?9:-1;
		for(int k=0;k<nlabels;k++)if(v>=starts[k].val && v<=stops[k].val)expected=k+1;
		testcheck(execute(v,v,j)==expected);
		if(maximum && !dense)testcheck(execute(v,v==limit?minimum:limit,j)==expected);
		checks++;
	}
	testcheck(guard==(maximum && !dense));
	testcheck(moves >= guard);
}


/* Reserve a low address so unchanged WORD32 dispatch bodies retain real pointers. */
int
main(int argc, char **argv)
{
	int big,maximum,full,wild,addr;
	void *memory=mmap((void*)0x18000000,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);

	testcheck(memory!=MAP_FAILED && (uintptr)memory+8192 < 0x7fffffff);
	table=memory;vmprogram=(VmInst*)(table+4096);
	for(int i=0;i<256;i++)vmprogram[i].index=i;
	for(big=0;big<2;big++){
	if(argc > 1 && ((strcmp(argv[1],"word")==0 && big) ||
	   (strcmp(argv[1],"big")==0 && !big)))continue;
	for(maximum=0;maximum<2;maximum++)
	for(full=0;full<3;full++)for(wild=0;wild<2;wild++)for(addr=0;addr<2;addr++)
		scenario(big,maximum,full,wild,addr,0);
	}
	if(argc==1 || strcmp(argv[1],"word")==0)
	for(wild=0;wild<2;wild++)for(addr=0;addr<2;addr++)scenario(0,1,0,wild,addr,1);
	for(int i=0;i<nalloc;i++)free(allocations[i]);
	free(cache);
	testcheck(munmap(memory,8192)==0);
	printf("case maximum pipeline: %d interpreted/relocated dispatch scenarios passed\n",checks);
	return 0;
}
