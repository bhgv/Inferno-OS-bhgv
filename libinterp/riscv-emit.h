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

/* Return instruction count, or 0 for unsupported/invalid/overflow. out==0
 * performs the same bounded sizing pass without touching memory. */
static int
rvemit(u32int *out, int cap, RvLayout *abi, int op)
{
	RvBuf b;
	int bytes, f3, f7, wide, rs, binary, branch, swap, invert;

	if((abi->xlen != 32 && abi->xlen != 64) ||
	   (abi->wordbytes != 4 && abi->wordbytes != 8) ||
	   abi->wordbytes*8 > abi->xlen || cap <= 0 ||
	   abi->pc < 0 || abi->s < 0 || abi->d < 0 || abi->m < 0 ||
	   abi->pc > 2047 || abi->s > 2047 || abi->d > 2047 || abi->m > 2047)
		return 0;
	b.out=out; b.n=0; b.cap=cap; b.bad=0; b.abi=abi;
	bytes=abi->wordbytes; f3=0; f7=0; binary=0; branch=0; swap=0; invert=0;
	switch(op) {
	case IRUNT:
		break;
	case IMOVF:
	case IMOVL:
		/* Capture both halves before storing, including aliasing operands. */
		rvptr(&b, Rvt0, abi->s);
		rvptr(&b, Rvt2, abi->d);
		rvi(&b, 0x03, 2, Rvt3, Rvt0, 0);
		rvi(&b, 0x03, 2, Rvt4, Rvt0, 4);
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
		rvi(&b, 0x03, abi->xlen == 64 ? 3 : 2, Rvt3, Rvt2, 0);
		rvs(&b, abi->xlen == 64 ? 3 : 2, Rva0, Rvt3, abi->pc);
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
	/* Division/modulo and large shifts need the VM's exception semantics.
	 * References/allocations/calls/FP/long arithmetic stay in optab. */
	default:
		return 0;
	}
	if(binary || branch) {
		rvptr(&b, Rvt0, abi->s);
		rvptr(&b, Rvt1, abi->m);
		rvload(&b, Rvt4, Rvt0, (op == ISHLB || op == ISHLW ||
			op == ISHRB || op == ISHRW || op == ILSRW) ? abi->wordbytes : bytes);
		rvload(&b, Rvt3, Rvt1, bytes);
		if(binary) {
			if(op == ISHLB || op == ISHLW || op == ISHRB || op == ISHRW || op == ILSRW) {
				/* Leave exceptional/undefined shift counts to the VM helper.
				 * No visible state has changed when returning 0 here. */
				rvi(&b, 0x13, 0, Rvt6, Rvzero, bytes*8);
				rvbranch(&b, 6, Rvt4, Rvt6, 12); /* bltu count, width, valid */
				rvi(&b, 0x13, 0, Rva0, Rvzero, 0);
				rvi(&b, 0x67, 0, Rvzero, Rvra, 0);
			}
			/* RV64 W arithmetic truncates to 32 bits; bitwise operations
			 * have no W encoding and are truncated by SW instead. */
			wide=abi->xlen == 64 && bytes == 4 &&
				(f3 == 0 || f3 == 1 || f3 == 5);
			rvr(&b, wide ? 0x3b : 0x33, f3, f7, Rvt3, Rvt3, Rvt4);
			rvptr(&b, Rvt2, abi->d);
			rvstore(&b, Rvt2, Rvt3, bytes);
		}else{
			/* Branch over pointer update when the relation is false.
			 * For bytes LBU makes signed compares equivalent to unsigned. */
			invert=f3 ^ 1;
			rs=swap ? Rvt3 : Rvt4;
			rvbranch(&b, invert, rs, swap ? Rvt4 : Rvt3, 16);
			rvptr(&b, Rvt2, abi->d);
			rvi(&b, 0x03, abi->xlen == 64 ? 3 : 2, Rvt3, Rvt2, 0);
			rvs(&b, abi->xlen == 64 ? 3 : 2, Rva0, Rvt3, abi->pc);
		}
	}
	rvi(&b, 0x13, 0, Rva0, Rvzero, 1); /* operation completed */
	rvi(&b, 0x67, 0, Rvzero, Rvra, 0); /* ret */
	return b.bad ? 0 : b.n;
}
