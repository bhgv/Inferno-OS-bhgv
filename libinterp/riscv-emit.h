/*
 * RV32I/RV64I leaf code for decoded Dis operations.
 * a0 points to REG; numeric leaves use caller-saved t0-t6 without a native stack.
 * Pointer construction uses an ABI-aligned save area around the C allocator call.
 * Emitted bodies use uncompressed instructions without relocations.
 * Guarded RV64 owner leaves materialize trusted GC flag addresses in registers.
 * The separate RV64 complete-tail bridge below embeds trusted executor/fast pointers.
 * Include isa.h first. u32int must be exactly 32 bits, independently of XLEN.
 */
enum { Rvzero=0, Rvra=1, Rvsp=2, Rvt0=5, Rvt1=6, Rvt2=7, Rva0=10, Rva1=11,
	   Rvt3=28, Rvt4=29, Rvt5=30, Rvt6=31, RvMax=384 };

typedef struct RvLayout RvLayout;
struct RvLayout {
	int xlen, wordbytes, hasm;
	int pc, s, d, m;
	int arraylen, arraydata;
	int stringlen, stringdata;
	int listdata, listtail;
	int fp, fixedtmp;
	int arraytype, typesize;
	int hasdiv;	/* Zmmul provides multiplication without division. */
	int framelr, framefp;
	int fixedcoef;
	int module, linkmodule, moduleprog, modulenprog, instbytes;
	int heaptype;
	int heapref, heapcolor, refbytes;
	void *mutator, *nprop;
	int markcolor;
	void *consentry, *ptrtype;
	int listtype, typeref;
};
typedef struct RvBuf RvBuf;
struct RvBuf {
	u32int *out;
	int n, cap, bad;
	RvLayout *abi;
};

static void
rvput(RvBuf *b, u32int v)
{
	if(b->n >= b->cap) {
		b->bad = 1;
		return;
	}
	if(b->out != 0)
		b->out[b->n] = v;
	b->n++;
}

static void
rvi(RvBuf *b, int op, int f3, int rd, int rs, int imm)
{
	rvput(b, ((u32int)imm & 0xfff)<<20 | rs<<15 | f3<<12 | rd<<7 | op);
}

static void
rvr(RvBuf *b, int op, int f3, int f7, int rd, int r1, int r2)
{
	rvput(b, (u32int)f7<<25 | r2<<20 | r1<<15 | f3<<12 | rd<<7 | op);
}

static void
rvs(RvBuf *b, int f3, int ptr, int value, int off)
{
	u32int u;
	u = (u32int)off & 0xfff;
	rvput(b, (u>>5)<<25 | value<<20 | ptr<<15 | f3<<12 | (u&31)<<7 | 0x23);
}

static void
rvbranch(RvBuf *b, int f3, int r1, int r2, int off)
{
	u32int u;
	u = (u32int)off;
	rvput(b, ((u>>12)&1)<<31 | ((u>>5)&63)<<25 | r2<<20 | r1<<15 |
		f3<<12 | ((u>>1)&15)<<8 | ((u>>11)&1)<<7 | 0x63);
}

static void
rvptr(RvBuf *b, int rd, int off)
{
	rvi(b, 0x03, b->abi->xlen == 64 ? 3 : 2, rd, Rva0, off);
}

/* Dis slots may be only four-byte aligned. Do not use LD/SD on values. */
static void
rvload(RvBuf *b, int rd, int ptr, int bytes)
{
	if(bytes == 8) {
		rvi(b, 0x03, 6, rd, ptr, 0);             /* lwu low */
		rvi(b, 0x03, 2, Rvt5, ptr, 4);          /* lw high */
		rvi(b, 0x13, 1, Rvt5, Rvt5, 32);        /* slli */
		rvr(b, 0x33, 6, 0, rd, rd, Rvt5);       /* or */
	}else
		rvi(b, 0x03, bytes == 1 ? 4 : bytes == 2 ? 1 : 2, rd, ptr, 0);
}

static void
rvstore(RvBuf *b, int ptr, int rs, int bytes)
{
	rvs(b, bytes == 1 ? 0 : bytes == 2 ? 1 : 2, ptr, rs, 0);
	if(bytes == 8) {
		rvi(b, 0x13, 5, Rvt5, rs, 32);          /* srli */
		rvs(b, 2, ptr, Rvt5, 4);
	}
}

/* Return zero to the VM unless the relation permits continuing without side effects. */
static void
rvguard(RvBuf *b, int relation, int left, int right)
{
	rvbranch(b, relation, left, right, 12);
	rvi(b, 0x13, 0, Rva0, Rvzero, 0);
	rvi(b, 0x67, 0, Rvzero, Rvra, 0);
}

/* Materialize trusted native addresses without adding a literal-pool alignment rule. */
static void
rvconstant(RvBuf *b, int reg, unsigned long long value)
{
	int i;

	rvi(b, 0x13, 0, reg, Rvzero, (value>>56)&255);
	for(i=6; i>=0; i--){
		rvi(b, 0x13, 1, reg, reg, 8);
		rvi(b, 0x13, 6, reg, reg, (value>>(8*i))&255);
	}
}


/* Patch a private forward branch after its complete guarded region is sized. */
static void
rvpatchbranch(RvBuf *b, int at, int relation, int left, int right)
{
	u32int word;
	RvBuf patch;

	patch.out = &word;
	patch.n = patch.bad = 0;
	patch.cap = 1;
	patch.abi = b->abi;
	rvbranch(&patch, relation, left, right, 4*(b->n-at));
	if(b->out != 0 && at < b->cap)
		b->out[at] = word;
}


/* Require a non-null complete span; t3/t6 are scratch and no VM field changes. */
static void
rvownerspan(RvBuf *b, int reg, int bytes)
{
	rvguard(b, 1, reg, Rvzero);
	rvi(b, 0x13, 0, Rvt6, Rvzero, bytes);
	rvr(b, 0x33, 0, 0, Rvt3, reg, Rvt6);
	rvguard(b, 7, Rvt3, reg);
}


/* Owner operands may be byte-packed. Heap metadata below remains naturally aligned. */
static void
rvownerload(RvBuf *b, int reg, int pointer)
{
	int i;

	rvi(b, 0x03, 4, reg, pointer, 0);
	for(i=1; i<8; i++){
		rvi(b, 0x03, 4, Rvt5, pointer, i);
		rvi(b, 0x13, 1, Rvt5, Rvt5, 8*i);
		rvr(b, 0x33, 6, 0, reg, reg, Rvt5);
	}
}


static void
rvownerstore(RvBuf *b, int pointer, int reg)
{
	int i;

	rvs(b, 0, pointer, reg, 0);
	for(i=1; i<8; i++){
		rvi(b, 0x13, 5, Rvt5, reg, 8*i);
		rvs(b, 0, pointer, Rvt5, i);
	}
}


/* Allocate a pointer-list node through the existing C allocator, then retain and publish
 * its captured head. An allocation error unwinds the native save area through the VM boundary. */
static int
rvpointercons(u32int *out, int cap, RvLayout *abi)
{
	u32int code[RvMax];
	RvBuf b;
	unsigned long long allocator, descriptor, cells[3];
	int i, j, fields[3], empty, marked;

	allocator = (unsigned long long)abi->consentry;
	descriptor = (unsigned long long)abi->ptrtype;
	cells[0] = (unsigned long long)abi->mutator;
	cells[1] = (unsigned long long)abi->nprop;
	if(abi->xlen != 64 || allocator == 0 || allocator == ~0ULL || allocator%2 ||
	   abi->refbytes != 8 || abi->heapref <= 0 || abi->heapref > 128 || abi->heapref%8 ||
	   abi->heapcolor < abi->heapref+4 || abi->heapcolor > 255 || abi->heapcolor%4 ||
	   abi->typeref < 0 || abi->typeref > 2047 || abi->typeref%4 ||
	   descriptor == 0 || descriptor%8 || descriptor > ~0ULL-(abi->typeref+4) ||
	   abi->markcolor < 0 || abi->markcolor > 255)
		return 0;
	cells[2] = descriptor+abi->typeref;
	fields[0] = abi->listtail; fields[1] = abi->listtype; fields[2] = abi->listdata;
	for(i=0; i<3; i++){
		if(fields[i] < 0 || fields[i] > 2047 || fields[i]%8 ||
		   cells[i] == 0 || cells[i]%4 || cells[i] > ~0ULL-4)
			return 0;
		for(j=0; j<i; j++)
			if(fields[i] == fields[j] || cells[i] == cells[j])
				return 0;
	}
	b.out = code; b.n = b.bad = 0; b.cap = RvMax; b.abi = abi;
	rvptr(&b, Rvt0, abi->s);
	rvownerspan(&b, Rvt0, 8);
	rvownerload(&b, Rvt2, Rvt0);
	rvguard(&b, 1, Rvt2, Rvzero);
	rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
	empty = b.n;
	rvbranch(&b, 0, Rvt2, Rvt6, 0);
	rvi(&b, 0x13, 0, Rvt6, Rvzero, abi->heapcolor);
	rvguard(&b, 6, Rvt6, Rvt2);
	rvi(&b, 0x13, 7, Rvt3, Rvt2, 7);
	rvguard(&b, 0, Rvt3, Rvzero);
	rvi(&b, 0x03, 3, Rvt3, Rvt2, -abi->heapref);
	rvguard(&b, 1, Rvt3, Rvzero);
	rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
	rvguard(&b, 1, Rvt3, Rvt6);
	rvpatchbranch(&b, empty, 0, Rvt2, Rvt6);
	rvconstant(&b, Rvt6, descriptor);
	rvi(&b, 0x03, 2, Rvt3, Rvt6, abi->typeref);
	rvguard(&b, 4, Rvzero, Rvt3);
	rvput(&b, 0x7ffff000U | (u32int)Rvt4<<7 | 0x37); /* lui t4, 0x7ffff */
	rvi(&b, 0x13, 0, Rvt4, Rvt4, 2047);
	rvi(&b, 0x13, 0, Rvt4, Rvt4, 2047);
	rvi(&b, 0x13, 0, Rvt4, Rvt4, 1);
	rvguard(&b, 1, Rvt3, Rvt4);
	rvptr(&b, Rvt1, abi->d);
	rvownerspan(&b, Rvt1, 8);
	/* Preserve the head and return address in one ABI-aligned native save area. */
	rvi(&b, 0x13, 0, Rvsp, Rvsp, -16);
	rvs(&b, 3, Rvsp, Rvt2, 0);
	rvs(&b, 3, Rvsp, Rvra, 8);
	rvi(&b, 0x13, 0, Rva1, Rvt1, 0);
	rvi(&b, 0x13, 0, Rva0, Rvzero, 8);
	rvconstant(&b, Rvt6, allocator);
	rvi(&b, 0x67, 0, Rvra, Rvt6, 0);
	rvi(&b, 0x03, 3, Rvt2, Rvsp, 0);
	rvi(&b, 0x03, 3, Rvra, Rvsp, 8);
	rvi(&b, 0x13, 0, Rvsp, Rvsp, 16);
	/* The allocator may collect. Reload descriptor counts and collector colors after it returns. */
	rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
	empty = b.n;
	rvbranch(&b, 0, Rvt2, Rvt6, 0);
	rvi(&b, 0x03, 3, Rvt3, Rvt2, -abi->heapref);
	rvi(&b, 0x13, 0, Rvt3, Rvt3, 1);
	rvs(&b, 3, Rvt2, Rvt3, -abi->heapref);
	rvi(&b, 0x03, 2, Rvt3, Rvt2, -abi->heapcolor);
	rvconstant(&b, Rvt6, cells[0]);
	rvi(&b, 0x03, 2, Rvt4, Rvt6, 0);
	marked = b.n;
	rvbranch(&b, 0, Rvt3, Rvt4, 0);
	rvi(&b, 0x13, 0, Rvt3, Rvzero, abi->markcolor);
	rvs(&b, 2, Rvt2, Rvt3, -abi->heapcolor);
	rvconstant(&b, Rvt6, cells[1]);
	rvi(&b, 0x13, 0, Rvt3, Rvzero, 1);
	rvs(&b, 2, Rvt6, Rvt3, 0);
	rvpatchbranch(&b, marked, 0, Rvt3, Rvt4);
	rvpatchbranch(&b, empty, 0, Rvt2, Rvt6);
	rvconstant(&b, Rvt6, descriptor);
	rvi(&b, 0x03, 2, Rvt3, Rvt6, abi->typeref);
	rvi(&b, 0x13, 0, Rvt3, Rvt3, 1);
	rvs(&b, 2, Rvt6, Rvt3, abi->typeref);
	rvs(&b, 3, Rva0, Rvt6, abi->listtype);
	rvs(&b, 3, Rva0, Rvt2, abi->listdata);
	rvi(&b, 0x13, 0, Rva0, Rvzero, 1);
	rvi(&b, 0x67, 0, Rvzero, Rvra, 0);
	if(b.bad || b.n > cap)
		return 0;
	if(out != 0)
		for(i=0; i<b.n; i++)
			out[i] = code[i];
	return b.n;
}


/* Native owner publication is allowed only for an empty or identical destination.
 * All lifetime guards precede retain/mark/publication; other owners use C destruction. */
static int
rvowner(u32int *out, int cap, RvLayout *abi, int op)
{
	u32int code[RvMax];
	RvBuf b;
	unsigned long long mutator, propagation;
	int offset, nilvalue, empty, same, marked, i, head, self;

	head = op == IHEADP || op == ITAIL;
	self = op == ISELF;
	offset = op == IHEADP ? abi->listdata : abi->listtail;
	mutator = (unsigned long long)abi->mutator;
	propagation = (unsigned long long)abi->nprop;
	if(abi->xlen != 64 || abi->module < 0 || abi->module > 2047 || abi->module%8 ||
	   abi->heapref <= 0 || abi->heapref > 255 || abi->heapref%8 ||
	   abi->heapcolor <= 0 || abi->heapcolor > 255 || abi->heapcolor%4 ||
	   abi->refbytes != 8 || abi->markcolor < 0 || abi->markcolor > 255 ||
	   (abi->heapcolor < abi->heapref+4 && abi->heapref < abi->heapcolor+8) ||
	   mutator == 0 || propagation == 0 || (mutator|propagation)%4 ||
	   mutator > ~0ULL-4 || propagation > ~0ULL-4 || mutator == propagation ||
	   (head && (offset < 0 || offset > 2039)))
		return 0;
	b.out = code;
	b.n = b.bad = 0;
	b.cap = RvMax;
	b.abi = abi;
	rvptr(&b, Rvt1, abi->d);
	rvownerspan(&b, Rvt1, 8);
	if(self)
		rvptr(&b, Rvt2, abi->module);
	else{
		rvptr(&b, Rvt0, abi->s);
		rvownerspan(&b, Rvt0, 8);
		rvownerload(&b, Rvt2, Rvt0);
	}
	if(head){
		rvownerspan(&b, Rvt2, offset+8);
		rvi(&b, 0x13, 0, Rvt0, Rvt2, offset);
		rvownerload(&b, Rvt2, Rvt0);
	}
	rvguard(&b, 1, Rvt2, Rvzero);
	rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
	nilvalue = -1;
	if(self)
		rvguard(&b, 1, Rvt2, Rvt6);
	else{
		nilvalue = b.n;
		rvbranch(&b, 0, Rvt2, Rvt6, 0);
	}
	i = abi->heapref > abi->heapcolor ? abi->heapref : abi->heapcolor;
	rvi(&b, 0x13, 0, Rvt6, Rvzero, i);
	rvguard(&b, 6, Rvt6, Rvt2);
	rvi(&b, 0x13, 7, Rvt3, Rvt2, 7);
	rvguard(&b, 0, Rvt3, Rvzero);
	rvi(&b, 0x03, 3, Rvt3, Rvt2, -abi->heapref);
	rvguard(&b, 1, Rvt3, Rvzero);
	rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
	rvguard(&b, 1, Rvt3, Rvt6);
	if(nilvalue >= 0)
		rvpatchbranch(&b, nilvalue, 0, Rvt2, Rvt6);
	rvownerload(&b, Rvt4, Rvt1);
	rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
	empty = b.n;
	rvbranch(&b, 0, Rvt4, Rvt6, 0);
	rvguard(&b, 0, Rvt4, Rvt2);
	rvpatchbranch(&b, empty, 0, Rvt4, Rvt6);
	nilvalue = b.n;
	rvbranch(&b, 0, Rvt2, Rvt6, 0);
	same = b.n;
	rvbranch(&b, 0, Rvt4, Rvt2, 0);
	rvi(&b, 0x13, 0, Rvt3, Rvt3, 1);
	rvs(&b, 3, Rvt2, Rvt3, -abi->heapref);
	rvpatchbranch(&b, same, 0, Rvt4, Rvt2);
	rvi(&b, 0x03, 2, Rvt3, Rvt2, -abi->heapcolor);
	rvconstant(&b, Rvt6, mutator);
	rvi(&b, 0x03, 2, Rvt4, Rvt6, 0);
	marked = b.n;
	rvbranch(&b, 0, Rvt3, Rvt4, 0);
	rvi(&b, 0x13, 0, Rvt3, Rvzero, abi->markcolor);
	rvs(&b, 2, Rvt2, Rvt3, -abi->heapcolor);
	rvconstant(&b, Rvt6, propagation);
	rvi(&b, 0x13, 0, Rvt3, Rvzero, 1);
	rvs(&b, 2, Rvt6, Rvt3, 0);
	rvpatchbranch(&b, marked, 0, Rvt3, Rvt4);
	rvpatchbranch(&b, nilvalue, 0, Rvt2, Rvt6);
	if(head)
		rvs(&b, 3, Rva0, Rvt0, abi->s);
	rvownerstore(&b, Rvt1, Rvt2);
	rvi(&b, 0x13, 0, Rva0, Rvzero, 1);
	rvi(&b, 0x67, 0, Rvzero, Rvra, 0);
	if(b.bad || b.n > cap)
		return 0;
	if(out != 0)
		for(i=0; i<b.n; i++)
			out[i] = code[i];
	return b.n;
}


/* Turn t3/t4 bits into signed keys using t5/t6, leaving FP state untouched. */
static void
rvrealkey(RvBuf *b, int reg)
{
	rvi(b, 0x13, 1, Rvt5, reg, 1);
	rvbranch(b, 0, Rvt5, Rvzero, 60);
	rvi(b, 0x13, 5, Rvt5, Rvt5, 53);
	rvguard(b, 1, Rvt5, Rvzero);
	rvi(b, 0x13, 0, Rvt6, Rvzero, 2047);
	rvbranch(b, 1, Rvt5, Rvt6, 20);
	rvi(b, 0x13, 1, Rvt5, reg, 12);
	rvguard(b, 0, Rvt5, Rvzero);
	rvi(b, 0x13, 5, Rvt5, reg, 0x400|63);
	rvi(b, 0x13, 5, Rvt5, Rvt5, 1);
	rvr(b, 0x33, 4, 0, reg, reg, Rvt5);
	rvbranch(b, 0, Rvzero, Rvzero, 8);
	rvi(b, 0x13, 0, reg, Rvzero, 0);
}

/* Multiply modulo XLEN without M, using only dead operand-pointer registers.
 * t3 and right hold captured operands; t0-t2 and t5 are scratch. Logical shifts
 * terminate after at most the operand width in bits, including negative values. */
static void
rvmultiply(RvBuf *b, int right, int bytes)
{
	rvi(b, 0x13, 0, Rvt0, Rvt3, 0);
	rvi(b, 0x13, 0, Rvt1, right, 0);
	if(bytes == 4){
		rvi(b, 0x13, 1, Rvt1, Rvt1, 32);
		rvi(b, 0x13, 5, Rvt1, Rvt1, 32);
	}
	rvi(b, 0x13, 0, Rvt2, Rvzero, 0);
	rvbranch(b, 0, Rvt1, Rvzero, 28);
	rvi(b, 0x13, 7, Rvt5, Rvt1, 1);
	rvbranch(b, 0, Rvt5, Rvzero, 8);
	rvr(b, 0x33, 0, 0, Rvt2, Rvt2, Rvt0);
	rvi(b, 0x13, 5, Rvt1, Rvt1, 1);
	rvi(b, 0x13, 1, Rvt0, Rvt0, 1);
	rvbranch(b, 1, Rvt1, Rvzero, -20);
	rvi(b, 0x13, 0, Rvt3, Rvt2, 0);
}

/* Scale a bounded program index using RV64I when multiplication is absent.
 * The constant multiplier is at most 2047, so the loop takes at most eleven
 * iterations. t1 is dead after the source load; t0 must retain the Module. */
static void
rvprogramscale(RvBuf *b, int stride)
{
	if(b->abi->hasm){
		rvi(b, 0x13, 0, Rvt6, Rvzero, stride);
		rvr(b, 0x33, 0, 1, Rvt4, Rvt4, Rvt6);
		return;
	}
	rvi(b, 0x13, 0, Rvt5, Rvzero, 0);
	rvi(b, 0x13, 0, Rvt6, Rvzero, stride);
	rvi(b, 0x13, 7, Rvt1, Rvt6, 1);
	rvbranch(b, 0, Rvt1, Rvzero, 8);
	rvr(b, 0x33, 0, 0, Rvt5, Rvt5, Rvt4);
	rvi(b, 0x13, 5, Rvt6, Rvt6, 1);
	rvbranch(b, 0, Rvt6, Rvzero, 12);
	rvi(b, 0x13, 1, Rvt4, Rvt4, 1);
	rvbranch(b, 0, Rvzero, Rvzero, -24);
	rvi(b, 0x13, 0, Rvt4, Rvt5, 0);
}


/* Return instruction count, or 0 for unsupported/invalid/overflow. out==0
 * performs the same bounded sizing pass without touching memory. */
static int
rvemit(u32int *out, int cap, RvLayout *abi, int op)
{
	RvBuf b;
	int bytes, f3, f7, wide, rs, binary, branch, swap, invert, shift, division, scale, realbranch;
	int coefconvert, coefdivide;

	if((abi->xlen != 32 && abi->xlen != 64) ||
	   (abi->wordbytes != 4 && abi->wordbytes != 8) ||
	   abi->wordbytes*8 > abi->xlen || cap <= 0 ||
	   abi->pc < 0 || abi->s < 0 || abi->d < 0 || abi->m < 0 ||
	   abi->pc > 2047 || abi->s > 2047 || abi->d > 2047 || abi->m > 2047 ||
	   (abi->pc|abi->s|abi->d|abi->m) % (abi->xlen/8) != 0)
		return 0;
	if(op == ICONSP)
		return rvpointercons(out, cap, abi);
	if(op == IMOVP || op == ISELF || op == IHEADP || op == ITAIL)
		return rvowner(out, cap, abi, op);
	b.out=out; b.n=0; b.cap=cap; b.bad=0; b.abi=abi;
	bytes=abi->wordbytes; f3=0; f7=0; binary=0; branch=0; swap=0; invert=0; shift=0; division=0; realbranch=0;
	switch(op) {
	case ITCMP:
		/* Both owner slots are native pointers; headers have natural alignment. */
		if(abi->xlen != 64 || abi->heaptype <= 0 || abi->heaptype > 255 || abi->heaptype%8) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt3, Rvt1, 8);
		rvptr(&b, Rvt1, abi->d); rvload(&b, Rvt4, Rvt1, 8);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
		rvbranch(&b, 0, Rvt3, Rvt6, 60);
		rvguard(&b, 1, Rvt3, Rvzero); rvguard(&b, 1, Rvt4, Rvzero);
		rvguard(&b, 1, Rvt4, Rvt6);
		rvi(&b, 0x03, 3, Rvt3, Rvt3, -abi->heaptype);
		rvi(&b, 0x03, 3, Rvt4, Rvt4, -abi->heaptype);
		rvguard(&b, 0, Rvt3, Rvt4);
		break;

	case IMOVPC:
		/* The bounded int-sized program length keeps the stride product in 64 bits. */
		if(abi->xlen != 64) return 0;
		if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
		   abi->linkmodule < 0 || abi->linkmodule > 2047 || abi->linkmodule%8 ||
		   abi->moduleprog < 0 || abi->moduleprog > 2047 || abi->moduleprog%8 ||
		   abi->modulenprog < 0 || abi->modulenprog > 2047 || abi->modulenprog%4 ||
		   abi->instbytes < 1 || abi->instbytes > 2047) return 0;
		rvptr(&b, Rvt0, abi->module); rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
		rvi(&b, 0x03, 3, Rvt0, Rvt0, abi->linkmodule); rvguard(&b, 1, Rvt0, Rvzero); rvguard(&b, 1, Rvt0, Rvt6);
		rvi(&b, 0x03, 2, Rvt3, Rvt0, abi->modulenprog); rvguard(&b, 5, Rvt3, Rvzero);
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt4, Rvt1, abi->wordbytes); rvguard(&b, 6, Rvt4, Rvt3);
		rvprogramscale(&b, abi->instbytes);
		rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->moduleprog); rvguard(&b, 1, Rvt3, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt3, Rvt6);
		rvr(&b, 0x33, 0, 0, Rvt4, Rvt4, Rvt3); rvguard(&b, 7, Rvt4, Rvt3);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt4, 8);
		break;

	case IMULX1: case IDIVX1: case ICVTXX1:
	case IMULX0: case IDIVX0: case ICVTXX0:
		/* Capture frame parameters before source/destination aliases are changed. */
		coefconvert = op == ICVTXX0 || op == ICVTXX1;
		coefdivide = op == IDIVX0 || op == IDIVX1;
		if(abi->xlen != 64 || !abi->hasdiv || (!coefconvert && !abi->hasm) ||
		   abi->fp < 0 || abi->fp > 2047 || abi->fp%8 || abi->fixedtmp < 0 || abi->fixedtmp > 2047 || abi->fixedtmp%4 ||
		   abi->fixedcoef < 0 || abi->fixedcoef > 2047 || abi->fixedcoef%4) return 0;
		rvptr(&b, Rvt1, abi->fp); rvguard(&b, 1, Rvt1, Rvzero);
		rvi(&b, 0x13, 0, Rvt5, Rvzero, -1); rvguard(&b, 1, Rvt1, Rvt5);
		rvi(&b, 0x13, 7, Rvt5, Rvt1, 3); rvguard(&b, 0, Rvt5, Rvzero);
		rvi(&b, 0x13, 0, Rvt0, Rvt1, abi->fixedcoef); rvload(&b, Rvt6, Rvt0, abi->wordbytes);
		if(!coefdivide){
			rvguard(&b, 1, Rvt6, Rvzero);
			rvi(&b, 0x13, 0, Rvt5, Rvzero, -1);
			rvguard(&b, 1, Rvt6, Rvt5);
		}
		if(coefconvert)rvptr(&b, Rvt1, abi->m);else rvi(&b, 0x13, 0, Rvt1, Rvt1, abi->fixedtmp);
		rvload(&b, Rvt4, Rvt1, abi->wordbytes);
		if(op == IMULX1 || op == IDIVX1 || op == ICVTXX1){
			/* The low two bits encode rounding modes, not part of the scale. */
			rvi(&b, 0x13, 7, Rvt0, Rvt4, 3); rvguard(&b, 0, Rvt0, Rvzero);
			rvi(&b, 0x13, 5, Rvt4, Rvt4, 0x400|2);
		}
		rvi(&b, 0x13, 0, Rvt0, Rvt4, 63); rvi(&b, 0x13, 0, Rvt5, Rvzero, 127);rvguard(&b, 6, Rvt0, Rvt5);
		if(!coefconvert){
			rvptr(&b, Rvt1, abi->s);rvload(&b, Rvt0, Rvt1, abi->wordbytes);
			if(coefdivide){
				rvguard(&b, 1, Rvt0, Rvzero);
				rvi(&b, 0x13, 0, Rvt5, Rvzero, -1);
				rvguard(&b, 1, Rvt0, Rvt5);
			}
		}
		rvptr(&b, Rvt1, coefconvert ? abi->s : abi->m);rvload(&b, Rvt3, Rvt1, abi->wordbytes);
		if(!coefconvert)rvr(&b, 0x33, 0, 1, Rvt3, Rvt3, coefdivide ? Rvt6 : Rvt0);
		rvbranch(&b, 4, Rvt4, Rvzero, 12);rvr(&b, 0x33, 1, 0, Rvt3, Rvt3, Rvt4);rvbranch(&b, 0, Rvzero, Rvzero, 12);
		rvr(&b, 0x33, 0, 32, Rvt4, Rvzero, Rvt4);rvr(&b, 0x33, 5, 32, Rvt3, Rvt3, Rvt4);
		rvr(&b, 0x33, 4, 1, Rvt3, Rvt3, coefdivide ? Rvt0 : Rvt6);
		rvptr(&b, Rvt2, abi->d);rvstore(&b, Rvt2, Rvt3, abi->wordbytes);
		break;
	case ICALL:
		/* Four-byte-aligned VM frames use paired stores for native links. */
		if(abi->xlen != 64 || abi->fp < 0 || abi->fp > 2047 || abi->fp%8 ||
		   abi->framelr < 0 || abi->framelr > 2047 || abi->framelr%8 ||
		   abi->framefp < 0 || abi->framefp > 2047 || abi->framefp%8 ||
		   abi->framelr == abi->framefp) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8); rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
		rvi(&b, 0x13, 7, Rvt5, Rvt0, 3); rvguard(&b, 0, Rvt5, Rvzero);
		rvptr(&b, Rvt1, abi->d); rvload(&b, Rvt3, Rvt1, 8);
		rvptr(&b, Rvt4, abi->pc); rvptr(&b, Rvt6, abi->fp);
		rvi(&b, 0x13, 0, Rvt1, Rvt0, abi->framelr); rvstore(&b, Rvt1, Rvt4, 8);
		rvi(&b, 0x13, 0, Rvt1, Rvt0, abi->framefp); rvstore(&b, Rvt1, Rvt6, 8);
		rvs(&b, 3, Rva0, Rvt0, abi->fp); rvs(&b, 3, Rva0, Rvt3, abi->pc);
		break;
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		if(abi->xlen != 64) return 0;
		bytes=8; branch=1; realbranch=1;
		f3=op == IBEQF ? 0 : op == IBNEF ? 1 : (op == IBLTF || op == IBGTF) ? 4 : 5;
		swap=op == IBLEF || op == IBGTF;
		break;
	case IINDX:
		/* A bounded positive descriptor product fits 62 bits before the final bound check. */
		if(abi->xlen != 64 || !abi->hasm ||
		   abi->arraylen < 0 || abi->arraylen > 2047 || abi->arraydata < 0 || abi->arraydata > 2047 ||
		   abi->arraytype < 0 || abi->arraytype > 2047 || abi->typesize < 0 || abi->typesize > 2047 ||
		   (abi->arraylen|abi->arraydata|abi->arraytype)%8 || abi->typesize%4) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8); rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
		rvi(&b, 0x03, abi->wordbytes == 8 ? 3 : 2, Rvt3, Rvt0, abi->arraylen); rvguard(&b, 5, Rvt3, Rvzero);
		rvptr(&b, Rvt1, abi->d); rvload(&b, Rvt4, Rvt1, abi->wordbytes); rvguard(&b, 6, Rvt4, Rvt3);
		rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->arraytype); rvguard(&b, 1, Rvt3, Rvzero); rvguard(&b, 1, Rvt3, Rvt6);
		rvi(&b, 0x03, 2, Rvt6, Rvt3, abi->typesize); rvguard(&b, 5, Rvt6, Rvzero);
		rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->arraydata);
		rvi(&b, 0x13, 0, Rvt5, Rvt3, 0);
		rvbranch(&b, 0, Rvt6, Rvzero, 84);
		rvi(&b, 0x13, 0, Rvt0, Rvzero, -1); rvi(&b, 0x13, 5, Rvt0, Rvt0, 33);
		rvguard(&b, 7, Rvt0, Rvt4); rvr(&b, 0x33, 0, 1, Rvt4, Rvt4, Rvt6); rvguard(&b, 7, Rvt0, Rvt4);
		rvguard(&b, 1, Rvt3, Rvzero); rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt3, Rvt6);
		rvr(&b, 0x33, 0, 0, Rvt5, Rvt3, Rvt4); rvguard(&b, 7, Rvt5, Rvt3);
		rvptr(&b, Rvt2, abi->m); rvstore(&b, Rvt2, Rvt5, abi->wordbytes);
		break;
	case IMULX: case IDIVX:
		/* Preserve the signed 64-bit intermediate; only DIVX needs hardware division. */
		if(abi->xlen != 64 || (op == IDIVX && !abi->hasdiv) || abi->fp < 0 || abi->fp > 2047 || abi->fp%8 ||
		   abi->fixedtmp < 0 || abi->fixedtmp > 2047 || abi->fixedtmp%4) return 0;
		rvptr(&b, Rvt1, abi->fp); rvguard(&b, 1, Rvt1, Rvzero);
		rvi(&b, 0x13, 0, Rvt5, Rvzero, -1); rvguard(&b, 1, Rvt1, Rvt5);
		rvi(&b, 0x13, 7, Rvt5, Rvt1, 3); rvguard(&b, 0, Rvt5, Rvzero);
		rvi(&b, 0x13, 0, Rvt1, Rvt1, abi->fixedtmp); rvload(&b, Rvt4, Rvt1, abi->wordbytes);
		rvi(&b, 0x13, 0, Rvt6, Rvt4, 63); rvi(&b, 0x13, 0, Rvt5, Rvzero, 127); rvguard(&b, 6, Rvt6, Rvt5);
		rvptr(&b, Rvt0, abi->s); rvload(&b, Rvt6, Rvt0, abi->wordbytes);
		if(op == IDIVX){
			rvguard(&b, 1, Rvt6, Rvzero); rvi(&b, 0x13, 0, Rvt5, Rvzero, -1); rvguard(&b, 1, Rvt6, Rvt5);
		}
		rvptr(&b, Rvt0, abi->m); rvload(&b, Rvt3, Rvt0, abi->wordbytes);
		if(op == IMULX){
			if(abi->hasm)rvr(&b, 0x33, 0, 1, Rvt3, Rvt3, Rvt6);
			else rvmultiply(&b, Rvt6, 8);
		}
		rvbranch(&b, 4, Rvt4, Rvzero, 12);
		rvr(&b, 0x33, 1, 0, Rvt3, Rvt3, Rvt4); rvbranch(&b, 0, Rvzero, Rvzero, 12);
		rvr(&b, 0x33, 0, 32, Rvt4, Rvzero, Rvt4); rvr(&b, 0x33, 5, 32, Rvt3, Rvt3, Rvt4);
		if(op == IDIVX) rvr(&b, 0x33, 4, 1, Rvt3, Rvt3, Rvt6);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, abi->wordbytes);
		break;
	case ICVTRF: case ICVTFR:
		/* Exact resizing uses only RV64I; operands retain four-byte slot alignment. */
		if(abi->xlen != 64) return 0;
		rvptr(&b, Rvt0, abi->s); rvload(&b, Rvt3, Rvt0, op == ICVTRF ? 4 : 8);
		if(op == ICVTRF){ rvi(&b, 0x13, 1, Rvt3, Rvt3, 32); rvi(&b, 0x13, 5, Rvt3, Rvt3, 32); }
		rvi(&b, 0x13, 5, Rvt6, Rvt3, op == ICVTRF ? 31 : 63);
		rvi(&b, 0x13, 1, Rvt6, Rvt6, op == ICVTRF ? 63 : 31);
		rvi(&b, 0x13, 1, Rvt3, Rvt3, op == ICVTRF ? 33 : 1);
		rvi(&b, 0x13, 5, Rvt3, Rvt3, op == ICVTRF ? 33 : 1);
		/* Zero skips all classification and joins the common sign publication. */
		rvbranch(&b, 0, Rvt3, Rvzero, op == ICVTRF ? 52 : 64);
		rvi(&b, 0x13, 5, Rvt4, Rvt3, op == ICVTRF ? 23 : 52);
		if(op == ICVTRF){
			rvguard(&b, 1, Rvt4, Rvzero);
			rvi(&b, 0x13, 0, Rvt5, Rvzero, 255); rvguard(&b, 6, Rvt4, Rvt5);
			rvi(&b, 0x13, 1, Rvt3, Rvt3, 29);
			rvi(&b, 0x13, 0, Rvt4, Rvzero, 896); rvi(&b, 0x13, 1, Rvt4, Rvt4, 52);
			rvr(&b, 0x33, 0, 0, Rvt3, Rvt3, Rvt4);
		}else{
			rvi(&b, 0x13, 0, Rvt4, Rvt4, -897);
			rvi(&b, 0x13, 0, Rvt5, Rvzero, 254); rvguard(&b, 6, Rvt4, Rvt5);
			rvi(&b, 0x13, 1, Rvt5, Rvt3, 35); rvguard(&b, 0, Rvt5, Rvzero);
			rvi(&b, 0x13, 0, Rvt4, Rvt4, 1); rvi(&b, 0x13, 1, Rvt4, Rvt4, 23);
			rvi(&b, 0x13, 1, Rvt3, Rvt3, 12); rvi(&b, 0x13, 5, Rvt3, Rvt3, 41);
			rvr(&b, 0x33, 6, 0, Rvt3, Rvt3, Rvt4);
		}
		rvr(&b, 0x33, 6, 0, Rvt3, Rvt3, Rvt6);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, op == ICVTRF ? 8 : 4);
		break;
	case ICVTWF: case ICVTLF:
		/* RV64I constructs exact binary64 bits without M or floating instructions. */
		if(abi->xlen != 64) return 0;
		rvptr(&b, Rvt0, abi->s); rvload(&b, Rvt3, Rvt0, op == ICVTWF ? abi->wordbytes : 8);
		rvi(&b, 0x13, 5, Rvt6, Rvt3, 0x400|63);
		rvr(&b, 0x33, 4, 0, Rvt3, Rvt3, Rvt6);
		rvr(&b, 0x33, 0, 32, Rvt3, Rvt3, Rvt6);
		rvi(&b, 0x13, 1, Rvt6, Rvt6, 63);
		rvi(&b, 0x13, 5, Rvt5, Rvt3, 53); rvguard(&b, 0, Rvt5, Rvzero);
		rvbranch(&b, 0, Rvt3, Rvzero, 40);
		rvi(&b, 0x13, 0, Rvt4, Rvzero, 1074);
		rvi(&b, 0x13, 5, Rvt5, Rvt3, 52);
		rvbranch(&b, 1, Rvt5, Rvzero, 16);
		rvi(&b, 0x13, 1, Rvt3, Rvt3, 1);
		rvi(&b, 0x13, 0, Rvt4, Rvt4, -1);
		rvbranch(&b, 0, Rvzero, Rvzero, -16);
		rvi(&b, 0x13, 1, Rvt4, Rvt4, 52);
		rvr(&b, 0x33, 0, 0, Rvt3, Rvt3, Rvt4);
		rvr(&b, 0x33, 6, 0, Rvt3, Rvt3, Rvt6);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, 8);
		break;
	case ICVTXX:
		/* RV64I suffices; RV32 keeps the complete fixed intermediate in C. */
		if(abi->xlen != 64) return 0;
		rvptr(&b, Rvt1, abi->m); rvload(&b, Rvt4, Rvt1, abi->wordbytes);
		rvi(&b, 0x13, 0, Rvt6, Rvt4, 63);
		rvi(&b, 0x13, 0, Rvt5, Rvzero, 127); rvguard(&b, 6, Rvt6, Rvt5);
		rvptr(&b, Rvt0, abi->s); rvload(&b, Rvt3, Rvt0, abi->wordbytes);
		rvbranch(&b, 4, Rvt4, Rvzero, 12);
		rvr(&b, 0x33, 1, 0, Rvt3, Rvt3, Rvt4);
		rvbranch(&b, 0, Rvzero, Rvzero, 12);
		rvr(&b, 0x33, 0, 32, Rvt4, Rvzero, Rvt4);
		rvr(&b, 0x33, 5, 32, Rvt3, Rvt3, Rvt4);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, abi->wordbytes);
		break;
	case IEXPW: case IEXPL:
		if(abi->xlen != 64 || !abi->hasm) return 0;
		rvptr(&b, Rvt0, abi->s); rvload(&b, Rvt4, Rvt0, abi->wordbytes);
		bytes = op == IEXPL ? 8 : abi->wordbytes;
		rvptr(&b, Rvt1, abi->m); rvload(&b, Rvt3, Rvt1, bytes);
		/* No division or signed exponent negation is needed on the negative path. */
		rvbranch(&b, 5, Rvt4, Rvzero, bytes == 8 ? 80 : 72);
		rvguard(&b, 1, Rvt3, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, 1); rvi(&b, 0x13, 0, Rvt5, Rvzero, 1);
		rvbranch(&b, 0, Rvt3, Rvt5, 32);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, 0); rvi(&b, 0x13, 0, Rvt5, Rvzero, -1);
		rvbranch(&b, 1, Rvt3, Rvt5, 20);
		rvi(&b, 0x13, 7, Rvt6, Rvt4, 1); rvi(&b, 0x13, 1, Rvt6, Rvt6, 1);
		rvr(&b, 0x33, 0, 32, Rvt6, Rvzero, Rvt6); rvi(&b, 0x13, 0, Rvt6, Rvt6, 1);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt6, bytes);
		rvi(&b, 0x13, 0, Rva0, Rvzero, 1); rvi(&b, 0x67, 0, Rvzero, Rvra, 0);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, 1);
		rvi(&b, 0x13, 7, Rvt5, Rvt4, 1);
		rvbranch(&b, 0, Rvt5, Rvzero, 8);
		rvr(&b, 0x33, 0, 1, Rvt6, Rvt6, Rvt3);
		rvi(&b, 0x13, 5, Rvt4, Rvt4, 1);
		rvbranch(&b, 0, Rvt4, Rvzero, 12);
		rvr(&b, 0x33, 0, 1, Rvt3, Rvt3, Rvt3);
		rvbranch(&b, 0, Rvzero, Rvzero, -24);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt6, bytes);
		break;
	case IINDC:
		if(abi->xlen != 64 || abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 != 0 ||
		   abi->stringdata < 0 || abi->stringdata > 2047 || abi->stringdata%4 != 0 ||
		   abi->stringdata < abi->stringlen+4) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
		rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
		/* Reserve room for every int-sized Rune index and inline field offset.
		 * Decline before metadata access; C retains the precise error contract. */
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
		rvi(&b, 0x13, 1, Rvt6, Rvt6, 34);
		rvguard(&b, 6, Rvt0, Rvt6);
		rvi(&b, 0x03, 2, Rvt3, Rvt0, abi->stringlen);
		rvput(&b, 0x80000000U | Rvt6<<7 | 0x37); rvguard(&b, 1, Rvt3, Rvt6);
		rvi(&b, 0x13, 0, Rvt4, Rvt3, 0);
		rvbranch(&b, 5, Rvt3, Rvzero, 8); rvr(&b, 0x33, 0, 32, Rvt3, Rvzero, Rvt3);
		rvptr(&b, Rvt1, abi->m); rvload(&b, Rvt6, Rvt1, abi->wordbytes);
		rvguard(&b, 6, Rvt6, Rvt3);
		rvbranch(&b, 5, Rvt4, Rvzero, 20);
		rvi(&b, 0x13, 1, Rvt6, Rvt6, 2);
		rvr(&b, 0x33, 0, 0, Rvt0, Rvt0, Rvt6);
		rvi(&b, 0x03, 2, Rvt3, Rvt0, abi->stringdata);
		rvbranch(&b, 0, Rvzero, Rvzero, 12);
		rvr(&b, 0x33, 0, 0, Rvt0, Rvt0, Rvt6);
		rvi(&b, 0x03, 4, Rvt3, Rvt0, abi->stringdata);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, abi->wordbytes);
		break;
	case ILENL:
		/* Brent checkpoints reject cycles without allocating or publishing partial counts. */
		if(abi->xlen != 64 || abi->listtail < 0 || abi->listtail > 2047 || abi->listtail%8 != 0) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
		rvi(&b, 0x13, 0, Rvt3, Rvzero, 0);
		rvi(&b, 0x13, 0, Rvt4, Rvzero, -1);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, 256);
		rvi(&b, 0x13, 0, Rvt2, Rvt0, 0);
		rvbranch(&b, 0, Rvt0, Rvt4, 68);
		rvguard(&b, 1, Rvt0, Rvzero);
		rvguard(&b, 6, Rvt3, Rvt6);
		rvi(&b, 0x03, 3, Rvt0, Rvt0, abi->listtail);
		rvi(&b, 0x13, 0, Rvt3, Rvt3, 1);
		rvguard(&b, 1, Rvt0, Rvt2);
		rvi(&b, 0x13, 0, Rvt1, Rvt3, 1);
		rvr(&b, 0x33, 7, 0, Rvt1, Rvt1, Rvt3);
		rvbranch(&b, 1, Rvt1, Rvzero, 8);
		rvi(&b, 0x13, 0, Rvt2, Rvt0, 0);
		rvbranch(&b, 0, Rvzero, Rvzero, -64);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, abi->wordbytes);
		break;
	case IHEADB: case IHEADW: case IHEADL: case IHEADF:
		if(abi->xlen != 64 || abi->listdata < 0 || abi->listdata > 2047 || abi->listdata%8 != 0) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
		rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
		/* Validate both additions before reading a scalar list payload. */
		rvi(&b, 0x13, 0, Rvt1, Rvt0, abi->listdata);
		rvguard(&b, 7, Rvt1, Rvt0);
		bytes = op == IHEADB ? 1 : op == IHEADW ? abi->wordbytes : 8;
		rvi(&b, 0x13, 0, Rvt6, Rvt1, bytes);
		rvguard(&b, 7, Rvt6, Rvt1);
		rvload(&b, Rvt3, Rvt1, bytes);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, bytes);
		break;
	case ILENC:
		if(abi->xlen != 64 || abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 != 0) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
		rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
		rvbranch(&b, 0, Rvt0, Rvt6, 56);
		/* Reserve room for every int-sized Rune index and inline field offset.
		 * Decline before metadata access; C retains the precise error contract. */
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
		rvi(&b, 0x13, 1, Rvt6, Rvt6, 34);
		rvguard(&b, 6, Rvt0, Rvt6);
		rvi(&b, 0x03, 2, Rvt3, Rvt0, abi->stringlen);
		rvput(&b, 0x80000000U | Rvt6<<7 | 0x37);
		rvguard(&b, 1, Rvt3, Rvt6);
		rvbranch(&b, 5, Rvt3, Rvzero, 8);
		rvr(&b, 0x33, 0, 32, Rvt3, Rvzero, Rvt3);
		rvbranch(&b, 0, Rvzero, Rvzero, 8);
		rvi(&b, 0x13, 0, Rvt3, Rvzero, 0);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, abi->wordbytes);
		break;
	case IHEADM:
	case IMOVM:
		if(abi->xlen != 64) return 0;
		if(op == IHEADM){
			if(abi->listdata < 0 || abi->listdata > 2047) return 0;
			rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
			rvguard(&b, 1, Rvt0, Rvzero);
			rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
		}
		/* Keep the length in a register even if the copy overwrites its slot. */
		rvptr(&b, Rvt1, abi->m); rvload(&b, Rvt3, Rvt1, abi->wordbytes);
		rvguard(&b, 5, Rvt3, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, 256);
		rvguard(&b, 7, Rvt6, Rvt3);
		/* Skip both span checks and all copy loops for a valid empty record. */
		rvbranch(&b, 0, Rvt3, Rvzero, op == IHEADM ? 148 : 132);
		if(op == IHEADM){
			/* Preserve the owner until the unsigned payload-offset sum is validated. */
			rvi(&b, 0x13, 0, Rvt4, Rvt0, abi->listdata);
			rvguard(&b, 7, Rvt4, Rvt0);
			rvi(&b, 0x13, 0, Rvt0, Rvt4, 0);
		}else
			rvptr(&b, Rvt0, abi->s);
		rvptr(&b, Rvt2, abi->d);
		/* Nonempty starts must be nonnil; unsigned end sums must not carry. */
		rvguard(&b, 1, Rvt0, Rvzero); rvguard(&b, 1, Rvt2, Rvzero);
		rvr(&b, 0x33, 0, 0, Rvt4, Rvt0, Rvt3); rvguard(&b, 7, Rvt4, Rvt0);
		rvr(&b, 0x33, 0, 0, Rvt4, Rvt2, Rvt3); rvguard(&b, 7, Rvt4, Rvt2);
		rvbranch(&b, 7, Rvt0, Rvt2, 40); /* source >= destination: forward */
		rvr(&b, 0x33, 0, 0, Rvt0, Rvt0, Rvt3);
		rvr(&b, 0x33, 0, 0, Rvt2, Rvt2, Rvt3);
		rvi(&b, 0x13, 0, Rvt0, Rvt0, -1);
		rvi(&b, 0x13, 0, Rvt2, Rvt2, -1);
		rvi(&b, 0x03, 4, Rvt4, Rvt0, 0); rvs(&b, 0, Rvt2, Rvt4, 0);
		rvi(&b, 0x13, 0, Rvt3, Rvt3, -1);
		rvbranch(&b, 1, Rvt3, Rvzero, -20);
		rvbranch(&b, 0, Rvzero, Rvzero, 28);
		rvi(&b, 0x03, 4, Rvt4, Rvt0, 0); rvs(&b, 0, Rvt2, Rvt4, 0);
		rvi(&b, 0x13, 0, Rvt0, Rvt0, 1);
		rvi(&b, 0x13, 0, Rvt2, Rvt2, 1);
		rvi(&b, 0x13, 0, Rvt3, Rvt3, -1);
		rvbranch(&b, 1, Rvt3, Rvzero, -20);
		break;
	case ILENA:
	case IINDB: case IINDW: case IINDL: case IINDF:
		if(abi->xlen != 64 ||
		   abi->arraylen < 0 || abi->arraydata < 0 || abi->arraylen > 2047 ||
		   abi->arraydata > 2047 || (abi->arraylen|abi->arraydata)%8 != 0) return 0;
		/* Dis operand slots can be four-byte aligned even for native pointers. */
		rvptr(&b, Rvt1, abi->s);
		rvload(&b, Rvt0, Rvt1, 8);
		if(op == ILENA){
			rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
			rvbranch(&b, 0, Rvt0, Rvzero, 16);
			rvbranch(&b, 0, Rvt0, Rvt6, 12);
			rvi(&b, 0x03, abi->wordbytes == 8 ? 3 : 2, Rvt3, Rvt0, abi->arraylen);
			rvput(&b, 0x0080006f); /* jal zero, done */
			rvi(&b, 0x13, 0, Rvt3, Rvzero, 0);
			rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, abi->wordbytes);
		}else{
			rvguard(&b, 1, Rvt0, Rvzero);
			rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
			rvi(&b, 0x03, abi->wordbytes == 8 ? 3 : 2, Rvt3, Rvt0, abi->arraylen);
			rvguard(&b, 5, Rvt3, Rvzero);
			rvptr(&b, Rvt1, abi->d); rvload(&b, Rvt4, Rvt1, abi->wordbytes);
			rvguard(&b, 6, Rvt4, Rvt3);
			scale = op == IINDB ? 0 : op == IINDW && abi->wordbytes == 4 ? 2 : 3;
			rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
			rvi(&b, 0x13, 5, Rvt6, Rvt6, 33+scale);
			rvguard(&b, 7, Rvt6, Rvt4);
			rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->arraydata);
			rvguard(&b, 1, Rvt3, Rvzero);
			rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt3, Rvt6);
			if(scale) rvi(&b, 0x13, 1, Rvt4, Rvt4, scale);
			rvr(&b, 0x33, 0, 0, Rvt5, Rvt3, Rvt4);
			rvguard(&b, 7, Rvt5, Rvt3);
			rvptr(&b, Rvt2, abi->m); rvstore(&b, Rvt2, Rvt5, abi->wordbytes);
		}
		break;
	case IRUNT:
		break;
	case INEGF:
	case IMOVF:
	case IMOVL:
		/* Capture both halves before storing, including aliasing operands. */
		rvptr(&b, Rvt0, abi->s);
		rvptr(&b, Rvt2, abi->d);
		rvi(&b, 0x03, 2, Rvt3, Rvt0, 0);
		rvi(&b, 0x03, 2, Rvt4, Rvt0, 4);
		/* Toggle the high word sign bit without requiring floating-point state. */
		if(op == INEGF){
			rvput(&b, 0x80000000U | Rvt5<<7 | 0x37);
			rvr(&b, 0x33, 4, 0, Rvt4, Rvt4, Rvt5);
		}
		rvs(&b, 2, Rvt2, Rvt3, 0);
		rvs(&b, 2, Rvt2, Rvt4, 4);
		break;
	case IMOVB: bytes=1; /* fall through */
	case IMOVW:
		rvptr(&b, Rvt0, abi->s);
		rvptr(&b, Rvt2, abi->d);
		rvload(&b, Rvt3, Rvt0, bytes);
		rvstore(&b, Rvt2, Rvt3, bytes);
		break;
	case ICVTBW:
	case ICVTWB:
	case ICVTWS:
	case ICVTSW:
		rvptr(&b, Rvt0, abi->s);
		rvptr(&b, Rvt2, abi->d);
		rvload(&b, Rvt3, Rvt0, op == ICVTBW ? 1 : op == ICVTSW ? 2 : bytes);
		rvstore(&b, Rvt2, Rvt3, op == ICVTWB ? 1 : op == ICVTWS ? 2 : bytes);
		break;
	case ICVTWL:
	case ICVTLW:
		if(abi->xlen != 64) return 0;
		rvptr(&b, Rvt0, abi->s); rvptr(&b, Rvt2, abi->d);
		rvload(&b, Rvt3, Rvt0, op == ICVTWL ? abi->wordbytes : 8);
		rvstore(&b, Rvt2, Rvt3, op == ICVTWL ? 8 : abi->wordbytes);
		break;
	case ILEA:
		/* Truncate to WORD exactly as xec.c does, without widening the slot. */
		rvptr(&b, Rvt3, abi->s);
		rvptr(&b, Rvt2, abi->d);
		rvstore(&b, Rvt2, Rvt3, bytes);
		break;
	case IJMP:
		rvptr(&b, Rvt2, abi->d);
		rvload(&b, Rvt3, Rvt2, abi->xlen/8);
		rvs(&b, abi->xlen == 64 ? 3 : 2, Rva0, Rvt3, abi->pc);
		break;
	case IDIVB: case IMODB: bytes=1; /* fall through */
	case IDIVW: case IMODW:
		if(abi->xlen != 64 || !abi->hasdiv) return 0;
		binary=1; division=1; f7=1;
		f3=(op == IMODB || op == IMODW) ? 6 : 4;
		break;
	case IDIVL: case IMODL:
		if(abi->xlen != 64 || !abi->hasdiv) return 0;
		bytes=8; binary=1; division=1; f7=1; f3=op == IMODL ? 6 : 4;
		break;
	case ISHLL: case ISHRL: case ILSRL:
		if(abi->xlen != 64) return 0;
		bytes=8; binary=1; shift=1;
		f3=op == ISHLL ? 1 : 5; f7=op == ISHRL ? 32 : 0;
		break;
	case IBEQL: case IBNEL: case IBLTL: case IBLEL: case IBGTL: case IBGEL:
		if(abi->xlen != 64) return 0;
		bytes=8; branch=1;
		f3=op == IBEQL ? 0 : op == IBNEL ? 1 : (op == IBLTL || op == IBGTL) ? 4 : 5;
		swap=op == IBLEL || op == IBGTL;
		break;
	case IADDL: case ISUBL: case IMULL: case IANDL: case IORL: case IXORL:
		if(abi->xlen != 64) return 0;
		bytes=8; binary=1;
		f7=op == ISUBL ? 32 : op == IMULL ? 1 : 0;
		f3=op == IANDL ? 7 : op == IORL ? 6 : op == IXORL ? 4 : 0;
		break;
	case IADDB: bytes=1; /* fall through */
	case IADDW: binary=1; break;
	case ISUBB: bytes=1; /* fall through */
	case ISUBW: binary=1; f7=32; break;
	case IMULB: bytes=1; /* fall through */
	case IMULW:
		if(!abi->hasm && abi->xlen != 64) return 0;
		binary=1; f7=1; break;
	case IANDB: bytes=1; /* fall through */
	case IANDW: binary=1; f3=7; break;
	case IORB: bytes=1; /* fall through */
	case IORW: binary=1; f3=6; break;
	case IXORB: bytes=1; /* fall through */
	case IXORW: binary=1; f3=4; break;
	case ISHLB: bytes=1; /* fall through */
	case ISHLW: binary=1; f3=1; break;
	case ISHRB: binary=1; bytes=1; f3=5; break;
	case ISHRW: binary=1; f3=5; f7=32; break;
	case ILSRW: binary=1; f3=5; break;
	case IBEQB: bytes=1; /* fall through */
	case IBEQW: branch=1; f3=0; break;
	case IBNEB: bytes=1; /* fall through */
	case IBNEW: branch=1; f3=1; break;
	case IBLTB: bytes=1; /* fall through */
	case IBLTW: branch=1; f3=4; break;
	case IBGEB: bytes=1; /* fall through */
	case IBGEW: branch=1; f3=5; break;
	case IBGTB: bytes=1; /* fall through */
	case IBGTW: branch=1; f3=4; swap=1; break;
	case IBLEB: bytes=1; /* fall through */
	case IBLEW: branch=1; f3=5; swap=1; break;
	/* References, allocation, calls and floating arithmetic stay in optab. */
	default:
		return 0;
	}
	shift = shift || op == ISHLB || op == ISHLW || op == ISHRB || op == ISHRW || op == ILSRW;
	if(binary || branch) {
		rvptr(&b, Rvt0, abi->s);
		rvptr(&b, Rvt1, abi->m);
		rvload(&b, Rvt4, Rvt0, shift ? abi->wordbytes : bytes);
		if(realbranch) rvrealkey(&b, Rvt4);
		/* Reject exceptional inputs before reading the value operand or touching d. */
		if(shift || division){
			if(shift){
				rvi(&b, 0x13, 0, Rvt6, Rvzero, bytes*8);
				rvbranch(&b, 6, Rvt4, Rvt6, 12);
			}else
				rvbranch(&b, 1, Rvt4, Rvzero, 12);
			rvi(&b, 0x13, 0, Rva0, Rvzero, 0);
			rvi(&b, 0x67, 0, Rvzero, Rvra, 0);
		}
		rvload(&b, Rvt3, Rvt1, bytes);
		if(realbranch) rvrealkey(&b, Rvt3);
		if(binary) {
			/* RV64 W arithmetic truncates to 32 bits; bitwise operations
			 * have no W encoding and are truncated by SW instead. */
			wide=abi->xlen == 64 && bytes == 4 &&
				(f3 == 0 || f3 == 1 || f3 == 5 || division);
			if(f7 == 1 && !division && !abi->hasm)
				rvmultiply(&b, Rvt4, bytes);
			else
				rvr(&b, wide ? 0x3b : 0x33, f3, f7, Rvt3, Rvt3, Rvt4);
			rvptr(&b, Rvt2, abi->d);
			rvstore(&b, Rvt2, Rvt3, bytes);
		}else{
			/* Branch over pointer update when the relation is false.
			 * For bytes LBU makes signed compares equivalent to unsigned. */
			invert=f3 ^ 1;
			rs=swap ? Rvt3 : Rvt4;
			rvbranch(&b, invert, rs, swap ? Rvt4 : Rvt3, abi->xlen == 64 ? 28 : 16);
			rvptr(&b, Rvt2, abi->d);
			rvload(&b, Rvt3, Rvt2, abi->xlen/8);
			rvs(&b, abi->xlen == 64 ? 3 : 2, Rva0, Rvt3, abi->pc);
		}
	}
	rvi(&b, 0x13, 0, Rva0, Rvzero, 1); /* operation completed */
	rvi(&b, 0x67, 0, Rvzero, Rvra, 0); /* ret */
	return b.bad ? 0 : b.n;
}


enum {
	Rva2=12, // Optional native fast-entry argument.
	RvBridgeWords=15, // Fixed RV64I tail and full-width literal pool budget.
};


/* Tail-enter the complete C executor without retaining a return address in disposable JIT memory.
 * Pointer literals use two aligned32 loads, so a four-byte-aligned entry never needs an unaligned LD.
 * This RV64I bridge leaves a0/ra/sp intact and supplies a1=opcode, a2=optional fast entry. */
static int
rvbridgeemit(u32int *out, int cap, unsigned long long executor, int op, unsigned long long fast)
{
	RvBuf buffer;

	if(op <= INOP || op >= MAXDIS || op == IECLR || executor == 0 || executor == ~0ULL || fast == ~0ULL || ((executor|fast)&1) != 0)
		return 0;
	if(cap < RvBridgeWords)
		return -1;
	buffer.out = out;
	buffer.n = 0;
	buffer.cap = cap;
	buffer.bad = 0;
	buffer.abi = 0;
	rvi(&buffer, 0x13, 0, Rva1, Rvzero, op);
	rvput(&buffer, (u32int)Rvt0<<7 | 0x17);
	rvi(&buffer, 0x03, 6, Rva2, Rvt0, 48);
	rvi(&buffer, 0x03, 6, Rvt1, Rvt0, 52);
	rvi(&buffer, 0x13, 1, Rvt1, Rvt1, 32);
	rvr(&buffer, 0x33, 6, 0, Rva2, Rva2, Rvt1);
	rvi(&buffer, 0x03, 6, Rvt1, Rvt0, 40);
	rvi(&buffer, 0x03, 6, Rvt0, Rvt0, 44);
	rvi(&buffer, 0x13, 1, Rvt0, Rvt0, 32);
	rvr(&buffer, 0x33, 6, 0, Rvt0, Rvt0, Rvt1);
	rvi(&buffer, 0x67, 0, Rvzero, Rvt0, 0);
	rvput(&buffer, (u32int)executor);
	rvput(&buffer, (u32int)(executor>>32));
	rvput(&buffer, (u32int)fast);
	rvput(&buffer, (u32int)(fast>>32));
	return buffer.bad ? -1 : buffer.n;
}
