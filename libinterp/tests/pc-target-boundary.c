

/* Preserve all state-image predicates in release-style sanitizer builds. */
static void
targetcheck(int condition)
{
	if(!condition)
		abort();
}


/* Copy a signed wire field without using native WORD width or alignment. */
static void
targetfield(void *data, int value, int bytes)
{
	vlong wide=value;

	memmove(data,bytes==4 ? (void*)&value : (void*)&wide,bytes);
}


/* Keep nonlocal VM errors outside the matrix's automatic object lifetimes. */
static int
targetinvoke(Jit *jit, int opcode, int route)
{
	raised=nil;
	if(setjmp(boundary))
		return 0;
	if(route==0)
		optab[opcode]();
	else if(route==1){
		if(!jit->entry[opcode](&R))
			optab[opcode]();
	}else
		targetcheck(jit->dispatch[opcode](&R)==1);
	return 1;
}


/* Construct destinations but never fetch or execute fabricated high-address code. */
int
main(void)
{
	static uchar memory[1024],before[1024],expectedbytes[1024];
	static REG saved,wanted;
	Module compiled={0},owner={0};
	Modlink link={0};
	Inst instructions[5]={{0}},sentinel;
	int opcodes[]={ICASE,ICASEL,IGOTO,IMOVPC,ICASEC};
	void (*handlers[])(void)={icase,casel,igoto,movpc,casec};
	int indices[]={0,1,2,7,31};
	Inst *destination;
	uintptr address,base;
	WORD source;
	LONG big=0;
	String *text=H;
	int family,index,tail,alignment,pattern,route,valid,accepted,header,stride,i;
	unsigned long positives=0,negatives=0;

	for(i=0;i<5;i++){
		instructions[i].op=opcodes[i];optab[opcodes[i]]=handlers[i];
	}
	compiled.prog=instructions;compiled.nprog=5;
	targetcheck(compile(&compiled,5,nil) && compiled.jit);
	for(i=0;i<5;i++)targetcheck(compiled.jit->entry[opcodes[i]]!=nil);
	link.m=&owner;owner.nprog=32;
	route=atoi(getenv("PC_TARGET_ROUTE"));
	targetcheck(route>=0 && route<=2);
	for(family=0;family<5;family++)for(index=0;index<5;index++)
	for(tail=0;tail<=sizeof(Inst)+2;tail++)for(alignment=0;alignment<8;alignment++)
	for(pattern=0;pattern<(family<2 ? 2 : 1);pattern++){
		if(getenv("PC_TARGET_FAMILY") && family!=atoi(getenv("PC_TARGET_FAMILY")))continue;
		memset(memory,0xa5,sizeof(memory));memset(&R,0,sizeof(R));
		address=(uintptr)-1-(uintptr)tail;
		base=address-(uintptr)indices[index]*sizeof(Inst);
		owner.prog=(Inst*)base;link.prog=owner.prog;
		R.M=&link;R.PC=&sentinel;R.FP=memory+800;R.SP=memory+900;
		R.s=memory+64+alignment;R.d=memory+256+alignment;R.m=H;
		R.IC=17;R.t=29;
		source=family==3 ? indices[index] : 0;
		if(family==4)memmove(R.s,&text,sizeof(text));
		else if(family==1)memmove(R.s,&big,sizeof(big));
		else memmove(R.s,&source,sizeof(source));
		if(family<2){
			header=family==1 ? 2*IBY2WD : IBY2WD;
			stride=family==1 ? 2*IBY2LG+2*IBY2WD : 3*IBY2WD;
			targetfield(R.d,pattern,4);
			if(pattern){
				targetfield((uchar*)R.d+header,0,family==1 ? 8 : 4);
				targetfield((uchar*)R.d+header+(family==1 ? IBY2LG : IBY2WD),1,family==1 ? 8 : 4);
				targetfield((uchar*)R.d+header+(family==1 ? 2*IBY2LG : 2*IBY2WD),indices[index],4);
			}
			targetfield((uchar*)R.d+header+pattern*stride,indices[index],4);
		}else if(family==2){
			targetfield((uchar*)R.d-IBY2WD,1,4);targetfield(R.d,indices[index],4);
		}else if(family==4){
			targetfield(R.d,0,IBY2WD);targetfield((uchar*)R.d+IBY2WD,indices[index],IBY2WD);
		}
		memmove(&saved,&R,sizeof(R));memmove(&wanted,&R,sizeof(R));
		memmove(before,memory,sizeof(memory));memmove(expectedbytes,memory,sizeof(memory));
		valid=tail>=sizeof(Inst);
		if(valid){
			destination=(Inst*)address;
			if(family==3)memmove(expectedbytes+256+alignment,&destination,sizeof(destination));
			else wanted.PC=destination;
		}
		accepted=targetinvoke(compiled.jit,opcodes[family],route);
		targetcheck(accepted==valid && (valid || raised==exBounds));
		targetcheck(!memcmp(&R,&wanted,sizeof(R)) && !memcmp(memory,expectedbytes,sizeof(memory)));
		if(valid)positives++;else negatives++;
	}
	targetcheck(positives && negatives);
	compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);
	printf("PASS PC target boundary WORD%zu cell%d route%d: %lu accepted, %lu rejected full Inst spans\n",
		sizeof(WORD)*8,IBY2WD,route,positives,negatives);
	return 0;
}
