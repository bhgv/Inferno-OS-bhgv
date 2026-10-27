#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include "pool.h"
#include "runt.h"
#include <stdlib.h>
#include <stdio.h>


/* Retain all inherited queue and transfer oracles when NDEBUG is enabled. */
static void
channelpredicate(int condition)
{
	if(!condition)
		abort();
}
#define channelcheck(condition) channelpredicate(!!(condition)) // Normalize pointer and scalar predicates.

static Proc worker;
static Prog current, peers[3], *running = &current;
static char *last_error;
static int live, nodes, allocations, fail_after = -1, awakened, removed, checks, acquired;
char exNomem[] = "allocation", exNilref[] = "nil", exAlt[] = "self alt", exBounds[] = "bounds";
char exHeap[] = "heap", exNegsize[] = "negative";
REG R;
Type Tarray = {1}, scalar = {1}, pointer = {1};
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);


/* Retain production Proc error frames while stubbing scheduler transitions. */
Proc *getup(void) { return &worker; }
void nexterror(void) { channelcheck(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *s) { last_error = s; nexterror(); }
int __lsan_is_turned_off(void) { return 1; }
Prog *currun(void) { return running; }
Prog *delrun(int state) { removed++; running->state = state; return running; }
void addrun(Prog *p) { awakened++; p->state = Pready; }
void acquire(void) { channelcheck(!acquired); acquired = 1; }
void release(void) { channelcheck(acquired); acquired = 0; }
void cblock(Prog *p) { (void)p; channelcheck(!"unexpected native block"); }
int print(char *fmt, ...) { (void)fmt; channelcheck(!"unexpected diagnostic"); return 0; }
GUARD


/* Bound the real heaparray allocation path and inject pool allocation failure. */
void*
poolalloc(Pool *pool, ulong n)
{
	void *p;

	(void)pool; allocations++;
	if(fail_after == 0 || n > 16384) return nil;
	if(fail_after > 0) fail_after--;
	p = calloc(1, n); channelcheck(p); live++;
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
	h = D2H(v); channelcheck(h->ref > 0);
	if(--h->ref) return;
	if(h->t == &Tarray){
		a = v;
		channelcheck(a->root == H);
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

	p = calloc(1, n); channelcheck(p); nodes++;
	return p;
}


/* Release a queue node exactly once. */
static void
nodefree(void *p)
{
	channelcheck(p && nodes > 0); nodes--; free(p);
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

	channelcheck(D2H(c)->ref == 1);
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
	static void *p;
	static union {
		uvlong align; // Natural alignment for independently shifted operand cells.
		uchar bytes[80]; // Stable slot image across protected error paths.
	} slots;
	static uchar before[80];
	int sa=(UWORD)value&7,ca=((UWORD)value*3+1)&7;

	p = pointers ? payload(value) : H;
	memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+8+sa;R.d=slots.bytes+40+ca;
	memmove(R.s,pointers ? (void*)&p : (void*)&value,pointers ? sizeof(p) : sizeof(value));
	memmove(R.d,&c,sizeof(c));memmove(before,slots.bytes,sizeof(before));
	isend();channelcheck(!memcmp(before,slots.bytes,sizeof(before)));
	if(pointers) destroy(p);
}


/* Receive into an owned destination and verify exact pointer replacement. */
static void
receivevalue(Channel *c, WORD expected, int pointers)
{
	static void *p,*destination;
	static WORD value;
	static union {
		uvlong align; // Natural alignment for independently shifted operand cells.
		uchar bytes[80]; // Stable slot image across protected error paths.
	} slots;
	static uchar before[80];
	int ca=(UWORD)expected&7,da=((UWORD)expected*5+2)&7;

	value=-1;
	p = pointers ? payload(-77) : H;
	memset(slots.bytes,0xa5,sizeof(slots.bytes));R.s=slots.bytes+8+ca;R.d=destination=slots.bytes+40+da;
	memmove(R.s,&c,sizeof(c));memmove(destination,pointers ? (void*)&p : (void*)&value,pointers ? sizeof(p) : sizeof(value));
	memmove(before,slots.bytes,sizeof(before));irecv();
	memmove(before+40+da,destination,pointers ? sizeof(p) : sizeof(value));channelcheck(!memcmp(before,slots.bytes,sizeof(before)));
	if(pointers){memmove(&p,destination,sizeof(p));channelcheck(*(WORD*)p==expected && D2H(p)->ref==1);destroy(p);}
	else{memmove(&value,destination,sizeof(value));channelcheck(value==expected);}
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
			channelcheck(c->size == count && c->front >= 0 && c->front < capacity);
			channelcheck(live == 1+(pointers ? count : 0) && removed == before);
			checks++;
		}
		while(count){ receivevalue(c, expected[head], pointers); head = (head+1)%32; count--; }
		closechannel(c); channelcheck(!live && !nodes);
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
				channelcheck(awakened == i && peers[i-1].ptr == nil && peers[i-1].state == Pready);
				if(pointers) destroy(objects[i-1]);
			}
		}
		channelcheck(c->size == 0 && c->send->prog == nil && !c->send->next && awakened == 2);
		closechannel(c); channelcheck(!live && !nodes); checks++;
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
		channelcheck(!memcmp(&R, &saved, sizeof(R)));
		channelcheck(result == (capacity ? occupied < capacity : limit > 0));
		channelcheck(allocations == before+(!capacity && limit > 0));
		channelcheck(!removed);
		for(value = 1; value <= occupied; value++) receivevalue(c, value, pointers);
		if(result) receivevalue(c, 99, pointers);
		closechannel(c); channelcheck(!live && !nodes); checks++;
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
		channelcheck(csendalt(c, pointers ? (void*)&input : (void*)&value, pointers ? &pointer : &scalar, limit));
		channelcheck(!memcmp(&R, &saved, sizeof(R)) && allocations == before);
		channelcheck(awakened == 1 && peers[0].ptr == nil && peers[0].state == Pready);
		if(pointers){ channelcheck(*(WORD*)output == 99 && D2H(output)->ref == 1); destroy(output); }
		else channelcheck(received == 99);
		closechannel(c); channelcheck(!live && !nodes); checks++;
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
	union { void *align; uchar bytes[64]; } space;
	uchar rawalt[sizeof(storage)+8],rawmembers[sizeof(members)+8],rawresult[24];
	Alt *a;
	WORD value, result, received, selected, *index;
	void *input, *output, *destination;
	int sending, pointers, buffered, wide, repeat, i;

	for(sending = 0; sending < 2; sending++)
	for(pointers = 0; pointers < 2; pointers++)
	for(buffered = 0; buffered < 2; buffered++)
	for(wide = 0; wide <= !sending; wide++)
	for(repeat = 0; repeat < 512; repeat++){
		openchannel(&boxes[0], 0, pointers);
		openchannel(&boxes[1], buffered ? 2 : 0, pointers); c = &boxes[1].c;
		memset(&array, 0, sizeof(array)); array.h.t = &Tarray;
		members[0] = members[4] = H; members[1] = &boxes[0].c;
		members[2] = members[3] = c; array.a.len = 5;memmove(rawmembers+((repeat+(repeat>>3)+(repeat>>6))&7),members,sizeof(members));
		array.a.data=rawmembers+((repeat+(repeat>>3)+(repeat>>6))&7);
		value = 99; received = -1;
		input = pointers ? payload(value) : H;
		output = pointers ? payload(-77) : H;
		index=(WORD*)(space.bytes+16+((repeat>>6)&7));selected=-1;memmove(index,&selected,sizeof(selected));
		destination = wide ? (void*)((uchar*)index+sizeof(WORD)) : pointers ? (void*)&output : (void*)&received;
		if(wide){
			if(pointers) memmove(destination,&output,sizeof(output));
			else memmove(destination,&received,sizeof(received));
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
		memmove(rawalt+(repeat&7),a,sizeof(storage));a=(Alt*)(rawalt+(repeat&7));
		result=-1;memmove(rawresult+((repeat>>3)&7),&result,sizeof(result));R.s=a;R.d=rawresult+((repeat>>3)&7); awakened = 0;
		xecalt(1);memmove(&result,rawresult+((repeat>>3)&7),sizeof(result));channelcheck(result == 1 && !removed);
		channelcheck(awakened == !buffered);
		if(!buffered) channelcheck(peers[0].ptr == nil && peers[0].state == Pready);
		if(sending && buffered){
			if(pointers){ destroy(input); input = H; }
			receivevalue(c, 99, pointers);
		}
		else{
			if(wide && !sending){memmove(&selected,index,sizeof(selected));channelcheck(selected==2 || selected==3);}
			if(pointers){
				memmove(&output,destination,sizeof(output)); channelcheck(*(WORD*)output == 99);
			}else{memmove(&received,destination,sizeof(received));channelcheck(received==99);}
		}
		if(pointers){ destroy(input); destroy(output); }
		for(i = 0; i < 2; i++){
			channelcheck(boxes[i].c.recv->prog == nil && !boxes[i].c.recv->next);
			channelcheck(boxes[i].c.send->prog == nil && !boxes[i].c.send->next);
			closechannel(&boxes[i].c);
		}
		channelcheck(!live && !nodes && !worker.nerr); checks++;
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
		channelcheck(result == (position >= 0 ? position : count));
		channelcheck(tuple[0] == -11 && tuple[1] == -22 && !removed);
		if(position >= 0){
			channelcheck(received == 99 && box.c.size == 0);
			channelcheck(box.c.recv->prog == nil && box.c.recv->next == nil);
			closechannel(&box.c);
		}else channelcheck(received == -33);
		channelcheck(!live && !nodes && !worker.nerr);
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
	union { void *align; uchar bytes[64]; } space;
	uchar rawalt[sizeof(storage)+8],rawmembers[sizeof(members)+8],rawresult[24];
	Alt *a;
	WORD value, result, received, selected, *index;
	void *input, *output, *destination;
	int sending, pointers, wide, repeat, i, before;

	for(sending = 0; sending < 2; sending++)
	for(pointers = 0; pointers < 2; pointers++)
	for(wide = 0; wide <= !sending; wide++)
	for(repeat = 0; repeat < 512; repeat++){
		openchannel(&boxes[0], 0, pointers); openchannel(&boxes[1], 0, pointers); c = &boxes[1].c;
		memset(&array, 0, sizeof(array)); array.h.t = &Tarray;
		members[0] = members[4] = H; members[1] = &boxes[0].c;
		members[2] = members[3] = c; array.a.len = 5;memmove(rawmembers+((repeat+(repeat>>3)+(repeat>>6))&7),members,sizeof(members));
		array.a.data=rawmembers+((repeat+(repeat>>3)+(repeat>>6))&7);
		value = 99; received = -1;
		input = pointers ? payload(value) : H; output = pointers ? payload(-77) : H;
		index=(WORD*)(space.bytes+16+((repeat>>6)&7));selected=-1;memmove(index,&selected,sizeof(selected));
		destination = wide ? (void*)((uchar*)index+sizeof(WORD)) : pointers ? (void*)&output : (void*)&received;
		if(wide){ if(pointers) memmove(destination,&output,sizeof(output)); else memmove(destination,&received,sizeof(received)); }
		a = (Alt*)&storage; a->nsend = sending ? 3 : 0; a->nrecv = sending ? 0 : 3;
		for(i = 0; i < 3; i++){
			a->ac[i].c = i == 1 ? (!sending && wide ? (Channel*)&array.a : c) : &boxes[0].c;
			a->ac[i].ptr = sending ? (pointers ? (void*)&input : (void*)&value) :
				i == 1 && wide ? (void*)index : destination;
		}
		memmove(rawalt+(repeat&7),a,sizeof(storage));a=(Alt*)(rawalt+(repeat&7));
		result=-1;memmove(rawresult+((repeat>>3)&7),&result,sizeof(result));R.s=a;R.d=rawresult+((repeat>>3)&7);
		current.state = Pready; awakened = 0; before = removed;
		xecalt(1);
		channelcheck(removed == before+1 && current.state == Palt && result == -1);
		running = &peers[0]; peers[0].state = Pready;
		if(sending){ R.s = &c; R.d = destination; irecv(); }
		else{ R.s = pointers ? (void*)&input : (void*)&value; R.d = &c; isend(); }
		running = &current;memmove(&result,rawresult+((repeat>>3)&7),sizeof(result));
		channelcheck(current.state == Pready && current.ptr == nil && result == 1 && awakened == 1);
		channelcheck(removed == before+1);
		if(wide && !sending){memmove(&selected,index,sizeof(selected));channelcheck(selected==2);}
		if(pointers){
			memmove(&output,destination,sizeof(output));
			channelcheck(output == input && *(WORD*)output == 99 && D2H(output)->ref == 2);
			destroy(input); destroy(output);
		}else{memmove(&received,destination,sizeof(received));channelcheck(received==99);}
		for(i = 0; i < 2; i++){
			channelcheck(boxes[i].c.recv->prog == nil && !boxes[i].c.recv->next);
			channelcheck(boxes[i].c.send->prog == nil && !boxes[i].c.send->next);
			closechannel(&boxes[i].c);
		}
		channelcheck(!live && !nodes && !worker.nerr); checks++;
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
		channelcheck(!kprotect(attemptnative, &attempt) && last_error == exHeap);
		fail_after = -1;
		channelcheck(!memcmp(&R, &saved, sizeof(R)) && box.c.buf == H && attempt.result == -1);
		if(pointers) channelcheck(D2H(input)->ref == 1);
		attempt.channel = nil;
		channelcheck(!kprotect(attemptnative, &attempt) && last_error == exNilref);
		channelcheck(!memcmp(&R,&saved,sizeof(R)) && attempt.result==-1);
		if(pointers)channelcheck(D2H(input)->ref==1);
		attempt.channel = H;
		channelcheck(!kprotect(attemptnative, &attempt) && last_error == exNilref);
		if(pointers) channelcheck(D2H(input)->ref == 1);
		attempt.channel = &box.c;
		box.c.buf = nil;
		channelcheck(!kprotect(attemptnative, &attempt) && last_error == exNilref);
		channelcheck(!memcmp(&R,&saved,sizeof(R)) && attempt.result==-1);
		if(pointers) channelcheck(D2H(input)->ref==1);
		box.c.buf = H;
		channelcheck(kprotect(attemptnative, &attempt) && attempt.result == 1);
		channelcheck(!memcmp(&R, &saved, sizeof(R)));
		receivevalue(&box.c, 99, pointers);
		closechannel(&box.c); channelcheck(!live && !nodes && !worker.nerr); checks++;
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
		channelcheck(kprotect(notifyservice, &file));
		channelcheck(!acquired && !removed && allocations == before);
		channelcheck(boxes[0].c.buf == H && boxes[1].c.buf == H);
		closechannel(&boxes[0].c); closechannel(&boxes[1].c);
		channelcheck(!live && !nodes && !worker.nerr); checks++;
	}
}


/* Route the actual receive instruction through the production exception protector. */
static int
retainrecv(void *unused)
{
	(void)unused;
	irecv();
	return 1;
}


/* A bad temporary owner must not dequeue, mutate ring data or publish a receive result. */
static void
checkretains(void)
{
	/* Stable oracle storage survives error paths with different instrumented stack depths. */
	static TestChannel box,before;
	static Channel *c;
	static Array *buffer;
	static Prog peerbefore;
	static Progq sendbefore,recvbefore;
	static REG saved;
	static union {
		uvlong align; // Natural alignment for independently shifted operand cells.
		uchar bytes[80]; // Stable slot image across protected error paths.
	} slots;
	static union {
		void *pointer; // Native pointer payload for a queued sender.
		WORD word; // Scalar payload in the selected VM word model.
	} outgoing;
	static uchar expected[80],data[32];
	void *empty=H;
	int queued,kind,pointers,sa,da,oldlive,oldnodes,oldallocations;
	char *failure,*selection=getenv("CHANNEL_RETAIN_KIND");
	char *queueselection=getenv("CHANNEL_RETAIN_QUEUED");

	for(queued=0;queued<2;queued++)for(kind=0;kind<5;kind++)
	for(pointers=0;pointers<2;pointers++)for(sa=0;sa<8;sa++)for(da=0;da<8;da++){
		if((!queued && kind>=3) || (selection && atoi(selection)!=kind) ||
		   (queueselection && atoi(queueselection)!=queued))continue;
		openchannel(&box,2,pointers);c=&box.c;buffer=c->buf;
		sendvalue(c,1,pointers);if(queued)sendvalue(c,2,pointers);
		memset(&peers[0],0,sizeof(peers[0]));
		outgoing.pointer=H;if(!pointers)outgoing.word=3;
		if(queued){peers[0].state=Psend;peers[0].ptr=&outgoing;cqadd(&c->send,&peers[0]);}
		failure=kind==2 ? exNilref : exHeap;
		if(kind==0)D2H(buffer)->ref=0;
		if(kind==1)D2H(buffer)->ref=(ulong)-1;
		if(kind==2)c->buf=nil;
		if(kind==3)box.h.ref=0;
		if(kind==4)box.h.ref=(ulong)-1;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));memset(&R,0,sizeof(R));
		R.s=slots.bytes+8+sa;R.d=slots.bytes+40+da;
		memmove(R.s,&c,sizeof(c));memmove(R.d,&empty,sizeof(empty));
		memmove(expected,slots.bytes,sizeof(expected));memmove(data,buffer->data,buffer->len*buffer->t->size);
		before=box;saved=R;peerbefore=peers[0];sendbefore=*c->send;recvbefore=*c->recv;
		oldlive=live;oldnodes=nodes;oldallocations=allocations;awakened=removed=0;last_error=nil;
		channelcheck(!kprotect(retainrecv,nil) && last_error==failure);
		channelcheck(!memcmp(&before,&box,sizeof(box)) && !memcmp(&saved,&R,sizeof(R)));
		channelcheck(!memcmp(&peerbefore,&peers[0],sizeof(peerbefore)) && !memcmp(&sendbefore,c->send,sizeof(sendbefore)));
		channelcheck(!memcmp(&recvbefore,c->recv,sizeof(recvbefore)) && !memcmp(expected,slots.bytes,sizeof(expected)));
		channelcheck(!memcmp(data,buffer->data,buffer->len*buffer->t->size));
		channelcheck(live==oldlive && nodes==oldnodes && allocations==oldallocations && !awakened && !removed && !worker.nerr);
		box.h.ref=1;D2H(buffer)->ref=1;c->buf=buffer;
		closechannel(c);channelcheck(!live && !nodes);checks++;
	}
}


/* Route an ordinary VM send through the production error protector. */
static int
ringsend(void *unused)
{
	(void)unused;
	isend();
	return 1;
}


/* Reject null buffered storage before publishing operands or queue registrations. */
static void
checknullbuffer(void)
{
	TestChannel box, before;
	REG saved;
	union {uvlong align;uchar bytes[80];} slots;
	uchar expected[80];
	Channel *c;
	WORD value=77;
	int sa,da,oldallocations;

	for(sa=0;sa<8;sa++)for(da=0;da<8;da++){
		openchannel(&box,0,0);c=&box.c;c->buf=nil;
		memset(slots.bytes,0xa5,sizeof(slots.bytes));memset(&R,0,sizeof(R));
		R.s=slots.bytes+8+sa;R.d=slots.bytes+40+da;
		memmove(R.s,&value,sizeof(value));memmove(R.d,&c,sizeof(c));
		saved=R;before=box;memmove(expected,slots.bytes,sizeof(expected));
		oldallocations=allocations;awakened=removed=0;
		channelcheck(!kprotect(ringsend,nil) && last_error==exNilref);
		channelcheck(!memcmp(&saved,&R,sizeof(R)) && !memcmp(&before,&box,sizeof(box)));
		channelcheck(!memcmp(expected,slots.bytes,sizeof(expected)));
		channelcheck(!c->send->prog && !c->recv->prog && !awakened && !removed);
		channelcheck(allocations==oldallocations && !worker.nerr);
		c->buf=H;closechannel(c);channelcheck(!live && !nodes);checks++;
	}
}


/* Exercise large ring metadata while only the selected physical slot is backed. */
static void
checkringedges(void)
{
	TestChannel box;
	Channel *c;
	Array *a;
	uchar *base;
	union {uvlong align;uchar bytes[80];} slots;
	WORD value, result;
	int direction, profile, shift, index, oldfront;
	char *selection=getenv("CHANNEL_RING_DIRECTION");

	for(direction=0;direction<2;direction++)
	for(profile=0;profile<3;profile++)for(shift=0;shift<8;shift++){
		if(selection && atoi(selection)!=direction)continue;
		openchannel(&box,4,0);c=&box.c;a=c->buf;base=a->data;
		a->len=0x7fffffff;
		c->front=profile==0 ? 0x7ffffffe : profile==1 ? 0x40000000 : 7;
		c->size=direction ? 2 : 1;oldfront=c->front;
		index=direction ? (profile==0 ? 1 : oldfront+2) : oldfront;
		a->data=(uchar*)((uintptr)base-(uintptr)index*scalar.size);
		value=12345;result=0;memmove(base,&value,sizeof(value));
		memset(slots.bytes,0xa5,sizeof(slots.bytes));memset(&R,0,sizeof(R));
		if(direction){
			value=67890;memmove(slots.bytes+shift,&value,sizeof(value));
			channelcheck(cputb(c,slots.bytes+shift)==1 && c->size==3 && c->front==oldfront);
			memmove(&result,base,sizeof(result));channelcheck(result==value);
		}else{
			channelcheck(cgetb(c,slots.bytes+shift)==1 && c->size==0);
			channelcheck(c->front==(oldfront==0x7ffffffe ? 0 : oldfront+1));
			memmove(&result,slots.bytes+shift,sizeof(result));channelcheck(result==value);
		}
		channelcheck(D2H(a)->ref==1);
		a->data=base;a->len=4;c->front=c->size=0;
		closechannel(c);channelcheck(!live && !nodes);checks++;
	}
}


/* Require all references, descriptors and queue nodes to balance across each matrix. */
int
main(void)
{
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "ring")) {scalar.size=sizeof(WORD);checkringedges();checknullbuffer();}
	scalar.size = sizeof(WORD); pointer.size = sizeof(void*); pointer.np = 1; pointer.map[0] = 0x80;
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "retains"))checkretains();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "fifo")) checkfifo();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "senders")) checksenders();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "native")) checknative();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "ready")) checkready();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "empty")) checkemptyguards();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "service")) checkservice();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "faults")) checkfaults();
	if(!getenv("CHANNEL_TRANSFER_CASE") || !strcmp(getenv("CHANNEL_TRANSFER_CASE"), "wake")) checkblockedwake();
	channelcheck(!live && !nodes && !worker.nerr && !removed);
	channelcheck(scalar.ref == 1 && pointer.ref == 1 && Tarray.ref == 1);
	printf("PASS channel transfers: %d FIFO/buffer/wakeup/native/reference scenarios, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
