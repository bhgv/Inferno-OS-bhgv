#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "kernel.h"
#include "draw.h"
#include "memdraw.h"
#include "memlayer.h"
#include <stdio.h>
#include <stdarg.h>

static Display *active; // Real native display allocation retained until the actual close body frees it.

static struct {
	void *pointer; // Exact native allocation identity.
	int kind; // Distinguish queue records, resources, channels and caller-owned payloads.
} allocations[64];
static int held, retiredlock, closes, images, callbacks, allocationsmade; // Observable teardown ordering.
static int labelmode, attempts, cancelling, balance, diagnostic, writes; // Bounded failure and cancellation state.
static Image *watched; // Callback publication observed at the display-lock boundary.
static void *watchedcontext; // Original context that must remain published until exclusion is acquired.
static Prog owner; // Bounded interpreter owner identity, no scheduler emulation.
static unsigned long checks; // Active predicates under NDEBUG.
int _drawdebug; // Existing diagnostic configuration.
char exImage[] = "image failure"; // Existing native error identity.


/* Retire only exact fixture allocations before a deliberate semantic stop. */
static void
checkat(int condition, int line)
{
	int i;

	if(!condition){
		fprintf(stderr, "Display close mismatch at %lu line %d\n", checks, line);
		for(i=0; i<nelem(allocations); i++)
			if(allocations[i].pointer != nil)
				free(allocations[i].pointer);
		exit(10);
	}
	checks++;
}

#define check(value) checkat((value), __LINE__) // All semantic checks remain active in optimized builds.


/* Allocate actual poisoned storage; queue nodes must initialize their own links. */
static void*
ownerallocate(ulong bytes, int kind)
{
	void *pointer;
	int i;

	pointer = malloc(bytes);
	check(pointer != nil);
	memset(pointer, 0xa5, bytes);
	for(i=0; i<nelem(allocations); i++)
		if(allocations[i].pointer == nil){
			allocations[i].pointer = pointer;
			allocations[i].kind = kind;
			allocationsmade++;
			return pointer;
		}
	check(0);
	return nil;
}


/* Free each native identity once; payload owners and default fonts stay independent. */
static void
ownerfree(void *pointer)
{
	int i;

	if(pointer == nil)
		return;
	for(i=0; i<nelem(allocations); i++)
		if(allocations[i].pointer == pointer){
			if(pointer == active){
				check(active->closing && active->refhead == nil && active->reftail == nil);
				check(held == 0 && retiredlock == 1 && closes == 3);
				active = nil;
			}
			allocations[i].pointer = nil;
			free(pointer);
			return;
		}
	check(0);
}


/* Count exact resource classes without dereferencing already retired objects. */
static int
live(int kind)
{
	int i, count;

	count = 0;
	for(i=0; i<nelem(allocations); i++)
		if(allocations[i].pointer != nil && allocations[i].kind == kind)
			count++;
	return count;
}


/* Preserve a bounded diagnostic rather than changing teardown control flow. */
void
kwerrstr(char *format, ...)
{
	USED(format);
	diagnostic++;
}


/* Supply the existing diagnostic transport, never a native failure callback. */
void
kgerrstr(char *text, uint bytes)
{
	check(bytes > 20);
	strcpy(text, "bounded close transport");
}


/* Avoid interpreting native Inferno format extensions as libc formats. */
int
print(char *format, ...)
{
	USED(format);
	return 0;
}


/* Preserve the actual close label formatting with a standard compatible format. */
int
snprint(char *buffer, int bytes, char *format, ...)
{
	va_list args;
	int result;

	va_start(args, format);
	result = vsnprintf(buffer, bytes, format, args);
	va_end(args);
	return result;
}


/* Observe the native VM owner boundary used by the actual display lock routines. */
Prog*
currun(void)
{
	return &owner;
}


/* Publish callback cancellation only after native display exclusion has been acquired. */
void
libqlock(void *lock)
{
	check(lock == &held && held == 0);
	if(cancelling)
		check(watched->refptr == watchedcontext);
	held = 1;
}


/* Match the actual lockdisplay reentry convention. */
void*
libqlowner(void *lock)
{
	check(lock == &held);
	return held ? &owner : nil;
}


/* No native close/cancel path may leak a display lock. */
void
libqunlock(void *lock)
{
	check(lock == &held && held == 1);
	held = 0;
}


/* Retire the lock only after channels and private notifications are gone. */
void
libqlfree(void *lock)
{
	check(lock == &held && held == 0 && active->refhead == nil && live(1) == 0);
	retiredlock++;
}


/* Keep ordinary remote output transitions balanced in cleanup flushes. */
void
release(void)
{
	check(balance == 0);
	balance--;
}


/* Restore exclusion before cleanup continues to touch Display. */
void
acquire(void)
{
	check(balance == -1);
	balance++;
}


/* Complete native output without invoking the VM or a queue callback. */
long
kchanio(void *channel, void *buffer, int bytes, int operation)
{
	USED(channel); USED(buffer);
	check(operation == OWRITE && bytes > 0 && channel == active->datachan && live(3) == 3);
	writes++;
	check(balance == (active->local ? 0 : -1));
	return bytes;
}


/* A real cancellation must disable local refresh before freeing its context. */
int
drawlsetrefresh(ulong qid, int id, void *function, void *context)
{
	USED(qid); USED(id);
	check(function == memlnorefresh && context == nil);
	check(watched->refptr == watchedcontext);
	return 1;
}


/* Provide the real no-refresh callback identity without a memlayer device. */
void
memlnorefresh(Memimage *image, Rectangle rectangle, void *context)
{
	USED(image); (void)rectangle; USED(context);
}

CLOSE_IMPLEMENTATIONS


/* Borrowed callback payloads may be poisoned only after cancellation has removed their entries. */
static void
notification(Image *image, Rectangle rectangle, void *context)
{
	USED(image); (void)rectangle;
	check(active != nil && !active->closing && context != nil);
	check(*(int*)context >= 0 && *(int*)context < 3);
	callbacks++;
}


/* Cleanup destructors attempt fresh production enqueues and cleanup flushes while closing. */
static void
attempt(void)
{
	Image temporary;
	Rectangle rectangle;
	int before;

	check(active->closing && held == 1 && active->refhead == nil && active->reftail == nil);
	memset(&temporary, 0, sizeof(temporary));
	memset(&rectangle, 0, sizeof(rectangle));
	temporary.display = active;
	before = allocationsmade;
	queuerefresh(&temporary, rectangle, notification, &attempts);
	check(allocationsmade == before && live(1) == 0);
	check(flushimage(active, 0) == 1 && callbacks == 0);
	attempts++;
}


/* Keep image destruction bounded while executing the actual close ordering around it. */
int
freeimage(Image *image)
{
	uchar *command;

	if(image == nil)
		return 0;
	attempt();
	command = bufimage(active, 1);
	check(command != nil);
	*command = 'f';
	check(flushimage(active, 0) == 1);
	images++;
	ownerfree(image);
	return 0;
}


/* Channel cleanup can try to enqueue too; close admission must already be disabled. */
void
libchanclose(void *channel)
{
	attempt();
	closes++;
	ownerfree(channel);
}


/* Failed label restoration must not skip any subsequent ownership retirement. */
int
libopen(char *name, int flags)
{
	check(strcmp(name, "close/label") == 0 && flags == OWRITE && active->closing && held == 1);
	return labelmode == 1 ? -1 : 7;
}


/* An ordinary native write refusal remains a best-effort label failure. */
int
libwrite(int fd, void *data, int bytes)
{
	check(fd == 7 && data == active->oldlabel && bytes == 3 && active->closing);
	return labelmode == 2 ? -1 : bytes;
}


/* Close label descriptors exactly when the actual open succeeded. */
int
libclose(int fd)
{
	check(fd == 7);
	return 0;
}


/* Create actual allocated resources; private queue nodes and caller payloads have separate ledgers. */
static void
setup(int complete)
{
	check(active == nil && live(0)+live(1)+live(2)+live(3)+live(4) == 0 && held == 0 && balance == 0);
	active = ownerallocate(sizeof(Display), 0);
	memset(active, 0, sizeof(Display));
	active->qlock = &held;
	active->bufp = active->buf;
	active->devdir = ownerallocate(8, 2);
	active->windir = ownerallocate(8, 2);
	strcpy(active->devdir, "draw"); strcpy(active->windir, "close");
	active->datachan = ownerallocate(8, 3);
	active->refchan = ownerallocate(8, 3);
	active->ctlchan = ownerallocate(8, 3);
	if(complete){
		active->white = ownerallocate(sizeof(Image), 2);
		active->black = ownerallocate(sizeof(Image), 2);
		active->opaque = ownerallocate(sizeof(Image), 2);
		active->transparent = ownerallocate(sizeof(Image), 2);
		active->image = ownerallocate(sizeof(Image), 2);
	}
	retiredlock = closes = callbacks = images = attempts = diagnostic = writes = 0;
	watched = nil;
	cancelling = 0;
}


/* Cover duplicates, head/middle/tail/all/no-match/nil cancellation under actual native lock bodies. */
static void
cancellation(void)
{
	static const int order[] = {0, 1, 0, 2, 0}; // Duplicate context at every queue position.
	int local, target, context[3], i, survivors, pattern, count, key;
	Image image;
	Rectangle rectangle;

	memset(&rectangle, 0, sizeof(rectangle));
	for(local=0; local<2; local++)
	for(pattern=0; pattern<4; pattern++)
	for(target=0; target<5; target++){
		setup(0);
		active->local = local;
		for(i=0; i<3; i++) context[i] = i;
		memset(&image, 0, sizeof(image));
		image.display = active;
		image.reffn = notification;
		count = pattern == 2 ? 1 : (pattern == 3 ? 0 : nelem(order));
		for(i=0; i<count; i++){
			key = pattern == 1 ? 0 : (pattern == 2 ? 1 : order[i]);
			queuerefresh(&image, rectangle, notification, &context[key]);
		}
		image.refptr = target < 3 ? &context[target] : (target == 3 ? &survivors : nil);
		watched = &image; watchedcontext = image.refptr; cancelling = 1;
		delrefresh(&image);
		cancelling = 0;
		check(image.refptr == nil && image.reffn == nil && held == 0);
		survivors = 0;
		for(i=0; i<count; i++){
			key = pattern == 1 ? 0 : (pattern == 2 ? 1 : order[i]);
			survivors += key != target;
		}
		check(live(1) == survivors);
		if(target < 3) context[target] = -1;
		check(flushimage(active, 0) == 1 && callbacks == survivors && live(1) == 0);
		callbacks = 0;
		closedisplay(active);
		check(active == nil && live(0)+live(1)+live(2)+live(3)+live(4) == 0);
	}
}


/* Queued payloads/default fonts survive close; only private queue/native owned resources retire. */
static void
closing(void)
{
	int complete, labels, count, i, local;
	Image image;
	Rectangle rectangle;
	void *font, *subfont;
	int *payload;

	memset(&rectangle, 0, sizeof(rectangle));
	for(local=0; local<2; local++)
	for(complete=0; complete<2; complete++)
	for(labels=0; labels<4; labels++)
	for(count=0; count<5; count++){
		setup(complete);
		active->local = local;
		payload = ownerallocate(sizeof(int), 4); *payload = 0;
		font = ownerallocate(8, 4); subfont = ownerallocate(8, 4);
		active->defaultfont = font; active->defaultsubfont = subfont;
		labelmode = labels;
		if(labels != 3) strcpy(active->oldlabel, "old");
		memset(&image, 0, sizeof(image)); image.display = active;
		for(i=0; i<count; i++) queuerefresh(&image, rectangle, notification, payload);
		if(labels == 0){
			active->closing = 1;
			check(flushimage(active, 0) == 1 && callbacks == 0 && live(1) == count);
		}
		closedisplay(active);
		check(active == nil && callbacks == 0 && held == 0 && balance == 0);
		check(live(0)+live(1)+live(2)+live(3) == 0 && live(4) == 3);
		check(images == (complete ? 4 : 0) && attempts == images+3 && writes == images);
		check(*payload == 0);
		ownerfree(payload); ownerfree(font); ownerfree(subfont);
	}
	closedisplay(nil);
}


/* Exercise owning close/cancellation code without claiming an asynchronous worker join. */
int
main(void)
{
	cancellation();
	closing();
	printf("PASS native64 display close: %lu predicates; native cancellation, close admission and exact resource retirement\n", checks);
	return 0;
}
