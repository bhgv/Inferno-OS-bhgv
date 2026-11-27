static int noattachalloc, attachedcases, attachrefusals, cachecycles; // Independent preflight/publication observations.


/* Observe refusal before a missing preflight guard can overflow or publish an owner. */
static void
attachmentallocation(void)
{
	storagecheck(!noattachalloc,"attachment preflight allocation");
}


/* Dynamic code destruction cannot be reached from these code-free native Modules. */
void
freedyncode(Module *module)
{
	USED(module);
	storagecheck(0,"unexpected dynamic code");
}


/* Allocate genuine native Module storage through the same guarded allocator as production metadata. */
static Module*
emptytarget(void)
{
	Module *module;

	module = storagemallocz(sizeof(*module),1);
	storagecheck(module != nil,"target allocation");
	module->ref = 1; module->rt = DONTCOMPILE; module->origmp = H;
	return module;
}


/* Preserve all target/source bytes and reference counts on a rejected attachment. */
static void
refusal(Module *target, Dis64StorageOwner *source, int allocation)
{
	Module before;
	Dis64StorageOwner saved;
	int refs[128];
	ulong dataref;
	unsigned int i;
	int result, baseline;

	memmove(&before,target,sizeof(before)); memmove(&saved,source,sizeof(saved));
	for(i=0; i<source->count; i++) refs[i] = source->types[i]->ref;
	dataref = source->data == H ? 0 : D2H(source->data)->ref;
	baseline = nativecount;
	operation = 0; failoperation = allocation ? 1 : 0; noattachalloc = !allocation;
	result = attach64storage(target,source);
	noattachalloc = 0; failoperation = 0;
	storagecheck(!result,"attachment refusal");
	storagecheck(memcmp(&before,target,sizeof(before)) == 0 && memcmp(&saved,source,sizeof(saved)) == 0,"attachment refusal atomic");
	storagecheck(nativecount == baseline && operation == allocation,"attachment refusal baseline");
	for(i=0; i<source->count; i++) storagecheck(source->types[i]->ref == refs[i],"attachment source type conservation");
	if(source->data != H) storagecheck(D2H(source->data)->ref == dataref,"attachment source data conservation");
	attachrefusals++;
}


/* Actual Module unload may precede its independent lease or follow cache/instance ownership. */
static void
attachcase(uchar *image, unsigned int bytes, int publish, int nested)
{
	Dis64StorageOwner *source;
	Module *module;
	Modlink *instance;
	Import imports[1] = {{0}};
	Type *element;
	void *data;
	int refs[128];
	unsigned int i, selected;

	reset(); modulecontext = nil; clonepolls = 0;
	source = prepare64storage(image,bytes); storagecheck(source != nil,"attachment source");
	memset(image,0xa5,bytes);
	element = nested ? clonegraph(source,1) : nil;
	clonecache();
	module = emptytarget();
	for(i=0; i<source->count; i++) refs[i] = source->types[i]->ref;
	selected = 0; data = source->data;
	if(data != H){
		for(i=0; i<source->count; i++) if(source->types[i] == D2H(data)->t) selected = i;
		D2H(data)->color = sweeper;
	}
	storagecheck(attach64storage(module,source),"attachment success");
	storagecheck(module->type != source->types || source->count == 0,"independent module table");
	storagecheck(module->ntype == source->count && module->mptype == selected,"attachment MP ordinal");
	for(i=0; i<source->count; i++){
		storagecheck(module->type[i] == source->types[i] && source->types[i]->ref == refs[i]+1,"attachment type retains");
	}
	if(data != H){
		storagecheck(module->origmp == data && D2H(data)->ref == 2,"attachment data retain");
		storagecheck(pins(data) == 1,"lease remains rooted");
		storagecheck(D2H(data)->color == propagator,"attachment mark barrier");
	}
	storagecheck(module->prog == nil && module->entry == nil && module->entryt == nil && module->nprog == 0 && module->cstep == nil,"no executable publication");
	attachedcases++;
	epochs(6);
	if(!publish){
		unload(module);
		for(i=0; i<source->count; i++) storagecheck(source->types[i]->ref == refs[i],"unpublished type rollback");
		if(data != H) storagecheck(D2H(data)->ref == 1 && pins(data) == 1,"unpublished data rollback");
		free64storage(source);
	}else{
		module->link = modules; modules = module;
		free64storage(source);
		if(data != H) storagecheck(pins(data) == 0 && D2H(data)->ref == 1,"cache root transfer");
		epochs(6); cachecycles += 6;
		module->ref++;
		clonepolls = 1;
		instance = linkmod(module,imports,1);
		modulecontext = nil; clonepolls = 0;
		storagecheck(instance != H && instance->m == module,"attached module instance");
		ptradd(D2H(instance));
		if(data != H) storagecheck(instance->MP != data && D2H(instance->MP)->t == module->type[selected],"attached instance descriptor");
		unload(module);
		storagecheck(modules == module && module->ref == 1,"instance retains cached module");
		epochs(6); cachecycles += 6;
		ptrdel(D2H(instance)); destroy(instance);
		storagecheck(modules == nil,"actual cache unlink");
	}
	if(element != nil){
		storagecheck(element->ref == 1,"attachment array element baseline");
		freetype(element);
	}
	drainroots(); storagecheck(nativecount == 0 && poolowners() == 0 && modules == nil && Tmodlink.ref == 1 && Tarray.ref == 1,"attachment final baseline");
}


/* Invalid native targets, retain limits, cached publication and allocation failure are atomic. */
static void
attachguards(uchar *image, unsigned int bytes)
{
	Dis64StorageOwner *source;
	Module *module;
	int mode, saved;
	ulong dataref;

	reset(); source = prepare64storage(image,bytes); storagecheck(source != nil && source->data != H,"guard source");
	module = emptytarget();
	refusal(module,source,1);
	for(mode=0; mode<9; mode++){
		if(mode == 0) module->ref = 2;
		if(mode == 1) module->rt = DYNMOD;
		if(mode == 2) module->ntype = 1;
		if(mode == 3) module->origmp = nil;
		if(mode == 4) module->compiled = 1;
		if(mode == 5) module->nprog = 1;
		if(mode == 6) module->entryt = source->types[0];
		if(mode == 7) module->type = source->types;
		if(mode == 8) modules = module;
		refusal(module,source,0);
		module->ref = 1; module->rt = DONTCOMPILE; module->ntype = 0; module->origmp = H;
		module->compiled = module->nprog = 0; module->entryt = nil; module->type = nil; modules = nil;
	}
	saved = source->types[0]->ref; source->types[0]->ref = 0x7fffffff;
	refusal(module,source,0); source->types[0]->ref = saved;
	dataref = D2H(source->data)->ref; D2H(source->data)->ref = (ulong)~0;
	refusal(module,source,0); D2H(source->data)->ref = dataref;
	source->pinned = 0; refusal(module,source,0); source->pinned = 1;
	operation = 0;
	storagecheck(!attach64storage(nil,source) && !attach64storage(H,source) && !attach64storage(module,nil) && !attach64storage(module,H) && operation == 0,"nil attachment guards");
	/* Attachment reserves one table retain and shares the data Heap, not a new heapz Type retain. */
	saved = source->types[0]->ref; source->types[0]->ref = 0x7ffffffe;
	storagecheck(attach64storage(module,source) && source->types[0]->ref == 0x7fffffff,"one Type retain boundary");
	unload(module); source->types[0]->ref = saved; module = emptytarget();
	dataref = D2H(source->data)->ref; D2H(source->data)->ref = (ulong)~0-1;
	storagecheck(attach64storage(module,source) && D2H(source->data)->ref == (ulong)~0,"one Heap retain boundary");
	unload(module); D2H(source->data)->ref = dataref; module = emptytarget();
	unload(module); free64storage(source); drainroots();
	storagecheck(nativecount == 0 && poolowners() == 0,"guard complete baseline");
}


/* Exercise independent raw storage shapes and actual cache/unload/instance roots without code execution. */
int
main(int argc, char **argv)
{
	FILE *file;
	uchar image[16384], saved[16384];
	char name[128];
	unsigned int rows, row, length, bytes, expected;
	int publish;

	storagecheck(argc == 2,"attachment fixture argument");
	file = fopen(argv[1],"rb"); storagecheck(file != nil,"attachment fixture file");
	rows = word(file);
	for(row=0; row<rows; row++){
		length = word(file); storagecheck(length < sizeof(name) && fread(name,1,length,file) == length,"attachment fixture name");
		name[length] = 0; expected = word(file); bytes = word(file);
		storagecheck(expected == 1 && bytes <= sizeof(image) && fread(saved,1,bytes,file) == bytes,"attachment fixture bytes");
		if(sizeof(WORD) != 4){
			Dis64StorageOwner empty = {0};
			Module *module;
			empty.data = H; reset(); module = emptytarget(); operation = 0;
			storagecheck(!attach64storage(module,&empty) && operation == 0,"legacy attachment model");
			unload(module); fclose(file); printf("PASS legacy attachment model refuses before allocation\n"); return 0;
		}
		for(publish=0; publish<2; publish++){
			memmove(image,saved,bytes); attachcase(image,bytes,publish,0);
		}
		if(strcmp(name,"managed-storage") == 0){
			memmove(image,saved,bytes); attachcase(image,bytes,1,1);
			memmove(image,saved,bytes); attachguards(image,bytes);
		}
	}
	storagecheck(fgetc(file) == EOF,"attachment fixture trailer"); fclose(file);
	printf("PASS Dis64 Module attachment: %d attachments, %d refusals, %d cache/instance epochs, %d total GC epochs, %d checks\n",attachedcases,attachrefusals,cachecycles,collections,checks);
	return 0;
}
