#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
#include <stddef.h>

static Proc worker;
static char *raised;
char exModule[]="module", exNilref[]="nil";
char exHeap[] = "heap";
char exBounds[] = "bounds";
REG R;
int mutator, nprop;
Type Tlist = {1}, Tptr = {1};
static Type record = {1};
static int live, allocations, failure, operation, checks;
static int constructoperation = -1;
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
	case 7: headmp(); break;
	case 8: headb(); break;
	case 9: headw(); break;
	case 10: headl(); break;
	case 11: headf(); break;
	case 12: headm(); break;
	case 13: lenl(); break;
	case 14: headp(); break;
	case 15: tail(); break;
	case 16: cons(0, R.d); break;
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

/* Replace the sole source owner while copying every one-byte pointer-map shape. */
static void
mappedheads(void)
{
	void *output[8], *expected[8];
	Type *types[] = {&record};
	Module owner = {0};
	Modlink module = {0};
	List *source, *child, *tail;
	WORD index = 0;
	int mask, slot, shared, i, extra;

	owner.type = types;
	owner.ntype = 1;
	module.m = &owner;
	R.M = &module;
	record.size = sizeof(output);
	record.np = 1;
	operation = 7;
	for(mask = 1; mask < 256; mask++)
	for(slot = 0; slot < 8; slot++){
		if(!(mask & (0x80>>slot)))
			continue;
		for(shared = 0; shared < 2; shared++){
			record.map[0] = mask;
			tail = H;
			cons(0, &tail);
			source = tail;
			cons(sizeof(output), &source);
			source->t = &record;
			record.ref++;
			child = H;
			cons(0, &child);
			extra = 0;
			for(i = 0; i < 8; i++){
				if(mask & (0x80>>i)){
					expected[i] = i&1 ? (void*)H : (void*)child;
					if(expected[i] == child){
						D2H(child)->ref++;
						extra++;
					}
					output[i] = H;
				}else{
					memset(&expected[i], 0x29+i, sizeof(expected[i]));
					memset(&output[i], 0xa9+i, sizeof(output[i]));
				}
			}
			memmove(source->data, expected, sizeof(expected));
			output[slot] = source;
			if(shared)
				D2H(source)->ref++;
			R.s = &source;
			R.d = output;
			R.m = &index;
			assert(kprotect(run, nil));
			assert(!memcmp(output, expected, sizeof(expected)));
			assert(D2H(child)->ref == 1+extra*(1+shared));
			if(shared){
				assert(D2H(source)->ref == 1);
				destroy(source);
			}
			freeptrs(output, &record);
			assert(D2H(child)->ref == 1);
			destroy(child);
			assert(live == 0 && record.ref == 1 && Tlist.ref == 1);
			checks++;
		}
	}
}

/* Metadata errors must not acquire a source reference; empty records do not replace owners. */
static void
mappedheadbounds(void)
{
	Type *types[] = {&record};
	Module owner = {0};
	Modlink module = {0};
	List *source, *output;
	WORD index;
	union { uvlong align; uchar bytes[16]; } operand;
	int mode;

	owner.type = types;
	owner.ntype = 1;
	module.m = &owner;
	R.M = &module;
	operation = 7;
	record.size = 0;
	record.np = 0;
	for(mode = 0; mode < 4; mode++){
		source = H;
		cons(0, &source);
		output = source;
		index = mode == 0 ? 0 : mode == 1 ? -1 : mode == 2 ? 1 :
			sizeof(WORD) > 4 ? (WORD)((uvlong)1<<32) : -2;
		R.s = &source;
		R.d = &output;
		memcpy(operand.bytes+4, &index, sizeof(index));
		R.m = operand.bytes+4;
		assert(kprotect(run, nil) == (mode == 0));
		assert(output == source && D2H(source)->ref == 1 && live == 1);
		destroy(source);
		assert(live == 0);
		checks++;
	}
	source = H;
	output = H;
	index = 0;
	R.s = &source;
	R.d = &output;
	memcpy(operand.bytes+4, &index, sizeof(index));
	R.m = operand.bytes+4;
	assert(!kprotect(run, nil) && raised == exNilref && output == H);
	checks++;
}

#ifndef LIST_HEAD_CASE
#define LIST_HEAD_CASE 0
#endif

/* Exercise scalar heads through unaligned owner/result slots and partial aliases. */
static void
scalarheads(void)
{
	union { uvlong align; uchar bytes[96]; } input, output;
	uchar original[64], expected[64], wantinput[96], wantoutput[96];
	List *source;
	int op, alias, i, width, size;

	size = (int)offsetof(List, data)+32;
	assert(size <= sizeof(original));
	if(LIST_HEAD_CASE != 1)
	for(op = 8; op <= 11; op++)
	for(alias = 0; alias < 4; alias++){
		source = H;
		cons(32, &source);
		for(i = 0; i < 32; i++)((uchar*)source->data)[i] = i*29+17;
		memcpy(original, source, size);
		memcpy(expected, source, size);
		memset(input.bytes, 0xa5, sizeof(input.bytes));
		memset(output.bytes, 0x5a, sizeof(output.bytes));
		memcpy(input.bytes+4, &source, sizeof(source));
		memcpy(wantinput, input.bytes, sizeof(wantinput));
		memcpy(wantoutput, output.bytes, sizeof(wantoutput));
		width = op == 8 ? 1 : op == 9 ? sizeof(WORD) : 8;
		R.s = input.bytes+4;
		R.d = alias == 0 ? output.bytes+4 : alias == 1 ? input.bytes+4 :
			(uchar*)source->data+(alias == 2 ? 4 : -4);
		if(alias < 2)
			memcpy((alias == 0 ? wantoutput : wantinput)+4, source->data, width);
		else
			memmove(expected+((uchar*)source->data-(uchar*)source)+(alias == 2 ? 4 : -4), source->data, width);
		operation = op;
		assert(kprotect(run, nil));
		assert(!memcmp(source, expected, size));
		assert(!memcmp(input.bytes, wantinput, sizeof(wantinput)));
		assert(!memcmp(output.bytes, wantoutput, sizeof(wantoutput)));
		memcpy(source, original, size);
		destroy(source);
		checks++;
	}
	for(op = 7; op <= 15; op++)
	for(i = 0; i < 2; i++){
		if(op == 13 || (op == 7 && LIST_HEAD_CASE == 1))continue;
		source = i ? nil : H;
		memcpy(input.bytes+4, &source, sizeof(source));
		memset(output.bytes, 0xa5, sizeof(output.bytes));
		memcpy(wantoutput, output.bytes, sizeof(wantoutput));
		R.s = input.bytes+4;
		R.d = output.bytes+4;
		R.m = (void*)1;
		if(LIST_HEAD_CASE == 1)R.s = &source;
		operation = op;
		assert(!kprotect(run, nil) && raised == exNilref);
		assert(!memcmp(output.bytes, wantoutput, sizeof(wantoutput)));
		checks++;
	}
}

/* Raw head copies capture their unaligned count before writing overlapping slots. */
static void
rawheads(void)
{
	union { uvlong align; uchar bytes[96]; } input;
	uchar expected[96];
	List *source;
	WORD length;
	int n, i;

	for(n = -1; n <= 32; n++){
		source = H;
		cons(32, &source);
		for(i = 0; i < 32; i++)((uchar*)source->data)[i] = i*19+31;
		memset(input.bytes, 0xa5, sizeof(input.bytes));
		memcpy(input.bytes+4, &source, sizeof(source));
		length = n;
		memcpy(input.bytes+20, &length, sizeof(length));
		memcpy(expected, input.bytes, sizeof(expected));
		if(n > 0)memcpy(expected+4, source->data, n);
		R.s = input.bytes+4;
		R.d = input.bytes+4;
		R.m = input.bytes+20;
		if(LIST_HEAD_CASE == 2)R.s = &source;
		operation = 12;
		assert(kprotect(run, nil) == (n >= 0));
		if(n < 0)assert(raised == exBounds);
		assert(!memcmp(input.bytes, expected, sizeof(expected)));
		destroy(source);
		checks++;
	}
}

/* Length scans must reject null links before storing through unaligned outputs. */
static void
listlengths(void)
{
	List nodes[129], *source;
	union { uvlong align; uchar bytes[64]; } slots;
	WORD result;
	int n, i, terminal;

	operation = 13;
	for(n = 0; n <= 129; n++)
	for(terminal = 0; terminal < 2; terminal++){
		memset(nodes, 0, sizeof(nodes));
		for(i = 0; i < n; i++)nodes[i].tail = i+1 < n ? &nodes[i+1] : terminal ? nil : H;
		source = n ? nodes : terminal ? nil : H;
		memset(slots.bytes, 0xa5, sizeof(slots.bytes));
		memcpy(slots.bytes+4, &source, sizeof(source));
		R.s = slots.bytes+4;
		R.d = slots.bytes+20;
		if(LIST_HEAD_CASE == 3){R.s = &source; R.d = &result;}
		assert(kprotect(run, nil) == !terminal);
		if(terminal){
			assert(raised == exNilref);
			for(i = 20; i < 20+sizeof(WORD); i++)assert(slots.bytes[i] == 0xa5);
		}else{
			memcpy(&result, R.d, sizeof(result));
			assert(result == n);
		}
		checks++;
	}
}

/* Pointer head and tail replacement must retain the child before freeing its owner. */
static void
pointerheads(void)
{
	List *child, *parent;
	int op;

	for(op = 14; op <= 15; op++){
		child = H;
		cons(0, &child);
		D2H(child)->ref++;
		if(op == 14){
			parent = H;
			cons(sizeof(child), &parent);
			memmove(parent->data, &child, sizeof(child));
			parent->t = &Tptr;
			Tptr.ref++;
		}else{
			parent = child;
			cons(0, &parent);
		}
		R.s = &parent;
		R.d = &parent;
		operation = op;
		assert(kprotect(run, nil));
		assert(parent == child && D2H(child)->ref == 2 && live == 1 && Tptr.ref == 1);
		destroy(parent);
		destroy(child);
		assert(live == 0 && Tlist.ref == 1);
		checks++;
	}
}

/* Constructors must capture unaligned values and preserve owners on heap failure. */
static void
constructors(void)
{
	union { uvlong align; uchar bytes[96]; } slots;
	union { BYTE byte; WORD word; LONG big; REAL real; void *pointer; } value;
	uchar payload[16], before[96];
	Type *types[] = {&record};
	Module owner = {0};
	Modlink module = {0};
	List *old, *result, *child;
	uvlong bits[] = {0, 0x8000000000000000ULL, 0x7ff0000000000000ULL,
		0x7ff8000000000042ULL, 0x7ff0000000000042ULL};
	WORD middle;
	int op, fail, width, i, livebefore;

	owner.type = types; owner.ntype = 1; module.m = &owner; R.M = &module;
	for(op = 0; op <= 6; op++)
	for(fail = 0; fail < 2; fail++){
		old = H;
		cons(0, &old);
		memset(&value, 0, sizeof(value));
		memset(payload, 0, sizeof(payload));
		width = 0;
		if(op == 0){value.byte = 0xff; width = sizeof(value.byte);}
		if(op == 1){value.word = (WORD)((UWORD)1<<(sizeof(WORD)*8-1)); width = sizeof(value.word);}
		if(op == 2){value.big = (-9223372036854775807LL-1); width = sizeof(value.big);}
		if(op == 3){value.pointer = H; width = sizeof(value.pointer);}
		if(op == 4){value.real = -1234.5; width = sizeof(value.real);}
		if(op <= 4)memcpy(payload, &value, width);
		if(op == 5){width = sizeof(payload); for(i = 0; i < width; i++)payload[i] = i*19+31;}
		if(op == 6){value.pointer = H; width = sizeof(value.pointer); memcpy(payload, &value, width);}
		record.size = width; record.np = op == 6; record.map[0] = 0x80;
		middle = op == 5 ? width : 0;
		memset(slots.bytes, 0xa5, sizeof(slots.bytes));
		memcpy(slots.bytes+4, payload, width);
		memcpy(slots.bytes+20, &old, sizeof(old));
		memcpy(slots.bytes+36, &middle, sizeof(middle));
		memcpy(before, slots.bytes, sizeof(before));
		R.s = slots.bytes+4; R.d = slots.bytes+20; R.m = slots.bytes+36;
		operation = op; failure = fail; livebefore = live;
		assert(kprotect(run, nil) == !fail);
		failure = 0;
		memcpy(&result, R.d, sizeof(result));
		if(fail){
			assert(raised == exHeap && result == old && live == livebefore && D2H(old)->ref == 1);
			assert(!memcmp(slots.bytes, before, sizeof(before)));
		}else{
			assert(result != old && result->tail == old && D2H(old)->ref == 1);
			assert(!memcmp(result->data, payload, width));
			memcpy(before+20, &result, sizeof(result));
			assert(!memcmp(slots.bytes, before, sizeof(before)));
		}
		destroy(result);
		assert(live == 0 && record.ref == 1 && Tptr.ref == 1 && Tlist.ref == 1);
		checks++;
	}
	/* Real construction is a bit-preserving copy, including signaling NaNs. */
	for(i = 0; i < nelem(bits); i++){
		old = H;
		memcpy(slots.bytes+4, &bits[i], sizeof(bits[i]));
		memcpy(slots.bytes+20, &old, sizeof(old));
		R.s = slots.bytes+4; R.d = slots.bytes+20; operation = 4;
		assert(kprotect(run, nil));
		memcpy(&result, R.d, sizeof(result));
		assert(!memcmp(result->data, &bits[i], sizeof(bits[i])));
		destroy(result);
		checks++;
	}
	/* Pointer and mapped constructors acquire exactly one child reference. */
	for(op = 3; op <= 6; op += 3){
		child = H;
		cons(0, &child);
		old = H; middle = 0;
		record.size = sizeof(child); record.np = 1; record.map[0] = 0x80;
		memcpy(slots.bytes+4, &child, sizeof(child));
		memcpy(slots.bytes+20, &old, sizeof(old));
		memcpy(slots.bytes+36, &middle, sizeof(middle));
		R.s = slots.bytes+4; R.d = slots.bytes+20; R.m = slots.bytes+36; operation = op;
		assert(kprotect(run, nil));
		memcpy(&result, R.d, sizeof(result));
		assert(D2H(child)->ref == 2 && !memcmp(result->data, &child, sizeof(child)));
		destroy(result);
		assert(D2H(child)->ref == 1);
		destroy(child);
		assert(live == 0 && record.ref == 1 && Tptr.ref == 1 && Tlist.ref == 1);
		checks++;
	}

}

/* Null tails and pointer heads must fail before allocation or publication. */
static void
constructerrors(void)
{
	Type *types[] = {&record};
	Module owner = {0};
	Modlink module = {0};
	List *tail;
	void *source;
	WORD middle = 0;
	int op, before;

	owner.type = types; owner.ntype = 1; module.m = &owner; R.M = &module;
	record.size = 0; record.np = 0;
	if(constructoperation != 8)
	for(op = 0; op <= 7; op++){
		if(constructoperation >= 0 && constructoperation != op)continue;
		tail = nil; source = H;
		R.s = &source; R.d = &tail; R.m = &middle;
		operation = op == 7 ? 16 : op;
		before = allocations;
		assert(!kprotect(run, nil) && raised == exNilref);
		assert(tail == nil && allocations == before && live == 0);
		checks++;
	}
	if(constructoperation < 0 || constructoperation == 8){
		tail = H; source = nil;
		R.s = &source; R.d = &tail; operation = 3;
		before = allocations;
		assert(!kprotect(run, nil) && raised == exNilref);
		assert(tail == H && allocations == before && live == 0);
		checks++;
	}
}

/* Validate retained heap owners before allocation, marking or destination publication. */
static void
retainerrors(void)
{
	union {uvlong align;uchar bytes[96];} slots;
	uchar expected[96];
	List *source,*old,*value;
	Heap *heap;
	int sa,da,alias,boundary,color,before;
	ulong refs;

	for(sa=0;sa<8;sa++)for(da=0;da<8;da++)for(alias=0;alias<2;alias++)
	for(boundary=0;boundary<2;boundary++)for(color=0;color<4;color++){
		source=H;old=H;cons(0,&source);cons(0,&old);
		heap=D2H(source);refs=boundary ? (ulong)-1 : 0;
		heap->ref=refs;heap->color=color;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));
		R.s=slots.bytes+8+sa;R.d=alias ? R.s : slots.bytes+56+da;
		memmove(R.s,&source,sizeof(source));value=alias ? source : old;
		memmove(R.d,&value,sizeof(value));memmove(expected,slots.bytes,sizeof(expected));
		operation=3;before=allocations;raised=nil;
		assert(!kprotect(run,nil) && raised==exHeap);
		assert(allocations==before && heap->ref==refs && heap->color==color);
		assert(!memcmp(expected,slots.bytes,sizeof(expected)) && Tptr.ref==1);
		heap->ref=1;destroy(source);destroy(old);assert(live==0);checks++;
	}
}

/* Require complete ownership balance and guard integrity in each VM word model. */
int
main(int argc, char **argv)
{
	Tptr.size = sizeof(void*); Tptr.np = 1; Tptr.map[0] = 0x80;
	if(argc == 2){
		constructoperation = atoi(argv[1]);
		assert(constructoperation >= 0 && constructoperation <= 8);
		constructerrors();
	}else if(LIST_HEAD_CASE == 1)scalarheads();
	else if(LIST_HEAD_CASE == 2)rawheads();
	else if(LIST_HEAD_CASE == 3)listlengths();
	else if(LIST_HEAD_CASE == 4)constructors();
	else if(LIST_HEAD_CASE == 7)retainerrors();
	else if(LIST_HEAD_CASE == 5 || LIST_HEAD_CASE == 6)constructerrors();
	else{
		retainerrors(); scalars(); aliases(); payloads(); sizes(); mappedheads(); mappedheadbounds();
		scalarheads(); rawheads(); listlengths(); pointerheads(); constructors();
	}
	assert(live == 0 && Tlist.ref == 1 && Tptr.ref == 1 && worker.nerr == 0);
	printf("PASS lists: %d width/alias/failure cases, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
