/* Additional borrowed-view and actual managed-owner operations; no collector or display server is modeled. */
static int operationcases; // Number of independently checked window, naming, writing and movement scenarios.


/* Adapt the void Frame64 entry to the real protected-error boundary. */
static int
invalidflush(void *argument)
{
	Draw_Image_flush(argument);
	return 1;
}


/* Check every eight-window nil mask against the actual native packet encoder in both directions. */
static void
windowoperations(void)
{
	DDisplay *dd;
	Draw_Image *base, *windows[8], *entry;
	DScreen *screen;
	F_Draw_Screen_top top;
	F_Draw_Screen_bottom bottom;
	F_Draw_Image_top single;
	F_Draw_Image_bottom last;
	F_Draw_Image_flush flush;
	uchar *storage, *data;
	Array *array;
	int i, kind, mask, length, count, p, f, before;

	dd=newdisplay();
	base=newimage(dd);
	screen=allocdrawscreen(base,base,0);
	require(screen != nil);
	destroy(base);
	for(i=0; i<8; i++){
		Image *native; // A newly allocated native window transfers to its managed wrapper on success.

		native=allocwindow(screen->screen,Rect(0,0,4,4),Refbackup,0);
		require(native != nil);
		windows[i]=mkdrawimage(native,&screen->drawscreen,(Draw_Display*)dd,nil);
		require(windows[i] != H);
	}
	storage=trackedallocate(sizeof(Array)+8*sizeof(entry)+3,0);
	require(storage != nil);
	array=(Array*)(storage+1);
	data=storage+sizeof(Array)+2;
	memmove((uchar*)array+offsetof(Array,data),&data,sizeof(data));
	length=8;
	memmove((uchar*)array+offsetof(Array,len),&length,sizeof(length));
	memset(&top,0,sizeof(top));
	memset(&bottom,0,sizeof(bottom));
	top.screen=bottom.screen=&screen->drawscreen;
	top.wins=bottom.wins=array;
	for(kind=0; kind<2; kind++) for(mask=0; mask<256; mask++){
		count=0;
		for(i=0; i<8; i++){
			entry=(mask&(1<<i)) ? windows[i] : H;
			memmove(data+i*sizeof(entry),&entry,sizeof(entry));
			if(entry != H)
				count++;
		}
		p=packets;
		f=flushes;
		if(kind==0)
			Draw_Screen_top(&top);
		else
			Draw_Screen_bottom(&bottom);
		require(held==0 && packets-p==(count!=0) && flushes-f==(count!=0));
		if(count){
			require(wire[0]=='t' && wire[1]==1-kind && (wire[2]|wire[3]<<8)==count);
			count=0;
			for(i=0; i<8; i++) if(mask&(1<<i)){
				require(BGLONG(wire+4+4*count)==((DImage*)windows[i])->image->id);
				count++;
			}
		}
		operationcases++;
	}
	for(i=0; i<8; i++){
		entry=H;
		memmove(data+i*sizeof(entry),&entry,sizeof(entry));
	}
	entry=nil;
	memmove(data,&entry,sizeof(entry));
	p=packets;
	f=flushes;
	Draw_Screen_top(&top);
	require(packets==p && flushes==f && held==0);
	entry=windows[0];
	memmove(data,&entry,sizeof(entry));
	failat=allocations+1;
	Draw_Screen_top(&top);
	failat=0;
	require(packets==p && flushes==f && held==0);
	length=2147483647;
	memmove((uchar*)array+offsetof(Array,len),&length,sizeof(length));
	Draw_Screen_top(&top);
	require(packets==p && flushes==f && held==0);
	top.wins=H;
	Draw_Screen_top(&top);
	require(packets==p && flushes==f && held==0);
	operationcases+=4;
	trackedfree(storage);
	memset(&single,0,sizeof(single));
	memset(&last,0,sizeof(last));
	single.win=last.win=windows[0];
	Draw_Image_top(&single);
	require(wire[0]=='t' && wire[1]==1 && (wire[2]|wire[3]<<8)==1);
	Draw_Image_bottom(&last);
	require(wire[0]=='t' && wire[1]==0 && (wire[2]|wire[3]<<8)==1);
	operationcases+=2;
	memset(&flush,0,sizeof(flush));
	flush.win=windows[0];
	for(kind=0; kind<3; kind++){
		flush.func=kind;
		before=flushes;
		Draw_Image_flush(&flush);
		require(((DImage*)windows[0])->flush==(kind!=0));
		require(flushes-before==(kind!=0) && held==0);
		operationcases++;
	}
	flush.func=9;
	before=flushes;
	require(!kprotect(invalidflush,&flush));
	require(strcmp(message,exInval)==0 && flushes==before && held==0);
	operationcases++;
	p=packets;
	topnwindows(nil,0);
	topnwindows(nil,1);
	bottomnwindows(nil,-1);
	{
		Image *invalid[1]={nil}; // A native nil entry must refuse before packet reservation.

		topnwindows(invalid,1);
	}
	require(packets==p);
	operationcases+=4;
	for(i=0; i<8; i++)
		destroy(windows[i]);
	destroy(screen);
	destroy(dd);
	baseline();
}


/* A self-aliased String has only its image owner; observation stops a missing retain before unsafe free. */
static void
nameoperations(int alias, int which)
{
	DDisplay *dd;
	Draw_Image *image;
	String *old, *incoming, *expected;
	F_Draw_Image_name frame;
	uchar output[16];
	WORD result;
	int i;

	dd=newdisplay();
	image=newimage(dd);
	old=H2D(String*,heapz(&Tstring));
	image->iname=old;
	incoming=which==2 ? H2D(String*,heapz(&Tstring)) : which==4 ? H : old;
	expected=which==1 || which==4 ? H : incoming;
	if(which==3)
		expected=old;
	memset(&frame,0,sizeof(frame));
	memset(output,0xa5,sizeof(output));
	frame.src=image;
	frame.name=incoming;
	frame.in=which==1 ? 0 : 1;
	frame.ret=alias ? &frame.in : (WORD*)(output+1);
	expectedin=frame.in;
	operationfail=which==3;
	if(which==0)
		retainwatch=old;
	Draw_Image_name(&frame);
	retainwatch=nil;
	memmove(&result,frame.ret,sizeof(result));
	operationfail=0;
	require(result==(which!=3) && image->iname==expected && held==0);
	if(expected!=H)
		require(D2H(expected)->ref==(which==2 ? 2 : 1));
	if(which==2)
		destroy(incoming);
	if(!alias)
		for(i=0; i<sizeof(output); i++)
			if(i<1 || i>=1+sizeof(result))
				require(output[i]==0xa5);
	destroy(image);
	destroy(dd);
	baseline();
	operationcases++;
}


/* Writing may return into the borrowed FD argument cell; all source owners stay live through native I/O. */
static void
writeoperations(int alias, int which)
{
	DDisplay *dd, *other;
	Draw_Image *image;
	Sys_FD *fd;
	Type *type;
	F_Draw_Display_writeimage frame;
	uchar output[16];
	WORD result;
	int before, i;

	dd=newdisplay();
	image=newimage(dd);
	type=dtype(freeheap,sizeof(Sys_FD),nil,0);
	require(type != nil);
	fd=H2D(Sys_FD*,heapz(type));
	fd->fd=73;
	other=which==3 ? newdisplay() : nil;
	memset(&frame,0,sizeof(frame));
	memset(output,0xa5,sizeof(output));
	frame.d=(Draw_Display*)(other!=nil ? other : dd);
	frame.i=image;
	frame.fd=which==2 ? H : fd;
	frame.ret=alias ? (WORD*)&frame.fd : (WORD*)(output+1);
	before=writes;
	operationfail=which==1;
	Draw_Display_writeimage(&frame);
	operationfail=0;
	memmove(&result,frame.ret,sizeof(result));
	require(result==(which==0 ? 19 : -1) && writes-before==(which<2) && held==0);
	require(D2H(fd)->ref==1 && type->ref==2);
	if(!alias)
		for(i=0; i<sizeof(output); i++)
			if(i<1 || i>=1+sizeof(result))
				require(output[i]==0xa5);
	if(other!=nil)
		destroy(other);
	destroy(fd);
	freetype(type);
	destroy(image);
	destroy(dd);
	baseline();
	operationcases++;
}


/* Refused wire stages leave geometry unchanged; a legal translation can require a delta wider than int32. */
static void
originoperations(int alias, int which)
{
	DDisplay *dd;
	Draw_Image *source;
	Image *native;
	Rectangle old, clip;
	Point log;
	F_Draw_Image_origin frame;
	uchar output[16];
	WORD result;
	int i, p, f, success;

	dd=newdisplay();
	source=newimage(dd);
	native=((DImage*)source)->image;
	old=Rect(2,3,9,11);
	clip=Rect(0,0,10,12);
	log=Pt(-20,70);
	if(which==4)
		log=Pt(2147483647,0);
	if(which==5){
		old=Rect(-2147483647-1,-2,-2147483647+3,2);
		clip=old;
		log=Pt(2147483647-4,70);
	}
	native->r=old;
	native->clipr=clip;
	source->r=DRECT(old);
	source->clipr=DRECT(clip);
	memset(&frame,0,sizeof(frame));
	memset(output,0xa5,sizeof(output));
	frame.win=source;
	frame.log.x=log.x;
	frame.log.y=log.y;
	frame.scr.x=-5;
	frame.scr.y=8;
	frame.ret=alias ? &frame.log.x : (WORD*)(output+1);
	p=packets;
	f=flushes;
	flushfail=which==1;
	wirefail=which==2;
	flushfailat=which==3 ? flushes+2 : 0;
	Draw_Image_origin(&frame);
	flushfail=wirefail=flushfailat=0;
	memmove(&result,frame.ret,sizeof(result));
	success=which==0 || which==5;
	require(result==(success ? 1 : -1) && held==0);
	if(success){
		require(native->r.min.x==log.x && native->r.min.y==log.y);
		require((vlong)native->r.max.x-native->r.min.x==(vlong)old.max.x-old.min.x);
		require((vlong)native->clipr.min.x-native->r.min.x==(vlong)clip.min.x-old.min.x);
		require(memcmp(&source->r,&native->r,sizeof(native->r))==0);
		require(memcmp(&source->clipr,&native->clipr,sizeof(native->clipr))==0);
		require(wire[0]=='o' && BGLONG(wire+5)==(uint)log.x && BGLONG(wire+9)==(uint)log.y);
		require(BGLONG(wire+13)==(uint)-5 && BGLONG(wire+17)==8);
	}else{
		require(memcmp(&native->r,&old,sizeof(old))==0 && memcmp(&native->clipr,&clip,sizeof(clip))==0);
		require(memcmp(&source->r,&old,sizeof(old))==0 && memcmp(&source->clipr,&clip,sizeof(clip))==0);
		if(which==1 || which==4)
			require(packets==p);
		if(which==4)
			require(flushes==f);
	}
	if(!alias)
		for(i=0; i<sizeof(output); i++)
			if(i<1 || i>=1+sizeof(result))
				require(output[i]==0xa5);
	destroy(source);
	destroy(dd);
	baseline();
	operationcases++;
}
