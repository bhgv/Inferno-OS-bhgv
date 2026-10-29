typedef struct QueueAttempt QueueAttempt;

/* Keep native operands independent of the protected error frame. */
struct QueueAttempt
{
	Progq **owner; // Root cell consumed by the real queue operation.
	Prog *process; // Append/removal target, including malformed target probes.
	int operation; // Zero appends, one cancels every match, two removes the head.
};


/* Execute authoritative queue code with the ordinary VM exception boundary. */
static int
queueattempt(void *arg)
{
	QueueAttempt *attempt;

	attempt = arg;
	if(attempt->operation == 0) cqadd(attempt->owner, attempt->process);
	else if(attempt->operation == 1) cqdelp(attempt->owner, attempt->process);
	else cqdel(attempt->owner);
	return 1;
}


/* Check rejection before allocation, freeing, link changes or register publication. */
static void
queueunchanged(QueueAttempt *attempt, Progq **nodes, int length)
{
	Progq before[16], *root;
	REG saved;
	int i, count;

	for(i = 0; i < length; i++) memmove(&before[i], nodes[i], sizeof(before[i]));
	root = attempt->owner == nil || attempt->owner == H ||
		(uintptr)attempt->owner%sizeof(void*) != 0 ||
		(uintptr)attempt->owner > (uintptr)-1-sizeof(root) ? nil : *attempt->owner;
	memset(&R, 0x55, sizeof(R)); memmove(&saved, &R, sizeof(saved));
	count = live;
	fail_after = 0;
	channelcheck(!kprotect(queueattempt, attempt) && last_error == exHeap);
	fail_after = -1;
	channelcheck(live == count && !memcmp(&R, &saved, sizeof(R)) && !worker.nerr);
	if(root != nil) channelcheck(*attempt->owner == root);
	for(i = 0; i < length; i++) channelcheck(!memcmp(nodes[i], &before[i], sizeof(before[i])));
	checks++;
}


/* Sweep malformed interior nodes and every cycle entry/length for linear walkers. */
static void
checkqueuechains(void)
{
	Progq *nodes[16], *root;
	QueueAttempt attempt;
	int operation, length, position, kind, i;

	for(operation = 0; operation < 2; operation++)
	for(length = 1; length <= 16; length++)
	for(position = 0; position < length; position++)
	for(kind = 0; kind < 8; kind++){
		if(getenv("QUEUE_CHAIN_OP") && operation != atoi(getenv("QUEUE_CHAIN_OP"))) continue;
		if(getenv("QUEUE_CHAIN_KIND") && kind != atoi(getenv("QUEUE_CHAIN_KIND"))) continue;
		if(kind == 6 && length == 1) continue;
		for(i = 0; i < length; i++) nodes[i] = emptyqueue();
		for(i = 0; i < length; i++){
			nodes[i]->prog = i%2 ? &others[0] : &current;
			nodes[i]->next = i+1 < length ? nodes[i+1] : nil;
		}
		if(kind == 0) nodes[position]->prog = H;
		if(kind == 1) nodes[position]->prog = (void*)1;
		if(kind == 2) nodes[position]->prog = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(kind == 3) nodes[position]->next = H;
		if(kind == 4) nodes[position]->next = (void*)1;
		if(kind == 5) nodes[position]->next = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(kind == 6) nodes[position]->prog = nil;
		if(kind == 7) nodes[length-1]->next = nodes[position];
		root = nodes[0];
		attempt.owner = &root; attempt.process = kind == 7 ? &others[2] : &current; attempt.operation = operation;
		queueunchanged(&attempt, nodes, length);
		for(i = 0; i < length; i++) nodefree(nodes[i]);
		channelcheck(!live);
	}
}


/* Reject unusable root cells, heads and append/cancellation targets before typed access. */
static void
checkqueueslots(void)
{
	Progq *nodes[1], *root;
	QueueAttempt attempt;
	int operation, kind;

	for(operation = 0; operation < 3; operation++)
	for(kind = 0; kind < (operation == 2 ? 8 : 12); kind++){
		nodes[0] = emptyqueue(); nodes[0]->prog = &current; root = nodes[0];
		attempt.owner = &root; attempt.process = &current; attempt.operation = operation;
		if(kind == 0) attempt.owner = nil;
		if(kind == 1) attempt.owner = H;
		if(kind == 2) attempt.owner = (void*)1;
		if(kind == 3) attempt.owner = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(kind == 4) root = nil;
		if(kind == 5) root = H;
		if(kind == 6) root = (void*)1;
		if(kind == 7) root = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(kind == 8) attempt.process = nil;
		if(kind == 9) attempt.process = H;
		if(kind == 10) attempt.process = (void*)1;
		if(kind == 11) attempt.process = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		queueunchanged(&attempt, nodes, 1);
		nodefree(nodes[0]); channelcheck(!live);
	}
}


/* Guard constant-time dequeue's head and immediate successor without a tail scan. */
static void
checkqueuepop(void)
{
	Progq *nodes[2], *root;
	QueueAttempt attempt;
	int kind, i;

	for(kind = 0; kind < 10; kind++){
		for(i = 0; i < 2; i++) nodes[i] = emptyqueue();
		nodes[0]->prog = &current; nodes[0]->next = nodes[1];
		nodes[1]->prog = &others[0];
		if(kind == 0) nodes[0]->prog = nil;
		if(kind == 1) nodes[0]->next = nodes[0];
		if(kind == 2) nodes[0]->next = H;
		if(kind == 3) nodes[0]->next = (void*)1;
		if(kind == 4) nodes[0]->next = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(kind == 5) nodes[1]->prog = nil;
		if(kind == 6) nodes[1]->prog = H;
		if(kind == 7) nodes[1]->prog = (void*)1;
		if(kind == 8) nodes[1]->prog = (void*)((uintptr)-1 & ~(uintptr)(sizeof(void*)-1));
		if(kind == 9) nodes[0]->prog = H;
		root = nodes[0]; attempt.owner = &root; attempt.process = &current; attempt.operation = 2;
		queueunchanged(&attempt, nodes, 2);
		for(i = 0; i < 2; i++) nodefree(nodes[i]);
		channelcheck(!live);
	}
}


/* Exercise valid lengths across Brent checkpoint boundaries and preserve failed appends. */
static void
checkqueuelong(void)
{
	static int lengths[] = {0, 1, 2, 3, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256, 257};
	Progq *root, *node, *nodes[257];
	Progq before[257];
	Prog *expected[258], *process;
	QueueAttempt attempt;
	REG saved;
	int size, pattern, target, length, i, count, prior;

	for(size = 0; size < nelem(lengths); size++)
	for(pattern = 0; pattern < 4; pattern++)
	for(target = 0; target < 3; target++){
		length = lengths[size];
		root = emptyqueue(); nodes[0] = root;
		count = 0;
		for(i = 0; i < length; i++){
			if(i != 0) nodes[i] = emptyqueue();
			process = pattern == 0 ? &current : &others[(i+pattern)%3];
			nodes[i]->prog = process;
			if(i != 0) nodes[i-1]->next = nodes[i];
			if(process != &others[target]) expected[count++] = process;
		}
		memset(&R, 0x55, sizeof(R)); memmove(&saved, &R, sizeof(saved));
		if(length != 0){
			for(i = 0; i < length; i++) memmove(&before[i], nodes[i], sizeof(before[i]));
			attempt.owner = &root; attempt.process = &current; attempt.operation = 0;
			prior = live; node = root; fail_after = 0;
			channelcheck(!kprotect(queueattempt, &attempt) && last_error == exNomem);
			fail_after = -1;
			channelcheck(root == node && live == prior);
			for(i = 0; i < length; i++) channelcheck(!memcmp(nodes[i], &before[i], sizeof(before[i])));
		}
		cqdelp(&root, &others[target]);
		queueequals(root, expected, count);
		cqadd(&root, &current); expected[count++] = &current;
		queueequals(root, expected, count);
		for(i = 0; i < count; i++){
			channelcheck(root->prog == expected[i]);
			cqdel(&root);
		}
		queueequals(root, nil, 0);
		channelcheck(!memcmp(&R, &saved, sizeof(R)) && !worker.nerr);
		releasequeue(root); channelcheck(!live); checks++;
	}
}
