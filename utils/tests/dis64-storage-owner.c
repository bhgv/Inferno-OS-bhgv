#include <setjmp.h>

struct NativeBlock {
	void *pointer; // Genuine native allocation identity.
	ulong bytes; // Exact payload before guard bytes.
};

static struct NativeBlock nativeblocks[256]; // Native owner/Type/root metadata, distinct from VM pool objects.
static int nativecount, operation, failoperation, errdepth, constructions, refusals, collections; // Fault/ownership observations.
static jmp_buf errorframes[8]; // Portable implementation of the platform error-label boundary.
static char diagnostic[ERRMAX]; // Ordinary VM diagnostic output, not a proof token.
char exHeap[] = "heap"; // Production heap allocation refusal identity.


/* A semantic control stops safely and releases only genuine fixture-owned native storage. */
static void
storagecheck(int condition, char *stage)
{
	int i;

	if(!condition){
		for(i=0; i<nelem(nativeblocks); i++)
			free(nativeblocks[i].pointer);
		fprintf(stderr,"storage owner mismatch: %s\n",stage);
		exit(10);
	}
	checks++;
}


/* Retain real host allocations and independent guards; one exact allocation may refuse. */
static void*
storagemalloc(ulong bytes)
{
	void *p;
	int i;

	if(++operation == failoperation)
		return nil;
	for(i=0; i<nelem(nativeblocks) && nativeblocks[i].pointer != nil; i++)
		;
	storagecheck(i < nelem(nativeblocks),"native ledger capacity");
	p = malloc(bytes+32);
	storagecheck(p != nil,"host allocation");
	memset(p,0xa5,bytes+32);
	nativeblocks[i].pointer = p;
	nativeblocks[i].bytes = bytes;
	nativecount++;
	return p;
}


/* Zero precisely the native allocation requested by production dtype/constructor code. */
static void*
storagemallocz(ulong bytes, int clear)
{
	void *p;

	p = storagemalloc(bytes);
	if(p != nil && clear)
		memset(p,0,bytes);
	return p;
}


/* Observe one real retirement per native identity before returning storage to libc. */
static void
storagefree(void *p)
{
	int i, j;

	if(p == nil)
		return;
	for(i=0; i<nelem(nativeblocks) && nativeblocks[i].pointer != p; i++)
		;
	storagecheck(i < nelem(nativeblocks),"native retirement");
	for(j=0; j<32; j++)
		storagecheck(((uchar*)p)[nativeblocks[i].bytes+j] == 0xa5,"native guard");
	nativeblocks[i].pointer = nil;
	nativecount--;
	free(p);
}


/* Capture ordinary diagnostics while the real kprotect owns the callback error frame. */
void
kwerrstr(char *format, ...)
{
	va_list args;

	va_start(args,format);
	vsnprintf(diagnostic,sizeof(diagnostic),format,args);
	va_end(args);
}


/* Replace only the platform nonlocal jump, retaining production protected callback semantics. */
void
error(char *message)
{
	storagecheck(errdepth > 0 && errdepth <= nelem(errorframes),"protected error");
	snprint(diagnostic,sizeof(diagnostic),"%s",message);
	longjmp(errorframes[--errdepth],1);
}

#undef waserror
#undef poperror
#define waserror() setjmp(errorframes[errdepth++]) // Host platform boundary, with stable caller state.
#define poperror() (--errdepth) // Retire only the successful platform error frame.
PROTECTION_IMPLEMENTATION


/* Native heap consistency failures cannot masquerade as a semantic omission control. */
static void
storagepoolfault(void *p, char *message, ulong caller)
{
	USED(p); USED(message); USED(caller);
	storagecheck(0,"pool geometry");
}

void (*poolfault)(void*, char*, ulong) = storagepoolfault; // Actual heap consistency boundary.


/* Diagnostic return-PC capture is a native platform service, not a heap owner. */
uintptr
getcallerpc(void *p)
{
	USED(p);
	return 0;
}


/* Actual heapz allocates real Heap payloads in a bounded collector-traversable arena. */
void*
poolalloc(Pool *pool, ulong bytes)
{
	Bhdr *block;
	int i;

	storagecheck(pool == heapmem && bytes+O(Bhdr,u.data) <= SlotBytes,"pool allocation extent");
	if(++operation == failoperation)
		return nil;
	for(i=1; i<ArenaSlots; i++){
		block = (Bhdr*)(arena.bytes+i*SlotBytes);
		if(block->magic == MAGIC_F){
			block->magic = MAGIC_A;
			freed[i] = 0;
			return B2D(block);
		}
	}
	storagecheck(0,"pool capacity");
	return nil;
}

#define malloc(n) storagemalloc(n) // Observe only actual production native allocation statements.
#define mallocz(n,z) storagemallocz(n,z) // Preserve dtype/owner zero-initialization semantics.
COLLECTOR_IMPLEMENTATION
HEAP_IMPLEMENTATION
OWNER_IMPLEMENTATION
#undef malloc
#undef mallocz


/* Bound actual collector progress without manufacturing scheduler/module/Heap roots. */
static void
epochs(int count)
{
	ulong target;
	int i;

	target = gcepochs+count;
	for(i=0; gcepochs != target && i<200; i++)
		rungc(nil);
	storagecheck(gcepochs == target,"collector progress");
	collections += count;
}


/* Cached native root metadata is separately owned and explicitly drained between fault schedules. */
static void
drainroots(void)
{
	Ptrhash *p;
	int i;

	for(i=0; i<PTRHASH; i++)
		storagecheck(ptrtab[i] == nil,"root baseline");
	while(ptrfree != nil){
		p = ptrfree;
		ptrfree = p->next;
		storagefree(p);
	}
}


/* Initialize only native pool block geometry; every managed object comes from actual heapz. */
static void
reset(void)
{
	Bhdr *block;
	int i;

	drainroots();
	storagecheck(nativecount == 0 && errdepth == 0,"native baseline");
	memset(&arena,0,sizeof(arena));
	memset(freed,0,sizeof(freed));
	for(i=0; i<ArenaSlots; i++){
		block = (Bhdr*)(arena.bytes+i*SlotBytes);
		block->magic = MAGIC_F;
		block->size = SlotBytes;
	}
	block = (Bhdr*)arena.bytes;
	block->magic = MAGIC_I;
	block->clink = nil;
	block->csize = sizeof(arena.bytes);
	gccolor = 3; mutator = 0; marker = 1; sweeper = 2;
	base = limit = ptr = nil;
	visit = gce = markdepth = nprop = gchalt = 0;
	gct = 1; quanta = Quanta; modules = nil;
	operation = failoperation = 0;
}


/* Count exact independent external root entries for one real heap object. */
static int
pins(void *data)
{
	Ptrhash *p;
	int count, i;

	count = 0;
	for(i=0; i<PTRHASH; i++)
		for(p=ptrtab[i]; p != nil; p=p->next)
			if(p->value == D2H(data))
				count++;
	return count;
}


/* Preserve mapped children/cycles through real epochs, then prove RC and sweep ownership separately. */
static void
graph(Dis64StorageOwner *lease, int alias)
{
	uchar *data, *child;
	Heap *heap;
	Type *type;
	void *nilvalue;
	unsigned int bits;

	data = lease->data;
	type = lease->types[1];
	storagecheck(pins(data) == 1,"owned pin");
	nilvalue = H;
	storagecheck(memcmp(data,&nilvalue,sizeof(nilvalue)) == 0,"runtime nil");
	memmove(&bits,data+8,sizeof(bits));
	storagecheck(bits == 0x80000000U,"initialized data");
	heap = heapz(type);
	child = H2D(uchar*,heap);
	heap->ref++;
	memmove(child,&child,sizeof(child));
	memmove(data,&child,sizeof(child));
	Setmark(D2H(data));
	if(alias){
		D2H(data)->ref++;
		ptradd(D2H(data));
	}
	epochs(6);
	storagecheck(D2H(data)->t == type && D2H(child)->t == type && type->ref == 3,"collector graph");
	storagecheck(!freed[1] && !freed[2],"mapped child lifetime");
	free64storage(lease);
	storagecheck(pins(data) == alias,"owned root retirement");
	if(alias){
		epochs(6);
		storagecheck(!freed[1] && !freed[2] && D2H(data)->ref == 1,"independent alias lifetime");
		ptrdel(D2H(data));
		destroy(data);
	}
	storagecheck(freed[1] == 1 && !freed[2],"RC parent retirement");
	epochs(6);
	storagecheck(freed[2] == 1,"cycle sweep");
}


/* Read independently serialized fixture extents without borrowing an execution permission. */
static unsigned int
word(FILE *file)
{
	uchar p[4];

	storagecheck(fread(p,1,4,file) == 4,"fixture word");
	return (unsigned int)p[0]<<24 | (unsigned int)p[1]<<16 | (unsigned int)p[2]<<8 | p[3];
}


/* Test every construction allocation, source-independent descriptor ownership and actual collector roots. */
int
main(int argc, char **argv)
{
	uchar image[16384], saved[16384];
	char name[128];
	Dis64StorageOwner *lease;
	Dis64StorageImage view;
	Dis64StorageLimits limits = {64,65536,1048576,65536,65536,1024,256,1024};
	Dis64Type type;
	FILE *file;
	unsigned int rows, row, names, expected, bytes, i, ordinal;
	int allocations, fault, alias;

	storagecheck(argc == 2,"fixture argument");
	file = fopen(argv[1],"rb");
	storagecheck(file != nil,"fixture file");
	rows = word(file);
	for(row=0; row<rows; row++){
		names = word(file);
		storagecheck(names < sizeof(name) && fread(name,1,names,file) == names,"fixture name");
		name[names] = 0;
		expected = word(file); bytes = word(file);
		storagecheck(bytes <= sizeof(image) && fread(saved,1,bytes,file) == bytes,"fixture bytes");
		reset();
		memmove(image,saved,bytes);
		if(sizeof(WORD) != 4){
			storagecheck(prepare64storage(image,bytes) == nil && operation == 0 && nativecount == 0,"legacy model refusal");
			fclose(file);
			printf("PASS legacy model refuses valid managed storage before allocation\n");
			return 0;
		}
		lease = prepare64storage(image,bytes);
		storagecheck((lease != nil) == expected,"construction verdict");
		storagecheck(memcmp(image,saved,bytes) == 0,"immutable input");
		if(!expected){
			storagecheck(operation == 0 && nativecount == 0,"preflight allocation");
			refusals++;
			continue;
		}
		constructions++;
		allocations = operation;
		storagecheck(dis64storageimagecheck(saved,bytes,&limits,&view),"fixture structure");
		storagecheck(lease->count == view.types.count,"descriptor population");
		for(ordinal=0; ordinal<lease->count; ordinal++){
			storagecheck(dis64typeget(&view.types,ordinal,&type),"fixture descriptor");
			storagecheck(lease->types[ordinal]->size == type.bytes && lease->types[ordinal]->np == type.mapbytes &&
				memcmp(lease->types[ordinal]->map,type.map,type.mapbytes) == 0,"descriptor map");
		}
		memset(image,0xa5,bytes);
		if(lease->data != H){
			storagecheck(pins(lease->data) == 1,"owned pin");
			epochs(6);
			storagecheck(!freed[1],"data lifetime");
		}
		free64storage(lease);
		drainroots();
		storagecheck(nativecount == 0,"descriptor baseline");
		for(fault=1; fault<=allocations; fault++){
			reset(); memmove(image,saved,bytes); failoperation = fault;
			lease = prepare64storage(image,bytes);
			storagecheck(lease == nil && errdepth == 0,"allocation refusal");
			refusals++;
			drainroots();
			storagecheck(nativecount == 0,"partial native baseline");
			for(i=1; i<ArenaSlots; i++)
				storagecheck(((Bhdr*)(arena.bytes+i*SlotBytes))->magic == MAGIC_F,"partial heap baseline");
		}
		if(strcmp(name,"managed-storage") == 0)
			for(alias=0; alias<2; alias++){
				reset(); memmove(image,saved,bytes); lease = prepare64storage(image,bytes);
				storagecheck(lease != nil,"graph construction");
				memset(image,0xa5,bytes);
				graph(lease,alias);
				drainroots();
				storagecheck(nativecount == 0,"graph native baseline");
			}
	}
	storagecheck(fgetc(file) == EOF,"fixture trailer");
	fclose(file);
	free64storage(nil);
	printf("PASS Dis64 storage owner: %u rows, %d constructions, %d refusals, %d collector epochs, %d checks\n",rows,constructions,refusals,collections,checks);
	return 0;
}
