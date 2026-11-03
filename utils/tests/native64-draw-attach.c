/* Model only one read of the native window-name file; every opened descriptor must close. */
int
libopen(char *name, int mode)
{
	USED(name);
	require(attachmode && mode==OREAD);
	if(filemode==1) return -1;
	fileopens++;
	return 91;
}


/* Refuse or return a bounded name without pretending to execute a filesystem. */
int
libread(int fd, void *buffer, int bytes)
{
	require(fd==91 && bytes==256);
	if(filemode==2) return 0;
	if(filemode==3) return -1;
	memmove(buffer,"window",6);
	return 6;
}


/* Observe descriptor lifetime independently of native image ownership. */
int
libclose(int fd)
{
	require(fd==91);
	filecloses++;
	return 0;
}


/* Require a fresh native header; the actual Screen/list retirement bodies remain linked. */
Image*
_allocwindow(Image *old, Screen *screen, Rectangle r, int refresh, ulong color)
{
	require(old==nil);
	return allocwindow(screen,r,refresh,color);
}


/* The refresh entry's service boundary is synchronous and borrowed; no device/thread is modeled. */
void
refreshslave(Display *display)
{
	require(nativealive(display,4));
	refreshcalls++;
}



/* Poison all actual Image fields before the production constructors initialize them. */
static void*
poisonedimage(size_t bytes)
{
	void *value;

	value=nativeallocate(bytes,1);
	if(value!=nil) memset(value,0xa5,bytes);
	return value;
}


/* Supply one bounded native image-info record; parsing remains the actual namedimage body. */
long
kchanio(void *channel, void *buffer, int bytes, int mode)
{
	char record[145];
	int i;

	USED(channel);
	require(mode==OREAD && bytes==145);
	memset(record,' ',144); record[144]=0;
	for(i=0;i<12;i++){
		char field[13];
		snprintf(field,sizeof(field),"%11d ",i<4 ? 0 : 10);
		memmove(record+i*12,field,12);
	}
	memmove(record+24,"r8g8b8a8    ",12);
	memmove(buffer,record,144);
	return 144;
}

ATTACH_CASES

/* Build real managed default owners; explicitly break the fixture's default-image cycles at teardown. */
static DDisplay*
attachdisplay(void)
{
	DDisplay *display;

	display=newdisplay();
	display->drawdisplay.image=newimage(display);
	display->drawdisplay.white=newimage(display);
	display->display->image=((DImage*)display->drawdisplay.image)->image;
	display->display->white=((DImage*)display->drawdisplay.white)->image;
	return display;
}


/* Release actual defaults after external graphs retire; collector traversal is outside this fixture. */
static void
attachclose(DDisplay *display)
{
	Draw_Image *base, *white;

	base=display->drawdisplay.image;
	white=display->drawdisplay.white;
	display->drawdisplay.image=H;
	display->drawdisplay.white=H;
	display->display->image=nil;
	display->display->white=nil;
	destroy(base);
	destroy(white);
	require(D2H(display)->ref==1 && display->dref->ref==1);
	destroy(display);
	baseline();
}


/* Check complete pairs, failure at every allocation, aliased Frame64 slots and existing old aliases. */
static void
attachcases(int failure, int alias, int file)
{
	DDisplay *display;
	F_Draw_Display_getwindow frame;
	F_Draw_Display_startrefresh refresh;
	void *pair[2], *oldpair[2], *slot;
	uchar storage[sizeof(pair)+2];
	Image *oldnative;
	int before, refs, startopens, startcloses;

	baseline();
	display=attachdisplay();
	memset(&frame,0,sizeof(frame));
	pair[0]=pair[1]=H;
	frame.d=&display->drawdisplay; frame.winname=H; frame.screen=H; frame.image=H;
	frame.ret=(void*)pair; frame.backup=Refbackup;
	filemode=0;
	Draw_Display_getwindow(&frame);
	require(pair[0]!=H && pair[1]!=H);
	oldpair[0]=pair[0]; oldpair[1]=pair[1];
	/* External aliases have their own actual references independent of the displaced result. */
	if(alias!=3){ D2H(oldpair[0])->ref++; D2H(oldpair[1])->ref++; }
	oldnative=((DImage*)oldpair[1])->image;
	frame.screen=oldpair[0]; frame.image=oldpair[1];
	if(alias==1){ slot=&frame.screen; }
	else { slot=storage+1; memmove(slot,pair,sizeof(pair)); }
	frame.ret=slot;
	before=nativecount; refs=D2H(display)->ref;
	startopens=fileopens; startcloses=filecloses;
	filemode=file;
	if(failure) failat=allocations+failure;
	if(alias==3){ tuplewatch=oldnative; tupleold=oldpair[1]; tupleslot=slot; }
	Draw_Display_getwindow(&frame);
	tuplewatch=nil; tupleold=nil; tupleslot=nil;
	failat=0;
	memmove(pair,slot,sizeof(pair));
	require(fileopens-startopens==(file!=1) && filecloses-startcloses==(file!=1));
	if(alias!=3) require(nativealive(oldnative,1) && oldnative->screen==((DScreen*)oldpair[0])->screen);
	if(failure){
		require(pair[0]==H && pair[1]==H && nativecount==before && D2H(display)->ref==refs);
	}else if(file){
		require(pair[0]==H && pair[1]==display->drawdisplay.image);
	}else{
		DScreen *screen=pair[0]; DImage *image=pair[1];
		require(screen!=oldpair[0] && image!=oldpair[1]);
		require(image->drawimage.screen==pair[0] && image->image->screen==screen->screen);
		require(screen->drawscreen.fill==display->drawdisplay.white);
		require(D2H(screen->drawscreen.image)->ref==1 && D2H(screen)->ref==2);
		require(D2H(display->drawdisplay.white)->ref==(alias==3 ? 2 : 3));
	}
	memset(&refresh,0,sizeof(refresh)); refresh.d=&display->drawdisplay;
	Draw_Display_startrefresh(&refresh);
	if(alias!=3){ destroy(oldpair[1]); destroy(oldpair[0]); }
	destroy(pair[1]); destroy(pair[0]);
	attachclose(display);
}


/* Observe old raw-graph retirement only after successful pair publication; refusals keep old slots intact. */
static void
rawcases(void)
{
	DDisplay *display;
	Image *window, *old;
	Screen *screen, *oldscreen;
	int step, before;

	display=attachdisplay(); window=nil; screen=nil; filemode=0;
	lockdisplay(display->display);
	require(gengetwindow(display->display,"name",&window,&screen,Refbackup)==1);
	for(step=1;step<=3;step++){
		old=window; oldscreen=screen; before=nativecount;
		failat=allocations+step;
		require(gengetwindow(display->display,"name",&window,&screen,Refbackup)<0);
		failat=0;
		require(window==old && screen==oldscreen && nativecount==before);
	}
	rawslot=(void**)&window; rawold=window;
	require(gengetwindow(display->display,"name",&window,&screen,Refbackup)==1);
	rawslot=nil; rawold=nil;
	filemode=2;
	require(gengetwindow(display->display,"name",&window,&screen,Refbackup)==1);
	require(window==display->display->image && screen==nil);
	unlockdisplay(display->display);
	require(fileopens==filecloses);
	attachclose(display);
}




/* Supply a real reusable callback identity without invoking a native refresh device. */
static void
existingcallback(Image *image, Rectangle rectangle, void *context)
{
	USED(image); USED(rectangle.min.x); USED(context);
	require(0);
}


/* Real native constructors must initialize callback/path state even when malloc returns dirty storage. */
static void
imageinitializers(void)
{
	DDisplay *display;
	Image *image;
	int failure, before;

	display=attachdisplay(); before=nativecount;
	image=freshallocimage(nil,display->display,Rect(0,0,10,10),RGBA32,0,0,0,0);
	require(image!=nil && image->reffn==nil && image->refptr==nil && image->fpath==nil);
	image->reffn=existingcallback; image->refptr=display;
	require(freshallocimage(image,display->display,Rect(1,1,20,20),RGBA32,0,0,0,0)==image);
	require(image->reffn==existingcallback && image->refptr==display);
	freeimage(image);
	for(failure=0;failure<2;failure++){
		if(failure) failat=allocations+1;
		image=freshnamedimage(display->display,"window"); failat=0;
		if(failure) require(image==nil);
		else{
			require(image!=nil && image->reffn==nil && image->refptr==nil && image->fpath==nil);
			freeimage(image);
		}
		require(nativecount==before);
	}
	attachclose(display);
}


/* Close genuine Heap/Type/native owners; malformed-control exits are independent active predicates. */
int
attachmain(void)
{
	int failure, alias, file;

	typesetup(); attachmode=1;
	for(alias=0;alias<2;alias++) for(failure=0;failure<=6;failure++) attachcases(failure,alias,0);
	for(file=1;file<=3;file++) attachcases(0,0,file);
	attachcases(0,3,0);
	rawcases();
	imageinitializers();
	freetype(TFont); freetype(TImage); freetype(TScreen); freetype(TDisplay);
	require(live==0 && nativecount==0 && worker.nerr==0 && gchalt==0);
	printf("PASS attach: %d predicates, 18 Frame64 pair scenarios, raw rollback/replacement, %d refresh service calls, all ledgers zero; no device/collector execution\n",checks,refreshcalls);
	return 0;
}
