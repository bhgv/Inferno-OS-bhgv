#include <sys/mman.h>
#include <errno.h>

extern void *__real_mmap(void*, size_t, int, int, int, off_t); // Actual host mapping boundary.
extern int __real_munmap(void*, size_t); // Actual mapping retirement boundary.
extern int __real_mprotect(void*, size_t, int); // Actual RW-to-RX permission boundary.

static void *numericmapping; // Sole mapping owned by the currently constructed private Module.
static size_t numericmappingbytes; // Exact requested extent, independent of page rounding.
static int numericmapmode, numericprotectmode; // One-shot return/error injection at actual OS boundaries.
static int numericmaps, numericunmaps, numericpublishes; // Successful native ownership transitions.
static int numericfaults, numericthrows, numericsystemfaults; // Independent failure-path observations.


/* Fail at an active semantic ownership check rather than accepting a crash or sanitizer report. */
static void
numericfaultcheck(int condition, char *stage)
{
	if(!condition){
		fprintf(stderr, "numeric fault mismatch: %s\n", stage);
		exit(10);
	}
	checks++;
}


/* Exercise the actual nested compiler/constructor error frames before ownership publication. */
static void
numericthrow(void)
{
	error(exNomem);
}


/* Keep genuine host mappings and inject only the requested allocation boundary. */
void*
__wrap_mmap(void *address, size_t bytes, int protection, int flags, int descriptor, off_t offset)
{
	void *mapping;
	int mode;

	mode = numericmapmode;
	numericmapmode = 0;
	if(mode == 2)
		numericthrow();
	if(mode == 1){
		errno = ENOMEM;
		return MAP_FAILED;
	}
	numericfaultcheck(numericmapping == nil, "single mapping");
	mapping = __real_mmap(address, bytes, protection, flags, descriptor, offset);
	if(mapping != MAP_FAILED){
		numericmapping = mapping;
		numericmappingbytes = bytes;
		numericmaps++;
	}
	return mapping;
}


/* Verify exact ownership retirement through the real compiler/freecode path. */
int
__wrap_munmap(void *address, size_t bytes)
{
	int result;

	numericfaultcheck(address == numericmapping && bytes == numericmappingbytes, "mapping identity");
	result = __real_munmap(address, bytes);
	numericfaultcheck(result == 0, "host unmap");
	numericmapping = nil;
	numericmappingbytes = 0;
	numericunmaps++;
	return result;
}


/* A publication failure must release the still-private RW mapping and Jit table. */
int
__wrap_mprotect(void *address, size_t bytes, int protection)
{
	int mode, result;

	numericfaultcheck(address == numericmapping && bytes == numericmappingbytes, "publication identity");
	mode = numericprotectmode;
	numericprotectmode = 0;
	if(mode == 2)
		numericthrow();
	if(mode == 1){
		errno = EACCES;
		return -1;
	}
	numericfaultcheck(protection == (PROT_READ|PROT_EXEC), "RX permissions");
	result = __real_mprotect(address, bytes, protection);
	if(result == 0)
		numericpublishes++;
	return result;
}


/* Drain the genuine external-root metadata only after every partial/full owner has retired. */
static void
numericfaultbaseline(void)
{
	int i;

	for(i=0; i<PTRHASH; i++)
		numericfaultcheck(ptrtab[i] == nil, "root baseline");
	numericdrain();
	numericfaultcheck(live == 0 && modules == nil && worker.nerr == 0, "native baseline");
	numericfaultcheck(numericmapping == nil && numericmaps == numericunmaps, "mapping baseline");
}


/* Refusal must keep the output nil and leave no root, Type, Module, Jit or mapping owner. */
static void
numericfaultrefusal(Module *module, Dis64StorageOwner *roots)
{
	numericfaultcheck(module == nil && roots == nil, "atomic refusal");
	numericfaultbaseline();
}


/* Reject invalid output geometry before a typed output read or any native allocation. */
static void
numericfaultarguments(uchar *image, unsigned int bytes)
{
	union {
		void *alignment; // Keep the byte buffer naturally aligned before offsetting it.
		uchar bytes[2*sizeof(void*)]; // Native output storage plus a deliberately unaligned address.
	} output;
	uchar saved[sizeof(output)];
	Dis64StorageOwner *roots;
	Module *module;
	int start;

	start = allocations;
	memset(&output, 0xa5, sizeof(output));
	memmove(saved, &output, sizeof(output));
	module = prepare64numeric("numeric-fault", image, bytes, 0,
		(Dis64StorageOwner**)(output.bytes+1));
	numericfaultcheck(module == nil && memcmp(saved, &output, sizeof(output)) == 0,
		"unaligned output refusal");
	module = prepare64numeric("numeric-fault", image, bytes, 0,
		(Dis64StorageOwner**)((uintptr)-1-(sizeof(void*)-1)));
	numericfaultcheck(module == nil, "wrapped output refusal");
	module = prepare64numeric("numeric-fault", image, bytes, 0, nil);
	numericfaultcheck(module == nil, "nil output refusal");
	module = prepare64numeric("numeric-fault", image, bytes, 0, H);
	numericfaultcheck(module == nil, "H output refusal");
	roots = H;
	module = prepare64numeric("numeric-fault", image, bytes, 0, &roots);
	numericfaultcheck(module == nil && roots == H, "occupied output refusal");
	roots = nil;
	module = prepare64numeric("numeric-fault", image, bytes, -1, &roots);
	numericfaultcheck(module == nil && roots == nil, "negative Jit refusal");
	module = prepare64numeric("numeric-fault", image, bytes, 2, &roots);
	numericfaultcheck(module == nil && roots == nil, "unknown Jit refusal");
	numericfaultcheck(allocations == start, "argument allocation baseline");
	numericfaultbaseline();
}


/* Fault every native allocation, including the real Jit table, and both OS ownership boundaries. */
int
main(int argc, char **argv)
{
	FILE *file;
	uchar image[16384];
	Module *module;
	Dis64StorageOwner *roots;
	unsigned int rows, row, bytes;
	int jit, start, count, fault, mode, boundary;

	require(argc == 2);
	file = fopen(argv[1], "rb");
	require(file != nil);
	rows = readword(file);
	for(row=0; row<rows; row++){
		bytes = readword(file);
		require(bytes <= sizeof(image) && fread(image, 1, bytes, file) == bytes);
		numericfaultarguments(image, bytes);
		for(jit=0; jit<=1; jit++){
			roots = nil;
			start = allocations;
			module = prepare64numeric("numeric-fault", image, bytes, jit, &roots);
			numericfaultcheck(module != nil && roots != nil, "positive constructor");
			numericfaultcheck(jit ? module->jit != nil && numericmapping != nil : module->jit == nil, "positive Jit owner");
			count = allocations-start;
			unload(module);
			free64storage(roots);
			numericfaultbaseline();
			for(mode=1; mode<=2; mode++){
				for(fault=1; fault<=count; fault++){
					roots = nil;
					if(mode == 1)
						failat = allocations+fault;
					else
						numericthrowat = allocations+fault;
					module = prepare64numeric("numeric-fault", image, bytes, jit, &roots);
					failat = numericthrowat = 0;
					numericfaultrefusal(module, roots);
					if(mode == 1)
						numericfaults++;
					else
						numericthrows++;
				}
			}
		}
		for(boundary=0; boundary<2; boundary++){
			for(mode=1; mode<=2; mode++){
				roots = nil;
				if(boundary == 0)
					numericmapmode = mode;
				else
					numericprotectmode = mode;
				module = prepare64numeric("numeric-fault", image, bytes, 1, &roots);
				numericfaultcheck(numericmapmode == 0 && numericprotectmode == 0, "OS injection reached");
				numericfaultrefusal(module, roots);
				numericsystemfaults++;
			}
		}
	}
	require(fgetc(file) == EOF);
	fclose(file);
	printf("PASS private numeric Jit rollback: %u raw images, %d allocation refusals, %d VM-error refusals, %d mapping/publication refusals, %d mappings retired, %d RX publications, %d checks; no guest execution or collector\n", rows, numericfaults, numericthrows, numericsystemfaults, numericunmaps, numericpublishes, checks);
	return 0;
}
