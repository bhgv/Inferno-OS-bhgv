/* Guard real VM payloads and exercise production tokenization on both scalar widths. */
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static char *raised;
static int live, calls, failat, checks;
static struct { Heap *heap; int size; } blocks[256];
Type Tstring = {1}, Tlist = {1}, Tptr = {1};
int mutator, nprop;
char exNilref[] = "nil", exInval[] = "invalid";
char exHeap[] = "heap", exNegsize[] = "negative", exBounds[] = "bounds", exNomem[] = "memory";

/* Use actual emulator error frames, including nested protected construction. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *message) { raised = message; nexterror(); }
/* Only LeakSanitizer process inspection is unavailable; track every VM block explicitly. */
int __lsan_is_turned_off(void) { return 1; }

/* Poison exact allocations so uninitialized list heads cannot resemble VM nil. */
Heap*
nheap(int size)
{
	Heap *h;
	int i;

	calls++;
	if(calls == failat)
		error(exNomem);
	assert(size >= 0 && size <= 4096);
	h = malloc(sizeof(Heap)+size+16);
	assert(h);
	memset(h, 0xa5, sizeof(Heap)+size+16);
	h->ref = 1;
	for(i = 0; i < 256 && blocks[i].heap; i++);
	assert(i < 256);
	blocks[i].heap = h;
	blocks[i].size = size;
	live++;
	return h;
}

/* Return exact capacities to the real string constructors. */
int
hmsize(void *p)
{
	int i;

	for(i = 0; i < 256 && blocks[i].heap != p; i++);
	assert(i < 256);
	return sizeof(Heap)+blocks[i].size;
}

/* Validate payload guards and balance actual list, string and type references. */
void
destroy(void *p)
{
	Heap *h;
	List *l;
	int i, j;

	if(p == H)
		return;
	h = D2H(p);
	assert(h->ref > 0);
	if(--h->ref)
		return;
	for(i = 0; i < 256 && blocks[i].heap != h; i++);
	assert(i < 256);
	for(j = 0; j < 16; j++)
		assert(((uchar*)(h+1))[blocks[i].size+j] == 0xa5);
	if(h->t == &Tlist){
		l = p;
		assert(l->t == &Tptr);
		destroy(*(String**)l->data);
		destroy(l->tail);
		Tptr.ref--;
	}else{
		assert(h->t == &Tstring);
		assert(((String*)p)->tmp == nil);
	}
	h->t->ref--;
	blocks[i].heap = nil;
	free(h);
	live--;
}

IMPLEMENTATION

/* Construct exactly the requested native VM representation, including embedded NUL. */
static String*
text(Rune *runes, int n, int wide)
{
	String *s;
	int i;

	s = wide ? newrunes(n) : newstring(n);
	for(i = 0; i < n; i++){
		if(wide)
			s->Srune[i] = runes[i];
		else{
			assert(runes[i] < Runeself);
			s->Sascii[i] = runes[i];
		}
	}
	return s;
}

/* Read characters without changing representation or relying on production delimiter logic. */
static Rune
character(String *s, int i)
{
	return s->len < 0 ? s->Srune[i] : (uchar)s->Sascii[i];
}

/* Invoke the real builtin under the same outer protection used by callers. */
static int
invoke(void *frame)
{
	Sys_tokenize(frame);
	return 1;
}

/* Independently locate maximal non-delimiter spans and compare every returned character. */
static int
verify(List *list, Rune *input, int n, Rune *delimiters, int nd)
{
	int i, j, start, count, match, k;
	String *s;

	i = count = 0;
	while(i < n){
		match = 0;
		for(j = 0; j < nd; j++)
			match |= input[i] == delimiters[j];
		if(match){ i++; continue; }
		start = i++;
		while(i < n){
			match = 0;
			for(j = 0; j < nd; j++)
				match |= input[i] == delimiters[j];
			if(match)break;
			i++;
		}
		assert(list != H);
		s = *(String**)list->data;
		assert(s != H && (s->len < 0 ? -s->len : s->len) == i-start);
		for(k = start; k < i; k++)
			assert(character(s, k-start) == input[k]);
		list = list->tail;
		count++;
	}
	assert(list == H);
	return count;
}

/* Sweep representations, separators, lengths and every list/string allocation failure. */
static void
scenario(int wide, int dw, int n, int pattern, int failures)
{
	Rune input[40], delimiters[4] = {' ', 0, 0x20ac, 0xffffffffU};
	Rune alphabet[7] = {'a', ' ', 0, 'b', 0x20ac, 0x1f600, 0xffffffffU};
	F_Sys_tokenize f = {0};
	List *old, *head;
	String *sentinel;
	int i, j, nd, count, before, allocation, ok;

	for(i = 0; i < n; i++)
		input[i] = alphabet[(i*5+pattern)%(wide ? 7 : 4)];
	nd = dw ? 4 : 2;
	f.s = text(input, n, wide);
	f.delim = text(delimiters, nd, dw);
	f.ret = calloc(1, sizeof(*f.ret));
	assert(f.ret);
	/* The old list owns the input too, exercising input/output graph aliasing. */
	old = H;
	head = cons(sizeof(String*), &old);
	head->t = &Tptr;
	Tptr.ref++;
	*(String**)head->data = f.s;
	D2H(f.s)->ref++;
	sentinel = f.s;
	before = live;
	for(j = 0; ; j++){
		f.ret->t0 = 12345;
		f.ret->t1 = old;
		allocation = calls;
		failat = failures ? calls+j+1 : 0;
		raised = nil;
		ok = kprotect(invoke, &f);
		if(ok){
			count = verify(f.ret->t1, input, n, delimiters, nd);
			assert(f.ret->t0 == count && worker.nerr == 0);
			assert(calls-allocation == 2*count);
			assert(D2H(sentinel)->ref == 1);
			destroy(f.ret->t1);
			checks++;
			break;
		}
		assert(raised == exNomem && worker.nerr == 0);
		assert(f.ret->t0 == 12345 && f.ret->t1 == old);
		assert(D2H(sentinel)->ref == 2 && live == before);
		checks++;
	}
	failat = 0;
	destroy(f.s);
	destroy(f.delim);
	free(f.ret);
	assert(live == 0 && Tlist.ref == 1 && Tptr.ref == 1 && Tstring.ref == 1);
}

/* Nil and empty delimiter/source combinations must release the previous result exactly once. */
static void
nilcases(void)
{
	F_Sys_tokenize f = {0};
	Rune a = 'a';
	Rune input[] = {'a', 0, 'b'};
	List *old;
	int mode;

	f.ret = calloc(1, sizeof(*f.ret));
	assert(f.ret);
	for(mode = 0; mode < 4; mode++){
		f.s = mode&1 ? text(&a, 1, 0) : H;
		f.delim = mode&2 ? text(&a, 1, 1) : H;
		old = H;
		cons(sizeof(String*), &old);
		old->t = &Tptr;
		Tptr.ref++;
		*(String**)old->data = text(&a, 1, 0);
		f.ret->t1 = old;
		f.ret->t0 = 99;
		assert(kprotect(invoke, &f));
		if(mode == 1)
			assert(f.ret->t0 == 1 && verify(f.ret->t1, &a, 1, nil, 0) == 1);
		else
			assert(f.ret->t0 == 0 && f.ret->t1 == H);
		assert(worker.nerr == 0);
		destroy(f.ret->t1);
		destroy(f.s);
		destroy(f.delim);
		checks++;
	}
	for(mode = 0; mode < 2; mode++){
		f.s = text(input, 3, mode);
		f.delim = newstring(0);
		f.ret->t1 = H;
		assert(kprotect(invoke, &f));
		assert(f.ret->t0 == 1 && verify(f.ret->t1, input, 3, nil, 0) == 1);
		destroy(f.ret->t1);
		destroy(f.s);
		destroy(f.delim);
		checks++;
	}
	free(f.ret);
	assert(live == 0 && Tlist.ref == 1 && Tptr.ref == 1 && Tstring.ref == 1);
}

/* Distinguish functional coverage from the deterministic allocation-failure sweeps. */
int
main(int argc, char **argv)
{
	int wide, dw, n, pattern, failures;

	failures = argc < 2 || strcmp(argv[1], "functional") != 0;
	for(wide = 0; wide < 2; wide++)
		for(dw = 0; dw < 2; dw++)
			for(n = 0; n <= 32; n++)
				for(pattern = 0; pattern < 7; pattern++)
					scenario(wide, dw, n, pattern, failures);
	nilcases();
	printf("PASS Sys.tokenize: %d representation, delimiter, ownership and failure cases, WORD%lu\n", checks, 8*sizeof(WORD));
	return 0;
}
