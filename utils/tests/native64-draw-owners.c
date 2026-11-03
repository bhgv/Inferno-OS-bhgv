#include <stdarg.h>
char exType[]="type"; // Existing typed-resource refusal identity.
void freedrawfont(Heap*,int); // Actual managed destructor bodies linked below.
void freedrawimage(Heap*,int); // Actual managed image retirement.
void freedrawscreen(Heap*,int); // Actual managed screen retirement.
void freedrawdisplay(Heap*,int); // Actual managed display retirement.
void actualdestroy(void*); // Actual heap implementation observed by the test wrapper.
DRAW_DECLARATIONS
Image *lookupimage(Draw_Image*); // Actual ordinary lookup body, deliberately absent from destruction.

static void nativeretire(void*,int); // Exact native resource owner retirement.
static void *nativeallocate(size_t,int); // Bounded native allocation service.
static void *dropwatch; // Owner whose retirement budget is independently known.
static int dropbudget; // Remaining permitted destroy calls for that watched owner.
static void *aliasedowner, *aliasedslot; // Observe publication before the old aliased owner disappears.
static int aliasactive; // Only the selected real Frame64 invocation is observed.


/* Preserve the actual managed destroy path after checking a test-owned retirement budget. */
void
destroy(void *value)
{
	if(value==dropwatch){ require(dropbudget>0); dropbudget--; }
	actualdestroy(value);
}

HEAP_IMPLEMENTATION


typedef struct NativeOwner NativeOwner;

struct NativeOwner {
	void *pointer; // Exact resource allocation identity, never fabricated Heap metadata.
	int kind; // Image1, Screen2, Font3, Display4 or DRef5 native storage.
};

static NativeOwner natives[128]; // Bounded native allocation ledger.
static int nativecount, nativecalls, freedimages, freedscreens, closes, held; // Exact owning service observations.
static int wirefail, flushfail, factoryfail, packets, flushes, reconfigures; // Bounded transport refusal and effect counters.
static Image front; // Borrowed display channel description, not an owned graphics device.
static DImage *retiringimage; // Observe private detachment before native free can block.
static uchar wire[64]; // Actual small native resource protocol packet.


/* The result must already differ from the old owner before actual pool retirement proceeds. */
static void
resourcepoolobserve(void *heap)
{
	void *current;

	if(aliasactive && heap==D2H(aliasedowner)){
		memmove(&current,aliasedslot,sizeof(current));
		require(current!=aliasedowner);
	}
}


/* Expose native allocation identity without reading possibly retired resource storage. */
static int
nativealive(void *pointer,int kind)
{
	unsigned int i;

	for(i=0;i<nelem(natives);i++) if(natives[i].pointer==pointer && natives[i].kind==kind) return 1;
	return 0;
}


/* Allocate actual guarded storage, independently tagged from managed pool allocations. */
static void*
nativeallocate(size_t bytes,int kind)
{
	void *pointer;
	unsigned int i;

	nativecalls++;
	pointer=trackedallocate(bytes,1);
	if(pointer==nil) return nil;
	for(i=0;i<nelem(natives) && natives[i].pointer!=nil;i++) ;
	require(i<nelem(natives));
	natives[i].pointer=pointer; natives[i].kind=kind; nativecount++;
	return pointer;
}


/* Retire each native identity once while checking child lifetime and private detachment before free. */
static void
nativeretire(void *pointer,int kind)
{
	unsigned int i;
	Screen *screen;
	Image *image, *cursor;

	if(pointer==nil) return;
	for(i=0;i<nelem(natives) && (natives[i].pointer!=pointer || natives[i].kind!=kind);i++) ;
	require(i<nelem(natives));
	if(kind==1){
		image=pointer;
		if(retiringimage!=nil && retiringimage->drawimage.iname!=H)
			require(retiringimage->image==nil);
		for(cursor=image->display->windows;cursor!=nil;cursor=cursor->next) require(cursor!=image);
		freedimages++;
	}
	if(kind==2){
		screen=pointer;
		require(screen->image==nil || nativealive(screen->image,1));
		require(screen->fill==nil || nativealive(screen->fill,1));
		freedscreens++;
	}
	if(kind==4){
		Display *d=pointer;
		require(held==0 && d->windows==nil && d->image==nil && d->black==nil && d->white==nil && d->opaque==nil && d->transparent==nil);
		closes++;
	}
	if(kind==5) require(((DRef*)pointer)->ref==0);
	natives[i].pointer=nil; nativecount--;
	trackedfree(pointer);
}


/* Copy the actual VM diagnostic so constructor rollback cannot erase allocation failure. */
void
kgerrstr(char *text,unsigned int bytes)
{
	if(bytes!=0) snprintf(text,bytes,"%s",message);
}


/* Model display exclusion without replacing managed ownership or creating native threads. */
int
lockdisplay(Display *d)
{
	require(nativealive(d,4));
	if(held) return 0;
	held=1;
	return 1;
}


/* Native unlock is non-releasing here, matching the audited current libqunlock path. */
void
unlockdisplay(Display *d)
{
	require(nativealive(d,4) && held==1);
	held=0;
}


/* Inspect actual small resource commands or refuse before any bytes are accepted. */
uchar*
bufimage(Display *d,int bytes)
{
	require(nativealive(d,4) && bytes>0 && bytes<=sizeof(wire));
	packets++;
	if(wirefail){kwerrstr("resource buffer refused");return nil;}
	memset(wire,0xa5,sizeof(wire));
	return wire;
}


/* Count nonthrowing transport failure; no real draw device or refresh callback is modeled. */
int
flushimage(Display *d,int visible)
{
	USED(visible);
	require(nativealive(d,4));
	flushes++;
	if(flushfail){kwerrstr("resource flush refused");return -1;}
	return 1;
}


/* Ordinary image lookup may update clip state; resource destruction must not invoke that path. */
void
replclipr(Image *image,int repl,Rectangle clip)
{
	require(gchalt==0);
	reconfigures++;
	image->repl=repl; image->clipr=clip;
}


/* The native image allocation service owns only its new Image on success. */
Image*
allocimage(Display *d,Rectangle r,ulong channel,int repl,ulong color)
{
	Image *image;

	USED(color);
	require(nativealive(d,4));
	if(factoryfail){kwerrstr("image creation refused");return nil;}
	image=nativeallocate(sizeof(*image),1);
	if(image==nil) return nil;
	image->display=d; image->r=r; image->clipr=r; image->id=nativecalls+100;
	image->chan=channel; image->repl=repl; image->depth=32;
	return image;
}


/* Colour mixing supplies a newly owned native image; its actual renderer remains outside this gate. */
Image*
allocimagemix(Display *d,ulong a,ulong b)
{
	USED(a); USED(b);
	return allocimage(d,Rect(0,0,1,1),RGBA32,1,0);
}


/* Create a native window with a real list edge so the production unlink/failure path is exercised. */
Image*
allocwindow(Screen *screen,Rectangle r,int backing,ulong color)
{
	Image *image;

	USED(backing);
	require(nativealive(screen,2));
	image=allocimage(screen->display,r,RGBA32,0,color);
	if(image!=nil){ image->screen=screen; image->next=screen->display->windows; screen->display->windows=image; }
	return image;
}


/* Keep font-cache implementation separate while checking native wrapper ownership exactly once. */
void
font_close(Font *font)
{
	require(font!=nil && nativealive(font,3));
	nativeretire(font,3);
}


/* No native Subfont is created by this wrapper-lifetime fixture. */
void
subfont_close(Subfont *font)
{
	USED(font); require(0);
}


/* Native subfont-cache teardown is an explicit service boundary, not managed GC evidence. */
void
freeallsubfonts(Display *d)
{
	require(nativealive(d,4));
}


/* Consume the final native display only after all its leases and list edges have retired. */
void
closedisplay(Display *d)
{
	nativeretire(d,4);
}

NATIVE_IMPLEMENTATION
DRAW_IMPLEMENTATION


/* Build real managed descriptors from the production private staging function, without registering Draw. */
static void
typesetup(void)
{
	Type *types[4]={nil};
	uchar a[sizeof(fontmap)],b[sizeof(imagemap)],c[sizeof(screenmap)],d[sizeof(displaymap)];

	require(drawmaketypes(types,a,b,c,d));
	TFont=types[0]; TImage=types[1]; TScreen=types[2]; TDisplay=types[3];
	require(TImage->size==sizeof(DImage) && TScreen->size==sizeof(DScreen));
	Tstring.size=sizeof(String); Tstring.free=freestring; Tstring.np=0;
}


/* Allocate a real managed Display wrapper plus its independently owned native display lease. */
static DDisplay*
newdisplay(void)
{
	DDisplay *dd;
	DRef *lease;
	Display *display;

	dd=H2D(DDisplay*,heapz(TDisplay));
	display=nativeallocate(sizeof(*display),4);
	lease=nativeallocate(sizeof(*lease),5);
	require(display!=nil && lease!=nil);
	lease->ref=1; lease->display=display; display->limbo=lease;
	memset(&front,0,sizeof(front)); front.chan=RGBA32; front.display=display; display->image=&front;
	dd->display=display; dd->dref=lease;
	return dd;
}


/* Use the actual private image constructor with one externally owned display root. */
static Draw_Image*
newimage(DDisplay *dd)
{
	Draw_Rect rectangle={{0,0},{4,4}};
	Draw_Image *image;
	int locked;

	locked=lockdisplay(dd->display);
	image=allocdrawimage(dd,rectangle,RGBA32,nil,0,0);
	if(locked) unlockdisplay(dd->display);
	require(image!=H);
	return image;
}


/* Every scenario must restore real Type counts and both managed/native ledgers. */
static void
baseline(void)
{
	require(nativecount==0 && live==4 && held==0 && worker.nerr==0 && gchalt==0);
	require(TFont->ref==1 && TImage->ref==1 && TScreen->ref==1 && TDisplay->ref==1 && Tstring.ref==1);
	retiringimage=nil; dropwatch=nil; aliasactive=0; failat=wirefail=flushfail=factoryfail=0;
}


/* Native failure must not leak a local Screen header or manufacture success after retry exhaustion. */
static void
nativefailures(void)
{
	DDisplay *dd;
	Image *a,*b,*window;
	Screen *screen;
	int count,i;

	dd=newdisplay(); a=allocimage(dd->display,Rect(0,0,1,1),RGBA32,0,0); b=allocimage(dd->display,Rect(0,0,1,1),RGBA32,0,0);
	for(i=0;i<3;i++){
		screen=allocscreen(a,b,0); require(screen!=nil);
		window=allocwindow(screen,Rect(0,0,1,1),Refbackup,0); require(window!=nil && dd->display->windows==window);
		count=nativecount; wirefail=i==1; flushfail=i==2;
		require(freeimage(window)==(i?-1:0) && nativecount==count-1 && dd->display->windows==nil);
		count=nativecount;
		require(freescreen(screen)==(i? -1:1) && nativecount==count-1);
		wirefail=flushfail=0;
	}
	failat=allocations+1; count=nativecount;
	require(allocscreen(a,b,0)==nil && nativecount==count); failat=0;
	failat=allocations+1;
	require(publicscreen(dd->display,42,RGBA32)==nil && nativecount==count); failat=0;
	flushfail=1; count=nativecount;
	require(allocscreen(a,b,0)==nil && nativecount==count);
	flushfail=0; screenid=2147483647;
	screen=allocscreen(a,b,0); require(screen!=nil && screen->id==1); freescreen(screen);
	freeimage(a); freeimage(b); destroy(dd); baseline();
}


/* Managed wrappers release mapped edges exactly once even when their private native pointer is absent. */
static void
retirement(void)
{
	DDisplay *dd;
	Draw_Image *image;
	DScreen *screen;
	DFont *font;
	String *name;
	int swept;
	Heap *owner;

	for(swept=0;swept<2;swept++){
		dd=newdisplay(); image=newimage(dd); name=H2D(String*,heapz(&Tstring));
		image->iname=name;
		D2H(name)->ref++;
		retiringimage=(DImage*)image;
		image->clipr.min.x=1;
		dropwatch=name; dropbudget=swept?0:1;
		owner=D2H(image);
		if(swept){
			owner->t->free(owner,1);
			require(D2H(name)->ref==2 && D2H(dd)->ref==2);
			dropwatch=nil; destroy(name); destroy(dd);
			freetype(owner->t); poolfree(heapmem,owner);
		}else destroy(image);
		dropwatch=nil; retiringimage=nil;
		require(D2H(name)->ref==1 && D2H(dd)->ref==1 && reconfigures==0);
		destroy(name); destroy(dd); baseline();
	}
	dd=newdisplay(); image=newimage(dd);
	screen=H2D(DScreen*,heapz(TScreen));
	screen->screen=nil; screen->dref=nil; screen->drawscreen.image=image; D2H(image)->ref++;
	dropwatch=image; dropbudget=1;
	destroy(screen); require(dropbudget==0 && D2H(image)->ref==1);
	dropwatch=nil; destroy(image); destroy(dd); baseline();
	dd=newdisplay(); name=H2D(String*,heapz(&Tstring));
	font=H2D(DFont*,heapz(TFont));
	font->drawfont.name=name; D2H(name)->ref++;
	font->drawfont.display=(Draw_Display*)dd; D2H(dd)->ref++;
	font->font=nil; font->dref=nil;
	destroy(font); require(D2H(name)->ref==1 && D2H(dd)->ref==1);
	destroy(name); destroy(dd); baseline();
	dd=newdisplay(); name=H2D(String*,heapz(&Tstring));
	font=H2D(DFont*,heapz(TFont)); font->font=nativeallocate(sizeof(Font),3);
	font->dref=dd->dref; font->dref->ref++;
	font->drawfont.name=name; D2H(name)->ref++;
	font->drawfont.display=(Draw_Display*)dd; D2H(dd)->ref++;
	destroy(font); require(D2H(name)->ref==1 && D2H(dd)->ref==1 && dd->dref->ref==1);
	destroy(name); destroy(dd); baseline();
	dd=newdisplay(); image=H2D(Draw_Image*,heapz(TImage));
	((DImage*)image)->image=nil; ((DImage*)image)->dref=nil;
	dd->drawdisplay.black=image; D2H(image)->ref++;
	dd->drawdisplay.white=image; D2H(image)->ref++;
	destroy(dd); require(D2H(image)->ref==1);
	destroy(image); baseline();
}


/* Inject the actual Heap allocation error after a native allocation and check complete rollback. */
static void
rollback(void)
{
	DDisplay *dd;
	Draw_Image *image,*fill;
	Draw_Rect rectangle={{0,0},{1,1}};
	DScreen *screen;
	int count,refs,leases;

	dd=newdisplay(); count=nativecount; refs=D2H(dd)->ref; leases=dd->dref->ref;
	failat=allocations+2;
	require(allocdrawimage(dd,rectangle,RGBA32,nil,0,0)==H);
	failat=0;
	require(nativecount==count && D2H(dd)->ref==refs && dd->dref->ref==leases && strcmp(message,exHeap)==0);
	image=newimage(dd); fill=newimage(dd); count=nativecount; refs=D2H(image)->ref; leases=dd->dref->ref;
	failat=allocations+2;
	screen=allocdrawscreen(image,fill,0); failat=0;
	require(screen==nil && nativecount==count && D2H(image)->ref==refs && dd->dref->ref==leases && strcmp(message,exHeap)==0);
	destroy(image); destroy(fill); destroy(dd); baseline();
}


/* Select the real result cell and watch publication before the old source owner can be freed. */
static void*
factoryslot(void *argument, void *fallback, void *old, int alias)
{
	void *slot;

	slot = alias ? argument : fallback;
	if(alias)
		memmove(slot, &old, sizeof(old));
	aliasedowner = old;
	aliasedslot = slot;
	aliasactive = alias;
	return slot;
}


/* Exercise five actual generated-frame factories, including aliased and byte-aligned result cells. */
static void
factories(int kind,int alias,int failure)
{
	DDisplay *dd;
	Draw_Image *image=nil;
	DScreen *screen=nil;
	void *source,*result,*old,*slot;
	uchar output[24];
	int i;

	dd=newdisplay(); source=dd;
	if(kind==2 || kind==4){
		image=newimage(dd); source=image;
		if(kind==4){ screen=allocdrawscreen(image,image,0); require(screen!=nil); source=&screen->drawscreen; destroy(image); }
		destroy(dd);
	}
	memset(output,0xa5,sizeof(output));
	old=alias?source:H2D(void*,heapz(&Tstring));
	slot=output+1; memmove(slot,&old,sizeof(old));
	if(failure==1 || failure==3) failat=allocations+2;
	if(failure==3 && (kind==0 || kind==1 || kind==4)) wirefail=1;
	if(failure==2){factoryfail=1;wirefail=1;}
	switch(kind){
	case 0: {
		F_Draw_Display_newimage f;
		memset(&f,0,sizeof(f));
		f.d=source;
		f.r.max.x=4;
		f.r.max.y=4;
		f.chans.desc=RGBA32;
		slot=factoryslot(&f.d,slot,old,alias);
		f.ret=slot;
		Draw_Display_newimage(&f);
		memmove(&result,slot,sizeof(result));
		break;
	}
	case 1: {
		F_Draw_Display_colormix f;
		memset(&f,0,sizeof(f));
		f.d=source;
		f.c1=1;
		f.c2=2;
		slot=factoryslot(&f.d,slot,old,alias);
		f.ret=slot;
		Draw_Display_colormix(&f);
		memmove(&result,slot,sizeof(result));
		break;
	}
	case 2: {
		F_Draw_Screen_allocate f;
		memset(&f,0,sizeof(f));
		f.image=source;
		f.fill=source;
		slot=factoryslot(&f.image,slot,old,alias);
		f.ret=slot;
		Draw_Screen_allocate(&f);
		memmove(&result,slot,sizeof(result));
		break;
	}
	case 3: {
		F_Draw_Display_publicscreen f;
		memset(&f,0,sizeof(f));
		f.d=source;
		f.id=42;
		slot=factoryslot(&f.d,slot,old,alias);
		f.ret=slot;
		Draw_Display_publicscreen(&f);
		memmove(&result,slot,sizeof(result));
		break;
	}
	case 4: {
		F_Draw_Screen_newwindow f;
		memset(&f,0,sizeof(f));
		f.screen=source;
		f.r.max.x=3;
		f.r.max.y=5;
		slot=factoryslot(&f.screen,slot,old,alias);
		f.ret=slot;
		Draw_Screen_newwindow(&f);
		memmove(&result,slot,sizeof(result));
		break;
	}
	default:require(0);result=H;
	}
	aliasactive=0;failat=wirefail=factoryfail=0;
	require((result==H)==(failure!=0) && held==0 && worker.nerr==0);
	if(failure==1 || failure==3) require(strcmp(message,exHeap)==0);
	if(!alias){
		for(i=0;i<sizeof(output);i++) if(i<1 || i>=1+sizeof(void*)) require(output[i]==0xa5);
		destroy(source);
	}
	destroy(result);baseline();
}


#ifdef DIS_NATIVE64_DRAW_HEADER
/* Exercise the actual native registration refusal, without calling the private descriptor stage. */
static int
guardedregistration(void *argument)
{
	USED(argument);
	drawmodinit();
	return 1;
}
#endif


/* Close genuine managed pool objects and native leases, retaining no descriptors or hidden live owners. */
int
main(void)
{
	int kind,alias,failure;

#ifdef DIS_NATIVE64_DRAW_HEADER
	require(!kprotect(guardedregistration,nil) && live==0 && nativecount==0);
	require(TFont==nil && TImage==nil && TScreen==nil && TDisplay==nil && strstr(message,"not supported")!=nil);
	printf("PASS native Draw registration stays closed before descriptors/resources\n");
	return 0;
#endif
	typesetup();baseline();
	nativefailures();retirement();rollback();
	for(kind=0;kind<5;kind++) for(alias=0;alias<2;alias++) for(failure=0;failure<4;failure++) factories(kind,alias,failure);
	freetype(TFont);freetype(TImage);freetype(TScreen);freetype(TDisplay);
	require(live==0 && nativecount==0 && worker.nerr==0 && gchalt==0);
	printf("PASS Draw owners: %d predicates, 40 actual Frame64 factory scenarios, real Heap/Type/refcount/rollback; native images=%d screens=%d displays=%d, all ledgers zero; collector/device not executed\n",checks,freedimages,freedscreens,closes);
	return 0;
}
