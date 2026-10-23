#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static char *raised;
char exModule[]="module";
char exHeap[] = "heap";
REG R;
int mutator, nprop;
Type Tlist = {1}, Tptr = {1};
static Type record = {1};
static int live, allocations, failure, operation, checks;
static struct { Heap *heap; int size; } blocks[32];

/* Use real emulator error frames around fallible list construction. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *message) { raised = message; nexterror(); }
/* LeakSanitizer process inspection is unavailable in this sandbox. */
int __lsan_is_turned_off(void) { return 1; }

/* Allocate exact payload sizes and guard every byte beyond them. */
Heap*
nheap(int size)
{
	Heap *h;
	int i;

	allocations++;
	if(failure || size < 0 || size > 4096)error(exHeap);
	h = calloc(1, sizeof(Heap)+size+32); assert(h); h->ref = 1;
	for(i = 0; i < 32 && blocks[i].heap; i++);
	assert(i < 32); blocks[i].heap = h; blocks[i].size = size;
	memset((uchar*)(h+1)+size, 0xa5, 32); live++;
	return h;
}

/* Enforce exact list payload guards and mapped head/tail ownership. */
void
destroy(void *p)
{
	Heap *h;
	List *l;
	int i, j;

	if(p == H)return;
	h = D2H(p); assert(h->ref > 0);
	if(--h->ref)return;
	for(i = 0; i < 32 && blocks[i].heap != h; i++);
	assert(i < 32);
	for(j = 0; j < 32; j++)assert(((uchar*)(h+1))[blocks[i].size+j] == 0xa5);
	assert(h->t == &Tlist); l = p;
	if(l->t != nil){ freeptrs(l->data, l->t); l->t->ref--; }
	destroy(l->tail); Tlist.ref--;
	blocks[i].heap = nil; free(h); live--;
}
#define OP(name) void name(void)
#define B(r) *((BYTE*)(R.r))
#define W(r) *((WORD*)(R.r))
#define V(r) *((LONG*)(R.r))
#define F(r) *((REAL*)(R.r))
#define L(r) *((List**)(R.r))
#define P(r) *((WORD**)(R.r))
#define BIT(bt,nb) ((bt)&(1<<(nb)))
IMPLEMENTATION

/* Dispatch actual interpreter instructions without substituting their logic. */
static int
run(void *unused)
{
	(void)unused;
	switch(operation){
	case 0: consb(); break;
	case 1: consw(); break;
	case 2: consl(); break;
	case 3: consp(); break;
	case 4: consf(); break;
	case 5: consm(); break;
	case 6: consmp(); break;
	}
	return 1;
}

/* Scalar payloads retain complete host values and preserve tails on heap failure. */
static void
scalars(void)
{
	union { BYTE byte; WORD word; LONG big; REAL real; void *pointer; } input;
	List *list, *old;
	int op, fail, n, before;
	uchar expected[sizeof(input)];

	for(op = 0; op < 5; op++) for(fail = 0; fail < 2; fail++){
		list = H; old = cons(0, &list); memset(&input, 0, sizeof(input));
		if(op == 0){ input.byte = 0xff; n = sizeof(input.byte); }
		if(op == 1){ input.word = (WORD)~(UWORD)0; n = sizeof(input.word); }
		if(op == 2){ input.big = (-9223372036854775807LL-1); n = sizeof(input.big); }
		if(op == 3){ input.pointer = H; n = sizeof(input.pointer); }
		if(op == 4){ input.real = -1234.5; n = sizeof(input.real); }
		memmove(expected, &input, n);
		R.s = &input; R.d = &list; operation = op; failure = fail; before = live;
		if(fail){ assert(!kprotect(run, nil)); assert(raised == exHeap && list == old && live == before); }
		else { assert(kprotect(run, nil)); assert(list->tail == old && !memcmp(list->data, expected, n)); }
		failure = 0; destroy(list); assert(live == 0); checks++;
	}
}

/* Source slots can be overwritten by the resulting list without changing the head value. */
static void
aliases(void)
{
	struct { List *list; WORD marker; } input;
	Type *types[] = {&record};
	Modlink module = {0};
	Module owner = {0};
	List *old;
	WORD size;
	int op, fail, before;

	owner.type = types; owner.ntype = 1; module.m = &owner;
	module.type = types; R.M = &module;
	record.size = sizeof(input); record.np = 1; record.map[0] = 0x80;
	for(op = 3; op <= 6; op++) for(fail = 0; fail < 2; fail++){
		if(op == 4)continue;
		input.list = H; old = cons(0, &input.list); input.marker = 12345;
		operation = op; R.s = &input; R.d = &input.list;
		size = op == 5 ? sizeof(input) : 0; R.m = &size;
		failure = fail; before = live;
		if(fail){
			assert(!kprotect(run, nil));
			assert(input.list == old && live == before && D2H(old)->ref == 1);
			failure = 0; destroy(input.list); checks++; continue;
		}
		assert(kprotect(run, nil));
		assert(input.list != old && input.list->tail == old);
		assert(*(List**)input.list->data == old);
		if(op != 3)assert(!memcmp((uchar*)input.list->data+sizeof(List*), (uchar*)&input+sizeof(List*), sizeof(input)-sizeof(List*)));
		assert(D2H(old)->ref == (op == 5 ? 1 : 2));
		destroy(input.list); assert(live == 0 && record.ref == 1); checks++;
	}
}

/* Raw payload lengths and shared tails preserve byte patterns and transfer one owner. */
static void
payloads(void)
{
	uchar data[258];
	List *list, *old;
	WORD size;
	int n, fail, shared, i, before;

	for(i = 0; i < sizeof(data); i++)data[i] = (uchar)(i*31+9);
	for(n = 0; n <= 257; n++) for(fail = 0; fail < 2; fail++)
	for(shared = 0; shared < 2; shared++){
		list = H; old = cons(0, &list);
		if(shared)D2H(old)->ref++;
		size = n; R.s = n == 0 ? nil : data; R.d = &list; R.m = &size;
		operation = 5; failure = fail; before = live;
		if(fail){ assert(!kprotect(run, nil)); assert(list == old && live == before); }
		else { assert(kprotect(run, nil)); assert(list->tail == old && !memcmp(list->data, data, n)); }
		assert(D2H(old)->ref == 1+shared);
		failure = 0; destroy(list);
		if(shared)destroy(old);
		assert(live == 0); checks++;
	}
}

/* Full-width raw sizes fail before allocation, source reads or destination replacement. */
static void
sizes(void)
{
	List *list, *old;
	WORD size;
	int mode, before;

	operation = 5;
	for(mode = 0; mode < 4; mode++){
		list = H; old = cons(0, &list);
		size = mode == 0 ? -1 : mode == 1 ? 0x7fffffff :
			sizeof(WORD)>4 ? (WORD)(((uvlong)1<<32)+mode-2) : -2;
		R.s = (void*)1; R.d = &list; R.m = &size; before = allocations;
		assert(!kprotect(run, nil));
		assert(raised == exHeap && list == old && allocations == before);
		destroy(list); checks++;
	}
	list = H; size = 0; R.s = nil; R.d = &list; R.m = &size;
	assert(kprotect(run, nil)); destroy(list); checks++;
}

/* Require complete ownership balance and guard integrity in each VM word model. */
int
main(void)
{
	Tptr.size = sizeof(void*); Tptr.np = 1; Tptr.map[0] = 0x80;
	scalars(); aliases(); payloads(); sizes();
	assert(live == 0 && Tlist.ref == 1 && Tptr.ref == 1 && worker.nerr == 0);
	printf("PASS lists: %d width/alias/failure cases, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
