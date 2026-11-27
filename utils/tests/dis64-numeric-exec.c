static int rxcompleted, rxdeclined, executed, declined; // Real generated fast-return observations, distinct from C fallback.
static uintptr rxbegin, rxend; // Exact actual Jit executable mapping extent for the active Module.


/* Invoke the actual generated entry, count only its completed return and preserve ordinary refusal. */
static int
numericfastrun(int (*fast)(REG*), REG *reg)
{
	int result;

	require((uintptr)fast >= rxbegin && (uintptr)fast < rxend);
	result=fast(reg);
	if(result) rxcompleted++; else rxdeclined++;
	return result;
}

ROOT_IMPLEMENTATION
NUMERIC_IMPLEMENTATION


/* Retire actual external-root metadata cache after all owning leases have left. */
static void
numericdrain(void)
{
	Ptrhash *p;
	int i;

	for(i=0; i<PTRHASH; i++) require(ptrtab[i] == nil);
	while(ptrfree != nil){p=ptrfree;ptrfree=p->next;trackedfree(p);}
}


/* Execute an actual Frame/Modlink/decoder/VM/JIT program, then unwind every owner before semantic checks. */
static void
numericrun(Module *module, Dis64StorageOwner *roots, unsigned long long expected, int offset, int width, int failure, int jit, int disable)
{
	Modlink *link;
	Frame *entry, *source;
	Prog process;
	unsigned long long result;
	void *nilvalue;
	int before, completed, i;

	memset(&process,0,sizeof(process)); module->ref++;
	link=mklinkmod(module,0); link->MP=module->origmp;
	if(link->MP != H) D2H(link->MP)->ref++;
	process.R.M=link; process.R.PC=module->entry;
	entry=mallocz(module->entryt->size,1); require(entry != nil); entry->t=module->entryt;
	source=entry; R.s=&source; newstack(&process); initmem(module->entryt, process.R.FP); free(entry); process.quanta=1000;
	rxcompleted=rxdeclined=0;
	if(jit){
		require(module->jit != nil && module->jit->code != nil && module->jit->size > 0);
		rxbegin=(uintptr)module->jit->code; rxend=rxbegin+module->jit->size;
		if(disable) for(i=0; i<256; i++) module->jit->entry[i]=module->jit->dispatch[i]=nil;
	}
	before=raised; message[0]='x';
	require(kprotect(execute,&process) == 0 && raised == before+1 && worker.nerr == 0);
	require(failure ? strcmp(message,exZdiv) == 0 : message[0] == 0);
	result=0; memmove(&result,module->origmp+offset,width); require(result == expected);
	nilvalue=H; require(memcmp(module->origmp,&nilvalue,sizeof(nilvalue)) == 0);
	completed=rxcompleted;
	destroystack(&R); require(R.M == H && R.FP == nil && R.EX == nil && module->ref == 1);
	unload(module); free64storage(roots); numericdrain(); require(live == 0);
	if(jit && !failure && completed == 0){
		fprintf(stderr,"numeric execution mismatch: RX completion\n"); exit(10);
	}
	executed+=completed; declined+=rxdeclined;
}


/* Raw bytes are poisoned after owned construction; mapped reference bits must remain native H throughout execution. */
int
main(int argc, char **argv)
{
	FILE *file;
	uchar image[16384], saved[16384];
	char name[128];
	Module *module;
	Dis64StorageOwner *roots;
	unsigned int rows,row,length,bytes,expected,offset,width,failure,high,low;
	unsigned long long value;
	int jit,disable;

	require(argc == 2 || argc == 3); disable=argc == 3;
	file=fopen(argv[1],"rb"); require(file != nil);
	INIT_HANDLERS
	rows=readword(file);
	for(row=0; row<rows; row++){
		length=readword(file); require(length < sizeof(name) && fread(name,1,length,file) == length);name[length]=0;
		expected=readword(file); high=readword(file);low=readword(file);value=((unsigned long long)high<<32)|low;
		offset=readword(file);width=readword(file);failure=readword(file);bytes=readword(file);
		require(bytes <= sizeof(image) && fread(saved,1,bytes,file) == bytes);
		for(jit=0; jit<=1; jit++){
			roots=nil;memmove(image,saved,bytes);
			module=prepare64numeric("numeric-execution",image,bytes,jit,&roots);
			require((module != nil) == expected);
			if(!expected){require(roots == nil && live == 0);continue;}
			memset(image,0xa5,bytes);
			numericrun(module,roots,value,offset,width,failure,jit,disable);
		}
	}
	require(fgetc(file) == EOF);fclose(file);
	printf("PASS private numeric C/RX execution: %u raw images, %d real completed RX returns, %d RX refusals, %d checks; collector not run\n",rows,executed,declined,checks);
	return 0;
}
