typedef struct TokenValue TokenValue;

struct TokenValue {
	WORD count; // Number of published words.
	List *words; // Owned result list published only after construction succeeds.
};


/* Check empty results, last retains and cleanup after descriptor capacity runs out. */
int
main(void)
{
	Rune input[]={'a',' ','b',' ','c'},delimiters[]={' '};
	int refs[]={0,-1,0x7fffffff,0x7ffffffe,0x7ffffffd};
	F_Sys_tokenize frame={0};
	uchar slots[sizeof(TokenValue)+8],saved[sizeof(TokenValue)+8];
	WORD count;
	List *words;
	int mode,n,wide,alignment,baseline,budget,ok,valid,i;
	ulong total=0;

	for(mode=0;mode<5;mode++)for(n=0;n<4;n++)for(wide=0;wide<2;wide++)for(alignment=0;alignment<8;alignment++){
		Tptr.ref=1;failure=0;
		frame.s=string(input,n ? n*2-1 : 0,wide);frame.delim=string(delimiters,1,wide);
		memset(slots,0xa5,sizeof(slots));count=-77;words=H;
		memmove(slots+alignment+offsetof(TokenValue,count),&count,sizeof(count));
		memmove(slots+alignment+offsetof(TokenValue,words),&words,sizeof(words));
		frame.ret=(void*)(slots+alignment);memmove(saved,slots,sizeof(slots));
		Tptr.ref=refs[mode];budget=refs[mode]>0 ? 0x7fffffff-refs[mode] : 0;
		valid=n==0 || n<=budget;baseline=live;allocations=0;raised=nil;
		ok=kprotect(invoke,&frame);
		if(ok!=valid || worker.nerr)abort();
		if(!ok){
			if(raised!=exHeap || live!=baseline || allocations!=2*budget || Tptr.ref!=refs[mode] || memcmp(saved,slots,sizeof(slots)))abort();
		}else{
			memmove(&count,slots+alignment+offsetof(TokenValue,count),sizeof(count));
			memmove(&words,slots+alignment+offsetof(TokenValue,words),sizeof(words));
			if(count!=n || allocations!=2*n || Tptr.ref!=refs[mode]+n)abort();
			for(i=0;i<sizeof(slots);i++)
				if(!((i>=alignment && i<alignment+sizeof(WORD)) ||
				   (i>=alignment+offsetof(TokenValue,words) && i<alignment+offsetof(TokenValue,words)+sizeof(words))) && slots[i]!=saved[i])abort();
			destroy(words);
			if(live!=baseline || Tptr.ref!=refs[mode])abort();
		}
		Tptr.ref=1;destroy(frame.s);destroy(frame.delim);
		if(live || Tstring.ref!=1 || Tlist.ref!=1)abort();
		total++;
	}
	printf("PASS token retains: %lu descriptor/partial-cleanup/publication cases WORD%zu\n",total,sizeof(WORD)*8);
	return 0;
}
