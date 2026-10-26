static int selected, native;
static Jit *code;
static int ops[]={INEWCB, INEWCW, INEWCL, INEWCF, INEWCP};


/* Model the normal decoder fallback when a guarded leaf declines an operand. */
static int
execute(void *unused)
{
	(void)unused;
	if(native && selected<5 && code->entry[ops[selected]](&R))
		return 1;
	switch(selected){
	case 0: newcb(); break;
	case 1: newcw(); break;
	case 2: newcl(); break;
	case 3: newcf(); break;
	case 4: newcp(); break;
	case 5: newcm(); break;
	case 6: newcmp(); break;
	}
	return 1;
}


/* Reject invalid capacity/output cells before queues, buffers or owner publication. */
static void
preflight(void)
{
	Channel *out;
	WORD len, width=13;
	int which, before;
	Heap header;
	Channel *saved;
	void *bad[] = {nil, H, (void*)(uintptr)-4};

	for(selected=0;selected<=5;selected+=5)
	for(native=0;native<2;native++)for(which=0;which<11;which++){
		out=H; len=1; R.d=&out; R.m=&len; R.s=&width;
		if(which<3)R.d=bad[which];
		else if(which<6)R.m=bad[which-3];
		else{
			memset(&header,0,sizeof(header));
			out=which==6 ? nil : which==7 ? (Channel*)8 :
				which==8 ? (Channel*)(uintptr)sizeof(Heap) :
				which==9 ? (Channel*)((uchar*)&header+sizeof(Heap)+1) : H2D(Channel*,&header);
		}
		saved=out;
		before=calls;last_error=nil;
		check(!kprotect(execute,nil));
		check(last_error==(which==6 ? exNilref : exHeap) && calls==before && live==0 && out==saved);
	}
	for(selected=5;selected<=6;selected++)for(which=0;which<3;which++){
		out=H; len=1; R.d=&out; R.m=&len; R.s=bad[which]; R.M=nil;
		before=calls;last_error=nil;
		check(!kprotect(execute,nil));
		check(last_error==exHeap && calls==before && live==0 && out==H);
	}
	puts("PASS scalar channel preflight: 50 scalar/raw/mapped slot/prefix/dead-owner/no-allocation cases");
}


/* Sweep real allocator failures, full-WORD capacities, operand aliases and replacement fallback. */
int
main(void)
{
	Module module={0};
	Inst instructions[5]={{0}};
	Type *types[]={&Tbyte,&Tword,&Tlong,&Treal,&Tptr};
	void (*movers[])(void)={movb,movw,movl,movf,movp};
	WORD lengths[]={0,1,3,-1,0x7fffffff,(WORD)0x8000000000000000ULL,(WORD)0x100000001ULL};
	uchar slots[112], before[112], expected[112];
	REG saved;
	Channel *out, *old;
	WORD len, count;
	int i, li, ma, da, alias, cutoff, ok, beforecalls, flags;
	int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	fenv_t environment;
	unsigned long checks=0;

	for(i=0;i<5;i++)instructions[i].op=ops[i];
	module.prog=instructions;module.nprog=5;
	check(compile(&module,5,nil) && module.jit!=nil);
	code=module.jit;
	for(i=0;i<5;i++)check(code->entry[ops[i]]!=nil);
	if(!getenv("CHANNEL_NO_ERRORS"))preflight();
	if(getenv("CHANNEL_PREFLIGHT_ONLY")){
		code->freecode(code->code,code->size);
		(free)(code);
		return 0;
	}
	fegetenv(&environment);
	for(selected=0;selected<5;selected++)for(li=0;li<7;li++)
	for(ma=0;ma<8;ma++)for(da=0;da<8;da++)for(alias=0;alias<3;alias++)
	for(cutoff=-1;cutoff<5;cutoff++)for(native=0;native<2;native++){
		if(getenv("CHANNEL_NO_ERRORS") && (li>2 || cutoff!=-1))continue;
		fail_after=-1;old=H;len=lengths[li];
		if(alias==2)old=cnewc(types[selected],movers[selected],0);
		memset(slots,0xa5,sizeof(slots));
		R.m=slots+8+ma;R.d=slots+64+da;
		memmove(R.m,&len,sizeof(len));memmove(R.d,&old,sizeof(old));
		if(alias==1)R.m=R.d;
		memmove(before,slots,sizeof(before));memmove(expected,before,sizeof(expected));
		memmove(&saved,&R,sizeof(R));
		count=alias==1 ? 0 : len;
		fail_after=cutoff;beforecalls=calls;last_error=nil;
		fesetround(modes[ma%4]);feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_DIVBYZERO);
		flags=fetestexcept(FE_ALL_EXCEPT);
		ok=kprotect(execute,nil);fail_after=-1;
		check(fegetround()==modes[ma%4] && fetestexcept(FE_ALL_EXCEPT)==flags);
		check(!memcmp(&saved,&R,sizeof(R)) && worker.nerr==0);
		memmove(&out,R.d,sizeof(out));
		if(ok){
			check(count>=0 && count<4 && out!=H && out!=old);
			check(out->mover==movers[selected] && out->front==0 && out->size==0);
			check(out->send!=nil && out->recv!=nil);
			check(count==0 ? out->buf==H : out->buf!=H && out->buf->len==count && out->buf->t==types[selected]);
			memmove(expected+64+da,&out,sizeof(out));
			destroy(out);
		}else{
			check(out==old && !memcmp(slots,before,sizeof(slots)));
			check(last_error==exNegsize || last_error==exHeap || last_error==exNomem);
			if(count<0 || (uvlong)count>=0x7fffffff)check(calls==beforecalls);
			if(old!=H)destroy(old);
		}
		check(!memcmp(slots,expected,sizeof(slots)));
		check(live==0 && types[selected]->ref==1 && Tchannel.ref==1 && Tarray.ref==1);
		checks++;
	}
	fesetenv(&environment);
	code->freecode(code->code,code->size);(free)(code);
	printf("PASS scalar channels C/RX: %lu allocator/type/alias/capacity/cleanup cases WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
