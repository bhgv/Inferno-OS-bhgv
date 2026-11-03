#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "kernel.h"
#include "draw.h"
#include <stdio.h>

static Display display; // Actual native display and embedded bounded output buffer.
static Image windows[4]; // Stable callback objects retained across the stream test.
static Screen screen; // Non-nil screen identity without device allocation.
static Prog owner, stranger; // Explicit bounded display-owner identities.
static void *owned[32]; // Exact private queue allocations, including injected refusals.
static int live, refuse, ownerok, balance, locked; // Observable allocation and exclusion state.
static int inputlength, inputposition, inputchunk, inputcalls, terminal, oversized; // Bounded input stream state.
static int outputlength, outputchunk, outputcalls, outputfault; // Bounded native short-write transport.
static uchar output[32768]; // Independent capture of actual submitted output bytes.
static int closeonread, closeoncallback; // Valid-lifetime close publication at bounded call boundaries.
static int phase, nextrecord, callbacks, reentrant, sequence[16], diagnostic; // Callback and error oracles.
static unsigned long checks; // Active semantic predicates, independent of NDEBUG.
int _drawdebug; // Existing production diagnostic option.
char exImage[] = "image failure"; // Existing production diagnostic identity.

REFRESH_ORACLES


/* Fail before following poisoned queue links, and retire fixture allocations for LSan. */
static void
checkat(int condition, int line)
{
	int i;

	if(!condition){
		fprintf(stderr, "Refresh mismatch at %lu line %d\n", checks, line);
		for(i=0; i<nelem(owned); i++)
			if(owned[i] != nil)
				free(owned[i]);
		exit(10);
	}
	checks++;
}

#define check(value) checkat((value), __LINE__) // Keep source locations in active predicates.


/* Poison newly allocated native queue records so initialization is observable. */
static void*
refreshallocate(ulong bytes)
{
	void *pointer;
	int i;

	check(bytes == sizeof(Refreshq));
	if(refuse)
		return nil;
	pointer = malloc(bytes);
	check(pointer != nil);
	memset(pointer, 0xa5, bytes);
	for(i=0; i<nelem(owned); i++)
		if(owned[i] == nil){
			owned[i] = pointer;
			live++;
			return pointer;
		}
	check(0);
	return nil;
}


/* Require exact retirement once, before executing the corresponding callback. */
static void
refreshfree(void *pointer)
{
	int i;

	for(i=0; i<nelem(owned); i++)
		if(owned[i] == pointer){
			owned[i] = nil;
			live--;
			free(pointer);
			return;
		}
	check(0);
}


/* Preserve a distinct native error reason across failure reporting. */
void
kwerrstr(char *format, ...)
{
	USED(format);
	diagnostic = 1;
}


/* Read, without replacing, the bounded transport's current error diagnostic. */
void
kgerrstr(char *buffer, uint bytes)
{
	check(bytes >= 20);
	strcpy(buffer, diagnostic == 2 ? "native write error" : "display error");
}


/* Observe production diagnostics without interpreting Inferno format specifiers as libc. */
int
print(char *format, ...)
{
	USED(format);
	return 0;
}


/* Keep remote channel exclusion transitions explicit around both reads and writes. */
void
release(void)
{
	check(balance == 0);
	balance--;
}


/* Require restoration on every ordinary native channel return, including errors. */
void
acquire(void)
{
	check(balance == -1);
	balance++;
}


/* Model the current VM owner without claiming a scheduler or parallel payload admission. */
Prog*
currun(void)
{
	return &owner;
}


/* Keep the production buffer-flush ownership refusal observable. */
void*
libqlowner(void *lock)
{
	USED(lock);
	return ownerok ? &owner : &stranger;
}


/* Acquire the display lock exactly once per complete input batch. */
int
lockdisplay(Display *d)
{
	check(d == &display && locked == 0);
	locked = 1;
	return 1;
}


/* Incomplete input and EOF must never leave an acquired display lock behind. */
void
unlockdisplay(Display *d)
{
	check(d == &display && locked == 1);
	locked = 0;
}


/* Supply independent byte partitions and capture exact submitted short-write prefixes. */
long
kchanio(void *channel, void *buffer, int bytes, int mode)
{
	int amount, chunk;
	uintptr address, base;

	if(mode == OREAD){
		check(channel == display.refchan && balance == -1 && locked == 0);
		check(++inputcalls < 1000 && bytes > 0 && bytes <= 100);
		if(oversized)
			return bytes+1;
		if(inputposition == inputlength){
			if(terminal < 0)
				diagnostic = 2;
			return terminal;
		}
		amount = inputlength-inputposition;
		chunk = inputchunk ? inputchunk : 1+(inputcalls*37)%101;
		if(amount > chunk)
			amount = chunk;
		if(amount > bytes)
			amount = bytes;
		memmove(buffer, wire+inputposition, amount);
		inputposition += amount;
		if(closeonread) display.closing = 1;
		return amount;
	}
	check(mode == OWRITE && channel == display.datachan);
	check(balance == (display.local ? 0 : -1));
	check(++outputcalls < 4096 && bytes > 0);
	address = (uintptr)buffer;
	base = (uintptr)display.buf;
	check(address >= base && address-base <= sizeof(display.buf));
	check((uintptr)bytes <= sizeof(display.buf)-(address-base));
	if(outputfault == 1)
		return 0;
	if(outputfault == 2 || (outputfault == 4 && outputlength != 0)){
		diagnostic = 2;
		return -1;
	}
	if(outputfault == 3)
		return bytes+1;
	amount = bytes < outputchunk ? bytes : outputchunk;
	check(outputlength+amount <= sizeof(output));
	memmove(output+outputlength, buffer, amount);
	outputlength += amount;
	return amount;
}

REFRESH_IMPLEMENTATIONS


/* Verify actual decoded callback values against independent Python wire fixtures. */
static void
notification(Image *image, Rectangle rectangle, void *context)
{
	int index, id, ordinal;

	ordinal = callbacks++;
	if(phase == 1){
		while(nextrecord < inputlength/20 && !events[nextrecord].deliver)
			nextrecord++;
		check(nextrecord < inputlength/20);
		index = events[nextrecord].id-7;
		check(image == &windows[index] && context == &windows[index]);
		check(memcmp(&rectangle, &events[nextrecord].rectangle, sizeof(rectangle)) == 0);
		check(locked == 1);
		nextrecord++;
		if(closeoncallback) display.closing = 1;
	}else{
		id = *(int*)context;
		check(image == nil && live == (id == 1 || (id == 2 && reentrant) ? 1 : 0));
		check(ordinal < nelem(sequence));
		sequence[ordinal] = id;
		if(reentrant && id == 1){
			static int third = 3; // Payload remains stable through nested native flushing.
			queuerefresh(&windows[0], rectangle, notification, &third);
			check(display.reftail->next == nil);
			check(flushimage(&display, 1) == 1);
		}
	}
}


/* Reset only after all private queue storage has been retired. */
static void
reset(void)
{
	int i;

	check(live == 0 && balance == 0 && locked == 0);
	memset(&display, 0, sizeof(display));
	memset(windows, 0, sizeof(windows));
	display.bufp = display.buf;
	display.refchan = &inputlength;
	display.datachan = &outputlength;
	display.windows = windows;
	for(i=0; i<nelem(windows); i++){
		windows[i].id = 7+i;
		windows[i].display = &display;
		windows[i].screen = i == 2 ? nil : &screen;
		windows[i].reffn = i == 3 ? nil : notification;
		windows[i].refptr = &windows[i];
		windows[i].next = i+1 == nelem(windows) ? nil : &windows[i+1];
	}
	ownerok = 1;
	closeonread = closeoncallback = 0;
	refuse = inputlength = inputposition = inputcalls = terminal = oversized = 0;
	inputchunk = 100;
	outputchunk = 7;
	outputlength = outputcalls = outputfault = phase = nextrecord = callbacks = reentrant = diagnostic = 0;
}


/* Enumerate every prefix and chunk size, plus irregular partitions and partial-record EOF. */
static void
streams(void)
{
	int length, chunk, end, expected, i;

	for(length=0; length<=sizeof(wire); length++)
	for(chunk=0; chunk<=101; chunk++)
	for(end=0; end<2; end++){
		reset();
		phase = 1;
		inputlength = length;
		inputchunk = chunk;
		terminal = -end;
		refreshslave(&display);
		expected = 0;
		for(i=0; i<length/20; i++)
			expected += events[i].deliver;
		check(callbacks == expected && inputposition == length);
		check(balance == 0 && locked == 0 && live == 0);
		if(end == 0 && length%20 != 0)
			check(diagnostic == 1);
		if(end == 1)
			check(diagnostic == 2);
		for(i=0; i<outputlength; i++)
			check(output[i] == 'v');
	}
	reset();
	oversized = 1;
	refreshslave(&display);
	check(callbacks == 0 && outputlength == 0 && balance == 0 && locked == 0);
	for(end=2; end<=4; end+=2){
		reset();
		phase = 1;
		inputlength = sizeof(wire);
		inputchunk = 20;
		outputfault = end;
		refreshslave(&display);
		check(inputposition == (end == 2 ? 20 : 40));
		check(callbacks == (end == 2 ? 1 : 2));
		check(balance == 0 && locked == 0 && diagnostic == 2);
	}
}


/* A close phase stops new callbacks while the caller still retains Display storage. */
static void
closestreams(void)
{
	reset();
	display.closing = 1;
	refreshslave(&display);
	check(inputcalls == 0 && callbacks == 0 && outputcalls == 0);
	reset();
	phase = 1; inputlength = sizeof(wire); closeonread = 1;
	refreshslave(&display);
	check(inputcalls == 1 && callbacks == 0 && outputcalls == 0);
	check(balance == 0 && locked == 0);
	reset();
	phase = 1; inputlength = sizeof(wire); closeoncallback = 1;
	refreshslave(&display);
	check(inputcalls == 1 && callbacks == 1);
	check(balance == 0 && locked == 0 && display.closing);
}


/* Verify buffer capacity, short-write completion, owner guards and failure diagnostics. */
static void
buffers(void)
{
	static const int sizes[] = {-1, 0, 1, Displaybufsize-1, Displaybufsize, Displaybufsize+1}; // Payload boundary cases.
	int local, fault, length, i, index;
	uchar *cursor;

	for(local=0; local<2; local++)
	for(fault=0; fault<5; fault++){
		reset();
		display.local = local;
		outputfault = fault;
		for(i=0; i<sizeof(display.buf); i++)
			display.buf[i] = i*17+3;
		display.bufp = display.buf+Displaybufsize;
		if(fault){
			check(flushimage(&display, 1) == -1);
			check(display.bufp == display.buf && balance == 0);
			if(fault == 2 || fault == 4)
				check(diagnostic == 2);
			for(i=0; i<outputlength; i++)
				check(output[i] == (uchar)(i*17+3));
		}else{
			check(flushimage(&display, 1) == 1);
			check(outputlength == Displaybufsize+1 && output[Displaybufsize] == 'v');
			for(i=0; i<Displaybufsize; i++)
				check(output[i] == (uchar)(i*17+3));
		}
	}
	for(index=0; index<nelem(sizes); index++){
		length = sizes[index];
		reset();
		cursor = display.bufp;
		if(length < 0 || length > Displaybufsize)
			check(bufimage(&display, length) == nil && display.bufp == cursor);
		else
			check(bufimage(&display, length) == cursor && display.bufp == cursor+length);
	}
	reset();
	display.bufp = display.buf+Displaybufsize;
	ownerok = 0;
	check(bufimage(&display, 0) == display.buf+Displaybufsize);
	check(bufimage(&display, 1) == nil && outputcalls == 0);
	ownerok = 1;
	check(bufimage(&display, 1) == display.buf && display.bufp == display.buf+1);
	reset();
	display.bufp = (uchar*)((uintptr)display.buf-1);
	check(bufimage(&display, 1) == nil && flushimage(&display, 1) == -1);
	check(outputcalls == 0);
	reset();
	display.bufp = (uchar*)((uintptr)display.buf+sizeof(display.buf)+1);
	check(bufimage(&display, 1) == nil && flushimage(&display, 0) == -1);
	check(outputcalls == 0);
	reset();
	display.bufp = display.buf+sizeof(display.buf);
	check(bufimage(&display, 0) == nil && flushimage(&display, 1) == -1 && outputcalls == 0);
	check(flushimage(&display, 0) == 1 && outputlength == sizeof(display.buf));
}


/* Poisoned queue storage, refused allocation and callback reentry use actual production bodies. */
static void
queues(void)
{
	int first, second;
	Refreshq *head, *tail;
	Rectangle rectangle;

	first = 1; second = 2;
	memset(&rectangle, 0, sizeof(rectangle));
	reset();
	queuerefresh(&windows[0], rectangle, nil, &first);
	check(live == 0 && display.refhead == nil && display.reftail == nil);
	queuerefresh(&windows[0], rectangle, notification, &first);
	check(live == 1 && display.refhead == display.reftail && display.reftail->next == nil);
	head = display.refhead; tail = display.reftail;
	refuse = 1;
	queuerefresh(&windows[0], rectangle, notification, &second);
	check(live == 1 && display.refhead == head && display.reftail == tail);
	refuse = 0;
	queuerefresh(&windows[0], rectangle, notification, &second);
	check(live == 2 && display.refhead == head && head->next == display.reftail && display.reftail->next == nil);
	head = display.refhead; tail = display.reftail;
	outputfault = 2;
	check(flushimage(&display, 1) == -1);
	check(live == 2 && callbacks == 0 && display.refhead == head && display.reftail == tail);
	outputfault = 0;
	reentrant = 1;
	check(flushimage(&display, 1) == 1);
	check(live == 0 && callbacks == 3 && display.refhead == nil && display.reftail == nil);
	check(sequence[0] == 1 && sequence[1] == 2 && sequence[2] == 3);
	reset();
	queuerefresh(&windows[0], rectangle, notification, &first);
	queuerefresh(&windows[0], rectangle, notification, &second);
	check(flushimage(&display, 0) == 1);
	check(live == 0 && callbacks == 2 && outputcalls == 0);
	check(sequence[0] == 1 && sequence[1] == 2);
}


/* Execute stream, output and queue contracts with real native Draw declarations. */
int
main(void)
{
	streams();
	closestreams();
	buffers();
	queues();
	printf("PASS native64 refresh: %lu predicates; fragmented records, native buffers and reentrant queue retirement\n", checks);
	return 0;
}
