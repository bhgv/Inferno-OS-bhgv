/* Execute real loader growth and VM type consumers with deliberately moving tables. */
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "pool.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>

static Proc worker;
REG R;
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
Type Tarray = {1}, Tlist = {1};
char exNilref[] = "nil";
char exModule[] = "module", exHeap[] = "heap", exNegsize[] = "negative", exNomem[] = "memory";
static Heap *allocated;
static Type *seen;
static int effects, failresize, operation, checks, sa, ma, da;
static union {uvlong alignment;uchar bytes[128];} operands;
static uchar operandbefore[128];
static size_t tablesize;
static Prog running;
Prog *currun(void) { return &running; }
void unextend(Frame *f) { (void)f; assert(0); }
static WORD indexvalue, count = 1;
static Modlink *source;
static void *result;
static uchar input[256], output[256];
static union { Frame align; uchar bytes[512]; } frames;
#define teststack frames.bytes

/* Use emulator exception frames so rejected instructions return to their caller. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *s) { (void)s; nexterror(); }
void kwerrstr(char *s, ...) { (void)s; }
void *mallocz(ulong n, int clear) { (void)clear; return calloc(1, n); }
int __lsan_is_turned_off(void) { return 1; }

/* Force every successful resize to free the old table, as a real realloc may do. */
static void*
moving(void *p, size_t n)
{
	void *q;

	if(failresize)return nil;
	q = malloc(n); assert(q);
	memmove(q, p, tablesize < n ? tablesize : n);
	free(p); tablesize = n;
	return q;
}
#define realloc moving
#define msize(p) 0

/* Bound heap allocations and record observable work before publishing results. */
void*
poolalloc(Pool *p, ulong n)
{
	(void)p;
	assert(allocated == nil && n < 4096);
	allocated = calloc(1, n); assert(allocated); effects++;
	return allocated;
}
void destroy(void *p) { assert(p == H); }
void markheap(Type *t, void *p) { (void)t; (void)p; }
void freeheap(Heap *h, int swept) { (void)h; (void)swept; }
void extend(void) { assert(0); }
void movtmp(void) { assert(0); }
void newc(Type *t, void (*mover)(void)) { assert(mover == movtmp); seen = t; effects++; }

#define OP(name) static void name(void)
#define W(r) (*(WORD*)R.r)
#define P(r) (*(WORD**)R.r)
#define T(r) (*(void**)R.r)
#define L(r) (*(List**)R.r)
#define BIT(bt,nb) ((bt)&(1<<(nb)))
IMPLEMENTATION

/* Drive ten consumers with the same full-width descriptor index. */
static int
run(void *unused)
{
	(void)unused;
	memset(operands.bytes,0xa5,sizeof(operands.bytes));
	R.s=operands.bytes+sa;R.m=operands.bytes+24+ma;R.d=operands.bytes+64+da;
	memmove(R.s,&indexvalue,sizeof(indexvalue));memmove(R.m,&indexvalue,sizeof(indexvalue));
	memmove(R.d,&result,sizeof(result));
	if(operation==2)memmove(R.s,&source,sizeof(source));
	if(operation==3 || operation==4)memmove(R.s,&count,sizeof(count));
	memmove(operandbefore,operands.bytes,sizeof(operandbefore));
	R.SP = teststack; R.TS = teststack+sizeof(teststack);
	switch(operation){
	case 0: new(); break;
	case 1: newz(); break;
	case 2: mnewz(); break;
	case 3: newa(); break;
	case 4: newaz(); break;
	case 5: newcmp(); break;
	case 6: R.s = input+sa; consmp(); break;
	case 7: R.s = input+sa; R.d = output+da; movmp(); break;
	case 8: frame(); break;
	case 9: seen = moduletype(source, indexvalue); break;
	}
	if(operation!=7){
		memmove(&result,operands.bytes+64+da,sizeof(result));
		memmove(operandbefore+64+da,&result,sizeof(result));
	}
	assert(!memcmp(operandbefore,operands.bytes,sizeof(operandbefore)));
	return 1;
}

/* Release the exact Type references taken by successful real heap operations. */
static void
releaseheap(void)
{
	Array *a;
	List *l;

	if(allocated == nil)return;
	if(allocated->t == &Tarray){
		a = H2D(Array*, allocated); freetype(a->t); Tarray.ref--;
	}else if(allocated->t == &Tlist){
		l = H2D(List*, allocated); freetype(l->t); Tlist.ref--;
	}else
		freetype(allocated->t);
	free(allocated); allocated = nil;
}

/* A native builtin sees the published callee, then mcall restores the caller. */
static int callbacks;
static Modlink *callee;
static void
builtin(void *frame)
{
	assert(R.M == callee && R.FP == frame);
	callbacks++;
}

/* Invoke real mcall under protection without changing operand or frame storage. */
static int
call(void *unused)
{
	(void)unused;
	mcall();
	return 1;
}

/* Both import and export indices must be checked before acquiring module refs. */
static void
calls(void)
{
	struct { Heap h; Modlink ml; } target;
	Module module = {0}, callerowner = {0};
	Modlink caller = {0}, *destination;
	Link exports[2] = {0};
	Frame frame, before, *argument;
	Type frametype = {0};
	Inst code[2];
	REG saved;
	WORD indices[8], operand;
	int i, n, kind, sa, ma, da, fa;
	uchar framebytes[sizeof(Frame)+8];
	uchar slots[3][24];

	memset(&target, 0, sizeof(target));
	target.h.ref = 1; target.ml.m = &module; target.ml.nlinks = 1;
	target.ml.MP = (uchar*)0x2000; module.ext = exports;
	exports[0].name = "entry"; caller.m = &callerowner;
	callee = destination = &target.ml; argument = &frame;
	indices[0] = 0; indices[1] = -1; indices[2] = 1; indices[3] = -2;
	indices[4] = INT_MIN; indices[5] = INT_MAX; n = 6;
	if(sizeof(WORD) > 4){ indices[n++] = (WORD)((uvlong)1<<32); indices[n++] = (WORD)-((vlong)1<<32); }
	for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++)for(fa=0;fa<8;fa++)
	for(kind = 0; kind < 5; kind++)for(i = 0; i < n; i++){
		argument=kind==3 ? nil : kind==4 ? H : (Frame*)(framebytes+fa);
		memset(&frame, 0xa5, sizeof(frame)); frame.t = nil; before = frame;
		memset(&R, 0, sizeof(R)); R.M = &caller; R.PC = code;
		R.FP = output; R.SP = input; R.MP = output;
		operand = indices[i]; R.s = slots[0]+sa; R.d = slots[1]+da; R.m = slots[2]+ma;
		memmove(R.s,&argument,sizeof(argument));memmove(R.d,&destination,sizeof(destination));memmove(R.m,&operand,sizeof(operand));
		target.ml.prog = kind == 2 ? nil : code; caller.compiled = kind == 1;
		if(kind == 2){
			frame.t = before.t = &frametype;
			target.ml.links[0].u.runt = exports[0].u.runt = builtin;
		}else
			target.ml.links[0].u.pc = exports[0].u.pc = code+1;
		memmove(framebytes+fa,&frame,sizeof(frame));
		saved = R;
		if(i >= 2 || kind>=3){
			assert(!kprotect(call, nil));
			assert(memcmp(&R, &saved, sizeof(R)) == 0);
			assert(memcmp(framebytes+fa, &before, sizeof(frame)) == 0 && target.h.ref == 1);
		}else{
			assert(kprotect(call, nil));
			memmove(&frame,framebytes+fa,sizeof(frame));
			assert(frame.lr == code && frame.fp == output && frame.mr == &caller);
			if(kind == 2){
				assert(R.M == &caller && R.FP == output && R.SP == (uchar*)argument);
				assert(target.h.ref == 1 && R.t == 0);
			}else{
				assert(R.M == &target.ml && R.FP == (uchar*)argument);
				assert(R.PC == code+1 && R.MP == target.ml.MP && R.t == 1);
				assert(target.h.ref == 2 && R.IC == (kind == 1)); target.h.ref--;
			}
		}
		checks++;
	}
	assert(callbacks == 2*512*8 && worker.nerr == 0);
}

/* A stale instance cache must never decide which descriptor an instruction uses. */
int
main(int argc, char **argv)
{
	Module m = {0};
	Modlink owner = {0}, peer = {0};
	F_Loader_tnew f = {0};
	WORD ret, bad[6];
	Type *saved, *expected;
	Type **table;
	int i, op, nbad;

	if(argc > 1 && strcmp(argv[1], "calls") == 0){ calls(); printf("module calls: %d cases\n", checks); return 0; }
	m.origmp = H; owner.m = peer.m = &m;
	m.type = malloc(sizeof(Type*)); assert(m.type); tablesize = sizeof(Type*);
	owner.type = peer.type = m.type; source = &peer; R.M = &peer;
	f.mp = &owner; f.ret = &ret; f.map = H; f.size = sizeof(Frame)+16;
	for(i = 0; i < 32; i++){
		Loader_tnew(&f); assert(ret == i && m.ntype == i+1);
		assert(owner.type == m.type);
	}
	/* peer.type still holds the first, freed allocation; never dereference it. */
	table = m.type; saved = table[0];
	failresize = 1; Loader_tnew(&f); failresize = 0;
	assert(ret == -1 && m.ntype == 32 && m.type == table && m.type[0] == saved); checks++;
	memset(input, 0x5a, sizeof(input));
	for(op = 0; op < 10; op++){
		operation = op;
		for(i = 0; i < m.ntype; i++)for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++){
			indexvalue = i; result = H; effects = 0; seen = nil;
			memset(output, 0, sizeof(output)); expected = m.type[i];
			assert(kprotect(run, nil));
			if(op == 5 || op == 9)assert(seen == expected);
			else if(op == 7)assert(memcmp(input+sa, output+da, expected->size) == 0);
			else if(op == 8)assert(((Frame*)result)->t == expected);
			else if(op == 3 || op == 4)assert(((Array*)result)->t == expected);
			else if(op == 6)assert(((List*)result)->t == expected);
			else assert(D2H(result)->t == expected);
			releaseheap(); assert(expected->ref == 1); checks++;
		}
		bad[0] = -1; bad[1] = INT_MIN; bad[2] = m.ntype; bad[3] = INT_MAX; nbad = 4;
		if(sizeof(WORD) > 4){ bad[nbad++] = (WORD)((uvlong)1<<32); bad[nbad++] = (WORD)-((vlong)1<<32); }
		for(i = 0; i < nbad; i++){
			indexvalue = bad[i]; result = H; effects = 0; seen = nil;
			memset(output, 0, sizeof(output));
			assert(!kprotect(run, nil));
			assert(!memcmp(operandbefore,operands.bytes,sizeof(operandbefore)));
			assert(result == H && effects == 0 && allocated == nil && seen == nil);
			assert(output[0] == 0 && R.SP == teststack && worker.nerr == 0); checks++;
		}
		indexvalue = 0;
		m.type[0] = nil; assert(!kprotect(run, nil)); m.type[0] = saved; checks++;
		saved->size = -1; assert(!kprotect(run, nil)); saved->size = sizeof(Frame)+16; checks++;
		m.type = nil; assert(!kprotect(run, nil)); m.type = table; checks++;
	}
	for(i = 0; i < m.ntype; i++)freetype(m.type[i]);
	free(m.type);
	assert(Tarray.ref == 1 && Tlist.ref == 1 && worker.nerr == 0);
	printf("module types/calls: %d cases\n", checks);
	return 0;
}
