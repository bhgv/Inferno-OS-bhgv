#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>

static ulong *patch, *tinit, *base, *code, *litpool, macro[8];
static Module *mod;
void (*comvec)(void);
static int pass, nlit, regdelay, live, checks;
int cflag;
static struct { void *p; ulong size; } blocks[16];
char exCphase[] = "phase";
#define ROMABLE 0
#define RELPC(pc) ((ulong)(base+(pc)))

/* Track exact payload sizes and poison uninitialized relocation storage. */
static void*
allocate(ulong n, int clear)
{
	void *p;
	int i;

	assert(n > 0 && n < 65536);
	p = malloc(n+16); assert(p);
	memset(p, clear ? 0 : 0xa5, n); memset((uchar*)p+n, 0x5a, 16);
	for(i = 0; i < 16 && blocks[i].p; i++)
		;
	assert(i < 16); blocks[i].p = p; blocks[i].size = n; live++;
	return p;
}

/* Check guard bytes and release every compiler allocation exactly once. */
static void
deallocate(void *p)
{
	int i, j;

	if(p == nil)return;
	for(i = 0; i < 16 && blocks[i].p != p; i++)
		;
	assert(i < 16);
	for(j = 0; j < 16; j++)assert(((uchar*)p)[blocks[i].size+j] == 0x5a);
	blocks[i].p = nil; live--; free(p);
}

/* Supply ordinary and cleared allocations at the real driver's boundaries. */
static void*
plain(ulong n)
{
	return allocate(n, 0);
}
/* Preserve the driver's cleared-allocation request. */
void*
mallocz(ulong n, int clear)
{
	return allocate(n, clear);
}
#define malloc plain
#define free deallocate

/* Existing cached helpers require no further emission in this fixture. */
static void
preamble(void)
{
}

/* Match call/helper encoders that index the PC immediately after the last opcode. */
static void
comp(Inst *i)
{
	assert(i->op == IRET);
	if(pass && i == mod->prog+mod->nprog-1)
		assert(patch[mod->nprog] == mod->nprog);
	*code++ = 0x1234;
}

/* Keep helper emission separate so the terminal Dis PC precedes macro bodies. */
static void
macrocode(void)
{
	*code++ = 0x5678;
}
static struct { int o; void (*f)(void); } macinit[] = {{0, macrocode}, {0, nil}};
static struct { int idx; void (*gen)(void); char *name; } mactab[] = {{0, macrocode, "macro"}};

/* No export or instruction in these fixtures selects a frame helper. */
static void
typecom(Type *t)
{
	(void)t;
	assert(0);
}

/* There are no exception handlers to relocate in these straight-line fixtures. */
static void
patchex(Module *m, ulong *p)
{
	assert(m->htab == nil && p == patch);
}

/* Observe publication without executing emitted words as host machine code. */
int
segflush(void *p, ulong n)
{
	assert(p && n);
	return 0;
}
/* Keep optional driver diagnostics from requiring the host formatting library. */
int
print(char *fmt, ...)
{
	(void)fmt;
	return 0;
}
/* Disassembly is disabled by cflag and must not affect emission. */
static int
das(ulong *p, ...)
{
	(void)p;
	return 1;
}
/* Any compiler phase mismatch is a test failure. */
static void
urk(char *s)
{
	(void)s;
	assert(0);
}
/* These valid fixtures must not raise a VM error. */
void
error(char *s)
{
	(void)s;
	assert(0);
}

/* LeakSanitizer cannot inspect threads; all fixture allocations are tracked above. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Vary terminal instruction positions while preserving the complete driver's two passes. */
int
main(void)
{
	Module m;
	Type descriptor = {0}, *table[] = {&descriptor};
	Link exports[1] = {{0}};
	Inst *instructions;
	int size, i;

	for(size = 1; size <= 32; size++){
		memset(&m, 0, sizeof(m));
		instructions = allocate(size*sizeof(Inst), 1);
		for(i = 0; i < size; i++)instructions[i].op = IRET;
		m.prog = instructions; m.nprog = size; m.entry = instructions+size-1;
		m.ext = exports; m.ntype = 1; m.type = table; m.origmp = H;
		comvec = allocate(16, 0);
		assert(compile(&m, size, nil));
		assert(m.compiled && m.prog == (Inst*)base && m.entry == (Inst*)(base+size-1));
		free(m.prog); free(comvec); comvec = nil;
		assert(live == 0); checks++;
	}
	printf("legacy JIT terminal PC: %d cases\n", checks);
	return 0;
}
