/* Retain the production descriptor allocator and RX code publication owner. */
void *mallocz(ulong n,int clear) {void *p=malloc(n);if(p && clear)memset(p,0,n);return p;}
/* Compiler publication does not inject errors in this native matrix. */
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
int mutator,nprop;

/* Compare real C stores and native leaves with complete heap/operand images. */
int
main(void)
{
	Module module={0};Inst instruction={0},sentinel;
	union {uvlong align;uchar bytes[128];} slots;
	uchar before[128],expectedslots[128],image[512],expected[512];
	String *old,*owner;
	REG saved;
	WORD value,index,captured,indexed;
	Rune rune;
	char *exception;
	int wide,profile,vi,sa,ma,da,alias,pass,valid,size,offset,i;
	int length,capacity,allocations,round,flags;
	int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
	uvlong values[]={0,65,127,128,0x20ac,0xffffffffULL,0x100000041ULL,0x100000080ULL};
	unsigned long total=0;
	char *selection=getenv("STRING_STORE_PROFILE");

	instruction.op=IINSC;module.prog=&instruction;module.nprog=1;
	assert(compile(&module,1,nil) && module.jit && module.jit->entry[IINSC]);
	for(wide=0;wide<2;wide++)for(profile=0;profile<12;profile++)for(vi=0;vi<8;vi++)
	for(sa=0;sa<8;sa++)for(ma=0;ma<8;ma++)for(da=0;da<8;da++)for(alias=0;alias<2;alias++){
		if(selection && atoi(selection)!=profile)continue;
		old=wide ? newrunes(32) : newstring(128);capacity=old->max;
		if(vi&1){old->tmp=malloc(64);assert(old->tmp);memset(old->tmp,0x5a,64);}
		old->len=wide ? -8 : 8;owner=old;length=8;index=profile==1 ? 7 : 0;
		for(i=0;i<8;i++){if(wide)old->Srune[i]=0x400+i;else old->Sascii[i]='A'+i;}
		if(profile==2)index=8;
		if(profile==3)index=-1;
		if(profile==4)index=sizeof(WORD)==8 ? (WORD)((uvlong)1<<32) : -1;
		if(profile==5)old->max=7;
		if(profile==6)old->len=INT_MIN;
		if(profile==7)owner=H;
		if(profile==8)owner=nil;
		if(profile==9)D2H(old)->ref=0;
		if(profile==10)D2H(old)->ref=2;
		if(profile==11)D2H(old)->ref=(ulong)-1;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));memset(&R,0,sizeof(R));
		value=(WORD)values[vi];R.s=slots.bytes+8+sa;R.m=slots.bytes+40+ma;R.d=slots.bytes+80+da;
		memmove(R.s,&value,sizeof(value));memmove(R.m,&index,sizeof(index));memmove(R.d,&owner,sizeof(owner));
		if(alias && index>=0 && index<8)R.s=(uchar*)old->Sascii+index*(wide ? sizeof(Rune) : 1);
		/* Invalid indexes still use a backed source slot. */
		if(alias && (index<0 || index>=8))R.s=old->Sascii;
		memmove(&captured,R.s,sizeof(captured));memmove(&indexed,R.m,sizeof(indexed));rune=captured;
		valid=owner==old && D2H(old)->ref==1 && old->len!=INT_MIN && old->max>=length &&
			indexed>=0 && indexed<length && (wide || rune<Runeself);
		exception=profile==3 || profile==4 ? exBounds : profile==5 || profile==6 || profile==9 ? exHeap : profile==8 ? exNilref : nil;
		size=sizeof(Heap)+sizeof(String)+128;assert(size<sizeof(image));
		memmove(image,D2H(old),size);memmove(before,slots.bytes,sizeof(before));
		memmove(expected,image,size);memmove(expectedslots,before,sizeof(before));
		if(valid){offset=(uchar*)old->Sascii-(uchar*)D2H(old)+indexed*(wide ? sizeof(Rune) : 1);memmove(expected+offset,&rune,wide ? sizeof(Rune) : 1);}
		R.PC=&sentinel;saved=R;allocations=calls;round=(profile+vi+sa)%4;
		for(pass=0;pass<2;pass++){
			memmove(D2H(old),image,size);memmove(slots.bytes,before,sizeof(before));R=saved;raised=nil;
			assert(!fesetround(rounds[round]));feclearexcept(FE_ALL_EXCEPT);feraiseexcept(FE_INVALID|FE_INEXACT);flags=fetestexcept(FE_ALL_EXCEPT);
			if(!pass)assert(module.jit->entry[IINSC](&R)==valid);
			else if(valid || exception){
				if(setjmp(boundary)==0){insc();assert(!exception);}
				else assert(raised==exception);
			}
			assert(!memcmp(D2H(old),expected,size) && !memcmp(slots.bytes,expectedslots,sizeof(expectedslots)));
			assert(!memcmp(&R,&saved,sizeof(R)) && calls==allocations);
			if(old->tmp)for(i=0;i<64;i++)assert((uchar)old->tmp[i]==0x5a);
			assert(fegetround()==rounds[round] && fetestexcept(FE_ALL_EXCEPT)==flags);
		}
		D2H(old)->ref=1;old->max=capacity;old->len=wide ? -8 : 8;destroy(old);
		assert(!live);total++;
	}
	assert(!fesetround(FE_TONEAREST));feclearexcept(FE_ALL_EXCEPT);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS string store native: %lu C/JIT/owner/capacity/alias/FP cases WORD%zu\n",total,sizeof(WORD)*8);
	return 0;
}
