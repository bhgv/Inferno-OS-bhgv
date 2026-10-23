/* The production loader's allocator/error adapters precede this fixture. */
REG R;
char exBounds[] = "bounds";
static int called = -1;
static union { long align; uchar data[256]; } fpdata, mpdata, indirectdata;
static LONG sourceword, middleword, destword;
static Inst *target;
void (*comvec)(void);
int loop_register(void);
Module *loop_load(char*, Dir*);
int matrix_register(void);
Module *matrix_load(char*, Dir*);
Module *signed_module_load(char*, Dir*);
Module *empty_load(char*, Dir*);
#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define UW(r) (*(UWORD*)R.r)
#define V(r) (*(LONG*)R.r)
#define UV(r) (*(ULONG*)R.r)
#define B(r) (*(BYTE*)R.r)
#define F(r) (*(REAL*)R.r)
#define JMP(r) R.PC = *(Inst**)R.r
OPERATIONS
DISPATCH
EXECUTOR

/* Restore identical typed storage for interpreter-versus-C differential steps. */
static void
prepare(Module *m, Modlink *ml, int index)
{
	memset(&fpdata, 0, sizeof(fpdata));
	memset(&mpdata, 0, sizeof(mpdata));
	memset(&indirectdata, 0, sizeof(indirectdata));
	*(uchar**)fpdata.data = indirectdata.data;
	*(uchar**)mpdata.data = indirectdata.data;
	*(WORD*)(fpdata.data+8) = *(WORD*)(mpdata.data+8) = *(WORD*)(indirectdata.data+8) = 2;
	*(WORD*)(fpdata.data+24) = *(WORD*)(mpdata.data+24) = 3;
	sourceword = 2; middleword = 3; destword = 0;
	memset(&R, 0, sizeof(R));
	R.M = ml; R.MP = mpdata.data; R.FP = fpdata.data; R.PC = m->prog+index;
	R.s = &sourceword; R.m = &middleword; R.d = &destword; R.t = 123;
	if(m->prog[index].op == IJMP || m->prog[index].op == IBGTW || m->prog[index].op == IBEQW){
		target = m->prog;
		R.d = &target;
	}
	called = -1;
}

/* Check all opcode selections and every legal source/destination/middle mode. */
static void
matrixcheck(void)
{
	Dir d = {0};
	Module *m;
	Modlink ml = {0};
	REG translated;
	uchar fp[256], mp[256], ind[256];
	LONG value;
	int i, observed;

	assert(matrix_register() && matrix_register());
	m = matrix_load("matrix", &d);
	assert(m != nil && m->cstep != nil && !m->compiled);
	ml.m = m; ml.prog = m->prog;
	for(i = 0; i < m->nprog; i++){
		prepare(m, &ml, i);
		assert(m->cstep() == 1);
		translated = R; observed = called; value = destword;
		memmove(fp, fpdata.data, sizeof(fp)); memmove(mp, mpdata.data, sizeof(mp));
		memmove(ind, indirectdata.data, sizeof(ind));
		prepare(m, &ml, i);
		dec[R.PC->add](); R.PC++;
		optab[m->prog[i].op]();
		assert(R.s == translated.s && R.m == translated.m && R.d == translated.d);
		assert(R.t == translated.t && R.PC == translated.PC && called == observed);
		assert(destword == value && !memcmp(fp, fpdata.data, sizeof(fp)));
		assert(!memcmp(mp, mpdata.data, sizeof(mp)) && !memcmp(ind, indirectdata.data, sizeof(ind)));
	}
	destroy(m->origmp); destroylinks(m); freemod(m); modules = nil;
	assert(live == 0);
}

/* Catch a bad PC using the same VM error stack used by ordinary operations. */
static int
badpc(void *unused)
{
	(void)unused;
	return R.M->m->cstep();
}

static int enters, leaves;

/* A partial C backend must leave the state untouched when declining a step. */
static int
decline(void)
{
	return 0;
}

/* Preserve the optional architecture-specific JIT entry protocol on fallback. */
static int
leaf(REG *reg)
{
	assert(reg == &R);
	leaves++;
	return 0;
}

/* Entry synchronization still runs before a leaf operation, just as before. */
static void
enter(void)
{
	enters++;
}

/* Run actual xec quanta, including partial-C fallback and exact-image attachment. */
static void
loopcheck(const char *file)
{
	Module *m, *plain;
	Modlink ml = {0};
	Prog process = {0};
	Dir d = {0};
	FILE *in;
	uchar image[4096], changed[4096];
	size_t length;
	int steps;
	int (*step)(void);
	Jit jit = {0};

	in = fopen(file, "rb"); assert(in != nil);
	length = fread(image, 1, sizeof(image), in); assert(fclose(in) == 0);
	/* Registering signed C code must not bypass the normal signature verifier. */
	assert(signed_module_load("signed", &d) == nil && live == 0);
	plain = empty_load("empty", &d); assert(plain != nil && plain->nprog == 0);
	destroylinks(plain); freemod(plain); modules = nil; assert(live == 0);
	/* MUSTCOMPILE fails before registration, then succeeds through generated C. */
	assert(parsemod("loop", image, length, &d) == nil && live == 0);
	assert(loop_register() && loop_register());
	m = loop_load("loop", &d);
	assert(m != nil && m->cstep != nil && !m->compiled && m->jit == nil);
	assert(m->htab && m->htab[0].ne == 1 && m->htab[0].etab[1].pc == 6);
	assert(m->ldt && !strcmp(m->ldt[0][0].name, "foreign"));
	assert(!strcmp(m->ext[0].name, ".mp") && m->ext[0].u.pc == nil);
	assert(m->origmp[0] == 'A' && m->origmp[1] == 'B');
	step = m->cstep;
	memmove(changed, image, length); changed[length-2] ^= 1;
	m->cstep = nil; disc_attach(m, changed, length); assert(m->cstep == nil);
	disc_attach(m, image, length-1); assert(m->cstep == nil);
	dontcompile = 1; disc_attach(m, image, length); assert(m->cstep == nil); dontcompile = 0;
	m->rt |= DONTCOMPILE; disc_attach(m, image, length); assert(m->cstep == nil); m->rt &= ~DONTCOMPILE;
	disc_attach(m, image, length); assert(m->cstep == step);
	ml.m = m; ml.prog = m->prog; ml.MP = m->origmp;
	memset(&fpdata, 0, sizeof(fpdata));
	process.R.M = &ml; process.R.PC = m->prog; process.R.FP = fpdata.data;
	for(steps = 0; steps < 44; steps += process.quanta){
		process.quanta = 44-steps < 3 ? 44-steps : 3;
		xec(&process);
	}
	assert(process.R.PC == m->prog+m->nprog);
	assert(*(WORD*)(fpdata.data+64) == 55 && *(WORD*)(fpdata.data+72) == 11);
	/* Reuse exactly the same loaded metadata through the interpreter route. */
	m->cstep = nil;
	memset(&fpdata, 0, sizeof(fpdata));
	process.R.PC = m->prog; process.quanta = 1;
	for(steps = 0; steps < 44; steps++) xec(&process);
	assert(process.R.PC == m->prog+m->nprog && *(WORD*)(fpdata.data+64) == 55);
	m->cstep = decline;
	jit.entry[IMOVW] = leaf; jit.enter = enter; m->jit = &jit;
	memset(&fpdata, 0, sizeof(fpdata)); process.R.PC = m->prog;
	for(steps = 0; steps < 44; steps++) xec(&process);
	assert(*(WORD*)(fpdata.data+64) == 55 && enters == 2 && leaves == 2);
	m->jit = nil;
	m->cstep = step; R = process.R;
	assert(!kprotect(badpc, nil) && worker.nerr == 0);
	/* A same-size changed image cannot satisfy MUSTCOMPILE with the stale translation. */
	plain = parsemod("changed", changed, length, &d); assert(plain == nil);
	destroy(m->origmp); destroylinks(m); freemod(m); modules = nil;
	assert(live == 0 && Tarray.ref == 1 && Tstring.ref == 1);
}

/* Exercise the emitted functions linked to production loader/decoder/xec bodies. */
int
main(int argc, char **argv)
{
	DisC invalid = {0};
	assert(argc == 2 && !disc_register(nil) && !disc_register(&invalid));
	loopcheck(argv[1]);
	matrixcheck();
	puts("dis2c: actual loader/xec/decoder, exact image identity, all opcode dispatches and 144 addressing modes passed");
	return 0;
}
