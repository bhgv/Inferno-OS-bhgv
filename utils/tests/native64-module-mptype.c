static LinkModule *modulecontext; // Stable production constructor context only while its protected body runs.
static Module testmodule; // Native module metadata, never a fabricated managed Heap.
static Modlink *testinstance; // Actual mklinkmod result, protected before caller publication.
static int invalidmodule, modulecases, modulefaults, moduleunloads; // Independent preflight/failure observations.


/* Refuse a missing preflight guard before native allocation or a potentially unsafe collector poll. */
static void
moduleeffects(int managed)
{
	if(modulecontext == nil)
		return;
	storagecheck(!invalidmodule,"module descriptor preflight");
	if(managed){
		storagecheck(pins(modulecontext->m->origmp) == 1,"module source pin");
		if(modulecontext->result != H)
			storagecheck(pins(modulecontext->result) == 1,"module instance pin");
	}
}


/* Module cache ownership is a native service boundary; real freemodlink consumes one module reference. */
void
unload(Module *module)
{
	storagecheck(module == &testmodule && module->ref == 2,"module reference retirement");
	module->ref--;
	moduleunloads++;
}


/* Dynamic module callbacks are outside this ordinary typed-data acceptance. */
void
newdyndata(Modlink *module)
{
	USED(module);
	storagecheck(0,"unexpected dynamic data");
}


/* No dynamic storage owner is supplied to the actual instance destructor. */
void
freedyndata(Modlink *module)
{
	USED(module);
	storagecheck(0,"unexpected dynamic retirement");
}


/* Empty imports need no export diagnostic; reaching it is an independent fixture error. */
int
snprint(char *output, int bytes, char *format, ...)
{
	USED(output); USED(bytes); USED(format);
	storagecheck(0,"unexpected export diagnostic");
	return 0;
}


/* Enter the real linkmod error/ownership boundary through its caller's real kprotect. */
static int
modulecall(void *argument)
{
	Import imports[1] = {{0}};

	USED(argument);
	testinstance = linkmod(&testmodule,imports,1);
	modulecontext = nil;
	return testinstance != H;
}


/* Ordinary legacy index0 and explicit indices1/64 select exact mapped MP and preserve child ownership. */
static void
modulecase(int ordinal, int invalid, int fault)
{
	Type *types[65], *mp;
	void *original, *child, *copied;
	Modlink *instance;
	uchar map[] = {0x80};
	unsigned int value, result;
	int i, ok, steps;

	reset(); clonepolls = 0; modulecontext = nil; invalidmodule = 0;
	memset(&testmodule,0,sizeof(testmodule)); memset(types,0,sizeof(types));
	for(i=0; i<=ordinal; i++){
		types[i] = dtype(freeheap,i == ordinal ? 16 : 80,i == ordinal ? map : nil,i == ordinal ? 1 : 0);
		storagecheck(types[i] != nil,"module fixture descriptor");
	}
	mp = types[ordinal];
	original = H2D(void*,heapz(mp)); ptradd(D2H(original));
	child = H2D(void*,heapz(mp));
	memmove(original,&child,sizeof(child)); Setmark(D2H(original));
	value = 0x89abcdefU; memmove((uchar*)original+8,&value,sizeof(value));
	testmodule.ref = 2; testmodule.rt = DONTCOMPILE;
	testmodule.ntype = ordinal+1; testmodule.type = types; testmodule.mptype = ordinal;
	testmodule.origmp = original;
	if(invalid == 1) testmodule.mptype = -1;
	if(invalid == 2) testmodule.mptype = testmodule.ntype;
	if(invalid == 3) testmodule.mptype = 0;
	if(invalid == 4) testmodule.ntype = 0;
	if(invalid == 5) D2H(original)->ref = 0;
	ptrdel(D2H(original)); clonecache();
	invalidmodule = invalid != 0;
	operation = 0; failoperation = fault; testinstance = H; clonepolls = 1;
	ok = kprotect(modulecall,nil);
	modulecontext = nil; clonepolls = 0; invalidmodule = 0;
	steps = operation; failoperation = 0;
	if(invalid == 5) D2H(original)->ref = 1;
	ptradd(D2H(original));
	modulecases++;
	if(invalid || fault){
		storagecheck(!ok && testinstance == H && testmodule.ref == 1 && errdepth == 0,"module refusal ownership");
		if(invalid) storagecheck(steps == 0,"module preflight effects");
		modulefaults++;
	}else{
		storagecheck(ok && testinstance != H && testmodule.ref == 2,"module ordinal success");
		instance = testinstance; ptradd(D2H(instance));
		storagecheck(instance->MP != original && D2H(instance->MP)->t == mp,"selected MP descriptor");
		storagecheck(instance->type == types && instance->m->mptype == ordinal,"original type ordinals");
		memmove(&copied,instance->MP,sizeof(copied));
		storagecheck(copied == child && D2H(child)->ref == 2,"instance child ownership");
		memmove(&result,instance->MP+8,sizeof(result)); storagecheck(result == value,"instance scalar bits");
		epochs(6);
		storagecheck(D2H(instance->MP)->t == mp && D2H(child)->ref == 2,"instance collector lifetime");
		ptrdel(D2H(instance)); destroy(instance); testinstance = H;
		storagecheck(testmodule.ref == 1,"instance module lifetime");
	}
	storagecheck(D2H(child)->ref == 1,"source child conservation");
	memmove(&copied,original,sizeof(copied)); storagecheck(copied == child,"source byte conservation");
	ptrdel(D2H(original));
	destroy(original);
	for(i=0; i<=ordinal; i++) freetype(types[i]);
	drainroots();
	storagecheck(nativecount == 0 && poolowners() == 0 && Tmodlink.ref == 1,"module complete baseline");
	if(!invalid && !fault)
		for(i=1; i<=steps; i++) modulecase(ordinal,0,i);
}


/* Exercise the same native construction/GC contract under both scalar models. */
int
main(void)
{
	int invalid;

	modulecase(0,0,0); modulecase(1,0,0); modulecase(64,0,0);
	for(invalid=1; invalid<=5; invalid++) modulecase(1,invalid,0);
	printf("PASS native Module MP ordinals: %d cases, %d refusals, %d unloads, %d GC epochs, %d checks\n",modulecases,modulefaults,moduleunloads,collections,checks);
	return 0;
}
