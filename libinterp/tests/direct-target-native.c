
typedef union DirectString DirectString;

union DirectString {
	uvlong align; // Naturally aligned String header origin.
	uchar bytes[sizeof(String)+32]; // Native header and actual character payload.
};


/* Keep complete-image and target-access checks active with NDEBUG. */
static void
directcheck(int condition)
{
	if(!condition)
		abort();
}


/* Compute predicates independently of emitter comparisons and branch encodings. */
static int
directpredicate(int relation, int order)
{
	switch(relation){
	case 0: return order==0;
	case 1: return order!=0;
	case 2: return order<0;
	case 3: return order<=0;
	case 4: return order>0;
	default: return order>=0;
	}
}


/* Isolate actual VM error unwinds from the caller's matrix storage. */
static int
directinvoke(Jit *jit, int opcode, int route)
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
		directcheck(jit->dispatch[opcode](&R)==1);
	return 1;
}


/* Check taken sentinels and unused poison targets through all 31 direct transfers. */
int
main(void)
{
	static uchar memory[512],expectedbytes[512];
	static REG saved,wanted;
	static DirectString strings[3];
	void (*handlers[])(void)={HANDLERS};
	int opcodes[]={OPCODES};
	Module compiled={0};
	Inst instructions[31]={{0}},sentinel;
	String *texts[4],*lefttext,*righttext;
	BYTE bytes[]={0,1,255},byte;
	WORD word;
	LONG big;
	REAL real;
	Rune rune=256;
	uvlong nanbits=0x7ff8000000000001ULL;
	void *target;
	char *failure;
	int op,kind,left,right,sa,da,mode,alias,route,take,valid,accepted,i;
	unsigned long checks=0,rejected=0,ignored=0;

	texts[0]=H;
	for(i=0;i<3;i++){
		texts[i+1]=(String*)strings[i].bytes;
		texts[i+1]->len=i==2 ? -1 : 1;texts[i+1]->max=8;
		if(i==2)memmove(texts[i+1]->Srune,&rune,sizeof(rune));
		else texts[i+1]->Sascii[0]=i==0 ? 'a' : 'z';
	}
	for(i=0;i<31;i++){
		instructions[i].op=opcodes[i];optab[opcodes[i]]=handlers[i];
	}
	compiled.prog=instructions;compiled.nprog=31;
	directcheck(compile(&compiled,31,nil) && compiled.jit);
	for(i=0;i<31;i++)directcheck(compiled.jit->entry[opcodes[i]]!=nil);
	route=atoi(getenv("DIRECT_TARGET_ROUTE"));directcheck(route>=0 && route<=2);
	for(op=0;op<31;op++)for(left=0;left<(op==30 ? 1 : op>=18 ? 4 : 3);left++)
	for(right=0;right<(op==30 ? 1 : op>=18 ? 4 : 3);right++)
	for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(mode=0;mode<7;mode++)for(alias=0;alias<2;alias++){
		if(getenv("DIRECT_TARGET_OP") && op!=atoi(getenv("DIRECT_TARGET_OP")))continue;
		memset(memory,0xa5,sizeof(memory));memset(&R,0,sizeof(R));
		R.s=memory+32+sa;R.m=memory+96+(sa+3)%8;R.d=memory+160+da;
		R.PC=&sentinel;R.FP=memory+320;R.SP=memory+448;R.IC=17;R.t=29;R.M=H;
		kind=op/6;
		if(kind==0){
			byte=bytes[left];memmove(R.s,&byte,sizeof(byte));byte=bytes[right];memmove(R.m,&byte,sizeof(byte));
		}else if(kind==1){
			word=left-1;memmove(R.s,&word,sizeof(word));word=right-1;memmove(R.m,&word,sizeof(word));
		}else if(kind==2){
			big=left-1;memmove(R.s,&big,sizeof(big));big=right-1;memmove(R.m,&big,sizeof(big));
		}else if(kind==3){
			real=left-1;memmove(R.s,left==3 ? (void*)&nanbits : (void*)&real,sizeof(real));
			real=right-1;memmove(R.m,right==3 ? (void*)&nanbits : (void*)&real,sizeof(real));
		}else if(kind==4){
			lefttext=texts[left];righttext=texts[right];memmove(R.s,&lefttext,sizeof(lefttext));memmove(R.m,&righttext,sizeof(righttext));
		}else R.s=R.m=H;
		take=op==30 || (kind==3 && (left==3 || right==3) ? op%6==1 :
			directpredicate(op%6,(left>right)-(left<right)));
		if(alias)R.d=&R.PC;
		target=mode==1 ? nil : mode==2 ? H : mode==3 ? (void*)((uintptr)-2) : memory+400;
		memmove(R.d,&target,sizeof(target));
		if(mode==4)R.d=H;
		if(mode==5)R.d=nil;
		if(mode==6)R.d=(void*)((uintptr)-2);
		memmove(&saved,&R,sizeof(R));memmove(&wanted,&R,sizeof(R));
		memmove(expectedbytes,memory,sizeof(memory));
		valid=!take || mode==0 || mode==3;
		if(take && valid)memmove(&wanted.PC,&target,sizeof(target));
		failure=kind==4 ? exHeap : exBounds;
		accepted=directinvoke(compiled.jit,opcodes[op],route);
		directcheck(accepted==valid && (valid || raised==failure));
		directcheck(!memcmp(&R,&wanted,sizeof(R)) && !memcmp(memory,expectedbytes,sizeof(memory)));
		checks++;if(!valid)rejected++;if(!take && mode>=4)ignored++;
	}
	directcheck(checks && rejected && (getenv("DIRECT_TARGET_OP") || ignored));
	compiled.jit->freecode(compiled.jit->code,compiled.jit->size);free(compiled.jit);
	printf("PASS direct targets WORD%zu route%d: %lu C/RX image routes, %lu errors, %lu ignored poisoned targets\n",
		sizeof(WORD)*8,route,checks,rejected,ignored);
	return 0;
}
