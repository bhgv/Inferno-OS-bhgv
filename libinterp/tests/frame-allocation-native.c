typedef struct FrameImage FrameImage;

struct FrameImage {
	uchar bytes[2048]; // Decoded cells, unaligned frames and surrounding canaries.
	Type descriptor; // Native-aligned descriptor including several map bytes.
	Type *types[1]; // Current module Type table.
	Module owner; // Live owner of the Type table and named export sentinel.
	Modlink link; // Imported frame entry and owning module identity.
	Link exports[2]; // Named-export path retained in C.
};


/* Keep byte-image and callback predicates active with NDEBUG. */
static void
allocationcheck(int condition)
{
	if(!condition)abort();
}


/* Build independent aligned metadata and decoded cells at arbitrary byte offsets. */
static void
allocationimage(FrameImage *image,int imported,int sa,int da,int fa,int alias,int mode)
{
	WORD index;
	Frame *frame;
	Type *type;
	Modlink *link;

	memset(image,0xa5,sizeof(*image));memset(&R,0xa5,sizeof(R));
	memset(&image->descriptor,0,sizeof(image->descriptor));
	memset(&image->owner,0,sizeof(image->owner));
	memset(&image->link,0,sizeof(image->link));
	memset(image->exports,0,sizeof(image->exports));
	type=&image->descriptor;link=&image->link;
	type->ref=1;type->size=sizeof(Frame)+32;
	if(mode==1){type->np=1;type->map[0]=0x80>>(sizeof(Frame)/sizeof(void*));}
	image->types[0]=type;image->owner.type=image->types;image->owner.ntype=1;
	image->owner.ext=image->exports;image->link.m=&image->owner;image->link.nlinks=1;
	image->link.links[0].frame=type;
	image->exports[0].name="entry";image->exports[0].frame=type;
	R.M=link;R.s=image->bytes+64+sa;R.m=image->bytes+128+sa;R.d=image->bytes+192+da;
	R.SP=image->bytes+512+fa;R.TS=image->bytes+1536;
	R.PC=(Inst*)image->bytes;R.IC=29;
	if(mode==2)R.TS=R.SP+type->size;
	if(alias==5){
		type->size=sizeof(Frame);type->np=0;
		R.SP=(uchar*)&R+O(REG,d)-O(Frame,t);R.TS=(uchar*)&R+sizeof(R);
	}
	frame=(Frame*)R.SP;
	if(alias==1)R.d=(uchar*)frame+O(Frame,t);
	if(alias==2)R.s=(uchar*)frame+O(Frame,mr);
	if(alias==3)R.d=(uchar*)frame+sizeof(Frame);
	if(alias==4)R.d=&R.d;
	index=imported && mode==3 ? -1 : 0;
	if(imported){memmove(R.s,&link,sizeof(link));memmove(R.m,&index,sizeof(index));}
	else memmove(R.s,&index,sizeof(index));
}


/* Apply an independent oracle store to either the stack image or the REG image. */
static void
allocationstore(FrameImage *expected,REG *wanted,FrameImage *live,void *destination,void *value,int bytes)
{
	uintptr address,base;

	address=(uintptr)destination;base=(uintptr)live;
	if(address>=base && address-base<=sizeof(*live)-bytes)
		memmove((uchar*)expected+(address-base),value,bytes);
	else{
		base=(uintptr)&R;
		allocationcheck(address>=base && address-base<=sizeof(R)-bytes);
		memmove((uchar*)wanted+(address-base),value,bytes);
	}
}


/* Compare real C, guarded RX plus C and complete cjitexec routes with full byte images. */
static unsigned long
allocationpositive(Jit *jit)
{
	FrameImage image,before,expected;
	REG saved,wanted;
	Frame *frame;
	Type *type;
	Modlink *empty;
	void *nilvalue;
	unsigned long checks=0;
	int imported,sa,da,fa,alias,mode,route,op,growth;

	for(imported=0;imported<2;imported++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)
	for(fa=0;fa<8;fa++)for(alias=0;alias<6;alias++)for(mode=0;mode<(imported ? 4 : 3);mode++){
		if(alias==5 && (mode==1 || mode==2 || fa!=0))continue;
		if(getenv("FRAME_ALLOCATION_ALIAS") && alias!=atoi(getenv("FRAME_ALLOCATION_ALIAS")))continue;
		if(getenv("FRAME_ALLOCATION_FRAME_SHIFT") && fa!=atoi(getenv("FRAME_ALLOCATION_FRAME_SHIFT")))continue;
		allocationimage(&image,imported,sa,da,fa,alias,mode);
		op=imported ? IMFRAME : IFRAME;type=&image.descriptor;frame=(Frame*)R.SP;
		growth=mode==2;
		memmove(&before,&image,sizeof(image));memmove(&expected,&image,sizeof(image));
		memmove(&saved,&R,sizeof(R));memmove(&wanted,&R,sizeof(R));
		if(growth)wanted.s=frame;
		else{
			wanted.SP=(uchar*)((uintptr)saved.SP+type->size);
			empty=nil;
			allocationstore(&expected,&wanted,&image,(uchar*)frame+O(Frame,t),&type,sizeof(type));
			allocationstore(&expected,&wanted,&image,(uchar*)frame+O(Frame,mr),&empty,sizeof(empty));
			if(mode==1){nilvalue=H;allocationstore(&expected,&wanted,&image,(uchar*)frame+sizeof(Frame),&nilvalue,sizeof(nilvalue));}
		}
		allocationstore(&expected,&wanted,&image,saved.d,&frame,sizeof(frame));
		for(route=0;route<3;route++){
			memmove(&image,&before,sizeof(image));memmove(&R,&saved,sizeof(R));initialized=extensions=0;raised=nil;
			if(route==0)optab[op]();
			else if(route==1){if(!jit->entry[op](&R))optab[op]();}
			else allocationcheck(jit->dispatch[op](&R)==1);
			if(getenv("FRAME_ALLOCATION_TRACE"))fprintf(stderr,"positive %d %d %d %d %d %d %d\n",imported,sa,da,fa,alias,mode,route);
			allocationcheck(!memcmp(&image,&expected,sizeof(image)) && !memcmp(&R,&wanted,sizeof(R)));
			allocationcheck(extensions==growth && initialized==(mode==1) && raised==nil);
			checks++;
		}
	}
	return checks;
}


/* Errors must precede allocation, map initialization, cursor changes and slot writes. */
static unsigned long
allocationinvalid(Jit *jit)
{
	FrameImage image,before;
	REG saved;
	Type *type;
	void *bad;
	unsigned long checks=0;
	int imported,kind,offset,route,failed,op,size,pointers;

	if(getenv("FRAME_ALLOCATION_NO_ERRORS"))return 0;
	for(imported=0;imported<2;imported++)for(kind=0;kind<10;kind++)for(offset=0;offset<64;offset++){
		if(getenv("FRAME_ALLOCATION_BAD") && kind!=atoi(getenv("FRAME_ALLOCATION_BAD")))continue;
		allocationimage(&image,imported,0,0,0,0,0);op=imported ? IMFRAME : IFRAME;
		bad=offset==0 ? nil : (void*)((uintptr)-offset);
		if(kind==0){if(offset>(imported ? sizeof(void*) : sizeof(WORD)))continue;R.s=bad;}
		else if(kind==1 || kind==2){if(offset>sizeof(void*))continue;R.d=bad;if(kind==2)R.TS=R.SP+image.descriptor.size;}
		else if(kind==3){if(!imported || offset>sizeof(WORD))continue;R.m=bad;}
		else if(kind==4){
			if(offset==0 || offset>=sizeof(void*))continue;
			type=(Type*)((uchar*)&image.descriptor+offset);size=image.descriptor.size;pointers=0;
			memmove((uchar*)type+O(Type,size),&size,sizeof(size));memmove((uchar*)type+O(Type,np),&pointers,sizeof(pointers));
			image.types[0]=type;image.link.links[0].frame=type;
		}else if(kind==5){if(offset!=0)continue;image.descriptor.np=2;}
		else if(kind==6){if(offset!=0)continue;image.descriptor.size=sizeof(Frame);image.descriptor.np=1;image.descriptor.map[0]=1;}
		else if(kind==7){if(offset!=0)continue;image.descriptor.size=sizeof(Frame)+1;image.descriptor.np=1;image.descriptor.map[0]=0x80>>(sizeof(Frame)/sizeof(void*));}
		else if(kind==8){if(offset!=0)continue;R.SP=H;}
		else{if(offset>sizeof(void*))continue;image.descriptor.np=1;image.descriptor.map[0]=0x80>>(sizeof(Frame)/sizeof(void*));R.d=bad;}
		memmove(&before,&image,sizeof(image));memmove(&saved,&R,sizeof(R));
		initialized=extensions=0;
		allocationcheck(jit->entry[op](&R)==0 && !memcmp(&saved,&R,sizeof(R)) && !memcmp(&before,&image,sizeof(image)));
		for(route=0;route<2;route++){
			memmove(&image,&before,sizeof(image));memmove(&R,&saved,sizeof(R));initialized=extensions=0;raised=nil;failed=0;
			if(setjmp(boundary)==0){if(route==0)optab[op]();else jit->dispatch[op](&R);}else failed=1;
			if(getenv("FRAME_ALLOCATION_TRACE"))fprintf(stderr,"negative %d %d %d %d\n",imported,kind,offset,route);
			allocationcheck(failed && raised!=nil && initialized==0 && extensions==0);
			allocationcheck(!memcmp(&saved,&R,sizeof(R)) && !memcmp(&before,&image,sizeof(image)));
			checks++;
		}
	}
	return checks;
}


/* A mapped control-link cell must decline natively and fail in C before any publication. */
static unsigned long
allocationheaders(Jit *jit)
{
	FrameImage image,before;
	REG saved;
	unsigned long checks=0;
	int imported,sa,da,fa,slot,route,op,failed;

	for(imported=0;imported<2;imported++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++)
	for(fa=0;fa<8;fa++)for(slot=0;slot<sizeof(Frame)/sizeof(void*);slot++){
		allocationimage(&image,imported,sa,da,fa,0,0);
		op=imported ? IMFRAME : IFRAME;
		image.descriptor.np=1;
		image.descriptor.map[0]=0x80>>slot;
		memmove(&before,&image,sizeof(image));memmove(&saved,&R,sizeof(R));
		allocationcheck(jit->entry[op](&R)==0 && !memcmp(&saved,&R,sizeof(R)) && !memcmp(&before,&image,sizeof(image)));
		for(route=0;route<2;route++){
			memmove(&image,&before,sizeof(image));memmove(&R,&saved,sizeof(R));
			initialized=extensions=0;raised=nil;failed=0;
			if(setjmp(boundary)==0){if(route==0)optab[op]();else jit->dispatch[op](&R);}else failed=1;
			allocationcheck(failed && raised==exHeap && initialized==0 && extensions==0);
			allocationcheck(!memcmp(&saved,&R,sizeof(R)) && !memcmp(&before,&image,sizeof(image)));
			checks++;
		}
	}
	return checks;
}


/* Compare synthetic unsigned extents without dereferencing their numeric stack addresses. */
static unsigned long
allocationextents(void)
{
	uintptr starts[]={1,0x1000,0x80000000ULL,1ULL<<63,(uintptr)-0x1000};
	uintptr gaps[6],limit;
	int sizes[]={sizeof(Frame),64,0x7fffffff};
	Type type={0};
	REG saved;
	unsigned long checks=0;
	int i,j,k;

	for(i=0;i<sizeof(starts)/sizeof(starts[0]);i++)for(j=0;j<3;j++){
		type.size=sizes[j];
		gaps[0]=0;gaps[1]=type.size-1;gaps[2]=type.size;gaps[3]=(uintptr)type.size+1;
		gaps[4]=0x1000;gaps[5]=1ULL<<63;
		for(k=0;k<6;k++){
			if(gaps[k]>(uintptr)-1-starts[i])continue;
			limit=starts[i]+gaps[k];
			if(limit==(uintptr)-1)continue;
			R.SP=(uchar*)starts[i];R.TS=(uchar*)limit;
			memmove(&saved,&R,sizeof(R));
			allocationcheck(framefits(&type)==((uintptr)type.size<gaps[k]) && !memcmp(&saved,&R,sizeof(R)));
			checks++;
		}
	}
	return checks;
}


/* Exercise both published entries while C owns mapped initialization and growth. */
int
main(void)
{
	Module module={0};
	Inst program[2]={{0}};
	unsigned long positive=0,negative=0,extents=0,headers=0;
	char *section=getenv("FRAME_ALLOCATION_SECTION");

	program[0].op=IFRAME;program[1].op=IMFRAME;module.prog=program;module.nprog=2;
	optab[IFRAME]=frame;optab[IMFRAME]=mframe;
	allocationcheck(compile(&module,2,nil) && module.jit);
	if(section==nil || !strcmp(section,"positive"))positive=allocationpositive(module.jit);
	if(section==nil || !strcmp(section,"invalid"))negative=allocationinvalid(module.jit);
	if(section==nil || !strcmp(section,"extent"))extents=allocationextents();
	if(section==nil || !strcmp(section,"headers"))headers=allocationheaders(module.jit);
	module.jit->freecode(module.jit->code,module.jit->size);free(module.jit);
	printf("PASS frame allocation C/RX WORD%zu: %lu image routes, %lu rejected routes, %lu integer extents, %lu header-map rejections\n",sizeof(WORD)*8,positive,negative,extents,headers);
	return 0;
}
