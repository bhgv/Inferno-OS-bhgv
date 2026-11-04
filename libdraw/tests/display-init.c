#include "lib9.h"
#include "draw.h"
#include "kernel.h"
#include <stdio.h>
#include <stdarg.h>

#define Blocks 128 // Exact native allocation slots, including poisoned headers and guard bytes.
#define Descriptors 32 // Independent raw descriptors and retained channel references.
#define up nil // Diagnostic-only process identity; no VM process or Heap is fabricated.
#define currun() nil // Diagnostic-only program identity at the native display boundary.

struct Block {
	void *pointer; // Actual malloc storage, followed by guard bytes.
	ulong bytes; // Exact payload capacity.
	int kind; // Ordinary1, lock2, default image3.
};

struct Descriptor {
	int raw; // One live original descriptor owner.
	int channels; // Independently retained channel references.
};

struct TestLock {
	int held; // Native exclusion state, independent from allocation identity.
};

static struct Block blocks[Blocks]; // Native ownership ledger only.
static struct Descriptor descriptors[Descriptors]; // Explicit OS/channel service boundary.
static int checks, cases, live, imageowners, operation, failstep, firstopen, fallback, statlocal, statsuccess, readmode, malformed; // Independent fault schedule.
static int directory = 7; // Device-selected draw namespace number.
int _drawdebug; // Debug printing is not exercised by the ownership fixture.
static char diagnostic[ERRMAX]; // Primary error must survive deliberately clobbering cleanup.
static char *devname = "/dev"; // Expected device prefix for this scenario.
static char *winname = "/dev"; // Independently owned window-directory prefix.


/* Stop before an unsafe omission, releasing only actual fixture-owned memory. */
static void
check(int condition, char *stage)
{
	int i;

	if(!condition){
		fprintf(stderr,"Display init mismatch: %s\n",stage);
		for(i=0; i<Blocks; i++)
			free(blocks[i].pointer);
		exit(10);
	}
	checks++;
}


/* Capture formatted native diagnostics without executing a VM or device error callback. */
void
kwerrstr(char *format, ...)
{
	va_list arguments;

	va_start(arguments,format);
	vsnprintf(diagnostic,sizeof(diagnostic),format,arguments);
	va_end(arguments);
}


/* Copy the exact current diagnostic before any cleanup operation overwrites it. */
void
kgerrstr(char *buffer, unsigned int bytes)
{
	if(bytes != 0)
		snprintf(buffer,bytes,"%s",diagnostic);
}


/* Inject each allocation/open/read/conversion operation independently. */
static int
refuse(char *stage)
{
	if(++operation == failstep){
		kwerrstr("injected %s",stage);
		return 1;
	}
	return 0;
}


/* Locate an exact native owner before its fields can be read or its storage released. */
static struct Block*
owner(void *pointer, int kind)
{
	int i;

	for(i=0; i<Blocks; i++)
		if(blocks[i].pointer == pointer && blocks[i].kind == kind)
			return blocks+i;
	check(0,"native owner identity");
	return nil;
}


/* Poison all newly allocated bytes and retain the exact requested capacity. */
static void*
allocate(ulong bytes, int kind, char *stage)
{
	uchar *p;
	int i;

	if(refuse(stage))
		return nil;
	p = malloc(bytes+32);
	check(p != nil,"fixture allocation");
	memset(p,0xa5,bytes+32);
	for(i=0; i<Blocks && blocks[i].pointer != nil; i++)
		;
	check(i < Blocks,"fixture capacity");
	blocks[i].pointer = p; blocks[i].bytes = bytes; blocks[i].kind = kind;
	live++;
	return p;
}


/* Consume exactly one native allocation and preserve its guard checks independently of sanitizers. */
static void
retire(void *p, int kind)
{
	struct Block *block;
	uchar *bytes;
	int i;

	if(p == nil)
		return;
	block = owner(p,kind);
	bytes = p;
	for(i=0; i<32; i++)
		check(bytes[block->bytes+i] == 0xa5,"allocation guards");
	block->pointer = nil;
	live--;
	free(p);
	kwerrstr("cleanup erased diagnostic");
}


/* The producer owns a full independent path, including when dev and win alias. */
static char*
pathcopy(char *text)
{
	char *copy;

	copy = allocate(strlen(text)+1,1,"path allocation");
	if(copy != nil)
		strcpy(copy,text);
	return copy;
}


/* Allocate a real native lock owner, with no mutex/worker implementation claim. */
void*
libqlalloc(void)
{
	struct TestLock *lock;

	lock = allocate(sizeof(*lock),2,"lock allocation");
	if(lock != nil)
		lock->held = 0;
	return lock;
}


/* Require native exclusion to be released before its lock storage disappears. */
void
libqlfree(void *pointer)
{
	struct TestLock *lock;

	owner(pointer,2);
	lock = pointer;
	check(lock->held == 0,"lock release");
	retire(pointer,2);
}


/* Model the initial exclusion retained on successful native display return. */
void
libqlock(void *pointer)
{
	struct TestLock *lock;

	owner(pointer,2);
	lock = pointer;
	check(lock->held == 0,"lock acquisition");
	lock->held = 1;
}


/* Release an acquired native lock exactly once. */
void
libqunlock(void *pointer)
{
	struct TestLock *lock;

	owner(pointer,2);
	lock = pointer;
	check(lock->held == 1,"unlock state");
	lock->held = 0;
}


/* Return descriptor zero too; raw and retained-channel owners are separate obligations. */
int
libopen(char *path, int mode)
{
	char expected[128];
	int i, control;

	snprintf(expected,sizeof(expected),"%s/draw/new",devname);
	control = strcmp(path,expected) == 0;
	if(!control){
		snprintf(expected,sizeof(expected),"%s/draw/%d/%s",devname,directory,mode == OREAD ? "refresh" : "data");
		check(strcmp(path,expected) == 0,"native path");
	}
	check(mode == ORDWR || mode == OREAD,"open mode");
	if(refuse("descriptor open"))
		return -1;
	if(control && fallback && firstopen++ == 0){
		kwerrstr("forced first open");
		return -1;
	}
	for(i=0; i<Descriptors && (descriptors[i].raw || descriptors[i].channels); i++)
		;
	check(i < Descriptors,"descriptor capacity");
	descriptors[i].raw = 1;
	return i;
}


/* Exercise fallback bind and retry without changing the native service's namespace. */
int
libbind(char *source, char *target, int mode)
{
	check(strcmp(source,"#i") == 0 && strcmp(target,devname) == 0 && mode == MBEFORE,"bind arguments");
	return refuse("bind") ? -1 : 0;
}


/* Build exact twelve-byte fields independently from the production parser. */
int
libread(int fd, void *buffer, int bytes)
{
	char record[144], field[13];
	int values[12] = {0,0,0,0,-10,-20,300,200,-10,-20,300,200};
	int i;

	check(fd >= 0 && fd < Descriptors && descriptors[fd].raw && bytes == 145,"read arguments");
	if(refuse("information read"))
		return -1;
	values[0] = malformed == 1 ? -1 : directory;
	values[3] = malformed == 2 ? 2 : 0;
	for(i=0; i<12; i++){
		if(i == 2)
			snprintf(field,sizeof(field),"%11s ",malformed == 3 ? "g0" : "r8g8b8a8");
		else
			snprintf(field,sizeof(field),"%11d ",values[i]);
		memmove(record+12*i,field,12);
	}
	memmove(buffer,record,144);
	if(readmode == 1) return 0;
	if(readmode == 2) return 143;
	if(readmode == 3) return 145;
	return 144;
}


/* Retain a channel without consuming the original descriptor owner. */
void*
libfdtochan(int fd, int mode)
{
	check(fd >= 0 && fd < Descriptors && descriptors[fd].raw && (mode == OREAD || mode == ORDWR),"channel conversion arguments");
	if(refuse("channel conversion"))
		return nil;
	descriptors[fd].channels++;
	return descriptors+fd;
}


/* Consume one successful conversion only, including partial conversion prefixes. */
void
libchanclose(void *channel)
{
	int i;

	if(channel == nil)
		return;
	for(i=0; i<Descriptors && channel != descriptors+i; i++)
		;
	check(i < Descriptors && descriptors[i].channels > 0,"channel retirement");
	descriptors[i].channels--;
	kwerrstr("cleanup erased diagnostic");
}


/* Consume the raw descriptor once; a retained channel may remain independently live. */
int
libclose(int fd)
{
	check(fd >= 0 && fd < Descriptors && descriptors[fd].raw == 1,"descriptor retirement");
	descriptors[fd].raw = 0;
	kwerrstr("cleanup erased diagnostic");
	return 0;
}


/* Optional fstat distinguishes local displays and preserves a high-bit native qid. */
Dir*
libdirfstat(int fd)
{
	Dir *dir;

	check(fd >= 0 && fd < Descriptors && descriptors[fd].raw,"stat descriptor");
	dir = allocate(sizeof(*dir),1,"optional stat");
	statsuccess = dir != nil;
	if(dir != nil){
		memset(dir,0,sizeof(*dir));
		dir->type = statlocal ? 'i' : 'x';
		dir->qid.path = 0x1234567887654321ULL;
	}
	return dir;
}


/* Allocate only a default-image service owner while its data channel and lock stay live. */
Image*
allocimage(Display *d, Rectangle r, ulong channel, int repl, ulong color)
{
	Image *image;
	struct TestLock *lock;

	USED(color);
	owner(d,1); owner(d->qlock,2);
	lock = d->qlock;
	check(lock->held && d->datachan != nil && d->refchan != nil && d->ctlchan != nil,"image prerequisites");
	check(r.min.x == 0 && r.min.y == 0 && r.max.x == 1 && r.max.y == 1 && channel == GREY1 && repl == 1,"default image arguments");
	image = allocate(sizeof(*image),3,"default image allocation");
	if(image != nil){
		memset(image,0,sizeof(*image));
		image->display = d; image->chan = channel; image->depth = 1;
		image->repl = repl; image->r = r; image->clipr = r;
		image->id = ++d->imageid;
		imageowners++;
	}
	return image;
}


/* Native release consumes its header even when cleanup I/O reports failure. */
int
freeimage(Image *image)
{
	struct TestLock *lock;

	if(image == nil)
		return 0;
	owner(image,3);
	owner(image->display,1);
	lock = image->display->qlock;
	owner(lock,2);
	check(lock->held && imageowners > 0,"image retirement exclusion");
	imageowners--;
	retire(image,3);
	return -1;
}


/* Display labels are empty in these scenarios; any unexpected external write is a failed fixture premise. */
int
libwrite(int fd, void *buffer, int bytes)
{
	USED(fd); USED(buffer); USED(bytes);
	check(0,"unexpected label write");
	return -1;
}


/* Keep only ordinary path formatting at an explicit native service boundary. */
int
sprint(char *buffer, char *format, ...)
{
	va_list arguments;
	int count;

	va_start(arguments,format);
	count = vsnprintf(buffer,128,format,arguments);
	va_end(arguments);
	check(count >= 0 && count < 128,"path extent");
	return count;
}


/* Supply bounded printf formatting for the optional close-label branch. */
int
snprint(char *buffer, int bytes, char *format, ...)
{
	va_list arguments;
	int count;

	va_start(arguments,format);
	count = vsnprintf(buffer,bytes,format,arguments);
	va_end(arguments);
	return count;
}


/* Diagnostic formatting registration does not allocate native display owners here. */
int
fmtinstall(int letter, int (*format)(Fmt*))
{
	check((letter == 'P' && format == Pfmt) || (letter == 'R' && format == Rfmt),"format registration");
	return 0;
}


/* Point diagnostics are outside this constructor ownership test. */
int
Pfmt(Fmt *format)
{
	USED(format);
	return 0;
}


/* Rectangle diagnostics are outside this constructor ownership test. */
int
Rfmt(Fmt *format)
{
	USED(format);
	return 0;
}


/* Debug output is an explicit service; no VM process state is read. */
int
print(char *format, ...)
{
	USED(format);
	return 0;
}

#define malloc(n) allocate(n,1,"native header allocation") // Preserve actual requested native capacities.
#define free(p) retire(p,1) // Observe root-image/display/path/Dir retirement only.
#define strdup(p) pathcopy(p) // Observe each independent owned directory copy.
IMPLEMENTATION
#undef malloc
#undef free
#undef strdup


/* Require complete independent conservation after either return path. */
static void
baseline(void)
{
	int i;

	for(i=0; i<Descriptors; i++){
		check(descriptors[i].raw == 0,"descriptor baseline");
		check(descriptors[i].channels == 0,"channel baseline");
	}
	check(imageowners == 0,"image baseline");
	check(live == 0,"memory baseline");
}


/* Validate the adopted graph, release its initial exclusion and run the actual close path. */
static void
finish(Display *d)
{
	struct TestLock *lock;

	check(d->image != nil && d->white != nil && d->black != nil && d->opaque != nil && d->transparent != nil,"complete defaults");
	check(strcmp(d->devdir,devname) == 0 && strcmp(d->windir,winname) == 0,"owned paths");
	check(d->devdir != d->windir,"independent directory owners");
	check(d->chan == RGBA32 && d->depth == 32 && d->image->id == 0 && d->image->repl == 0,"display metadata");
	check(d->local == (!CHECKLOCKING && statsuccess && statlocal),"local display flag");
	check(d->image->chan == RGBA32 && d->image->depth == 32 && d->dirno == directory,"parsed metadata");
	check(d->image->r.min.x == -10 && d->image->r.min.y == -20 && d->image->r.max.x == 300 && d->image->r.max.y == 200,"root geometry");
	check(d->image->reffn == nil && d->image->fpath == nil && d->image->screen == nil && d->image->next == nil,"root initialization");
	check(d->defaultfont == nil && d->defaultsubfont == nil && d->limbo == nil && d->refhead == nil && d->reftail == nil,"display initialization");
	check(d->datachan != nil && d->refchan != nil && d->ctlchan != nil,"complete channels");
	check(d->bufp == d->buf && d->bufsize == Displaybufsize,"display buffer");
	if(d->local)
		check(d->dataqid == 0x1234567887654321ULL,"native qid width");
	lock = d->qlock;
	check(lock->held == 1,"success lock");
	libqunlock(lock);
	closedisplay(d);
}


/* Inject one constructor operation and verify all actual owners retire on either outcome. */
static int
onecase(int failure)
{
	Display *d;
	int total;

	baseline();
	operation = firstopen = statsuccess = 0;
	failstep = failure;
	diagnostic[0] = 0;
	d = initdisplay(devname,winname,nil);
	total = operation;
	if(d != nil)
		finish(d);
	else
		check(strstr(diagnostic,"cleanup") == nil &&
			(strstr(diagnostic,"injected") != nil || strstr(diagnostic,"invalid") != nil || strstr(diagnostic,"forced first open") != nil),"diagnostic preservation");
	baseline();
	cases++;
	return total;
}


/* Enumerate every reachable normal/fallback allocation and I/O refusal with active predicates. */
int
main(void)
{
	Display *first, *second;
	char longname[105];
	int total, step, mode;

	check(sizeof(void*) == 8 && sizeof(ulong) == 8,"native model");
	for(mode=0; mode<2; mode++){
		devname = mode ? "/custom/device" : "/dev";
		winname = devname;
		for(statlocal=0; statlocal<2; statlocal++)
		for(fallback=0; fallback<2; fallback++){
			total = onecase(0);
			for(step=1; step<=total; step++)
				onecase(step);
		}
	}
	fallback = 0; failstep = 0; statlocal = 1;
	for(readmode=1; readmode<=3; readmode++)
		onecase(0);
	readmode = 0;
	for(malformed=1; malformed<=3; malformed++)
		onecase(0);
	malformed = 0;
	memset(longname,'x',103); longname[103] = 0;
	devname = winname = longname; directory = 2147483647;
	onecase(0);
	devname = "/dev"; winname = "/other/window"; directory = 7;
	onecase(0);
	winname = devname;
	memset(longname,'x',104); longname[104] = 0;
	operation = 0;
	check(initdisplay(longname,nil,nil) == nil && operation == 0,"oversized device prefix");
	check(initdisplay(nil,longname,nil) == nil && operation == 0,"oversized window prefix");
	operation = firstopen = 0; failstep = 0;
	first = initdisplay(nil,nil,nil);
	second = initdisplay(nil,nil,nil);
	check(first != nil && second != nil && first != second && first->qlock != second->qlock,"independent displays");
	finish(first);
	check(imageowners == 4 && live > 0,"second display survives");
	finish(second);
	baseline();
	printf("PASS native display initialization: %d active checks, %d refusal/success scenarios; native allocation/FD/channel/lock ledgers zero\n",checks,cases);
	return 0;
}
