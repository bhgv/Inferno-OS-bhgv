
typedef struct BufferedImage BufferedImage;

struct BufferedImage {
	Heap channelheap; // Prefix proving native channel metadata has a nonwrapping origin.
	Channel channel; // Stable buffered channel with no queued peers.
	Progq send; // Empty sender sentinel.
	Progq receive; // Empty receiver sentinel.
	Heap arrayheap; // Live buffered Array header retained temporarily by C receives.
	Array array; // Ring descriptor with real native pointers and active WORD length.
	Type type; // Pointer-free scalar/raw/mapped descriptor.
	uchar data[4096]; // Ring bytes and neighboring canaries.
	uchar value[512]; // Independent external operand bytes.
	uchar slots[512]; // Possibly unaligned decoded channel-owner cell.
};


/* Initialize independent metadata and deterministic payload/canary bytes. */
static void
bufferimage(BufferedImage *image, int width, int capacity, int front, int count, int mapped)
{
	int i;

	memset(image, 0, sizeof(*image));
	for(i = 0; i < sizeof(image->data); i++)
		image->data[i] = (i*37+11)&255;
	for(i = 0; i < sizeof(image->value); i++)
		image->value[i] = (i*17+3)&255;
	memset(image->slots, 0xa5, sizeof(image->slots));
	image->channelheap.ref = image->arrayheap.ref = 2;
	image->channel.buf = &image->array;
	image->channel.send = &image->send;
	image->channel.recv = &image->receive;
	image->channel.front = front;
	image->channel.size = count;
	image->channel.mover = mapped ? movtmp : movm;
	if(mapped)
		image->channel.mid.t = &image->type;
	else
		image->channel.mid.w = width;
	image->array.len = capacity;
	image->array.t = &image->type;
	image->array.root = H;
	image->array.data = image->data+8;
	image->type.size = width;
	image->type.ref = 1;
}


/* Run C and actual native code against a captured full-image memmove oracle. */
static void
bufferpositive(Jit *code, BufferedImage *image, int receive, int shift, int alias)
{
	BufferedImage before, expected;
	REG saved, wanted;
	Channel *channel;
	ChannelLeaf transaction;
	uchar *slot, *value, *source, *destination;
	int width, index, pass, flags;

	channel = &image->channel;
	width = image->type.size;
	index = receive ? channel->front : (channel->front+channel->size)%image->array.len;
	slot = (uchar*)((uintptr)image->array.data+(uintptr)index*width);
	value = alias == 1 ? slot+1 : alias == 2 ? slot-1 : alias == 3 ? image->slots+shift : image->value+shift;
	if(width == 0)
		value = shift&1 ? H : nil;
	memset(&R, 0xa5, sizeof(R));
	R.s = receive ? image->slots+shift : value;
	R.d = receive ? value : image->slots+shift;
	R.m = (void*)1;
	memmove(receive ? R.s : R.d, &channel, sizeof(channel));
	memmove(&saved, &R, sizeof(R));
	memmove(&wanted, &R, sizeof(R));
	memmove(&before, image, sizeof(before));
	memmove(&expected, image, sizeof(expected));
	source = receive ? slot : value;
	destination = receive ? value : slot;
	if(width != 0)
		memmove((uchar*)&expected+(destination-(uchar*)image), source, width);
	expected.channel.size += receive ? -1 : 1;
	if(receive)
		expected.channel.front = (channel->front+1)%image->array.len;
	wanted.s = source;
	wanted.d = destination;
	wanted.m = &channel->mid;
	/* The real helper may only fill its private transaction, never VM state. */
	memset(&transaction, 0xa5, sizeof(transaction));
	mappedcheck(cchanleaf(&R, receive, &transaction) == 1);
	mappedcheck(!memcmp(image, &before, sizeof(before)) && !memcmp(&R, &saved, sizeof(R)));
	mappedcheck(transaction.source == source && transaction.destination == destination && transaction.bytes == width);
	for(pass = 0; pass < 2; pass++){
		memmove(image, &before, sizeof(before));
		memmove(&R, &saved, sizeof(R));
		feclearexcept(FE_ALL_EXCEPT);
		feraiseexcept(FE_DIVBYZERO);
		flags = fetestexcept(FE_ALL_EXCEPT);
		if(pass == 0)
			mappedcheck(code->entry[receive ? IRECV : ISEND](&R) == 1);
		else if(receive)
			irecv();
		else
			isend();
		mappedcheck(!memcmp(image, &expected, sizeof(expected)) && !memcmp(&R, &wanted, sizeof(R)));
		mappedcheck(fetestexcept(FE_ALL_EXCEPT) == flags);
	}
}


/* Native rejection must preserve both transaction canaries and all VM bytes. */
static void
bufferdecline(Jit *code, BufferedImage *image, int receive)
{
	BufferedImage before;
	REG saved;
	ChannelLeaf transaction, canary;

	memmove(&before, image, sizeof(before));
	memmove(&saved, &R, sizeof(R));
	memset(&transaction, 0xa5, sizeof(transaction));
	memmove(&canary, &transaction, sizeof(canary));
	mappedcheck(cchanleaf(&R, receive, &transaction) == 0);
	mappedcheck(!memcmp(&transaction, &canary, sizeof(canary)));
	mappedcheck(code->entry[receive ? IRECV : ISEND](&R) == 0);
	mappedcheck(!memcmp(image, &before, sizeof(before)) && !memcmp(&R, &saved, sizeof(R)));
}


/* Custom movers keep their existing interpreter semantics. */
static void
bufferunknown(void)
{
	abort();
}


/* Check private scratch ownership and shifted REG images without executing VM writes. */
static unsigned long
bufferpreflight(void)
{
	BufferedImage image, before;
	REG registers;
	Channel *channel;
	ChannelLeaf leaf;
	uchar regbytes[sizeof(REG)+8], saved[sizeof(REG)+8], scratch[sizeof(ChannelLeaf)+8];
	void *objects[9], *output;
	uintptr extents[9];
	int receive, shift, result, object, offset, direction;
	unsigned long checks = 0;

	for(receive = 0; receive < 2; receive++){
		bufferimage(&image, 8, 3, 0, receive ? 1 : 0, 0);
		channel = &image.channel;
		memset(&registers, 0xa5, sizeof(registers));
		registers.s = receive ? image.slots : image.value;
		registers.d = receive ? image.value : image.slots;
		memmove(image.slots, &channel, sizeof(channel));
		memmove(&before, &image, sizeof(image));
		for(shift = 0; shift < (getenv("BUFFER_PREFLIGHT_ALIASES_ONLY") ? 0 : 8); shift++){
			memset(regbytes, 0xa5, sizeof(regbytes));
			memmove(regbytes+shift, &registers, sizeof(registers));
			memmove(saved, regbytes, sizeof(saved));
			memset(scratch, 0x5a, sizeof(scratch));
			result = cchanleaf((REG*)(regbytes+shift), receive, scratch+shift);
			mappedcheck(result == 1 && !memcmp(&image, &before, sizeof(image)) && !memcmp(saved, regbytes, sizeof(saved)));
			memmove(&leaf, scratch+shift, sizeof(leaf));
			mappedcheck(leaf.source == (receive ? image.data+8 : image.value) && leaf.destination == (receive ? image.value : image.data+8) && leaf.bytes == 8);
			for(offset = 0; offset < sizeof(scratch); offset++)
				if(offset < shift || offset >= shift+sizeof(leaf))
					mappedcheck(scratch[offset] == 0x5a);
			checks++;
		}
		if(getenv("BUFFER_PREFLIGHT_ALIGNMENT_ONLY"))
			continue;
		objects[0] = &registers; extents[0] = sizeof(registers);
		objects[1] = &image.channelheap; extents[1] = sizeof(Heap)+sizeof(Channel);
		objects[2] = &image.arrayheap; extents[2] = sizeof(Heap)+sizeof(Array);
		objects[3] = &image.type; extents[3] = sizeof(Type);
		objects[4] = receive ? (void*)&image.send : (void*)&image.receive; extents[4] = sizeof(Progq);
		objects[5] = image.slots; extents[5] = sizeof(channel);
		objects[6] = image.data+8; extents[6] = 8;
		objects[7] = image.value; extents[7] = 8;
		objects[8] = receive ? (void*)&image.receive : (void*)&image.send; extents[8] = sizeof(Progq);
		for(object = 0; object < 9; object++)
		for(offset = 1-(int)sizeof(leaf); offset < (int)extents[object]; offset++){
			output = (void*)((uintptr)objects[object]+offset);
			mappedcheck(cchanleaf(&registers, receive, output) == 0);
			mappedcheck(!memcmp(&image, &before, sizeof(image)));
			checks++;
		}
		for(direction = -1; direction <= 2; direction++)
		for(offset = 0; offset < sizeof(leaf); offset++){
			output = offset == 0 ? nil : (void*)((uintptr)-1-(offset-1));
			mappedcheck(cchanleaf(&registers, direction, output) == 0 && !memcmp(&image, &before, sizeof(image)));
			checks++;
		}
	}
	return checks;
}


/* Sweep ring wrap, all supported byte widths, overlap, unaligned decoded owners and fallback. */
int
main(void)
{
	Module module = {0};
	Inst instructions[2] = {{0}};
	BufferedImage image;
	Channel *channel;
	REG saved;
	BufferedImage before;
	int receive, width, capacity, front, variant, shift, alias, mapped, bad, mover;
	void (*movers[])(void) = {movb, movw, movl, movf};
	int widths[] = {1, sizeof(WORD), 8, 8};
	int capacities[] = {1, 3, 8};
	unsigned long checks = 0, rejected = 0, preflight;
	char *expectederror;

	preflight = bufferpreflight();
	if(getenv("BUFFER_PREFLIGHT_ONLY")){
		printf("PASS buffered private preflight WORD%zu: %lu cases\n", sizeof(WORD)*8, preflight);
		return 0;
	}
	instructions[0].op = ISEND;
	instructions[1].op = IRECV;
	module.prog = instructions;
	module.nprog = 2;
	mappedcheck(compile(&module, 2, nil) && module.jit->entry[ISEND] && module.jit->entry[IRECV]);
	for(receive = 0; receive < 2; receive++)
	for(mapped = 0; mapped < 2; mapped++)
	for(width = 0; width <= (getenv("BUFFER_BAD_ONLY") ? -1 : 256); width++)
	for(capacity = 0; capacity < 3; capacity++)
	for(front = 0; front < capacities[capacity]; front++)
	for(variant = 0; variant < 2; variant++)
	for(shift = 0; shift < 8; shift++)
	for(alias = 0; alias < 4; alias++){
		bufferimage(&image, width, capacities[capacity], front,
			receive ? (variant ? capacities[capacity] : 1) : (variant ? capacities[capacity]-1 : 0), mapped);
		bufferpositive(module.jit, &image, receive, shift, alias);
		checks++;
	}
	for(receive = 0; receive < 2; receive++)
	for(mover = 0; mover < (getenv("BUFFER_BAD_ONLY") ? 0 : 4); mover++)
	for(capacity = 0; capacity < 3; capacity++)
	for(front = 0; front < capacities[capacity]; front++)
	for(variant = 0; variant < 2; variant++)
	for(shift = 0; shift < 8; shift++)
	for(alias = 0; alias < 4; alias++){
		bufferimage(&image, widths[mover], capacities[capacity], front,
			receive ? (variant ? capacities[capacity] : 1) : (variant ? capacities[capacity]-1 : 0), 0);
		image.channel.mover = movers[mover];
		bufferpositive(module.jit, &image, receive, shift, alias);
		checks++;
	}
	for(receive = 0; receive < 2; receive++)
	for(mapped = 0; mapped < (getenv("BUFFER_BAD_ONLY") ? 0 : 2); mapped++)
	for(variant = 0; variant < 3; variant++)
	for(shift = 0; shift < 8; shift++){
		bufferimage(&image, 0, 1, 0, receive ? 1 : 0, mapped);
		image.array.data = variant == 0 ? nil : variant == 1 ? H : (void*)1;
		bufferpositive(module.jit, &image, receive, shift, 0);
		checks++;
	}
	for(receive = 0; receive < 2; receive++)
	for(mapped = 0; mapped < (getenv("BUFFER_BAD_ONLY") ? 0 : 2); mapped++)
	for(variant = 0; variant < 3; variant++)
	for(shift = 0; shift < 8; shift++)
	for(alias = 0; alias < 4; alias++){
		front = variant == 0 ? 0 : variant == 1 ? 0x40000000 : 0x7ffffffe;
		bufferimage(&image, 8, 0x7fffffff, front, receive ? 1 : 0, mapped);
		image.array.data = (uchar*)((uintptr)(image.data+8)-(uintptr)front*8);
		bufferpositive(module.jit, &image, receive, shift, alias);
		checks++;
	}
	for(receive = 0; receive < 2; receive++)
	for(bad = 0; bad < 47; bad++)
	for(shift = 0; shift < 8; shift++){
		if(getenv("BUFFER_BAD") && bad != atoi(getenv("BUFFER_BAD")))
			continue;
		if(getenv("BUFFER_RECEIVE") && receive != atoi(getenv("BUFFER_RECEIVE")))
			continue;
		if(getenv("BUFFER_SHIFT") && shift != atoi(getenv("BUFFER_SHIFT")))
			continue;
		expectederror = bad == 8 || bad == 21 || (bad == 22 && shift == 0) ? exNilref : bad == 9 ? exBounds : exHeap;
		bufferimage(&image, 8, 3, 0, receive ? 1 : 0, 0);
		memset(&R, 0xa5, sizeof(R));
		channel = &image.channel;
		R.s = receive ? image.slots+shift : image.value;
		R.d = receive ? image.value : image.slots+shift;
		R.m = (void*)1;
		memmove(receive ? R.s : R.d, &channel, sizeof(channel));
		if(bad == 0)image.channel.buf = H;
		if(bad == 1)image.channel.buf = nil;
		if(bad == 2)image.channel.front = -1;
		if(bad == 3)image.channel.front = 3;
		if(bad == 4)image.channel.size = -1;
		if(bad == 5)image.channel.size = 4;
		if(bad == 6)image.type.np = -1;
		if(bad == 7){ image.type.np = 1; image.type.map[0] = 0x40; }
		if(bad == 8)image.array.data = H;
		if(bad == 9)image.array.data = (void*)((uintptr)-1-8);
		if(bad == 10)image.array.len = 0;
		if(bad == 11)image.arrayheap.ref = 0;
		if(bad == 12)image.arrayheap.ref = (ulong)-1;
		if(bad == 13)(receive ? &image.send : &image.receive)->prog = (Prog*)1;
		if(bad == 14)image.channel.mover = bufferunknown;
		if(bad == 15)image.channel.mid.w = 9;
		if(bad == 16){ if(receive)R.d = &image.channel.front; else R.s = &image.channel.front; }
		if(bad == 17){ if(receive)R.d = &R; else R.s = &R; }
		if(bad == 18){ image.type.size = 257; image.channel.mid.w = 257; }
		if(bad == 19){ if(receive)R.s = shift&1 ? H : nil; else R.d = shift&1 ? H : nil; }
		if(bad == 20){ if(receive)R.s = (void*)((uintptr)-1-shift); else R.d = (void*)((uintptr)-1-shift); }
		if(bad == 21 || bad == 22){
			Channel *wrong;

			wrong = bad == 21 ? (shift&1 ? H : nil) : (void*)((uintptr)-1-shift);
			memmove(receive ? R.s : R.d, &wrong, sizeof(wrong));
		}
		if(bad == 23 || bad == 24){
			Progq *wrong;

			wrong = bad == 23 ? (shift&1 ? H : nil) : (void*)((uintptr)-1-shift);
			if(receive)image.channel.send = wrong;
			else image.channel.recv = wrong;
		}
		if(bad == 25)image.channel.mover = nil;
		if(bad == 26)image.channel.mover = (void(*)(void))(uintptr)-1;
		if(bad >= 27 && bad < 41){
			Progq **owner, *head;
			int kind;

			/* Exercise the selected queue and the opposite enqueue queue independently. */
			owner = (bad < 34 ? receive : !receive) ? &image.channel.send : &image.channel.recv;
			head = *owner;
			kind = (bad-27)%7;
			if(kind == 0) *owner = nil;
			if(kind == 1) *owner = H;
			if(kind == 2) *owner = (void*)1;
			if(kind == 3) *owner = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
			if(kind == 4) head->prog = H;
			if(kind == 5) head->prog = (void*)1;
			if(kind == 6) head->prog = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		}
		if(bad >= 41){
			Progq *head;
			int kind;

			head = (bad < 44 ? receive : !receive) ? &image.send : &image.receive;
			kind = (bad-41)%3;
			head->next = kind == 0 ? H : kind == 1 ? head :
				(bad < 44 ? (receive ? &image.receive : &image.send) : (receive ? &image.send : &image.receive));
		}
		/* Send has no temporary Array retain, so dead-ref rejection is receive-only. */
		if(!receive && (bad == 11 || bad == 12))
			continue;
		bufferdecline(module.jit, &image, receive);
		rejected++;
		if(bad >= 19 && !getenv("BUFFER_NO_C_ERRORS")){
			memmove(&before, &image, sizeof(before));
			memmove(&saved, &R, sizeof(saved));
			raised = nil;
			if(setjmp(boundary) == 0){
				if(receive)irecv(); else isend();
				mappedcheck(0);
			}
			mappedcheck(raised == expectederror && !memcmp(&before, &image, sizeof(before)) && !memcmp(&saved, &R, sizeof(saved)));
		}
		if(bad >= 2 && bad <= 10 && (bad != 10 || receive) && !getenv("BUFFER_NO_C_ERRORS")){
			memmove(&before, &image, sizeof(before));
			memmove(&saved, &R, sizeof(saved));
			raised = nil;
			if(setjmp(boundary) == 0){
				if(receive)cgetb(&image.channel, image.value);
				else cputb(&image.channel, image.value);
				mappedcheck(0);
			}
			mappedcheck(raised == expectederror && !memcmp(&before, &image, sizeof(before)) && !memcmp(&saved, &R, sizeof(saved)));
		}
	}
	module.jit->freecode(module.jit->code, module.jit->size);
	free(module.jit);
	printf("PASS buffered C/RX WORD%zu: %lu accepted, %lu declined, %lu private-preflight cases\n", sizeof(WORD)*8, checks, rejected, preflight);
	return 0;
}
