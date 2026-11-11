/* Real ILOAD/readmod/parser/linker with controlled namespace I/O and production GC. */
static const ManagedImageCase *managednamespace;
static int cacheopens, cachecloses, cachereads, cacheparses, cachechecks;
static int cachereleases, cacheacquires, cachefd, cacheoffset, cachekind;
static int cacheloads, cachefast, cachecases;

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
	directory->length = managednamespace->providerbytes;
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
	count = managednamespace->providerbytes-cacheoffset;
	if(count > bytes) count = bytes;
	if(count > 7) count = 7; // Exercise the actual complete-file read loop.
	memmove(buffer, managednamespace->provider+cacheoffset, count);
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
managedcachecase(int direction, int jit, int grow, int kind)
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
	int step, success, completed, loads, before, expectedparses;

	test = managedcachefixtures[direction];
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
	require(caller != nil && caller->m->nprog == 6*Dis64ManagedCalls);
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
			if(kind >= 2 && kind <= 5 && loads == 2)
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
	require(step < 512 && cachefd == 0 && cachereleases == cacheacquires);
	if(kind < 2 || kind == 6){
		if(completed != Dis64ManagedCalls || loads != Dis64ManagedCalls)
			fprintf(stderr, "managed cache: direction=%d jit=%d grow=%d kind=%d step=%d calls=%d loads=%d success=%d error=%s\n", direction, jit, grow, kind, step, completed, loads, success, message);
		require(completed == Dis64ManagedCalls && loads == Dis64ManagedCalls);
		memmove(&result, process.R.FP+72, sizeof(result));
		require(result != H && (direction ? (String*)result : managedhead(result))->Sascii[0] == 'x');
		require(!kprotect(execute, &process) && message[0] == 0);
	}else
		require(!success && completed == 1 && loads == 2);
	expectedparses = kind == 6 ? Dis64ManagedCalls : kind == 0 || kind == 3 || kind == 5 ? 1 : 2;
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
}

static void
managedcachecontrol(void)
{
	int direction, jit, grow, kind;

	for(direction=0; direction<2; direction++)
		for(jit=0; jit<2; jit++)
			for(grow=0; grow<2; grow++)
				for(kind=0; kind<7; kind++)
					managedcachecase(direction, jit, grow, kind);
#if defined(__x86_64__) || defined(__aarch64__) || (defined(__riscv) && __riscv_xlen == 64)
	require(cachefast == 400);
#endif
	printf("PASS real managed ILOAD/readmod/linkmod: %d cases, %d LOADs, %d native completions; cache identity/JIT reuse, changed files and I/O/signing refusal\n", cachecases, cacheloads, cachefast);
}
