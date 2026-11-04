#define GcSlots 8 // Collector chain header plus managed allocation slots.
#define GcSlotBytes 256 // Aligned stride for the largest object used here.

static union {
	void *alignment; // Align pool and Heap headers.
	uchar bytes[GcSlots*GcSlotBytes]; // Contiguous pool geometry traversed by rungc.
} numericArena;


/* Resolve an arena slot shared by the allocator and collector. */
static Bhdr*
numericblock(int slot)
{
	return (Bhdr*)(numericArena.bytes+slot*GcSlotBytes);
}


/* Recover the pool header without invoking the allocator fault macro. */
static Bhdr*
numericowner(Heap *heap)
{
	return (Bhdr*)((uchar*)heap-O(Bhdr, u.data));
}


/* Return the actual arena chain to the collector. */
Bhdr*
poolchain(Pool *pool)
{
	require(pool == heapmem);
	return numericblock(0);
}


/* Reserve a pool slot and let the real heap constructor initialize its object. */
void*
poolalloc(Pool *pool, ulong bytes)
{
	Bhdr *block;
	int slot;

	require(pool == heapmem && bytes <= GcSlotBytes-O(Bhdr, u.data));
	for(slot=1; slot<GcSlots; slot++){
		block = numericblock(slot);
		if(block->magic == MAGIC_F){
			block->magic = MAGIC_A;
			return B2D(block);
		}
	}
	return nil;
}


/* Keep retired bytes observable while making the slot available for reuse. */
void
poolfree(Pool *pool, void *pointer)
{
	Bhdr *block;
	int slot;

	require(pool == heapmem);
	for(slot=1; slot<GcSlots; slot++){
		block = numericblock(slot);
		if(B2D(block) == pointer){
			require(block->magic == MAGIC_A);
			block->magic = MAGIC_F;
			return;
		}
	}
	require(0);
}


/* Fail if the collector unexpectedly enters its diagnostic branch. */
int
print(char *format, ...)
{
	USED(format);
	require(0);
	return 0;
}


/* Set up empty pool geometry before the first real allocation. */
static void
numericpoolinit(void)
{
	Bhdr *block;
	int slot;

	for(slot=0; slot<GcSlots; slot++){
		block = numericblock(slot);
		block->size = GcSlotBytes;
		block->magic = MAGIC_F;
	}
	block = numericblock(0);
	block->magic = MAGIC_I;
	block->csize = sizeof(numericArena.bytes);
}


/* Collect on the real saved process registers until three color epochs complete. */
static void
numericcollect(Prog *process)
{
	ulong target;
	int steps;

	target = gcepochs+3;
	for(steps=0; gcepochs != target && steps<32; steps++)
		rungc(process);
	require(gcepochs == target && gcbroken == 0);
}


/* Run single-instruction quanta with collection between them, then execute IEXIT. */
static int
numericgc(uchar *image, unsigned int bytes, int jit, char *omit)
{
	Dis64StorageOwner *lease;
	Module *module;
	Modlink *link;
	Frame *entry, *source;
	Prog process;
	Heap *child, *frameChild, *garbage;
	Type *scalar, *mpType;
	void *value;
	uchar *frameSlot;
	int before, survived, slot, quantum;
	WORD result;

	lease = nil;
	module = prepare64numeric("numeric-gc", image, bytes, jit, &lease);
	require(module != nil && lease != nil);
	memset(&process, 0, sizeof(process));
	process.exval = H;
	process.state = Pready;
	process.quanta = 1;
	module->ref++;
	link = mklinkmod(module, 0);
	link->MP = module->origmp;
	D2H(link->MP)->ref++;
	process.R.M = link;
	process.R.PC = module->entry;
	entry = mallocz(module->entryt->size, 1);
	require(entry != nil);
	entry->t = module->entryt;
	source = entry;
	R.s = &source;
	newstack(&process);
	initmem(module->entryt, process.R.FP);
	frameSlot = process.R.FP+64;
	memmove(&value, frameSlot, sizeof(value));
	require(value == H);
	memmove(&value, frameSlot+8, sizeof(value));
	require(value == H);
	free(entry);
	free64storage(lease);
	for(slot=0; slot<PTRHASH; slot++)
		require(ptrtab[slot] == nil);
	require(modules == nil);

	/* Install a host-owned child into the mapped slot that numeric operands exclude. */
	scalar = dtype(freeheap, sizeof(WORD), nil, 0);
	require(scalar != nil);
	child = heapz(scalar);
	value = H2D(void*, child);
	memmove(link->MP, &value, sizeof(value));
	mpType = D2H(link->MP)->t;
	frameChild = heapz(scalar);
	value = H2D(void*, frameChild);
	memmove(frameSlot, &value, sizeof(value));
	if(omit != nil && strcmp(omit, "mp") == 0)
		mpType->mark = noptrs;
	if(omit != nil && strcmp(omit, "frame") == 0)
		module->entryt->mark = noptrs;
	rxcompleted = rxdeclined = 0;
	if(jit){
		require(module->jit != nil && module->jit->code != nil);
		rxbegin = (uintptr)module->jit->code;
		rxend = rxbegin+module->jit->size;
	}
	before = raised;
	survived = 3;
	for(quantum=1; quantum<=3; quantum++){
		garbage = heapz(scalar);
		require(kprotect(execute, &process) == 1 && raised == before);
		require(process.R.PC == module->entry+quantum);
		require(process.R.M == link && process.R.FP != nil);
		numericcollect(&process);
		require(numericowner(garbage)->magic == MAGIC_F);
		require(numericowner(D2H(link))->magic == MAGIC_A);
		require(numericowner(D2H(link->MP))->magic == MAGIC_A);
		memmove(&result, link->MP+8, sizeof(result));
		require(result == 40+2*quantum);
		if((survived & 1) && numericowner(child)->magic != MAGIC_A){
			survived &= ~1;
			value = H;
			memmove(link->MP, &value, sizeof(value));
		}
		if((survived & 2) && numericowner(frameChild)->magic != MAGIC_A){
			survived &= ~2;
			value = H;
			memmove(frameSlot, &value, sizeof(value));
		}
	}
	freetype(scalar);
	require(kprotect(execute, &process) == 0 && raised == before+1 && worker.nerr == 0);
	require(message[0] == 0);
	memmove(&result, link->MP+8, sizeof(result));
	require(result == 46 && rxcompleted == 3*jit && rxdeclined == 0);
	destroystack(&R);
	require(R.M == H && R.FP == nil && R.EX == nil && module->ref == 1);
	unload(module);
	numericdrain();
	require(live == 0 && Tmodlink.ref == 1);
	for(slot=1; slot<GcSlots; slot++)
		require(numericblock(slot)->magic == MAGIC_F);
	return survived;
}


/* Check C and machine-code execution, with separate mapped trace omission control. */
int
main(int argc, char **argv)
{
	FILE *file;
	uchar image[4096];
	size_t bytes;
	int jit, survived;

	require(argc == 2 || argc == 3);
	file = fopen(argv[1], "rb");
	require(file != nil);
	bytes = fread(image, 1, sizeof(image), file);
	require(bytes > 0 && bytes < sizeof(image) && feof(file));
	fclose(file);
	numericpoolinit();
	optab[IADDW] = addw;
	optab[IEXIT] = iexit;
	for(jit=0; jit<=1; jit++){
		survived = numericgc(image, bytes, jit, argc == 3 ? argv[2] : nil);
		if(survived != 3){
			fprintf(stderr, "numeric GC mismatch: %s child was swept\n", survived == 1 ? "frame" : survived == 2 ? "mp" : "both");
			return 10;
		}
	}
	printf("PASS numeric C/RX with real collector: %lu epochs, %d checks\n", gcepochs, checks);
	return 0;
}
