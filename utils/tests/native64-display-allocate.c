#include "tk.h"

static Prog allocationprogram; // Borrowed VM context; no Tk implementation is exercised.
static TkTop allocationtop; // Borrowed cached-display context only.
static int allocationcases, initcalls, initfailure, lockfailure, fontpolicy, fontcalls; // Native service schedules.
static int rollbackexpected, inputready; // Refusal ownership and completion of native device capture.
static void *inputwatch, *resultwatch, *resultold; // Real managed input and result publication observations.
static Display *constructed; // Current initdisplay service owner, or nil.
static Image *defaults[5]; // Exact independent native default image identities.
ROOT_IMPLEMENTATION

static Cache defaultcache; // Bounded optional cache-service record, not its lookup algorithm.


/* A semantic omission must stop before unsafe cleanup or a stale input dereference. */
static void
displaycheck(int condition, char *stage)
{
	int i;

	if(!condition){
		fprintf(stderr, "Display allocation mismatch: %s\n", stage);
		for(i=0; i<RuntimeBlocks; i++)
			if(blocks[i].pointer != nil)
				trackedfree(blocks[i].pointer);
		exit(10);
	}
	checks++;
}


/* Observe the real external-root registry and mapped child edges at nested allocation boundaries. */
static void
allocationrootobserve(void)
{
	Ptrhash *entry;
	DDisplay *owner;
	Draw_Image *images[5];
	Heap *root;
	int bucket, roots, children, expected, i;

	root = nil;
	roots = 0;
	for(bucket=0; bucket<PTRHASH; bucket++)
		for(entry=ptrtab[bucket]; entry!=nil; entry=entry->next){
			root = entry->value;
			roots++;
		}
	displaycheck(roots == 1 && root != nil && root->t == TDisplay, "construction root");
	owner = H2D(DDisplay*,root);
	images[0] = owner->drawdisplay.image; images[1] = owner->drawdisplay.black;
	images[2] = owner->drawdisplay.white; images[3] = owner->drawdisplay.opaque;
	images[4] = owner->drawdisplay.transparent;
	children = 0;
	for(i=0; i<5; i++)
		if(images[i] != H){
			displaycheck(D2H(images[i])->t == TImage && images[i]->display == (Draw_Display*)owner, "construction child");
			children++;
		}
	expected = allocationstep-3;
	if(expected > 5) expected = 5;
	displaycheck(children == expected, "construction edges");
}


/* Retire only reusable external-root metadata after the operation removed its live pin. */
static void
allocationrootcleanup(void)
{
	Ptrhash *entry;
	int bucket;

	for(bucket=0; bucket<PTRHASH; bucket++)
		displaycheck(ptrtab[bucket] == nil, "root retirement");
	while(ptrfree != nil){
		entry = ptrfree;
		ptrfree = entry->next;
		trackedfree(entry);
	}
}


/* Compare a possibly packed result without assigning it any pointer-provenance authority. */
static void
displayobserve(void)
{
	void *value;

	if(resultwatch != nil){
		memmove(&value, resultwatch, sizeof(value));
		displaycheck(value == resultold, "early publication");
	}
}


/* Read only the existing cached-display context; no Tk initializer or event loop runs. */
Prog*
currun(void)
{
	return &allocationprogram;
}


/* Observe the existing cache-helper mutability notification, without collector traversal. */
void
poolmutable(void *owner)
{
	displaycheck(allocationtop.dd != H && owner == D2H(allocationtop.dd), "cached owner");
	inputready = 1;
}


/* The process-wide cache lock is a non-owning bounded service in this fixture. */
void*
libqlalloc(void)
{
	displayobserve();
	if(lockfailure){
		kwerrstr("cache lock refused");
		return nil;
	}
	return &allocationprogram;
}


/* Match the lock held by successful initdisplay, including the local-display path. */
void
libqunlock(void *lock)
{
	displaycheck(constructed != nil && lock == constructed->qlock && held == 1, "initial unlock");
	held = 0;
}


/* Return five independent native image owners with deliberately distinct geometry/channels. */
Display*
initdisplay(char *device, char *window, void (*callback)(Display*, char*))
{
	Display *d;
	int i;

	USED(callback);
	displayobserve();
	initcalls++;
	inputready = 1;
	displaycheck(device == window && (device == nil || strcmp(device,"fixturefont") == 0), "device capture");
	if(initfailure){
		kwerrstr("native display refused");
		return nil;
	}
	d = nativeallocate(sizeof(*d),4);
	displaycheck(d != nil, "fixture display storage");
	constructed = d;
	d->qlock = d;
	for(i=0; i<5; i++){
		defaults[i] = nativeallocate(sizeof(Image),1);
		displaycheck(defaults[i] != nil, "fixture image storage");
		defaults[i]->display = d;
		defaults[i]->id = i+1;
		defaults[i]->r = Rect(i,i+1,i+8,i+10);
		defaults[i]->clipr = defaults[i]->r;
		defaults[i]->chan = i == 0 ? RGBA32 : GREY1;
		defaults[i]->repl = i != 0;
	}
	d->image = defaults[0]; d->black = defaults[1]; d->white = defaults[2];
	d->opaque = defaults[3]; d->transparent = defaults[4];
	held = 1;
	return d;
}


/* Rollback closes the still-native defaults; successful managed retirement has detached them. */
void
allocationclosedisplay(Display *d)
{
	Image *images[5];
	int i;

	displaycheck(held == 0, "close unlocked");
	if(d == constructed){
		images[0] = d->image; images[1] = d->black; images[2] = d->white;
		images[3] = d->opaque; images[4] = d->transparent;
		for(i=0; i<5; i++){
			if(rollbackexpected)
				displaycheck(images[i] == defaults[i] && nativealive(images[i],1), "native rollback ownership");
			else
				displaycheck(images[i] == nil, "successful detach");
			if(images[i] != nil)
				nativeretire(images[i],1);
		}
		d->image = d->black = d->white = d->opaque = d->transparent = nil;
		constructed = nil;
	}
	nativeretire(d,4);
	kwerrstr("cleanup diagnostic");
}


/* Keep default-font construction separate while observing its position after all wrappers. */
Subfont*
getdefont(Display *d)
{
	Subfont *font;
	DRef *reference;

	displayobserve();
	allocationrootobserve();
	reference = d->limbo;
	displaycheck(held == 1 && reference->ref == 6, "font after wrappers");
	fontcalls++;
	if(fontpolicy == 0)
		return nil;
	font = nativeallocate(sizeof(*font),6);
	displaycheck(font != nil, "fixture subfont storage");
	font->n = 256; font->height = 15; font->ascent = 13;
	return font;
}


/* Release the bounded default-subfont service owner, without a managed-cache or rasterizer claim. */
void
allocationsubfontclose(Subfont *font)
{
	displaycheck(nativealive(font,6), "subfont owner");
	nativeretire(font,6);
}

/* Observe the bounded default-font description at the existing formatting boundary. */
int
sprint(char *buffer, char *format, ...)
{
	va_list arguments;
	int count;

	va_start(arguments,format);
	count = vsnprintf(buffer,128,format,arguments);
	va_end(arguments);
	displaycheck(count >= 0 && count < 128, "font description");
	return count;
}


ALLOCATION_IMPLEMENTATION


/* Tear down the public graph explicitly; a tracing collector is not simulated by reference counts. */
static void
releasegraph(Draw_Display *display)
{
	Draw_Image *images[5];
	int i;

	if(display == H)
		return;
	images[0] = display->image; images[1] = display->black; images[2] = display->white;
	images[3] = display->opaque; images[4] = display->transparent;
	display->image = display->black = display->white = display->opaque = display->transparent = H;
	for(i=0; i<5; i++)
		destroy(images[i]);
	destroy(display);
}


/* Reset independent observations between actual Frame64 invocations. */
static void
allocationsetup(void)
{
	baseline();
	allocationfail = allocationstep = 0;
	allocationprogram.tktop = nil;
	memset(&allocationtop,0,sizeof(allocationtop));
	allocationtop.dd = H;
	cacheqlock = nil;
	initcalls = initfailure = lockfailure = fontpolicy = fontcalls = rollbackexpected = inputready = 0;
	inputwatch = resultwatch = resultold = nil;
	constructed = nil;
	memset(&defaultcache,0,sizeof(defaultcache));
}


/* Cover aliased and packed destinations plus each managed-wrapper refusal. */
static void
allocationcase(int layout, int failure, int policy)
{
	F_Draw_Display_allocate frame;
	String *device;
	Draw_Display *result;
	DDisplay *owner;
	uchar packed[sizeof(void*)+2];
	void *slot, *old;
	int i, savedcloses;
	Draw_Image *images[5];

	allocationsetup();
	fontpolicy = policy;
	memset(&frame,0,sizeof(frame));
	device = layout == 3 ? H : H2D(String*,heapz(&Tstring));
	old = device;
	frame.dev = device;
	slot = layout == 0 ? (void*)&frame.dev : (void*)(packed+(layout == 3 ? 1 : layout));
	memmove(slot,&old,sizeof(old));
	frame.ret = slot;
	inputwatch = device;
	resultwatch = slot; resultold = old;
	initfailure = failure == 2;
	lockfailure = failure == 1;
	/* Native service storage is excluded from the production managed/DRef fault schedule. */
	allocationfail = failure >= 3 ? failure-2 : 0;
	allocationstep = 0;
	rollbackexpected = failure >= 4;
	if(failure == 1 || failure == 3)
		inputwatch = nil;
	savedcloses = closes;
	Draw_Display_allocate(&frame);
	resultwatch = inputwatch = nil;
	memmove(&result,slot,sizeof(result));
	allocationrootcleanup();
	if(failure != 0){
		displaycheck(result == H, "refused result");
		displaycheck(held == 0 && nativecount == 0 && live == 4, "rollback conservation");
		displaycheck(closes == savedcloses+(failure >= 4), "rollback close count");
		displaycheck(fontcalls == 0, "no font before wrappers");
		if(failure >= 4)
			displaycheck(strcmp(message,failure == 5 ? "ptradd malloc" : exHeap) == 0, "primary diagnostic");
	}else{
		displaycheck(result != H && held == 0, "complete result");
		owner = (DDisplay*)result;
		displaycheck(D2H(owner)->ref == 6 && owner->dref->ref == 6, "complete references");
		images[0] = result->image; images[1] = result->black; images[2] = result->white;
		images[3] = result->opaque; images[4] = result->transparent;
		for(i=0; i<5; i++){
			displaycheck(images[i] != H && ((DImage*)images[i])->image == defaults[i], "default identity");
			displaycheck(images[i]->r.min.x == i && images[i]->r.min.y == i+1 && images[i]->chans.desc == defaults[i]->chan, "default geometry");
			displaycheck(images[i]->display == result && images[i]->iname == H, "mapped owners");
		}
		displaycheck(fontcalls == 1 && defaultcache.ref == (policy == 3), "optional font phase");
		releasegraph(result);
	}
	displaycheck(defaultcache.ref == 0, "default cache baseline");
	allocationfail = 0;
	baseline();
	allocationcases++;
}


/* Invoke malformed-result cases through the real protected VM error boundary. */
static int
allocationinvoke(void *frame)
{
	Draw_Display_allocate(frame);
	return 1;
}


/* Cached reuse captures one reference through an aligned local, even for a packed caller result. */
static void
cachedcase(int self)
{
	F_Draw_Display_allocate frame;
	DDisplay *owner;
	Draw_Display *result;
	uchar packed[sizeof(void*)+1];
	void *old;

	allocationsetup();
	owner = newdisplay();
	allocationtop.dd = &owner->drawdisplay;
	allocationprogram.tktop = &allocationtop;
	old = self ? (void*)owner : H;
	if(self)
		D2H(owner)->ref++;
	memmove(packed+1,&old,sizeof(old));
	memset(&frame,0,sizeof(frame));
	frame.ret = (Draw_Display**)(packed+1); frame.dev = H;
	Draw_Display_allocate(&frame);
	memmove(&result,packed+1,sizeof(result));
	displaycheck(result == (Draw_Display*)owner && D2H(owner)->ref == 2 && initcalls == 0, "cached result");
	destroy(result);
	destroy(owner);
	baseline();
	allocationcases++;
}


/* Execute all refusal schedules with real managed allocations and conservation checks. */
int
allocationmain(void)
{
	F_Draw_Display_allocate frame;
	void *old;
	int layout, failure, policy;

	typesetup();
	for(layout=0; layout<3; layout++)
		for(failure=0; failure<=10; failure++)
			allocationcase(layout,failure,0);
	allocationcase(3,0,0);
	for(policy=1; policy<=3; policy++)
		allocationcase(1,0,policy);
	cachedcase(0); cachedcase(1);
	allocationsetup();
	memset(&frame,0,sizeof(frame));
	displaycheck(!kprotect(allocationinvoke,&frame) && strcmp(message,exHeap) == 0, "nil result address");
	frame.ret = H;
	displaycheck(!kprotect(allocationinvoke,&frame) && strcmp(message,exHeap) == 0, "sentinel result address");
	frame.ret = (Draw_Display**)((uintptr)-1-3);
	displaycheck(!kprotect(allocationinvoke,&frame) && strcmp(message,exHeap) == 0, "wrapped result address");
	old = nil; frame.ret = (Draw_Display**)&old;
	displaycheck(!kprotect(allocationinvoke,&frame) && strcmp(message,exNilref) == 0, "nil old owner");
	baseline();
	freetype(TFont); freetype(TImage); freetype(TScreen); freetype(TDisplay);
	displaycheck(live == 0 && nativecount == 0 && worker.nerr == 0 && held == 0, "final baseline");
	printf("PASS Display.allocate: %d active checks, %d Frame64 scenarios; actual Heap/Type/maps/retirement; native/font/lock services bounded\n",checks,allocationcases);
	return 0;
}
