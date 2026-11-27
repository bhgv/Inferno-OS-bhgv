
/* Read immutable fixture bytes without giving the VM trusted local-kind declarations. */
static unsigned int
launchimage(char *path, uchar *image, unsigned int capacity)
{
	FILE *file;
	unsigned int bytes;

	file = fopen(path, "rb");
	require(file != nil);
	bytes = fread(image, 1, capacity, file);
	require(!ferror(file) && feof(file) && fclose(file) == 0);
	return bytes;
}


/* Run an admitted file through real root publication, calls, result write and retirement. */
static void
launchrun(Module *module, unsigned int kind, uvlong expected)
{
	Prog *process;
	uchar *root;
	uvlong value;
	unsigned int width;
	int done, steps;

	starting = module;
	started = nil;
	require(kprotect(start, nil));
	process = started;
	rootenvironment(process);
	root = process->R.FP;
	width = kind == Dis64Int ? 4 : 8;
	process->quanta = 1;
	for(steps=0; steps<128; steps++){
		done = kprotect(executeprocess, process);
		if(!done)
			break;
	}
	require(steps < 128);
	if(expecterror)
		require(strcmp(worker.env->errstr, exZdiv) == 0);
	else{
		require(worker.env->errstr[0] == 0 && process->R.FP == root);
		value = 0;
		memmove(&value, root+module->entryt->size-8, width);
		require(value == expected);
	}
	observed = process;
	progexit();
	emptyqueues();
	require(live == 0 && modules == nil && nlinkpins == 0 && Tmodlink.ref == 1);
}


/* Check the real file/cache boundary before executing a subset with independent expected results. */
int
main(int argc, char **argv)
{
	static uchar image[Dis64CallImageBytes], saved[Dis64CallImageBytes]; // Bounded inputs stay outside the native host stack.
	uchar pilot[4096];
	unsigned int bytes, pilotbytes, kind;
	uvlong expected;
	Module *module, *alias, *opposite;
	int accepted, jit, total, fault, before;

	require(argc == 7 || argc == 8);
	expecterror = atoi(argv[6]);
	accepted = atoi(argv[2]);
	kind = atoi(argv[3]);
	expected = strtoull(argv[4], nil, 10);
	bytes = launchimage(argv[1], image, sizeof(image));
	pilotbytes = launchimage(argv[5], pilot, sizeof(pilot));
	memmove(saved, image, bytes);
	rootpgrp.r.ref = rootfgrp.r.ref = rootegrp.r.ref = 1;
	worker.defenv.pgrp = &rootpgrp;
	worker.defenv.fgrp = &rootfgrp;
	worker.defenv.egrp = &rootegrp;
	worker.defenv.errstr = worker.defenv.errbuf0;
	worker.defenv.syserrstr = worker.defenv.errbuf1;
	worker.env = &worker.defenv;
	worker.type = Interp;
	optab[IMOVW] = countmovw;
	optab[IADDW] = countaddw;
	optab[IDIVW] = countdivw;
	optab[ISHLW] = countshlw;
	optab[IEXIT] = countexit;
	optab[IFRAME] = actual_frame;
	optab[ICALL] = actual_call;
	optab[IRET] = actual_ret;
	optab[ILEA] = actual_lea;
	optab[ICVTWF] = actual_cvtwf;
	optab[IMOVL] = actual_movl;
	optab[IMOVF] = actual_movf;
	if(argc == 8){
		require(dis64launchmode() == 0 && !dis64selectlaunch("dis64-calls"));
		require(fileprepare("calls", image, bytes, 0) == nil && live == 0);
		module = fileprepare("scalar", pilot, pilotbytes, 0);
		require(module != nil && module->cstep == nil);
		unload(module);
		opposite = prepare64calls("same", image, bytes, 0);
		require(opposite != nil);
		/* Mirror the fixture's complete identity: a Qid hit must not bypass profile admission. */
		opposite->dev = 37;
		opposite->dtype = 19;
		opposite->mtime = filemtime;
		opposite->qid.path = 0x100000123ULL;
		opposite->qid.vers = 0x23456789;
		opposite->qid.type = 0x80;
		modules = opposite;
		opposite->ref++;
		before = fileopens;
		require(readmod("same", opposite, 0) == nil && opposite->ref == 1 && fileopens == before);
		unload(opposite);
		require(live == 0 && modules == nil);
		return 0;
	}
	require(!dis64selectlaunch(nil) && !dis64selectlaunch("unknown"));
	require(dis64selectlaunch("dis64-calls") && dis64launchmode() == 1);
	require(!dis64selectlaunch("dis64-calls"));
	require(fileprepare("scalar", pilot, pilotbytes, 0) == nil && live == 0 && modules == nil);
	require(readmod("$Sys", nil, 0) == nil && live == 0);
	/* Publish an owned opposite-profile cache entry solely to probe cross-profile refusal. */
	opposite = actualprepare64pilot("same", pilot, pilotbytes, 0);
	require(opposite != nil);
	/* Mirror the fixture's complete identity: a Qid hit must not bypass profile admission. */
	opposite->dev = 37;
	opposite->dtype = 19;
	opposite->mtime = filemtime;
	opposite->qid.path = 0x100000123ULL;
	opposite->qid.vers = 0x23456789;
	opposite->qid.type = 0x80;
	modules = opposite;
	opposite->ref++;
	before = fileopens;
	require(readmod("same", opposite, 0) == nil && opposite->ref == 1 && fileopens == before);
	unload(opposite);
	require(modules == nil && live == 0);
	for(jit=0; jit<2; jit++){
		module = fileprepare("calls", image, bytes, jit);
		require((module != nil) == accepted);
		if(!accepted){
			require(live == 0 && modules == nil && worker.nerr == 0);
			continue;
		}
		require(module->cstep == native64lea8step && constructorcalls == 0);
		alias = lookmod("calls");
		require(alias == module && module->ref == 2);
		alias = readmod("calls", alias, 0);
		require(alias == module && fileposition == 0 && module->ref == 2);
		unload(alias);
		unload(module);
		require(live == 0 && modules == nil);
		/* Every actual read/parse/construct allocation must roll back its unpublished owner. */
		allocations = 0;
		module = fileprepare("calls", image, bytes, jit);
		require(module != nil);
		total = allocations;
		unload(module);
		for(fault=1; fault<=total; fault++){
			allocations = 0;
			failat = fault;
			module = fileprepare("calls", image, bytes, jit);
			failat = 0;
			require(module == nil && live == 0 && modules == nil && worker.nerr == 0);
		}
		admissionrequired = 1;
		require(fileprepare("calls", image, bytes, jit) == nil && live == 0 && modules == nil);
		admissionrequired = 0;
		if(kind != 0){
			module = fileprepare("calls", image, bytes, jit);
			require(module != nil);
			memset(image, 0xa5, bytes);
			launchrun(module, kind, expected);
			memmove(image, saved, bytes);
		}
	}
	require(live == 0 && worker.nerr == 0 && fileopens == filecloses);
	return 0;
}
