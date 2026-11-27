static int numericsuccess, numericrefusal, numericfaults; // Independent complete/partial constructor observations.


/* Managed/JIT execution is proved separately; this fixture exercises actual constructor and collector ownership. */
int
compile(Module *module, int count, Modlink *link)
{
	USED(module); USED(count); USED(link);
	storagecheck(0,"unexpected native compiler service");
	return 0;
}


/* Copy the immutable caller label into genuine guarded native storage. */
static char*
numericdup(char *text)
{
	char *copy;
	ulong bytes;

	bytes = strlen(text)+1;
	copy = storagemalloc(bytes);
	if(copy != nil) memmove(copy,text,bytes);
	return copy;
}

#define mallocz(n,z) storagemallocz(n,z) // Observe actual numeric constructor allocation statements.
#define strdup(p) numericdup(p) // Preserve owned path copying and fault injection.
NUMERIC_IMPLEMENTATION
#undef mallocz
#undef strdup


/* Check every lowered entry/type/MP owner after source bytes can no longer be borrowed. */
static void
numericowned(Module *module, Dis64StorageOwner *roots)
{
	storagecheck(module != nil && roots != nil,"numeric owner pair");
	storagecheck(module->prog != nil && module->entry != nil && module->entry >= module->prog && module->entry < module->prog+module->nprog,"numeric entry");
	storagecheck(module->entryt != nil && (module->origmp == H || module->type[module->mptype] == D2H(module->origmp)->t),"numeric original ordinals");
	storagecheck(module->origmp == roots->data && (module->origmp == H ? roots->pinned == 0 : D2H(module->origmp)->ref == 2 && pins(module->origmp) == 1),"numeric construction root");
	storagecheck(strcmp(module->path,"numeric-stage") == 0 && strcmp(module->name,"unit") == 0,"numeric copied identity");
}


/* Drive real construction/failure cleanup, lease publication and actual module unload with guarded storage. */
int
main(int argc, char **argv)
{
	FILE *file;
	uchar image[16384], saved[16384];
	char name[128];
	Module *module;
	Dis64StorageOwner *roots;
	unsigned int rows, row, length, bytes, expected;
	int allocations, fault;

	storagecheck(argc == 2,"numeric fixture argument");
	file=fopen(argv[1],"rb"); storagecheck(file != nil,"numeric fixture file"); rows=word(file);
	for(row=0; row<rows; row++){
		length=word(file); storagecheck(length < sizeof(name) && fread(name,1,length,file) == length,"numeric fixture name"); name[length]=0;
		expected=word(file); bytes=word(file); storagecheck(bytes <= sizeof(image) && fread(saved,1,bytes,file) == bytes,"numeric fixture bytes");
		reset(); modulecontext=nil; roots=nil; memmove(image,saved,bytes);
		module=prepare64numeric("numeric-stage",image,bytes,0,&roots);
		if(sizeof(WORD) != 4){
			storagecheck(module == nil && roots == nil && operation == 0,"legacy numeric refusal");
			fclose(file); printf("PASS legacy numeric Module refusal before allocation\n"); return 0;
		}
		storagecheck((module != nil) == expected,"numeric constructor verdict");
		storagecheck(memcmp(image,saved,bytes) == 0,"numeric immutable input");
		if(!expected){
			storagecheck(roots == nil && operation == 0 && nativecount == 0,"numeric preflight atomic"); numericrefusal++; continue;
		}
		numericsuccess++; allocations=operation; memset(image,0xa5,bytes); numericowned(module,roots);
		epochs(6);
		module->link=modules; modules=module; free64storage(roots); roots=nil;
		epochs(6); storagecheck(modules == module && (module->origmp == H || D2H(module->origmp)->ref == 1),"numeric cache lifetime");
		unload(module); drainroots(); storagecheck(nativecount == 0 && poolowners() == 0 && modules == nil,"numeric complete baseline");
		for(fault=1; fault<=allocations; fault++){
			reset(); memmove(image,saved,bytes); roots=nil; failoperation=fault;
			module=prepare64numeric("numeric-stage",image,bytes,0,&roots);
			storagecheck(module == nil && roots == nil && errdepth == 0,"numeric allocation refusal"); numericfaults++;
			failoperation=0; drainroots(); storagecheck(nativecount == 0 && poolowners() == 0 && modules == nil,"numeric partial baseline");
		}
	}
	storagecheck(fgetc(file) == EOF,"numeric fixture trailer"); fclose(file);
	printf("PASS private numeric Module: %d successes, %d preflight refusals, %d allocation refusals, %d GC epochs, %d checks\n",numericsuccess,numericrefusal,numericfaults,collections,checks);
	return 0;
}
