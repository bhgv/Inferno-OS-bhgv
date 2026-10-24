#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static char *raised;
char exNilref[] = "nil", exInval[] = "invalid";
char exHeap[] = "heap", exNegsize[] = "negative", exBounds[] = "bounds";
Type Tstring = {1}, Tlist = {1};
Type Tptr = {1, 0, 0, sizeof(String*), 1, 0, 0, {0x80}};
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

/* Poison payloads so cleanup cannot rely on uninitialized list heads being nil. */
Heap*
nheap(int size)
{
	Heap *heap;
	int i;

	if(++allocations == failure)
		error(exHeap);
	assert(size >= 0 && size < 4096);
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

/* Validate guards and head/tail ownership while releasing every completed word. */
void
destroy(void *value)
{
	Heap *heap;
	List *list;
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
	if(heap->t == &Tlist){
		list = value;
		if(list->t != nil){
			assert(list->t == &Tptr);
			freeptrs(list->data, list->t);
			Tptr.ref--;
		}
		destroy(list->tail);
		Tlist.ref--;
	}else{
		assert(heap->t == &Tstring);
		assert(((String*)value)->tmp == nil);
		Tstring.ref--;
	}
	blocks[i].heap = nil;
	free(heap);
	live--;
}

#define BIT(bt, nb) ((bt)&(1<<(nb)))
IMPLEMENTATION

/* Construct either representation independently of the tokenizer under test. */
static String*
string(Rune *runes, int length, int wide)
{
	String *s;
	int i;

	s = wide ? newrunes(length) : newstring(length);
	for(i = 0; i < length; i++)
		if(wide)
			s->Srune[i] = runes[i];
		else{
			assert(runes[i] < 256);
			s->Sascii[i] = runes[i];
		}
	return s;
}

/* Bind the actual builtin call to the outer runtime protection frame. */
static int
invoke(void *frame)
{
	Sys_tokenize(frame);
	return 1;
}

/* Compare words with a separate character-array oracle, including embedded NULs. */
static void
one(Rune *source, int length, Rune *delimiters, int ndelim,
    int wide, int dwide, int nilsource, int delimmode, int fail)
{
	F_Sys_tokenize frame = {0};
	struct { WORD t0; List *t1; } result;
	int first[32], last[32], count, i, j, member, active, baseline, ok, nildelim;
	List *old, *node;
	String *word;

	failure = 0;
	frame.s = nilsource ? H : string(source, length, wide);
	nildelim = delimmode == 1;
	if(delimmode == 2){
		frame.delim = frame.s;
		if(frame.s != H)
			D2H(frame.s)->ref++;
		delimiters = source;
		ndelim = length;
	}else
		frame.delim = nildelim ? H : string(delimiters, ndelim, dwide);
	old = H;
	node = cons(sizeof(String*), &old);
	node->t = &Tptr;
	Tptr.ref++;
	*(String**)node->data = frame.s;
	if(frame.s != H)
		D2H(frame.s)->ref++;
	result.t0 = -77;
	result.t1 = old;
	frame.ret = (void*)&result;
	count = active = 0;
	if(nilsource)
		length = 0;
	for(i = 0; i <= length; i++){
		member = i == length;
		if(!nildelim && i < length)
			for(j = 0; j < ndelim; j++)
				if(source[i] == delimiters[j])
					member = 1;
		if(member && active){
			last[count++] = i;
			active = 0;
		}else if(!member && !active){
			first[count] = i;
			active = 1;
		}
	}
	baseline = live;
	failure = fail;
	allocations = 0;
	raised = nil;
	ok = kprotect(invoke, &frame);
	assert(worker.nerr == 0);
	failure = 0;
	if(fail > 0 && fail <= count*2){
		assert(!ok && raised == exHeap);
		assert(result.t0 == -77 && result.t1 == old && live == baseline);
	}else{
		assert(ok && raised == nil);
		assert(result.t0 == count);
		node = result.t1;
		for(i = 0; i < count; i++){
			assert(node != H && node->t == &Tptr);
			word = *(String**)node->data;
			assert(word != H && word != frame.s);
			assert((word->len < 0 ? -word->len : word->len) == last[i]-first[i]);
			for(j = first[i]; j < last[i]; j++)
				assert((word->len < 0 ? word->Srune[j-first[i]] : (uchar)word->Sascii[j-first[i]]) == source[j]);
			node = node->tail;
		}
		assert(node == H);
	}
	for(i = 0; i < length; i++)
		assert((frame.s->len < 0 ? frame.s->Srune[i] : (uchar)frame.s->Sascii[i]) == source[i]);
	destroy(result.t1);
	destroy(frame.s);
	destroy(frame.delim);
	assert(live == 0 && Tstring.ref == 1 && Tlist.ref == 1 && Tptr.ref == 1);
	checks++;
}

/* Reject malformed metadata before allocating or publishing a partial tuple. */
static void
malformed(void)
{
	F_Sys_tokenize frame = {0};
	struct { WORD t0; List *t1; } result;
	String *source, *delimiter;
	Rune a = 'a';
	int which, baseline, ok;

	for(which = 0; which < 6; which++){
		source = string(&a, 1, 0);
		delimiter = string(&a, 1, 0);
		frame.s = source;
		frame.delim = delimiter;
		result.t0 = -77; result.t1 = H;
		frame.ret = (void*)&result;
		if(which == 0)frame.s = nil;
		if(which == 1)frame.delim = nil;
		if(which == 2)source->len = (-0x7fffffff-1);
		if(which == 3)delimiter->len = (-0x7fffffff-1);
		if(which == 4)frame.ret = nil;
		if(which == 5)frame.ret = H;
		baseline = live;
		allocations = 0; raised = nil;
		ok = kprotect(invoke, &frame);
		assert(!ok && raised == (which == 2 || which == 3 ? exInval : exNilref));
		assert(worker.nerr == 0 && allocations == 0 && live == baseline);
		assert(result.t0 == -77 && result.t1 == H);
		source->len = delimiter->len = 1;
		destroy(source); destroy(delimiter);
		assert(live == 0);
		checks++;
	}
}

/* Compare all byte characters against each one-character delimiter set. */
static void
membership(void)
{
	Rune rune;
	String *delimiter;
	int c, d, wide;

	for(d = 0; d < 256; d++)
	for(wide = 0; wide < 2; wide++){
		rune = d;
		delimiter = string(&rune, 1, wide);
		for(c = 0; c < 256; c++){
			assert(tokdelim(c, delimiter) == (c == d));
			checks++;
		}
		destroy(delimiter);
		assert(live == 0 && Tstring.ref == 1);
	}
}

/* Exercise mixed byte/rune delimiters and every byte alignment of the tuple. */
static void
bytealignment(void)
{
	F_Sys_tokenize frame = {0};
	typedef struct { WORD t0; List *t1; } Value;
	Value result;
	uchar storage[sizeof(Value)+16];
	Rune source[3], delim;
	int c, wide, dwide, alignment;
	String *word;

	for(c = 128; c < 256; c++)
	for(wide = 0; wide < 2; wide++)
	for(dwide = 0; dwide < 2; dwide++)
	for(alignment = 0; alignment < 8; alignment++){
		source[0] = c; source[1] = 'x'; source[2] = c;
		delim = c;
		frame.s = string(source, 3, wide);
		frame.delim = string(&delim, 1, dwide);
		result.t0 = -77; result.t1 = H;
		memset(storage, 0xa5, sizeof(storage));
		memmove(storage+alignment, &result, sizeof(result));
		frame.ret = (void*)(storage+alignment);
		assert(kprotect(invoke, &frame) && worker.nerr == 0);
		memmove(&result, storage+alignment, sizeof(result));
		assert(result.t0 == 1 && result.t1 != H && result.t1->tail == H);
		memmove(&word, result.t1->data, sizeof(word));
		assert(word->len == (wide ? -1 : 1));
		assert((wide ? word->Srune[0] : (uchar)word->Sascii[0]) == 'x');
		assert(storage[alignment+sizeof(result)] == 0xa5);
		if(alignment)assert(storage[alignment-1] == 0xa5);
		destroy(result.t1); destroy(frame.s); destroy(frame.delim);
		assert(live == 0 && Tstring.ref == 1 && Tlist.ref == 1 && Tptr.ref == 1);
		checks++;
	}
}

/* Cover nil/empty delimiters, both representations, NULs and every failure boundary. */
int
main(int argc, char **argv)
{
	Rune source[32], delimiters[8];
	Rune alphabet[] = {'a', 'b', ' ', 0, 0x400, 0x1f600};
	unsigned state = 1;
	int sample, length, i, mask, n, wide, dwide, ascii, dascii, fail;

	if(argc == 1 || strcmp(argv[1], "membership") == 0)membership();
	if(argc == 1 || strcmp(argv[1], "malformed") == 0)malformed();
	if(argc == 1 || strcmp(argv[1], "alignment") == 0)bytealignment();
	if(argc == 1 || strcmp(argv[1], "nil") == 0){
		source[0] = 'a'; source[1] = 0; source[2] = 'b';
		for(wide = 0; wide < 2; wide++)for(dwide = 0; dwide < 2; dwide++)
			{
				one(source, 3, delimiters, 0, wide, dwide, 0, 1, 0);
				one(source, 3, delimiters, 0, wide, dwide, 0, 2, 0);
			}
	}
	if(argc == 1 || strcmp(argv[1], "content") == 0)
	for(sample = 0; sample < 512; sample++){
		length = sample%25;
		ascii = 1;
		for(i = 0; i < length; i++){
			state = state*1664525U+1013904223U;
			source[i] = alphabet[(state>>16)%6];
			if(source[i] >= 128)
				ascii = 0;
		}
		for(mask = 0; mask < 32; mask++){
			n = 0; dascii = 1;
			for(i = 0; i < 5; i++)if(mask&(1<<i)){
				delimiters[n++] = alphabet[i];
				if(delimiters[n-1] >= 128)
					dascii = 0;
			}
			for(wide = !ascii; wide < 2; wide++)for(dwide = !dascii; dwide < 2; dwide++){
				one(source, length, delimiters, n, wide, dwide, 0, 0, 0);
				one(source, length, delimiters, n, wide, dwide, 1, 0, 0);
				one(source, length, delimiters, n, wide, dwide, 0, 1, 0);
			}
		}
	}
	if(argc == 1 || strcmp(argv[1], "failure") == 0){
		for(i = 0; i < 9; i++)
			source[i] = i%2 ? ' ' : 'a';
		delimiters[0] = ' ';
		for(wide = 0; wide < 2; wide++)for(dwide = 0; dwide < 2; dwide++)
		for(fail = 1; fail <= 11; fail++)
			one(source, 9, delimiters, 1, wide, dwide, 0, 0, fail);
	}
	assert(checks > 0);
	printf("Sys tokenize: %d delimiter/representation/ownership/failure cases\n", checks);
	return 0;
}
