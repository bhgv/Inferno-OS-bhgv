#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "draw.h"
#include "tk.h"
#include "pool.h"
#define require(condition) do { if(!(condition)){ fprintf(stderr, "Tk owner check failed at %d\n", __LINE__); abort(); } } while(0) // Keep checks active under NDEBUG.
#include <stdio.h>

static Proc worker;
static char *raised;
char exHeap[] = "heap", exNegsize[] = "negative", exBounds[] = "bounds", exNomem[] = "memory", exNilref[] = "nil";
Type Tstring = {1};
static Type topType = {1, 0, 0, sizeof(TkTop)}, channelType = {1, 0, 0, sizeof(Channel)};
static Type *fakeTkTop = &topType;
Type Tptr = {1};
static Prog program;
static int ownerlive = 1; // Registry presence may disappear before a top is collected.
static Type displayType = {1, 0, 0, sizeof(Draw_Display)};
static Display nativeDisplay;
static Image nativeImage;
static TkCtxt topcontext;
static void tkdetachowner(TkTop*);
int mutator, nprop;
static int allocations, failure, live, checks;
static struct { Heap *heap; int size; } blocks[256];

/* Retain real runtime error frames without starting an emulator process. */
Proc*
getup(void)
{
	return &worker;
}


/* Route allocation errors through the runtime's nested nonlocal error frames. */
void
nexterror(void)
{
	require(worker.nerr > 0);
	siglongjmp(worker.estack[--worker.nerr], 1);
}


/* Keep the original VM error observable after cleanup and rethrow. */
void
error(char *message)
{
	raised = message;
	nexterror();
}


/* Heap blocks are counted explicitly; process inspection for LSan is unavailable. */
int
__lsan_is_turned_off(void)
{
	return 1;
}


/* Poison exact payloads and inject failures before VM allocation. */
Heap*
nheap(int size)
{
	Heap *heap;
	int i;

	if(++allocations == failure)
		error(exHeap);
	require(size >= 0 && size < 16384);
	heap = malloc(sizeof(Heap)+size+32);
	require(heap);
	memset(heap, 0, sizeof(Heap));
	memset(heap+1, 0xa5, size+32);
	heap->ref = 1;
	for(i = 0; i < 256 && blocks[i].heap; i++);
	require(i < 256);
	blocks[i].heap = heap;
	blocks[i].size = size;
	live++;
	return heap;
}


/* Expose exact payload bounds to the production string constructors. */
int
hmsize(void *heap)
{
	int i;

	for(i = 0; i < 256; i++)
		if(blocks[i].heap == heap)
			return sizeof(Heap)+blocks[i].size;
	abort();
}


/* Validate guards and model only the weak program-to-top pointer on final release. */
void
destroy(void *value)
{
	Heap *heap;

	int i, j;

	if(value == H)
		return;
	heap = D2H(value);
	for(i = 0; i < 256 && blocks[i].heap != heap; i++);
	require(i < 256 && heap->ref > 0);
	for(j = 0; j < 32; j++)
		require(((uchar*)(heap+1))[blocks[i].size+j] == 0xa5);
	if(--heap->ref)
		return;
	if(heap->t == fakeTkTop){
		TkTop *top = value;
		tkdetachowner(top);
		if(top->root != nil){
			free(top->root->name);
			free(top->root);
		}
		if(top->dd != nil && top->dd != H)destroy(top->dd);
		if(top->wreq != nil && top->wreq != H)destroy(top->wreq);
		topType.ref--;
	}else if(heap->t == &displayType)
		displayType.ref--;
	else if(heap->t == &channelType)
		channelType.ref--;
	else{
		require(heap->t == &Tstring && ((String*)value)->tmp == nil);
		Tstring.ref--;
	}
	blocks[i].heap = nil;
	free(heap);
	live--;
}

static int locked, interleave, variablemode, nativevars;
static TkVar *variable;
static TkCtxt context;
char TkNotop[] = "no top", TkBadvl[] = "bad value", TkNomem[] = "no memory", TkNotvt[] = "not channel";

/* Return the existing program without starting an interpreter scheduler. */
Prog*
currun(void)
{
	return &program;
}


/* Allocate bounded opaque owners using real native record sizes. */
Heap*
heapz(Type *type)
{
	Heap *h;

	h = nheap(type->size);
	h->t = type;
	type->ref++;
	memset(h+1, 0, type->size);
	return h;
}


/* Publishing either a top or a cached display requires a live heap owner. */
void
poolmutable(void *heap)
{
	require(((Heap*)heap)->ref > 0);
	((Heap*)heap)->color = mutator;
}


/* Record a valid immutable allocation before the constructor publishes it. */
void
poolimmutable(void *heap)
{
	require(((Heap*)heap)->ref > 0);
}


/* Input names are ASCII, so no string cache capacity is needed. */
ulong
msize(void *pointer)
{
	(void)pointer;
	abort();
}


/* Supply a live display and distinct native widget records for actual top construction. */
Display*
checkdisplay(Draw_Display *display)
{
	require(display != nil && display != H && D2H(display)->t == &displayType);
	nativeImage.r.max.x = 640;
	nativeImage.r.max.y = 480;
	nativeDisplay.image = &nativeImage;
	return &nativeDisplay;
}


/* Allocate the real Tk/TkWin extent; the fixture releases it with its top. */
Tk*
tknewobj(TkTop *top, int kind, int size)
{
	Tk *widget;

	(void)top;
	require(kind == TKframe && size == sizeof(Tk)+sizeof(TkWin));
	widget = calloc(1, size);
	require(widget != nil);
	return widget;
}


/* Release only an unfinished widget allocation. */
void
tkfreeobj(Tk *widget)
{
	free(widget->name);
	free(widget);
}


/* Window event queues are outside this bounded ownership fixture. */
TkCtxt*
tknewctxt(Display *display)
{
	(void)display;
	return &topcontext;
}


/* Keep the root name independent between newly allocated windows. */
TkName*
tkmkname(char *name)
{
	(void)name;
	return calloc(1, sizeof(TkName)+8);
}


/* Resolve a weak primary owner only while its process is registered. */
Prog*
progpid(int pid)
{
	return ownerlive && pid == program.pid ? &program : nil;
}


/* Window resizing is outside constructor and reference ownership checks. */
void
tkmoveresize(Tk *tk, int a, int b, int c, int d)
{
	(void)tk;
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	abort();
}


/* Constructor options have no side effects in the bounded native-window fixture. */
void
tktopopt(Tk *tk, char *options)
{
	(void)tk;
	(void)options;
}


/* Construct a tracked request channel for each actual new top. */
Channel*
cnewc(Type *type, void (*mover)(void), int length)
{
	(void)type;
	(void)mover;
	require(length == 8);
	return H2D(Channel*, heapz(&channelType));
}


/* The bounded channel fixture records the mover identity without moving messages. */
void
movp(void)
{
	abort();
}


/* Record the context lock without relying on a real native event thread. */
static void
lockctxt(TkCtxt *ctxt)
{
	require(ctxt == &context && !locked);
	locked = 1;
}


/* Deterministically replace or remove the variable as soon as its lock is released. */
static void
unlockctxt(TkCtxt *ctxt)
{
	require(ctxt == &context && locked);
	locked = 0;
	if(interleave && variable){
		destroy(variable->value);
		variable->value = H;
		if(interleave == 2){
			free(variable);
			nativevars--;
			variable = nil;
		}
	}
}


/* Bind unrelated variable lookup/allocation to a controlled record under the lock. */
TkVar*
tkmkvar(TkTop *top, char *name, int type)
{
	require(top && locked && strcmp(name, "chan") == 0 && type == TkVchan);
	return variablemode == 1 ? nil : variable;
}

IMPLEMENTATION

static F_Tk_quote quote;
static String *quoted;

/* Catch expected quote allocation/length failures through real runtime protection. */
static int
quoteinvoke(void *unused)
{
	(void)unused;
	Tk_quote(&quote);
	return 1;
}


/* Build a representation independently of the production quoting algorithm. */
static String*
makestring(Rune *data, int length, int wide)
{
	String *s;
	int i;

	s = wide ? newrunes(length) : newstring(length);
	for(i = 0; i < length; i++)
		if(wide)
			s->Srune[i] = data[i];
		else{
			require(data[i] < 128);
			s->Sascii[i] = data[i];
		}
	return s;
}


/* Check independent character escaping and old-result retention under allocation failure. */
static void
quotecase(Rune *data, int length, int wide, int oldmode, int fail, int nilinput)
{
	Rune expected[128];
	String *old;
	int i, n, ok;

	failure = 0;
	quote.s = nilinput ? H : makestring(data, length, wide);
	old = H;
	if(oldmode == 1)
		old = c2string("previous", 8);
	else if(oldmode == 2 && quote.s != H){
		old = quote.s;
		D2H(old)->ref++;
	}
	quote.ret = &quoted;
	quoted = old;
	expected[0] = '{'; n = 1;
	if(!nilinput)for(i = 0; i < length; i++){
		if(data[i] == '{' || data[i] == '}' || data[i] == '\\')
			expected[n++] = '\\';
		expected[n++] = data[i];
	}
	expected[n++] = '}';
	allocations = 0;
	failure = fail;
	raised = nil;
	ok = kprotect(quoteinvoke, nil);
	failure = 0;
	require(worker.nerr == 0);
	if(fail){
		require(!ok && raised == exHeap && quoted == old);
	}else{
		require(ok && quoted != H);
		require((quoted->len < 0 ? -quoted->len : quoted->len) == n);
		for(i = 0; i < n; i++)
			require((quoted->len < 0 ? quoted->Srune[i] : (uchar)quoted->Sascii[i]) == expected[i]);
	}
	destroy(quoted);
	destroy(quote.s);
	require(live == 0 && Tstring.ref == 1);
	checks++;
}


/* Reject huge output metadata before reading source bytes or allocating a result. */
static void
quotebounds(void)
{
	String forged, *old;
	int lengths[] = {2147483647, -2147483647, (-2147483647-1)};
	int i;

	for(i = 0; i < 3; i++){
		memset(&forged, 0, sizeof(forged));
		forged.len = lengths[i];
		old = quoted = c2string("old", 3);
		quote.ret = &quoted; quote.s = &forged;
		allocations = 0; raised = nil;
		require(!kprotect(quoteinvoke, nil));
		require(raised == exHeap && allocations == 0 && quoted == old);
		destroy(quoted);
		require(!live && Tstring.ref == 1);
		checks++;
	}
}


/* Every API call creates a fresh namespace; secondary destruction cannot detach the primary. */
static void
topcases(void)
{
	F_Tk_toplevel frame = {0};
	Tk_Toplevel *result, *primary;
	Draw_Display *display, *cached;
	String *device;
	int repeat, j;

	program.pid = 17;
	for(repeat = 1; repeat <= 32; repeat++){
		display = H2D(Draw_Display*, heapz(&displayType));
		result = H;
		frame.ret = &result;
		frame.d = display;
		frame.arg = H;
		Tk_toplevel(&frame);
		primary = result;
		require(program.tktop == primary && ((TkTop*)primary)->ownerpid == program.pid);
		D2H(primary)->ref++;
		for(j = 0; j < repeat; j++){
			Tk_toplevel(&frame);
			require(result != primary && program.tktop == primary);
			require(((TkTop*)result)->root != ((TkTop*)primary)->root);
			require(((TkTop*)result)->ownerpid == 0);
			cached = H;
			require(drawcachedisplay(&cached, H) && cached == display);
			require(D2H(display)->ref == 4);
			require(drawcachedisplay(&cached, H) && D2H(display)->ref == 4);
			destroy(cached);
		}
		device = c2string("remote", 6);
		cached = H;
		require(!drawcachedisplay(&cached, device) && cached == H);
		destroy(device);
		destroy(result);
		require(program.tktop == primary);
		destroy(primary);
		require(program.tktop == nil && D2H(display)->ref == 1);
		destroy(display);
		require(!live && topType.ref == 1 && displayType.ref == 1 && channelType.ref == 1);
		checks++;
	}
	primary = (Tk_Toplevel*)H2D(TkTop*, heapz(fakeTkTop));
	((TkTop*)primary)->ownerpid = program.pid;
	program.tktop = primary;
	ownerlive = 0;
	destroy(primary);
	program.tktop = nil;
	ownerlive = 1;
	require(!live);
	checks++;
}


/* Exercise replacement, same-channel assignment and variable deletion immediately on unlock. */
static void
namecases(void)
{
	F_Tk_namechan frame = {0};
	TkTop *top;
	Channel *channel;
	String *result;
	int same;

	for(interleave = 0; interleave < 3; interleave++)for(same = 0; same < 2; same++)
	for(variablemode = 0; variablemode < 3; variablemode++){
		top = H2D(TkTop*, heapz(fakeTkTop)); top->ctxt = &context;
		channel = H2D(Channel*, heapz(&channelType));
		variable = calloc(1, sizeof(TkVar)); require(variable); nativevars++;
		variable->type = variablemode == 2 ? TkVstring : TkVchan;
		variable->value = same ? channel : H2D(Channel*, heapz(&channelType));
		if(same)D2H(channel)->ref++;
		result = H;
		frame.t = (Tk_Toplevel*)top; frame.c = channel;
		frame.n = c2string("chan", 4); frame.ret = &result;
		Tk_namechan(&frame);
		require(!locked && result != H);
		if(variablemode == 0){
			require(D2H(channel)->ref == (interleave ? 1 : 2));
			require(result->len == 0 && D2H(channel)->color == propagator);
		}else{
			char *message = variablemode == 1 ? TkNomem : TkNotvt;
			require(result->len == strlen(message));
			require(memcmp(result->Sascii, message, result->len) == 0);
		}
		destroy(result); destroy(frame.n);
		if(variable){ destroy(variable->value); free(variable); nativevars--; variable = nil; }
		destroy(channel); destroy(top);
		require(!live && !nativevars && channelType.ref == 1 && topType.ref == 1 && Tstring.ref == 1);
		checks++;
	}
	interleave = 0;
}


/* Cross representation, escaped characters, previous owners and allocation failures. */
int
main(int argc, char **argv)
{
	Rune data[32], alphabet[] = {'a', 0, '{', '}', '\\', 0x400, 0x1f600};
	unsigned state = 1;
	int sample, i, length, ascii, wide, old, fail;

	mutator = 1;
	if(argc == 1 || strcmp(argv[1], "quote") == 0){
		for(sample = 0; sample < 2048; sample++){
			length = sample%25; ascii = 1;
			for(i = 0; i < length; i++){
				state = state*1664525U+1013904223U;
				data[i] = alphabet[(state>>16)%7];
				if(data[i] >= 128)ascii = 0;
			}
			for(wide = !ascii; wide < 2; wide++)for(old = 0; old < 3; old++)for(fail = 0; fail < 2; fail++)
				quotecase(data, length, wide, old, fail, 0);
		}
		for(old = 0; old < 3; old++)for(fail = 0; fail < 2; fail++)
			quotecase(data, 0, 0, old, fail, 1);
		quotebounds();
	}
	if(argc > 1 && strcmp(argv[1], "bounds") == 0)quotebounds();
	if(argc == 1 || strcmp(argv[1], "top") == 0)topcases();
	if(argc == 1 || strcmp(argv[1], "name") == 0)namecases();
	require(checks > 0);
	printf("Tk owners: %d quote/bounds/top/channel/interleaving cases\n", checks);
	return 0;
}
