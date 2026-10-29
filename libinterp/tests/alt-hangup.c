typedef struct HangupImage HangupImage;

/* Keep each blocked process's guard table, array members and output bytes independent. */
struct HangupImage
{
	uchar table[sizeof(Alt)+11*sizeof(Altc)+8]; // Three channels with up to four duplicate guards.
	uchar members[4*sizeof(Channel*)+8]; // Three channels and one inactive H entry, packed independently.
	uchar result[sizeof(WORD)+8]; // Selection output must remain unchanged on hangup.
	uchar payload[2*sizeof(WORD)+8]; // Scalar or array receive payload, never transferred here.
};


/* Map independent modeled processes to the ordinary scheduler and native interrupt adapters. */
static Prog*
hangupprocess(int index)
{
	return index == 0 ? &current : &others[index-1];
}


/* Finalization must cancel singleton, duplicate and multi-process ALT registrations exactly once. */
static void
checkalthangup(void)
{
	AltChannelBox boxes[3];
	AltArrayBox arrays[4], beforearrays[4];
	HangupImage images[4], before[4];
	REG saved, processregs[4];
	Type descriptor;
	Prog *p, outsider, *expected[1];
	Channel *member;
	Alt *a;
	Altc guard;
	int form, peers, duplicates, closing, ha, ma, mapped, trigger, background, other, i, j, counts[2];

	for(form = 0; form < 3; form++)
	for(peers = 1; peers <= 4; peers++)
	for(duplicates = 1; duplicates <= 4; duplicates++)
	for(closing = 0; closing < 3; closing++)
	for(ha = 0; ha < 8; ha++)
	for(ma = 0; ma < (form == 2 ? 8 : 1); ma++)
	for(mapped = 0; mapped < 2; mapped++)
	for(trigger = 0; trigger < 2; trigger++)
	for(background = 0; background < 2; background++){
		if(getenv("HANGUP_FORM") && form != atoi(getenv("HANGUP_FORM"))) continue;
		if(getenv("HANGUP_PEERS") && peers != atoi(getenv("HANGUP_PEERS"))) continue;
		if(getenv("HANGUP_DUPLICATES") && duplicates != atoi(getenv("HANGUP_DUPLICATES"))) continue;
		if(getenv("HANGUP_TRIGGER") && trigger != atoi(getenv("HANGUP_TRIGGER"))) continue;
		memset(&outsider, 0, sizeof(outsider));
		other = (closing+1)%3;
		expected[0] = &outsider;
		memset(boxes, 0, sizeof(boxes));
		memset(arrays, 0, sizeof(arrays));
		memset(&descriptor, 0, sizeof(descriptor));
		memset(images, 0x66, sizeof(images));
		memset(&current, 0, sizeof(current)); memset(others, 0, sizeof(others));
		descriptor.ref = 4;
		for(i = 0; i < 3; i++){
			boxes[i].c.buf = H; boxes[i].c.mover = publicationmove;
			boxes[i].c.send = emptyqueue(); boxes[i].c.recv = emptyqueue();
			if(mapped){ boxes[i].c.mover = movtmp; boxes[i].c.mid.t = &descriptor; }
		}
		if(background){
			outsider.state = form == 0 ? Psend : Precv;
			outsider.ptr = images[0].payload;
			outsider.chan = &boxes[other].c;
			cqadd(form == 0 ? &boxes[other].c.send : &boxes[other].c.recv, &outsider);
		}
		counts[0] = form == 0 ? 3*duplicates : 0;
		counts[1] = form == 0 ? 0 : form == 1 ? 3*duplicates : duplicates;
		removed = awakened = hangupinterrupts = publicationmoves = 0;
		for(i = 0; i < peers; i++){
			arrays[i].h.t = &Tarray; arrays[i].a.len = 4;
			arrays[i].a.data = images[i].members+ma;
			for(j = 0; j < 4; j++){
				member = j == 3 ? H : &boxes[j].c;
				memmove(arrays[i].a.data+j*sizeof(member), &member, sizeof(member));
			}
			a = (Alt*)(images[i].table+ha);
			memmove(a, counts, sizeof(counts));
			for(j = 0; j < counts[0]+counts[1]; j++){
				guard.c = form == 2 ? (Channel*)&arrays[i].a : &boxes[j%3].c;
				guard.ptr = images[i].payload+ha;
				memmove((uchar*)a+offsetof(Alt, ac)+j*sizeof(guard), &guard, sizeof(guard));
			}
			p = hangupprocess(i); queuerunning = p;
			p->state = Pready; p->ptr = images[i].payload;
			memset(&R, 0x55, sizeof(R)); R.s = a; R.d = images[i].result+ha;
			p->R = R; blocking = 1;
			channelcheck(kprotect(prepare, nil) && p->state == Palt);
			memmove(&processregs[i], &p->R, sizeof(p->R));
		}
		/* No hangup may publish a selection or overwrite any saved guard/member image. */
		memmove(before, images, sizeof(images));
		memmove(beforearrays, arrays, sizeof(arrays));
		memmove(&saved, &R, sizeof(R));
		hangupwatch = 1;
		if(trigger) freechan(&boxes[closing].h, 0);
		else killcomm(form == 0 ? &boxes[closing].c.send : &boxes[closing].c.recv);
		hangupwatch = 0;
		channelcheck(awakened == peers && removed == peers && !hangupinterrupts);
		channelcheck(!memcmp(&R, &saved, sizeof(R)) && !memcmp(images, before, sizeof(images)));
		channelcheck(!memcmp(arrays, beforearrays, sizeof(arrays)));
		for(i = 0; i < peers; i++){
			p = hangupprocess(i);
			channelcheck(p->state == Pready && p->ptr == nil);
			channelcheck(p->kill != nil && !strcmp(p->kill, "alt channel hungup"));
			channelcheck(!memcmp(&p->R, &processregs[i], sizeof(p->R)));
		}
		if(background){
			channelcheck(outsider.state == (form == 0 ? Psend : Precv));
			channelcheck(outsider.ptr == images[0].payload && outsider.kill == nil);
		}
		for(i = 0; i < 3; i++){
			if(i != closing){
				queueequals(boxes[i].c.send, expected, background && i == other && form == 0);
				queueequals(boxes[i].c.recv, expected, background && i == other && form != 0);
			}
			else{
				channelcheck((form == 0 ? boxes[i].c.send : boxes[i].c.recv) == nil);
				if(trigger) channelcheck(boxes[i].c.send == nil && boxes[i].c.recv == nil);
			}
			if(i != closing || !trigger) freechan(&boxes[i].h, 0);
			channelcheck(boxes[i].c.send == nil && boxes[i].c.recv == nil);
		}
		channelcheck(awakened == peers+background);
		if(background) channelcheck(outsider.state == Pready && outsider.ptr == nil && outsider.kill == changup);
		channelcheck(descriptor.ref == (mapped ? 1 : 4) && !publicationmoves && !live && !worker.nerr);
		queuerunning = &current; removed = awakened = 0; checks++;
	}
}


/* Preserve ordinary SEND/RECV hangups and native Prelease interruption while draining queues. */
static void
checkqueuehangup(void)
{
	AltChannelBox box;
	Prog *p;
	REG saved;
	int count, patterns, pattern, code, side, trigger, i, releases;

	patterns = 1;
	for(count = 0; count <= 4; count++){
		for(pattern = 0; pattern < patterns; pattern++)
		for(side = 0; side < 2; side++)
		for(trigger = 0; trigger < 2; trigger++){
			memset(&box, 0, sizeof(box));
			memset(&current, 0, sizeof(current)); memset(others, 0, sizeof(others));
			box.c.buf = H; box.c.mover = publicationmove; box.c.send = emptyqueue(); box.c.recv = emptyqueue();
			code = pattern; releases = 0; awakened = hangupinterrupts = 0;
			for(i = 0; i < count; i++){
				p = hangupprocess(i); p->ptr = &box;
				p->state = code%3 == 0 ? Psend : code%3 == 1 ? Precv : Prelease;
				if(p->state == Prelease) releases++;
				code /= 3;
				cqadd(side ? &box.c.send : &box.c.recv, p);
			}
			memset(&R, 0x55, sizeof(R)); memmove(&saved, &R, sizeof(R));
			hangupwatch = 1;
			if(trigger) freechan(&box.h, 0);
			else killcomm(side ? &box.c.send : &box.c.recv);
			hangupwatch = 0;
			channelcheck(awakened == count-releases && hangupinterrupts == releases);
			channelcheck(!memcmp(&R, &saved, sizeof(R)));
			for(i = 0; i < count; i++){
				p = hangupprocess(i); channelcheck(p->ptr == nil);
				if(p->state == Prelease) channelcheck(p->kill == nil);
				else channelcheck(p->state == Pready && p->kill == changup);
			}
			if(!trigger) freechan(&box.h, 0);
			channelcheck(box.c.send == nil && box.c.recv == nil && !live && !worker.nerr);
			awakened = 0; checks++;
		}
		patterns *= 3;
	}
}
