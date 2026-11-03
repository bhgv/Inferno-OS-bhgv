#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "draw.h"
#include "kernel.h"
#include "draw-native64.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

static Display display, other; // Explicit native display identities, not graphics device ownership.
static Image images[4]; // Destination, source, background and cached font image.
static Font font; // Real native cache geometry with preloaded independent glyphs.
static Cacheinfo glyphs[1024+NFLOOK]; // Real native glyph cache storage and actual hit paths.
static Draw_Image objects[3]; // Borrowed VM lookup objects without resource allocation.
static Draw_Font fontobject; // Borrowed VM font object resolved only by the lookup transport.
static unsigned long checks; // Active semantic predicates under NDEBUG.
static int cachecalls, ages, requests, used, locked, flushes, failat, cachemode, subfontcalls; // Effect observations and controlled service refusals.
static uchar actual[1024]; // Bounded complete native command trace.
static jmp_buf recovery; // Explicit VM error transport, no scheduler/GC simulation.
static char *raised; // Actual error identity delivered by the source consumer.
char exNilref[]="nil", exHeap[]="heap"; // Required pre-effect refusal identities.

typedef struct TextCase TextCase;

struct TextCase {
	int wide; // Zero ASCII, one native Rune input.
	int count; // Captured logical character count.
	int variant; // Text/textop/textbg/textbgop/width callable identity.
	int mode; // Zero native API, one actual compiler-generated Frame64 consumer.
	int fault; // Exact native reservation to refuse, or zero.
	Point p; // Independent requested point.
	Point bgp; // Independent background origin.
	Point result; // Independent returned point or width in x.
	int requests; // Independent count of attempted complete command reservations.
	int bytes; // Independent actual trace size after a possible refusal.
	uchar expected[1024]; // Exact Python protocol bytes, including optional operator prefixes.
};

static TextCase *current; // Active immutable packet and result oracle.


/* Stop before a damaged source control reaches a bad read, arithmetic operation or callback. */
static void
checkat(int condition, int line)
{
	if(!condition){
		fprintf(stderr,"text semantic mismatch after %lu checks line %d variant=%d mode=%d\n",checks,line,
			current!=nil?current->variant:-1,current!=nil?current->mode:-1);
		exit(10);
	}
	checks++;
}

#define check(condition) checkat((condition), __LINE__) // Preserve semantic predicates with source coordinates.


/* Deliver only expected VM metadata errors to the local checked boundary. */
void
error(char *message)
{
	raised=message;
	longjmp(recovery,1);
}


/* A Rune consumer must not allocate or mutate a UTF-8 conversion cache. */
char*
string2c(String *string)
{
	USED(string);
	check(0);
	return "";
}


/* Expose one complete packet reservation and refuse the selected attempt without trace mutation. */
uchar*
bufimage(Display *d,int bytes)
{
	uchar *p;

	check(d==&display && current!=nil && requests<current->requests && bytes>0 && bytes<=261);
	requests++;
	if(requests==failat) return nil;
	check(used+bytes<=sizeof(actual));
	p=actual+used;
	used+=bytes;
	return p;
}


/* Keep existing native formatting diagnostics outside the packet/result oracle. */
int
_drawprint(int fd,char *format,...)
{
	USED(fd); USED(format);
	return 0;
}


/* Resolve only declared borrowed VM images; no arbitrary-pointer or managed lifetime proof is implied. */
Image*
checkimage(Draw_Image *object)
{
	int i;

	for(i=0;i<3;i++) if(object==objects+i) return images+i;
	check(0);
	return nil;
}


/* Preserve exact font identity at the bounded native lookup boundary. */
Font*
checkfont(Draw_Font *object)
{
	check(object==&fontobject);
	return &font;
}


/* Count actual display exclusion without modeling native worker ownership. */
int
lockdisplay(Display *d)
{
	check(d==&display && locked==0);
	locked=1;
	return 1;
}


/* Require every native consumer to release its acquired display exclusion. */
void
unlockdisplay(Display *d)
{
	check(d==&display && locked==1);
	locked=0;
}


/* Observe flush only inside the owning display exclusion. */
void
checkflush(Draw_Image *object)
{
	check(object==objects && locked==1);
	flushes++;
}


/* Cached-glyph positives must never enter allocation/resize or device font loading. */
static int
fontresize(Font *f,int width,int count,int depth)
{
	USED(f); USED(width); USED(count); USED(depth);
	check(0);
	return 0;
}


/* Unknown glyph transport is deliberately absent from this cache-hit owning proof. */
int
loadchar(Font *f,Rune r,Cacheinfo *entry,int index,int count,char **name)
{
	USED(f); USED(r); USED(entry); USED(index); USED(count); USED(name);
	check(0);
	return 0;
}


/* Controlled missing-subfont refusal exercises the real text/width fallback branches. */
Subfont*
_getsubfont(Display *d,char *name,int height)
{
	check(d==&display && name!=nil && height==20 && cachemode==2);
	subfontcalls++;
	return nil;
}

/* Native subfont resource retirement is outside this preloaded cache-hit transport. */
void
freesubfont(Subfont *subfont)
{
	USED(subfont);
	check(0);
}

CACHE_IMPLEMENTATION


/* Positive calls execute actual cachechars; selected refusals bound cache service behavior independently. */
int
cachechars(Font *f,char **s,Rune **r,ushort *out,int count,int *width,char **name)
{
	cachecalls++;
	if(cachemode==1){
		check(cachecalls<=12);
		*width=0; *name=nil;
		return 0;
	}
	if(cachemode==2){
		*width=2147483647; *name="missing";
		return 0;
	}
	if(cachemode==3){
		check(count>0 && s!=nil && **s=='A');
		(*s)++; *width=2147483647; *name=nil; out[0]=81;
		return 1;
	}
	return actualcachechars(f,s,r,out,count,width,name);
}


/* Keep the real age algorithm while stopping a damaged width control before overflowing addition. */
void
agefont(Font *f)
{
	check(!(cachemode==3 && cachecalls>1));
	ages++;
	actualagefont(f);
}

PACKET_HELPER
NATIVE_IMPLEMENTATION
VM_IMPLEMENTATION
TEXT_ORACLES


/* Establish independently specified glyph widths/identities for the actual native cache-hit path. */
static void
setup(void)
{
	static Rune runes[]={65,66,67,68,937,128512}; // ASCII, Greek and supplementary-plane scalar inputs.
	unsigned int i, slot;

	memset(&display,0,sizeof(display)); memset(&font,0,sizeof(font)); memset(glyphs,0,sizeof(glyphs));
	memset(images,0,sizeof(images));
	for(i=0;i<4;i++) images[i].display=&display;
	images[0].id=7; images[1].id=9; images[2].id=11; images[3].id=13;
	images[0].clipr=Rect(-300,-400,500,600);
	font.display=&display; font.cacheimage=images+3; font.cache=glyphs;
	font.ncache=1024+NFLOOK; font.age=1000; font.ascent=12; font.height=20;
	for(i=0;i<nelem(runes);i++){
		slot=17*runes[i]&1023;
		check(glyphs[slot].age==0);
		glyphs[slot].value=runes[i]; glyphs[slot].width=runes[i]%7+1; glyphs[slot].age=999;
	}
	cachecalls=ages=requests=used=locked=flushes=cachemode=subfontcalls=0;
	memset(actual,0xa5,sizeof(actual)); raised=nil;
}


/* Invoke fresh actual Frame64 declarations and preserve every input byte. */
static void
framecall(TextCase *c,String *input,uchar *result)
{
	switch(c->variant){
	FRAME_DISPATCH
	default: check(0);
	}
}


/* Exercise actual public C text wrappers as well as generated-frame consumers. */
static Point
nativecall(TextCase *c,char *ascii,Rune *runes)
{
	Point sp={-17,19};

	if(c->variant==4) return Pt(c->wide?runestringnwidth(&font,runes,c->count):stringnwidth(&font,ascii,c->count),0);
	if(c->wide){
		switch(c->variant){
		case 0: return runestringnop(images,c->p,images+1,sp,&font,runes,c->count,SoverD);
		case 1: return runestringnop(images,c->p,images+1,sp,&font,runes,c->count,(Drawop)5);
		case 2: return runestringnbgop(images,c->p,images+1,sp,&font,runes,c->count,images+2,c->bgp,SoverD);
		case 3: return runestringnbgop(images,c->p,images+1,sp,&font,runes,c->count,images+2,c->bgp,(Drawop)5);
		}
	}else{
		switch(c->variant){
		case 0: return stringnop(images,c->p,images+1,sp,&font,ascii,c->count,SoverD);
		case 1: return stringnop(images,c->p,images+1,sp,&font,ascii,c->count,(Drawop)5);
		case 2: return stringnbgop(images,c->p,images+1,sp,&font,ascii,c->count,images+2,c->bgp,SoverD);
		case 3: return stringnbgop(images,c->p,images+1,sp,&font,ascii,c->count,images+2,c->bgp,(Drawop)5);
		}
	}
	check(0);
	return ZP;
}


/* Compare exact packets/partial returns and unaligned result cells, including shortened String payloads. */
static void
exercise(TextCase *c)
{
	union { uvlong alignment; uchar bytes[2048]; } storage;
	static Rune alphabets[2][4]={{65,66,67,68},{65,937,128512,66}}; // Independent Unicode inputs.
	String *input;
	Point result;
	uchar output[24];
	char *ascii;
	Rune *runes;
	void *cache;
	int i;

	current=c; setup(); failat=c->fault;
	memset(&storage,0xa5,sizeof(storage));
	input=(String*)storage.bytes; input->len=c->wide?-c->count:c->count; input->max=c->count;
	cache=(void*)0x12345678; input->tmp=cache;
	ascii=input->Sascii; runes=input->Srune;
	for(i=0;i<c->count;i++) if(c->wide) runes[i]=alphabets[1][i%4]; else ascii[i]=alphabets[0][i%4];
	/* Native APIs receive terminated storage; VM views must repair a stale shortened tail themselves. */
	if(!c->mode || c->count==0){ if(c->wide) runes[c->count]=0; else ascii[c->count]=0; }
	memset(output,0xa5,sizeof(output));
	if(c->mode){
		framecall(c,input,output+1);
		memmove(&result.x,output+1,4);
		result.y=0;
		if(c->variant!=4) memmove(&result.y,output+5,4);
		check(output[0]==0xa5);
		for(i=c->variant==4?5:9;i<sizeof(output);i++) check(output[i]==0xa5);
	}else result=nativecall(c,ascii,runes);
	check(result.x==c->result.x && result.y==c->result.y && requests==c->requests && used==c->bytes);
	check(memcmp(actual,c->expected,used)==0 && locked==0 && input->tmp==cache);
	for(i=used;i<sizeof(actual);i++) check(actual[i]==0xa5);
}


/* Verify deterministic skipped outputs, missing backgrounds and each pre-effect metadata/result refusal. */
static void
views(void)
{
	union { uvlong alignment; uchar bytes[128]; } storage;
	F_Draw_Image_textbg f;
	F_Draw_Font_width width;
	DrawTextView view;
	Draw_Point result;
	String *input;
	int n, caught;

	current=nil;
	for(n=0;n<8;n++){
		setup(); memset(&f,0,sizeof(f)); result.x=result.y=99;
		f.ret=&result; f.p.x=17; f.p.y=-19; f.dst=objects; f.src=objects+1; f.bg=objects+2; f.font=&fontobject; f.str=H;
		if(n==0) f.dst=H;
		if(n==1) f.src=H;
		if(n==2) f.font=H;
		if(n==3) f.bg=H;
		if(n>=4){
			memset(&storage,0,sizeof(storage)); input=(String*)storage.bytes;
			input->len=input->max=1; input->Sascii[0]='A'; f.str=input;
			if(n==4) images[0].display=&other;
			if(n==5) images[1].display=&other;
			if(n==6) font.display=&other;
			if(n==7) images[2].display=&other;
		}
		Draw_Image_textbg(&f);
		check(result.x==17 && result.y==-19 && requests==0 && cachecalls==0 && locked==0 && flushes==0);
	}
	for(n=0;n<7;n++){
		setup(); memset(&storage,0xa5,sizeof(storage)); input=(String*)storage.bytes;
		input->len=input->max=1;
		if(n==0) input=nil;
		if(n==1) input->len=(-2147483647-1);
		if(n==2) input->max=0;
		if(n==3) input->max=-1;
		if(n==4) input=(String*)((uintptr)-1-4);
		if(n==5){ input=(String*)(storage.bytes+1); memmove((uchar*)input+O(String,len),&(int){-1},4); memmove((uchar*)input+O(String,max),&(int){1},4); }
		if(n==6){ input=(String*)(storage.bytes+1); memmove((uchar*)input+O(String,len),&(int){1},4); memmove((uchar*)input+O(String,max),&(int){1},4); }
		caught=setjmp(recovery);
		if(!caught) drawtextview(input,&view);
		check((caught!=0)==(n!=6));
		if(caught) check(raised==(n==0?exNilref:exHeap));
		check(requests==0 && cachecalls==0 && locked==0);
	}
	for(n=0;n<3;n++){
		setup(); memset(&width,0,sizeof(width)); width.str=H; width.f=H;
		width.ret=n==0?nil:n==1?H:(WORD*)((uintptr)-1-1);
		caught=setjmp(recovery);
		if(!caught) Draw_Font_width(&width);
		check(caught && raised==exHeap && requests==0 && cachecalls==0);
	}
}


/* Exercise real progress/fallback/overflow branches and exact count boundaries without NUL lookahead. */
static void
cachefailures(void)
{
	TextCase empty;
	Point point, start={10,20};
	char text[]="AAA", *exact, *cursor;
	ushort indices[2];
	int width;
	char *name;

	memset(&empty,0,sizeof(empty)); current=&empty;
	setup(); cachemode=1;
	point=stringnop(images,start,images+1,ZP,&font,text,3,SoverD);
	check(point.x==10 && point.y==20 && requests==0 && cachecalls==11);
	setup(); cachemode=2;
	check(stringnwidth(&font,text,3)==0 && subfontcalls==1 && ages==0);
	setup(); cachemode=2;
	point=stringnop(images,start,images+1,ZP,&font,text,3,SoverD);
	check(point.x==10 && point.y==20 && subfontcalls==1 && ages==0);
	setup(); cachemode=3;
	check(stringnwidth(&font,text,3)==2147483647 && cachecalls==2 && ages==1);
	setup();
	check(stringnwidth(&font,"A\316\251\360\237\230\200",3)==17);
	setup(); exact=malloc(1); check(exact!=nil); exact[0]='A'; cursor=exact;
	check(actualcachechars(&font,&cursor,nil,indices,1,&width,&name)==1 && cursor==exact+1 && width==3);
	check(stringnwidth(&font,exact,1)==3);
	empty.requests=1; empty.bytes=0; current=&empty; failat=1;
	point=stringnop(images,start,images+1,ZP,&font,exact,1,SoverD);
	check(point.x==10 && point.y==20 && requests==1);
	free(exact);
	cursor=H;
	check(actualcachechars(&font,&cursor,nil,indices,0,&width,&name)==0 && width==0);
}


/* Run the independent Unicode/geometry oracle and the actual cache/failure/Frame64 paths. */
int
main(void)
{
	unsigned int i;

	check(sizeof(void*)==8 && sizeof(WORD)==4 && sizeof(Frame)==32);
	views(); cachefailures();
	for(i=0;i<nelem(cases);i++) exercise(cases+i);
	printf("PASS native text: %u independent native/Frame64 cases, %lu predicates, actual cache hits/width/packets, five consumers; miss/I-O/lookup/locks bounded\n",(unsigned int)nelem(cases),checks);
	return 0;
}
