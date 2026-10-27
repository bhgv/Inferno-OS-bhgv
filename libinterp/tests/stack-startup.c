#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

static Proc worker;
static struct { Prog *head, *tail, *runhd, *runtl; } isched;
static Prog *slots[256], parent, *running, *created;
static Osenv parentenv;
static Pgrp pg;
static Fgrp fg;
static Egrp eg;
static struct {void *p; size_t size;} blocks[128];
static int live, allocations, failat, checks, unframes;
static char *raised;
static Module module;
static Type type, *types[1];
static Link exports[2];
static Modlink *ml;
static Inst code[2];
static union { double align; uchar bytes[512]; } framebuf, arena;
static Frame *input;
static int operation, expectedpanic, user_error;
static Prog child, *result;
static WORD operand;
static void *output;
REG R;
int mutator, nprop;
char exHeap[] = "heap", exNomem[] = "memory", exModule[] = "module", exSpawn[] = "spawn", exNilref[] = "nil";
char exBounds[] = "bounds"; // Invalid decoded frame operand range.
char Enomem[] = "memory";

/* Preserve real emulator error frames and error identity. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *s) { raised = s; nexterror(); }
void panic(char *s, ...) { if(expectedpanic && !strcmp(s, "no pids"))error(s); fprintf(stderr, "unexpected panic: %s\n", s); abort(); }
/* Disable only unsupported LeakSanitizer process inspection. */
int __lsan_is_turned_off(void) { return 1; }

/* Allocate exactly the requested span with visible uninitialized bytes and guards. */
static void*
allocate(size_t n, int clear)
{
	void *p;
	int i;

	allocations++;
	if(allocations == failat || n > 1024*1024)return nil;
	p = malloc(n+32); assert(p); memset(p, clear ? 0 : 0xa5, n); memset((uchar*)p+n, 0xa5, 32);
	for(i = 0; i < 128 && blocks[i].p; i++);
	assert(i < 128); blocks[i].p = p; blocks[i].size = n; live++;
	return p;
}

/* Check all guards before freeing tracked VM allocations or an injected user string. */
static void
releaseblock(void *p)
{
	int i, j;

	if(p == nil)return;
	for(i = 0; i < 128 && blocks[i].p != p; i++);
	if(i < 128){
		for(j = 0; j < 32; j++)assert(((uchar*)p)[blocks[i].size+j] == 0xa5);
		blocks[i].p = nil; live--;
	}
	free(p);
}

/* Model the already-tested linkmod ownership contract at the scheduler boundary. */
Modlink*
linkmod(Module *m, Import *imports, int make_mp)
{
	Heap *h;
	Modlink *link;

	assert(imports->name == nil && make_mp == 1);
	h = allocate(sizeof(Heap)+sizeof(Modlink)+sizeof(Modl), 1);
	if(h == nil){ unload(m); error(exHeap); }
	h->ref = 1; link = H2D(Modlink*, h);
	link->m = m; link->MP = H; link->prog = code; link->type = types; link->nlinks = 2;
	link->links[0].frame = link->links[1].frame = &type;
	link->links[0].u.pc = code; link->links[1].u.pc = code+1;
	return link;
}
/* Legacy startup body adapters are used only for independent baseline runs. */
Modlink *mklinkmod(Module *m, int n) { Import empty; assert(n == 0); empty.name = nil; return linkmod(m, &empty, 1); }
Heap *nheap(int n) { Heap *h = allocate(sizeof(Heap)+n, 1); if(!h)error(exHeap); h->ref = 1; return h; }
void newmp(void *d, void *s, Type *t) { (void)d; (void)s; (void)t; assert(0); }
void unload(Module *m) { assert(m->ref > 0); m->ref--; }
void destroy(void *p) {
	Heap *h;
	if(p == H)return;
	h = D2H(p); assert(h->ref > 0);
	if(--h->ref)return;
	unload(((Modlink*)p)->m); releaseblock(h);
}

/* Supply scheduler boundaries while executing real newprog and newgrp decisions. */
static Prog **pidlook(int pid) { return &slots[(uint)pid%256]; }
Prog *currun(void) { return running; }
void xec(Prog *p) { (void)p; }
void addrun(Prog *p) {
	if(isched.runtl)isched.runtl->link = p; else isched.runhd = p;
	isched.runtl = p; created = p;
}
static void addgrp(Prog *p, Prog *owner) { (void)p; (void)owner; }
static Progs *delgrp(Prog *p) { assert(p->group == nil); return nil; }
static int testincref(Ref *r) { return ++r->ref; }
void FPsave(void *p) { (void)p; }
void FPrestore(void *p) { (void)p; }
/* Model failure or cancellation at the environment-copy boundary. */
void kstrdup(char **p, char *s) {
	if(user_error == 1)error(exNomem);
	*p = malloc(strlen(s)+1); assert(*p); strcpy(*p, s);
	if(user_error == 2)error(exNomem);
	if(user_error == 3)parent.kill = "parent stopped";
}
void unframe(void) { unframes++; }

#define malloc(n) allocate((n), 1)
#define mallocz(n, clear) allocate((n), (clear))
#define free(p) releaseblock(p)
#define incref(p) testincref((Ref*)(p))
#define OP(name) void test_##name(void)
#define W(r) *((WORD*)(R.r))
#define T(r) *((void**)(R.r))
#define BIT(bt, nb) ((bt)&(1<<(nb)))
IMPLEMENTATION

/* Reset metadata and source bytes without retaining any scheduler or heap owner. */
static void
setup(void)
{
	Import empty;

	assert(live == 0 && worker.nerr == 0);
	memset(&isched, 0, sizeof(isched)); memset(slots, 0, sizeof(slots));
	memset(&module, 0, sizeof(module)); memset(&type, 0, sizeof(type));
	memset(exports, 0, sizeof(exports)); memset(&R, 0, sizeof(R));
	memset(&parent, 0, sizeof(parent)); memset(&parentenv, 0, sizeof(parentenv));
	memset(&child, 0, sizeof(child));
	pg.r.ref = fg.r.ref = eg.r.ref = 1;
	parentenv.pgrp = &pg; parentenv.fgrp = &fg; parentenv.egrp = &eg; parentenv.user = "parent";
	parent.osenv = &parentenv; running = &parent;
	type.ref = 1; type.size = sizeof(Frame)+64; type.np = 1;
	type.map[0] = 0x80>>(sizeof(Frame)/sizeof(void*)); types[0] = &type;
	module.ref = 2; module.origmp = H; module.entryt = &type; module.entry = code;
	module.type = types; module.ntype = 1; module.ext = exports;
	exports[0].name = "entry"; exports[0].frame = &type; exports[0].u.pc = code+1;
	allocations = 0; failat = -1; raised = nil; unframes = 0; created = result = nil; user_error = 0;
	empty.name = nil; ml = linkmod(&module, &empty, 1);
	R.M = ml; R.MP = H; child.R.M = ml;
	memset(framebuf.bytes, 0x67, sizeof(framebuf.bytes)); input = (Frame*)framebuf.bytes;
	input->t = &type; input->lr = code; input->fp = arena.bytes; input->mr = ml;
	*(void**)(framebuf.bytes+sizeof(Frame)) = H;
	R.s = &input; output = H; R.d = &output;
	R.SP = arena.bytes; R.TS = arena.bytes+sizeof(arena.bytes);
	allocations = 0;
}

/* Remove a successful fixture child/root and release its explicit owners. */
static void
discard(Prog *p)
{
	Osenv *env;

	env = p->osenv;
	if(env->pgrp)env->pgrp->r.ref--;
	if(env->fgrp)env->fgrp->r.ref--;
	if(env->egrp)env->egrp->r.ref--;
	free(env->user); free(p->R.EX); free(p->group); destroy(p->R.M); free(p);
	memset(&isched, 0, sizeof(isched)); memset(slots, 0, sizeof(slots)); created = nil;
}

/* Run actual stack/dispatch/startup functions inside one outer error frame. */
static int
run(void *unused)
{
	(void)unused;
	switch(operation){
	case 0: newstack(&child); break;
	case 1: R.s = &type; extend(); break;
	case 2: result = schedmod(&module); break;
	case 3: test_spawn(); break;
	case 4: test_mspawn(); break;
	case 5: test_frame(); break;
	case 6: test_mframe(); break;
	case 7: result = newprog(nil, ml); break;
	}
	return 1;
}

/* Assert precise extent boundaries and unchanged source frames, including allocation errors. */
static void
stacks(void)
{
	uchar saved[512];
	Stkext *sx;
	ulong hints[] = {0, 16, 128, 1024};
	REG before;
	int i;

	for(i = 0; i < 4; i++){
		setup(); operation = 0; module.ss = hints[i]; memmove(saved, framebuf.bytes, sizeof(saved));
		assert(kprotect(run, nil)); sx = (Stkext*)child.R.EX;
		assert(child.R.SP <= child.R.TS && child.R.FP == sx->reg.tos.fu);
		assert(child.R.TS-child.R.EX >= OA(Stkext, reg.tos.fu)+type.size);
		assert(sx->reg.TR == &type && sx->reg.SP == nil && sx->reg.EX == nil);
		assert(!memcmp(framebuf.bytes, saved, sizeof(saved)));
		assert(!memcmp(child.R.FP+sizeof(Frame), saved+sizeof(Frame), type.size-sizeof(Frame)));
		assert(((Frame*)child.R.FP)->t == nil && ((Frame*)child.R.FP)->fp == nil);
		free(child.R.EX); destroy(ml); assert(live == 0); checks++;
	}
	setup(); operation = 0; failat = 1; before = child.R;
	memmove(saved, framebuf.bytes, sizeof(saved));
	assert(!kprotect(run, nil) && raised == exNomem);
	assert(!memcmp(&before, &child.R, sizeof(before)) && !memcmp(saved, framebuf.bytes, sizeof(saved)));
	failat = -1; destroy(ml); checks++;
	setup(); operation = 1; before = R;
	assert(kprotect(run, nil)); sx = (Stkext*)R.EX;
	assert(R.SP <= R.TS && R.TS-R.EX == OA(Stkext, reg.tos.fu)+2*type.size);
	assert(sx->reg.SP == before.SP && sx->reg.TS == before.TS && sx->reg.EX == before.EX);
	assert(*(void**)((uchar*)R.s+sizeof(Frame)) == H);
	unextend(R.s); assert(R.SP == before.SP && R.TS == before.TS && R.EX == before.EX);
	destroy(ml); assert(live == 0); checks++;
	for(i = 0; i < 4; i++){
		setup(); operation = i & 1;
		if(i < 2)type.size = INT_MAX; else module.ss = (ulong)INT_MAX+1;
		assert(!kprotect(run, nil) && raised == exHeap && allocations == 0);
		destroy(ml); assert(live == 0); checks++;
	}
}

/* Compare extent geometry across frame sizes, exact fits and configured hints. */
static void
geometry(void)
{
	static int extra[] = {0, 8, 24, 64, 128, 256};
	uchar saved[512];
	REG before;
	ulong hint, minimum, expected;
	int op, i, j;

	for(op = 0; op < 2; op++)for(i = 0; i < 6; i++)for(j = 0; j < 8; j++){
		setup(); operation = op; type.size = sizeof(Frame)+extra[i];
		if(extra[i] == 0)type.np = 0;
		minimum = 4*sizeof(void*)+type.size;
		switch(j){
		case 0: hint = 0; break;
		case 1: hint = 1; break;
		case 2: hint = 15; break;
		case 3: hint = 16; break;
		case 4: hint = minimum-1; break;
		case 5: hint = minimum; break;
		case 6: hint = minimum+1; break;
		default: hint = 4096; break;
		}
		module.ss = hint; before = R; memmove(saved, framebuf.bytes, sizeof(saved));
		expected = hint < minimum ? 4*sizeof(void*)+(op ? 2 : 1)*type.size : hint;
		assert(kprotect(run, nil));
		if(op == 0){
			assert(child.R.TS-child.R.EX == expected && child.R.SP <= child.R.TS);
			assert(!memcmp(saved, framebuf.bytes, sizeof(saved))); free(child.R.EX);
		}else{
			assert(R.TS-R.EX == expected && R.SP <= R.TS);
			unextend(R.s); assert(R.SP == before.SP && R.TS == before.TS && R.EX == before.EX);
		}
		destroy(ml); assert(live == 0); checks++;
	}
}

/* Overflow and narrowing cutoffs must be decided before pointer arithmetic or copying. */
static void
limits(void)
{
	int op, i;
	int sizes[4];

	sizes[0] = sizeof(Frame)-1; sizes[1] = INT_MIN;
	sizes[2] = INT_MAX-OA(Stkext, reg.tos.fu);
	sizes[3] = (INT_MAX-OA(Stkext, reg.tos.fu))/2+1;
	for(op = 0; op < 2; op++)for(i = 0; i < 4; i++){
		setup(); operation = op; type.size = sizes[i];
		assert(!kprotect(run, nil));
		if(i < 2 || op == 1)assert(raised == exHeap && allocations == 0);
		else assert(raised == exNomem && allocations == 1);
		destroy(ml); assert(live == 0); checks++;
	}
}

/* Root startup must own a full-sized frame and leave no published process on failure. */
static void
roots(void)
{
	int count, point, i;
	void *saved;
	Frame *f;

	setup(); destroy(ml); ml = H; module.ref = 2; operation = 2; saved = R.s;
	assert(kprotect(run, nil) && result != nil); count = allocations;
	assert(R.s == saved && result == created && result->R.PC == code);
	f = (Frame*)result->R.FP;
	assert(f->lr == nil && f->mr == nil && f->fp == nil && f->t == nil);
	assert(*(void**)((uchar*)f+sizeof(Frame)) == H);
	assert(*((uchar*)f+type.size-1) == 0);
	discard(result); assert(live == 0 && module.ref == 1); checks++;
	for(point = 1; point <= count; point++){
		setup(); destroy(ml); ml = H; module.ref = 2; operation = 2; saved = R.s;
		allocations = 0; failat = point;
		assert(!kprotect(run, nil)); assert(R.s == saved && worker.nerr == 0);
		assert(live == 0 && module.ref == 1 && created == nil && isched.head == nil && isched.runhd == nil);
		checks++;
	}
	for(i = 0; i < 3; i++){
		setup(); destroy(ml); ml = H; module.ref = 2; operation = 2; saved = R.s;
		if(i == 0)module.entryt = nil;
		else if(i == 1)type.size = sizeof(Frame)-1;
		else module.ss = ULONG_MAX;
		assert(!kprotect(run, nil) && raised == exHeap && allocations == 0);
		assert(R.s == saved && live == 0 && module.ref == 1 && created == nil); checks++;
	}

}

/* Child stack failure must precede publication, and source arguments stay with the parent. */
static void
spawns(void)
{
	uchar saved[512];
	Inst *target = code+1;
	int op, point;

	for(op = 3; op <= 4; op++){
		setup(); operation = op; operand = -1; R.m = &operand;
		R.d = op == 3 ? (void*)&target : (void*)&ml;
		memmove(saved, framebuf.bytes, sizeof(saved));
		assert(kprotect(run, nil) && created != nil && unframes == 1);
		assert(created->R.PC == code+1 && created->R.SP <= created->R.TS);
		assert(!memcmp(saved, framebuf.bytes, sizeof(saved)));
		discard(created); destroy(ml); assert(live == 0 && module.ref == 1); checks++;
		for(point = 1; point <= 2; point++){
			setup(); operation = op; operand = -1; R.m = &operand;
			R.d = op == 3 ? (void*)&target : (void*)&ml;
			memmove(saved, framebuf.bytes, sizeof(saved)); failat = point;
			assert(!kprotect(run, nil) && created == nil && unframes == 0);
			assert(live == 1 && D2H(ml)->ref == 1 && !memcmp(saved, framebuf.bytes, sizeof(saved)));
			failat = -1; destroy(ml); assert(live == 0); checks++;
		}
	}
}

/* Misaligned decoded slots must preserve child entries and publication ordering. */
static void
spawnalignment(void)
{
	uchar slots[2][24],saved[512];
	Inst *target=code+1;
	int da,ma,op,point;

	for(da=0;da<8;da++)for(ma=0;ma<8;ma++)for(op=3;op<=4;op++)for(point=0;point<(getenv("SPAWN_SUCCESS_ONLY") ? 1 : 3);point++){
		setup();operation=op;operand=-1;R.m=slots[0]+ma;R.d=slots[1]+da;
		memmove(R.m,&operand,sizeof(operand));
		if(op==3)memmove(R.d,&target,sizeof(target));else memmove(R.d,&ml,sizeof(ml));
		memmove(saved,framebuf.bytes,sizeof(saved));failat=point ? point : -1;
		if(!point){
			assert(kprotect(run,nil) && created && created->R.PC==code+1 && unframes==1);
			assert(!memcmp(saved,framebuf.bytes,sizeof(saved)));discard(created);destroy(ml);
		}else{
			assert(!kprotect(run,nil) && !created && !unframes);
			assert(!memcmp(saved,framebuf.bytes,sizeof(saved)) && D2H(ml)->ref==1);
			failat=-1;destroy(ml);
		}
		assert(live==0);checks++;
	}
}

/* Nil instances and absent owners must fail before stack or process publication. */
static void
spawnmodules(void)
{
	Modlink *saved;
	Module *owner;
	REG before;
	int mode;

	for(mode = 0; mode < 4; mode++){
		setup(); operation = 4; operand = 0; R.m = &operand; R.d = &ml;
		saved = ml; owner = ml->m;
		if(mode < 2)
			ml = mode == 0 ? nil : H;
		else
			ml->m = mode == 2 ? nil : H;
		before = R;
		assert(!kprotect(run, nil) && raised == exModule);
		assert(!memcmp(&before, &R, sizeof(R)));
		assert(allocations == 0 && created == nil && unframes == 0);
		ml = saved; ml->m = owner;
		destroy(ml); assert(live == 0); checks++;
	}
}

/* Check complete positive/negative link indices and descriptor indices before access. */
static void
indices(void)
{
	WORD invalid[9] = {2, -2, -3, (WORD)(1ULL<<31), -((WORD)1<<30)};
	int count = 5;
	int op, i;

	if(sizeof(WORD) > 4){
		invalid[count++] = (WORD)(1ULL<<32); invalid[count++] = -(WORD)(1ULL<<32);
		invalid[count++] = (WORD)0x7fffffffffffffffLL;
		invalid[count++] = (WORD)(-0x7fffffffffffffffLL-1);
	}
	for(op = 4; op <= 6; op++)for(i = 0; i < count; i++){
		setup(); operation = op; operand = invalid[i]; R.m = &operand;
		if(op == 5)R.s = &operand;
		else if(op == 6)R.s = &ml;
		else R.d = &ml;
		assert(!kprotect(run, nil) && allocations == 0 && created == nil);
		destroy(ml); assert(live == 0); checks++;
	}
	for(op = 5; op <= 6; op++){
		setup(); operation = op; operand = op == 5 ? 0 : -1; R.m = &operand;
		R.s = op == 5 ? (void*)&operand : (void*)&ml;
		assert(kprotect(run, nil) && output == arena.bytes && R.SP == arena.bytes+type.size);
		destroy(ml); checks++;
	}
}

/* Frame publication must accept unaligned operands on both stack paths. */
static void
framealignment(void)
{
	union {uvlong alignment;uchar bytes[112];} slots;
	uchar expected[112];
	Frame *frame;
	int op,s,m,d,slow;

	for(op=5;op<=6;op++)for(s=0;s<8;s++)for(m=0;m<8;m++)for(d=0;d<8;d++)for(slow=0;slow<2;slow++){
		setup();operation=op;operand=op==5 ? 0 : -1;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));
		R.s=slots.bytes+8+s;R.m=slots.bytes+40+m;R.d=slots.bytes+80+d;
		if(op==5)memmove(R.s,&operand,sizeof(operand));else memmove(R.s,&ml,sizeof(ml));
		memmove(R.m,&operand,sizeof(operand));memmove(R.d,&output,sizeof(output));
		memmove(expected,slots.bytes,sizeof(expected));
		if(slow)R.TS=R.SP;
		assert(kprotect(run,nil));
		memmove(&frame,slots.bytes+80+d,sizeof(frame));
		assert(frame->t==(slow ? nil : &type) && *(void**)((uchar*)frame+sizeof(Frame))==H);
		if(slow)assert(((Stkext*)R.EX)->reg.TR==&type);
		memmove(expected+80+d,&frame,sizeof(frame));assert(!memcmp(expected,slots.bytes,sizeof(expected)));
		if(slow)unextend(frame);else assert((uchar*)frame==arena.bytes && R.SP==arena.bytes+type.size);
		destroy(ml);assert(live==0);checks++;
	}
}

/* Bad frame metadata must fail before changing the stack or destination. */
static void
frameerrors(void)
{
	REG before;
	Modlink *saved;
	Module *owner;
	int op, mode;

	for(op = 5; op <= 6; op++)for(mode = 0; mode < 5; mode++){
		setup(); operation = op; operand = op == 5 ? 0 : -1;
		R.m = &operand;
		R.s = op == 5 ? (void*)&operand : (void*)&ml;
		if(mode == 0)types[0] = exports[0].frame = nil;
		else if(mode == 1)types[0] = exports[0].frame = H;
		else if(mode == 2)type.size = 0;
		else if(mode == 3)type.size = sizeof(Frame)-1;
		else type.size = -1;
		before = R;
		assert(!kprotect(run, nil) && raised != nil && allocations == 0);
		assert(!memcmp(&before, &R, sizeof(R)) && output == H);
		destroy(ml); assert(live == 0); checks++;
	}
	for(mode = 0; mode < 2; mode++){
		setup(); operation = 6; operand = -1; R.m = &operand; R.s = &ml;
		saved = ml; ml = mode == 0 ? nil : H; before = R;
		assert(!kprotect(run, nil) && raised == exModule && allocations == 0);
		assert(!memcmp(&before, &R, sizeof(R)) && output == H);
		ml = saved; destroy(ml); assert(live == 0); checks++;
	}
	for(mode = 0; mode < 3; mode++){
		setup(); operation = 6; operand = -1; R.m = &operand; R.s = &ml;
		owner = ml->m;
		if(mode == 0)ml->m = nil;
		else if(mode == 1)ml->m = H;
		else ml->nlinks = -1;
		before = R;
		assert(!kprotect(run, nil) && raised != nil && allocations == 0);
		assert(!memcmp(&before, &R, sizeof(R)) && output == H);
		ml->m = owner; ml->nlinks = 0;
		destroy(ml); assert(live == 0); checks++;
	}

}

/* Environment errors must not leave a process in any scheduler or group list. */
static void
environments(void)
{
	Inst *target = code+1;
	uchar saved[512];
	int mode;

	for(mode = 1; mode <= 3; mode++){
		setup(); operation = 3; R.d = &target; user_error = mode;
		memmove(saved, framebuf.bytes, sizeof(saved));
		assert(!kprotect(run, nil) && created == nil && unframes == 0);
		assert(live == 1 && D2H(ml)->ref == 1 && isched.head == nil && isched.runhd == nil);
		assert(pg.r.ref == 1 && fg.r.ref == 1 && eg.r.ref == 1);
		assert(!memcmp(saved, framebuf.bytes, sizeof(saved)));
		user_error = 0; destroy(ml); assert(live == 0); checks++;
	}
}

/* The previous signed doubling path overflowed before reaching the allocator. */
static void
overflowlimit(void)
{
	setup(); operation = 1; type.size = INT_MAX;
	assert(!kprotect(run, nil) && raised == exHeap && allocations == 0);
	destroy(ml); assert(live == 0); checks++;
}

/* Exhaustion is terminal, but it must be detected before signed PID overflow. */
static void
pids(void)
{
	setup(); operation = 7; expectedpanic = 1;
	assert(!kprotect(run, nil) && created == nil && allocations == 0);
	assert(!strcmp(raised, "no pids") && live == 1 && D2H(ml)->ref == 1);
	expectedpanic = 0; destroy(ml); assert(live == 0); checks++;
}

/* Keep independent failure families available for before/after comparisons. */
int
main(void)
{
	char *only;

	only = getenv("STACK_CASES");
	if(only == nil || !strcmp(only, "stack")){ stacks(); geometry(); limits(); }
	if(only == nil || !strcmp(only, "root"))roots();
	if(only != nil && !strcmp(only,"spawnalignment"))spawnalignment();
	if(only == nil || !strcmp(only, "spawn")){ spawns(); spawnmodules(); }
	if(only == nil || !strcmp(only, "indices"))indices();
	if(only == nil || !strcmp(only, "framealignment"))framealignment();
	if(only == nil || !strcmp(only, "frameerrors"))frameerrors();
	if(only == nil || !strcmp(only, "environment"))environments();
	if(only != nil && !strcmp(only, "pid"))pids();
	if(only == nil || !strcmp(only, "overflow"))overflowlimit();
	assert(live == 0 && worker.nerr == 0);
	printf("stack/startup: %d cases\n", checks);
	return 0;
}
