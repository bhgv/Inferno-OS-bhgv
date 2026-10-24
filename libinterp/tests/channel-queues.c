#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>

static Proc worker;
static Prog current, others[3];
static char *last_error;
static int fail_after = -1, live, removed, awakened, checks;
static Progq **blockedqueue;
static int blockmode, blockcalls;
static char interrupted[] = "interrupted";
char exNomem[] = "allocation", exNilref[] = "nil", exAlt[] = "self alt", exBounds[] = "bounds";
REG R;
Type Tarray;

/* Use the emulator's real error-stack layout for every injected failure. */
Proc *getup(void) { return &worker; }
void nexterror(void) { assert(worker.nerr > 0); siglongjmp(worker.estack[--worker.nerr], 1); }
void error(char *s) { last_error = s; nexterror(); }
int __lsan_is_turned_off(void) { return 1; }
GUARD

/* Account for all queue nodes and fail a chosen allocation deterministically. */
static void*
nodealloc(ulong size)
{
	void *p;

	if(fail_after == 0)
		return nil;
	if(fail_after > 0)
		fail_after--;
	p = malloc(size);
	assert(p);
	live++;
	return p;
}

/* Every production queue free must balance a fixture allocation. */
static void
nodefree(void *p)
{
	assert(p && live > 0);
	live--;
	free(p);
}

/* Model only runnable-list transitions, leaving queue logic in production code. */
Prog *currun(void) { return &current; }
Prog *delrun(int state) { removed++; current.state = state; return &current; }
void addrun(Prog *p) { awakened++; p->state = Pready; }
int print(char *fmt, ...) { (void)fmt; assert(0); return 0; }
static int cputb(Channel *c, void *v) { (void)c; (void)v; assert(0); return 0; }
static int cgetb(Channel *c, void *v) { (void)c; (void)v; assert(0); return 0; }
#define OP(fn) void fn(void)
#define C(r) (*(Channel**)R.r)
#define malloc nodealloc
#define free nodefree
IMPLEMENTATION
#undef malloc
#undef free

/* Model wakeup, interruption and killcomm cleanup at the native blocking boundary. */
void
cblock(Prog *p)
{
	blockcalls++;
	if(blockmode == 1)
		error(interrupted);
	cqdelp(blockedqueue, p);
	p->ptr = nil;
	if(blockmode == 2)
		error(interrupted);
}

/* Construct the same permanent empty-head invariant as channel creation. */
static Progq*
emptyqueue(void)
{
	Progq *q;

	q = nodealloc(sizeof(*q));
	q->prog = nil;
	q->next = nil;
	return q;
}

/* Free a fixture queue after production operations have finished. */
static void
releasequeue(Progq *q)
{
	Progq *next;

	assert(q);
	while(q){
		next = q->next;
		nodefree(q);
		q = next;
	}
}

/* Check exact FIFO survivors, with no empty nodes inside a nonempty queue. */
static void
queueequals(Progq *q, Prog **expected, int n)
{
	int i;

	assert(q);
	if(n == 0){
		assert(q->prog == nil && q->next == nil);
		return;
	}
	for(i = 0; i < n; i++){
		assert(q && q->prog == expected[i]);
		q = q->next;
	}
	assert(q == nil);
}

/* Exhaust every ternary queue through length eight and every removal target. */
static void
checkqueues(void)
{
	Progq *q;
	Prog *expected[10];
	int length, patterns, pattern, target, i, code, n;

	patterns = 1;
	for(length = 0; length <= 8; length++){
		for(pattern = 0; pattern < patterns; pattern++)
		for(target = 0; target < 3; target++){
			q = emptyqueue(); code = pattern; n = 0;
			for(i = 0; i < length; i++){
				cqadd(&q, &others[code%3]);
				if(code%3 != target)
					expected[n++] = &others[code%3];
				code /= 3;
			}
			cqdelp(&q, &others[target]);
			queueequals(q, expected, n);
			cqdelp(&q, &others[target]);
			queueequals(q, expected, n);
			cqadd(&q, &current); expected[n++] = &current;
			queueequals(q, expected, n);
			for(i = 0; i < n; i++){
				assert(q->prog == expected[i]); cqdel(&q);
			}
			queueequals(q, expected, 0);
			releasequeue(q); assert(live == 0); checks++;
		}
		patterns *= 3;
	}
}

static Channel *selected;
static int sending, blocking;

/* Invoke actual send/receive instructions inside the real VM error boundary. */
static int
communicate(void *unused)
{
	(void)unused;
	if(sending) isend(); else irecv();
	return 1;
}

/* Exercise allocation failure before and successful transition after registration. */
static void
checkblocking(void)
{
	Channel c;
	WORD data = 42;
	Prog *expected[2];
	int fault;

	for(sending = 0; sending < 2; sending++)
	for(fault = 0; fault < 2; fault++){
		memset(&c, 0, sizeof(c));
		c.buf = H; c.send = emptyqueue(); c.recv = emptyqueue();
		cqadd(sending ? &c.send : &c.recv, &others[0]);
		memset(&current, 0, sizeof(current)); current.state = Pready;
		current.ptr = &data; current.chan = (Channel*)1;
		R.s = sending ? (void*)&data : (void*)&selected;
		R.d = sending ? (void*)&selected : (void*)&data;
		R.IC = 17; R.t = 23; selected = &c; removed = 0;
		fail_after = fault ? -1 : 0;
		if(!fault){
			assert(!kprotect(communicate, nil) && last_error == exNomem);
			assert(!removed && current.state == Pready);
			assert(current.ptr == &data && current.chan == (Channel*)1);
			assert(R.IC == 17 && R.t == 23);
			expected[0] = &others[0];
			queueequals(sending ? c.send : c.recv, expected, 1);
		}else{
			assert(kprotect(communicate, nil));
			assert(removed == 1 && current.state == (sending ? Psend : Precv));
			assert(current.ptr == &data && current.chan == &c && R.IC == 1 && R.t == 1);
			expected[0] = &others[0]; expected[1] = &current;
			queueequals(sending ? c.send : c.recv, expected, 2);
		}
		fail_after = -1;
		releasequeue(c.send); releasequeue(c.recv); assert(live == 0); checks++;
	}
}

/* Invalid channel owners must fail before touching queues, data or decoded registers. */
static void checknilslots(void)
{
	union {uvlong align;uchar bytes[24];} slot;
	uchar expected[24];
	Channel *owner;
	WORD data=42;
	REG before;
	int direction,kind,align;

	for(direction=0;direction<2;direction++)for(kind=0;kind<2;kind++)for(align=0;align<8;align++){
		sending=direction;owner=kind ? H : nil;
		memset(slot.bytes,0xa5,sizeof(slot.bytes));memmove(slot.bytes+align,&owner,sizeof(owner));
		memmove(expected,slot.bytes,sizeof(expected));
		R.s=sending ? (void*)&data : slot.bytes+align;R.d=sending ? slot.bytes+align : (void*)&data;
		R.IC=17;R.t=23;before=R;
		assert(!kprotect(communicate,nil) && last_error==exNilref);
		assert(!memcmp(&R,&before,sizeof(R)) && !memcmp(expected,slot.bytes,sizeof(expected)) && data==42 && live==0);
		checks++;
	}
}

/* Run complete alt preparation rather than replacing its registration logic. */
static int
prepare(void *unused)
{
	(void)unused;
	xecalt(blocking);
	return 1;
}

/* Fail each registration of repeated scalar/array guards and preserve other waiters. */
static void
checkalts(void)
{
	struct { int nsend, nrecv; Altc ac[8]; } storage;
	struct { Heap h; Array a; } array;
	struct { Heap h; Channel c; } boxes[3];
	Channel *channels[3], *members[4];
	Prog *expected[1];
	WORD result, destinations[8][2];
	Alt *a;
	int fault, i, mode, count, ok, foreign;

	a = (Alt*)&storage;
	for(foreign = 0; foreign < 2; foreign++)
	for(blocking = 0; blocking < 2; blocking++)
	for(mode = 0; mode < 5; mode++)
	for(fault = 0; fault <= 18; fault++){
		memset(&storage, 0, sizeof(storage));
		memset(&array, 0, sizeof(array));
		array.h.t = &Tarray; array.a.len = 4; array.a.data = (uchar*)members;
		for(i = 0; i < 3; i++){
			memset(&boxes[i], 0, sizeof(boxes[i]));
			channels[i] = &boxes[i].c;
			channels[i]->buf = H;
			channels[i]->send = emptyqueue(); channels[i]->recv = emptyqueue();
			if(foreign) cqadd(mode >= 3 ? &channels[i]->send : &channels[i]->recv, &others[0]);
		}
		for(i = 0; i < 4; i++) members[i] = i == 3 ? H : channels[i];
		a->nsend = mode >= 3 ? 8 : 0; a->nrecv = mode >= 3 ? 0 : 8;
		for(i = 0; i < 8; i++){
			a->ac[i].c = mode == 1 && i%2 ? (Channel*)&array.a : channels[i%3];
			a->ac[i].ptr = destinations[i];
		}
		if(mode == 2 || mode == 4) a->ac[7].c = H;
		memset(&current, 0, sizeof(current)); current.state = Pready;
		R.s = a; R.d = &result; result = -77; removed = 0;
		count = live; fail_after = fault;
		ok = kprotect(prepare, nil);
		fail_after = -1;
		if(!ok){
			assert(last_error == exNomem || ((mode == 2 || mode == 4) && last_error == exNilref));
			assert(result == -77 && !removed && current.state == Pready);
		}else if(blocking){
			assert(removed == 1 && current.state == Palt && result == -77);
			altdone(a, &current, nil, -1);
		}else assert(!removed && result == 8);
		assert(live == count);
		expected[0] = &others[0];
		for(i = 0; i < 3; i++){
			queueequals(channels[i]->recv, expected, mode >= 3 ? 0 : foreign);
			queueequals(channels[i]->send, expected, mode >= 3 ? foreign : 0);
			releasequeue(channels[i]->send); releasequeue(channels[i]->recv);
		}
		assert(live == 0 && worker.nerr == 0); checks++;
	}
}

/* A rejected conflict must never reach communication or wake a foreign peer. */
static void
unexpectedtransfer(void)
{
	assert(!"conflicting alt reached data transfer");
}

/* Detect scalar/array receive registrations at every depth behind other waiters. */
static void
checkselfalt(void)
{
	struct { Heap h; Channel c; } box;
	struct { Heap h; Array a; } array;
	struct { int nsend, nrecv; Altc ac[2]; } storage;
	Channel *members[5];
	Prog *expected[3];
	WORD result, data[2];
	Alt *a;
	int foreign, wide, duplicate, i;

	for(foreign = 0; foreign < 4; foreign++)
	for(wide = 0; wide < 2; wide++)
	for(duplicate = 1; duplicate <= 4; duplicate++){
		memset(&box, 0, sizeof(box));
		memset(&array, 0, sizeof(array));
		array.h.t = &Tarray; array.a.data = (uchar*)members;
		array.a.len = duplicate+1;
		for(i = 0; i < duplicate; i++) members[i] = &box.c;
		members[duplicate] = H;
		box.c.send = emptyqueue(); box.c.recv = emptyqueue(); box.c.buf = H;
		box.c.mover = unexpectedtransfer;
		for(i = 0; i < foreign; i++){
			others[i].state = Precv; others[i].ptr = data;
			expected[i] = &others[i]; cqadd(&box.c.recv, expected[i]);
		}
		a = (Alt*)&storage; a->nsend = a->nrecv = 1;
		a->ac[0].c = &box.c;
		a->ac[1].c = wide ? (Channel*)&array.a : &box.c;
		a->ac[0].ptr = a->ac[1].ptr = data;
		result = -77; R.s = a; R.d = &result;
		current.state = Pready; removed = 0;
		assert(!kprotect(prepare, nil) && last_error == exAlt);
		assert(result == -77 && !removed && current.state == Pready);
		queueequals(box.c.send, nil, 0); queueequals(box.c.recv, expected, foreign);
		releasequeue(box.c.send); releasequeue(box.c.recv);
		assert(live == 0 && worker.nerr == 0); checks++;
	}
}

/* Native channel helpers clean stale payload pointers even if queue allocation fails. */
static void
checknative(void)
{
	Progq *q;
	Prog *expected[1];
	WORD payload = 99;
	int fault;

	for(blockmode = 0; blockmode < 3; blockmode++)
	for(fault = 0; fault < 2; fault++){
		q = emptyqueue(); cqadd(&q, &others[0]);
		blockedqueue = &q; current.ptr = &payload; blockcalls = 0;
		fail_after = fault ? -1 : 0;
		if(waserror()){
			assert(last_error == (fault ? interrupted : exNomem));
		}else{
			ccom(&q, &current);
			assert(fault && blockmode == 0);
			poperror();
		}
		fail_after = -1;
		assert(current.ptr == nil && blockcalls == fault);
		expected[0] = &others[0]; queueequals(q, expected, 1);
		releasequeue(q); assert(live == 0 && worker.nerr == 0); checks++;
	}
}

/* Exercise actual registration and wakeup selection through independently aligned slots. */
static void
checkaltslots(void)
{
	union {uvlong align;uchar bytes[256];} storage;
	uchar outputs[48],payload[48],members[64],before[48];
	struct { Heap h; Channel c; } boxes[2];
	struct { Heap h; Array a; } array;
	Channel *entry;
	Altc guard;
	Alt *a;
	Prog *expected[1];
	WORD result,index;
	int ha,oa,pa,aa,kind,foreign,i,counts[2];

	for(ha=0;ha<8;ha++)for(oa=0;oa<8;oa++)for(pa=0;pa<8;pa++)for(aa=0;aa<8;aa++)
	for(kind=0;kind<3;kind++)for(foreign=0;foreign<2;foreign++){
		for(i=0;i<2;i++){
			memset(&boxes[i],0,sizeof(boxes[i]));boxes[i].c.buf=H;
			boxes[i].c.send=emptyqueue();boxes[i].c.recv=emptyqueue();
			if(foreign){cqadd(&boxes[i].c.send,&others[0]);cqadd(&boxes[i].c.recv,&others[0]);}
		}
		memset(&current,0,sizeof(current));memset(&array,0,sizeof(array));array.h.t=&Tarray;
		array.a.data=members+aa;array.a.len=3;
		entry=H;memmove(members+aa,&entry,sizeof(entry));entry=&boxes[0].c;memmove(members+aa+sizeof(entry),&entry,sizeof(entry));
		entry=&boxes[1].c;memmove(members+aa+2*sizeof(entry),&entry,sizeof(entry));
		memset(storage.bytes,0xa5,sizeof(storage.bytes));a=(Alt*)(storage.bytes+ha);
		counts[0]=kind==0 ? 2 : 0;counts[1]=kind==0 ? 0 : 2;memmove(a,counts,sizeof(counts));
		guard.c=&boxes[0].c;guard.ptr=payload+pa;
		memmove((uchar*)a+offsetof(Alt,ac),&guard,sizeof(guard));
		guard.c=kind==2 ? (Channel*)&array.a : &boxes[1].c;
		memmove((uchar*)a+offsetof(Alt,ac)+sizeof(guard),&guard,sizeof(guard));
		memset(outputs,0xa5,sizeof(outputs));memset(payload,0xa5,sizeof(payload));memmove(before,payload,sizeof(payload));
		current.R.d=outputs+oa;
		altrdy(a,&current);altdone(a,&current,&boxes[1].c,kind==0 ? 0 : 1);
		memmove(&result,outputs+oa,sizeof(result));assert(result==1);
		if(kind==2){memmove(&index,payload+pa,sizeof(index));assert(index==2);memmove(before+pa,&index,sizeof(index));}
		assert(current.ptr==payload+pa+(kind==2 ? sizeof(WORD) : 0));
		assert(!memcmp(payload,before,sizeof(payload)));
		for(i=0;i<48;i++)if(i<oa || i>=oa+(int)sizeof(WORD))assert(outputs[i]==0xa5);
		expected[0]=&others[0];
		for(i=0;i<2;i++){
			queueequals(boxes[i].c.send,expected,foreign);queueequals(boxes[i].c.recv,expected,foreign);
			releasequeue(boxes[i].c.send);releasequeue(boxes[i].c.recv);
		}
		assert(!live);checks++;
	}
}

/* Partial cleanup must tolerate null channels, malformed arrays and rejected headers. */
static void
checkaltgeometry(void)
{
	struct { int nsend,nrecv; Altc ac[2]; } storage;
	struct { Heap h; Channel c; } box;
	struct { Heap h; Array a; } array;
	Channel *members[2];
	WORD result;
	int mode;

	for(mode=0;mode<12;mode++){
		memset(&storage,0,sizeof(storage));memset(&box,0,sizeof(box));memset(&array,0,sizeof(array));
		box.c.buf=H;box.c.send=emptyqueue();box.c.recv=emptyqueue();array.h.t=&Tarray;
		members[0]=&box.c;members[1]=nil;array.a.data=(uchar*)members;array.a.len=2;
		storage.nrecv=2;storage.ac[0].c=&box.c;storage.ac[1].c=(Channel*)&array.a;
		if(mode==1)storage.ac[1].c=nil;
		if(mode==2){storage.nsend=2;storage.nrecv=0;storage.ac[1].c=nil;}
		if(mode==3)array.a.len=-1;
		if(mode==4)array.a.len=(WORD)0x7fffffff;
		if(mode==5)array.a.data=nil;
		if(mode==6)array.a.data=H;
		if(mode==7)array.a.data=(uchar*)((uintptr)-1-3);
		if(mode==8)storage.nrecv=-1;
		if(mode==9)storage.nsend=0x7fffffff;
		R.s=mode==10 ? nil : mode==11 ? H : (void*)&storage;R.d=&result;result=-77;removed=0;blocking=0;
		assert(!kprotect(prepare,nil));assert(!removed && result==-77);
		assert(last_error==((mode<3 || mode>=10) ? exNilref : exBounds));
		queueequals(box.c.send,nil,0);queueequals(box.c.recv,nil,0);
		releasequeue(box.c.send);releasequeue(box.c.recv);assert(!live && !worker.nerr);checks++;
	}
}

/* Require balanced allocations and error frames after every scenario. */
int
main(void)
{
	if(!getenv("QUEUE_CASE") || !strcmp(getenv("QUEUE_CASE"),"altslots"))checkaltslots();
	if(!getenv("QUEUE_CASE") || !strcmp(getenv("QUEUE_CASE"),"altgeometry"))checkaltgeometry();
	if(!getenv("QUEUE_CASE") || !strcmp(getenv("QUEUE_CASE"), "queues")) checkqueues();
	if(!getenv("QUEUE_CASE") || !strcmp(getenv("QUEUE_CASE"), "blocking")) { checkblocking(); checknilslots(); }
	if(!getenv("QUEUE_CASE") || !strcmp(getenv("QUEUE_CASE"), "alt")){ checkalts(); checkselfalt(); }
	if(!getenv("QUEUE_CASE") || !strcmp(getenv("QUEUE_CASE"), "native")) checknative();
	assert(live == 0 && worker.nerr == 0 && awakened == 0);
	printf("PASS channel queues: %d FIFO/duplicate/removal/allocation/alt scenarios, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
