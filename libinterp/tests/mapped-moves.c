#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include "pool.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>

REG R;
Pool *heapmem;
int mutator, nprop;
void (*heapmonitor)(int, void*, ulong);
Type channelowner = {1}, Tarray = {1}, leaf = {1, 0, 0, sizeof(WORD)};
char exHeap[] = "heap", exNegsize[] = "negative", exNomem[] = "memory", exNilref[] = "nil", exModule[] = "module", exType[]="type";
static int live, checks, wakes;
static struct { Heap *heap; ulong size; } blocks[256];
static Prog current, peer;
static jmp_buf boundary, nested[8]; // Oracle and protected buffered callback error frames.
static char *nestedmessage; // Preserve the exact original VM error through temporary-owner cleanup.
static int nesteddepth; // Current nested protection depth.
static int expectederror;
void freearray(Heap*, int);

/* Expose the modeled current program only at the scheduler boundary. */
Prog*
currun(void)
{
	return &current;
}

/* Direct rendezvous tests never enqueue the current process. */
Prog*
delrun(int state)
{
	(void)state;
	abort();
}

/* A completed rendezvous wakes exactly the queued peer. */
void
addrun(Prog *program)
{
	assert(program == &peer);
	wakes++;
}

/* Alt selection is covered separately; no alt peer is installed in this fixture. */
void
altdone(Alt *selection, Prog *program, Channel *channel, int sending)
{
	(void)selection; (void)program; (void)channel; (void)sending;
	abort();
}

/* Production metadata diagnostics are expected only for explicitly invalid module indices. */
void
error(char *message)
{
	nestedmessage = message;
	if(nesteddepth)
		longjmp(nested[nesteddepth-1], 1);
	if(!expectederror)
		abort();
	longjmp(boundary, 1);
}

/* Keep nested buffered callback cleanup active before propagating to the outer error oracle. */
int
kprotect(int (*fn)(void*), void *arg)
{
	int slot, result;

	slot = nesteddepth++;
	if(slot >= nelem(nested))
		abort();
	if(setjmp(nested[slot])){
		nesteddepth = slot;
		return 0;
	}
	result = fn(arg);
	nesteddepth = slot;
	return result;
}


/* Preserve the fixture's synchronous error expectation after cleanup. */
void
krethrow(void)
{
	error(nestedmessage);
}


/* Valid direct rendezvous must not emit buffer-state diagnostics. */
int
print(char *format, ...)
{
	(void)format;
	abort();
}

/* Count and release every allocation explicitly when LSan inspection is unavailable. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

/* Supply exact payload extents with poisoned bytes and a trailing allocation guard. */
void*
poolalloc(Pool *pool, ulong size)
{
	Heap *h;
	int i;

	(void)pool;
	assert(size <= 16384);
	h = malloc(size+32);
	assert(h);
	memset(h, 0xa5, size+32);
	for(i = 0; i < 256 && blocks[i].heap; i++);
	assert(i < 256);
	blocks[i].heap = h;
	blocks[i].size = size;
	live++;
	return h;
}

/* Release real array maps and child references while enforcing exact payload guards. */
void
destroy(void *value)
{
	Heap *h;
	int i, j;

	if(value == H)
		return;
	h = D2H(value);
	assert(h->ref > 0);
	if(--h->ref)
		return;
	for(i = 0; i < 256 && blocks[i].heap != h; i++);
	assert(i < 256);
	for(j = 0; j < 32; j++)
		assert(((uchar*)h)[blocks[i].size+j] == 0xa5);
	if(h->t == &Tarray){
		freearray(h, 0);
		Tarray.ref--;
	}else if(h->t == &channelowner){
		Channel *c = value;
		destroy(c->buf);
		c->mid.t->ref--;
		channelowner.ref--;
	}else{
		assert(h->t == &leaf);
		leaf.ref--;
	}
	blocks[i].heap = nil;
	free(h);
	live--;
}

#define BIT(bt, nb) ((bt)&(1<<(nb)))
#define OP(fn) void fn(void)
#define W(r) (*(WORD*)R.r)
#define C(r) (*(Channel**)R.r)
char exBounds[] = "bounds"; // Invalid complete operand span.
IMPLEMENTATION

/* Keep one external owner so the independent reference oracle can inspect every child. */
static void*
child(void)
{
	Heap *h;

	h = nheap(sizeof(WORD));
	h->t = &leaf;
	leaf.ref++;
	return H2D(void*, h);
}

/* Compare full copies, both overlap directions and shared mapped fields with a snapshot oracle. */
static void
overlap(int mover, int width, int from, int to, int aliases)
{
	Type type = {1};
	Type *descriptor = &type, *table[] = {&type};
	Module module = {0};
	Modlink link = {0};
	void *objects[16], *expected[16], *snapshot[8];
	struct { uvlong before[4]; void *slots[16]; uvlong after[4]; } data;
	WORD index = 0;
	uchar metadata[32];
	int i, j, count;

	type.size = width*sizeof(void*); type.np = 1;
	type.map[0] = 0xff << (8-width);
	module.type = table; module.ntype = 1;
	link.m = &module; link.type = table;
	memset(&data, 0xa5, sizeof(data));
	for(i = 0; i < 16; i++)
		objects[i] = child();
	for(i = 0; i < 16; i++){
		data.slots[i] = expected[i] = objects[aliases ? i%3 : i];
		D2H(data.slots[i])->ref++;
	}
	for(i = 0; i < width; i++)snapshot[i] = expected[from+i];
	for(i = 0; i < width; i++)expected[to+i] = snapshot[i];
	R.M = &link; R.s = &data.slots[from]; R.d = &data.slots[to];
	R.m = metadata+((from+to+width+aliases)&7);
	if(mover)memmove(R.m,&index,sizeof(index));else memmove(R.m,&descriptor,sizeof(descriptor));
	if(sizeof(WORD) < sizeof(void*))
		assert((uvlong)(ulong)descriptor > 0xffffffffULL);
	if(mover)movmp(); else movtmp();
	for(i = 0; i < 16; i++)assert(data.slots[i] == expected[i]);
	for(i = 0; i < 16; i++){
		count = 1;
		for(j = 0; j < 16; j++)if(expected[j] == objects[i])count++;
		assert(D2H(objects[i])->ref == count);
	}
	for(i = 0; i < 4; i++)assert(data.before[i] == 0xa5a5a5a5a5a5a5a5ULL && data.after[i] == data.before[i]);
	for(i = 0; i < 16; i++){ destroy(data.slots[i]); destroy(objects[i]); }
	assert(!live && leaf.ref == 1 && type.ref == 1);
	checks++;
}

/* Non-pointer words and padding must survive mapped copies, including exact self assignment. */
static void
mixed(void)
{
	struct Record { void *left; WORD number; void *right; } source, destination, saved;
	uvlong values[] = {0, 1, 0x7fffffffULL, 0x80000000ULL, 0xffffffffULL,
		0x100000000ULL, 0x7fffffffffffffffULL, 0x8000000000000000ULL, ~0ULL};
	Type type = {1}, *descriptor = &type, *table[] = {&type};
	Module module = {0};
	Modlink link = {0};
	void *objects[4];
	WORD index = 0;
	int mover, same, alias, mask, value, i, refs;

	type.size = sizeof(source); type.np = 1;
	type.map[0] = (0x80 >> (offsetof(struct Record, left)/sizeof(void*))) |
		(0x80 >> (offsetof(struct Record, right)/sizeof(void*)));
	module.type = table; module.ntype = 1;
	link.m = &module; link.type = table;
	for(mover = 0; mover < 2; mover++)for(same = 0; same < 2; same++)
	for(alias = 0; alias < 2; alias++)for(mask = 0; mask < 4; mask++)for(value = 0; value < 9; value++){
		for(i = 0; i < 4; i++)objects[i] = child();
		memset(&source, 0xa5, sizeof(source));
		memset(&destination, 0x5a, sizeof(destination));
		source.left = mask&1 ? objects[0] : H;
		source.right = mask&2 ? objects[alias ? 0 : 1] : H;
		source.number = (WORD)values[value];
		if(source.left != H)D2H(source.left)->ref++;
		if(source.right != H)D2H(source.right)->ref++;
		if(!same){
			destination.left = objects[2]; destination.right = objects[3];
			D2H(objects[2])->ref++; D2H(objects[3])->ref++;
		}
		memmove(&saved, &source, sizeof(saved));
		R.M = &link; R.s = &source; R.d = same ? (void*)&source : (void*)&destination;
		R.m = mover ? (void*)&index : (void*)&descriptor;
		if(mover)movmp(); else movtmp();
		assert(memcmp(R.d, &saved, sizeof(saved)) == 0);
		assert(memcmp(&source, &saved, sizeof(saved)) == 0);
		for(i = 0; i < 4; i++){
			refs = 1+(source.left == objects[i])+(source.right == objects[i]);
			if(!same)refs += (destination.left == objects[i])+(destination.right == objects[i]);
			assert(D2H(objects[i])->ref == refs);
		}
		destroy(source.left); destroy(source.right);
		if(!same){ destroy(destination.left); destroy(destination.right); }
		for(i = 0; i < 4; i++)destroy(objects[i]);
		assert(!live && leaf.ref == 1);
		checks++;
	}
}

/* Fill and drain real typed buffers through every initial ring position. */
static void
buffered(void)
{
	Type type = {1};
	Channel channel;
	void *input[3], *output[3], *expected[8][3], *objects[8];
	int capacity, front, round, item, i, j;

	type.size = sizeof(input); type.np = 1; type.map[0] = 0xe0;
	for(capacity = 1; capacity <= 8; capacity++)for(front = 0; front < capacity; front++){
		memset(&channel, 0, sizeof(channel));
		channel.buf = H2D(Array*, heaparray(&type, capacity));
		channel.mid.t = &type; channel.mover = movtmp; channel.front = front;
		for(i = 0; i < 8; i++)objects[i] = child();
		for(round = 0; round < 16; round++){
			for(item = 0; item < capacity; item++){
				for(i = 0; i < 3; i++)input[i] = expected[item][i] = objects[(round+item+i)%8];
				assert(cputb(&channel, input));
				checks++;
			}
			assert(!cputb(&channel, input));
			for(item = 0; item < capacity; item++){
				for(i = 0; i < 3; i++){ output[i] = objects[(item+i)%8]; D2H(output[i])->ref++; }
				assert(cgetb(&channel, output));
				for(i = 0; i < 3; i++)assert(output[i] == expected[item][i]);
				for(i = 0; i < 3; i++)destroy(output[i]);
				checks++;
			}
			assert(!cgetb(&channel, output));
			for(i = 0; i < 8; i++)assert(D2H(objects[i])->ref == 1);
			for(j = 0; j < capacity*3; j++)assert(((void**)channel.buf->data)[j] == H);
			assert(channel.size == 0 && channel.front == front);
		}
		destroy(channel.buf);
		for(i = 0; i < 8; i++)destroy(objects[i]);
		assert(!live && leaf.ref == 1 && type.ref == 1 && Tarray.ref == 1);
	}
}

/* Both direct sender and receiver paths must pass the native channel descriptor unchanged. */
static void
rendezvous(void)
{
	Type type = {1};
	Channel channel, *slot;
	Progq send, receive;
	void *input[3], *output[3], *objects[4];
	int sending, shared, i;

	type.size = sizeof(input); type.np = 1; type.map[0] = 0xe0;
	for(sending = 0; sending < 2; sending++)for(shared = 0; shared < 2; shared++){
		memset(&channel, 0, sizeof(channel)); memset(&peer, 0, sizeof(peer));
		memset(&send, 0, sizeof(send)); memset(&receive, 0, sizeof(receive));
		channel.buf = H; channel.mid.t = &type; channel.mover = movtmp;
		channel.send = &send; channel.recv = &receive; slot = &channel;
		for(i = 0; i < 4; i++)objects[i] = child();
		for(i = 0; i < 3; i++){
			input[i] = objects[shared ? 0 : i]; D2H(input[i])->ref++;
			output[i] = objects[3]; D2H(output[i])->ref++;
		}
		peer.state = sending ? Precv : Psend;
		peer.ptr = sending ? (void*)output : (void*)input;
		if(sending){ receive.prog = &peer; R.s = input; R.d = &slot; }
		else{ send.prog = &peer; R.s = &slot; R.d = output; }
		wakes = 0;
		if(sending)isend(); else irecv();
		assert(wakes == 1 && peer.ptr == nil);
		for(i = 0; i < 3; i++){ assert(output[i] == input[i]); destroy(input[i]); destroy(output[i]); }
		for(i = 0; i < 4; i++){ assert(D2H(objects[i])->ref == 1); destroy(objects[i]); }
		assert(!live && leaf.ref == 1);
		checks++;
	}
}

/* Receiving can replace the channel's sole owner before buffer cleanup or refill. */
static void
borrowedchannels(void)
{
	Type type = {1};
	Channel *channel;
	Progq send, receive;
	void *output[8], *message[8], *object;
	int width, slot, capacity, front, shared, blocked, i, j, refs;

	type.np = 1;
	for(width = 1; width <= 8; width++)
	for(slot = 0; slot < width; slot++)
	for(capacity = 1; capacity <= 4; capacity++)
	for(front = 0; front < capacity; front++)
	for(shared = 0; shared < 2; shared++)
	for(blocked = 0; blocked < 2; blocked++){
		type.size = width*sizeof(void*);
		type.map[0] = 0xff << (8-width);
		channel = H2D(Channel*, nheap(sizeof(Channel)));
		memset(channel, 0, sizeof(*channel));
		D2H(channel)->t = &channelowner;
		channelowner.ref++;
		channel->mid.t = &type;
		type.ref++;
		channel->mover = movtmp;
		channel->buf = H2D(Array*, heaparray(&type, capacity));
		channel->front = front;
		channel->size = capacity;
		memset(&send, 0, sizeof(send));
		memset(&receive, 0, sizeof(receive));
		channel->send = &send;
		channel->recv = &receive;
		object = child();
		for(i = 0; i < capacity; i++)for(j = 0; j < width; j++){
			((void**)channel->buf->data)[i*width+j] = object;
			D2H(object)->ref++;
		}
		for(i = 0; i < width; i++){
			output[i] = H;
			message[i] = object;
			D2H(object)->ref++;
		}
		output[slot] = channel;
		if(shared)
			D2H(channel)->ref++;
		wakes = 0;
		if(blocked){
			memset(&peer, 0, sizeof(peer));
			peer.state = Psend;
			peer.ptr = message;
			send.prog = &peer;
		}
		R.s = &output[slot];
		R.d = output;
		irecv();
		assert(wakes == blocked && (!blocked || peer.ptr == nil));
		for(i = 0; i < width; i++)
			assert(output[i] == object);
		refs = 1+2*width;
		if(shared){
			assert(D2H(channel)->ref == 1);
			assert(channel->size == capacity-1+blocked);
			refs += channel->size*width;
		}
		assert(D2H(object)->ref == refs);
		if(shared)
			destroy(channel);
		for(i = 0; i < width; i++){
			destroy(output[i]);
			destroy(message[i]);
		}
		assert(D2H(object)->ref == 1);
		destroy(object);
		assert(!live && type.ref == 1 && channelowner.ref == 1 && leaf.ref == 1);
		checks++;
	}
}

/* Empty mapped records still resolve module metadata but never access data addresses. */
static void
empty(void)
{
	Type type = {1}, *descriptor = &type, *table[] = {&type};
	Module module = {0};
	Modlink link = {0};
	WORD index = 0;
	int mover, source, destination, capacity, front, round, item;

	module.type = table; module.ntype = 1;
	link.m = &module; link.type = table;
	for(mover = 0; mover < 2; mover++)for(source = 0; source < 2; source++)for(destination = 0; destination < 2; destination++){
		R.M = &link; R.s = source ? H : nil; R.d = destination ? H : nil;
		R.m = mover ? (void*)&index : (void*)&descriptor;
		if(mover)movmp(); else movtmp();
		checks++;
	}
	for(capacity = 1; capacity <= 4; capacity++)for(front = 0; front < capacity; front++){
		Channel channel;
		memset(&channel, 0, sizeof(channel));
		channel.buf = H2D(Array*, heaparray(&type, capacity));
		channel.mid.t = &type; channel.mover = movtmp; channel.front = front;
		for(round = 0; round < 8; round++){
			for(item = 0; item < capacity; item++){ assert(cputb(&channel, H)); checks++; }
			for(item = 0; item < capacity; item++){ assert(cgetb(&channel, nil)); checks++; }
			assert(channel.size == 0 && channel.front == front);
		}
		destroy(channel.buf);
		assert(!live && type.ref == 1 && Tarray.ref == 1);
	}
	R.M = &link; R.m = &index; index = 1;
	expectederror = 1;
	if(setjmp(boundary) == 0){ movmp(); assert(0); }
	expectederror = 0;
	checks++;
}

/* Reject bad native descriptors before touching inaccessible payload storage. */
static void
metadataerrors(void)
{
	Type type={0},*descriptor;
	uchar slot[32];
	REG before;
	int align,kind;

	for(align=0;align<8;align++)for(kind=0;kind<4;kind++){
		type.size=kind==2 ? -1 : 0;descriptor=kind==0 ? nil : kind==1 ? H : &type;
		R.m=slot+align;R.s=(void*)1;R.d=(void*)1;memmove(R.m,&descriptor,sizeof(descriptor));before=R;
		if(kind==3)movtmp();
		else{
			expectederror=1;if(setjmp(boundary)==0){movtmp();assert(0);}expectederror=0;
		}
		assert(!memcmp(&R,&before,sizeof(R)) && !live);checks++;
	}
}

/* Exercise mixed-width pointers, overlapping records, typed FIFO and direct rendezvous. */
int
main(int argc, char **argv)
{
	int mover, width, from, to, aliases;

	if(argc==1 || !strcmp(argv[1],"metadata"))metadataerrors();
	if(argc == 1 || strcmp(argv[1], "overlap") == 0)
	for(mover = 0; mover < 2; mover++)for(width = 1; width <= 8; width++)
	for(from = 0; from+width <= 16; from++)for(to = 0; to+width <= 16; to++)for(aliases = 0; aliases < 2; aliases++)
		overlap(mover, width, from, to, aliases);
	if(argc == 1 || strcmp(argv[1], "mixed") == 0)mixed();
	if(argc == 1 || strcmp(argv[1], "buffer") == 0)buffered();
	if(argc == 1 || strcmp(argv[1], "borrowed") == 0)borrowedchannels();
	if(argc == 1 || strcmp(argv[1], "direct") == 0)rendezvous();
	if(argc == 1 || strcmp(argv[1], "empty") == 0)empty();
	assert(!live && checks > 0);
	printf("Mapped moves: %d pointer/overlap/FIFO/rendezvous/empty cases\n", checks);
	return 0;
}
