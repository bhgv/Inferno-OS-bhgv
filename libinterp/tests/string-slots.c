
/* Check publication boundaries without reading pointer slots through typed casts. */
static void
slotresult(uchar *bytes, uchar *before, int offset, int size)
{
	assert(!memcmp(bytes, before, offset));
	assert(!memcmp(bytes+offset+size, before+offset+size, 128-offset-size));
	checks++;
}

/* Keep the runtime return/error family independently runnable in a fresh process. */
static void
checkreturns(void)
{
	union {uvlong align;uchar bytes[128];} slots;
	uchar before[128];
	String *old,*result;
	int sa,op,alias;
	int lengthonly=getenv("STRING_SLOT_CASE") && !strcmp(getenv("STRING_SLOT_CASE"),"returns-length");
	int validonly=getenv("STRING_SLOT_CASE") && !strcmp(getenv("STRING_SLOT_CASE"),"returns-valid");

	/* Runtime return slots may alias the C-string owner and have any byte alignment. */
	if(!lengthonly)
	for(sa=0;sa<8;sa++)for(op=0;op<2;op++)for(alias=0;alias<2;alias++){
		old=c2string("old",3);memset(slots.bytes,0xa5,128);R.d=slots.bytes+32+sa;
		memmove(R.d,&old,sizeof(old));memmove(before,slots.bytes,128);
		if(!validonly){
		failure=0;
		if(op)EXPECT(retnstr(alias ? old->Sascii : "abc",alias ? 2 : 3,(String**)R.d),exNomem);
		else EXPECT(retstr(alias ? old->Sascii : "abc",(String**)R.d),exNomem);
		failure=-1;assert(!memcmp(before,slots.bytes,128) && D2H(old)->ref==1);
		}
		if(op)retnstr(alias ? old->Sascii : "abc",alias ? 2 : 3,(String**)R.d);
		else retstr(alias ? old->Sascii : "abc",(String**)R.d);
		memmove(&result,R.d,sizeof(result));assert(result->len==(op && alias ? 2 : 3));
		assert(!memcmp(result->Sascii,alias ? "old" : "abc",result->len));
		slotresult(slots.bytes,before,32+sa,sizeof(result));destroy(result);
		old=c2string("old",3);memmove(R.d,&old,sizeof(old));
		if(op)retnstr(nil,0,(String**)R.d);else retstr(nil,(String**)R.d);
		memmove(&result,R.d,sizeof(result));assert(result==H);checks++;
	}
	if(validonly)return;
	for(sa=0;sa<8;sa++)for(op=0;op<3;op++){
		old=c2string("old",3);R.d=slots.bytes+32+sa;memmove(R.d,&old,sizeof(old));
		returnlength=op==0 ? 0x100000001ULL : op==1 ? 0x80000000ULL : 0x7fffffffULL;
		EXPECT(retstr("a",(String**)R.d),exHeap);returnlength=0;
		memmove(&result,R.d,sizeof(result));assert(result==old && D2H(old)->ref==1);destroy(old);
	}
}

/* Exercise independent slot alignments, exact owner aliases and failure atomicity. */
int
main(void)
{
	union {uvlong align; uchar bytes[128];} slots;
	uchar before[128];
	String *s, *t, *result, *old;
	Array *a;
	WORD start, end, word, parsed;
	LONG big,parsedbig;
	REAL real, number;
	int sa, ma, da, op, alias, wide, i;
	void (*formats[])(void)={cvtwc,cvtlc,cvtfc};

	if(getenv("STRING_SLOT_CASE") && !strncmp(getenv("STRING_SLOT_CASE"),"returns",7)){checkreturns();goto finished;}

	for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(op=0;op<3;op++){
		word=-1234567;big=-9223372036854775807LL;real=-123.5;
		memset(slots.bytes,0xa5,128);R.s=slots.bytes+16+sa;R.d=slots.bytes+80+da;
		old=c2string("old",3);memmove(R.d,&old,sizeof(old));
		if(op==0)memmove(R.s,&word,sizeof(word));
		if(op==1)memmove(R.s,&big,sizeof(big));
		if(op==2)memmove(R.s,&real,sizeof(real));
		memmove(before,slots.bytes,128);
		failure=0;EXPECT(formats[op](),exNomem);failure=-1;
		assert(!memcmp(slots.bytes,before,128) && D2H(old)->ref==1);
		formats[op]();memmove(&result,R.d,sizeof(result));
		assert(!strcmp(string2c(result),op==0 ? "-1234567" : op==1 ? "-9223372036854775807" : "-123.5"));
		slotresult(slots.bytes,before,80+da,sizeof(result));destroy(result);
	}
	for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(wide=0;wide<3;wide++)for(alias=0;alias<2;alias++){
		s=wide==2 ? H : wide ? newrunes(4) : c2string("-123",4);
		if(wide==1){s->Srune[0]='-';s->Srune[1]='1';s->Srune[2]='2';s->Srune[3]='3';}
		for(op=0;op<3;op++){
			memset(slots.bytes,0xa5,128);R.s=slots.bytes+16+sa;R.d=alias ? R.s : slots.bytes+80+da;
			memmove(R.s,&s,sizeof(s));memmove(before,slots.bytes,128);
			if(op==0){cvtcw();memmove(&parsed,R.d,sizeof(parsed));assert(parsed==(wide==2 ? 0 : -123));}
			else if(op==1){cvtcf();memmove(&number,R.d,sizeof(number));assert(number==(wide==2 ? 0. : -123.));}
			else{cvtcl();memmove(&parsedbig,R.d,sizeof(parsedbig));assert(parsedbig==(wide==2 ? 0 : -123));}
			slotresult(slots.bytes,before,(uchar*)R.d-slots.bytes,op==2 ? sizeof(parsedbig) : op==1 ? sizeof(number) : sizeof(parsed));
		}destroy(s);
	}
	for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(alias=0;alias<2;alias++){
		s=c2string("abc",3);old=alias ? s : c2string("old",3);
		memset(slots.bytes,0xa5,128);R.s=slots.bytes+16+sa;R.d=alias ? R.s : slots.bytes+80+da;
		memmove(R.s,&s,sizeof(s));memmove(R.d,&old,sizeof(old));memmove(before,slots.bytes,128);
		cvtca();memmove(&a,R.d,sizeof(a));assert(a->len==3 && !memcmp(a->data,"abc",3));
		slotresult(slots.bytes,before,(uchar*)R.d-slots.bytes,sizeof(a));if(!alias)destroy(s);
		old=c2string("old",3);memset(slots.bytes,0xa5,128);memmove(R.s,&a,sizeof(a));
		if(!alias)memmove(R.d,&old,sizeof(old));else destroy(old);
		memmove(before,slots.bytes,128);cvtac();memmove(&result,R.d,sizeof(result));
		assert(result->len==3 && !memcmp(result->Sascii,"abc",3));
		slotresult(slots.bytes,before,(uchar*)R.d-slots.bytes,sizeof(result));if(!alias)destroy(a);destroy(result);
	}
	for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++)for(alias=0;alias<2;alias++){
		memset(slots.bytes,0xa5,128);R.s=slots.bytes+16+sa;R.m=slots.bytes+48+ma;R.d=slots.bytes+80+da;
		start=1;end=3;s=c2string("abcd",4);memmove(R.s,&start,sizeof(start));memmove(R.m,&end,sizeof(end));memmove(R.d,&s,sizeof(s));
		memmove(before,slots.bytes,128);slicec();memmove(&result,R.d,sizeof(result));
		assert(result->len==2 && !memcmp(result->Sascii,"bc",2));slotresult(slots.bytes,before,80+da,sizeof(result));destroy(result);
		s=c2string("ab",2);t=c2string("cd",2);old=alias ? s : c2string("old",3);
		R.d=alias ? R.m : slots.bytes+80+da;memmove(R.s,&t,sizeof(t));memmove(R.m,&s,sizeof(s));memmove(R.d,&old,sizeof(old));memmove(before,slots.bytes,128);
		addc();memmove(&result,R.d,sizeof(result));assert(result->len==4 && !memcmp(result->Sascii,"abcd",4));
		slotresult(slots.bytes,before,(uchar*)R.d-slots.bytes,sizeof(result));if(!alias)destroy(s);destroy(t);destroy(result);
	}
	/* Compare every valid/invalid slice with independent ASCII/rune contents. */
	for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++)
	for(wide=0;wide<3;wide++)for(start=-1;start<6;start++)for(end=-1;end<6;end++){
		s=wide==2 ? H : wide ? newrunes(4) : c2string("abcd",4);
		if(wide==1)for(i=0;i<4;i++)s->Srune[i]=0x100+i;
		memset(slots.bytes,0xa5,128);R.s=slots.bytes+16+sa;R.m=slots.bytes+48+ma;R.d=slots.bytes+80+da;
		memmove(R.s,&start,sizeof(start));memmove(R.m,&end,sizeof(end));memmove(R.d,&s,sizeof(s));memmove(before,slots.bytes,128);
		if(start<0 || end<start || end>(wide==2 ? 0 : 4)){
			EXPECT(slicec(),exBounds);assert(!memcmp(slots.bytes,before,128));destroy(s);
		}else{
			slicec();memmove(&result,R.d,sizeof(result));
			if(start==end)assert(result==H);
			else{
				assert(result->len==(wide ? -(end-start) : end-start));
				for(i=0;i<end-start;i++)assert((wide ? result->Srune[i] : result->Sascii[i])==(wide ? 0x100+start+i : 'a'+start+i));
			}
			slotresult(slots.bytes,before,80+da,sizeof(result));destroy(result);
		}
	}
	/* Split empty and shared owners at every pointer-slot alignment. */
	for(sa=0;sa<8;sa++)for(wide=0;wide<3;wide++)for(alias=0;alias<2;alias++)for(op=0;op<2;op++){
		s=wide==2 ? H : wide ? newrunes(2) : c2string("ab",2);
		if(wide==1){s->Srune[0]='a';s->Srune[1]='b';}
		old=s;if(alias && old!=H)D2H(old)->ref++;
		memset(slots.bytes,0xa5,128);R.d=slots.bytes+32+sa;memmove(R.d,&s,sizeof(s));memmove(before,slots.bytes,128);
		if(old!=H){
			failure=0;EXPECT(splitc((String**)R.d,op),exNomem);failure=-1;
			assert(!memcmp(before,slots.bytes,128) && D2H(old)->ref==(alias ? 2 : 1));
		}
		result=splitc((String**)R.d,op);memmove(&s,R.d,sizeof(s));assert(result==s);
		if(old==H)assert(result==H);
		else{
			assert(result->len==(wide || op ? -2 : 2));
			if(wide || op)assert(result->Srune[0]=='a' && result->Srune[1]=='b');
			else assert(!memcmp(result->Sascii,"ab",2));
		}
		slotresult(slots.bytes,before,32+sa,sizeof(result));
		if(alias && old!=H){assert(D2H(old)->ref==1);destroy(old);}destroy(result);
	}
	if(!getenv("STRING_SLOT_CASE") || strcmp(getenv("STRING_SLOT_CASE"),"vm"))checkreturns();
	/* Invalid representations and wide bounds must not allocate, release or publish. */
	for(i=0;i<4;i++){
		s=i==0 ? nil : newstring(0);if(i==1)s->len=INT_MIN;
		start=0;end=i==2 ? (WORD)-1 : i==3 ? (WORD)((UWORD)1<<(sizeof(WORD)*8-2)) : 0;
		R.s=&start;R.m=&end;R.d=&s;old=s;
		EXPECT(slicec(),i==0 ? exNilref : i==1 ? exHeap : exBounds);assert(s==old);
		if(s){s->len=0;destroy(s);}
	}
	EXPECT(string2c(nil),exNilref);EXPECT(stringdup(nil),exNilref);
	EXPECT(addstring(nil,H,0),exNilref);
finished:
	assert(live==0 && Tstring.ref==1 && Tarray.ref==1 && Tbyte.ref==1);
	printf("PASS string slots: %d alignment/alias/bounds/ownership checks WORD%zu\n",checks,sizeof(WORD)*8);
	return 0;
}
