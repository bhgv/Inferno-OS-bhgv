#include "dis64-bound-call.h"

#define ResultCode 256U // Bounded raw fixtures; no general loader or allocation policy is introduced.

static Dis64CallDecoded resultdecoded[ResultCode]; // Owned decode facts overwritten by preflight.
static Dis64InitWork resultwork[ResultCode]; // Disposable numeric/return analysis scratch.
static unsigned int resultqueue[ResultCode]; // Shared bounded verifier queue.
static Dis64BoundCall resultproof[Dis64LocalFunctions]; // Complete atomic proof records.
static Dis64TypeTable resulttypes; // Borrowed validated descriptors of the current raw vector.
static unsigned int resultcount, resultentry, resultframe; // Current admitted fixture geometry.
static int resultdirectcontrol; // Run direct-entry refusal probes in isolated guard-removal controls.
static int resultentrycontrol; // Skip repeated cache reuse only when deliberately publishing a raw entry.
static int resultdirectcount; // Number of checked decoded C/published native entry probes.
static int resultcachecount; // Bad published aliases rejected before cache reuse.


/* Semantic controls must stop before a truncated address can be consumed by CALL. */
static void
resultcheck(int condition, const char *text)
{
	if(!condition){
		fprintf(stderr, "%s\n", text);
		exit(10);
	}
}


/* Read independent big-endian fixture words without a production metadata decoder. */
static unsigned int
resultnumber(FILE *file)
{
	uchar bytes[4];

	require(fread(bytes, 1, 4, file) == 4);
	return ((unsigned int)bytes[0]<<24)|((unsigned int)bytes[1]<<16)|((unsigned int)bytes[2]<<8)|bytes[3];
}


/* Construct actual owned VM descriptors/instructions only AFTER complete raw proof.
 * Existing direct lowering is reused; only the proved indirect destination is installed here.
 * This is an owning-source trusted fixture, deliberately not a production file-loader entry.
 */
static Module*
resultmodule(int jit, int grow)
{
	Module *module;
	Dis64Type type;
	unsigned int i, mode;
	Inst *instruction;

	require(sizeof(WORD) == 4 && sizeof(void*) == 8);
	for(i=0; i<resultcount; i++){
		require(dis64runtimeinstruction(&resultdecoded[i].instruction));
		if(UDST(resultdecoded[i].instruction.mode) == (AFP|AIND))
			require(resultdecoded[i].instruction.destination >= 0 && resultdecoded[i].instruction.destination <= 65535 &&
				resultdecoded[i].secondary >= 0 && resultdecoded[i].secondary <= 65535);
	}
	module = mallocz(sizeof(*module), 1);
	require(module != nil);
	module->origmp = H;
	module->ref = 1;
	module->rt = DONTCOMPILE;
	module->ntype = resulttypes.count;
	module->type = mallocz(module->ntype*sizeof(Type*), 1);
	require(module->type != nil);
	for(i=0; i<resulttypes.count; i++){
		require(dis64typeget(&resulttypes, i, &type));
		module->type[i] = dtype(freeheap, type.bytes, nil, 0);
		require(module->type[i] != nil);
	}
	module->entryt = module->type[resultframe];
	module->ss = grow ? 0 : 4096;
	module->nprog = resultcount;
	module->prog = mallocz(module->nprog*sizeof(Inst), 1);
	require(module->prog != nil);
	for(i=0; i<resultcount; i++){
		instruction = module->prog+i;
		dis64runtimeinst(instruction, &resultdecoded[i].instruction, module->prog);
		mode = UDST(resultdecoded[i].instruction.mode);
		if(mode == (AFP|AIND)){
			instruction->d.i.f = resultdecoded[i].instruction.destination;
			instruction->d.i.s = resultdecoded[i].secondary;
		}
	}
	module->entry = module->prog+resultentry;
	module->cstep = native64lea8step;
	module->name = strdup("verified-result-call");
	module->path = strdup("verified-result-call");
	require(module->name != nil && module->path != nil);
	if(jit)
		require(compile(module, module->nprog, nil));
	return module;
}


/* Enter through production newstack and leave the module owned for independent assertions. */
static void
resultprocess(Module *module, Prog *process)
{
	Modlink *link;
	Frame *entry, *source;

	memset(process, 0, sizeof(*process));
	module->ref++;
	link = mklinkmod(module, 0);
	link->MP = H;
	process->R.M = link;
	process->R.PC = module->entry;
	entry = mallocz(module->entryt->size, 1);
	require(entry != nil);
	entry->t = module->entryt;
	source = entry;
	R.s = &source;
	newstack(process);
	free(entry);
}


/* Return the current prepared owner without interpreting the result-address bytes. */
static Frame*
resultprepared(uchar *root, int pointer)
{
	Frame *frame;

	memmove(&frame, root+pointer, sizeof(frame));
	require(frame != nil && frame != H);
	return frame;
}


/* Execute raw-proved scalar calls, checking full addresses, partial-write guards and cleanup. */
static void
resultrun(int jit, int grow, int failure, int quanta, int saved, unsigned int kind,
	uvlong expected, int allocationfailure, int chain, uvlong second)
{
	Module *module;
	Prog process;
	Frame *pending;
	REG snapshot, *cleanup;
	uchar *root, wanted[8], actual[8], poison[8], localpoison[8], *tail;
	void *address;
	unsigned int width;
	int steps, before, done, checked, pointer, copied;
	uvlong tailvalue;

	module = resultmodule(jit, grow);
	resultprocess(module, &process);
	root = process.R.FP;
	tail = root+module->entryt->size-8;
	memset(poison, 0xa5, sizeof(poison));
	memset(localpoison, 0x3c, sizeof(localpoison));
	memmove(tail, poison, 8);
	memmove(root+80, localpoison, 8);
	memmove(root+88, localpoison, 8);
	process.quanta = 1;
	before = raised;
	checked = copied = 0;
	pointer = resultdecoded[1].instruction.destination;
	if(allocationfailure == 1)
		failat = allocations+1;
	for(steps=0; steps<128; steps++){
		done = kprotect(execute, &process);
		if(done == 0)
			break;
		/* The first four wire instructions end immediately after the real LEA. */
		if((checked == 0 && process.R.PC == module->prog+4) ||
		   (chain && checked == 1 && process.R.PC == module->prog+10)){
			pending = resultprepared(root, pointer);
			memmove(actual, (uchar*)pending+32, 8);
			address = tail;
			memmove(wanted, &address, 8);
			resultcheck(memcmp(actual, wanted, 8) == 0, "native64 result address mismatch");
			checked++;
			if(!chain || checked == 2)
				process.quanta = quanta;
		}
		if(chain && !copied && process.R.PC == module->prog+6){
			memmove(wanted, localpoison, 8);
			if(kind == Dis64Int || kind == Dis64Bool){
				unsigned int word = expected;
				memmove(wanted, &word, 4);
			}else
				memmove(wanted, &expected, 8);
			memmove(actual, root+80, 8);
			require(memcmp(actual, wanted, 8) == 0);
			copied = 1;
			if(allocationfailure == 2)
				failat = allocations+1;
		}
	}
	failat = 0;
	require(steps < 128 && raised == before+1 && worker.nerr == 0);
	require(allocationfailure ? strcmp(message, exNomem) == 0 : failure ? strcmp(message, exZdiv) == 0 : message[0] == 0);
	require(allocationfailure == 1 ? checked == 0 : allocationfailure == 2 ? checked == 1 : checked == (chain ? 2 : 1));
	require(!chain || allocationfailure == 1 || copied);
	width = kind == Dis64Int || kind == Dis64Bool ? 4 : 8;
	memmove(wanted, poison, 8);
	tailvalue = chain && failure != 1 && !allocationfailure ? second : expected;
	if(chain && allocationfailure != 1 || !chain && !allocationfailure && failure != 1){
		if(width == 4){
			unsigned int word = tailvalue;
			memmove(wanted, &word, 4);
		}else
			memmove(wanted, &tailvalue, 8);
	}
	memmove(actual, tail, 8);
	require(memcmp(actual, wanted, 8) == 0);
	memmove(wanted, localpoison, 8);
	if(chain ? allocationfailure != 1 : !failure && !allocationfailure){
		if(width == 4){
			unsigned int word = expected;
			memmove(wanted, &word, 4);
		}else
			memmove(wanted, &expected, 8);
	}
	memmove(actual, root+80, 8);
	require(memcmp(actual, wanted, 8) == 0);
	if(chain){
		memmove(wanted, localpoison, 8);
		if(!failure && !allocationfailure){
			if(width == 4){
				unsigned int word = second;
				memmove(wanted, &word, 4);
			}else
				memmove(wanted, &second, 8);
		}
		memmove(actual, root+88, 8);
		require(memcmp(actual, wanted, 8) == 0);
	}
	/* A physical tail write before failure never executes the caller's result copy. */
	snapshot = R;
	cleanup = saved ? &snapshot : &R;
	destroystack(cleanup);
	require(cleanup->EX == nil && cleanup->FP == nil && cleanup->M == H && module->ref == 1);
	unload(module);
	require(live == 0);
}


/* Call the actual step under kprotect without conflating a zero fallback with an error. */
static int
resultguardstep(void *argument)
{
	*(int*)argument = native64lea8step();
	return 1;
}


/* Shape/descriptor failures must precede decode publication and leave result bytes untouched. */
static void
resultguards(Module *module, Frame *pending)
{
	REG snapshot;
	Inst instruction;
	uchar actual[8], poison[8];
	int which, before, value, status, size, pointers, allocated;

	snapshot = R;
	instruction = module->prog[1];
	size = module->entryt->size;
	pointers = module->entryt->np;
	memset(poison, 0xa5, 8);
	allocated = allocations;
	for(which=0; which<11; which++){
		R = snapshot;
		module->prog[1] = instruction;
		module->entryt->size = size;
		module->entryt->np = pointers;
		module->cstep = native64lea8step;
		switch(which){
		case 0: module->prog[1].add = SRC(AIMM)|DST(AFP|AIND); break;
		case 1: module->prog[1].d.i.f = 64; break;
		case 2: module->prog[1].d.i.s = 40; break;
		case 3: module->prog[1].s.ind = 96; break;
		case 4: module->entryt->size = 64; break;
		case 5: module->entryt->size = 113; break;
		case 6: module->entryt->np = 1; break;
		case 7: R.FP = nil; break;
		case 8: R.FP = H; break;
		case 9: module->cstep = nil; break;
		case 10: R.PC = module->prog; break;
		}
		before = raised;
		value = -1;
		status = kprotect(resultguardstep, &value);
		resultcheck(which < 9 ? status == 0 && raised == before+1 && strcmp(message, exBounds) == 0 :
			status == 1 && raised == before && value == 0, "native64 LEA8 shape guard missed");
		resultcheck(R.PC == (which == 10 ? module->prog : snapshot.PC), "native64 LEA8 shape guard missed");
		memmove(actual, (uchar*)pending+32, 8);
		resultcheck(memcmp(actual, poison, 8) == 0 && allocations == allocated && worker.nerr == 0,
			"native64 LEA8 shape guard missed");
	}
	module->prog[1] = instruction;
	module->entryt->size = size;
	module->entryt->np = pointers;
	module->cstep = native64lea8step;
	R = snapshot;
}


/* Invoke only published entries; arbitrary jumps into private machine code are outside this ABI. */
static int
resultdirect(void *argument)
{
	int (*entry)(REG*);

	memmove(&entry, argument, sizeof(entry));
	return entry == nil ? cjitexec(&R, ILEA, nil) : entry(&R);
}


/* A foreign translation callback must never be invoked by decoded LEA entries. */
static int
resultotherstep(void)
{
	resultcheck(0, "direct LEA invoked foreign translation");
	return 0;
}


/* Require decoded direct entries to reject translated modes before any write. */
static void
resultdirects(Module *module, Frame *pending, int jit, int mode)
{
	REG saved;
	VmFetch fetched;
	Modlink *owner;
	int (*entry)(REG*);
	int (*step)(void);
	uchar before[8], after[8], wanted[8];
	WORD address;
	int route, status, which, refused, compiled;

	saved = R;
	owner = R.M;
	step = module->cstep;
	compiled = owner->compiled;
	memmove(before, (uchar*)pending+32, 8);
	for(route=0; route<(jit ? 3 : 1); route++)
	for(which=0; which<8; which++){
		R = saved;
		require(vmfetch(&fetched));
		vmdecode(&fetched);
		address = (WORD)(uintptr)R.s;
		memmove(wanted, before, 8);
		refused = mode || which != 0;
		if(!refused)
			memmove(wanted, &address, sizeof(address));
		entry = route == 0 ? nil : route == 1 ? module->jit->entry[ILEA] : module->jit->dispatch[ILEA];
		switch(which){
		case 1: R.M = nil; break;
		case 2: R.M = H; break;
		case 3: owner->m = nil; break;
		case 4: owner->m = H; break;
		case 5: owner->compiled = 1; break;
		case 6: module->cstep = (int(*)(void))H; break;
		case 7: module->cstep = resultotherstep; break;
		}
		status = kprotect(resultdirect, &entry);
		owner->m = module;
		owner->compiled = compiled;
		module->cstep = step;
		resultcheck(refused ? status == 0 && strcmp(message, exCompile) == 0 : status == 1,
			"direct LEA mode guard missed");
		memmove(after, (uchar*)pending+32, 8);
		resultcheck(memcmp(after, wanted, 8) == 0 && R.PC == saved.PC+1 && worker.nerr == 0,
			"direct LEA mode guard missed");
		memmove((uchar*)pending+32, before, 8);
		resultdirectcount++;
	}
	R = saved;
}


/* Probe ordinary WORD LEA and trusted LEA8 against the SAME cached module between modes. */
static int
resultwidth(int jit, int grow, int pointer, int onlymode)
{
	Module *module;
	Prog process;
	Frame *pending;
	uchar *root, *mapping, *tail, actual[8], wanted[8];
	void *address;
	WORD word;
	int phase, mode, count;

	module = mallocz(sizeof(*module), 1);
	require(module != nil);
	module->origmp = H;
	module->ref = 1;
	module->rt = DONTCOMPILE;
	module->ss = grow ? 0 : 4096;
	module->ntype = 2;
	module->type = mallocz(2*sizeof(Type*), 1);
	require(module->type != nil);
	module->type[0] = dtype(freeheap, 112, nil, 0);
	module->type[1] = dtype(freeheap, 88, nil, 0);
	require(module->type[0] != nil && module->type[1] != nil);
	module->entryt = module->type[0];
	module->nprog = 3;
	module->prog = mallocz(3*sizeof(Inst), 1);
	require(module->prog != nil);
	module->prog[0].op = IFRAME;
	module->prog[0].add = SRC(AIMM)|DST(AFP);
	module->prog[0].s.imm = 1;
	module->prog[0].d.ind = pointer;
	module->prog[1].op = ILEA;
	module->prog[1].add = SRC(AFP)|DST(AFP|AIND);
	module->prog[1].s.ind = 104;
	module->prog[1].d.i.f = pointer;
	module->prog[1].d.i.s = 32;
	module->prog[2].op = IEXIT;
	module->prog[2].add = SRC(AXXX)|DST(AXXX);
	module->entry = module->prog;
	module->name = strdup("result-width-isolation");
	module->path = strdup("result-width-isolation");
	require(module->name != nil && module->path != nil);
	mapping = nil;
	if(jit){
		require(compile(module, 3, nil));
		require(module->jit->entry[ILEA] != nil);
		mapping = module->jit->code;
	}
	count = 0;
	for(phase=0; phase<4; phase++){
		mode = phase&1;
		if(onlymode >= 0 && mode != onlymode)
			continue;
		module->cstep = mode ? native64lea8step : nil;
		if(jit && !resultentrycontrol){
			int (*published)(REG*);
			int before;

			require(compile(module, 3, nil));
			require(module->jit->code == mapping);
			published = module->jit->entry[ILEA];
			before = allocations;
			module->jit->entry[ILEA] = (int(*)(REG*))mapping;
			resultcheck(!compile(module, 3, nil), "LEA cache alias guard missed");
			require(allocations == before && module->jit->code == mapping);
			module->jit->entry[ILEA] = published;
			require(compile(module, 3, nil));
			resultcachecount++;
		}
		resultprocess(module, &process);
		process.quanta = 1;
		root = process.R.FP;
		tail = root+104;
		require(kprotect(execute, &process) == 1);
		pending = resultprepared(root, pointer);
		memset((uchar*)pending+32, 0xa5, 8);
		if(onlymode < 0 || resultdirectcontrol)
			resultdirects(module, pending, jit, mode);
		if(mode)
			resultguards(module, pending);
		resultcheck(kprotect(execute, &process) == 1, "native64 trusted predecode dispatch missed");
		address = tail;
		memset(wanted, 0xa5, 8);
		if(mode)
			memmove(wanted, &address, 8);
		else{
			word = (WORD)(uintptr)address;
			memmove(wanted, &word, sizeof(word));
		}
		memmove(actual, (uchar*)pending+32, 8);
		resultcheck(memcmp(actual, wanted, 8) == 0, mode ? "native64 result address mismatch" : "ordinary LEA changed width");
		destroystack(&R);
		require(module->ref == 1);
		count++;
	}
	unload(module);
	require(live == 0);
	return count;
}


/* Keep protected runtime failures and refused raw inputs distinct from trusted width probes. */
int
main(int argc, char **argv)
{
	static uchar data[4][Dis64LocalBytes+1];
	Dis64FunctionLocals locals[Dis64LocalFunctions];
	Dis64LocalSlot slots[Dis64LocalFunctions][Dis64LocalSlots];
	unsigned int sizes[4], objectlimit, localcount, workcap, limit, records;
	unsigned int vectors, c, i, j, valid, kind, failure, high, low, chain;
	uvlong expected, second;
	FILE *file;
	int jit, grow, q, saved, pointer, widths, runs, refused, before, onlyjit, onlymode;
	int quanta[] = {1, 2, 3, 7, 16, 64, 1000};

	require(argc == 2 || argc == 4);
	optab[IFRAME] = frame;
	optab[ICALL] = call;
	optab[IRET] = ret;
	optab[ILEA] = lea;
	optab[ICVTWF] = cvtwf;
	optab[IMOVW] = movw;
	optab[IMOVL] = movl;
	optab[IMOVF] = movf;
	optab[IDIVW] = divw;
	optab[IEXIT] = iexit;
	onlyjit = argc == 4 ? atoi(argv[2]) : -1;
	resultdirectcontrol = argc == 4 && (strcmp(argv[3], "direct") == 0 || strcmp(argv[3], "entry") == 0);
	resultentrycontrol = argc == 4 && strcmp(argv[3], "entry") == 0;
	onlymode = argc == 4 ? strcmp(argv[3], "ordinary") != 0 : -1;
	widths = runs = refused = 0;
	for(jit=0; jit<2; jit++){
		if(onlyjit >= 0 && jit != onlyjit)
			continue;
		for(grow=0; grow<2; grow++)
			for(pointer=40; pointer<=56; pointer+=8)
				widths += resultwidth(jit, grow, pointer, onlymode);
	}
	if(argc == 4)
		return 0;
	printf("PASS decoded LEA entry guards: %d ordinary/refused C/leaf/complete probes WORD%d\n", resultdirectcount, (int)sizeof(WORD)*8);
	printf("PASS LEA cache alias guard: %d rejected cache mutations WORD%d\n", resultcachecount, (int)sizeof(WORD)*8);
	if(sizeof(WORD) == 8){
		printf("PASS native64 address isolation: %d ordinary/trusted C/RX/cache/extent probes WORD64; 264 shape/fallback probes; no wire execution\n", widths);
		return 0;
	}
	file = fopen(argv[1], "rb");
	require(file != nil);
	vectors = resultnumber(file);
	for(c=0; c<vectors; c++){
		for(i=0; i<4; i++){
			sizes[i] = resultnumber(file);
			require(sizes[i] <= Dis64LocalBytes);
		}
		objectlimit = resultnumber(file);
		resultentry = resultnumber(file);
		resultframe = resultnumber(file);
		localcount = resultnumber(file);
		workcap = resultnumber(file);
		limit = resultnumber(file);
		require(localcount <= Dis64LocalFunctions && workcap <= ResultCode);
		for(i=0; i<4; i++)
			require(fread(data[i], 1, sizes[i], file) == sizes[i]);
		for(i=0; i<localcount; i++){
			locals[i].symbol = resultnumber(file);
			locals[i].entry = resultnumber(file);
			locals[i].frame = resultnumber(file);
			locals[i].count = resultnumber(file);
			require(locals[i].count <= Dis64LocalSlots);
			for(j=0; j<locals[i].count; j++){
				slots[i][j].offset = resultnumber(file);
				slots[i][j].kind = resultnumber(file);
			}
			locals[i].slots = slots[i];
		}
		valid = resultnumber(file);
		kind = resultnumber(file);
		high = resultnumber(file);
		low = resultnumber(file);
		expected = ((uvlong)high<<32)|low;
		failure = resultnumber(file);
		chain = resultnumber(file);
		high = resultnumber(file);
		low = resultnumber(file);
		second = ((uvlong)high<<32)|low;
		before = allocations;
		i = dis64boundcallcheck(data[0], sizes[0], data[1], sizes[1], data[2], sizes[2], data[3], sizes[3],
			objectlimit, resultentry, resultframe, locals, localcount, resultdecoded, resultwork, resultqueue,
			workcap, limit, resultproof, Dis64LocalFunctions, &records);
		require(i == valid && allocations == before && live == 0);
		if(!valid){
			refused++;
			continue;
		}
		require(dis64typescheck(data[3], sizes[3], 65536, objectlimit, &resulttypes));
		resultcount = dis64u32(data[0]);
		for(jit=0; jit<2; jit++)
			for(grow=0; grow<2; grow++)
				for(q=0; q<(int)nelem(quanta); q++)
					for(saved=0; saved<2; saved++){
						resultrun(jit, grow, failure, quanta[q], saved, kind, expected, 0, chain, second);
						runs++;
						if(grow && failure == 0){
							resultrun(jit, grow, failure, quanta[q], saved, kind, expected, 1, chain, second);
							runs++;
							if(chain){
								resultrun(jit, grow, failure, quanta[q], saved, kind, expected, 2, chain, second);
								runs++;
							}
						}
					}
	}
	require(fgetc(file) == EOF && fclose(file) == 0);
	printf("PASS verified raw result calls: %d C/RX/quanta/extent/saved/error runs; %d refused before allocation; WORD32\n", runs, refused);
	printf("PASS native64 address isolation: %d ordinary/trusted C/RX/cache/extent probes WORD32; 264 shape/fallback probes\n", widths);
	return 0;
}
