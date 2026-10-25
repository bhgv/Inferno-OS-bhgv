

/* A round trip releases the callee's acquired reference without destroying it. */
void
destroy(void *module)
{
	Heap *heap;

	heap = D2H(module);
	if(heap->ref <= 1)abort();
	heap->ref--;
}


/* Mix actual C and RX call/return paths while preserving independent VM images. */
int
main(void)
{
	Module compiled={0};
	Inst instructions[2]={{0}}, code[3];
	Modlink caller={0}, *target, *original;
	Heap *heap;
	Type type={0};
	Frame links, *frame;
	uchar bytes[sizeof(Frame)+8], slots[128], savedslots[128], expectedbytes[sizeof(Frame)+8];
	REG before, expected;
	WORD index=0;
	ulong refs[]={1,2,(ulong)~0-1}, total=0;
	int flags[]={0,1,-1,0x12345678};
	int mode, alias, pass, alignment, fa, ri;

	heap = calloc(1,sizeof(Heap)+sizeof(Modlink));
	if(!heap)abort();
	target = H2D(Modlink*,heap);
	instructions[0].op=IMCALL;
	instructions[1].op=IRET;
	compiled.prog=instructions;
	compiled.nprog=2;
	if(!compile(&compiled,2,nil) || !compiled.jit ||
	   !compiled.jit->entry[IMCALL] || !compiled.jit->entry[IRET])abort();
	for(mode=0;mode<4;mode++)for(alias=0;alias<2;alias++)for(pass=0;pass<4;pass++)
	for(alignment=0;alignment<8;alignment++)for(fa=0;fa<8;fa++)for(ri=0;ri<3;ri++){
		memset(&R,0,sizeof(R));
		memset(bytes,0xa5,sizeof(bytes));
		memset(slots,0xa5,sizeof(slots));
		memset(&links,0,sizeof(links));
		links.t=&type;
		frame=(Frame*)(bytes+fa);
		memmove(frame,&links,sizeof(links));
		heap->ref=refs[ri];
		target->nlinks=1;
		target->links[0].u.pc=code+1;
		target->prog=code;
		target->compiled=caller.compiled=flags[mode];
		target->MP=(uchar*)0x12345678;
		caller.MP=(uchar*)0x98765432;
		original=alias ? target : &caller;
		R.M=original;
		R.MP=original->MP;
		R.PC=code+2;
		R.FP=slots+96;
		R.SP=slots+112;
		R.IC=17;
		R.t=23;
		R.s=slots+alignment;
		R.m=slots+32+alignment;
		R.d=slots+64+alignment;
		memmove(R.s,&frame,sizeof(frame));
		memmove(R.m,&index,sizeof(index));
		memmove(R.d,&target,sizeof(target));
		memmove(&before,&R,sizeof(R));
		memmove(savedslots,slots,sizeof(slots));
		memmove(expectedbytes,bytes,sizeof(bytes));
		memmove(expectedbytes+fa+offsetof(Frame,lr),&before.PC,sizeof(before.PC));
		memmove(expectedbytes+fa+offsetof(Frame,fp),&before.FP,sizeof(before.FP));
		memmove(expectedbytes+fa+offsetof(Frame,mr),&original,sizeof(original));
		if(pass&1)mcall();
		else if(!compiled.jit->entry[IMCALL](&R))abort();
		if(R.M!=target || R.FP!=(uchar*)frame || R.PC!=code+1 || heap->ref!=refs[ri]+1)abort();
		if(pass&2)ret();
		else if(!compiled.jit->entry[IRET](&R))abort();
		memmove(&expected,&before,sizeof(expected));
		expected.SP=(uchar*)frame;
		expected.t=1;
		if(memcmp(&expected,&R,sizeof(R)) || memcmp(bytes,expectedbytes,sizeof(bytes)) ||
		   memcmp(slots,savedslots,sizeof(slots)) || heap->ref!=refs[ri])abort();
		total++;
	}
	compiled.jit->freecode(compiled.jit->code,compiled.jit->size);
	free(compiled.jit);
	free(heap);
	printf("PASS MCALL/RET round trips: %lu C/RX/mode/alias/refcount/alignment cases WORD%zu\n",
		total,sizeof(WORD)*8);
	return 0;
}
