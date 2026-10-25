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

/* Divide a captured signed value without taking the host MIN/-1 overflow trap. */
static void
x64signeddivide(X64Buf *b, int bytes)
{
	int ordinary, done;

	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xfb); x64byte(b, 0xff);
	x64byte(b, 0x75); ordinary=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0xf7); x64byte(b, 0xd8);
	x64byte(b, 0x31); x64byte(b, 0xd2);
	x64byte(b, 0xeb); done=b->n; x64byte(b, 0);
	b->bytes[ordinary]=b->n-ordinary-1;
	if(bytes == 8)x64byte(b, 0x48);
	x64byte(b, 0x99);
	x64byte(b, bytes == 8 ? 0x49 : 0x41); x64byte(b, 0xf7); x64byte(b, 0xfb);
	b->bytes[done]=b->n-done-1;
}

/* Form VN correction from captured operands; invert WN polarity only for nonzero VN products. */
static void
x64fixedround(X64Buf *b, int alu, int wordbytes)
{
	int skip[3], count, sign, i;

	count=0;
	x64byte(b, 0x45); x64byte(b, 0x31); x64byte(b, 0xc9); /* r9=correction */
	x64byte(b, 0x83); x64byte(b, 0xfe); x64byte(b, 2);
	x64byte(b, 0x72); skip[count++]=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x74); skip[count++]=b->n; x64byte(b, 0);
	if(alu == 0){
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2);
		x64byte(b, 0x74); skip[count++]=b->n; x64byte(b, 0);
	}
	if(alu == 1){
		x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc1);
		x64byte(b, 0x4d); x64byte(b, 0x31); x64byte(b, 0xd9);
		x64byte(b, 0x49); x64byte(b, 0xc1); x64byte(b, 0xf9); x64byte(b, 63);
		x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xc9); x64byte(b, 1);
	}else{
		/* LEA performs the WORD-width modular a-1 without reading memory. */
		x64byte(b, wordbytes == 4 ? 0x45 : 0x4d); x64byte(b, 0x8d); x64byte(b, 0x4b); x64byte(b, 0xff);
		x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc0);
		x64byte(b, 0x79); sign=b->n; x64byte(b, 0);
		x64byte(b, wordbytes == 4 ? 0x41 : 0x49); x64byte(b, 0xf7); x64byte(b, 0xd9);
		b->bytes[sign]=b->n-sign-1;
		if(wordbytes == 4){x64byte(b, 0x4d); x64byte(b, 0x63); x64byte(b, 0xc9);}
		x64byte(b, 0x49); x64byte(b, 0x0f); x64byte(b, 0xba); x64byte(b, 0xf8); x64byte(b, 63);
	}
	for(i=0;i<count;i++)b->bytes[skip[i]]=b->n-skip[i]-1;
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

/* Resolve either string representation, retaining a native character stride. */
static void
x64stringheader(X64Buf *b, int owner, int length, int stride, int lenoff, int dataoff)
{
	int empty, positive, done;

	x64byte(b, 0x41); x64byte(b, 0xb8+stride); x64word(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0+owner*9); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8+owner); x64byte(b, 0xff);
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64field(b, length, owner, lenoff, 4);
	x64byte(b, 0x81); x64byte(b, 0xf8+length); x64word(b, 0x80000000U); x64guard(b, 0x75);
	x64byte(b, 0x85); x64byte(b, 0xc0+length*9);
	x64byte(b, 0x79); positive=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0xf7); x64byte(b, 0xd8+length);
	x64byte(b, 0x41); x64byte(b, 0xb8+stride); x64word(b, 4);
	b->bytes[positive]=b->n-positive-1;
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xc0+owner); x64word(b, dataoff);
	x64byte(b, 0xeb); done=b->n; x64byte(b, 0);
	b->bytes[empty]=b->n-empty-1;
	x64byte(b, 0x31); x64byte(b, 0xc0+length*9);
	b->bytes[done]=b->n-done-1;
}

/* Reject NaNs without FP effects; finite/subnormal comparisons use host FP controls. */
static void
x64realguard(X64Buf *b, int reg)
{
	int ordinary;

	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2+reg*8);
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe2);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xea); x64byte(b, 53);
	x64byte(b, 0x81); x64byte(b, 0xfa); x64word(b, 2047);
	x64byte(b, 0x75); ordinary=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2+reg*8);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe2); x64byte(b, 12);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x74);
	b->bytes[ordinary]=b->n-ordinary-1;
}

/* Emit a SysV AMD64 leaf, returning its bounded count of four-byte code units. */
static int
leafemit(u32int *out, int cap, LeafLayout *abi, int op)
{
	X64Buf b;
	LeafOp p;
	int i, patch, n, empty, other, done, scale, loop, equal;
	static uchar arithmetic[] = {0x01, 0x29, 0, 0x21, 0x09, 0x31};
	static uchar inverse[] = {0x75, 0x74, 0x7d, 0x7f, 0x7e, 0x7c};
	static uchar realinverse[] = {0x75, 0x74, 0x73, 0x77, 0x76, 0x72};

	if(cap <= 0 || (abi->wordbytes != 4 && abi->wordbytes != 8) ||
	   abi->pc < 0 || abi->s < 0 || abi->d < 0 || abi->m < 0 ||
	   !leafop(&p, op, abi->wordbytes)) return 0;
	b.n = 0;
	/* ENDBR64 also permits entry when the host enables indirect-branch tracking. */
	x64byte(&b, 0xf3); x64byte(&b, 0x0f); x64byte(&b, 0x1e); x64byte(&b, 0xfa);
	if(p.kind == LeafStringBranch){
		/* Compare captured characters without touching a false branch target. */
		if(abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 ||
		   abi->stringdata < 0 || abi->stringdata > 2047) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64ptr(&b, 1, abi->m); x64load(&b, 1, 8);
		x64stringheader(&b, 0, 2, 0, abi->stringlen, abi->stringdata);
		x64stringheader(&b, 1, 6, 1, abi->stringlen, abi->stringdata);
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8);
		x64byte(&b, 0x74); equal=b.n; x64byte(&b, 0);
		loop=b.n;
		x64byte(&b, 0x85); x64byte(&b, 0xd2); x64byte(&b, 0x74); empty=b.n; x64byte(&b, 0);
		x64byte(&b, 0x85); x64byte(&b, 0xf6); x64byte(&b, 0x74); other=b.n; x64byte(&b, 0);
		x64byte(&b, 0x44); x64byte(&b, 0x0f); x64byte(&b, 0xb6); x64byte(&b, 0x10);
		x64byte(&b, 0x41); x64byte(&b, 0x80); x64byte(&b, 0xf8); x64byte(&b, 1);
		x64byte(&b, 0x74); x64byte(&b, 3);
		x64byte(&b, 0x44); x64byte(&b, 0x8b); x64byte(&b, 0x10);
		x64byte(&b, 0x44); x64byte(&b, 0x0f); x64byte(&b, 0xb6); x64byte(&b, 0x19);
		x64byte(&b, 0x41); x64byte(&b, 0x80); x64byte(&b, 0xf9); x64byte(&b, 1);
		x64byte(&b, 0x74); x64byte(&b, 3);
		x64byte(&b, 0x44); x64byte(&b, 0x8b); x64byte(&b, 0x19);
		x64byte(&b, 0x45); x64byte(&b, 0x39); x64byte(&b, 0xda);
		x64byte(&b, 0x75); done=b.n; x64byte(&b, 0);
		x64byte(&b, 0x4c); x64byte(&b, 0x01); x64byte(&b, 0xc0);
		x64byte(&b, 0x4c); x64byte(&b, 0x01); x64byte(&b, 0xc9);
		x64byte(&b, 0xff); x64byte(&b, 0xca); x64byte(&b, 0xff); x64byte(&b, 0xce);
		x64byte(&b, 0xeb); x64byte(&b, loop-b.n-1);
		b.bytes[empty]=b.n-empty-1; b.bytes[other]=b.n-other-1;
		b.bytes[equal]=b.n-equal-1;
		x64byte(&b, 0x39); x64byte(&b, 0xf2);
		b.bytes[done]=b.n-done-1;
		x64byte(&b, inverse[p.condition]); patch=b.n; x64byte(&b, 0);
		x64ptr(&b, 2, abi->d); x64load(&b, 2, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x97); x64word(&b, abi->pc);
		b.bytes[patch]=b.n-patch-1;
	}else if(p.kind == LeafTypeCheck){
		/* Capture both owners before inspecting their immutable heap types. */
		if(abi->heaptype <= 0 || abi->heaptype > 255 || abi->heaptype%8) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64ptr(&b, 1, abi->d); x64load(&b, 1, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
		x64byte(&b, 0x74); done=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf9); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64field(&b, 2, 0, -abi->heaptype, 8);
		x64field(&b, 0, 1, -abi->heaptype, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xd0); x64guard(&b, 0x74);
		b.bytes[done]=b.n-done-1;
	}else if(p.kind == LeafProgramAddress){
		/* Materialize a Dis PC only after validating the module, index and native span. */
		if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
		   abi->linkmodule < 0 || abi->linkmodule > 2047 || abi->linkmodule%8 ||
		   abi->moduleprog < 0 || abi->moduleprog > 2047 || abi->moduleprog%8 ||
		   abi->modulenprog < 0 || abi->modulenprog > 2047 || abi->modulenprog%4 ||
		   abi->instbytes < 1 || abi->instbytes > 2047) return 0;
		x64ptr(&b, 0, abi->module); x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xc0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64field(&b, 0, 0, abi->linkmodule, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		x64field(&b, 1, 0, abi->modulenprog, 4);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
		x64byte(&b, 0x48); x64byte(&b, 0x69); x64byte(&b, 0xc0); x64word(&b, abi->instbytes);
		x64field(&b, 2, 11, abi->moduleprog, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xd0); x64guard(&b, 0x73);
		x64ptr(&b, 2, abi->d); x64store(&b, 0, 8);
	}else if(p.kind == LeafFixedCoefficient){
		/* Capture frame parameters and both operands before any aliased result write. */
		if(abi->fp < 0 || abi->fp > 2047 || abi->fp%8 || abi->fixedtmp < 0 || abi->fixedtmp > 2047 ||
		   abi->fixedcoef < 0 || abi->fixedcoef > 2047) return 0;
		x64ptr(&b, 1, abi->fp); x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xc8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64field(&b, 2, 9, abi->fixedcoef, abi->wordbytes);
		if(p.alu != 1){
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xd3);
		}
		if(p.alu == 2){x64ptr(&b, 1, abi->m); x64load(&b, 1, abi->wordbytes);}
		else x64field(&b, 1, 9, abi->fixedtmp, abi->wordbytes);
		if(p.condition){
			/* Capture both rounding flags before decoding the signed scale. */
			x64byte(&b, 0x89); x64byte(&b, 0xce); x64byte(&b, 0x83); x64byte(&b, 0xe6); x64byte(&b, 3);
			x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xf9); x64byte(&b, 2);
		}
		x64byte(&b, 0x48); x64byte(&b, 0x8d); x64byte(&b, 0x41); x64byte(&b, 63);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 127); x64guard(&b, 0x72);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		if(p.alu == 1){
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		}else if(p.alu == 0){x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc2);}
		if(p.alu != 2){x64ptr(&b, 0, abi->m); x64load(&b, 0, abi->wordbytes);}
		if(p.condition){
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc0);
			if(p.alu == 0){x64byte(&b, 0x49); x64byte(&b, 0x31); x64byte(&b, 0xd0);}
		}
		if(p.condition)x64fixedround(&b, p.alu, abi->wordbytes);
		if(p.alu != 2){x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0xaf); x64byte(&b, 0xc2);}
		if(p.condition){
			/* Mask selection precedes shifting and uses unmultiplied signs. */
			x64byte(&b, 0x40); x64byte(&b, 0xf6); x64byte(&b, 0xc6); x64byte(&b, 1); x64byte(&b, 0x74); done=b.n; x64byte(&b, 0);
			/* A scale guard is needed only when the original signs require a mask. */
			x64byte(&b, 0x4d); x64byte(&b, 0x85); x64byte(&b, 0xc0);
			x64byte(&b, p.alu == 1 ? 0x7f : 0x79); other=b.n; x64byte(&b, 0);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x78);
			x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xd9);
			x64byte(&b, 0x48); x64byte(&b, 0xc7); x64byte(&b, 0xc2); x64word(&b, -1);
			x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xe2);
			x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xd2);
			x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xd9);
			x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xd0);
			b.bytes[done]=b.n-done-1; b.bytes[other]=b.n-other-1;
		}
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9);
		x64byte(&b, 0x78); x64byte(&b, 5); x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xe0);
		x64byte(&b, 0xeb); x64byte(&b, 6); x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xd9); x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xf8);
		if(p.condition && p.alu != 1){x64byte(&b, 0x4c); x64byte(&b, 0x01); x64byte(&b, 0xc8);}
		x64signeddivide(&b, 8);
		if(p.condition && p.alu == 1){x64byte(&b, 0x4c); x64byte(&b, 0x01); x64byte(&b, 0xc8);}
		x64ptr(&b, 2, abi->d); x64store(&b, 0, abi->wordbytes);
	}else if(p.kind == LeafCall){
		/* Capture the target and old links before touching an aliased frame. */
		if(abi->fp < 0 || abi->fp > 2047 || abi->fp%8 ||
		   abi->framelr < 0 || abi->framelr > 2047 || abi->framelr%8 ||
		   abi->framefp < 0 || abi->framefp > 2047 || abi->framefp%8 ||
		   abi->framelr == abi->framefp) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64ptr(&b, 1, abi->d); x64load(&b, 1, 8);
		x64ptr(&b, 0, abi->pc); x64ptr(&b, 1, abi->fp);
		x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0x80); x64word(&b, abi->framelr);
		x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0x88); x64word(&b, abi->framefp);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x87); x64word(&b, abi->fp);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x8f); x64word(&b, abi->pc);
	}else if(p.kind == LeafDescriptorIndex){
		/* Positive strides have bounded products; empty elements retain any backing identity. */
		if(abi->arraylen < 0 || abi->arraylen > 2047 || abi->arraydata < 0 || abi->arraydata > 2047 ||
		   abi->arraytype < 0 || abi->arraytype > 2047 || abi->typesize < 0 || abi->typesize > 2047 ||
		   (abi->arraylen|abi->arraydata|abi->arraytype)%8 || abi->typesize%4) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		x64field(&b, 1, 0, abi->arraylen, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
		x64ptr(&b, 0, abi->d); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
		x64field(&b, 2, 11, abi->arraytype, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64field(&b, 1, 2, abi->typesize, 4);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
		x64field(&b, 2, 11, abi->arraydata, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9);
		x64byte(&b, 0x74); empty=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x3d); x64word(&b, 0x7fffffff); x64guard(&b, 0x76);
		x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0xaf); x64byte(&b, 0xc1);
		x64byte(&b, 0x48); x64byte(&b, 0x3d); x64word(&b, 0x7fffffff); x64guard(&b, 0x76);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xd0); x64guard(&b, 0x73);
		x64byte(&b, 0xeb); done=b.n; x64byte(&b, 0);
		b.bytes[empty]=b.n-empty-1;
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xd0);
		b.bytes[done]=b.n-done-1;
		x64ptr(&b, 2, abi->m); x64store(&b, 0, abi->wordbytes);
	}else if(p.kind == LeafFixedFrame){
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
			/* Reject zero before loading the dividend; -1 uses modular native negation. */
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
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
		if(p.alu){ x64signeddivide(&b, 8); }
		x64ptr(&b, 2, abi->d); x64store(&b, 0, abi->wordbytes);
	}else if(p.kind == LeafRealResize){
		/* Native conversion preserves the host C rounding and sticky FP flags. */
		x64ptr(&b, 0, abi->s);
		x64byte(&b, p.input == 4 ? 0xf3 : 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x10); x64byte(&b, 0x00);
		x64byte(&b, p.input == 4 ? 0xf3 : 0xf2); x64byte(&b, 0x0f); x64byte(&b, 0x5a); x64byte(&b, 0xc0);
		x64ptr(&b, 2, abi->d);
		x64byte(&b, p.bytes == 4 ? 0xf3 : 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x11); x64byte(&b, 0x02);
	}else if(p.kind == LeafIntegerReal){
		/* Convert the complete signed input, including rounded magnitudes above 2^53. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, p.input);
		x64byte(&b, 0xf2); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x2a); x64byte(&b, 0xc0);
		x64ptr(&b, 2, abi->d);
		x64byte(&b, 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x11); x64byte(&b, 0x02);
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
		/* Negative exponents need only zero/one tests and parity, never negation. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64ptr(&b, 1, abi->m); x64load(&b, 1, p.bytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
		x64byte(&b, 0x79); scale=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x75);
		x64byte(&b, 0xba); x64word(&b, 1);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf9); x64byte(&b, 1);
		x64byte(&b, 0x74); empty=b.n; x64byte(&b, 0);
		x64byte(&b, 0x31); x64byte(&b, 0xd2);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf9); x64byte(&b, 0xff);
		x64byte(&b, 0x75); other=b.n; x64byte(&b, 0);
		x64byte(&b, 0x89); x64byte(&b, 0xc2); x64byte(&b, 0x83); x64byte(&b, 0xe2); x64byte(&b, 1);
		x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xda);
		x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xd2);
		x64byte(&b, 0x48); x64byte(&b, 0xff); x64byte(&b, 0xc2);
		b.bytes[empty]=b.n-empty-1; b.bytes[other]=b.n-other-1;
		x64ptr(&b, 2, abi->d); x64store(&b, 2, p.bytes);
		x64byte(&b, 0xb8); x64word(&b, 1); x64byte(&b, 0xc3);
		b.bytes[scale]=b.n-scale-1;
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
			x64field(&b, 1, 0, abi->arraylen, abi->wordbytes);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
			x64ptr(&b, 0, abi->d); x64load(&b, 0, abi->wordbytes);
			x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
			scale = p.bytes == 1 ? 0 : p.bytes == 4 ? 2 : 3;
			x64byte(&b, 0x48); x64byte(&b, 0x3d); x64word(&b, 0x7fffffff>>scale); x64guard(&b, 0x76);
			x64field(&b, 1, 11, abi->arraydata, 8);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x75);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf9); x64byte(&b, 0xff); x64guard(&b, 0x75);
			if(scale){ x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe0); x64byte(&b, scale); }
			x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xc8); x64guard(&b, 0x73);
			x64ptr(&b, 2, abi->m); x64store(&b, 0, abi->wordbytes);
		}
	}else if(p.kind == LeafRealPower){
		/* Match the C squaring order and reciprocal, including minimum WORD. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64ptr(&b, 1, abi->m);
		x64byte(&b, 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x10); x64byte(&b, 0x09);
		x64byte(&b, 0x48); x64byte(&b, 0xba); x64word(&b, 0); x64word(&b, 0x3ff00000);
		x64byte(&b, 0x66); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x6e); x64byte(&b, 0xc2);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
		x64byte(&b, 0x79); done=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xd8);
		x64byte(&b, 0x66); x64byte(&b, 0x0f); x64byte(&b, 0x28); x64byte(&b, 0xd0);
		x64byte(&b, 0xf2); x64byte(&b, 0x0f); x64byte(&b, 0x5e); x64byte(&b, 0xd1);
		x64byte(&b, 0x66); x64byte(&b, 0x0f); x64byte(&b, 0x28); x64byte(&b, 0xca);
		b.bytes[done]=b.n-done-1;
		loop=b.n;
		x64byte(&b, 0xa8); x64byte(&b, 1);
		x64byte(&b, 0x74); x64byte(&b, 4);
		x64byte(&b, 0xf2); x64byte(&b, 0x0f); x64byte(&b, 0x59); x64byte(&b, 0xc1);
		x64byte(&b, 0x48); x64byte(&b, 0xd1); x64byte(&b, 0xe8);
		x64byte(&b, 0x74); done=b.n; x64byte(&b, 0);
		x64byte(&b, 0xf2); x64byte(&b, 0x0f); x64byte(&b, 0x59); x64byte(&b, 0xc9);
		x64byte(&b, 0xeb); x64byte(&b, loop-b.n-1);
		b.bytes[done]=b.n-done-1;
		x64ptr(&b, 2, abi->d);
		x64byte(&b, 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x11); x64byte(&b, 0x02);
	}else if(p.kind == LeafRealInteger){
		/* Guard bits before any FP effects; rejected leaves must preserve FP status.
		 * The symmetric bounds leave room for the half-unit addition in every FP mode. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		if(p.alu){
			x64ptr(&b, 1, abi->m); x64load(&b, 1, 8);
			x64byte(&b, 0x48); x64byte(&b, 0xd1); x64byte(&b, 0xe1);
			x64byte(&b, 0x48); x64byte(&b, 0xba); x64word(&b, 0); x64word(&b, 0x7fe00000);
			x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xd1); x64guard(&b, 0x74);
		}
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc1);
		x64byte(&b, 0x48); x64byte(&b, 0xd1); x64byte(&b, 0xe1);
		x64byte(&b, 0x48); x64byte(&b, 0xba); x64word(&b, p.bytes == 4 ? 0xffc00000U : 0xfffffffeU); x64word(&b, p.bytes == 4 ? 0x83bfffffU : 0x87bfffffU);
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xd1); x64guard(&b, 0x72);
		x64byte(&b, 0x66); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x6e); x64byte(&b, 0xc0);
		if(p.alu){
			x64byte(&b, 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x59); x64byte(&b, 0x01);
			/* Choose rounding from the product, including its signed-zero result. */
			x64byte(&b, 0x66); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x7e); x64byte(&b, 0xc0);
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc1);
			x64byte(&b, 0x48); x64byte(&b, 0xd1); x64byte(&b, 0xe1);
		}
		/* Negative zero takes the positive half, matching the C less-than test. */
		x64byte(&b, 0x48); x64byte(&b, 0xba); x64word(&b, 0); x64word(&b, 0x3fe00000);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
		x64byte(&b, 0x79); done=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9);
		x64byte(&b, 0x74); other=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0xba); x64byte(&b, 0xfa); x64byte(&b, 63);
		b.bytes[done]=b.n-done-1; b.bytes[other]=b.n-other-1;
		x64byte(&b, 0x66); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x6e); x64byte(&b, 0xca);
		x64byte(&b, 0xf2); x64byte(&b, 0x0f); x64byte(&b, 0x58); x64byte(&b, 0xc1);
		x64byte(&b, 0xf2); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x2c); x64byte(&b, 0xc0);
		x64ptr(&b, 2, abi->d); x64store(&b, 0, p.bytes);
	}else if(p.kind == LeafRealArithmetic){
		/* Snapshot both operands before an overlapping destination store.
		 * Scalar SSE2 inherits the C host's rounding mode and sticky exceptions. */
		x64ptr(&b, 0, abi->m);
		x64byte(&b, 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x10); x64byte(&b, 0x00);
		x64ptr(&b, 1, abi->s);
		if(p.alu == 4){
			/* CVTXF converts signed WORD before the separate real multiplication. */
			x64byte(&b, 0x49); x64byte(&b, abi->wordbytes == 4 ? 0x63 : 0x8b); x64byte(&b, 0x01);
			x64byte(&b, 0xf2); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x2a); x64byte(&b, 0xc8);
		}else{
			x64byte(&b, 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x10); x64byte(&b, 0x09);
		}
		x64byte(&b, 0xf2); x64byte(&b, 0x0f);
		x64byte(&b, p.alu == 0 ? 0x58 : p.alu == 1 ? 0x5c : p.alu == 3 ? 0x5e : 0x59); x64byte(&b, 0xc1);
		x64ptr(&b, 2, abi->d);
		x64byte(&b, 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x11); x64byte(&b, 0x02);
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
			x64signeddivide(&b, p.bytes);
			x64ptr(&b, 2, abi->d); x64store(&b, p.alu ? 2 : 0, p.bytes);
		}
	}else if(p.kind == LeafRealBranch){
		/* Read the branch target only after an ordered hardware comparison takes it. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8); x64realguard(&b, 0);
		x64ptr(&b, 1, abi->m); x64load(&b, 1, 8); x64realguard(&b, 1);
		x64byte(&b, 0x66); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x6e); x64byte(&b, 0xc0);
		x64byte(&b, 0x66); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x6e); x64byte(&b, 0xc9);
		x64byte(&b, 0x66); x64byte(&b, 0x0f); x64byte(&b, 0x2e); x64byte(&b, 0xc1);
		x64byte(&b, realinverse[p.condition]); patch=b.n; x64byte(&b, 0);
		x64ptr(&b, 2, abi->d);
		x64byte(&b, 0x49); x64byte(&b, 0x8b); x64byte(&b, 0x02);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x87); x64word(&b, abi->pc);
		b.bytes[patch]=b.n-patch-1;
	}else if(p.kind == LeafAlu || p.kind == LeafBranch){
		x64ptr(&b, 0, abi->s); x64ptr(&b, 1, abi->m);
		x64load(&b, 0, p.bytes);
		x64load(&b, 1, p.bytes);
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
