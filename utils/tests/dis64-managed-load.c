/* Real ILOAD/readmod/parser/linker with controlled namespace I/O and production GC. */
static const ManagedImageCase *managednamespace, *managedreplacement;

static int cacheopens, cachecloses, cachereads, cacheparses, cachechecks;
static int cachereleases, cacheacquires, cachefd, cacheoffset, cachekind;
static int cacheloads, cachefast, cachecases;

static const ManagedImageCase*
managedreadcase(void)
{
	return cacheopens >= 2 && managedreplacement != nil ? managedreplacement : managednamespace;
}


void release(void) { require(cachereleases == cacheacquires); cachereleases++; }
void acquire(void) { require(cachereleases == cacheacquires+1); cacheacquires++; }
void newdyndata(Modlink *link) { USED(link); require(0); }
void newmp(void *target, void *source, Type *type) { USED(target); USED(source); USED(type); require(0); }

char*
string2c(String *text)
{
	require(text != nil && text != H && text->len >= 0);
	return (char*)text->Sascii;
}

int
kopen(char *path, int mode)
{
	require(cachefd == 0 && mode == 0 && strcmp(path, dis64bootprovider) == 0);
	cacheopens++;
	if(cachekind >= 7 && cacheopens == 2) expectedlinkerrors = 1;
	cacheoffset = 0;
	if(cachekind == 5 && cacheopens == 2)
		return -1;
	cachefd = 7;
	return cachefd;
}

Dir*
kdirfstat(int fd)
{
	Dir *directory;

	require(fd == 7 && cachefd == fd);
	directory = mallocz(sizeof(*directory), 1);
	require(directory != nil);
	directory->length = managedreadcase()->providerbytes;
	directory->dev = 3;
	directory->type = 4;
	directory->mtime = 5;
	directory->qid.path = 6;
	directory->qid.vers = cachekind == 6 ? cacheopens : cachekind != 0 && cacheopens >= 2;
	return directory;
}

long
kread(int fd, void *buffer, long bytes)
{
	long count;

	require(fd == 7 && cachefd == fd && bytes > 0);
	cachereads++;
	if(cachekind == 3 && cacheopens == 2)
		return 0;
	count = managedreadcase()->providerbytes-cacheoffset;
	if(count > bytes) count = bytes;
	if(count > 7) count = 7; // Exercise the actual complete-file read loop.
	memmove(buffer, managedreadcase()->provider+cacheoffset, count);
	if(cachekind == 2 && cacheopens == 2 && cacheoffset == 0)
		((uchar*)buffer)[0] ^= 1; // Changed identity must not hide malformed replacement bytes.
	cacheoffset += count;
	return count;
}

int
kclose(int fd)
{
	require(fd == 7 && cachefd == fd);
	cachefd = 0;
	cachecloses++;
	return 0;
}

int
mustbesigned(char *path, uchar *image, ulong bytes, Dir *directory)
{
	require(strcmp(path, dis64bootprovider) == 0 && image != nil && bytes != 0 && directory != nil);
	cachechecks++;
	return cachekind == 4 && cacheopens == 2;
}

static int
managedparse(void *argument)
{
	return dis64parse(argument);
}

/* The fixture selects the actual managed Dis64 parser, without a legacy parser fallback. */
Module*
parsemod(char *path, uchar *image, ulong bytes, Dir *directory)
{
	DisLoad load = {0};

	cacheparses++;
	load.path = path;
	load.code = image;
	load.length = bytes;
	load.dir = directory;
	if(kprotect(managedparse, &load))
		return load.m;
	if(load.m != nil)
		unload(load.m);
	return nil;
}

/* Observe distinct instances sharing a cached immutable Module/JIT, then changed-file replacement. */
static void
managedcachecase(int direction, int jit, int grow, int kind, const ManagedImageCase *selected, int group)
{
	Prog process = {0};
	const ManagedImageCase *test;
	Modlink *caller, *loaded, *previous;
	Module *cached;
	Jit *mapping;
	Type *frame;
	Frame *entry;
	String *text;
	void *argument, *result;
	Inst *pc;
	int step, success, completed, loads, before, expectedparses, filemode, stringresult;

	test = selected == nil ? managedcachefixtures[direction] : selected;
	require(test != nil && modules == nil);
	managednamespace = test;
	cacheopens = cachecloses = cachereads = cacheparses = cachechecks = 0;
	cachereleases = cacheacquires = cachefd = cacheoffset = 0;
	cachekind = kind;
	dis64bootmanaged = 1;
	dis64bootprovider = "/managed-provider.dis64";
	cflag = jit;
	caller = prepare64managedtrial("cached-file-caller", test->consumer, test->consumerbytes,
		dis64bootprovider, test->provider, test->providerbytes, jit);
	require(caller != nil);
	filemode = caller->prog[0].op == ILOAD;
	require(caller->m->nprog == (group ? (filemode ? 7 : 6)*Dis64ManagedCalls-1 : 6*Dis64ManagedCalls));
	if(!filemode){
		memmove(&loaded, caller->MP, sizeof(loaded));
		require(loaded != nil && loaded != H && loaded->nlinks == 2);
		if(jit){
			require(loaded->m->jit != nil);
			rxbegin = (uintptr)loaded->m->jit->code;
			rxend = rxbegin+loaded->m->jit->size;
		}
	}
	frame = caller->m->entryt;
	caller->m->ss = grow ? 0 : 4096;
	process.exval = H;
	process.state = Pready;
	process.quanta = 1;
	process.R.M = caller;
	process.R.PC = caller->prog;
	entry = mallocz(frame->size, 1);
	require(entry != nil);
	entry->t = frame;
	R.s = &entry;
	newstack(&process);
	free(entry);
	initmem(frame, process.R.FP);
	text = managedstring();
	argument = direction ? (void*)managedlist(text) : text;
	managedput(process.R.FP+64, argument);
	ptrdel(D2H(caller));
	managedprocess = &process;
	previous = nil;
	cached = nil;
	mapping = nil;
	loads = completed = 0;
	before = rxcompleted;
	for(step=0; step<512; step++){
		pc = process.R.PC;
		if(process.R.M == caller && pc == caller->prog+caller->m->nprog-1)
			break;
		success = kprotect(execute, &process);
		if(!success)
			break;
		if(pc->op == ILOAD){
			loads++;
			memmove(&loaded, caller->MP, sizeof(loaded));
			if(kind >= 2 && kind != 6 && loads == 2)
				require(loaded == H);
			else{
				require(loaded != nil && loaded != H && loaded != previous);
				require(loaded->m == modules && (loaded->m->rt & Dis64ManagedFile));
				if((kind == 1 && loads == 2) || (kind == 6 && loads > 1)){
					require(loaded->m != cached && (!jit || loaded->m->jit != mapping));
					cached = nil;
				}
				if(cached != nil)
					require(loaded->m == cached && loaded->m->jit == mapping);
				cached = loaded->m;
				mapping = cached->jit;
				previous = loaded;
				if(jit){
					require(mapping != nil && mapping->code != nil);
					rxbegin = (uintptr)mapping->code;
					rxend = rxbegin+mapping->size;
				}
			}
		}
		if(pc->op == IRET)
			completed++;
		numericcollect(&process);
	}
	require(step < 512 && cachefd == 0 && cachereleases == cacheacquires && expectedlinkerrors == 0);
	if(kind < 2 || kind == 6){
		if(completed != Dis64ManagedCalls || loads != (filemode ? Dis64ManagedCalls : 0))
			fprintf(stderr, "managed cache: direction=%d jit=%d grow=%d kind=%d step=%d calls=%d loads=%d success=%d error=%s\n", direction, jit, grow, kind, step, completed, loads, success, message);
		require(completed == Dis64ManagedCalls && loads == (filemode ? Dis64ManagedCalls : 0));
		memmove(&result, process.R.FP+72, sizeof(result));
		stringresult = group ? !direction : direction;
		require(result != H && (stringresult ? (String*)result : managedhead(result))->Sascii[0] == 'x');
		if(group) require((stringresult ? (String*)result : managedhead(result)) == text);
		require(!kprotect(execute, &process) && message[0] == 0);
	}else
		require(!success && completed == 1 && loads == 2);
	expectedparses = !filemode ? 0 : kind == 6 ? Dis64ManagedCalls : kind == 0 || kind == 3 || kind == 5 ? 1 : 2;
	require(cacheparses == expectedparses && cachechecks == expectedparses);
	require(cacheopens == loads && cachecloses == cacheopens-(kind == 5));
	cacheloads += loads;
	cachefast += rxcompleted-before;
	cachecases++;
	destroystack(&R);
	numericdrain();
	require(modules == nil && Tstring.ref == 1 && Tlist.ref == 1 && Tptr.ref == 1 && Tmodlink.ref == 1);
	dis64bootmanaged = 0;
	dis64bootprovider = nil;
	cflag = 0;
	managedreplacement = nil;
}

static void
managedcachecontrol(void)
{
	int direction, jit, grow, kind;

	for(direction=0; direction<2; direction++)
		for(jit=0; jit<2; jit++)
			for(grow=0; grow<2; grow++)
				for(kind=0; kind<7; kind++)
					managedcachecase(direction, jit, grow, kind, nil, 0);
#if defined(__x86_64__) || defined(__aarch64__) || (defined(__riscv) && __riscv_xlen == 64)
	require(cachefast == 400);
#endif
	printf("PASS real managed ILOAD/readmod/linkmod: %d cases, %d LOADs, %d native completions; cache identity/JIT reuse, changed files and I/O/signing refusal\n", cachecases, cacheloads, cachefast);
}

/* Check every interface record/body, then execute compiled two-import chains with real GC. */
static void
managedgroupcontrol(void)
{
	static const Dis64ManagedSchema schema = {Dis64TrialVersion, Dis64TrialString, Dis64TrialList};
	static const LimboNative64Managed shapes[] = {
		{&limbo_native64_String_unquoted_entry, LimboNative64String, LimboNative64ListString},
		{&limbo_native64_String_quoted_entry, LimboNative64ListString, LimboNative64String},
	};
	const ManagedImageCase *chosen[2][2] = {{nil}}, *thin[2] = {nil}, *test;
	Dis64ManagedPlan plan;
	Module *module;
	int i, direction, jit, grow, kind, filemode, before, casesbefore, loadsbefore, fastbefore, refusals = 0, first, attempts, failure, mode;

	before = allocations;
	for(i=0; i<nelem(managedgroups); i++){
		test = &managedgroups[i];
		memset(&plan, 0, sizeof(plan));
		plan.selected.entry = 99;
		require(dis64managedimagegroup(test->consumer, test->consumerbytes, test->provider, test->providerbytes,
			&dis64numericlimits, &schema, shapes, nelem(shapes), &plan) == test->accepted);
		if(test->accepted){
			require(plan.initial == test->direction+1 && plan.count >= 1 && plan.count <= 2);
			if(plan.count == 2) require(plan.matches[0].entry != plan.matches[1].entry);
			if(plan.provider.exports.count == 1) thin[test->direction] = test;
			filemode = dis64managedfilecode(&plan.consumer.code);
			if(plan.count == 2 && plan.consumer.code.count == (filemode ? 7 : 6)*Dis64ManagedCalls-1)
				chosen[test->direction][filemode] = test;
		}else
			require(plan.selected.entry == 99 && plan.consumer.code.data == nil);
	}
	require(allocations == before);
	dis64bootmanaged = 1;
	dis64bootprovider = "/managed-provider.dis64";
	for(i=0; i<nelem(managedgroups); i++){
		test = &managedgroups[i];
		numericdrain();
		before = live;
		module = prepare64managedfile(dis64bootprovider, (uchar*)test->provider, test->providerbytes, 0);
		require((module != nil) == test->provideraccepted);
		if(module != nil) unload(module);
		numericdrain();
		require(live == before);
	}
	for(direction=0; direction<2; direction++){
		test = chosen[direction][1];
		require(test != nil);
		for(mode=0; mode<2; mode++){
			Modlink *caller;
			numericdrain(); before = live; first = allocations;
			if(mode == 0){
				module = prepare64managedfile(dis64bootprovider, (uchar*)test->provider, test->providerbytes, 0);
				require(module != nil && module->ext[1].name != nil && module->ext[2].name == nil);
				attempts = allocations-first; unload(module);
			}else{
				caller = prepare64managedtrial("group-caller", test->consumer, test->consumerbytes,
					dis64bootprovider, test->provider, test->providerbytes, 0);
				require(caller != nil && caller->m->ldt[0][1].name != nil && caller->m->ldt[0][2].name == nil);
				attempts = allocations-first; ptrdel(D2H(caller)); destroy(caller);
			}
			numericdrain(); require(live == before);
			for(failure=1; failure<=attempts; failure++){
				failat = allocations+failure;
				if(mode == 0)
					require(prepare64managedfile(dis64bootprovider, (uchar*)test->provider, test->providerbytes, 0) == nil);
				else
					require(prepare64managedtrial("group-caller", test->consumer, test->consumerbytes,
						dis64bootprovider, test->provider, test->providerbytes, 0) == nil);
				failat = 0; numericdrain(); require(live == before && Tmodlink.ref == 1 && worker.nerr == 0);
			}
			refusals += attempts;
		}
	}
	printf("PASS dual managed owners: %d allocation refusals and partial export/LDT cleanup\n", refusals);
	dis64bootmanaged = 0;
	casesbefore = cachecases; loadsbefore = cacheloads; fastbefore = cachefast;
	for(direction=0; direction<2; direction++)
		for(filemode=0; filemode<2; filemode++){
			require(chosen[direction][filemode] != nil);
			for(jit=0; jit<2; jit++)
				for(grow=0; grow<2; grow++)
					for(kind=0; kind<(filemode ? 9 : 1); kind++){
						managedreplacement = kind >= 7 ? thin[kind-7] : nil;
						if(kind >= 7) require(managedreplacement != nil);
						managedcachecase(direction, jit, grow, kind, chosen[direction][filemode], 1);
					}
		}
	require(cachecases-casesbefore == 80 && cacheloads-loadsbefore == 864);
#if defined(__x86_64__) || defined(__aarch64__) || (defined(__riscv) && __riscv_xlen == 64)
	require(cachefast-fastbefore == 536);
#endif
	printf("PASS dual managed imports/exports: %d wire pairs, %d C/JIT/GC cases, %d LOADs, %d native completions; typed result transfer and pointer identity\n",
		(int)nelem(managedgroups), cachecases-casesbefore, cacheloads-loadsbefore, cachefast-fastbefore);
}
