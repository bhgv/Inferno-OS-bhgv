/* Fill direct decoded operands without serializing native pointer values. */
static void
callinstruction(Inst *instruction, int op, int source, int destination, WORD value, WORD offset)
{
	instruction->op = op;
	instruction->add = SRC(source)|DST(destination);
	instruction->s.imm = value;
	instruction->d.ind = offset;
}


/* Address one argument cell through a full-width prepared frame pointer. */
static void
callargument(Inst *instruction, int op, int source, WORD value, ushort frameoffset, ushort offset)
{
	callinstruction(instruction, op, source, AIND|AFP, value, 0);
	instruction->d.i.f = frameoffset;
	instruction->d.i.s = offset;
}


/* Construct trusted native instructions; this is deliberately not a file loader. */
static Module*
callmodule(int jit, int grow, int failure, int padding)
{
	Module *m;
	Inst *p;
	Heap *data;
	uchar rootmap[] = {0, 0x10}; // Root owner at native byte88.
	uchar argumentmap[] = {0, 0x80}; // Managed argument at native byte64.

	m = mallocz(sizeof(*m), 1);
	require(m != nil);
	m->origmp = H;
	m->ref = 1;
	m->rt = DONTCOMPILE;
	m->ntype = 3;
	m->type = mallocz(3*sizeof(Type*), 1);
	require(m->type != nil);
	m->type[0] = dtype(freeheap, 4, nil, 0);
	m->type[1] = dtype(freeheap, 96, rootmap, sizeof(rootmap));
	m->type[2] = dtype(freeheap, 80+padding, argumentmap, sizeof(argumentmap));
	require(m->type[0] != nil && m->type[1] != nil && m->type[2] != nil);
	m->entryt = m->type[1];
	m->ss = grow ? 0 : 4096;
	m->nprog = 15;
	m->prog = mallocz(m->nprog*sizeof(Inst), 1);
	require(m->prog != nil);
	p = m->prog;
	callinstruction(p+0, IFRAME, AIMM, AFP, 2, 40);
	callargument(p+1, IMOVP, AFP, 88, 40, 64);
	/* Prepare an outer call before evaluating its argument in another call. */
	callinstruction(p+2, IFRAME, AIMM, AFP, 2, 56);
	callargument(p+3, IMOVP, AFP, 88, 56, 64);
	if(failure == 2)
		callinstruction(p+4, IDIVW, AIMM, AFP, 0, 72);
	else
		callargument(p+4, IMOVW, AIMM, 41, 56, 72);
	/* Full-width native result addresses come only from this trusted constructor. */
	callargument(p+5, IMOVL, AFP, 48, 56, 32);
	callinstruction(p+6, ICALL, AFP, AIMM, 56, 0);
	p[6].d.ins = failure == 3 ? nil : p+12;
	callargument(p+7, IMOVW, AIMM, 41, 40, 72);
	callargument(p+8, IMOVL, AFP, 48, 40, 32);
	callinstruction(p+9, ICALL, AFP, AIMM, 40, 0);
	p[9].d.ins = p+12;
	callinstruction(p+10, IMOVW, AFP, AMP, 80, 0);
	callinstruction(p+11, IEXIT, AXXX, AXXX, 0, 0);
	callinstruction(p+12, failure == 1 ? IDIVW : IADDW, AIMM, AFP,
		failure == 1 ? 0 : 1, 72);
	callargument(p+13, IMOVW, AFP, 72, 32, 0);
	callinstruction(p+14, IRET, AXXX, AXXX, 0, 0);
	m->entry = p;
	data = heapz(m->type[0]);
	m->origmp = H2D(uchar*, data);
	m->name = strdup("trusted-local-call");
	m->path = strdup("trusted-local-call");
	require(m->name != nil && m->path != nil);
	if(jit)
		require(compile(m, m->nprog, nil));
	return m;
}


/* Run real FRAME/CALL/RET across quanta and extents, checking complete ownership release. */
static void
runcall(int jit, int grow, int failure, int quanta, int padding, int saved)
{
	Module *m;
	Modlink *link;
	Frame *entry, *source;
	Prog process;
	REG snapshot, *cleanup;
	void *result, *owner;
	int before, steps, done, extents;
	Stkext *sx;
	WORD value;

	m = callmodule(jit, grow, failure, padding);
	memset(&process, 0, sizeof(process));
	m->ref++;
	link = mklinkmod(m, 0);
	link->MP = m->origmp;
	D2H(link->MP)->ref++;
	process.R.M = link;
	process.R.PC = m->entry;
	entry = mallocz(m->entryt->size, 1);
	require(entry != nil);
	entry->t = m->entryt;
	initmem(m->entryt, entry);
	source = entry;
	R.s = &source;
	newstack(&process);
	free(entry);
	owner = H2D(void*, heapz(m->type[0]));
	memmove(process.R.FP+88, &owner, sizeof(owner));
	result = process.R.FP+80;
	memmove(process.R.FP+48, &result, sizeof(result));
	process.quanta = quanta;
	before = raised;
	message[0] = 'x';
	for(steps=0; steps<64; steps++){
		done = kprotect(execute, &process);
		if(done == 0)
			break;
	}
	require(steps < 64 && raised == before+1 && worker.nerr == 0);
	require(failure ? strcmp(message, failure == 3 ? exBounds : exZdiv) == 0 : message[0] == 0);
	memmove(&value, m->origmp, sizeof(value));
	require(value == (failure ? 0 : 42));
	/* Each retained argument must still be owned until return or terminal unwind. */
	require(D2H(owner)->ref == (failure ? 3 : 1));
	extents = 0;
	for(sx=(Stkext*)R.EX; sx != nil; sx=(Stkext*)sx->reg.EX)
		extents++;
	require(extents == (grow && failure ? 3 : 1));
	snapshot = R;
	cleanup = saved ? &snapshot : &R;
	destroystack(cleanup);
	require(cleanup->EX == nil && cleanup->FP == nil && cleanup->M == H && m->ref == 1);
	unload(m);
	if(live != 0){
		fprintf(stderr, "unentered frame owner leaked: jit=%d grow=%d failure=%d live=%d\n", jit, grow, failure, live);
		exit(10);
	}
}


/* Keep scalar file admission separate from trusted native call execution. */
int
main(int argc, char **argv)
{
	int jit, grow, failure, q, padding, saved, cases;
	int quanta[] = {1, 2, 3, 7, 16, 64, 1000};
	uchar argumentmap[] = {0, 0x80}; // Reference at native byte64.

	require(sizeof(WORD) == 4 && sizeof(void*) == 8);
	/* Existing managed descriptor admission requires a pointer-aligned extent. */
	require(dtype(freeheap, 84, argumentmap, sizeof(argumentmap)) == nil);
	optab[IFRAME] = frame;
	optab[ICALL] = call;
	optab[IRET] = ret;
	optab[IMOVP] = movp;
	optab[IMOVW] = movw;
	optab[IMOVL] = movl;
	optab[IADDW] = addw;
	optab[IDIVW] = divw;
	optab[IEXIT] = iexit;
	if(argc == 3){
		runcall(atoi(argv[1]), atoi(argv[2]), 2, 1, 0, 0);
		return 0;
	}
	require(argc == 1);
	cases = 0;
	for(jit=0; jit<2; jit++)
		for(grow=0; grow<2; grow++)
			for(failure=0; failure<4; failure++)
				for(q=0; q<(int)nelem(quanta); q++)
					for(padding=0; padding<=8; padding+=8)
						for(saved=0; saved<2; saved++){
							runcall(jit, grow, failure, quanta[q], padding, saved);
							cases++;
						}
	printf("PASS trusted native local calls: %d C/RX/quanta/extent/error cases; file CALL ABI unchanged\n", cases);
	return 0;
}
