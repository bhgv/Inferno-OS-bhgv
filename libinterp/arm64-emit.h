#include "leaf64-ops.h"
enum { LeafMax=64 };
typedef struct A64Buf A64Buf;
struct A64Buf { u32int code[LeafMax]; int n; };

/* Stage fixed-width instructions without exposing a partial output buffer. */
static void
a64put(A64Buf *b, u32int instruction)
{
	if(b->n < LeafMax) b->code[b->n] = instruction;
	b->n++;
}

/* Load a native REG pointer with an aligned unsigned immediate offset. */
static void
a64ptr(A64Buf *b, int reg, int offset)
{
	a64put(b, 0xf9400000U | (u32int)(offset/8)<<10 | reg);
}

/* Read byte/short/word/long values in caller-saved x12 or x13. */
static void
a64load(A64Buf *b, int reg, int ptr, int bytes)
{
	u32int instruction;
	instruction = bytes == 8 ? 0xf9400000U : bytes == 4 ? 0xb9400000U :
		bytes == 2 ? 0x79c00000U : 0x39400000U;
	a64put(b, instruction | ptr<<5 | reg);
}

/* Store a captured result through x11, preserving neighboring operand bytes. */
static void
a64store(A64Buf *b, int reg, int bytes)
{
	u32int instruction;
	instruction = bytes == 8 ? 0xf9000000U : bytes == 4 ? 0xb9000000U :
		bytes == 2 ? 0x79000000U : 0x39000000U;
	a64put(b, instruction | 11<<5 | reg);
}

/* Emit AAPCS64 leaves with x0 as REG*, no stack frame and no callee-saved clobbers. */
static int
leafemit(u32int *out, int cap, LeafLayout *abi, int op)
{
	A64Buf b;
	LeafOp p;
	int i;
	u32int width;
	static u32int arithmetic[] = {0x0b000000, 0x4b000000, 0x1b007c00,
		0x0a000000, 0x2a000000, 0x4a000000};
	static int inverse[] = {1, 0, 10, 12, 13, 11};

	if(cap <= 0 || (abi->wordbytes != 4 && abi->wordbytes != 8) ||
	   abi->pc < 0 || abi->s < 0 || abi->d < 0 || abi->m < 0 ||
	   abi->pc > 32760 || abi->s > 32760 || abi->d > 32760 || abi->m > 32760 ||
	   (abi->pc|abi->s|abi->d|abi->m)%8 != 0 ||
	   !leafop(&p, op, abi->wordbytes)) return 0;
	b.n = 0;
	a64put(&b, 0xd503245f); /* BTI c; a compatible hint on earlier cores */
	if(p.kind == LeafMove || p.kind == LeafAddress){
		a64ptr(&b, 9, abi->s);
		if(p.kind == LeafMove) a64load(&b, 12, 9, p.input);
		else a64put(&b, 0xaa0903ec); /* mov x12, x9 */
		/* LDRSH W sign-extends only to 32 bits; widen when the VM word is 64 bits. */
		if(p.bytes == 8 && (p.input == 2 || (p.input == 4 && op == ICVTWL))) a64put(&b, 0x93407d8c); /* sxtw x12, w12 */
		a64ptr(&b, 11, abi->d); a64store(&b, 12, p.bytes);
	}else if(p.kind == LeafShift || p.kind == LeafDivide){
		a64ptr(&b, 9, abi->s);
		a64load(&b, 12, 9, p.kind == LeafShift ? abi->wordbytes : p.bytes);
		if(p.kind == LeafShift){
			/* Validate the complete count before accessing a possibly invalid value span. */
			a64put(&b, 0xf100001fU | (p.bytes*8)<<10 | 12<<5);
			a64put(&b, 0x54000063); /* b.lo valid */
		}else
			a64put(&b, 0xb500006c); /* cbnz x12, valid */
		a64put(&b, 0x52800000); a64put(&b, 0xd65f03c0);
		a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, p.bytes);
		width = p.bytes == 8 ? 0x80000000U : 0;
		if(p.kind == LeafShift){
			a64put(&b, (p.alu == 0 ? 0x1ac02000U : p.alu == 1 ? 0x1ac02400U : 0x1ac02800U) |
				width | 12<<16 | 13<<5 | 13);
			a64ptr(&b, 11, abi->d); a64store(&b, 13, p.bytes);
		}else{
			/* SDIV defines minimum/-1; MSUB gives the VM's zero remainder. */
			a64put(&b, 0x1ac00c00U | width | 12<<16 | 13<<5 | 14);
			if(p.alu) a64put(&b, 0x1b008000U | width | 12<<16 | 13<<10 | 14<<5 | 14);
			a64ptr(&b, 11, abi->d); a64store(&b, 14, p.bytes);
		}
	}else if(p.kind == LeafAlu || p.kind == LeafBranch){
		a64ptr(&b, 9, abi->s); a64ptr(&b, 10, abi->m);
		a64load(&b, 12, 9, p.bytes); a64load(&b, 13, 10, p.bytes);
		width = p.bytes == 8 ? 0x80000000U : 0;
		if(p.kind == LeafAlu){
			a64put(&b, arithmetic[p.alu] | width | 12<<16 | 13<<5 | 13);
			a64ptr(&b, 11, abi->d); a64store(&b, 13, p.bytes);
		}else{
			a64put(&b, 0x6b00001fU | width | 13<<16 | 12<<5);
			a64put(&b, 0x54000080U | inverse[p.condition]);
			a64ptr(&b, 11, abi->d); a64load(&b, 12, 11, 8);
			a64put(&b, 0xf9000000U | (u32int)(abi->pc/8)<<10 | 12);
		}
	}else if(p.kind == LeafJump){
		a64ptr(&b, 11, abi->d); a64load(&b, 12, 11, 8);
		a64put(&b, 0xf9000000U | (u32int)(abi->pc/8)<<10 | 12);
	}
	a64put(&b, 0x52800020); a64put(&b, 0xd65f03c0);
	if(b.n > LeafMax || b.n > cap) return 0;
	if(out != nil) for(i = 0; i < b.n; i++) out[i] = b.code[i];
	return b.n;
}
