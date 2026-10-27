typedef struct ControlImage ControlImage;

struct ControlImage {
	uchar bytes[4096]; // All mutable owners, frames, descriptors and decoded cells.
	Module module; // Aligned code owner for negative export lookup.
	Inst program[3]; // Stable original Dis return and destination identities.
};


/* Keep all image and error predicates active under NDEBUG. */
static void
controlcheck(int condition)
{
	if(!condition)
		abort();
}


/* Seed native-pointer metadata and decoded cells without adding alignment requirements. */
static void
controlimage(ControlImage *image, int op, int slotshift, int frameshift, int metashift, int alias)
{
	Frame frame;
	Modlink *callee, *caller;
	Module *module;
	Type type;
	Link exports[2];
	Heap heap;
	Modl entry;
	uchar *base, *address;
	WORD index;
	int offset;

	base=image->bytes;
	memset(image,0xa5,sizeof(*image));
	memset(&R,0xa5,sizeof(R));
	memset(&image->module,0,sizeof(image->module));
	memset(&frame,0,sizeof(frame));
	memset(&heap,0,sizeof(heap));
	memset(&type,0,sizeof(type));
	memset(exports,0,sizeof(exports));
	callee=(Modlink*)(base+512+metashift);
	caller=(Modlink*)(base+1024+metashift);
	address=base+2048+frameshift;
	heap.ref=3;
	memmove((uchar*)callee-sizeof(Heap),&heap,sizeof(heap));
	memmove((uchar*)caller-sizeof(Heap),&heap,sizeof(heap));
	memset(callee,0,sizeof(*callee));
	memset(caller,0,sizeof(*caller));
	module=&image->module;
	memmove((uchar*)callee+O(Modlink,m),&module,sizeof(module));
	offset=1;
	memmove((uchar*)callee+O(Modlink,nlinks),&offset,sizeof(offset));
	entry.u.pc=image->program+2;
	entry.frame=(Type*)(base+1536+metashift);
	memmove((uchar*)callee+O(Modlink,links),&entry,sizeof(entry));
	memmove((uchar*)callee+O(Modlink,prog),&entry.u.pc,sizeof(entry.u.pc));
	frame.lr=image->program+1;
	frame.fp=base+3072;
	frame.mr=alias==3 ? nil : alias==4 ? callee : caller;
	frame.t=(Type*)(base+1536+metashift);
	memmove(frame.t,&type,sizeof(type));
	memmove(address,&frame,sizeof(frame));
	exports[0].name="entry";
	exports[0].u=entry.u;
	exports[0].frame=entry.frame;
	image->module.ext=(Link*)(base+2560+metashift);
	memmove(image->module.ext,exports,sizeof(exports));
	R.s=base+64+slotshift;
	R.m=base+128+slotshift;
	R.d=base+192+slotshift;
	R.PC=image->program;
	R.FP=op==IRET ? address : base+3328;
	R.SP=base+3456;
	R.M=op==IRET ? callee : caller;
	R.MP=base+3584;
	R.IC=29;R.t=17;
	index=alias==5 ? -1 : 0;
	if(op==ICALL){
		if(alias==1)R.d=address+O(Frame,lr);
		if(alias==2)R.s=address+O(Frame,fp);
		memmove(R.s,&address,sizeof(address));
		memmove(R.d,&entry.u.pc,sizeof(entry.u.pc));
	}else if(op==IMCALL){
		if(alias==1)R.m=address+O(Frame,fp);
		if(alias==2)R.d=address+O(Frame,mr);
		memmove(R.s,&address,sizeof(address));
		memmove(R.m,&index,sizeof(index));
		memmove(R.d,&callee,sizeof(callee));
	}
}


/* Compare direct C, direct fast-plus-C and independently callable complete entries. */
static unsigned long
controlpositive(Jit *jit)
{
	ControlImage image,before,expected;
	REG saved,wanted;
	Frame frame;
	Modlink *callee,*caller;
	uchar *address,*mp;
	Inst *target;
	ulong references;
	unsigned long checks=0;
	int ops[]={ICALL,IMCALL,IRET};
	int operation,sa,fa,ma,alias,route,op;

	for(operation=0;operation<3;operation++)
	for(sa=0;sa<8;sa++)for(fa=0;fa<8;fa++)for(ma=0;ma<8;ma++)for(alias=0;alias<6;alias++){
		op=ops[operation];
		if((op==ICALL && alias==5) || (op==IRET && (alias==1 || alias==2 || alias==5)))continue;
		controlimage(&image,op,sa,fa,ma,alias);
		callee=(Modlink*)(image.bytes+512+ma);
		caller=(Modlink*)(image.bytes+1024+ma);
		address=image.bytes+2048+fa;
		memmove(&before,&image,sizeof(image));memmove(&expected,&image,sizeof(image));
		memmove(&saved,&R,sizeof(R));memmove(&wanted,&R,sizeof(R));
		if(op==IRET){
			memmove(&frame,address,sizeof(frame));
			wanted.FP=frame.fp;wanted.PC=frame.lr;wanted.SP=address;
			if(frame.mr!=nil){
				wanted.M=frame.mr;
				memmove(&mp,(uchar*)frame.mr+O(Modlink,MP),sizeof(mp));
				wanted.MP=mp;references=2;
				memmove(expected.bytes+512+ma-sizeof(Heap)+O(Heap,ref),&references,sizeof(references));
			}
		}else{
			memmove(expected.bytes+2048+fa+O(Frame,lr),&saved.PC,sizeof(saved.PC));
			memmove(expected.bytes+2048+fa+O(Frame,fp),&saved.FP,sizeof(saved.FP));
			wanted.FP=address;
			if(op==ICALL){memmove(&target,R.d,sizeof(target));wanted.PC=target;}
			else{
				memmove(expected.bytes+2048+fa+O(Frame,mr),&caller,sizeof(caller));
				wanted.M=callee;wanted.MP=nil;wanted.PC=image.program+2;wanted.t=1;
				references=4;
				memmove(expected.bytes+512+ma-sizeof(Heap)+O(Heap,ref),&references,sizeof(references));
			}
		}
		for(route=0;route<3;route++){
			memmove(&image,&before,sizeof(image));memmove(&R,&saved,sizeof(R));raised=nil;
			if(getenv("CONTROL_TRACE"))fprintf(stderr,"positive op=%d sa=%d fa=%d ma=%d alias=%d route=%d\n",op,sa,fa,ma,alias,route);
			if(route==0)optab[op]();
			else if(route==1){if(!jit->entry[op](&R))optab[op]();}
			else controlcheck(jit->dispatch[op](&R)==1);
			controlcheck(!memcmp(&image,&expected,sizeof(image)) && !memcmp(&R,&wanted,sizeof(R)) && raised==nil);
			checks++;
		}
	}
	return checks;
}


/* Preserve opaque legacy machine-code tokens without executing their destinations. */
static unsigned long
controltokens(Jit *jit)
{
	ControlImage image, before, expected;
	REG saved, wanted;
	Modlink *callee, *caller;
	Inst *target;
	uchar *address;
	ulong references;
	int flags[]={0,1,-1,0x12345678};
	int operation, mode, offset, route, op;
	unsigned long checks=0;

	for(operation=0; operation<2; operation++)
	for(mode=0; mode<4; mode++)for(offset=2; offset<=sizeof(Inst)+1; offset++){
		if(mode==0 && offset<=sizeof(Inst))continue;
		op=operation==0 ? ICALL : IMCALL;
		controlimage(&image,op,0,0,0,0);
		callee=(Modlink*)(image.bytes+512);
		caller=(Modlink*)(image.bytes+1024);
		address=image.bytes+2048;
		target=(Inst*)((uintptr)-offset);
		memmove((uchar*)callee+O(Modlink,compiled),&flags[mode],sizeof(int));
		memmove((uchar*)caller+O(Modlink,compiled),&flags[mode],sizeof(int));
		if(op==ICALL)memmove(R.d,&target,sizeof(target));
		else memmove((uchar*)callee+O(Modlink,links)+O(Modl,u),&target,sizeof(target));
		memmove(&before,&image,sizeof(image));memmove(&expected,&image,sizeof(image));
		memmove(&saved,&R,sizeof(R));memmove(&wanted,&R,sizeof(R));
		memmove(expected.bytes+2048+O(Frame,lr),&saved.PC,sizeof(saved.PC));
		memmove(expected.bytes+2048+O(Frame,fp),&saved.FP,sizeof(saved.FP));
		wanted.FP=address;wanted.PC=target;
		if(op==IMCALL){
			memmove(expected.bytes+2048+O(Frame,mr),&caller,sizeof(caller));
			wanted.M=callee;wanted.MP=nil;wanted.t=1;
			references=4;
			memmove(expected.bytes+512-sizeof(Heap)+O(Heap,ref),&references,sizeof(references));
		}
		for(route=0; route<3; route++){
			memmove(&image,&before,sizeof(image));memmove(&R,&saved,sizeof(R));raised=nil;
			if(route==0)optab[op]();
			else if(route==1)controlcheck(jit->entry[op](&R)==1);
			else controlcheck(jit->dispatch[op](&R)==1);
			controlcheck(!memcmp(&image,&expected,sizeof(image)) &&
				!memcmp(&R,&wanted,sizeof(R)) && raised==nil);
			checks++;
		}
	}
	return checks;
}


/* Exercise pure import/export lookup, including shifted metadata and atomic refusals. */
static unsigned long
controlentries(void)
{
	ControlImage image;
	Modlink *callee;
	Module *module,*shiftedmodule;
	Link *exports;
	Modl entry,canary;
	Inst *pc;
	Type *type;
	void *bad;
	WORD index;
	unsigned long checks=0;
	int shift,kind,offset,count;

	for(shift=0;shift<8;shift++){
		controlimage(&image,IMCALL,0,0,shift,0);
		callee=(Modlink*)(image.bytes+512+shift);
		module=&image.module;
		shiftedmodule=(Module*)(image.bytes+3072+shift);
		memmove(shiftedmodule,module,sizeof(*module));
		memmove((uchar*)callee+O(Modlink,m),&shiftedmodule,sizeof(shiftedmodule));
		for(index=-1;index<=0;index++){
			memset(&entry,0xa5,sizeof(entry));
			controlcheck(moduleentry(callee,index,&entry));
			pc=image.program+2;type=(Type*)(image.bytes+1536+shift);
			controlcheck(entry.u.pc==pc && entry.frame==type);
			checks++;
		}
		for(kind=0;kind<5;kind++)for(offset=0;offset<=sizeof(Module);offset++){
			controlimage(&image,IMCALL,0,0,shift,0);
			bad=offset==0 ? nil : (void*)((uintptr)-offset);
			index=0;
			if(kind==0){if(offset>O(Modlink,links))continue;callee=bad;}
			else if(kind==1){if(offset==0 || offset>sizeof(Heap))continue;callee=(Modlink*)(uintptr)offset;}
			else{
				callee=(Modlink*)(image.bytes+512+shift);
				if(kind==2){if(offset!=0)continue;count=-1;memmove((uchar*)callee+O(Modlink,nlinks),&count,sizeof(count));}
				else if(kind==3){index=-1;memmove((uchar*)callee+O(Modlink,m),&bad,sizeof(bad));}
				else{if(offset>sizeof(Link))continue;index=-1;exports=bad;memmove((uchar*)&image.module+O(Module,ext),&exports,sizeof(exports));}
			}
			memset(&entry,0xa5,sizeof(entry));memmove(&canary,&entry,sizeof(entry));
			controlcheck(!moduleentry(callee,index,&entry) && !memcmp(&entry,&canary,sizeof(entry)));
			checks++;
		}
	}
	return checks;
}


/* Require a complete, write-free decline followed by the ordinary VM error. */
static unsigned long
controlinvalid(Jit *jit)
{
	ControlImage image,before;
	REG saved;
	void *bad;
	unsigned long checks=0;
	int ops[]={ICALL,IMCALL,IRET};
	int operation,kind,offset,route,op,failed;

	if(getenv("CONTROL_NO_ERRORS"))return 0;
	for(operation=0;operation<3;operation++)for(kind=0;kind<17;kind++)for(offset=0;offset<128;offset++){
		op=ops[operation];
		if(getenv("CONTROL_BAD") && kind!=atoi(getenv("CONTROL_BAD")))continue;
		if(getenv("CONTROL_OP") && op!=atoi(getenv("CONTROL_OP")))continue;
		controlimage(&image,op,0,0,0,0);
		bad=offset==0 ? nil : (void*)((uintptr)-offset);
		if(kind==0 && op!=IRET){if(offset>sizeof(void*))continue;R.s=bad;}
		else if(kind==1 && op!=IRET){if(offset>sizeof(void*))continue;R.d=bad;}
		else if(kind==2 && op==IMCALL){if(offset>sizeof(WORD))continue;R.m=bad;}
		else if(kind==3){
			if(offset>(op==ICALL ? O(Frame,fp)+sizeof(void*) : op==IMCALL ? O(Frame,mr)+sizeof(void*) : sizeof(Frame)))continue;
			if(op==IRET)R.FP=bad;else memmove(R.s,&bad,sizeof(bad));
		}else if(kind==4 && op==IRET){
			if(offset==0 || offset>O(Type,np)+sizeof(int))continue;
			memmove((uchar*)R.FP+O(Frame,t),&bad,sizeof(bad));
		}else if(kind==5 && op==IRET){
			if(offset==0 || offset>O(Modlink,compiled)+sizeof(int))continue;
			memmove((uchar*)R.FP+O(Frame,mr),&bad,sizeof(bad));
		}else if(kind==6 && op==IMCALL){
			if(offset==0 || offset>O(Modlink,links))continue;
			memmove(R.d,&bad,sizeof(bad));
		}else if(kind==7 && (op==IMCALL || op==IRET)){
			if(offset==0 || offset>=sizeof(Heap))continue;
			bad=(void*)(uintptr)offset;
			if(op==IMCALL)memmove(R.d,&bad,sizeof(bad));else R.M=bad;
		}else if(kind>=8 && op==IRET && offset==0){
			bad=(void*)1;
			memmove((uchar*)R.FP+O(Frame,t),&bad,sizeof(bad));
			bad=kind==8 || kind==9 || kind==11 ? nil : H;
			if(kind==8)memmove((uchar*)R.FP+O(Frame,fp),&bad,sizeof(bad));
			if(kind==9)memmove((uchar*)R.FP+O(Frame,lr),&bad,sizeof(bad));
			if(kind==10)memmove((uchar*)R.FP+O(Frame,mr),&bad,sizeof(bad));
			if(kind>=11)R.M=bad;
		}else if(kind==13 && op==ICALL){
			if(offset>1)continue;
			bad=offset==0 ? nil : H;
			memmove(R.d,&bad,sizeof(bad));
		}else if(kind==14 && (op==ICALL || op==IMCALL)){
			if(offset>sizeof(Inst))continue;
			if(op==ICALL)memmove(R.d,&bad,sizeof(bad));
			else memmove(image.bytes+512+O(Modlink,links)+O(Modl,u),&bad,sizeof(bad));
		}else if(kind==15 && op==ICALL){
			Inst *target=(Inst*)((uintptr)-2);

			if(offset>O(Modlink,compiled)+sizeof(int))continue;
			memmove(R.d,&target,sizeof(target));
			R.M=bad;
		}else if(kind==16 && op==IRET){
			if(offset==0 || offset>sizeof(Frame))continue;
			memmove((uchar*)R.FP+O(Frame,fp),&bad,sizeof(bad));
		}else continue;
		memmove(&before,&image,sizeof(image));memmove(&saved,&R,sizeof(R));
		if(getenv("CONTROL_TRACE"))fprintf(stderr,"negative op=%d kind=%d offset=%d\n",op,kind,offset);
		controlcheck(jit->entry[op](&R)==0 && !memcmp(&R,&saved,sizeof(R)) && !memcmp(&image,&before,sizeof(image)));
		for(route=0;route<2;route++){
			memmove(&image,&before,sizeof(image));memmove(&R,&saved,sizeof(R));raised=nil;failed=0;
			if(setjmp(boundary)==0){if(route==0)optab[op]();else jit->dispatch[op](&R);}else failed=1;
			controlcheck(failed && raised!=nil && !memcmp(&R,&saved,sizeof(R)) && !memcmp(&image,&before,sizeof(image)));
			checks++;
		}
	}
	return checks;
}


/* Exercise actual compiler publication and both VM WORD models. */
int
main(void)
{
	Module module={0};
	Inst program[3]={{0}};
	unsigned long positive=0,negative=0,entries=0,tokens=0;
	char *section=getenv("CONTROL_SECTION");

	program[0].op=ICALL;program[1].op=IMCALL;program[2].op=IRET;
	module.prog=program;module.nprog=3;
	optab[ICALL]=call;optab[IMCALL]=mcall;optab[IRET]=ret;
	controlcheck(compile(&module,3,nil) && module.jit);
	if(section==nil || !strcmp(section,"positive"))positive=controlpositive(module.jit);
	if(section==nil || !strcmp(section,"invalid"))negative=controlinvalid(module.jit);
	if(section==nil || !strcmp(section,"entry"))entries=controlentries();
	if(section==nil || !strcmp(section,"tokens"))tokens=controltokens(module.jit);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS control C/RX WORD%zu: %lu complete-image routes, %lu rejected routes, %lu entry checks, %lu native-token constructions\n",sizeof(WORD)*8,positive,negative,entries,tokens);
	return 0;
}
