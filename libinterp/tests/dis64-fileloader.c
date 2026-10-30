static const uchar *filesource; // Immutable host-file contents exposed to actual readmod.
static ulong filemtime = 0x12345678; // Filesystem identity, changed explicitly for refresh tests.
static ulong filebytes, fileposition; // Exact source extent and current read cursor.
static int fileopen, fileopens, filecloses, filechunk = 7; // Bounded host-I/O observations.
static int admissionrequired, admissionthrow, admissioncalls, constructorcalls; // Existing admission boundary and construction observations.
int dontcompile;


/* Expose one read-only test file without bypassing the production read loop. */
int
kopen(char *path, int mode)
{
	(void)path;
	require(mode == OREAD && !fileopen);
	fileopen = 1;
	fileposition = 0;
	fileopens++;
	return 7;
}


/* Return owned filesystem identity with a full-width Qid for the parser. */
Dir*
kdirfstat(int descriptor)
{
	Dir *directory;

	require(descriptor == 7 && fileopen);
	directory = mallocz(sizeof(*directory), 1);
	if(directory == nil)
		return nil;
	directory->length = filebytes;
	directory->dev = 37;
	directory->type = 19;
	directory->mtime = filemtime;
	directory->qid.path = 0x100000123ULL;
	directory->qid.vers = 0x23456789;
	directory->qid.type = 0x80;
	return directory;
}


/* Require the real reader to join short native replies into one complete image. */
long
kread(int descriptor, void *destination, long requested)
{
	ulong count;

	require(descriptor == 7 && fileopen && requested > 0);
	count = requested;
	if(count > filebytes-fileposition)
		count = filebytes-fileposition;
	if(count > filechunk)
		count = filechunk;
	memmove(destination, filesource+fileposition, count);
	fileposition += count;
	return count;
}


/* Observe descriptor closure before parsing or returning from any I/O failure. */
int
kclose(int descriptor)
{
	require(descriptor == 7 && fileopen);
	fileopen = 0;
	filecloses++;
	return 0;
}


/* Scalar Dis64 fixtures never select native shared objects. */
int
dynldable(int descriptor)
{
	(void)descriptor;
	require(0);
	return 0;
}


/* Reject accidental entry into the unrelated native-library loader. */
Module*
newdyncode(int descriptor, char *path, Dir *directory)
{
	(void)descriptor;
	(void)path;
	(void)directory;
	require(0);
	return nil;
}


/* Keep the unreachable native-library seek path linkable. */
vlong
kseek(int descriptor, vlong offset, int direction)
{
	(void)descriptor;
	(void)offset;
	(void)direction;
	require(0);
	return -1;
}


/* Model only the existing unsigned-file predicate; its implementation is unchanged. */
int
mustbesigned(char *path, uchar *image, ulong bytes, Dir *directory)
{
	(void)path;
	require(image != nil && bytes == filebytes && directory->qid.path == 0x100000123ULL);
	admissioncalls++;
	if(admissionthrow)
		error("fixture admission callback error");
	return admissionrequired;
}


/* No signature decoding is needed for the new unsigned scalar container. */
int
verifysigner(uchar *signature, int signaturebytes, uchar *image, ulong bytes)
{
	(void)signature;
	(void)signaturebytes;
	(void)image;
	(void)bytes;
	require(0);
	return 0;
}


/* Count the real constructor after routing has selected the new format. */
Module*
prepare64pilot(char *path, uchar *image, ulong bytes, int jit)
{
	constructorcalls++;
	return actualprepare64pilot(path, image, bytes, jit);
}


/* Legacy managed-string initializers are outside this scalar routing fixture. */
String*
c2string(char *text, int bytes)
{
	(void)text;
	(void)bytes;
	require(0);
	return nil;
}


/* Legacy array initializers are covered by the independent existing loader matrix. */
Heap*
heaparray(Type *type, int count)
{
	(void)type;
	(void)count;
	require(0);
	return nil;
}


/* No new-format scalar path may initialize an old array object. */
void
initarray(Type *type, Array *array)
{
	(void)type;
	(void)array;
	require(0);
}


/* New-format scalar Modules are not matched against legacy Dis-to-C image bytes. */
void
disc_attach(Module *module, const uchar *image, ulong bytes)
{
	(void)module;
	(void)image;
	(void)bytes;
	require(0);
}

#define A(slot) *((Array**)(slot)) // Existing parser array-owner addressing, never used by Dis64.
FILE_IMPLEMENTATION


/* Select actual file input and compiler request, then use the complete production reader. */
static Module*
fileprepare(char *path, uchar *image, ulong bytes, int jit)
{
	Module *module;
	int oldcompile;

	filesource = image;
	filebytes = bytes;
	oldcompile = cflag;
	cflag = jit;
	module = readmod(path, nil, 0);
	cflag = oldcompile;
	require(!fileopen && fileopens == filecloses);
	if(module != nil){
		require(modules == module && module->dev == 37 && module->dtype == 19);
		require(fileposition == bytes);
		require(module->mtime == filemtime && module->qid.path == 0x100000123ULL);
		require(module->qid.vers == 0x23456789 && module->qid.type == 0x80);
		require(module->compiled == 0 && (module->jit != nil) == (jit && !dontcompile));
	}
	return module;
}


/* Verify cache stability, all constructor/I/O allocation failures, and no format fallback. */
static void
fileadmission(uchar *image, unsigned int bytes)
{
	Module *anchor, *module;
	uchar changed[4096];
	int failure, total, baseline, before, i;

	anchor = fileprepare("anchor", image, bytes, 0);
	require(anchor != nil && anchor->ref == 1);
	module = lookmod("anchor");
	require(module == anchor && anchor->ref == 2);
	unload(module);
	require(lookmod("absent") == nil && anchor->ref == 1);
	baseline = live;
	before = constructorcalls;
	anchor->ref++;
	module = readmod("anchor", anchor, 0);
	require(module == anchor && anchor->ref == 2 && constructorcalls == before && fileposition == 0);
	unload(module);
	require(modules == anchor && live == baseline && !fileopen);
	filemtime++;
	anchor->ref++;
	module = readmod("anchor", anchor, 0);
	require(module != nil && module != anchor && module->mtime == filemtime && anchor->ref == 1);
	require(modules == module && module->link == anchor && constructorcalls == before+1);
	unload(module);
	filemtime--;
	require(modules == anchor && live == baseline && !fileopen);
	for(i=0; i<2; i++){
		admissionrequired = i == 0;
		admissionthrow = i == 1;
		allocations = 0;
		before = constructorcalls;
		require(fileprepare("rejected", image, bytes, 0) == nil && constructorcalls == before);
		require(modules == anchor && live == baseline && worker.nerr == 0);
	}
	admissionrequired = admissionthrow = 0;
	memmove(changed, image, bytes);
	changed[9] = 2;
	before = constructorcalls;
	require(fileprepare("future-version", changed, bytes, 0) == nil);
	require(constructorcalls == before+1 && modules == anchor && live == baseline);
	for(i=0; i<bytes; i++){
		require(fileprepare("truncated", image, i, 0) == nil);
		require(modules == anchor && live == baseline && worker.nerr == 0);
	}
	for(i=0; i<2; i++){
		allocations = 0;
		module = fileprepare("allocation-count", image, bytes, i);
		require(module != nil);
		total = allocations;
		unload(module);
		require(modules == anchor && live == baseline);
		for(failure=1; failure<=total; failure++){
			allocations = 0;
			failat = failure;
			require(fileprepare("allocation-failure", image, bytes, i) == nil);
			failat = 0;
			require(modules == anchor && live == baseline && worker.nerr == 0);
		}
	}
	for(i=0; i<3; i++){
		filechunk = i == 0 ? 1 : i == 1 ? 3 : 8192;
		module = fileprepare("short-read", image, bytes, 0);
		require(module != nil);
		unload(module);
		require(modules == anchor && live == baseline);
	}
	filechunk = 7;
	dontcompile = 1;
	module = fileprepare("compiler-disabled", image, bytes, 1);
	require(module != nil && module->jit == nil);
	unload(module);
	dontcompile = 0;
	unload(anchor);
	require(modules == nil && live == 0 && fileopens == filecloses);
}

FILE_SCHEDULER


/* Run actual reader/parser/cache and then the existing production scheduler lifecycle. */
int
main(int argc, char **argv)
{
	uchar image[4096], original[4096];
	unsigned int bytes;
	FILE *file;
	int before;

	require(argc == 3);
	worker.defenv.errstr = worker.defenv.errbuf0;
	worker.defenv.syserrstr = worker.defenv.errbuf1;
	worker.env = &worker.defenv;
	file = fopen(argv[1], "rb");
	require(file != nil);
	bytes = fread(image, 1, sizeof(image), file);
	require(!ferror(file) && feof(file) && fclose(file) == 0);
	if(strcmp(argv[2], "gated") == 0){
		Dis64Pilot semantic;

		require(dis64pilotcheck(image, bytes, 4096, 64, &semantic));
		before = constructorcalls;
		require(fileprepare("gated", image, bytes, 0) == nil && live == 0 && modules == nil);
#if defined(DIS64_PILOT_LOADER)
		require(constructorcalls == before+1 && admissioncalls == 1 && sizeof(WORD) != 4);
#else
		require(constructorcalls == before && admissioncalls == 0);
#endif
		printf("PASS Dis64 file loader gated: actual readmod/parsemod rejects valid image; no published owner\n");
		return 0;
	}
	memmove(original, image, bytes);
	fileadmission(image, bytes);
	require(memcmp(original, image, bytes) == 0);
	before = checks;
	require(schedulermain(argc, argv) == 0);
	require(modules == nil && live == 0 && fileopens == filecloses && checks > before);
	printf("PASS Dis64 file loader: %d predicates; reader/parser/cache + actual scheduler lifecycle; %d admission checks and %d constructors\n",
		checks, admissioncalls, constructorcalls);
	return 0;
}
