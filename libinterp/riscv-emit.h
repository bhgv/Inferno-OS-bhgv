/*
 * RV32I/RV64I leaf code for decoded Dis operations.
 * a0 points to REG; only caller-saved t0-t6 are clobbered. No native stack,
 * absolute addresses, compressed instructions, relocations or helper calls.
 * Include isa.h first. u32int must be exactly 32 bits, independently of XLEN.
 */
enum { Rvzero=0, Rvra=1, Rvt0=5, Rvt1=6, Rvt2=7, Rva0=10,
	   Rvt3=28, Rvt4=29, Rvt5=30, Rvt6=31, RvMax=64 };

typedef struct RvLayout RvLayout;
struct RvLayout {
	int xlen, wordbytes, hasm;
	int pc, s, d, m;
	int arraylen, arraydata;
	int stringlen, stringdata;
	int listdata, listtail;
	int fp, fixedtmp;
	int arraytype, typesize;
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

/* Return instruction count, or 0 for unsupported/invalid/overflow. out==0
 * performs the same bounded sizing pass without touching memory. */
static int
rvemit(u32int *out, int cap, RvLayout *abi, int op)
{
	RvBuf b;
	int bytes, f3, f7, wide, rs, binary, branch, swap, invert, shift, division, scale, realbranch;

	if((abi->xlen != 32 && abi->xlen != 64) ||
	   (abi->wordbytes != 4 && abi->wordbytes != 8) ||
	   abi->wordbytes*8 > abi->xlen || cap <= 0 ||
	   abi->pc < 0 || abi->s < 0 || abi->d < 0 || abi->m < 0 ||
	   abi->pc > 2047 || abi->s > 2047 || abi->d > 2047 || abi->m > 2047 ||
	   (abi->pc|abi->s|abi->d|abi->m) % (abi->xlen/8) != 0)
		return 0;
	b.out=out; b.n=0; b.cap=cap; b.bad=0; b.abi=abi;
	bytes=abi->wordbytes; f3=0; f7=0; binary=0; branch=0; swap=0; invert=0; shift=0; division=0; realbranch=0;
	switch(op) {
	case IBEQF: case IBNEF: case IBLTF: case IBLEF: case IBGTF: case IBGEF:
		if(abi->xlen != 64) return 0;
		bytes=8; branch=1; realbranch=1;
		f3=op == IBEQF ? 0 : op == IBNEF ? 1 : (op == IBLTF || op == IBGTF) ? 4 : 5;
		swap=op == IBLEF || op == IBGTF;
		break;
	case IINDX:
		/* A bounded positive descriptor product fits 62 bits before the final bound check. */
		if(abi->xlen != 64 || abi->wordbytes != 8 || !abi->hasm ||
		   abi->arraylen < 0 || abi->arraylen > 2047 || abi->arraydata < 0 || abi->arraydata > 2047 ||
		   abi->arraytype < 0 || abi->arraytype > 2047 || abi->typesize < 0 || abi->typesize > 2047 ||
		   (abi->arraylen|abi->arraydata|abi->arraytype)%8 || abi->typesize%4) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8); rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
		rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->arraylen); rvguard(&b, 5, Rvt3, Rvzero);
		rvptr(&b, Rvt1, abi->d); rvload(&b, Rvt4, Rvt1, 8); rvguard(&b, 6, Rvt4, Rvt3);
		rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->arraytype); rvguard(&b, 1, Rvt3, Rvzero); rvguard(&b, 1, Rvt3, Rvt6);
		rvi(&b, 0x03, 2, Rvt6, Rvt3, abi->typesize); rvguard(&b, 5, Rvt6, Rvzero);
		rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->arraydata);
		rvi(&b, 0x13, 0, Rvt5, Rvt3, 0);
		rvbranch(&b, 0, Rvt6, Rvzero, 84);
		rvi(&b, 0x13, 0, Rvt0, Rvzero, -1); rvi(&b, 0x13, 5, Rvt0, Rvt0, 33);
		rvguard(&b, 7, Rvt0, Rvt4); rvr(&b, 0x33, 0, 1, Rvt4, Rvt4, Rvt6); rvguard(&b, 7, Rvt0, Rvt4);
		rvguard(&b, 1, Rvt3, Rvzero); rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt3, Rvt6);
		rvr(&b, 0x33, 0, 0, Rvt5, Rvt3, Rvt4); rvguard(&b, 7, Rvt5, Rvt3);
		rvptr(&b, Rvt2, abi->m); rvstore(&b, Rvt2, Rvt5, 8);
		break;
	case IMULX: case IDIVX:
		/* Fixed 64-bit intermediates require RV64M and explicit frame offsets. */
		if(abi->xlen != 64 || !abi->hasm || abi->fp < 0 || abi->fp > 2047 || abi->fp%8 ||
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
		if(op == IMULX) rvr(&b, 0x33, 0, 1, Rvt3, Rvt3, Rvt6);
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
		rvguard(&b, 5, Rvt4, Rvzero);
		bytes = op == IEXPL ? 8 : abi->wordbytes;
		rvptr(&b, Rvt1, abi->m); rvload(&b, Rvt3, Rvt1, bytes);
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
		   abi->stringdata < 0 || abi->stringdata > 2047 || abi->stringdata%4 != 0) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
		rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
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
		/* Bounded scans decline without publishing a partial length. */
		if(abi->xlen != 64 || abi->listtail < 0 || abi->listtail > 2047 || abi->listtail%8 != 0) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
		rvi(&b, 0x13, 0, Rvt3, Rvzero, 0);
		rvi(&b, 0x13, 0, Rvt4, Rvzero, -1);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, 128);
		rvbranch(&b, 0, Rvt0, Rvt4, 40);
		rvguard(&b, 1, Rvt0, Rvzero);
		rvguard(&b, 6, Rvt3, Rvt6);
		rvi(&b, 0x03, 3, Rvt0, Rvt0, abi->listtail);
		rvi(&b, 0x13, 0, Rvt3, Rvt3, 1);
		rvbranch(&b, 0, Rvzero, Rvzero, -36);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, abi->wordbytes);
		break;
	case IHEADB: case IHEADW: case IHEADL: case IHEADF:
		if(abi->xlen != 64 || abi->listdata < 0 || abi->listdata > 2047 || abi->listdata%8 != 0) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
		rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt0, Rvt6);
		rvi(&b, 0x13, 0, Rvt1, Rvt0, abi->listdata);
		bytes = op == IHEADB ? 1 : op == IHEADW ? abi->wordbytes : 8;
		rvload(&b, Rvt3, Rvt1, bytes);
		rvptr(&b, Rvt2, abi->d); rvstore(&b, Rvt2, Rvt3, bytes);
		break;
	case ILENC:
		if(abi->xlen != 64 || abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 != 0) return 0;
		rvptr(&b, Rvt1, abi->s); rvload(&b, Rvt0, Rvt1, 8);
		rvguard(&b, 1, Rvt0, Rvzero);
		rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
		rvbranch(&b, 0, Rvt0, Rvt6, 36);
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
		rvi(&b, 0x13, 0, Rvt6, Rvzero, 128);
		rvguard(&b, 7, Rvt6, Rvt3);
		rvbranch(&b, 0, Rvt3, Rvzero, 76);
		if(op == IHEADM)
			rvi(&b, 0x13, 0, Rvt0, Rvt0, abi->listdata);
		else
			rvptr(&b, Rvt0, abi->s);
		rvptr(&b, Rvt2, abi->d);
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
		if(abi->xlen != 64 || (op != ILENA && abi->wordbytes != 8) ||
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
			rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->arraylen);
			rvguard(&b, 5, Rvt3, Rvzero);
			rvptr(&b, Rvt1, abi->d); rvload(&b, Rvt4, Rvt1, 8);
			rvguard(&b, 6, Rvt4, Rvt3);
			scale = op == IINDB ? 0 : 3;
			rvi(&b, 0x13, 0, Rvt6, Rvzero, -1);
			rvi(&b, 0x13, 5, Rvt6, Rvt6, 33+scale);
			rvguard(&b, 7, Rvt6, Rvt4);
			rvi(&b, 0x03, 3, Rvt3, Rvt0, abi->arraydata);
			rvguard(&b, 1, Rvt3, Rvzero);
			rvi(&b, 0x13, 0, Rvt6, Rvzero, -1); rvguard(&b, 1, Rvt3, Rvt6);
			if(scale) rvi(&b, 0x13, 1, Rvt4, Rvt4, scale);
			rvr(&b, 0x33, 0, 0, Rvt5, Rvt3, Rvt4);
			rvguard(&b, 7, Rvt5, Rvt3);
			rvptr(&b, Rvt2, abi->m); rvstore(&b, Rvt2, Rvt5, 8);
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
		/* Existing xec.c stores addresses in WORD. Do not widen that slot. */
		if(bytes*8 != abi->xlen)
			return 0;
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
		if(abi->xlen != 64 || !abi->hasm) return 0;
		binary=1; division=1; f7=1;
		f3=(op == IMODB || op == IMODW) ? 6 : 4;
		break;
	case IDIVL: case IMODL:
		if(abi->xlen != 64 || !abi->hasm) return 0;
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
		if(abi->xlen != 64 || (op == IMULL && !abi->hasm)) return 0;
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
		if(!abi->hasm) return 0;
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
