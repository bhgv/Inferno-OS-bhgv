#include <sys/wait.h>

typedef struct AltImages AltImages;

/* Only these bytes may change when results and payloads overlap ALT metadata. */
struct AltImages
{
	uvlong alignment; // Keep the base stable while each table/slot is shifted independently.
	uchar table[sizeof(Alt)+3*sizeof(Altc)+32]; // Three guards plus packed-output suffix space.
	uchar members[5*sizeof(Channel*)+32]; // Four member pointers plus overlapping payload suffix.
	uchar result[32]; // Separate selection result when metadata is the payload target.
	uchar payload[64]; // Independent scalar value or array receive tuple.
};

typedef struct AltChannelBox AltChannelBox;
typedef struct AltArrayBox AltArrayBox;
typedef struct AltGuardTable AltGuardTable;

/* Keep actual heap prefixes next to their native channel objects. */
struct AltChannelBox
{
	Heap h; // Native heap header used by channel/owner preflight.
	Channel c; // Concrete channel with owned FIFO queue nodes.
};

/* The runtime distinguishes a channel array through its actual heap Type identity. */
struct AltArrayBox
{
	Heap h; // Header carrying the exact Tarray identity.
	Array a; // Native descriptor whose backing pointer cells may be packed.
};

/* Three receive guards exercise cleanup past the first selected/invalid cell. */
struct AltGuardTable
{
	int nsend, nrecv; // Native ALT guard counts, not VM WORD fields.
	Altc ac[3]; // Concrete or array guard and original payload address.
};

static int publicationmoves, publicationfault; // Observed actual mover calls and post-write error injection.


/* Keep the production rendezvous path; only the scalar mover's work/error is controlled. */
static void
publicationmove(void)
{
	WORD value;

	memmove(&value, R.s, sizeof(value));
	memmove(R.d, &value, sizeof(value));
	publicationmoves++;
	if(publicationfault)
		error(interrupted);
}


/* Add one oracle store at the same logical cell inside the independent expected image. */
static void
publicationstore(AltImages *expected, AltImages *actual, void *slot, WORD value)
{
	uintptr offset;

	offset = (uchar*)slot-(uchar*)actual;
	channelcheck(offset <= sizeof(*actual)-sizeof(value));
	memmove((uchar*)expected+offset, &value, sizeof(value));
}


/* Exhaust packed scalar/array guards with output aliases and preserved unrelated FIFO waiters. */
static void
checkpublication(int phase, int payloadalias, int tuplealias)
{
	AltImages images, expected;
	AltChannelBox boxes[2];
	AltArrayBox array;
	Alt *a;
	Altc guard;
	Channel *member;
	Prog *foreign[1];
	void *target, *resultslot, *tuple, *data;
	WORD value, peerdata;
	int counts[2], ha, shift, location, kind, background, fault, block, i, ok;
	uintptr offsets[4];

	/* Later guard owners, selected operand pointers, counts and member backing all overlap safely. */
	offsets[0] = offsetof(Alt, ac)+sizeof(Altc)+offsetof(Altc, c);
	offsets[1] = offsetof(Alt, ac)+offsetof(Altc, ptr);
	offsets[2] = 0;
	offsets[3] = 2*sizeof(Channel*);
	for(ha = 0; ha < 8; ha++)
	for(shift = 0; shift < 8; shift++)
	for(location = 0; location < 4; location++)
	for(kind = 0; kind < 3; kind++)
	for(background = 0; background < 2; background++)
	for(fault = 0; fault <= (phase == 2); fault++)
	for(block = 0; block <= (phase == 2); block++){
		if(tuplealias && kind != 2)
			continue;
		memset(&images, 0xa5, sizeof(images));
		memset(&array, 0, sizeof(array));
		memset(&current, 0, sizeof(current));
		memset(others, 0, sizeof(others));
		current.state = Pready;
		for(i = 0; i < 2; i++){
			memset(&boxes[i], 0, sizeof(boxes[i]));
			boxes[i].h.ref = 1;
			boxes[i].c.buf = H;
			boxes[i].c.mover = publicationmove;
			boxes[i].c.send = emptyqueue();
			boxes[i].c.recv = emptyqueue();
			if(background)
				cqadd(kind == 0 ? &boxes[i].c.send : &boxes[i].c.recv, &others[1]);
		}
		a = (Alt*)(images.table+ha);
		counts[0] = kind == 0 ? 3 : 0;
		counts[1] = kind == 0 ? 0 : 3;
		memmove(a, counts, sizeof(counts));
		array.h.t = &Tarray;
		array.a.len = 4;
		array.a.data = images.members+ha;
		for(i = 0; i < 4; i++){
			member = i == 1 ? &boxes[0].c : i == 3 ? &boxes[1].c : H;
			memmove(array.a.data+i*sizeof(member), &member, sizeof(member));
		}
		target = location == 3 ? images.members+ha+offsets[location]+shift : (uchar*)a+offsets[location]+shift;
		resultslot = payloadalias || tuplealias ? images.result+shift : target;
		tuple = tuplealias ? target : images.payload+shift;
		data = payloadalias ? target : images.payload+shift;
		if(kind == 2){
			if(payloadalias)
				tuple = (uchar*)target-sizeof(WORD);
			data = (uchar*)tuple+sizeof(WORD);
		}
		for(i = 0; i < 3; i++){
			guard.c = i == 0 ? kind == 2 ? (Channel*)&array.a : &boxes[0].c : &boxes[1].c;
			guard.ptr = i == 0 ? kind == 2 ? tuple : data : images.payload+32;
			memmove((uchar*)a+offsetof(Alt, ac)+i*sizeof(guard), &guard, sizeof(guard));
		}
		value = 0x334455;
		peerdata = value;
		if(phase == 2){
			others[0].state = kind == 0 ? Precv : Psend;
			others[0].ptr = &peerdata;
			cqadd(kind == 0 ? &boxes[0].c.recv : &boxes[0].c.send, &others[0]);
		}
		memset(&R, 0, sizeof(R));
		R.s = a;
		R.d = resultslot;
		R.IC = 17;
		R.t = 37;
		current.R = R;
		publicationmoves = removed = awakened = 0;
		publicationfault = fault;
		blocking = phase == 1 || block;
		memmove(&expected, &images, sizeof(expected));
		publicationstore(&expected, &images, resultslot, phase == 0 ? 3 : 0);
		if(phase != 0 && kind == 2)
			publicationstore(&expected, &images, tuple, 1);
		if(phase == 2){
			if(kind == 0)
				memmove(&value, (uchar*)&expected+((uchar*)data-(uchar*)&images), sizeof(value));
			else
				publicationstore(&expected, &images, data, value);
		}
		if(phase == 1){
			channelcheck(kprotect(prepare, nil));
			channelcheck(current.state == Palt && current.R.s == a && current.R.d == resultslot);
			altdone(a, &current, &boxes[0].c, kind != 0);
			channelcheck(current.ptr == data);
			ok = 1;
		}else
			ok = kprotect(prepare, nil);
		channelcheck(ok == !fault && (ok || last_error == interrupted));
		channelcheck(!memcmp(&images, &expected, sizeof(images)));
		/* Tuple/payload aliases may replace the selection cell after its initial publication. */
		channelcheck(removed == (phase == 1) && R.IC == (phase == 1 ? 1 : 17) && publicationmoves == (phase == 2));
		channelcheck(awakened == (phase == 2 && !fault));
		channelcheck(R.t == (phase == 1 ? 1 : phase == 2 && (block || !fault) ? 0 : 37));
		if(phase == 2 && kind == 0)
			channelcheck(peerdata == value);
		foreign[0] = &others[1];
		for(i = 0; i < 2; i++){
			queueequals(boxes[i].c.send, foreign, kind == 0 ? background : 0);
			queueequals(boxes[i].c.recv, foreign, kind == 0 ? 0 : background);
			releasequeue(boxes[i].c.send);
			releasequeue(boxes[i].c.recv);
		}
		channelcheck(!live && !worker.nerr);
		checks++;
	}
}

static Channel *publicationselected; // Concrete wakeup owner for the protected cancellation probe.


/* Run the actual blocked wakeup cleanup under the same VM error boundary as ready ALT. */
static int
publicationfinish(void *table)
{
	altdone(table, &current, publicationselected, 1);
	return 1;
}


/* Invalid tuple/output cells must not strand registrations or publish an incomplete wakeup. */
static void
badpublication(int mode, int index)
{
	AltGuardTable storage;
	AltChannelBox boxes[2];
	AltArrayBox array;
	Channel *member;
	void *bad[3];
	WORD result, tuple[2];
	int i;

	bad[0] = nil;
	bad[1] = H;
	bad[2] = (void*)((uintptr)-1-sizeof(WORD)+1);
	{
		memset(&storage, 0, sizeof(storage));
		memset(&array, 0, sizeof(array));
		memset(&current, 0, sizeof(current));
		memset(others, 0, sizeof(others));
		for(i = 0; i < 2; i++){
			memset(&boxes[i], 0, sizeof(boxes[i]));
			boxes[i].c.buf = H;
			boxes[i].c.send = emptyqueue();
			boxes[i].c.recv = emptyqueue();
			boxes[i].c.mover = publicationmove;
		}
		member = &boxes[0].c;
		array.h.t = &Tarray;
		array.a.len = 1;
		array.a.data = (uchar*)&member;
		storage.nrecv = 3;
		storage.ac[0].c = mode == 2 ? member : (Channel*)&array.a;
		storage.ac[0].ptr = mode == 2 ? tuple : bad[index];
		for(i = 1; i < 3; i++){
			storage.ac[i].c = &boxes[1].c;
			storage.ac[i].ptr = tuple;
		}
		result = -77;
		R.s = &storage;
		R.d = &result;
		R.IC = 17;
		current.R = R;
		publicationmoves = publicationfault = awakened = removed = 0;
		blocking = mode != 0;
		if(mode == 0){
			others[0].state = Psend;
			others[0].ptr = tuple;
			cqadd(&boxes[0].c.send, &others[0]);
			channelcheck(!kprotect(prepare, nil));
			channelcheck(boxes[0].c.send->prog == &others[0]);
			cqdelp(&boxes[0].c.send, &others[0]);
		}else{
			channelcheck(kprotect(prepare, nil) && current.state == Palt);
			publicationselected = member;
			if(mode == 2)
				current.R.d = bad[index];
			channelcheck(!kprotect(publicationfinish, &storage));
		}
		channelcheck(last_error == exBounds && result == -77);
		channelcheck(publicationmoves == 0 && awakened == 0 && !worker.nerr);
		for(i = 0; i < 2; i++){
			queueequals(boxes[i].c.send, nil, 0);
			queueequals(boxes[i].c.recv, nil, 0);
			releasequeue(boxes[i].c.send);
			releasequeue(boxes[i].c.recv);
		}
		channelcheck(!live && current.ptr == nil);
		checks++;
	}
}

static void (*volatile publicationexit)(int) = _Exit; // Avoid no-return stack cleanup after a VM error already unwound it.


/* Keep each rejected-cell jump topology in a fresh process with fully instrumented stacks. */
static void
checkbadpublication(int mode)
{
	pid_t child;
	int index, status;

	for(index = 0; index < 3; index++){
		child = fork();
		channelcheck(child >= 0);
		if(child == 0){
			badpublication(mode, index);
			publicationexit(0);
		}
		channelcheck(waitpid(child, &status, 0) == child);
		channelcheck(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		checks++;
	}
}
