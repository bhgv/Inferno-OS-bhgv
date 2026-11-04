static Dis64StorageOwner *clonesource; // Genuine live source lease during one protected clone.
static int clonerefs[128]; // Independent Type-retain baseline per original ordinal.
static int clonepolls, clonecalls, clonefaults, clonegraphs, bodyrefusals; // Active collection and refusal observations.
char exNegsize[] = "size"; // Actual array allocation refusal identity.


/* Only the platform nonlocal transfer is replaced; actual arraycpy uses actual krethrow. */
void
nexterror(void)
{
	storagecheck(errdepth > 0,"clone error frame");
	longjmp(errorframes[--errdepth],1);
}


/* Observe exact table retains before a missing retain could cause unsafe cleanup. */
static void
cloneobserve(Dis64StorageOwner *result, int stage)
{
	unsigned int i;
	Type *mp;

	storagecheck(clonesource != nil && result->count == clonesource->count,"clone population");
	mp = clonesource->data == H ? nil : D2H(clonesource->data)->t;
	for(i=0; i<result->count; i++){
		storagecheck(result->types[i] == clonesource->types[i],"clone ordinals");
		storagecheck(result->types[i]->ref == clonerefs[i]+1+(stage && result->types[i] == mp),"clone type retains");
	}
	if(stage)
		storagecheck(result->data != clonesource->data && pins(result->data) == 1,"clone pin");
}


/* Genuine production collector epochs may run at every descendant pool allocation. */
static void
cloneallocationpoll(void)
{
	if(clonepolls)
		epochs(3);
}


/* Root metadata caching is native-owned and may be drained while other real roots stay active. */
static void
clonecache(void)
{
	Ptrhash *p;

	while(ptrfree != nil){
		p = ptrfree; ptrfree = p->next; storagefree(p);
	}
}


/* Count actual arena owners, independent of native descriptor/root metadata. */
static int
poolowners(void)
{
	int i, n;

	n = 0;
	for(i=1; i<ArenaSlots; i++)
		if(((Bhdr*)(arena.bytes+i*SlotBytes))->magic == MAGIC_A)
			n++;
	return n;
}


/* Capture live Type references before a clone acquires any descriptor owner. */
static void
clonebaseline(Dis64StorageOwner *source)
{
	unsigned int i;

	clonesource = source;
	storagecheck(source->count <= nelem(clonerefs),"clone fixture population");
	for(i=0; i<source->count; i++)
		clonerefs[i] = source->types[i]->ref;
}


/* Build genuine shared leaves or two nested arrays; immediate pins cover construction allocations. */
static Type*
clonegraph(Dis64StorageOwner *source, int nested)
{
	Type *element;
	Array *outer, *inner;
	void *child;
	uchar map[] = {0x80};
	unsigned int value;

	child = H2D(void*,heapz(source->types[1]));
	ptradd(D2H(child));
	value = 73; memmove((uchar*)child+8,&value,sizeof(value));
	element = nil;
	if(nested){
		element = dtype(freeheap,sizeof(void*),map,sizeof(map));
		storagecheck(element != nil,"array element descriptor");
		outer = H2D(Array*,heaparray(element,1)); ptradd(D2H(outer));
		inner = H2D(Array*,heaparray(element,1)); ptradd(D2H(inner));
		memmove(inner->data,&child,sizeof(child)); Setmark(D2H(inner));
		memmove(outer->data,&inner,sizeof(inner)); Setmark(D2H(outer));
		memmove(source->data,&outer,sizeof(outer)); Setmark(D2H(source->data));
		ptrdel(D2H(inner)); ptrdel(D2H(outer));
	}else{
		memmove(source->data,&child,sizeof(child)); Setmark(D2H(source->data));
	}
	ptrdel(D2H(child));
	return element;
}


/* Independent clone semantics: scalar conservation, ordinary sharing and native recursive array copies. */
static void
clonedata(Dis64StorageOwner *source, Dis64StorageOwner *copy, int graphkind)
{
	void *original, *cloned, *leaf, *other;
	Array *a, *b;
	int level;

	if(source->data == H){
		storagecheck(copy->data == H,"empty cloned data");
		return;
	}
	storagecheck(copy->data != source->data && D2H(copy->data)->t == D2H(source->data)->t,"independent clone data");
	storagecheck(memcmp(copy->data+8,source->data+8,8) == 0,"clone scalar bits");
	memmove(&original,source->data,sizeof(original));
	memmove(&cloned,copy->data,sizeof(cloned));
	if(graphkind < 2){
		storagecheck(original == cloned,"ordinary reference sharing");
		if(graphkind == 1)
			storagecheck(D2H(original)->ref == 2,"shared child retain");
		return;
	}
	for(level=0; level<2; level++){
		a = original; b = cloned;
		storagecheck(a != b && D2H(a)->t == &Tarray && D2H(b)->t == &Tarray,"nested array independence");
		storagecheck(a->t == b->t && a->len == 1 && b->len == 1,"nested array geometry");
		memmove(&leaf,a->data,sizeof(leaf)); memmove(&other,b->data,sizeof(other));
		original = leaf; cloned = other;
	}
	storagecheck(original == cloned && D2H(original)->ref == 2,"nested leaf sharing");
}


/* Clone a source repeatedly with every fallible native/heap/root/array allocation refused in turn. */
static void
exercise(uchar *image, unsigned int bytes, int graphkind)
{
	Dis64StorageOwner *source, *copy;
	Type *element;
	uchar saved[16];
	unsigned int i;
	int baseowners, basenative, attempts, fault;

	reset(); clonepolls = 0;
	source = prepare64storage(image,bytes);
	storagecheck(source != nil,"source preparation");
	memset(image,0xa5,bytes);
	element = graphkind ? clonegraph(source,graphkind == 2) : nil;
	clonecache();
	if(source->data != H) memmove(saved,source->data,sizeof(saved));
	baseowners = poolowners(); basenative = nativecount;
	clonebaseline(source);
	operation = failoperation = 0; clonepolls = 1;
	copy = clone64storage(source); clonecalls++;
	storagecheck(copy != nil,"clone construction");
	attempts = operation;
	clonedata(source,copy,graphkind);
	free64storage(copy); clonecache();
	storagecheck(poolowners() == baseowners && nativecount == basenative,"clone cleanup baseline");
	for(fault=1; fault<=attempts; fault++){
		operation = 0; failoperation = fault;
		copy = clone64storage(source); clonefaults++;
		storagecheck(copy == nil && errdepth == 0,"clone allocation refusal");
		clonecache();
		storagecheck(poolowners() == baseowners && nativecount == basenative,"clone refusal baseline");
		if(source->data != H){
			storagecheck(pins(source->data) == 1 && memcmp(saved,source->data,sizeof(saved)) == 0,"source conservation");
		}
		for(i=0; i<source->count; i++)
			storagecheck(source->types[i]->ref == clonerefs[i],"source type conservation");
	}
	if(graphkind != 0){
		void *leaf;
		Array *array;
		ulong savedref;
		int level;

		memmove(&leaf,source->data,sizeof(leaf));
		if(graphkind == 2)
			for(level=0; level<2; level++){
				array = leaf; memmove(&leaf,array->data,sizeof(leaf));
			}
		savedref = D2H(leaf)->ref;
		D2H(leaf)->ref = (ulong)~0;
		operation = failoperation = 0;
		copy = clone64storage(source); bodyrefusals++;
		storagecheck(copy == nil && errdepth == 0,"nested body refusal");
		storagecheck(D2H(leaf)->ref == (ulong)~0,"nested body conservation");
		D2H(leaf)->ref = savedref;
		clonecache();
		storagecheck(poolowners() == baseowners && nativecount == basenative && pins(source->data) == 1,"nested body baseline");
		for(i=0; i<source->count; i++)
			storagecheck(source->types[i]->ref == clonerefs[i],"nested body type conservation");
	}
	operation = failoperation = 0;
	copy = clone64storage(source); clonecalls++;
	storagecheck(copy != nil,"final clone construction");
	clonedata(source,copy,graphkind);
	free64storage(source);
	if(copy->data != H){
		storagecheck(pins(copy->data) == 1,"independent clone root");
		epochs(6);
		storagecheck(D2H(copy->data)->ref == 1,"clone survives source retirement");
	}
	clonesource = nil;
	free64storage(copy); clonepolls = 0;
	if(element != nil){
		storagecheck(element->ref == 1,"array element baseline");
		freetype(element);
	}
	drainroots();
	storagecheck(nativecount == 0 && poolowners() == 0 && Tarray.ref == 1,"clone final baseline");
	clonegraphs++;
}


/* Reserve both MP descriptor retains before allocating; malformed or saturated sources remain unchanged. */
static void
retainlimits(uchar *image, unsigned int bytes)
{
	Dis64StorageOwner *source, *copy;
	int saved, ref, accepted;
	int values[] = {0,-1,0x7fffffff,0x7ffffffe,0x7ffffffd};
	unsigned int i;

	reset(); source = prepare64storage(image,bytes);
	storagecheck(source != nil && source->data != H,"retain source");
	saved = source->types[1]->ref;
	for(i=0; i<nelem(values); i++){
		ref = values[i]; source->types[1]->ref = ref;
		operation = failoperation = 0; clonebaseline(source);
		accepted = ref == 0x7ffffffd;
		copy = clone64storage(source);
		storagecheck((copy != nil) == accepted,"MP retain admission");
		if(copy != nil) free64storage(copy);
		else storagecheck(operation == 0,"MP retain preflight");
		storagecheck(source->types[1]->ref == ref,"MP retain conservation");
		clonecache();
	}
	source->types[1]->ref = saved;
	clonesource = nil;
	operation = 0;
	storagecheck(clone64storage(nil) == nil && clone64storage(H) == nil && operation == 0,"nil clone preflight");
	free64storage(source); drainroots();
	storagecheck(nativecount == 0 && poolowners() == 0,"retain final baseline");
}


/* Reuse every independently serialized accepted storage shape, then test nested managed instance copying. */
int
main(int argc, char **argv)
{
	FILE *file;
	uchar image[16384], saved[16384];
	char name[128];
	unsigned int rows, row, length, bytes, expected;

	storagecheck(argc == 2,"clone fixture argument");
	file = fopen(argv[1],"rb"); storagecheck(file != nil,"clone fixture file");
	rows = word(file);
	for(row=0; row<rows; row++){
		length = word(file); storagecheck(length < sizeof(name) && fread(name,1,length,file) == length,"clone fixture name");
		name[length] = 0; expected = word(file); bytes = word(file);
		storagecheck(expected == 1 && bytes <= sizeof(image) && fread(saved,1,bytes,file) == bytes,"clone fixture bytes");
		if(sizeof(WORD) != 4){
			Dis64StorageOwner empty = {0};
			empty.data = H;
			reset(); storagecheck(clone64storage(&empty) == nil && operation == 0,"legacy clone refusal");
			fclose(file); printf("PASS legacy clone model refusal before allocation\n"); return 0;
		}
		memmove(image,saved,bytes); exercise(image,bytes,0);
		if(strcmp(name,"managed-storage") == 0){
			memmove(image,saved,bytes); exercise(image,bytes,1);
			memmove(image,saved,bytes); exercise(image,bytes,2);
			memmove(image,saved,bytes); retainlimits(image,bytes);
		}
	}
	storagecheck(fgetc(file) == EOF,"clone fixture trailer"); fclose(file);
	printf("PASS Dis64 storage clones: %d graphs, %d graph clones, %d allocation refusals, %d body refusals, %d GC epochs, %d checks\n",clonegraphs,clonecalls,clonefaults,bodyrefusals,collections,checks);
	return 0;
}
