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
	Modlink caller={0},callee={0};
	REG saved,expected;
	int alignment,kind,restore,pass,bad,native,failed;
	ulong total=0;

	instruction.op=IRET;module.prog=&instruction;module.nprog=1;
	if(!compile(&module,1,nil) || !module.jit || !module.jit->entry[IRET])abort();
	for(alignment=0;alignment<8;alignment++)for(kind=0;kind<3;kind++)
	for(restore=0;restore<2;restore++)for(pass=0;pass<2;pass++)for(bad=0;bad<14;bad++){
		memset(arena.bytes,0xa5,sizeof(arena.bytes));memset(&R,0,sizeof(R));
		expectedframe=(Frame*)(arena.bytes+128+alignment);expectedtype=&type;expectedmodule=&callee;
		type.np=kind==2;links.t=kind==0 ? nil : &type;
		links.fp=arena.bytes+320;links.lr=code+2;links.mr=restore ? &caller : nil;
		caller.compiled=callee.compiled=0;caller.MP=arena.bytes+400;
		if(bad==1)links.fp=H;
		if(bad==2)links.lr=nil;
		if(bad==3)links.lr=H;
		if(bad==4)links.mr=H;
		if(bad==5)links.t=H;
		if(bad==6)type.np=-1;
		if(bad==7)links.fp=nil;
		if(bad==10 || bad==11)links.mr=&caller;
		if(bad>=12)links.t=nil;
		memmove(expectedframe,&links,sizeof(links));
		if(kind==0 || bad>=12){
			Type *extenttype=bad==12 ? nil : bad==13 ? H : expectedtype;
			memmove((uchar*)SEXTYPE(expectedframe)+offsetof(Stkext,reg.TR),&extenttype,sizeof(extenttype));
		}
		R.FP=(uchar*)expectedframe;R.PC=code;R.M=&callee;R.SP=arena.bytes+480;
		if(bad==8)R.FP=nil;
		if(bad==9)R.FP=H;
		if(bad==10)R.M=nil;
		if(bad==11)R.M=H;
		memmove(savedarena,arena.bytes,sizeof(savedarena));memmove(&saved,&R,sizeof(R));
		released=extended=mapped=0;
		native=bad==0 && kind==1 && !restore;
		if(pass==0){
			if(module.jit->entry[IRET](&R)!=native)abort();
			if(!native && (memcmp(&saved,&R,sizeof(R)) || memcmp(savedarena,arena.bytes,sizeof(savedarena))))abort();
		}
		failed=0;
		if(pass!=0 || !native){
			if(setjmp(boundary)==0)ret();
			else failed=1;
		}
		if(bad){
			if(!failed || memcmp(&saved,&R,sizeof(R)) || memcmp(savedarena,arena.bytes,sizeof(savedarena)) || released || extended || mapped)abort();
		}else{
			memmove(&expected,&saved,sizeof(expected));
			expected.FP=links.fp;expected.SP=(uchar*)expectedframe;expected.PC=links.lr;
			if(restore){expected.M=&caller;expected.MP=caller.MP;}
			if(failed || memcmp(&expected,&R,sizeof(R)) || extended!=(kind==0) || mapped!=(kind==2) || released!=restore)abort();
		}
		total++;
	}
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS RET C/RX: %lu metadata/cleanup/REG/alias cases WORD%zu\n",total,sizeof(WORD)*8);
	return 0;
}
