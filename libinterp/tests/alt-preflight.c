

/* Reject malformed later guards before any registration, allocation or output publication. */
static void
checkaltmetadata(void)
{
	AltChannelBox boxes[3], beforeboxes[3];
	AltArrayBox array, fake;
	Progq *original[6], beforequeues[6], tail, beforetail;
	AltGuardTable table;
	AltImages images, beforeimages;
	Alt *a;
	Altc guard;
	Channel *badchannel, *member;
	Prog beforeprog;
	REG saved;
	WORD result;
	uvlong beforeattempts;
	int form, position, bad, ha, ma, background, block, i, count, prior, ok;
	char *expected;

	for(form = 0; form < 3; form++)
	for(position = 0; position < 3; position++)
	for(bad = 0; bad < 32; bad++)
	for(ha = 0; ha < 8; ha++)
	for(ma = 0; ma < 8; ma++)
	for(background = 0; background < 2; background++)
	for(block = 0; block < 2; block++){
		if(bad >= 24 && bad <= 30 && form != 2) continue;
		if(bad == 31 && form != 0) continue;
		if(getenv("ALT_META_BAD") && bad != atoi(getenv("ALT_META_BAD"))) continue;
		if(getenv("ALT_META_FORM") && form != atoi(getenv("ALT_META_FORM"))) continue;
		memset(boxes, 0, sizeof(boxes));
		memset(&array, 0, sizeof(array));
		memset(&fake, 0, sizeof(fake));
		memset(&tail, 0, sizeof(tail));
		memset(&current, 0, sizeof(current));
		memset(&images, 0x66, sizeof(images));
		for(i = 0; i < 3; i++){
			boxes[i].c.buf = H;
			original[2*i] = boxes[i].c.send = emptyqueue();
			original[2*i+1] = boxes[i].c.recv = emptyqueue();
			if(background){
				cqadd(&boxes[i].c.send, &others[0]);
				cqadd(&boxes[i].c.recv, &others[0]);
			}
		}
		badchannel = &boxes[position].c;
		expected = bad == 0 || bad == 1 || bad == 4 ? exNilref : exHeap;
		if(bad == 0) badchannel = nil;
		if(bad == 1) badchannel = H;
		if(bad == 2) badchannel = (void*)1;
		if(bad == 3) badchannel = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(bad == 4) badchannel->buf = nil;
		if(bad == 5) badchannel->buf = (void*)1;
		if(bad == 6) badchannel->buf = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(bad == 7) badchannel->size = -1;
		if(bad == 8) badchannel->size = 1;
		if(bad == 9) badchannel->send = nil;
		if(bad == 10) badchannel->recv = H;
		if(bad == 11) badchannel->send = (void*)1;
		if(bad == 12) badchannel->recv = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(bad == 13) badchannel->send->prog = H;
		if(bad == 14) badchannel->recv->prog = (void*)1;
		if(bad == 15) badchannel->send->prog = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(bad == 16){ badchannel->recv->prog = nil; badchannel->recv->next = badchannel->recv; }
		if(bad >= 17 && bad <= 20){
			badchannel->send->prog = &others[0];
			if(bad == 17) badchannel->send->next = badchannel->send;
			else if(bad == 18) badchannel->send->next = (void*)1;
			else badchannel->send->next = &tail;
			if(bad == 20) tail.prog = H;
		}
		if(bad == 21){ badchannel->buf = &fake.a; fake.a.len = 1; badchannel->size = 2; }
		if(bad == 22){ badchannel->buf = &fake.a; fake.a.len = -1; }
		if(bad == 23) badchannel = (void*)sizeof(Heap);
		array.h.t = &Tarray;
		array.a.len = 3;
		array.a.data = images.members+ma;
		member = &boxes[(position+1)%3].c;
		memmove(array.a.data, &member, sizeof(member));
		member = H;
		memmove(array.a.data+sizeof(member), &member, sizeof(member));
		memmove(array.a.data+2*sizeof(member), &badchannel, sizeof(badchannel));
		if(bad >= 24 && bad <= 28){
			expected = exBounds;
			if(bad == 24) array.a.len = -1;
			if(bad == 25) array.a.len = (WORD)0x7fffffff;
			if(bad == 26) array.a.data = nil;
			if(bad == 27) array.a.data = H;
			if(bad == 28) array.a.data = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		}
		if(bad == 29){
			fake.h.t = &Tarray;
			fake.a.len = 0; fake.a.data = H;
			member = (Channel*)&fake.a;
			memmove(array.a.data+2*sizeof(member), &member, sizeof(member));
		}
		memset(&table, 0, sizeof(table));
		table.nsend = form == 0 ? 3 : 0;
		table.nrecv = form == 0 ? 0 : 3;
		for(i = 0; i < 3; i++){
			guard.c = &boxes[i].c;
			if(i == position){
				guard.c = badchannel;
				if(form == 2 && bad != 1) guard.c = (Channel*)&array.a;
			}
			guard.ptr = images.payload+8;
			table.ac[i] = guard;
		}
		if(bad == 30) table.ac[position].c = (void*)((uchar*)&array.a+1);
		if(bad == 31) table.ac[position].c = (Channel*)&array.a;
		a = (Alt*)(images.table+ha);
		count = offsetof(Alt, ac)+3*sizeof(Altc);
		memmove(a, &table, count);
		result = -77;
		memmove(images.result+(ha+ma)%8, &result, sizeof(result));
		memset(&R, 0x55, sizeof(R));
		R.s = a; R.d = images.result+(ha+ma)%8;
		current.state = Pready; current.R = R;
		blocking = block; removed = awakened = 0;
		memmove(&beforeimages, &images, sizeof(images));
		memmove(beforeboxes, boxes, sizeof(boxes));
		memmove(&beforetail, &tail, sizeof(tail));
		memmove(&saved, &R, sizeof(R));
		/* ALT clears t before xecalt; NBALT leaves the original value intact. */
		if(block) saved.t = 0;
		memmove(&beforeprog, &current, sizeof(current));
		for(i = 0; i < 6; i++) memmove(&beforequeues[i], original[i], sizeof(beforequeues[i]));
		prior = live; beforeattempts = nodeattempts; fail_after = 0;
		ok = kprotect(prepare, nil);
		if(ok || last_error != expected)
			fprintf(stderr, "ALT metadata form=%d position=%d bad=%d ha=%d ma=%d "
				"background=%d block=%d ok=%d error=%s expected=%s\n",
				form, position, bad, ha, ma, background, block, ok, last_error == nil ? "<none>" : last_error, expected);
		channelcheck(!ok && last_error == expected);
		fail_after = -1;
		channelcheck(live == prior && nodeattempts == beforeattempts && !worker.nerr && !removed && !awakened);
		channelcheck(!memcmp(&R, &saved, sizeof(R)) && !memcmp(&current, &beforeprog, sizeof(current)));
		channelcheck(!memcmp(&images, &beforeimages, sizeof(images)) && !memcmp(boxes, beforeboxes, sizeof(boxes)));
		channelcheck(!memcmp(&tail, &beforetail, sizeof(tail)));
		for(i = 0; i < 6; i++){
			channelcheck(!memcmp(original[i], &beforequeues[i], sizeof(beforequeues[i])));
			nodefree(original[i]);
		}
		channelcheck(!live); checks++;
	}
}
