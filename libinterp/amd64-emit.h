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

/* Return to the VM without writes unless the preceding condition permits execution. */
static void
x64guard(X64Buf *b, int condition)
{
	x64byte(b, condition); x64byte(b, 3);
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
}

/* Read a scalar header or payload field from an already validated object. */
static void
x64field(X64Buf *b, int result, int owner, int offset, int bytes)
{
	x64byte(b, owner >= 8 ? 0x49 : 0x48);
	if(bytes == 1){ x64byte(b, 0x0f); x64byte(b, 0xb6); }
	else x64byte(b, bytes == 8 ? 0x8b : 0x63);
	x64byte(b, 0x80 | result<<3 | (owner&7));
	x64word(b, offset);
}

/* Turn rax/rcx bits into signed keys using rdx; defer NaNs and denormals. */
static void
x64realkey(X64Buf *b, int reg)
{
	int zero, normal, done;

	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2+reg*8);
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe2);
	x64byte(b, 0x74); zero = b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xea); x64byte(b, 53);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x75);
	x64byte(b, 0x81); x64byte(b, 0xfa); x64word(b, 2047);
	x64byte(b, 0x75); normal = b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2+reg*8);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe2); x64byte(b, 12);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x74);
	b->bytes[normal] = b->n-normal-1;
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2+reg*8);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xfa); x64byte(b, 63);
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xea);
	x64byte(b, 0x48); x64byte(b, 0x31); x64byte(b, 0xd0+reg);
	x64byte(b, 0xeb); done = b->n; x64byte(b, 0);
	b->bytes[zero] = b->n-zero-1;
	x64byte(b, 0x31); x64byte(b, reg == 0 ? 0xc0 : 0xc9);
	b->bytes[done] = b->n-done-1;
}

/* Emit a SysV AMD64 leaf, returning its bounded count of four-byte code units. */
static int
leafemit(u32int *out, int cap, LeafLayout *abi, int op)
{
	X64Buf b;
	LeafOp p;
	int i, patch, n, empty, other, done, scale, loop;
	static uchar arithmetic[] = {0x01, 0x29, 0, 0x21, 0x09, 0x31};
	static uchar inverse[] = {0x75, 0x74, 0x7d, 0x7f, 0x7e, 0x7c};

	if(cap <= 0 || (abi->wordbytes != 4 && abi->wordbytes != 8) ||
	   abi->pc < 0 || abi->s < 0 || abi->d < 0 || abi->m < 0 ||
	   !leafop(&p, op, abi->wordbytes)) return 0;
	b.n = 0;
	/* ENDBR64 also permits entry when the host enables indirect-branch tracking. */
	x64byte(&b, 0xf3); x64byte(&b, 0x0f); x64byte(&b, 0x1e); x64byte(&b, 0xfa);
	if(p.kind == LeafFixedFrame){
		/* Read the explicit frame layout before capturing either arithmetic operand. */
		if(abi->fp < 0 || abi->fp > 2047 || abi->fp%8 || abi->fixedtmp < 0 || abi->fixedtmp > 2047) return 0;
		x64ptr(&b, 1, abi->fp);
		x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xc8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x49); x64byte(&b, 0x81); x64byte(&b, 0xc1); x64word(&b, abi->fixedtmp);
		x64load(&b, 1, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x8d); x64byte(&b, 0x51); x64byte(&b, 63);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 127); x64guard(&b, 0x72);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc2);
		if(p.alu){
			/* Preserve divide errors and minimum/-1 behavior through the C fallback. */
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xd3);
		}
		x64ptr(&b, 0, abi->m); x64load(&b, 0, abi->wordbytes);
		if(!p.alu){ x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0xaf); x64byte(&b, 0xc2); }
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9);
		x64byte(&b, 0x78); x64byte(&b, 5);
		x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xe0);
		x64byte(&b, 0xeb); x64byte(&b, 6);
		x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xd9);
		x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xf8);
		if(p.alu){ x64byte(&b, 0x48); x64byte(&b, 0x99); x64byte(&b, 0x49); x64byte(&b, 0xf7); x64byte(&b, 0xfb); }
		x64ptr(&b, 2, abi->d); x64store(&b, 0, abi->wordbytes);
	}else if(p.kind == LeafRealResize){
		/* Resize exact normal values and signed zeros without changing FP status. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, p.input);
		if(p.input == 4){ x64byte(&b, 0x89); x64byte(&b, 0xc0); }
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc2);
		x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xea); x64byte(&b, p.input == 4 ? 31 : 63);
		x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe2); x64byte(&b, p.input == 4 ? 63 : 31);
		x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe0); x64byte(&b, p.input == 4 ? 33 : 1);
		x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe8); x64byte(&b, p.input == 4 ? 33 : 1);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
		x64byte(&b, 0x74); empty=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc1);
		x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe9); x64byte(&b, p.input == 4 ? 23 : 52);
		if(p.input == 4){
			x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x75);
			x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, 255); x64guard(&b, 0x72);
			x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe0); x64byte(&b, 29);
			x64byte(&b, 0x48); x64byte(&b, 0xb9); x64word(&b, 0); x64word(&b, 0x38000000);
			x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xc8);
		}else{
			x64byte(&b, 0x48); x64byte(&b, 0x81); x64byte(&b, 0xe9); x64word(&b, 897);
			x64byte(&b, 0x48); x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, 254); x64guard(&b, 0x72);
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
			x64byte(&b, 0x49); x64byte(&b, 0xc1); x64byte(&b, 0xe3); x64byte(&b, 35);
			x64byte(&b, 0x4d); x64byte(&b, 0x85); x64byte(&b, 0xdb); x64guard(&b, 0x74);
			x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe8); x64byte(&b, 29);
			x64byte(&b, 0x48); x64byte(&b, 0xb9); x64word(&b, 0xc0000000U); x64word(&b, 1);
			x64byte(&b, 0x48); x64byte(&b, 0x29); x64byte(&b, 0xc8);
		}
		b.bytes[empty]=b.n-empty-1;
		x64byte(&b, 0x48); x64byte(&b, 0x09); x64byte(&b, 0xd0);
		x64ptr(&b, 2, abi->d); x64store(&b, 0, p.bytes);
	}else if(p.kind == LeafIntegerReal){
		/* Exact magnitudes below 2^53 need no FP rounding or exception changes. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, p.input);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc2); /* rdx=source */
		x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xfa); x64byte(&b, 63);
		x64byte(&b, 0x48); x64byte(&b, 0x31); x64byte(&b, 0xd0);
		x64byte(&b, 0x48); x64byte(&b, 0x29); x64byte(&b, 0xd0); /* unsigned magnitude */
		x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe2); x64byte(&b, 63);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		x64byte(&b, 0x49); x64byte(&b, 0xc1); x64byte(&b, 0xeb); x64byte(&b, 53);
		x64byte(&b, 0x4d); x64byte(&b, 0x85); x64byte(&b, 0xdb); x64guard(&b, 0x74);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
		x64byte(&b, 0x74); empty=b.n; x64byte(&b, 0);
		x64byte(&b, 0xb9); x64word(&b, 1074);
		loop=b.n;
		x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0xba); x64byte(&b, 0xe0); x64byte(&b, 52);
		x64byte(&b, 0x72); done=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0xd1); x64byte(&b, 0xe0);
		x64byte(&b, 0xff); x64byte(&b, 0xc9);
		x64byte(&b, 0xeb); x64byte(&b, loop-b.n-1);
		b.bytes[done]=b.n-done-1;
		x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe1); x64byte(&b, 52);
		x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xc8);
		x64byte(&b, 0x48); x64byte(&b, 0x09); x64byte(&b, 0xd0);
		b.bytes[empty]=b.n-empty-1;
		x64ptr(&b, 2, abi->d); x64store(&b, 0, 8);
	}else if(p.kind == LeafFixedShift){
		/* Translating the signed interval avoids overflowing when negating minimum. */
		x64ptr(&b, 1, abi->m); x64load(&b, 1, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x8d); x64byte(&b, 0x51); x64byte(&b, 63); /* lea rdx,[rcx+63] */
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 127);
		x64guard(&b, 0x72);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); /* test rcx,rcx */
		x64byte(&b, 0x78); x64byte(&b, 5);
		x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xe0);
		x64byte(&b, 0xeb); x64byte(&b, 6);
		x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xd9); /* neg rcx */
		x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xf8); /* sar rax,cl */
		x64ptr(&b, 2, abi->d); x64store(&b, 0, abi->wordbytes);
	}else if(p.kind == LeafPower){
		/* Let the VM apply negative-exponent results and zero-base exceptions. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x79);
		x64ptr(&b, 1, abi->m); x64load(&b, 1, p.bytes);
		x64byte(&b, 0xba); x64word(&b, 1); /* rdx = result */
		loop = b.n;
		x64byte(&b, 0xa8); x64byte(&b, 1);
		x64byte(&b, 0x74); x64byte(&b, 4);
		x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0xaf); x64byte(&b, 0xd1);
		x64byte(&b, 0x48); x64byte(&b, 0xd1); x64byte(&b, 0xe8);
		x64byte(&b, 0x74); done = b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0xaf); x64byte(&b, 0xc9);
		x64byte(&b, 0xeb); x64byte(&b, loop-b.n-1);
		b.bytes[done] = b.n-done-1;
		x64ptr(&b, 2, abi->d); x64store(&b, 2, p.bytes);
	}else if(p.kind == LeafStringIndex){
		if(abi->stringlen < 0 || abi->stringdata < 0) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3); /* r11 = owner */
		x64field(&b, 1, 0, abi->stringlen, 4);
		x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, 0x80000000U); x64guard(&b, 0x75);
		x64byte(&b, 0x89); x64byte(&b, 0xca); /* edx = representation sign */
		x64byte(&b, 0x85); x64byte(&b, 0xc9);
		x64byte(&b, 0x79); x64byte(&b, 2); x64byte(&b, 0xf7); x64byte(&b, 0xd9);
		x64ptr(&b, 0, abi->m); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
		x64byte(&b, 0x85); x64byte(&b, 0xd2);
		x64byte(&b, 0x78); other = b.n; x64byte(&b, 0);
		x64byte(&b, 0x49); x64byte(&b, 0x0f); x64byte(&b, 0xb6);
		x64byte(&b, 0x84); x64byte(&b, 0x03); x64word(&b, abi->stringdata);
		x64byte(&b, 0xeb); done = b.n; x64byte(&b, 0);
		b.bytes[other] = b.n-other-1;
		x64byte(&b, 0x49); x64byte(&b, 0x63); x64byte(&b, 0x84); x64byte(&b, 0x83); x64word(&b, abi->stringdata);
		b.bytes[done] = b.n-done-1;
		x64ptr(&b, 2, abi->d); x64store(&b, 0, abi->wordbytes);
	}else if(p.kind == LeafListLength){
		/* Bounded scans decline without publishing a partial length. */
		if(abi->listtail < 0) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x31); x64byte(&b, 0xc9); /* count = 0 */
		loop = b.n;
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
		x64byte(&b, 0x74); done = b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, 128); x64guard(&b, 0x72);
		x64field(&b, 0, 0, abi->listtail, 8);
		x64byte(&b, 0xff); x64byte(&b, 0xc1);
		x64byte(&b, 0xeb); x64byte(&b, loop-b.n-1);
		b.bytes[done] = b.n-done-1;
		x64ptr(&b, 2, abi->d); x64store(&b, 1, abi->wordbytes);
	}else if(p.kind == LeafHead){
		if(abi->listdata < 0) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64field(&b, 0, 0, abi->listdata, p.bytes);
		x64ptr(&b, 2, abi->d); x64store(&b, 0, p.bytes);
	}else if(p.kind == LeafStringLength){
		if(abi->stringlen < 0) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
		x64byte(&b, 0x74); empty = b.n; x64byte(&b, 0);
		x64field(&b, 0, 0, abi->stringlen, 4);
		x64byte(&b, 0x3d); x64word(&b, 0x80000000U); x64guard(&b, 0x75);
		x64byte(&b, 0x85); x64byte(&b, 0xc0);
		x64byte(&b, 0x79); x64byte(&b, 2); x64byte(&b, 0xf7); x64byte(&b, 0xd8);
		x64byte(&b, 0xeb); done = b.n; x64byte(&b, 0);
		b.bytes[empty] = b.n-empty-1;
		x64byte(&b, 0x31); x64byte(&b, 0xc0);
		b.bytes[done] = b.n-done-1;
		x64ptr(&b, 2, abi->d); x64store(&b, 0, abi->wordbytes);
	}else if(p.kind == LeafCopy || p.kind == LeafHeadCopy){
		if(p.kind == LeafHeadCopy){
			if(abi->listdata < 0) return 0;
			x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		}
		/* Capture the signed length before copying can overwrite its operand. */
		x64ptr(&b, 0, abi->m); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x79);
		x64byte(&b, 0x74); empty = b.n; x64byte(&b, 0);
		/* Leave large records to the platform's optimized memmove. */
		x64byte(&b, 0x48); x64byte(&b, 0x3d); x64word(&b, 128); x64guard(&b, 0x76);
		if(p.kind == LeafHeadCopy){
			x64byte(&b, 0x4d); x64byte(&b, 0x8d); x64byte(&b, 0x83); x64word(&b, abi->listdata);
		}else
			x64ptr(&b, 0, abi->s);
		x64ptr(&b, 2, abi->d);
		/* Copy downward whenever the destination follows the source. */
		x64byte(&b, 0x4d); x64byte(&b, 0x39); x64byte(&b, 0xc2);
		x64byte(&b, 0x76); other = b.n; x64byte(&b, 0);
		x64byte(&b, 0x49); x64byte(&b, 0x01); x64byte(&b, 0xc0);
		x64byte(&b, 0x49); x64byte(&b, 0x01); x64byte(&b, 0xc2);
		loop = b.n;
		x64byte(&b, 0x49); x64byte(&b, 0xff); x64byte(&b, 0xc8);
		x64byte(&b, 0x49); x64byte(&b, 0xff); x64byte(&b, 0xca);
		x64byte(&b, 0x41); x64byte(&b, 0x8a); x64byte(&b, 0x08);
		x64byte(&b, 0x41); x64byte(&b, 0x88); x64byte(&b, 0x0a);
		x64byte(&b, 0x48); x64byte(&b, 0xff); x64byte(&b, 0xc8);
		x64byte(&b, 0x75); x64byte(&b, loop-b.n-1);
		x64byte(&b, 0xeb); done = b.n; x64byte(&b, 0);
		b.bytes[other] = b.n-other-1;
		loop = b.n;
		x64byte(&b, 0x41); x64byte(&b, 0x8a); x64byte(&b, 0x08);
		x64byte(&b, 0x41); x64byte(&b, 0x88); x64byte(&b, 0x0a);
		x64byte(&b, 0x49); x64byte(&b, 0xff); x64byte(&b, 0xc0);
		x64byte(&b, 0x49); x64byte(&b, 0xff); x64byte(&b, 0xc2);
		x64byte(&b, 0x48); x64byte(&b, 0xff); x64byte(&b, 0xc8);
		x64byte(&b, 0x75); x64byte(&b, loop-b.n-1);
		b.bytes[empty] = b.n-empty-1; b.bytes[done] = b.n-done-1;
	}else if(p.kind == LeafLength || p.kind == LeafIndex){
		if(abi->arraylen < 0 || abi->arraydata < 0) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		if(p.kind == LeafLength){
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
			x64byte(&b, 0x74); empty = b.n; x64byte(&b, 0);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
			x64byte(&b, 0x74); other = b.n; x64byte(&b, 0);
			x64field(&b, 0, 0, abi->arraylen, abi->wordbytes);
			x64byte(&b, 0xeb); done = b.n; x64byte(&b, 0);
			b.bytes[empty] = b.n-empty-1; b.bytes[other] = b.n-other-1;
			x64byte(&b, 0x31); x64byte(&b, 0xc0);
			b.bytes[done] = b.n-done-1;
			x64ptr(&b, 2, abi->d); x64store(&b, 0, abi->wordbytes);
		}else{
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3); /* r11=array */
			x64field(&b, 1, 0, abi->arraylen, 8);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
			x64ptr(&b, 0, abi->d); x64load(&b, 0, 8);
			x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
			scale = p.bytes == 1 ? 0 : 3;
			x64byte(&b, 0x48); x64byte(&b, 0x3d); x64word(&b, 0x7fffffff>>scale); x64guard(&b, 0x76);
			x64field(&b, 1, 11, abi->arraydata, 8);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x75);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf9); x64byte(&b, 0xff); x64guard(&b, 0x75);
			if(scale){ x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe0); x64byte(&b, scale); }
			x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xc8); x64guard(&b, 0x73);
			x64ptr(&b, 2, abi->m); x64store(&b, 0, 8);
		}
	}else if(p.kind == LeafMove || p.kind == LeafAddress || p.kind == LeafNegReal){
		x64ptr(&b, 0, abi->s);
		if(p.kind != LeafAddress) x64load(&b, 0, p.input);
		else { x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xc0); }
		/* Flip the binary64 sign without rounding or quieting NaN payloads. */
		if(p.kind == LeafNegReal){
			x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0xba);
			x64byte(&b, 0xf8); x64byte(&b, 63); /* btc rax, 63 */
		}
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
	}else if(p.kind == LeafAlu || p.kind == LeafBranch || p.kind == LeafRealBranch){
		x64ptr(&b, 0, abi->s); x64ptr(&b, 1, abi->m);
		x64load(&b, 0, p.bytes);
		if(p.kind == LeafRealBranch) x64realkey(&b, 0);
		x64load(&b, 1, p.bytes);
		if(p.kind == LeafRealBranch) x64realkey(&b, 1);
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
