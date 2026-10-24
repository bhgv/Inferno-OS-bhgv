#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include "draw.h"
#include "tk.h"
#include "pool.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static char *raised;
char exHeap[] = "heap", exNegsize[] = "negative", exBounds[] = "bounds", exNomem[] = "memory", exNilref[] = "nil";
Type Tstring = {1};
static Type topType = {1, 0, 0, sizeof(TkTop)}, channelType = {1, 0, 0, sizeof(Channel)};
static Type *fakeTkTop = &topType;
Type Tptr = {1};
static Prog program;
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
	assert(worker.nerr > 0);
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
	assert(size >= 0 && size < 16384);
	heap = malloc(sizeof(Heap)+size+32);
	assert(heap);
	memset(heap, 0, sizeof(Heap));
	memset(heap+1, 0xa5, size+32);
	heap->ref = 1;
	for(i = 0; i < 256 && blocks[i].heap; i++);
	assert(i < 256);
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
	assert(i < 256 && heap->ref > 0);
	for(j = 0; j < 32; j++)
		assert(((uchar*)(heap+1))[blocks[i].size+j] == 0xa5);
	if(--heap->ref)
		return;
	if(heap->t == fakeTkTop){
		if(program.tktop == value)
			program.tktop = nil;
		topType.ref--;
	}else if(heap->t == &channelType)
		channelType.ref--;
	else{
		assert(heap->t == &Tstring && ((String*)value)->tmp == nil);
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

/* Reused tops must already be live before their pool mode changes. */
void
poolmutable(void *heap)
{
	assert(((Heap*)heap)->ref > 0);
	((Heap*)heap)->color = mutator;
}

/* New-top construction is outside the exercised reuse path. */
void
poolimmutable(void *heap)
{
	(void)heap;
	abort();
}

/* Input names are ASCII, so no string cache capacity is needed. */
ulong
msize(void *pointer)
{
	(void)pointer;
	abort();
}

/* Unexpected construction of a new GUI top must terminate a reuse case. */
Display*
checkdisplay(Draw_Display *display)
{
	(void)display;
	abort();
}

/* Window allocation is outside existing-top ownership tests. */
Tk*
tknewobj(TkTop *top, int kind, int size)
{
	(void)top;
	(void)kind;
	(void)size;
	abort();
}

/* Window cleanup is outside existing-top ownership tests. */
void
tkfreeobj(Tk *tk)
{
	(void)tk;
	abort();
}

/* Context allocation is outside existing-top ownership tests. */
TkCtxt*
tknewctxt(Display *display)
{
	(void)display;
	abort();
}

/* Native name allocation is outside existing-top ownership tests. */
TkName*
tkmkname(char *name)
{
	(void)name;
	abort();
}

/* Geometry callbacks compile the unexecuted new-top branch. */
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

/* New-top option parsing must never run in a reuse case. */
void
tktopopt(Tk *tk, char *options)
{
	(void)tk;
	(void)options;
	abort();
}

/* Channel construction belongs to the unexecuted new-top path. */
Channel*
cnewc(Type *type, void (*mover)(void), int length)
{
	(void)type;
	(void)mover;
	(void)length;
	abort();
}

/* Mover identity compiles the unexecuted new-top path. */
void
movp(void)
{
	abort();
}

/* Record the context lock without relying on a real native event thread. */
static void
lockctxt(TkCtxt *ctxt)
{
	assert(ctxt == &context && !locked);
	locked = 1;
}

/* Deterministically replace or remove the variable as soon as its lock is released. */
static void
unlockctxt(TkCtxt *ctxt)
{
	assert(ctxt == &context && locked);
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
	assert(top && locked && strcmp(name, "chan") == 0 && type == TkVchan);
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
			assert(data[i] < 128);
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
	assert(worker.nerr == 0);
	if(fail){
		assert(!ok && raised == exHeap && quoted == old);
	}else{
		assert(ok && quoted != H);
		assert((quoted->len < 0 ? -quoted->len : quoted->len) == n);
		for(i = 0; i < n; i++)
			assert((quoted->len < 0 ? quoted->Srune[i] : (uchar)quoted->Sascii[i]) == expected[i]);
	}
	destroy(quoted);
	destroy(quote.s);
	assert(live == 0 && Tstring.ref == 1);
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
		assert(!kprotect(quoteinvoke, nil));
		assert(raised == exHeap && allocations == 0 && quoted == old);
		destroy(quoted);
		assert(!live && Tstring.ref == 1);
		checks++;
	}
}

/* Reusing a weak program top must retain a fresh result before releasing an aliased old one. */
static void
topcases(void)
{
	TkTop *top, *old;
	F_Tk_toplevel frame = {0};
	Tk_Toplevel *result;
	int mode, repeat, j;

	for(mode = 0; mode < 3; mode++)for(repeat = 1; repeat <= 32; repeat++){
		top = H2D(TkTop*, heapz(fakeTkTop));
		program.tktop = top;
		result = H;
		if(mode == 1)
			result = (Tk_Toplevel*)H2D(TkTop*, heapz(fakeTkTop));
		if(mode == 2)
			result = (Tk_Toplevel*)top;
		frame.ret = &result;
		old = mode == 2 ? nil : top;
		for(j = 0; j < repeat; j++){
			D2H(top)->color = 0;
			nprop = 0;
			Tk_toplevel(&frame);
			assert(D2H(top)->color == mutator && nprop == 0);
			assert(result == (Tk_Toplevel*)top && program.tktop == top);
			assert(D2H(top)->ref == (old ? 2 : 1));
		}
		destroy(result);
		if(old)
			destroy(old);
		assert(!live && topType.ref == 1 && program.tktop == nil);
		checks++;
	}
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
		variable = calloc(1, sizeof(TkVar)); assert(variable); nativevars++;
		variable->type = variablemode == 2 ? TkVstring : TkVchan;
		variable->value = same ? channel : H2D(Channel*, heapz(&channelType));
		if(same)D2H(channel)->ref++;
		result = H;
		frame.t = (Tk_Toplevel*)top; frame.c = channel;
		frame.n = c2string("chan", 4); frame.ret = &result;
		Tk_namechan(&frame);
		assert(!locked && result != H);
		if(variablemode == 0){
			assert(D2H(channel)->ref == (interleave ? 1 : 2));
			assert(result->len == 0 && D2H(channel)->color == propagator);
		}else{
			char *message = variablemode == 1 ? TkNomem : TkNotvt;
			assert(result->len == strlen(message));
			assert(memcmp(result->Sascii, message, result->len) == 0);
		}
		destroy(result); destroy(frame.n);
		if(variable){ destroy(variable->value); free(variable); nativevars--; variable = nil; }
		destroy(channel); destroy(top);
		assert(!live && !nativevars && channelType.ref == 1 && topType.ref == 1 && Tstring.ref == 1);
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
	assert(checks > 0);
	printf("Tk owners: %d quote/bounds/top/channel/interleaving cases\n", checks);
	return 0;
}
