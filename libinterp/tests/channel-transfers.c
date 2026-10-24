#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include "pool.h"
#include "runt.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static Prog current, peers[3], *running = &current;
static char *last_error;
static int live, nodes, allocations, fail_after = -1, awakened, removed, checks, acquired;
char exNomem[] = "allocation", exNilref[] = "nil", exAlt[] = "self alt";
char exHeap[] = "heap", exNegsize[] = "negative";
REG R;
Type Tarray = {1}, scalar = {1}, pointer = {1};
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);

/* Retain production Proc error frames while stubbing scheduler transitions. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *s) { last_error = s; nexterror(); }
int __lsan_is_turned_off(void) { return 1; }
Prog *currun(void) { return running; }
Prog *delrun(int state) { removed++; running->state = state; return running; }
void addrun(Prog *p) { awakened++; p->state = Pready; }
void acquire(void) { assert(!acquired); acquired = 1; }
void release(void) { assert(acquired); acquired = 0; }
void cblock(Prog *p) { (void)p; assert(!"unexpected native block"); }
int print(char *fmt, ...) { (void)fmt; assert(!"unexpected diagnostic"); return 0; }
GUARD

/* Bound the real heaparray allocation path and inject pool allocation failure. */
void*
poolalloc(Pool *pool, ulong n)
{
	void *p;

	(void)pool; allocations++;
	if(fail_after == 0 || n > 16384) return nil;
	if(fail_after > 0) fail_after--;
	p = calloc(1, n); assert(p); live++;
	return p;
}

/* Model ordinary reference destruction and recursively release actual array maps. */
void
destroy(void *v)
{
	Heap *h;
	Array *a;
	WORD i;

	if(v == H) return;
	h = D2H(v); assert(h->ref > 0);
	if(--h->ref) return;
	if(h->t == &Tarray){
		a = v;
		assert(a->root == H);
		for(i = 0; i < a->len; i++) freeptrs(a->data+i*a->t->size, a->t);
		a->t->ref--; Tarray.ref--;
	}
	free(h); live--;
}

/* Queue allocations are accounted separately from payload and buffer ownership. */
static void*
nodealloc(ulong n)
{
	void *p;

	p = calloc(1, n); assert(p); nodes++;
	return p;
}

/* Release a queue node exactly once. */
static void
nodefree(void *p)
{
	assert(p && nodes > 0); nodes--; free(p);
}

#define BIT(bt, nb) ((bt)&(1<<(nb)))
#define OP(fn) void fn(void)
#define W(r) (*(WORD*)R.r)
#define P(r) (*(WORD**)R.r)
#define C(r) (*(Channel**)R.r)
#define malloc nodealloc
#define free nodefree
IMPLEMENTATION
#undef malloc
#undef free

/* Create a live scalar payload with one external owner. */
static void*
payload(WORD value)
{
	Heap *h;

	h = nheap(sizeof(WORD));
	*(WORD*)H2D(void*, h) = value;
	return H2D(void*, h);
}

/* Initialize channel storage with the real heap prefix needed by alt dispatch. */
typedef struct TestChannel TestChannel;
struct TestChannel { Heap h; Channel c; };
static void
openchannel(TestChannel *box, int capacity, int pointers)
{
	Channel *c;

	memset(box, 0, sizeof(*box)); c = &box->c;
	box->h.ref = 1;
	c->send = nodealloc(sizeof(Progq)); c->recv = nodealloc(sizeof(Progq));
	c->buf = capacity ? H2D(Array*, heaparray(pointers ? &pointer : &scalar, capacity)) : H;
	c->mover = pointers ? movp : movw;
}

/* Release buffers and remaining queue nodes after checking all expected outcomes. */
static void
closechannel(Channel *c)
{
	Progq *q, *next;
	int direction;

	assert(D2H(c)->ref == 1);
	destroy(c->buf);
	for(direction = 0; direction < 2; direction++){
		q = direction ? c->recv : c->send;
		while(q){ next = q->next; nodefree(q); q = next; }
	}
}

/* Transmit one scalar or pointer through the actual VM send instruction. */
static void
sendvalue(Channel *c, WORD value, int pointers)
{
	void *p;
	union {uvlong align;uchar bytes[80];} slots;
	uchar before[80];
	int sa=(UWORD)value&7,ca=((UWORD)value*3+1)&7;

	p = pointers ? payload(value) : H;
	memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+8+sa;R.d=slots.bytes+40+ca;
	memmove(R.s,pointers ? (void*)&p : (void*)&value,pointers ? sizeof(p) : sizeof(value));
	memmove(R.d,&c,sizeof(c));memmove(before,slots.bytes,sizeof(before));
	isend();assert(!memcmp(before,slots.bytes,sizeof(before)));
	if(pointers) destroy(p);
}

/* Receive into an owned destination and verify exact pointer replacement. */
static void
receivevalue(Channel *c, WORD expected, int pointers)
{
	void *p,*destination;
	WORD value=-1;
	union {uvlong align;uchar bytes[80];} slots;
	uchar before[80];
	int ca=(UWORD)expected&7,da=((UWORD)expected*5+2)&7;

	p = pointers ? payload(-77) : H;
	memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+8+ca;R.d=destination=slots.bytes+40+da;
	memmove(R.s,&c,sizeof(c));memmove(destination,pointers ? (void*)&p : (void*)&value,pointers ? sizeof(p) : sizeof(value));
	memmove(before,slots.bytes,sizeof(before));irecv();
	memmove(before+40+da,destination,pointers ? sizeof(p) : sizeof(value));assert(!memcmp(before,slots.bytes,sizeof(before)));
	if(pointers){memmove(&p,destination,sizeof(p));assert(*(WORD*)p==expected && D2H(p)->ref==1);destroy(p);}
	else{memmove(&value,destination,sizeof(value));assert(value==expected);}
}

/* Compare long mixed ring-buffer sequences with an independent FIFO. */
static void
checkfifo(void)
{
	TestChannel box;
	Channel *c;
	WORD expected[32], next;
	uint state = 0x4649464f;
	int capacity, front, pointers, step, head, count, before;

	for(pointers = 0; pointers < 2; pointers++)
	for(capacity = 1; capacity <= 16; capacity++)
	for(front = 0; front < capacity; front++){
		openchannel(&box, capacity, pointers); c = &box.c; c->front = front;
		head = count = 0; next = 1; before = removed;
		for(step = 0; step < 512; step++){
			state = state*1664525U+1013904223U;
			if(count == 0 || (count < capacity && ((state>>16)&1))){
				expected[(head+count)%32] = next;
				sendvalue(c, next++, pointers); count++;
			}else{
				receivevalue(c, expected[head], pointers); head = (head+1)%32; count--;
			}
			assert(c->size == count && c->front >= 0 && c->front < capacity);
			assert(live == 1+(pointers ? count : 0) && removed == before);
			checks++;
		}
		while(count){ receivevalue(c, expected[head], pointers); head = (head+1)%32; count--; }
		closechannel(c); assert(!live && !nodes);
	}
}

/* A full buffer drains oldest-first while waking and appending queued senders. */
static void
checksenders(void)
{
	TestChannel box;
	Channel *c;
	WORD values[2];
	void *objects[2];
	int pointers, capacity, front, i;

	for(pointers = 0; pointers < 2; pointers++)
	for(capacity = 1; capacity <= 8; capacity++)
	for(front = 0; front < capacity; front++){
		openchannel(&box, capacity, pointers); c = &box.c; c->front = front;
		for(i = 1; i <= capacity; i++) sendvalue(c, i, pointers);
		for(i = 0; i < 2; i++){
			values[i] = capacity+1+i; objects[i] = pointers ? payload(values[i]) : H;
			memset(&peers[i], 0, sizeof(Prog)); peers[i].state = Psend;
			peers[i].ptr = pointers ? (void*)&objects[i] : (void*)&values[i];
			cqadd(&c->send, &peers[i]);
		}
		awakened = 0;
		for(i = 1; i <= capacity+2; i++){
			receivevalue(c, i, pointers);
			if(i <= 2){
				assert(awakened == i && peers[i-1].ptr == nil && peers[i-1].state == Pready);
				if(pointers) destroy(objects[i-1]);
			}
		}
		assert(c->size == 0 && c->send->prog == nil && !c->send->next && awakened == 2);
		closechannel(c); assert(!live && !nodes); checks++;
	}
}

/* Native best-effort sends can drop, deliver directly, or allocate bounded buffering. */
static void
checknative(void)
{
	TestChannel box;
	Channel *c;
	REG saved;
	void *input, *output;
	WORD value, received;
	int pointers, capacity, occupied, limit, result, before;

	for(pointers = 0; pointers < 2; pointers++)
	for(capacity = 0; capacity <= 3; capacity++)
	for(occupied = 0; occupied <= capacity; occupied++)
	for(limit = -2; limit <= 3; limit++){
		if(getenv("CHANNEL_NATIVE_LIMIT") && limit != atoi(getenv("CHANNEL_NATIVE_LIMIT"))) continue;
		openchannel(&box, capacity, pointers); c = &box.c;
		for(value = 1; value <= occupied; value++) sendvalue(c, value, pointers);
		input = pointers ? payload(99) : H; value = 99;
		memset(&R, 0x55, sizeof(R)); saved = R; before = allocations;
		result = csendalt(c, pointers ? (void*)&input : (void*)&value, pointers ? &pointer : &scalar, limit);
		assert(!memcmp(&R, &saved, sizeof(R)));
		assert(result == (capacity ? occupied < capacity : limit > 0));
		assert(allocations == before+(!capacity && limit > 0));
		assert(!removed);
		for(value = 1; value <= occupied; value++) receivevalue(c, value, pointers);
		if(result) receivevalue(c, 99, pointers);
		closechannel(c); assert(!live && !nodes); checks++;
	}
	for(pointers = 0; pointers < 2; pointers++)
	for(limit = -2; limit <= 3; limit++){
		if(getenv("CHANNEL_NATIVE_LIMIT") && limit != atoi(getenv("CHANNEL_NATIVE_LIMIT"))) continue;
		openchannel(&box, 0, pointers); c = &box.c;
		input = pointers ? payload(99) : H; output = pointers ? payload(-1) : H;
		value = 99; received = -1;
		memset(&peers[0], 0, sizeof(Prog)); peers[0].state = Precv;
		peers[0].ptr = pointers ? (void*)&output : (void*)&received;
		cqadd(&c->recv, &peers[0]); awakened = 0;
		memset(&R, 0x55, sizeof(R)); saved = R; before = allocations;
		assert(csendalt(c, pointers ? (void*)&input : (void*)&value, pointers ? &pointer : &scalar, limit));
		assert(!memcmp(&R, &saved, sizeof(R)) && allocations == before);
		assert(awakened == 1 && peers[0].ptr == nil && peers[0].state == Pready);
		if(pointers){ assert(*(WORD*)output == 99 && D2H(output)->ref == 1); destroy(output); }
		else assert(received == 99);
		closechannel(c); assert(!live && !nodes); checks++;
	}
}

/* Ready scalar/array guards select only live peers or available buffer slots. */
static void
checkready(void)
{
	TestChannel boxes[2];
	Channel *c, *members[5];
	struct { Heap h; Array a; } array;
	struct { int nsend, nrecv; Altc ac[3]; } storage;
	union { void *align; uchar bytes[32]; } space;
	Alt *a;
	WORD value, result, received, *index;
	void *input, *output, *destination;
	int sending, pointers, buffered, wide, repeat, i;

	for(sending = 0; sending < 2; sending++)
	for(pointers = 0; pointers < 2; pointers++)
	for(buffered = 0; buffered < 2; buffered++)
	for(wide = 0; wide <= !sending; wide++)
	for(repeat = 0; repeat < 64; repeat++){
		openchannel(&boxes[0], 0, pointers);
		openchannel(&boxes[1], buffered ? 2 : 0, pointers); c = &boxes[1].c;
		memset(&array, 0, sizeof(array)); array.h.t = &Tarray;
		members[0] = members[4] = H; members[1] = &boxes[0].c;
		members[2] = members[3] = c; array.a.len = 5; array.a.data = (uchar*)members;
		value = 99; received = -1;
		input = pointers ? payload(value) : H;
		output = pointers ? payload(-77) : H;
		index = (WORD*)(space.bytes+sizeof(void*)-sizeof(WORD)); *index = -1;
		destination = wide ? (void*)(index+1) : pointers ? (void*)&output : (void*)&received;
		if(wide){
			if(pointers) *(void**)destination = output;
			else *(WORD*)destination = received;
		}
		if(buffered){
			if(!sending) sendvalue(c, 99, pointers);
		}else{
			memset(&peers[0], 0, sizeof(Prog)); peers[0].state = sending ? Precv : Psend;
			peers[0].ptr = sending ? destination : pointers ? (void*)&input : (void*)&value;
			cqadd(sending ? &c->recv : &c->send, &peers[0]);
		}
		a = (Alt*)&storage; a->nsend = sending ? 3 : 0; a->nrecv = sending ? 0 : 3;
		for(i = 0; i < 3; i++){
			a->ac[i].c = i == 1 ? (!sending && wide ? (Channel*)&array.a : c) : &boxes[0].c;
			a->ac[i].ptr = sending ? (pointers ? (void*)&input : (void*)&value) :
				i == 1 && wide ? (void*)index : destination;
		}
		result = -1; R.s = a; R.d = &result; awakened = 0;
		xecalt(1); assert(result == 1 && !removed);
		assert(awakened == !buffered);
		if(!buffered) assert(peers[0].ptr == nil && peers[0].state == Pready);
		if(sending && buffered){
			if(pointers){ destroy(input); input = H; }
			receivevalue(c, 99, pointers);
		}
		else{
			if(wide && !sending) assert(*index == 2 || *index == 3);
			if(pointers){
				output = *(void**)destination; assert(*(WORD*)output == 99);
			}else assert(*(WORD*)destination == 99);
		}
		if(pointers){ destroy(input); destroy(output); }
		for(i = 0; i < 2; i++){
			assert(boxes[i].c.recv->prog == nil && !boxes[i].c.recv->next);
			assert(boxes[i].c.send->prog == nil && !boxes[i].c.send->next);
			closechannel(&boxes[i].c);
		}
		assert(!live && !nodes && !worker.nerr); checks++;
	}
}

/* Empty receive-array guards neither form data pointers nor hide a ready scalar guard. */
static void
checkemptyguards(void)
{
	struct { Heap h; Array a; } array;
	struct { int nsend, nrecv; Altc ac[4]; } storage;
	TestChannel box;
	Alt *a;
	WORD tuple[2], result, received;
	int empty, position, representation, i, count;

	for(empty = 0; empty <= 3; empty++)
	for(position = -1; position <= empty; position++)
	for(representation = 0; representation < 2; representation++){
		memset(&array, 0, sizeof(array));
		array.h.t = &Tarray;
		array.a.len = 0;
		array.a.data = representation ? H : nil;
		if(position >= 0){
			openchannel(&box, 2, 0);
			sendvalue(&box.c, 99, 0);
		}
		count = empty+(position >= 0);
		a = (Alt*)&storage;
		a->nsend = 0;
		a->nrecv = count;
		tuple[0] = -11; tuple[1] = -22;
		received = -33; result = -44;
		for(i = 0; i < count; i++){
			a->ac[i].c = i == position ? &box.c : (Channel*)&array.a;
			a->ac[i].ptr = i == position ? &received : tuple;
		}
		R.s = a; R.d = &result;
		xecalt(position >= 0);
		assert(result == (position >= 0 ? position : count));
		assert(tuple[0] == -11 && tuple[1] == -22 && !removed);
		if(position >= 0){
			assert(received == 99 && box.c.size == 0);
			assert(box.c.recv->prog == nil && box.c.recv->next == nil);
			closechannel(&box.c);
		}else assert(received == -33);
		assert(!live && !nodes && !worker.nerr);
		checks++;
	}
}

/* Wake actual blocked alt registrations from a second modeled VM process. */
static void
checkblockedwake(void)
{
	TestChannel boxes[2];
	Channel *c, *members[5];
	struct { Heap h; Array a; } array;
	struct { int nsend, nrecv; Altc ac[3]; } storage;
	union { void *align; uchar bytes[32]; } space;
	Alt *a;
	WORD value, result, received, *index;
	void *input, *output, *destination;
	int sending, pointers, wide, repeat, i, before;

	for(sending = 0; sending < 2; sending++)
	for(pointers = 0; pointers < 2; pointers++)
	for(wide = 0; wide <= !sending; wide++)
	for(repeat = 0; repeat < 32; repeat++){
		openchannel(&boxes[0], 0, pointers); openchannel(&boxes[1], 0, pointers); c = &boxes[1].c;
		memset(&array, 0, sizeof(array)); array.h.t = &Tarray;
		members[0] = members[4] = H; members[1] = &boxes[0].c;
		members[2] = members[3] = c; array.a.len = 5; array.a.data = (uchar*)members;
		value = 99; received = -1;
		input = pointers ? payload(value) : H; output = pointers ? payload(-77) : H;
		index = (WORD*)(space.bytes+sizeof(void*)-sizeof(WORD)); *index = -1;
		destination = wide ? (void*)(index+1) : pointers ? (void*)&output : (void*)&received;
		if(wide){ if(pointers) *(void**)destination = output; else *(WORD*)destination = received; }
		a = (Alt*)&storage; a->nsend = sending ? 3 : 0; a->nrecv = sending ? 0 : 3;
		for(i = 0; i < 3; i++){
			a->ac[i].c = i == 1 ? (!sending && wide ? (Channel*)&array.a : c) : &boxes[0].c;
			a->ac[i].ptr = sending ? (pointers ? (void*)&input : (void*)&value) :
				i == 1 && wide ? (void*)index : destination;
		}
		result = -1; R.s = a; R.d = &result;
		current.state = Pready; awakened = 0; before = removed;
		xecalt(1);
		assert(removed == before+1 && current.state == Palt && result == -1);
		running = &peers[0]; peers[0].state = Pready;
		if(sending){ R.s = &c; R.d = destination; irecv(); }
		else{ R.s = pointers ? (void*)&input : (void*)&value; R.d = &c; isend(); }
		running = &current;
		assert(current.state == Pready && current.ptr == nil && result == 1 && awakened == 1);
		assert(removed == before+1);
		if(wide && !sending) assert(*index == 2);
		if(pointers){
			output = *(void**)destination;
			assert(output == input && *(WORD*)output == 99 && D2H(output)->ref == 2);
			destroy(input); destroy(output);
		}else assert(*(WORD*)destination == 99);
		for(i = 0; i < 2; i++){
			assert(boxes[i].c.recv->prog == nil && !boxes[i].c.recv->next);
			assert(boxes[i].c.send->prog == nil && !boxes[i].c.send->next);
			closechannel(&boxes[i].c);
		}
		assert(!live && !nodes && !worker.nerr); checks++;
	}
	removed = 0;
}

typedef struct NativeAttempt NativeAttempt;
struct NativeAttempt
{
	Channel *channel;
	void *input;
	Type *type;
	int result;
};

/* Save native send outcomes outside the runtime's nonlocal error frame. */
static int
attemptnative(void *arg)
{
	NativeAttempt *a;

	a = arg;
	a->result = csendalt(a->channel, a->input, a->type, 3);
	return 1;
}

/* Allocation errors preserve unconsumed inputs and permit a successful retry. */
static void
checkfaults(void)
{
	TestChannel box;
	NativeAttempt attempt;
	REG saved;
	void *input;
	WORD value = 99;
	int pointers;

	for(pointers = 0; pointers < 2; pointers++){
		openchannel(&box, 0, pointers);
		input = pointers ? payload(value) : H;
		attempt.channel = &box.c; attempt.type = pointers ? &pointer : &scalar;
		attempt.input = pointers ? (void*)&input : (void*)&value;
		attempt.result = -1;
		memset(&R, 0x55, sizeof(R)); saved = R;
		fail_after = 0;
		assert(!kprotect(attemptnative, &attempt) && last_error == exHeap);
		fail_after = -1;
		assert(!memcmp(&R, &saved, sizeof(R)) && box.c.buf == H && attempt.result == -1);
		if(pointers) assert(D2H(input)->ref == 1);
		attempt.channel = H;
		assert(!kprotect(attemptnative, &attempt) && last_error == exNilref);
		if(pointers) assert(D2H(input)->ref == 1);
		attempt.channel = &box.c;
		assert(kprotect(attemptnative, &attempt) && attempt.result == 1);
		assert(!memcmp(&R, &saved, sizeof(R)));
		receivevalue(&box.c, 99, pointers);
		closechannel(&box.c); assert(!live && !nodes && !worker.nerr); checks++;
	}
}

/* Run the real FileIO close-notification caller with no receiver available. */
static int
notifyservice(void *file)
{
	srvunblock(file, 42);
	return 1;
}

/* Service close uses the negative-capacity sentinel for both read and write requests. */
static void
checkservice(void)
{
	TestChannel boxes[2];
	SrvFile file;
	Type readtype = {1}, writetype = {1};
	int mode, before;

	/* The temporary WORD32 model retains host pointers, so its generated map
	 * quantum is not a native 32-bit FileIO ABI. Test that path in WORD64. */
	if(sizeof(WORD) != sizeof(void*)) return;
	readtype.size = sizeof(Sys_FileIO_read); readtype.np = 1; readtype.map[0] = 0x10;
	writetype.size = sizeof(Sys_FileIO_write); writetype.np = 1; writetype.map[0] = 0x50;
	for(mode = 0; mode < 4; mode++){
		openchannel(&boxes[0], 0, 0); openchannel(&boxes[1], 0, 0);
		boxes[0].c.mid.t = &readtype; boxes[1].c.mid.t = &writetype;
		memset(&file, 0, sizeof(file));
		file.read = mode&1 ? &boxes[0].c : H;
		file.write = mode&2 ? &boxes[1].c : H;
		before = allocations;
		assert(kprotect(notifyservice, &file));
		assert(!acquired && !removed && allocations == before);
		assert(boxes[0].c.buf == H && boxes[1].c.buf == H);
		closechannel(&boxes[0].c); closechannel(&boxes[1].c);
		assert(!live && !nodes && !worker.nerr); checks++;
	}
}

/* Require all references, descriptors and queue nodes to balance across each matrix. */
int
main(void)
{
	scalar.size = sizeof(WORD); pointer.size = sizeof(void*); pointer.np = 1; pointer.map[0] = 0x80;
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "fifo")) checkfifo();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "senders")) checksenders();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "native")) checknative();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "ready")) checkready();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "empty")) checkemptyguards();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "service")) checkservice();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "faults")) checkfaults();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "wake")) checkblockedwake();
	assert(!live && !nodes && !worker.nerr && !removed);
	assert(scalar.ref == 1 && pointer.ref == 1 && Tarray.ref == 1);
	printf("PASS channel transfers: %d FIFO/buffer/wakeup/native/reference scenarios, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
