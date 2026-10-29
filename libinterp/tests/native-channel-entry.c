typedef struct EntryAttempt EntryAttempt;

/* Stable native arguments and result live outside the protected jump frames. */
struct EntryAttempt
{
	Channel *channel; // Concrete channel used by all native entry variants.
	void *payload; // Packed original source or destination cell.
	Type *type; // Payload descriptor required by best-effort send cleanup.
	int operation; // Zero sends, one receives, two sends without blocking.
	int capacity; // Buffer capacity for the best-effort send variant.
	int result; // Assigned only after the public entry returns normally.
};

static int entrymovecalls, entryfailcall; // Mover ordinal selects receive versus queued-sender refill failures.
static int entrymovemode, entrypointers; // Controlled real scalar/pointer mover followed by REG borrowing.


/* Execute the production mover, including real ownership, with optional errors before/after publication. */
static void
entrymove(void)
{
	entrymovecalls++;
	if(entrymovemode == 2 && entrymovecalls >= entryfailcall) entrytouch(2);
	if(entrypointers) movp(); else movw();
	entrytouch(entrymovemode == 3 && entrymovecalls >= entryfailcall ? 2 : 1);
}


/* The actual emu/os entry implementations run beneath an outer native error handler. */
static int
entryattempt(void *arg)
{
	EntryAttempt *call;

	call = arg;
	if(call->operation == 0){
		csend(call->channel, call->payload);
		call->result = 1;
	}else if(call->operation == 1){
		crecv(call->channel, call->payload);
		call->result = 1;
	}else
		call->result = csendalt(call->channel, call->payload, call->type, call->capacity);
	return 1;
}


/* Compare native register restoration while actual C movers copy/retain packed operands. */
static void
checkentry(int family)
{
	TestChannel box;
	EntryAttempt call;
	REG saved;
	uchar source[32], destination[32];
	void *input, *output;
	WORD value, got;
	int api, pointers, buffered, sa, da, mode, ok, consumed;

	memset(&entrypoison, 0xe3, sizeof(entrypoison));
	for(api = 0; api < 3; api++)
	for(pointers = 0; pointers < 2; pointers++)
	for(buffered = 0; buffered < 2; buffered++)
	for(sa = 0; sa < 8; sa++)
	for(da = 0; da < 8; da++)
	for(mode = 1; mode <= (family == 0 ? 3 : 2); mode++){
		if(getenv("ENTRY_API") && api != atoi(getenv("ENTRY_API"))) continue;
		if(getenv("ENTRY_MODE") && mode != atoi(getenv("ENTRY_MODE"))) continue;
		if(getenv("ENTRY_BUFFERED") && buffered != atoi(getenv("ENTRY_BUFFERED"))) continue;
		if(getenv("ENTRY_POINTERS") && pointers != atoi(getenv("ENTRY_POINTERS"))) continue;
		if(family != 0 && api != 2) continue;
		if(family == 1 && buffered) continue;
		if(family == 2 && !pointers) continue;
		openchannel(&box, buffered ? 2 : 0, pointers);
		memset(peers, 0, sizeof(peers));
		memset(source, 0xa5, sizeof(source));
		memset(destination, 0xa5, sizeof(destination));
		value = 99;
		input = pointers ? payload(value) : H;
		output = H;
		memmove(source+sa, pointers ? (void*)&input : (void*)&value, pointers ? sizeof(input) : sizeof(value));
		if(pointers) memmove(destination+da, &output, sizeof(output));
		else{ got = -77; memmove(destination+da, &got, sizeof(got)); }
		if(family == 0){
			if(buffered && api == 1) sendvalue(&box.c, value, pointers);
			if(!buffered){
				peers[0].state = api == 1 ? Psend : Precv;
				peers[0].ptr = api == 1 ? source+sa : destination+da;
				cqadd(api == 1 ? &box.c.send : &box.c.recv, &peers[0]);
			}
		}
		box.c.mover = family == 0 ? entrymove : pointers ? movp : movw;
		entrypointers = pointers;
		entrymovemode = mode;
		entrymovecalls = 0; entryfailcall = 1;
		entrypool = family == 1 ? mode : 0;
		entrydestroy = family == 2 ? mode : 0;
		entrytouches = 0;
		call.channel = &box.c;
		call.payload = api == 1 ? destination+da : source+sa;
		call.type = pointers ? &pointer : &scalar;
		call.operation = api;
		call.capacity = family == 2 ? -1 : 3;
		call.result = -77;
		memset(&R, 0x55, sizeof(R)); memmove(&saved, &R, sizeof(saved));
		removed = awakened = 0;
		ok = kprotect(entryattempt, &call);
		entrypool = entrydestroy = 0;
		if(memcmp(&R, &saved, sizeof(R)) || !entrytouches)
			fprintf(stderr, "entry context api=%d pointers=%d buffered=%d mode=%d family=%d touches=%d ok=%d error=%s\n", api, pointers, buffered, mode, family, entrytouches, ok, last_error ? last_error : "none");
		channelcheck(!memcmp(&R, &saved, sizeof(R)) && entrytouches > 0);
		channelcheck(ok == (mode == 1) && (ok || last_error == entryerror));
		channelcheck(ok ? call.result == (family == 2 && !buffered ? 0 : 1) : call.result == -77);
		channelcheck(!removed && !worker.nerr);
		consumed = api == 2 && ok;
		if(pointers && (!buffered || api == 1)){
			memmove(&output, destination+da, sizeof(output));
			if(output != H) destroy(output);
		}
		closechannel(&box.c);
		if(pointers && !consumed) destroy(input);
		channelcheck(!live && !nodes);
		checks++;
	}
	awakened = 0;
}


/* Native waits preserve scheduler resume/quantum state instead of restoring an obsolete REG snapshot. */
static void
checkentryblocking(void)
{
	TestChannel box;
	EntryAttempt call;
	WORD value = 99;
	int receive, mode, ok;

	memset(&entryresume, 0x23, sizeof(entryresume)); entryresume.IC = 1;
	for(receive = 0; receive < 2; receive++)
	for(mode = 1; mode <= 2; mode++){
		if(getenv("ENTRY_MODE") && mode != atoi(getenv("ENTRY_MODE"))) continue;
		openchannel(&box, 0, 0);
		current.chan = &box.c;
		call.channel = &box.c; call.payload = &value;
		call.operation = receive; call.result = -77;
		memset(&R, 0x55, sizeof(R));
		entryblock = mode;
		ok = kprotect(entryattempt, &call);
		entryblock = 0;
		channelcheck(ok == (mode == 1) && (ok || last_error == entryerror));
		channelcheck(!memcmp(&R, &entryresume, sizeof(R)) && current.ptr == nil);
		channelcheck(box.c.send->prog == nil && box.c.recv->prog == nil && !worker.nerr);
		closechannel(&box.c); channelcheck(!live && !nodes); checks++;
	}
}


/* Malformed native readiness inputs reject before registering, allocating or changing REG. */
static void
checkentryinvalid(void)
{
	TestChannel box;
	EntryAttempt call;
	REG saved;
	Array fake;
	Progq queue, *original;
	WORD value = 99;
	int api, bad, before;

	for(api = 0; api < 3; api++)
	for(bad = 0; bad < 16; bad++){
		if(getenv("ENTRY_API") && api != atoi(getenv("ENTRY_API"))) continue;
		if(getenv("ENTRY_BAD") && bad != atoi(getenv("ENTRY_BAD"))) continue;
		openchannel(&box, 0, 0);
		call.channel = &box.c; call.payload = &value; call.type = &scalar;
		call.operation = api; call.capacity = 3; call.result = -77;
		memset(&fake, 0, sizeof(fake)); memset(&queue, 0, sizeof(queue));
		if(bad == 0) call.channel = nil;
		if(bad == 1) call.channel = H;
		if(bad == 2) call.channel = (void*)1;
		if(bad == 3) box.c.buf = nil;
		if(bad == 4) box.c.size = -1;
		if(bad == 5) box.c.size = 1;
		if(bad == 6){ box.c.buf = &fake; fake.len = -1; }
		if(bad == 7){ box.c.buf = &fake; fake.len = 1; box.c.size = 2; }
		if(bad == 8){ box.c.buf = &fake; fake.len = (WORD)0x7fffffff+((sizeof(WORD) == 8) ? 1LL : 0); if(sizeof(WORD) == 4) fake.len = -1; }
		if(bad >= 9){
			if(bad == 9) queue.prog = H;
			if(bad == 10) queue.prog = (void*)1;
			if(bad == 11) queue.prog = (void*)((uintptr)-1-sizeof(Prog)+1);
			original = api == 1 ? box.c.send : box.c.recv;
			if(api == 1) box.c.send = &queue; else box.c.recv = &queue;
			if(bad >= 12){
				Progq *badqueue;

				badqueue = bad == 12 ? nil : bad == 13 ? H : bad == 14 ? (void*)((uintptr)-1-sizeof(Progq)+1) : (void*)1;
				if(api == 1) box.c.send = badqueue; else box.c.recv = badqueue;
			}
		}
		memset(&R, 0x55, sizeof(R)); memmove(&saved, &R, sizeof(saved)); before = allocations;
		channelcheck(!kprotect(entryattempt, &call));
		channelcheck(last_error == (bad == 0 || bad == 1 || bad == 3 ? exNilref : exHeap));
		channelcheck(!memcmp(&R, &saved, sizeof(R)) && allocations == before && call.result == -77);
		if(bad >= 9){ if(api == 1) box.c.send = original; else box.c.recv = original; }
		box.c.buf = H; box.c.size = 0;
		closechannel(&box.c); channelcheck(!live && !nodes && !worker.nerr); checks++;
	}
}


/* Balance temporary array/channel references when either stage of queued-sender refill raises. */
static void
checkentryrefill(void)
{
	TestChannel box;
	EntryAttempt call;
	REG saved;
	uchar source[32], destination[32];
	void *input, *output;
	WORD value;
	int pointers, mode, stage, sa, da, ok;

	memset(&entrypoison, 0xe3, sizeof(entrypoison));
	for(pointers = 0; pointers < 2; pointers++)
	for(mode = 1; mode <= 3; mode++)
	for(stage = 1; stage <= 2; stage++)
	for(sa = 0; sa < 8; sa++)
	for(da = 0; da < 8; da++){
		if(getenv("ENTRY_MODE") && mode != atoi(getenv("ENTRY_MODE"))) continue;
		if(getenv("ENTRY_STAGE") && stage != atoi(getenv("ENTRY_STAGE"))) continue;
		if(getenv("ENTRY_POINTERS") && pointers != atoi(getenv("ENTRY_POINTERS"))) continue;
		openchannel(&box, 1, pointers);
		sendvalue(&box.c, 77, pointers);
		value = 99; input = pointers ? payload(value) : H; output = H;
		memmove(source+sa, pointers ? (void*)&input : (void*)&value, pointers ? sizeof(input) : sizeof(value));
		if(pointers) memmove(destination+da, &output, sizeof(output));
		memset(&peers[0], 0, sizeof(peers[0]));
		peers[0].ptr = source+sa; peers[0].state = Psend;
		cqadd(&box.c.send, &peers[0]);
		box.c.mover = entrymove; entrypointers = pointers; entrymovemode = mode;
		entrymovecalls = 0; entryfailcall = stage;
		call.channel = &box.c; call.payload = destination+da; call.operation = 1; call.result = -77;
		memset(&R, 0x55, sizeof(R)); memmove(&saved, &R, sizeof(saved));
		removed = awakened = 0;
		ok = kprotect(entryattempt, &call);
		channelcheck(ok == (mode == 1) && (ok || last_error == entryerror));
		channelcheck(!memcmp(&R, &saved, sizeof(R)) && box.h.ref == 1 && D2H(box.c.buf)->ref == 1);
		channelcheck(peers[0].ptr == (ok ? nil : source+sa) && !removed && !worker.nerr);
		if(pointers){ memmove(&output, destination+da, sizeof(output)); if(output != H) destroy(output); }
		closechannel(&box.c);
		if(pointers) destroy(input);
		channelcheck(!live && !nodes); checks++;
	}
	awakened = 0;
}


/* Invoke the same authoritative instruction used by the interpreter and JIT C fallback. */
static int
directchannelattempt(void *arg)
{
	int *receive;

	receive = arg;
	if(*receive) irecv(); else isend();
	return 1;
}


/* Reject malformed readiness before payload, REG, peer queues or scheduler publication. */
static void
checkdirectinvalid(void)
{
	TestChannel box;
	Channel *channel;
	Progq queue, *original;
	Array fake;
	REG saved;
	uchar slot[32], payloadcell[32], beforepayload[32];
	WORD value;
	int receive, side, bad, ca, pa, before, oldnodes, oldremoved, oldawakened;

	for(receive = 0; receive < 2; receive++)
	for(side = 0; side < 2; side++)
	for(bad = 0; bad < 18; bad++)
	for(ca = 0; ca < 8; ca++)
	for(pa = 0; pa < 8; pa++){
		openchannel(&box, 0, 0);
		channel = &box.c;
		memset(&fake, 0, sizeof(fake));
		memset(&queue, 0, sizeof(queue));
		original = side ? box.c.send : box.c.recv;
		if(bad == 0) channel = nil;
		if(bad == 1) channel = H;
		if(bad == 2) channel = (void*)1;
		if(bad == 3) box.c.buf = nil;
		if(bad == 4) box.c.size = -1;
		if(bad == 5) box.c.size = 1;
		if(bad == 6){ box.c.buf = &fake; fake.len = -1; }
		if(bad == 7){ box.c.buf = &fake; fake.len = 1; box.c.size = 2; }
		if(bad == 8){ box.c.buf = &fake; fake.len = -1; }
		if(bad >= 9 && bad < 16){
			if(bad == 9) queue.prog = H;
			if(bad == 10) queue.prog = (void*)1;
			if(bad == 11) queue.prog = (void*)((uintptr)-1-sizeof(Prog)+1);
			if(side) box.c.send = &queue; else box.c.recv = &queue;
			if(bad >= 12){
				Progq *invalid;

				invalid = bad == 12 ? nil : bad == 13 ? H : bad == 14 ?
					(void*)((uintptr)-1-sizeof(Progq)+1) : (void*)1;
				if(side) box.c.send = invalid; else box.c.recv = invalid;
			}
		}
		if(bad == 16) box.c.buf = (void*)1;
		if(bad == 17) box.c.buf = (void*)((uintptr)-1-sizeof(Array)+1);
		memset(slot, 0x77, sizeof(slot));
		memset(payloadcell, 0x66, sizeof(payloadcell));
		value = 99;
		memmove(slot+ca, &channel, sizeof(channel));
		memmove(payloadcell+pa, &value, sizeof(value));
		memmove(beforepayload, payloadcell, sizeof(payloadcell));
		memset(&R, 0x55, sizeof(R));
		R.s = receive ? slot+ca : payloadcell+pa;
		R.d = receive ? payloadcell+pa : slot+ca;
		memmove(&saved, &R, sizeof(saved));
		before = allocations; oldnodes = nodes;
		oldremoved = removed; oldawakened = awakened;
		channelcheck(!kprotect(directchannelattempt, &receive));
		channelcheck(last_error == (bad == 0 || bad == 1 || bad == 3 ? exNilref : exHeap));
		channelcheck(!memcmp(&R, &saved, sizeof(R)));
		channelcheck(!memcmp(payloadcell, beforepayload, sizeof(payloadcell)));
		channelcheck(allocations == before && nodes == oldnodes);
		channelcheck(removed == oldremoved && awakened == oldawakened);
		if(side) box.c.send = original; else box.c.recv = original;
		channelcheck(original->prog == nil && original->next == nil);
		box.c.buf = H; box.c.size = 0;
		closechannel(&box.c);
		channelcheck(!live && !nodes && !worker.nerr);
		checks++;
	}
}
