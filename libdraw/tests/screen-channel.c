#include "lib9.h"
#include "draw.h"
#include <stdio.h>

static Display display, foreign; // Independent native display identities.
static Image base, fill, previous, window, reused; // Bounded native headers, without VM ownership claims.
static uchar packet[64]; // Actual constructor protocol bytes.
static int checks, cases, reservations, flushes, allocations, imagecalls, attachments, failure; // Independent effect ledger.
static void *screenowner; // Only the currently owned genuine Screen allocation.
static ulong screenbytes; // Payload length before independent guard bytes.
static ulong expectedchan; // Channel selected independently by each test scenario.


/* Semantic failures, including omission controls, stop before any unsafe dereference. */
static void
check(int condition, char *stage)
{
	if(!condition){
		free(screenowner);
		screenowner = nil;
		fprintf(stderr,"Screen channel mismatch: %s\n",stage);
		exit(10);
	}
	checks++;
}


/* Native diagnostics are outside the channel/ownership proof. */
void
kwerrstr(char *format, ...)
{
	USED(format);
}


/* Reserve a real bounded packet, with a separately injected reservation refusal. */
uchar*
bufimage(Display *d, int n)
{
	check(d == &display && n > 0 && n <= sizeof(packet),"packet display");
	reservations++;
	if(failure == 2)
		return nil;
	memset(packet,0xa5,sizeof(packet));
	return packet;
}


/* Observe constructor completion independently from reservation and allocation. */
int
flushimage(Display *d, int visible)
{
	check(d == &display && visible == 0,"flush display");
	flushes++;
	return failure == 3 ? -1 : 1;
}


/* Allocate genuine native Screen storage, permitting the first constructor allocation to refuse. */
static void*
screenallocate(ulong bytes)
{
	void *p;

	allocations++;
	if(failure == 1)
		return nil;
	check(screenowner == nil,"single screen owner");
	p = malloc(bytes+32);
	check(p != nil,"fixture allocation");
	memset(p,0xa5,bytes+32);
	screenowner = p;
	screenbytes = bytes;
	return p;
}


/* Retire the actual Screen owner once, checking writes never crossed its native header. */
static void
screenretire(void *p)
{
	int i;

	check(p == screenowner && p != nil,"screen retirement");
	for(i=0; i<32; i++)
		check(((uchar*)p)[screenbytes+i] == 0xa5,"screen guard");
	screenowner = nil;
	free(p);
}


/* The image constructor is a bounded service; record channel and unchanged arguments before refusal. */
Image*
_allocimage(Image *old, Display *d, Rectangle r, ulong chan, int repl, ulong val, int id, int refresh)
{
	Image *result;

	imagecalls++;
	check(d == &display && chan == expectedchan,"window channel");
	check(r.min.x == -7 && r.max.y == 91 && repl == 0 && val == 0x89abcdefUL && id > 0 && refresh == Refbackup,"window arguments");
	if(failure == 4)
		return nil;
	result = old == nil ? &window : old;
	result->display = d;
	result->chan = chan;
	return result;
}


/* Attachment must follow successful list publication, once per public wrapper call. */
void
attachtowindow(Image *i)
{
	check(i == display.windows && i->screen != nil,"attachment publication");
	attachments++;
}

#define free(p) screenretire(p) // Retire only genuine constructor Screen storage.
#define malloc(n) screenallocate(n) // Inject only actual constructor storage allocation.
SCREEN_IMPLEMENTATION
#undef malloc
#undef free


/* Decode protocol integers independently of the production BPLONG serializer. */
static ulong
wire32(uchar *p)
{
	return (ulong)p[0] | (ulong)p[1]<<8 | (ulong)p[2]<<16 | (ulong)p[3]<<24;
}


/* Exercise owned and public screens, exact wire channels, window reuse and every constructor refusal. */
static void
scenario(int public, int fail, int reuse, ulong chan)
{
	Screen *screen;
	Image *result;
	Rectangle r;

	memset(&display,0,sizeof(display));
	memset(&base,0,sizeof(base));
	memset(&fill,0,sizeof(fill));
	memset(&reused,0,sizeof(reused));
	base.display = fill.display = &display;
	base.id = 17;
	fill.id = 23;
	base.chan = chan;
	expectedchan = chan;
	failure = fail;
	allocations = reservations = flushes = imagecalls = attachments = 0;
	screen = public ? publicscreen(&display,73,chan) : allocscreen(&base,&fill,1);
	cases++;
	if(fail >= 1 && fail <= 3){
		check(screen == nil,"constructor refusal");
		check(imagecalls == 0 && attachments == 0 && screenowner == nil,"refusal effects");
		check(flushes == (fail == 3 ? (public ? 1 : 25) : 0),"retry bound");
		return;
	}
	check(screen != nil && screen->chan == chan,"retained channel");
	check(screen->display == &display && screen->id > 0,"screen identity");
	check(screen->image == (public ? nil : &base) && screen->fill == (public ? nil : &fill),"borrowed owners");
	if(public)
		check(packet[0] == 'S' && wire32(packet+1) == 73 && wire32(packet+5) == chan,"public packet");
	else
		check(packet[0] == 'A' && wire32(packet+5) == 17 && wire32(packet+9) == 23 && packet[13] == 1,"owned packet");
	/* Channel remains valid even when the borrowed image metadata changes later. */
	base.chan = CMAP8;
	display.windows = &previous;
	r.min.x = -7; r.min.y = 4; r.max.x = 83; r.max.y = 91;
	result = reuse ? _allocwindow(&reused,screen,r,Refbackup,0x89abcdefUL) : allocwindow(screen,r,Refbackup,0x89abcdefUL);
	check(imagecalls == 1,"single image construction");
	if(fail == 4){
		check(result == nil && display.windows == &previous && attachments == 0,"window refusal");
	}else{
		check(result == (reuse ? &reused : &window) && display.windows == result,"window publication");
		check(result->screen == screen && result->next == &previous,"window linkage");
		check(attachments == !reuse,"attachment count");
	}
	screenretire(screen);
	check(screenowner == nil,"screen baseline");
}


/* Refuse malformed native handles before allocation, buffering or image-constructor side effects. */
static void
invalid(void)
{
	Screen s;
	int mode;

	failure = 0;
	for(mode=0; mode<8; mode++){
		memset(&s,0,sizeof(s));
		memset(&display,0,sizeof(display));
		s.display = &display; s.id = 1; s.chan = RGBA32;
		if(mode == 1) s.display = nil;
		if(mode == 2) display.closing = 1;
		if(mode == 3) s.id = 0;
		if(mode == 4) s.id = -1;
		if(mode == 5) s.chan = 0;
		if(mode == 6) s.chan = 0xff;
		if(mode == 7){
			if(sizeof(ulong) <= 4) continue;
			s.chan = (uvlong)RGBA32 | ((uvlong)1<<32);
		}
		allocations = reservations = imagecalls = attachments = 0;
		check(_allocwindow(nil,mode == 0 ? nil : &s,Rect(0,0,1,1),Refbackup,0) == nil,"invalid window");
		if(mode != 0)
			check(publicscreen(s.display,s.id,s.chan) == nil,"invalid public screen");
		check(allocations == 0 && reservations == 0 && imagecalls == 0 && attachments == 0,"invalid effects");
		cases++;
	}
	base.display = &display; base.chan = RGBA32; fill.display = &foreign;
	check(allocscreen(&base,&fill,0) == nil && allocations == 0,"foreign screen");
	fill.display = &display;
	display.closing = 1;
	check(allocscreen(&base,&fill,0) == nil && allocations == 0,"closed owned screen");
	display.closing = 0;
	base.chan = 0;
	check(allocscreen(&base,&fill,0) == nil && allocations == 0,"invalid owned channel");
	check(allocscreen(nil,&fill,0) == nil && allocscreen(&base,nil,0) == nil,"nil owned inputs");
}


/* Cover all native channel depths, both screen factories and reused/fresh window headers. */
int
main(void)
{
	ulong channels[] = {GREY1,GREY2,GREY4,GREY8,CMAP8,RGB16,RGB24,RGBA32,ARGB32};
	int c, public, fail, reuse;

	for(c=0; c<nelem(channels); c++)
		for(public=0; public<2; public++)
			for(fail=0; fail<=4; fail++)
				for(reuse=0; reuse<2; reuse++)
					scenario(public,fail,reuse,channels[c]);
	invalid();
	printf("PASS native Screen channel: %d checks, %d cases\n",checks,cases);
	return 0;
}
