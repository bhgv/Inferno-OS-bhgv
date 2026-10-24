#include "leaf64-ops.h"
enum { LeafMax=64 };
typedef struct X64Buf X64Buf;
struct X64Buf { uchar bytes[LeafMax*4]; int n; };

/* Append one instruction byte to the fixed private emission buffer. */
static void
x64byte(X64Buf *b, int value)
{
	if(b->n < sizeof(b->bytes)) b->bytes[b->n] = value;
	b->n++;
}

/* Write little-endian displacements without unaligned host stores. */
static void
x64word(X64Buf *b, int value)
{
	int i;
	for(i = 0; i < 4; i++) x64byte(b, (u32int)value >> (i*8));
}

/* Fetch a REG pointer into r8, r9 or r10 using the SysV rdi argument. */
static void
x64ptr(X64Buf *b, int reg, int offset)
{
	x64byte(b, 0x4c); x64byte(b, 0x8b);
	x64byte(b, 0x87+reg*8); x64word(b, offset);
}

/* Capture a scalar in rax or rcx before any possibly aliased destination store. */
static void
x64load(X64Buf *b, int reg, int bytes)
{
	x64byte(b, 0x49);
	if(bytes == 8) x64byte(b, 0x8b);
	else if(bytes == 4) x64byte(b, 0x63);
	else { x64byte(b, 0x0f); x64byte(b, bytes == 1 ? 0xb6 : 0xbf); }
	x64byte(b, reg*9);
}

/* Store only the selected VM value width through r10. */
static void
x64store(X64Buf *b, int reg, int bytes)
{
	if(bytes == 2) x64byte(b, 0x66);
	x64byte(b, bytes == 8 ? 0x49 : 0x41);
	x64byte(b, bytes == 1 ? 0x88 : 0x89);
	x64byte(b, 2+reg*8);
}

/* Emit a SysV AMD64 leaf, returning its bounded count of four-byte code units. */
static int
leafemit(u32int *out, int cap, LeafLayout *abi, int op)
{
	X64Buf b;
	LeafOp p;
	int i, patch, n;
	static uchar arithmetic[] = {0x01, 0x29, 0, 0x21, 0x09, 0x31};
	static uchar inverse[] = {0x75, 0x74, 0x7d, 0x7f, 0x7e, 0x7c};

	if(cap <= 0 || (abi->wordbytes != 4 && abi->wordbytes != 8) ||
	   abi->pc < 0 || abi->s < 0 || abi->d < 0 || abi->m < 0 ||
	   !leafop(&p, op, abi->wordbytes)) return 0;
	b.n = 0;
	/* ENDBR64 also permits entry when the host enables indirect-branch tracking. */
	x64byte(&b, 0xf3); x64byte(&b, 0x0f); x64byte(&b, 0x1e); x64byte(&b, 0xfa);
	if(p.kind == LeafMove || p.kind == LeafAddress){
		x64ptr(&b, 0, abi->s);
		if(p.kind == LeafMove) x64load(&b, 0, p.input);
		else { x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xc0); }
		x64ptr(&b, 2, abi->d);
		x64store(&b, 0, p.bytes);
	}else if(p.kind == LeafShift || p.kind == LeafDivide){
		x64ptr(&b, 0, abi->s);
		x64load(&b, 0, p.kind == LeafShift ? abi->wordbytes : p.bytes);
		if(p.kind == LeafShift){
			/* Unsigned comparison rejects negative/full-width counts before reading m. */
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, p.bytes*8);
			x64byte(&b, 0x72); x64byte(&b, 3);
		}else{
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
			x64byte(&b, 0x75); x64byte(&b, 3);
		}
		/* Return zero without changing VM-visible storage on rejected inputs. */
		x64byte(&b, 0x31); x64byte(&b, 0xc0); x64byte(&b, 0xc3);
		if(p.kind == LeafDivide){
			/* IDIV traps on minimum/-1; let the VM implement its wrap rule. */
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
			x64byte(&b, 0x75); x64byte(&b, 3);
			x64byte(&b, 0x31); x64byte(&b, 0xc0); x64byte(&b, 0xc3);
		}
		x64ptr(&b, 1, abi->m); x64load(&b, 1, p.bytes);
		if(p.kind == LeafShift){
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xca); /* rdx=value */
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc1); /* rcx=count */
			if(p.bytes == 8) x64byte(&b, 0x48);
			x64byte(&b, 0xd3); x64byte(&b, p.alu == 0 ? 0xe2 : p.alu == 1 ? 0xea : 0xfa);
			x64ptr(&b, 2, abi->d); x64store(&b, 2, p.bytes);
		}else{
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3); /* r11=divisor */
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc8); /* rax=dividend */
			if(p.bytes == 8) x64byte(&b, 0x48);
			x64byte(&b, 0x99); /* cqo/cdq */
			x64byte(&b, p.bytes == 8 ? 0x49 : 0x41); x64byte(&b, 0xf7); x64byte(&b, 0xfb);
			x64ptr(&b, 2, abi->d); x64store(&b, p.alu ? 2 : 0, p.bytes);
		}
	}else if(p.kind == LeafAlu || p.kind == LeafBranch){
		x64ptr(&b, 0, abi->s); x64ptr(&b, 1, abi->m);
		x64load(&b, 0, p.bytes); x64load(&b, 1, p.bytes);
		if(p.bytes == 8) x64byte(&b, 0x48);
		if(p.kind == LeafAlu){
			if(p.alu == 2){ x64byte(&b, 0x0f); x64byte(&b, 0xaf); x64byte(&b, 0xc8); }
			else { x64byte(&b, arithmetic[p.alu]); x64byte(&b, 0xc1); }
			x64ptr(&b, 2, abi->d); x64store(&b, 1, p.bytes);
		}else{
			x64byte(&b, 0x39); x64byte(&b, 0xc8);
			x64byte(&b, inverse[p.condition]); patch = b.n; x64byte(&b, 0);
			x64ptr(&b, 2, abi->d);
			x64byte(&b, 0x49); x64byte(&b, 0x8b); x64byte(&b, 0x02);
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x87); x64word(&b, abi->pc);
			b.bytes[patch] = b.n-patch-1;
		}
	}else if(p.kind == LeafJump){
		x64ptr(&b, 2, abi->d);
		x64byte(&b, 0x49); x64byte(&b, 0x8b); x64byte(&b, 0x02);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x87); x64word(&b, abi->pc);
	}
	x64byte(&b, 0xb8); x64word(&b, 1); x64byte(&b, 0xc3);
	while(b.n%4 != 0) x64byte(&b, 0x90);
	n = b.n/4;
	if(b.n > sizeof(b.bytes) || n > cap) return 0;
	if(out != nil) for(i = 0; i < n; i++)
		out[i] = (u32int)b.bytes[i*4] | (u32int)b.bytes[i*4+1]<<8 |
			(u32int)b.bytes[i*4+2]<<16 | (u32int)b.bytes[i*4+3]<<24;
	return n;
}
