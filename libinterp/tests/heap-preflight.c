static Type *pretype; // Selected constructor or read-only clone descriptor.
static Array *prearray; // Selected source for protected array cloning.
static void *presrc, *predst; // Selected mapped source and destination spans.
static int preop; // Constructor/array/mapped operation selected by the oracle.
static unsigned long prechecks; // Number of independent rejection or copy cases.

static union {
	Heap align; // Natural alignment before applying byte shifts.
	uchar bytes[sizeof(Heap)+sizeof(Array)+16]; // Complete fake source and guard bytes.
} preowner;

static union {
	Heap align; // Native alignment for constructor descriptors.
	uchar bytes[sizeof(Type)+8]; // Complete descriptor image and all byte shifts.
} predescriptor;

static struct {
	Heap heap; // Canonical child header with a retained fixture owner.
	uchar data[8]; // Opaque native child identity.
} prechildren[2];

static uchar preinput[64], preoutput[64], presaved[64], preoutsave[64]; // Stable operand/canary images.


/* Invoke constructors or cloning inside the real VM error-stack protocol. */
static int
preinvoke(void *unused)
{
	(void)unused;
	if(preop == 0)heapz(pretype);
	else if(preop == 1)heap(pretype);
	else if(preop == 2)heaparray(pretype, 1);
	else if(preop == 3)arraycpy(prearray);
	else newmp(predst, presrc, pretype);
	return 1;
}


/* Early rejection cannot allocate, publish a partial object or lose an error frame. */
static void
prerejected(void)
{
	raised = nil;
	clonecheck(!kprotect(preinvoke, nil) && raised == exHeap);
	clonecheck(allocations == 0 && live == 0 && worker.nerr == 0);
	clonecheck(atom.ref == 1 && leaf.ref == 1 && Tarray.ref == 1);
	prechecks++;
}


/* Mutable Type descriptors require a complete, naturally aligned native prefix. */
static void
preconstructors(void)
{
	static Type descriptor;
	int op, shift, offset;

	for(op = 0; op < 3; op++){
		for(offset = 0; offset < (int)O(Type, map); offset++){
			setup(); preop = op; pretype = (Type*)((uintptr)-1-offset);
			prerejected();
		}
		for(shift = 1; shift < 8; shift++){
			setup(); preop = op; descriptor = atom;
			memmove(predescriptor.bytes+shift, &descriptor, sizeof(descriptor));
			pretype = (Type*)(predescriptor.bytes+shift);
			prerejected();
		}
	}
}


/* Build a valid empty Array image without allocating a fixture heap owner. */
static void
presource(void)
{
	static Heap header;
	static Array array;

	memset(&header, 0, sizeof(header));
	memset(&array, 0, sizeof(array));

	header.ref = 1; header.t = &Tarray;
	array.t = &leaf; array.root = H; array.data = nil;
	memmove(preowner.bytes, &header, sizeof(header));
	memmove(preowner.bytes+sizeof(Heap), &array, sizeof(array));
	prearray = (Array*)(preowner.bytes+sizeof(Heap)); preop = 3;
}


/* Reject source headers, source Types and root identities before clone allocation. */
static void
presources(void)
{
	static Heap header;
	static Array array;
	int offset, shift;

	for(offset = 0; offset <= (int)sizeof(Heap); offset++){
		setup(); preop = 3; prearray = (Array*)(uintptr)offset; prerejected();
	}
	for(offset = 1; offset < (int)sizeof(Array); offset++){
		setup(); preop = 3; prearray = (Array*)((uintptr)-1-offset); prerejected();
	}
	for(shift = 1; shift < 8; shift++){
		setup(); presource();
		memmove(&header, preowner.bytes, sizeof(header));
		memmove(&array, prearray, sizeof(array));
		memmove(preowner.bytes+shift, &header, sizeof(header));
		memmove(preowner.bytes+shift+sizeof(Heap), &array, sizeof(array));
		prearray = (Array*)(preowner.bytes+shift+sizeof(Heap)); prerejected();
	}
	for(offset = 0; offset < (int)O(Type, map); offset++){
		setup(); presource(); prearray->t = (Type*)((uintptr)-1-offset); prerejected();
	}
	for(offset = 0; offset <= (int)sizeof(Heap); offset++){
		setup(); presource(); prearray->root = (Array*)(uintptr)offset; prerejected();
	}
	for(shift = 1; shift < 8; shift++){
		setup(); presource(); prearray->root = (Array*)(preowner.bytes+sizeof(Heap)+shift); prerejected();
	}
}


/* Read-only packed maps retain exactly the marked child fields at every byte shift. */
static void
premapped(void)
{
	static Type descriptor;
	void *first, *second;
	uchar savedtype[sizeof(Type)+8];
	int shift, sa, da, mask, kind, counts[2], i;
	uchar maps[] = {0, 0x80, 0x40, 0xc0};

	for(shift = 0; shift < 8; shift++)for(sa = 0; sa < 8; sa++)
	for(da = 0; da < 8; da++)for(mask = 0; mask < 4; mask++)for(kind = 0; kind < 3; kind++){
		setup(); descriptor = mp; descriptor.size = 2*sizeof(void*); descriptor.np = 1; descriptor.map[0] = maps[mask];
		memset(predescriptor.bytes, 0xa5, sizeof(predescriptor.bytes));
		memmove(predescriptor.bytes+shift, &descriptor, sizeof(descriptor));
		memmove(savedtype, predescriptor.bytes, sizeof(savedtype));
		pretype = (Type*)(predescriptor.bytes+shift);
		for(i = 0; i < 2; i++){prechildren[i].heap.ref = 10; prechildren[i].heap.t = &atom; prechildren[i].heap.color = 0; counts[i] = 0;}
		first = prechildren[0].data; second = kind == 0 ? H : kind == 1 ? first : prechildren[1].data;
		if(maps[mask]&0x80)counts[0]++;
		if((maps[mask]&0x40) && kind)counts[kind == 1 ? 0 : 1]++;
		memset(preinput, 0x5a, sizeof(preinput)); memset(preoutput, 0xa5, sizeof(preoutput));
		presrc = preinput+sa; predst = preoutput+da; preop = 4;
		memmove(presrc, &first, sizeof(first)); memmove((uchar*)presrc+sizeof(first), &second, sizeof(second));
		memmove(presaved, preinput, sizeof(presaved));
		mutator = 1; nprop = 7;
		clonecheck(kprotect(preinvoke, nil) && !worker.nerr && !allocations && !live);
		clonecheck(!memcmp(predst, presrc, descriptor.size) && !memcmp(preinput, presaved, sizeof(preinput)));
		clonecheck(!memcmp(savedtype, predescriptor.bytes, sizeof(savedtype)));
		for(i = 0; i < da; i++)clonecheck(preoutput[i] == 0xa5);
		for(i = da+descriptor.size; i < (int)sizeof(preoutput); i++)clonecheck(preoutput[i] == 0xa5);
		for(i = 0; i < 2; i++)clonecheck(prechildren[i].heap.ref == (ulong)(10+counts[i]) && prechildren[i].heap.color == (counts[i] ? propagator : 0));
		clonecheck(nprop == (counts[0]+counts[1] ? 1 : 7));
		freeptrs(predst, pretype);
		for(i = 0; i < 2; i++)clonecheck(prechildren[i].heap.ref == 10);
		prechecks++;
	}
}


/* A wrapping mapped record cannot copy bytes or initialize destination owners. */
static void
prespans(void)
{
	int shift, offset, destination;
	static Type descriptor;

	for(shift = 0; shift < 8; shift++)for(destination = 0; destination < 2; destination++)
	for(offset = 0; offset < 2*(int)sizeof(void*); offset++){
		setup(); preop = 4; descriptor = mp; descriptor.size = 2*sizeof(void*); descriptor.np = 0;
		memmove(predescriptor.bytes+shift, &descriptor, sizeof(descriptor)); pretype = (Type*)(predescriptor.bytes+shift);
		memset(preinput, 0x5a, sizeof(preinput)); memset(preoutput, 0xa5, sizeof(preoutput));
		presrc = preinput; predst = preoutput;
		memmove(presaved, preinput, sizeof(presaved));
		memmove(preoutsave, preoutput, sizeof(preoutsave));
		if(destination)predst = (void*)((uintptr)-1-offset);else presrc = (void*)((uintptr)-1-offset);
		prerejected();
		clonecheck(!memcmp(preinput, presaved, sizeof(presaved)) && !memcmp(preoutput, preoutsave, sizeof(preoutsave)));
	}
	for(offset = 0; offset < (int)(O(Type, np)+sizeof(int)); offset++){
		setup(); preop = 4; pretype = (Type*)((uintptr)-1-offset); presrc = preinput; predst = preoutput; prerejected();
	}
}


/* Bad mapped children leave only already-acquired owners in a cleanup-safe result. */
static void
prechildowners(void)
{
	static Type descriptor;
	static void *good, *bad, *field;
	int position, shift, offset;

	for(position = 0; position < 2; position++)for(shift = 0; shift < 8; shift++)
	for(offset = 0; offset <= (int)sizeof(Heap)+7; offset++){
		setup(); descriptor = mp; descriptor.size = 2*sizeof(void*); descriptor.np = 1; descriptor.map[0] = 0xc0;
		memmove(predescriptor.bytes+shift, &descriptor, sizeof(descriptor)); pretype = (Type*)(predescriptor.bytes+shift);
		prechildren[0].heap.ref = 10; prechildren[0].heap.t = &atom; good = prechildren[0].data;
		bad = offset <= (int)sizeof(Heap) ? (void*)(uintptr)offset : (uchar*)good+offset-sizeof(Heap);
		field = position ? good : bad;
		presrc = preinput; predst = preoutput; preop = 4;
		memmove(presrc, &field, sizeof(field)); field = position ? bad : good;
		memmove((uchar*)presrc+sizeof(field), &field, sizeof(field));
		prerejected();
		clonecheck(prechildren[0].heap.ref == (ulong)(10+position));
		memmove(&field, predst, sizeof(field)); clonecheck(field == (position ? good : H));
		memmove(&field, (uchar*)predst+sizeof(field), sizeof(field)); clonecheck(field == H);
		freeptrs(predst, pretype); clonecheck(prechildren[0].heap.ref == 10);
	}
}


/* Run independent allocation, source and mapped-copy families in fresh processes. */
int
main(void)
{
	char *family = getenv("HEAP_PREFLIGHT_CASE");

	if(!strcmp(family,"constructors"))preconstructors();
	else if(!strcmp(family,"sources"))presources();
	else if(!strcmp(family,"mapped"))premapped();
	else if(!strcmp(family,"spans"))prespans();
	else if(!strcmp(family,"children"))prechildowners();
	else abort();
	printf("PASS heap preflight %s: %lu cases WORD%zu\n", family, prechecks, sizeof(WORD)*8);
	return 0;
}
