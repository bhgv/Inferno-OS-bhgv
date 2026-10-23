/* Real MIPS compiler publication with deterministic instruction-emission boundaries. */
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>

static ulong *patch, *tinit, *base, *code, *litpool, macro[2];
static Module *mod;
void (*comvec)(void);
int cflag;
static int pass, nlit, regdelay, allocations, failat, live, emissions, helpers, checks;
static struct { void *p; ulong size; } blocks[16];
char exCphase[] = "phase";

/* Inject each allocation failure and poison ordinary allocations. */
static void*
allocate(ulong n, int clear)
{
	void *p;
	int i;

	allocations++;
	if(allocations == failat)return nil;
	assert(n > 0 && n < 65536);
	p = malloc(n); assert(p); memset(p, clear ? 0 : 0xa5, n);
	for(i = 0; i < 16 && blocks[i].p; i++);
	assert(i < 16); blocks[i].p = p; blocks[i].size = n; live++;
	return p;
}

/* Detect double frees and retain exact accounting across failed and retried compiles. */
static void
deallocate(void *p)
{
	int i;

	if(p == nil)return;
	for(i = 0; i < 16 && blocks[i].p != p; i++);
	assert(i < 16); blocks[i].p = nil; live--; free(p);
}
static void *plain(ulong n) { return allocate(n, 0); }
void *mallocz(ulong n, int clear) { return allocate(n, clear); }
#define malloc plain
#define free deallocate

/* Stub the encoders while preserving two passes, offsets and frame type selection. */
static void preamble(void) { *code++ = 0x1234; emissions++; }
static void comp(Inst *i) {
	assert(i->op == IFRAME || i->op == IRET);
	if(i->op == IFRAME)tinit[i->s.imm] = 1;
	*code++ = i->op; emissions++;
}
static void macrocode(void) { *code++ = 0x5678; emissions++; }
static struct { int o; void (*f)(void); } macinit[] = {{0, macrocode}, {0, nil}};
static void typecom(Type *t) { assert(t == mod->type[0]); helpers++; }
static void patchex(Module *m, ulong *p) { assert(m->htab == nil && p == patch); }
int segflush(void *p, ulong n) { assert(p != nil && n > 0); return 0; }
int print(char *fmt, ...) { (void)fmt; return 0; }
static void das(ulong *p) { (void)p; }
static void urk(char *s) { fprintf(stderr, "unexpected compiler error: %s\n", s); assert(0); }
IMPLEMENTATION

/* Uncached and cached preambles must survive every failed compile and a retry. */
static void
scenario(int cached, int cutoff)
{
	Module m = {0};
	Modlink instance;
	Link exports[2] = {0};
	Type types[2] = {{0}}, *table[] = {&types[0], &types[1]};
	Inst *instructions;
	int ok, before, i, succeeded;
	void *savedvec;

	memset(&instance, 0, sizeof(instance));
	assert(live == 0); allocations = 0; failat = 0; emissions = helpers = 0;
	comvec = cached ? allocate(10*sizeof(ulong), 0) : nil; savedvec = comvec;
	instructions = allocate(2*sizeof(Inst), 1);
	instructions[0].op = IFRAME; instructions[0].s.imm = 0; instructions[1].op = IRET;
	m.prog = instructions; m.nprog = 2; m.entry = instructions+1; m.ext = exports;
	m.ntype = 2; m.type = table; m.origmp = H;
	exports[0].name = "entry"; exports[0].u.pc = instructions; exports[0].frame = table[0];
	instance.m = &m; instance.nlinks = 1;
	for(i = 0; i < 1; i++){ instance.links[i].u.pc = instructions+i; instance.links[i].frame = table[0]; }
	before = live; allocations = 0; failat = cutoff;
	ok = compile(&m, 2, &instance); failat = 0;
	succeeded = cutoff == 0 || cutoff > (cached ? 3 : 4);
	assert(ok == succeeded);
	if(!ok){
		assert(!m.compiled && m.prog == instructions && m.entry == instructions+1);
		assert(exports[0].u.pc == instructions);
		for(i = 0; i < 1; i++)assert(instance.links[i].u.pc == instructions+i);
		assert(helpers == 0 && live == before+(!cached && comvec != nil));
		if(cutoff <= 2 || (!cached && cutoff == 3))assert(emissions == 0);
		assert(compile(&m, 2, &instance));
	}
	assert(m.compiled && m.prog != instructions);
	assert(m.entry == (Inst*)(base+1) && exports[0].u.pc == (Inst*)base);
	for(i = 0; i < 1; i++)assert(instance.links[i].u.pc == (Inst*)(base+i));
	assert(helpers == 3 && live == 2 && comvec != nil);
	if(cached)assert(comvec == savedvec);
	free(m.prog); free(comvec); comvec = nil; assert(live == 0); checks++;
}

/* Repeat failure/retry cycles so stale compiler globals cannot mask ownership errors. */
int
main(void)
{
	int cached, cutoff, repeat;

	for(repeat = 0; repeat < 20; repeat++)
		for(cached = 0; cached < 2; cached++)
			for(cutoff = 0; cutoff <= 5; cutoff++)scenario(cached, cutoff);
	printf("MIPS publication: %d cases\n", checks);
	return 0;
}

/* Disable only unsupported LeakSanitizer process inspection. */
int __lsan_is_turned_off(void) { return 1; }
