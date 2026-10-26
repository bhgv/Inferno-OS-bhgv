/* Supply the builtin identity used by numeric leaf publication in this linked fixture. */
Type Tstring = {1}, Tarray = {1}; // Builtin identities used by native leaf publication.
int mutator,nprop;


/* Provide normal allocation ownership for the production publication path. */
void*
mallocz(ulong size,int clear)
{
	void *p;

	p=malloc(size);
	if(p && clear)memset(p,0,size);
	return p;
}


/* Compilation receives valid metadata and must complete without a VM error. */
int
kprotect(int (*fn)(void*),void *arg)
{
	return fn(arg);
}


/* Execute exact return images, cleanup fallback and write-free metadata failures. */
int
main(void)
{
	union {uvlong align;uchar bytes[512];} arena;
	uchar savedarena[512];
	Module module={0};
	Inst instruction={0},code[3];
	Frame links;
	Type type={0};
	ReturnOwner calleeowner={0};
	Modlink caller={0},*callee=&calleeowner.link;
	REG saved,expected;
	int alignment,kind,restore,pass,bad,native,failed;
	ulong reference,total=0;
	char *selection=getenv("RETURN_NATIVE_BAD");

	instruction.op=IRET;module.prog=&instruction;module.nprog=1;
	if(!compile(&module,1,nil) || !module.jit || !module.jit->entry[IRET])abort();
	for(alignment=0;alignment<8;alignment++)for(kind=0;kind<3;kind++)
	for(restore=0;restore<2;restore++)for(pass=0;pass<2;pass++)for(bad=0;bad<20;bad++){
		if(selection && bad!=atoi(selection))continue;
		memset(arena.bytes,0xa5,sizeof(arena.bytes));memset(&R,0,sizeof(R));
		expectedframe=(Frame*)(arena.bytes+128+alignment);expectedtype=&type;expectedmodule=callee;
		calleeowner.heap.ref=3;type.np=kind==2;links.t=kind==0 ? nil : &type;
		links.fp=arena.bytes+320;links.lr=code+2;links.mr=restore ? &caller : nil;
		caller.compiled=callee->compiled=0;caller.MP=arena.bytes+400;
		if(bad==1)links.fp=H;
		if(bad==2)links.lr=nil;
		if(bad==3)links.lr=H;
		if(bad==4)links.mr=H;
		if(bad==5)links.t=H;
		if(bad==6)type.np=-1;
		if(bad==7)links.fp=nil;
		if(bad==10 || bad==11)links.mr=&caller;
		if(bad==12 || bad==13)links.t=nil;
		if(bad==14){links.mr=&caller;calleeowner.heap.ref=0;}
		if(bad==15){links.mr=&caller;calleeowner.heap.ref=1;}
		if(bad==16){links.mr=&caller;caller.compiled=1;}
		if(bad==17)links.mr=callee;
		if(bad==18){links.mr=callee;calleeowner.heap.ref=1;}
		if(bad==19){links.mr=callee;calleeowner.heap.ref=0;}
		callee->MP=arena.bytes+448;
		memmove(expectedframe,&links,sizeof(links));
		if(kind==0 || bad==12 || bad==13){
			Type *extenttype=bad==12 ? nil : bad==13 ? H : expectedtype;
			memmove((uchar*)SEXTYPE(expectedframe)+offsetof(Stkext,reg.TR),&extenttype,sizeof(extenttype));
		}
		R.FP=(uchar*)expectedframe;R.PC=code;R.M=callee;R.SP=arena.bytes+480;
		if(bad==8)R.FP=nil;
		if(bad==9)R.FP=H;
		if(bad==10)R.M=nil;
		if(bad==11)R.M=H;
		reference=calleeowner.heap.ref;
		memmove(savedarena,arena.bytes,sizeof(savedarena));memmove(&saved,&R,sizeof(R));
		released=extended=mapped=0;
		native=(bad==0 || (bad>=15 && bad<=17)) && kind==1 &&
			(links.mr==nil || (bad!=15 && bad!=16));
		if(pass==0){
			if(module.jit->entry[IRET](&R)!=native)abort();
			if(!native && (memcmp(&saved,&R,sizeof(R)) || memcmp(savedarena,arena.bytes,sizeof(savedarena))))abort();
		}
		failed=0;
		if(pass!=0 || !native){
			if(setjmp(boundary)==0)ret();
			else failed=1;
		}
		if((bad && bad<15) || bad>=18){
			if(calleeowner.heap.ref!=reference)abort();
			if(!failed || memcmp(&saved,&R,sizeof(R)) || memcmp(savedarena,arena.bytes,sizeof(savedarena)) || released || extended || mapped)abort();
		}else{
			memmove(&expected,&saved,sizeof(expected));
			expected.FP=links.fp;expected.SP=(uchar*)expectedframe;expected.PC=links.lr;
			if(links.mr){
				expected.M=links.mr;expected.MP=links.mr->MP;
				if(bad==16){expected.IC=1;expected.t=1;}
			}
			if(calleeowner.heap.ref!=reference-(links.mr!=nil))abort();
			if(failed || memcmp(&expected,&R,sizeof(R)) || extended!=(kind==0) || mapped!=(kind==2) || released!=(links.mr!=nil && !(pass==0 && native)))abort();
		}
		total++;
	}
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS RET C/RX: %lu metadata/cleanup/REG/alias cases WORD%zu\n",total,sizeof(WORD)*8);
	return 0;
}
