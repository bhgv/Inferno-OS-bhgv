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
	x64byte(b, 0x48 | (owner >= 8) | (result >= 8 ? 4 : 0));
	if(bytes == 1){ x64byte(b, 0x0f); x64byte(b, 0xb6); }
	else x64byte(b, bytes == 8 ? 0x8b : 0x63);
	x64byte(b, 0x80 | (result&7)<<3 | (owner&7));
	x64word(b, offset);
}


/* Resolve either string representation, retaining a native character stride. */
static void
x64stringheader(X64Buf *b, int owner, int length, int stride, int lenoff, int maxoff, int dataoff)
{
	int empty, positive, done;

	x64byte(b, 0x41); x64byte(b, 0xb8+stride); x64word(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0+owner*9); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8+owner); x64byte(b, 0xff);
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64field(b, length, owner, lenoff, 4);
	x64byte(b, 0x85); x64byte(b, 0xc0+length*9);
	x64byte(b, 0x79); positive=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0xf7); x64byte(b, 0xd8+length);
	x64byte(b, 0x41); x64byte(b, 0xb8+stride); x64word(b, 4);
	b->bytes[positive]=b->n-positive-1;
	/* Signed capacity bounds also reject the absolute value of INT_MIN. */
	x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x90+owner); x64word(b, maxoff);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xd0+length); x64guard(b, 0x7e);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xc0+owner); x64word(b, dataoff);
	x64byte(b, 0xeb); done=b->n; x64byte(b, 0);
	b->bytes[empty]=b->n-empty-1;
	x64byte(b, 0x31); x64byte(b, 0xc0+length*9);
	b->bytes[done]=b->n-done-1;
}

/* Validate a power scale without FP effects: rax is source, rcx is scale/product magnitude. */
static void
x64realproduct(X64Buf *b)
{
	int unit;

	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xca);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe2); x64byte(b, 12);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x74);
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe1);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe9); x64byte(b, 53);
	x64byte(b, 0x81); x64byte(b, 0xe9); x64word(b, 1023);
	x64byte(b, 0x8d); x64byte(b, 0x51); x64byte(b, 16);
	x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 32); x64guard(b, 0x76);
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x74); unit=b->n; x64byte(b, 0);
	/* Non-unit scales require normal input and normal exact product exponents. */
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2);
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe2);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xea); x64byte(b, 53);
	x64byte(b, 0x8d); x64byte(b, 0x72); x64byte(b, 0xff);
	x64byte(b, 0x81); x64byte(b, 0xfe); x64word(b, 2045); x64guard(b, 0x76);
	x64byte(b, 0x01); x64byte(b, 0xca);
	x64byte(b, 0x83); x64byte(b, 0xea); x64byte(b, 1);
	x64byte(b, 0x81); x64byte(b, 0xfa); x64word(b, 2045); x64guard(b, 0x76);
	b->bytes[unit]=b->n-unit-1;
	x64byte(b, 0x48); x64byte(b, 0x63); x64byte(b, 0xc9);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe1); x64byte(b, 52);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe1);
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

/* Prefer short private-layout displacements without restricting future native field offsets. */
static void
x64caseptr(X64Buf *b, int reg, int offset)
{
	if(offset > 127){
		x64ptr(b, reg, offset);
		return;
	}
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x47+reg*8); x64byte(b, offset);
}

/* Read signed table words or native metadata with the shortest available displacement. */
static void
x64casefield(X64Buf *b, int result, int owner, int offset, int bytes)
{
	if(offset < -128 || offset > 127){
		x64field(b, result, owner, offset, bytes);
		return;
	}
	x64byte(b, owner >= 8 ? 0x49 : 0x48); x64byte(b, bytes == 8 ? 0x8b : 0x63);
	x64byte(b, 0x40 | result<<3 | (owner&7)); x64byte(b, offset);
}

/* Defer rejection to one of two nearby returns, preserving short conditional transfers. */
static void
x64caseguard(X64Buf *b, int success, int *patch, int *count)
{
	x64byte(b, success^1);
	patch[(*count)++]=b->n;
	x64byte(b, 0);
}

/* Search fixed-width numeric tables without helper calls or VM writes before target validation. */
static int
x64case(X64Buf *b, LeafLayout *abi, LeafOp *op)
{
	int reject[32], count, first, last, i, displacement;
	int loop, empty, left, right, found, leftlabel, rightlabel, done;
	int header, stride;

	if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
	   abi->linkmodule < 0 || abi->linkmodule > 2047 || abi->linkmodule%8 ||
	   abi->linkcompiled < 0 || abi->linkcompiled > 2047 || abi->linkcompiled%4 ||
	   abi->moduleprog < 0 || abi->moduleprog > 2047 || abi->moduleprog%8 ||
	   abi->modulenprog < 0 || abi->modulenprog > 2047 || abi->modulenprog%4 ||
	   abi->instbytes < 1 || abi->instbytes > 2047)
		return 0;
	header=op->alu ? 8 : 4; stride=op->alu ? 24 : 12; count=0;
	/* Rejection islands cover both ends without near branches or a larger code buffer. */
	x64byte(b, 0xeb); x64byte(b, 3); first=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, abi->module);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x40); x64byte(b, 0xff);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xfd); x64caseguard(b, 0x76, reject, &count);
	x64casefield(b, 2, 8, abi->linkcompiled, 4);
	x64byte(b, 0x85); x64byte(b, 0xd2); x64caseguard(b, 0x74, reject, &count);
	x64casefield(b, 0, 8, abi->linkmodule, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0xff);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xfd); x64caseguard(b, 0x76, reject, &count);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc3); /* r11=module */
	x64caseptr(b, 0, abi->s); x64load(b, 0, op->bytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc6); /* rsi=value */
	x64caseptr(b, 2, abi->d);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x52); x64byte(b, 0xff);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xfb); x64caseguard(b, 0x76, reject, &count);
	x64casefield(b, 1, 10, 0, 4);
	x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 4095); x64caseguard(b, 0x76, reject, &count);
	x64byte(b, 0x48); x64byte(b, 0x6b); x64byte(b, 0xc1); x64byte(b, stride);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc0); x64byte(b, header);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd2);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc2); x64caseguard(b, 0x73, reject, &count);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xfc); x64caseguard(b, 0x76, reject, &count);
	x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x0a); /* r9=default */
	x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x42); x64byte(b, header); /* r8=rows */
	loop=b->n;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc8);
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe8);
	x64byte(b, 0x48); x64byte(b, 0x6b); x64byte(b, 0xc0); x64byte(b, stride);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x14); x64byte(b, 0x00);
	x64byte(b, 0x48); x64byte(b, op->alu ? 0x8b : 0x63); x64byte(b, 0x02);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc6);
	x64byte(b, 0x7c); left=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, op->alu ? 0x8b : 0x63); x64byte(b, 0x42); x64byte(b, op->alu ? 8 : 4);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc6);
	x64byte(b, 0x7d); right=b->n; x64byte(b, 0);
	x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x4a); x64byte(b, op->alu ? 16 : 8);
	x64byte(b, 0xeb); found=b->n; x64byte(b, 0);
	leftlabel=b->n;
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe9);
	x64byte(b, 0xeb); x64byte(b, loop-b->n-1);
	rightlabel=b->n;
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc8);
	x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe8);
	x64byte(b, 0x48); x64byte(b, 0x29); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9);
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x42); x64byte(b, stride);
	x64byte(b, 0xeb); x64byte(b, loop-b->n-1);
	done=b->n;
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[empty]=done-empty-1; b->bytes[found]=done-found-1;
	b->bytes[left]=leftlabel-left-1; b->bytes[right]=rightlabel-right-1;
	x64casefield(b, 2, 11, abi->modulenprog, 4);
	x64byte(b, 0x85); x64byte(b, 0xd2); x64caseguard(b, 0x7f, reject, &count);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xd1); x64caseguard(b, 0x72, reject, &count);
	x64casefield(b, 0, 11, abi->moduleprog, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0xff);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xfd); x64caseguard(b, 0x76, reject, &count);
	x64byte(b, 0x4d); x64byte(b, 0x69); x64byte(b, 0xc9); x64word(b, abi->instbytes);
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xc8); x64caseguard(b, 0x73, reject, &count);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, abi->pc);
	x64byte(b, 0xeb); x64byte(b, 3); last=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	if(b->n > sizeof(b->bytes)) return 0;
	for(i=0;i<count;i++){
		displacement=first-reject[i]-1;
		if(displacement < -128 || displacement > 127) displacement=last-reject[i]-1;
		if(displacement < -128 || displacement > 127) return 0;
		b->bytes[reject[i]]=displacement;
	}
	return 1;
}

/* Copy and mark live owners into empty or identical slots without helper calls. */
static int
x64owner(X64Buf *b, LeafLayout *abi, LeafOp *p)
{
	unsigned long long color;
	int offset, empty, same, marked;

	if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
	   abi->heapref <= 0 || abi->heapref > 255 || abi->heapref%8 ||
	   abi->heapcolor <= 0 || abi->heapcolor > 255 || abi->heapcolor%4 ||
	   abi->refbytes != 8 || abi->mutator == nil || abi->nprop == nil ||
	   abi->markcolor < 0 || abi->markcolor > 255)
		return 0;
	offset=p->alu == 2 ? abi->listdata : abi->listtail;
	if(p->alu >= 2 && (offset < 0 || offset > 2047))
		return 0;
	if(p->alu == 1){
		x64ptr(b, 0, abi->module);
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0);
	}else{
		x64ptr(b, 0, abi->s); x64load(b, 0, 8);
	}
	if(p->alu >= 2){
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64guard(b, 0x75);
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff); x64guard(b, 0x75);
		x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x80); x64word(b, offset);
		x64load(b, 0, 8);
	}
	x64ptr(b, 2, abi->d);
	x64byte(b, 0x4d); x64byte(b, 0x8b); x64byte(b, 0x0a); /* r9=previous owner */
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
	x64byte(b, 0x74); same=b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xc1); x64guard(b, 0x74);
	b->bytes[same]=b->n-same-1;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
	empty=-1;
	if(p->alu == 1)
		x64guard(b, 0x75);
	else{
		x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	}
	x64field(b, 1, 0, -abi->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff); x64guard(b, 0x75);
	/* An identical owner needs no retain/release pair, but must pass the same guards. */
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
	x64byte(b, 0x75); same=b->n; x64byte(b, 0);
	/* No fallible checks remain after the first owner mutation. */
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x88); x64word(b, -abi->heapref);
	b->bytes[same]=b->n-same-1;
	x64field(b, 2, 0, -abi->heapcolor, 4);
	color=(unsigned long long)abi->mutator;
	x64byte(b, 0x49); x64byte(b, 0xbb); x64word(b, color); x64word(b, color>>32);
	x64byte(b, 0x41); x64byte(b, 0x3b); x64byte(b, 0x13); x64byte(b, 0x74); marked=b->n; x64byte(b, 0);
	/* Match Setmark's two native int stores, retaining existing nprop on no-mark paths. */
	x64byte(b, 0xc7); x64byte(b, 0x80); x64word(b, -abi->heapcolor); x64word(b, abi->markcolor);
	color=(unsigned long long)abi->nprop;
	x64byte(b, 0x49); x64byte(b, 0xbb); x64word(b, color); x64word(b, color>>32);
	x64byte(b, 0x41); x64byte(b, 0xc7); x64byte(b, 0x03); x64word(b, 1);
	b->bytes[marked]=b->n-marked-1;
	if(empty >= 0)
		b->bytes[empty]=b->n-empty-1;
	if(p->alu >= 2){
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, abi->s);
	}
	x64store(b, 0, 8);
	return 1;
}

/* Share a nearby rejection return while keeping every guard before copy publication. */
static int
x64mappedguard(X64Buf *b, int success, int fallback)
{
	int distance=fallback-b->n-2;
	if(distance < -128 || distance > 127)
		return 0;
	x64byte(b, success^1); x64byte(b, distance);
	return 1;
}

/* Copy small pointer-free records through the live Module.type table, never its cache. */
static int
x64mapped(X64Buf *b, LeafLayout *a, LeafOp *p)
{
	int fallback, empty, forward, loop;

	if(a->module < 0 || a->module > 2047 || a->module%8 ||
	   a->linkmodule < 0 || a->linkmodule > 2047 || a->linkmodule%8 ||
	   a->moduletypes < 0 || a->moduletypes > 2047 || a->moduletypes%8 ||
	   a->modulenty < 0 || a->modulenty > 2047 || a->modulenty%4 ||
	   a->typesize < 0 || a->typesize > 2047 || a->typesize%4 ||
	   a->typenp < 0 || a->typenp > 2047 || a->typenp%4 ||
	   (p->alu && (a->listdata < 0 || a->listdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8)))
		return 0;
	x64byte(b, 0xeb); x64byte(b, 3); fallback=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	if(p->alu){
		x64caseptr(b, 0, a->s); x64load(b, 0, 8);
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
		if(!x64mappedguard(b, 0x75, fallback)) return 0;
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
		if(!x64mappedguard(b, 0x75, fallback)) return 0;
		x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc3);
	}
	x64caseptr(b, 0, a->module);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64casefield(b, 0, 0, a->linkmodule, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64casefield(b, 1, 0, a->moduletypes, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64casefield(b, 2, 0, a->modulenty, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2);
	if(!x64mappedguard(b, 0x7f, fallback)) return 0;
	/* A second return bounds backward displacements even for larger native layouts. */
	x64byte(b, 0xeb); x64byte(b, 3); fallback=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->m); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd0);
	if(!x64mappedguard(b, 0x72, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe0); x64byte(b, 3);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc1);
	if(!x64mappedguard(b, 0x73, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x09);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64casefield(b, 0, 1, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, 256);
	if(!x64mappedguard(b, 0x76, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64casefield(b, 2, 1, a->typenp, 4);
	x64byte(b, 0x85); x64byte(b, 0xd2);
	if(!x64mappedguard(b, 0x74, fallback)) return 0;
	if(p->alu){
		x64casefield(b, 1, 11, -a->heapref, 8);
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
		if(!x64mappedguard(b, 0x75, fallback)) return 0;
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
		if(!x64mappedguard(b, 0x75, fallback)) return 0;
		x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x83); x64word(b, a->listdata);
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, a->s);
	}else
		x64caseptr(b, 0, a->s);
	x64caseptr(b, 2, a->d);
	/* Select a byte stride so overlap uses memmove order without changing DF. */
	x64byte(b, 0xbe); x64word(b, 1);
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc2);
	x64byte(b, 0x76); forward=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0xc7); x64byte(b, 0xc6); x64word(b, -1);
	x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x44); x64byte(b, 0x00); x64byte(b, 0xff);
	x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x54); x64byte(b, 0x02); x64byte(b, 0xff);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[forward]=b->n-forward-1;
	loop=b->n;
	x64byte(b, 0x41); x64byte(b, 0x8a); x64byte(b, 0x08);
	x64byte(b, 0x41); x64byte(b, 0x88); x64byte(b, 0x0a);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xf0);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xf2);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc8);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[empty]=b->n-empty-1;
	return 1;
}

/* Return success when the preceding comparison identifies an empty operation. */
static void
x64sliceempty(X64Buf *b, int notempty)
{
	x64byte(b, notempty); x64byte(b, 6);
	x64byte(b, 0xb8); x64word(b, 1); x64byte(b, 0xc3);
}

/* Copy short byte slices with complete geometry/span guards and no owner helpers. */
static int
x64slicecopy(X64Buf *b, LeafLayout *a)
{
	int fallback, forward, loop, identical;

	if(a->arraylen < 0 || a->arraylen > 2047 || a->arraylen%a->wordbytes ||
	   a->arraydata < 0 || a->arraydata > 2047 || a->arraydata%8 ||
	   a->arraytype < 0 || a->arraytype > 2047 || a->arraytype%8 ||
	   a->typesize < 0 || a->typesize > 2047 || a->typesize%4 ||
	   a->typenp < 0 || a->typenp > 2047 || a->typenp%4)
		return 0;
	x64byte(b, 0xeb); x64byte(b, 3); fallback=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->s); x64load(b, 0, 8);
	/* Adding one maps C null and H onto the unsigned interval [0,1]. */
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64sliceempty(b, 0x77);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc0);
	x64caseptr(b, 1, a->d); x64load(b, 1, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc9);
	x64casefield(b, 0, 8, a->arraylen, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, 128);
	if(!x64mappedguard(b, 0x76, fallback)) return 0;
	x64caseptr(b, 2, a->m); x64load(b, 2, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xd6); /* rsi=index */
	x64casefield(b, 1, 9, a->arraylen, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	if(!x64mappedguard(b, 0x79, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xca);
	if(!x64mappedguard(b, 0x76, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x29); x64byte(b, 0xd1);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc8);
	if(!x64mappedguard(b, 0x76, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64sliceempty(b, 0x75);
	x64byte(b, 0xeb); x64byte(b, 3); fallback=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* index is nonnegative and bounded by a signed WORD destination length. */
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc2);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xea); x64byte(b, 31);
	x64byte(b, 0x85); x64byte(b, 0xd2);
	if(!x64mappedguard(b, 0x74, fallback)) return 0;
	x64casefield(b, 2, 9, a->arraytype, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xff);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64casefield(b, 1, 2, a->typesize, 4);
	x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1);
	if(!x64mappedguard(b, 0x74, fallback)) return 0;
	x64casefield(b, 1, 2, a->typenp, 4);
	x64byte(b, 0x85); x64byte(b, 0xc9);
	if(!x64mappedguard(b, 0x74, fallback)) return 0;
	x64casefield(b, 1, 8, a->arraydata, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc8);
	x64casefield(b, 1, 9, a->arraydata, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	if(!x64mappedguard(b, 0x75, fallback)) return 0;
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xca);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xf2);
	if(!x64mappedguard(b, 0x73, fallback)) return 0;
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc2);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc2);
	if(!x64mappedguard(b, 0x73, fallback)) return 0;
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd2);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc2);
	if(!x64mappedguard(b, 0x73, fallback)) return 0;
	/* An explicit signed stride gives memmove order while preserving DF. */
	x64byte(b, 0xbe); x64word(b, 1);
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc2);
	/* All span guards precede the no-store path for identical physical ranges. */
	x64byte(b, 0x74); identical=b->n; x64byte(b, 0);
	x64byte(b, 0x76); forward=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xce); x64byte(b, 0xff);
	x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x44); x64byte(b, 0x00); x64byte(b, 0xff);
	x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x54); x64byte(b, 0x02); x64byte(b, 0xff);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[forward]=b->n-forward-1;
	loop=b->n;
	x64byte(b, 0x41); x64byte(b, 0x8a); x64byte(b, 0x08);
	x64byte(b, 0x41); x64byte(b, 0x88); x64byte(b, 0x0a);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xf0);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xf2);
	x64byte(b, 0xff); x64byte(b, 0xc8);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[identical]=b->n-identical-1;
	return 1;
}


/* Load a scalar field with a checked short displacement and low register numbers. */
static void
x64smallfield(X64Buf *b, int result, int owner, int offset, int bytes)
{
	x64byte(b, 0x48);
	x64byte(b, bytes == 8 ? 0x8b : 0x63);
	x64byte(b, 0x40 | result<<3 | owner);
	x64byte(b, offset);
}


/* Share a nearby rejection return while short branch reach permits it. */
static void
x64moduleguard(X64Buf *b, int success, int fallback)
{
	int distance;

	distance = fallback-b->n-2;
	if(distance >= -128 && distance <= 127){
		x64byte(b, success^1);
		x64byte(b, distance);
	}else
		x64guard(b, success);
}


/* Concatenate matching string representations within unique capacity and bounded copy size. */
static int
x64stringappend(X64Buf *b, LeafLayout *a)
{
	int overlap, empty, loop, narrow, fallback;

	if(a->stringlen < 0 || a->stringlen > 127 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 127 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 127 ||
	   a->heapref <= 0 || a->heapref > 128 || a->heapref%8 || a->refbytes != 8)
		return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64ptr(b, 0, a->m); x64ptr(b, 1, a->d);
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc8); x64moduleguard(b, 0x74, fallback);
	x64load(b, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2);
	x64ptr(b, 0, a->s); x64load(b, 0, 8);
	/* owner+1 must be above one: this rejects both C null and language nil. */
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x42); x64byte(b, 1);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x40); x64byte(b, 1);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0x7a); x64byte(b, -a->heapref); x64byte(b, 1); x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0x78); x64byte(b, -a->heapref); x64byte(b, 0); x64moduleguard(b, 0x75, fallback);
	x64smallfield(b, 1, 2, a->stringlen, 4);
	/* Keep a native sign mask while taking the absolute character count. */
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc9);
	x64byte(b, 0x49); x64byte(b, 0xc1); x64byte(b, 0xf9); x64byte(b, 63);
	x64byte(b, 0x4c); x64byte(b, 0x31); x64byte(b, 0xc9);
	x64byte(b, 0x4c); x64byte(b, 0x29); x64byte(b, 0xc9);
	x64smallfield(b, 6, 0, a->stringlen, 4);
	/* Mixed representations retain the allocating/widening C implementation. */
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xf0);
	x64byte(b, 0x4d); x64byte(b, 0x31); x64byte(b, 0xc8);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x79, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x31); x64byte(b, 0xce);
	x64byte(b, 0x4c); x64byte(b, 0x29); x64byte(b, 0xce);
	x64byte(b, 0x81); x64byte(b, 0xfe); x64word(b, 256); x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x40); x64byte(b, a->stringmax);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xc6); x64moduleguard(b, 0x7e, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x52); x64byte(b, a->stringmax);
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x1c); x64byte(b, 0x31);
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xd3); x64moduleguard(b, 0x7e, fallback);
	/* Capacity remains in characters; only the copy geometry uses byte counts. */
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); narrow = b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe1); x64byte(b, 2);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe6); x64byte(b, 2);
	x64byte(b, 0x49); x64byte(b, 0xf7); x64byte(b, 0xdb);
	b->bytes[narrow] = b->n-narrow-1;
	/* The signed capacity comparison bounds the sum by INT_MAX before any store. */
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x4c); x64byte(b, 0x0a); x64byte(b, a->stringdata);
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x40); x64byte(b, a->stringdata);
	x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x14); x64byte(b, 0x30);
	/* Each full span includes the inline offset, so a wrapped base also rejects. */
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xc2); // cmp r10,rax: source end versus owner.
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x04); x64byte(b, 0x31); // r8=destination end.
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xd0); // cmp r8,rdx.
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x40); x64byte(b, a->stringdata); // Restore source base.
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xc1);
	x64byte(b, 0x76); overlap = b->n; x64byte(b, 0);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xd1); x64moduleguard(b, 0x73, fallback);
	b->bytes[overlap] = b->n-overlap-1;
	x64byte(b, 0x85); x64byte(b, 0xf6);
	x64byte(b, 0x74); empty = b->n; x64byte(b, 0);
	loop = b->n;
	x64byte(b, 0x45); x64byte(b, 0x0f); x64byte(b, 0xb6); x64byte(b, 0x10);
	x64byte(b, 0x44); x64byte(b, 0x88); x64byte(b, 0x11);
	x64byte(b, 0x49); x64byte(b, 0xff); x64byte(b, 0xc0);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1);
	x64byte(b, 0xff); x64byte(b, 0xce);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes))
		return 0;
	b->bytes[empty] = b->n-empty-1;
	x64byte(b, 0x44); x64byte(b, 0x89); x64byte(b, 0x5a); x64byte(b, a->stringlen);
	return 1;
}


/* Parse short signed decimal ASCII without helpers, caches or partial stores. */
static int
x64decimal(X64Buf *b, LeafLayout *a, LeafOp *p)
{
	int absent, empty, minus, digits, loop, positive, i;
	unsigned int distance;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringdata < 0 || a->stringdata > 2047) return 0;
	x64ptr(b, 0, a->s); x64load(b, 0, 8);
	x64byte(b, 0x45); x64byte(b, 0x31); x64byte(b, 0xc0); /* r8d=value */
	x64byte(b, 0x45); x64byte(b, 0x31); x64byte(b, 0xc9); /* r9d=negative */
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
	x64byte(b, 0x0f); x64byte(b, 0x84); absent=b->n; x64word(b, 0);
	x64field(b, 1, 0, a->stringlen, 4);
	x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, p->input+1); x64guard(b, 0x76);
	x64field(b, 2, 0, a->stringmax, 4);
	x64byte(b, 0x39); x64byte(b, 0xca); x64guard(b, 0x7d);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0xb0); x64word(b, a->stringdata);
	/* string2c would terminate ASCII; decline if that would change VM bytes. */
	x64byte(b, 0x80); x64byte(b, 0x3c); x64byte(b, 0x0e); x64byte(b, 0); x64guard(b, 0x74);
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64byte(b, 0x0f); x64byte(b, 0xb6); x64byte(b, 0x16);
	x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, '-');
	x64byte(b, 0x41); x64byte(b, 0x0f); x64byte(b, 0x94); x64byte(b, 0xc1);
	x64byte(b, 0x74); minus=b->n; x64byte(b, 0);
	x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, '+');
	x64byte(b, 0x75); digits=b->n; x64byte(b, 0);
	b->bytes[minus]=b->n-minus-1;
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc6);
	x64byte(b, 0xff); x64byte(b, 0xc9);
	b->bytes[digits]=b->n-digits-1;
	x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, p->input); x64guard(b, 0x76);
	/* strtod rejects sign-only input; leave that case to its existing C path. */
	if(p->alu){
		x64byte(b, 0x85); x64byte(b, 0xc9); x64guard(b, 0x75);
	}
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x74); positive=b->n; x64byte(b, 0);
	loop=b->n;
	x64byte(b, 0x0f); x64byte(b, 0xb6); x64byte(b, 0x16);
	x64byte(b, 0x83); x64byte(b, 0xea); x64byte(b, '0');
	x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 9); x64guard(b, 0x76);
	x64byte(b, 0x4d); x64byte(b, 0x6b); x64byte(b, 0xc0); x64byte(b, 10);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xd0);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc6);
	x64byte(b, 0xff); x64byte(b, 0xc9);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	x64byte(b, 0x45); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x74); minus=b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0xf7); x64byte(b, 0xd8);
	if(b->n > sizeof(b->bytes)) return 0;
	if(b->n-empty-1 > 127 ||
	   b->n-positive-1 > 127 || b->n-minus-1 > 127) return 0;
	distance=b->n-absent-4;
	for(i=0;i<4;i++)b->bytes[absent+i]=distance>>(i*8);
	b->bytes[empty]=b->n-empty-1;
	b->bytes[positive]=b->n-positive-1; b->bytes[minus]=b->n-minus-1;
	if(p->alu){
		/* Fifteen decimal digits convert exactly in every FP rounding mode. */
		x64byte(b, 0xf2); x64byte(b, 0x49); x64byte(b, 0x0f); x64byte(b, 0x2a); x64byte(b, 0xc0);
		x64byte(b, 0x66); x64byte(b, 0x48); x64byte(b, 0x0f); x64byte(b, 0x7e); x64byte(b, 0xc0);
		x64byte(b, 0x49); x64byte(b, 0xc1); x64byte(b, 0xe1); x64byte(b, 63);
		x64byte(b, 0x4c); x64byte(b, 0x09); x64byte(b, 0xc8); // Preserve the sign of decimal negative zero.
	}else{
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0);
	}
	x64ptr(b, 2, a->d); x64store(b, 0, p->bytes);
	return 1;
}


/* Update or append a character only within valid, uniquely owned capacity. */
static int
x64stringstore(X64Buf *b, LeafLayout *a)
{
	int absolute, wide, done, unchanged, positive;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringdata < 0 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8) return 0;
	x64ptr(b, 0, a->s); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x41); x64byte(b, 0x89); x64byte(b, 0xc0); /* r8d=Rune(value) */
	x64ptr(b, 1, a->m); x64load(b, 1, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xce); /* rsi=full WORD index */
	x64ptr(b, 2, a->d); x64load(b, 2, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xff); x64guard(b, 0x75);
	x64field(b, 0, 2, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 1); x64guard(b, 0x74);
	x64field(b, 1, 2, a->stringlen, 4);
	x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 0x80000000U); x64guard(b, 0x75);
	x64byte(b, 0x41); x64byte(b, 0x89); x64byte(b, 0xc9); /* preserve representation */
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); absolute=b->n; x64byte(b, 0);
	x64byte(b, 0xf7); x64byte(b, 0xd9);
	b->bytes[absolute]=b->n-absolute-1;
	x64field(b, 0, 2, a->stringmax, 4);
	x64byte(b, 0x39); x64byte(b, 0xc8); x64guard(b, 0x7d);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64guard(b, 0x76);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc6); x64guard(b, 0x72);
	x64byte(b, 0x45); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x78); wide=b->n; x64byte(b, 0);
	x64byte(b, 0x41); x64byte(b, 0x81); x64byte(b, 0xf8); x64word(b, 128); x64guard(b, 0x72);
	x64byte(b, 0x44); x64byte(b, 0x88); x64byte(b, 0x84); x64byte(b, 0x32); x64word(b, a->stringdata);
	x64byte(b, 0xeb); done=b->n; x64byte(b, 0);
	b->bytes[wide]=b->n-wide-1;
	x64byte(b, 0x44); x64byte(b, 0x89); x64byte(b, 0x84); x64byte(b, 0xb2); x64word(b, a->stringdata);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[done]=b->n-done-1;
	/* Preserve the captured representation sign when extending the logical length. */
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce);
	x64byte(b, 0x75); unchanged = b->n; x64byte(b, 0);
	x64byte(b, 0xff); x64byte(b, 0xc1);
	x64byte(b, 0x45); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); positive = b->n; x64byte(b, 0);
	x64byte(b, 0xf7); x64byte(b, 0xd9);
	b->bytes[positive] = b->n-positive-1;
	x64byte(b, 0x89); x64byte(b, 0x8a); x64word(b, a->stringlen);
	if(b->n > sizeof(b->bytes))
		return 0;
	b->bytes[unchanged] = b->n-unchanged-1;
	return 1;
}


/* Decode selected binary64 values and format captured integers without allocating. */
static int
x64formatinteger(X64Buf *b, LeafLayout *a, LeafOp *op)
{
	int positive, scaled, storemax, fallback, countloop, digitloop;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 128 || a->heapref%8 || a->refbytes != 8) return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->s); x64load(b, 0, op->bytes);
	if(op->alu){
		/* +0 and signed powers 2^0..2^19 have exact fixed decimal spelling. */
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); // test   rax,rax
		x64byte(b, 0x74); x64byte(b, 0x2c); // je     31 <ready>
		x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2); // mov    rdx,rax
		x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe2); x64byte(b, 0x0c); // shl    rdx,0xc
		x64moduleguard(b, 0x74, fallback); // Fraction must be zero.
		x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc1); // mov    rcx,rax
		x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe9); x64byte(b, 0x34); // shr    rcx,0x34
		x64byte(b, 0x80); x64byte(b, 0xe5); x64byte(b, 0x07); // and    ch,0x7
		x64byte(b, 0x66); x64byte(b, 0x81); x64byte(b, 0xe9); x64byte(b, 0xff); x64byte(b, 0x03); // sub    cx,0x3ff
		x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0x13); // cmp    ecx,0x13
		x64moduleguard(b, 0x76, fallback); // Unbiased exponent must be 0..19.
		x64byte(b, 0xff); x64byte(b, 0xc2); // inc    edx
		x64byte(b, 0x48); x64byte(b, 0xd3); x64byte(b, 0xe2); // shl    rdx,cl
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); // test   rax,rax
		x64byte(b, 0x89); x64byte(b, 0xd0); // mov    eax,edx
		x64byte(b, 0x79); x64byte(b, 0x03); // jns    31 <ready>
		x64byte(b, 0x48); x64byte(b, 0xf7); x64byte(b, 0xd8); // neg    rax
	}
	x64caseptr(b, 2, a->d); x64load(b, 2, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x4a); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64casefield(b, 1, 2, a->stringlen, 4);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc9); // r9 preserves the old representation.
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); positive=b->n; x64byte(b, 0);
	x64byte(b, 0xf7); x64byte(b, 0xd9);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[positive]=b->n-positive-1;
	x64casefield(b, 6, 2, a->stringmax, 4);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64moduleguard(b, 0x7d, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); scaled=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe6); x64byte(b, 2);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xfe); x64word(b, 0x7fffffff); x64moduleguard(b, 0x7e, fallback);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[scaled]=b->n-scaled-1;
	x64casefield(b, 1, 2, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xd2); // r10 retains the owner through division.
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* Count sign and digits before touching even a one-character destination. */
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc0); // mov    r8,rax
	x64byte(b, 0x41); x64byte(b, 0xbb); x64word(b, 10); // Both decimal passes divide by ten.
	x64byte(b, 0x31); x64byte(b, 0xc9); // xor    ecx,ecx
	if(!op->alu)
		x64byte(b, 0x48); // Integer inputs need the full-width TEST.
	x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x79); x64byte(b, 0x05); // jns    f <positive>
	x64byte(b, 0x48); x64byte(b, 0xf7); x64byte(b, 0xd8); // neg    rax
	x64byte(b, 0xff); x64byte(b, 0xc1); // Count the sign.
	countloop=b->n;
	x64byte(b, 0xff); x64byte(b, 0xc1); // Count one decimal digit.
	x64byte(b, 0x31); x64byte(b, 0xd2); // xor    edx,edx
	x64byte(b, 0x49); x64byte(b, 0xf7); x64byte(b, 0xf3); // div    r11
	if(!op->alu)
		x64byte(b, 0x48); // Integer inputs need the full-width TEST.
	x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x75); x64byte(b, countloop-b->n-1); // Recompute after either TEST width.
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); // cmp    rsi,rcx
	x64moduleguard(b, 0x7d, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0); // Restore the captured signed input.
	/* Validate both payload sums before changing representation or writing digits. */
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd2);
	x64byte(b, 0x48); x64byte(b, a->stringdata<=127 ? 0x83 : 0x81); x64byte(b, 0xc2);
	if(a->stringdata<=127)
		x64byte(b, a->stringdata);
	else
		x64word(b, a->stringdata);
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xd7); // rdi holds the payload after REG operands are captured.
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xca); x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); storemax=b->n; x64byte(b, 0);
	if(a->stringmax<=127){
		x64byte(b, 0x41); x64byte(b, 0x89); x64byte(b, 0x72); x64byte(b, a->stringmax);
	}else{
		x64byte(b, 0x41); x64byte(b, 0x89); x64byte(b, 0xb2); x64word(b, a->stringmax);
	}
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[storemax]=b->n-storemax-1;
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xfe); // rsi becomes the byte payload base.
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc8); // mov    r8,rcx
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xd1); // mov    rcx,rdx
	x64byte(b, 0xc6); x64byte(b, 0x01); x64byte(b, 0x00); // mov    BYTE PTR [rcx],0x0
	if(!op->alu)
		x64byte(b, 0x48); // Integer inputs need the full-width TEST.
	x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x79); x64byte(b, 0x06); // jns    14 <again>
	x64byte(b, 0xc6); x64byte(b, 0x06); x64byte(b, 0x2d); // mov    BYTE PTR [rsi],0x2d
	x64byte(b, 0x48); x64byte(b, 0xf7); x64byte(b, 0xd8); // neg    rax
	digitloop=b->n;
	x64byte(b, 0x31); x64byte(b, 0xd2); // xor    edx,edx
	x64byte(b, 0x49); x64byte(b, 0xf7); x64byte(b, 0xf3); // div    r11
	x64byte(b, 0x80); x64byte(b, 0xc2); x64byte(b, 0x30); // add    dl,0x30
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9); // dec    rcx
	x64byte(b, 0x88); x64byte(b, 0x11); // mov    BYTE PTR [rcx],dl
	if(!op->alu)
		x64byte(b, 0x48); // Integer inputs need the full-width TEST.
	x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x75); x64byte(b, digitloop-b->n-1); // Recompute after either TEST width.
	if(a->stringlen<=127){
		x64byte(b, 0x45); x64byte(b, 0x89); x64byte(b, 0x42); x64byte(b, a->stringlen);
	}else{
		x64byte(b, 0x45); x64byte(b, 0x89); x64byte(b, 0x82); x64word(b, a->stringlen);
	}
	return 1;
}


/* Update a unique prefix or move a bounded substring downward within its owner. */
static int
x64stringslice(X64Buf *b, LeafLayout *a)
{
	int positive, ascii, loop, done, prefix;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringdata < 0 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8) return 0;
	x64ptr(b, 0, a->s); x64load(b, 0, a->wordbytes);
	x64ptr(b, 1, a->m); x64load(b, 1, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64guard(b, 0x79);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc1); x64guard(b, 0x77);
	x64ptr(b, 2, a->d); x64load(b, 2, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xff); x64guard(b, 0x75);
	/* r8 retains the signed representation; r9 is its native absolute length. */
	x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x82); x64word(b, a->stringlen);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0xc1);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); positive=b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0xf7); x64byte(b, 0xd9);
	b->bytes[positive]=b->n-positive-1;
	x64field(b, 6, 2, a->stringmax, 4);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xce); x64guard(b, 0x7d);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xc9); x64guard(b, 0x76);
	x64field(b, 6, 2, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfe); x64byte(b, 1); x64guard(b, 0x74);
	x64byte(b, 0x48); x64byte(b, 0x29); x64byte(b, 0xc1);
	/* Prefixes need only a length update, so their size does not bound a copy loop. */
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x74); prefix=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 128); x64guard(b, 0x76);
	b->bytes[prefix]=b->n-prefix-1;
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc9); // r9 is the signed result length.
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x79); ascii=b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0xf7); x64byte(b, 0xd9);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe0); x64byte(b, 2);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe1); x64byte(b, 2);
	b->bytes[ascii]=b->n-ascii-1;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x74); done=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0xb2); x64word(b, a->stringdata);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xf0);
	loop=b->n;
	x64byte(b, 0x44); x64byte(b, 0x8a); x64byte(b, 0x00);
	x64byte(b, 0x44); x64byte(b, 0x88); x64byte(b, 0x06);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc0);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc6);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[done]=b->n-done-1;
	x64byte(b, 0x44); x64byte(b, 0x89); x64byte(b, 0x8a); x64word(b, a->stringlen);
	return 1;
}


/* Allocate a pointer-free frame within the current extent after write-free checks. */
static int
x64frame(X64Buf *b, LeafLayout *a, int imported)
{
	int offsets[10], i;

	offsets[0]=a->module; offsets[1]=a->linkmodule; offsets[2]=a->moduletypes;
	offsets[3]=a->sp; offsets[4]=a->ts; offsets[5]=a->frametype; offsets[6]=a->framemr;
	offsets[7]=a->modulenty; offsets[8]=a->typenp; offsets[9]=a->typesize;
	for(i=0; i<10; i++)
		if(offsets[i]<0 || offsets[i]>2047 || offsets[i]%(i<7 ? 8 : 4))return 0;
	if(imported && (a->linknlinks<0 || a->linknlinks>2047 || a->linknlinks%4 ||
	   a->linklinks<0 || a->linklinks>2047 || a->linklinks%8 ||
	   a->modlbytes!=16 || a->modlframe<0 || a->modlframe>8 || a->modlframe%8))return 0;
	if(a->framesize<32 || a->framesize>2047 || a->framesize%8 || a->frametype>a->framesize-8 || a->framemr>a->framesize-8 ||
	   a->frametype==a->framemr)return 0;
	if(imported){
		/* Resolve a nonnegative import index without reading the legacy module Type table. */
		x64ptr(b, 0, a->s); x64load(b, 0, 8);
		x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64guard(b, 0x77);
		/* Invalid module owners must decline before reading the index operand. */
		x64ptr(b, 1, a->m); x64field(b, 6, 9, 0, a->wordbytes);
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xf6); x64guard(b, 0x79);
		x64field(b, 1, 0, a->linknlinks, 4);
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64guard(b, 0x79);
		x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64guard(b, 0x72);
		x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe6); x64byte(b, 4);
		x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x84); x64byte(b, 0x30);
		x64word(b, a->linklinks+a->modlframe);
	}else{
		x64ptr(b, 0, a->s); x64load(b, 0, a->wordbytes);
		x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc6); // rsi is the complete signed index.
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xf6); x64guard(b, 0x79);
		x64ptr(b, 0, a->module); x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0);
		for(i=0; i<3; i++){
			/* Pointer+1 is 0/1 for H/null; no address is dereferenced on rejection. */
			x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
			x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64guard(b, 0x77);
			if(i==0)x64field(b, 0, 0, a->linkmodule, 8);
			if(i==1){
				x64field(b, 1, 0, a->modulenty, 4);
				x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64guard(b, 0x72);
				x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64guard(b, 0x79);
				x64field(b, 0, 0, a->moduletypes, 8);
			}
		}
		x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x04); x64byte(b, 0xf0);
	}
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64guard(b, 0x77);
	x64field(b, 1, 0, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, a->framesize); x64guard(b, 0x7d);
	x64field(b, 2, 0, a->typenp, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x74);
	x64ptr(b, 0, a->sp); x64ptr(b, 1, a->ts);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc0); x64guard(b, 0x75);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff); x64guard(b, 0x75);
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc1); x64guard(b, 0x73);
	x64byte(b, 0x4d); x64byte(b, 0x29); x64byte(b, 0xc1);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xc9); x64guard(b, 0x77);
	/* Capture all live metadata before publishing the new stack cursor. */
	x64ptr(b, 2, a->d);
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x8f); x64word(b, a->sp);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x80); x64word(b, a->frametype);
	x64byte(b, 0x49); x64byte(b, 0xc7); x64byte(b, 0x80); x64word(b, a->framemr); x64word(b, 0);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0x02);
	return 1;
}


/* Use short field displacements where the private module layout permits them. */
static void
x64modulefield(X64Buf *b, int result, int owner, int offset, int bytes)
{
	if(offset < -128 || offset > 127){
		x64field(b, result, owner, offset, bytes);
		return;
	}
	x64byte(b, 0x48 | (owner >= 8) | (result >= 8 ? 4 : 0));
	x64byte(b, bytes == 8 ? 0x8b : 0x63);
	x64byte(b, 0x40 | (result&7)<<3 | (owner&7));
	x64byte(b, offset);
}


/* Restore pointer-free frames and release retained same-mode module owners. */
static int
x64return(X64Buf *b, LeafLayout *a)
{
	int offsets[11], i, j, fallback, ordinary, done;

	offsets[0]=a->fp; offsets[1]=a->sp; offsets[2]=a->pc;
	offsets[3]=a->frametype; offsets[4]=a->framemr;
	offsets[5]=a->framefp; offsets[6]=a->framelr;
	offsets[7]=a->module; offsets[8]=a->regmp; offsets[9]=a->linkmp;
	offsets[10]=a->linkcompiled;
	for(i=0; i<11; i++)
		if(offsets[i]<0 || offsets[i]>2047 || offsets[i]%(i==10 ? 4 : 8))return 0;
	for(i=3; i<7; i++)for(j=i+1; j<7; j++)
		if(offsets[i]==offsets[j])return 0;
	if(a->typenp<0 || a->typenp>2047 || a->typenp%4 ||
	   a->heapref<=0 || a->heapref>128 || a->heapref%8 || a->refbytes!=8)return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* r8 keeps the frame, r9 the caller FP, r10 the PC and r11 the saved module. */
	x64modulefield(b, 8, 7, a->fp, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 0, 8, a->frametype, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 1, 0, a->typenp, 4);
	x64byte(b, 0x85); x64byte(b, 0xc9); x64moduleguard(b, 0x74, fallback);
	x64modulefield(b, 9, 8, a->framefp, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x51); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 10, 8, a->framelr, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x52); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	/* A nil saved module skips every current-module and heap-header read. */
	x64modulefield(b, 11, 8, a->framemr, 8);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xdb);
	x64byte(b, 0x74); ordinary=b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x53); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 0, 7, a->module, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 1, 0, a->linkcompiled, 4);
	x64modulefield(b, 2, 11, a->linkcompiled, 4);
	x64byte(b, 0x39); x64byte(b, 0xd1); x64moduleguard(b, 0x74, fallback);
	/* rax/rcx capture the owner and count; rsi captures caller MP before stores. */
	x64modulefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9);
	x64modulefield(b, 6, 11, a->linkmp, 8);
	if(b->n>sizeof(b->bytes) || b->n-ordinary-1>127)return 0;
	b->bytes[ordinary]=b->n-ordinary-1;
	/* Capture all links and MP before restoring REG or releasing the current owner. */
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x8f); x64word(b, a->fp);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, a->sp);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x97); x64word(b, a->pc);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xdb);
	x64byte(b, 0x74); done=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x88); x64word(b, -a->heapref);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x9f); x64word(b, a->module);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xb7); x64word(b, a->regmp);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[done]=b->n-done-1;
	return 1;
}


/* Retain bytecode callees after validating owners, execution modes and frame inputs. */
static int
x64modulecall(X64Buf *b, LeafLayout *a)
{
	int offsets[11], i, fallback;

	offsets[0]=a->module; offsets[1]=a->fp; offsets[2]=a->regmp; offsets[3]=a->linkprog;
	offsets[4]=a->linkmp; offsets[5]=a->linklinks; offsets[6]=a->framelr;
	offsets[7]=a->framefp; offsets[8]=a->framemr; offsets[9]=a->regtemp; offsets[10]=a->linknlinks;
	for(i=0;i<11;i++)
		if(offsets[i]<0 || offsets[i]>2047 || offsets[i]%(i<9 ? 8 : i==9 ? a->wordbytes : 4))return 0;
	if(a->linkcompiled<0 || a->linkcompiled>2047 || a->linkcompiled%4 ||
	   a->modlbytes!=16 || a->modlpc<0 || a->modlpc>8 || a->modlpc%8 || a->heapref<=0 || a->heapref>128 || a->heapref%8 || a->refbytes!=8 ||
	   a->framelr==a->framefp || a->framelr==a->framemr || a->framefp==a->framemr)return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64ptr(b, 0, a->d); x64load(b, 0, 8);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc0); // r8 is the module owner.
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x9f); x64word(b, a->module);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x53); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 1, 8, a->linkcompiled, 4);
	x64modulefield(b, 2, 11, a->linkcompiled, 4);
	x64byte(b, 0x39); x64byte(b, 0xd1); x64moduleguard(b, 0x74, fallback);
	x64modulefield(b, 1, 0, a->linkprog, 8);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64ptr(b, 1, a->m); x64modulefield(b, 6, 9, 0, a->wordbytes);
	x64modulefield(b, 1, 0, a->linknlinks, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64moduleguard(b, 0x79, fallback);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64moduleguard(b, 0x72, fallback);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe6); x64byte(b, 4);
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x94); x64byte(b, 0x30); x64word(b, a->linklinks+a->modlpc);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x52); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64ptr(b, 1, a->s); x64load(b, 1, 8);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc9); // r9 is the captured frame.
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 2, 7, a->pc, 8); x64modulefield(b, 6, 7, a->fp, 8);
	x64modulefield(b, 0, 8, a->linkmp, 8);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x91); x64word(b, a->framelr);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xb1); x64word(b, a->framefp);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0x99); x64word(b, a->framemr);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x8f); x64word(b, a->fp);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, a->module);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x88); x64word(b, -a->heapref);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, a->regmp);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x97); x64word(b, a->pc);
	if(a->wordbytes==8)x64byte(b, 0x48);
	x64byte(b, 0xc7); x64byte(b, 0x87); x64word(b, a->regtemp); x64word(b, 1);
	return 1;
}


/* Narrow unique array views without allocating, retaining or visiting elements. */
static int
x64arrayslice(X64Buf *b, LeafLayout *a)
{
	int fields[7], i, j, fallback, empty, rootcase, rootdone;

	fields[0]=a->arraylen; fields[1]=a->arraydata; fields[2]=a->arraytype;
	fields[3]=a->arrayroot; fields[4]=a->typesize; fields[5]=a->typeref; fields[6]=a->typenp;
	for(i=0; i<7; i++)
		if(fields[i]<0 || fields[i]>2047 || fields[i]%(i==0 ? a->wordbytes : i<4 ? 8 : 4))return 0;
	/* Header fields and the three int descriptor fields must occupy distinct spans. */
	for(i=0; i<4; i++)for(j=i+1; j<4; j++)
		if(fields[i]<fields[j]+(j==0 ? a->wordbytes : 8) &&
		   fields[j]<fields[i]+(i==0 ? a->wordbytes : 8))return 0;
	if(a->typesize==a->typeref || a->typenp==a->typeref || a->typenp==a->typesize)return 0;
	if(a->heapref<=0 || a->heapref>128 || a->heapref%8 || a->refbytes!=8)return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* r8/r9 retain start/end; r10 keeps the view throughout every guard. */
	x64ptr(b, 0, a->s); x64modulefield(b, 8, 8, 0, a->wordbytes);
	x64ptr(b, 1, a->m); x64modulefield(b, 9, 9, 0, a->wordbytes);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x79, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc1); x64moduleguard(b, 0x7d, fallback);
	x64ptr(b, 2, a->d); x64modulefield(b, 10, 10, 0, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x4a); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 1, 10, a->arraylen, a->wordbytes);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xc9); x64moduleguard(b, 0x7e, fallback);
	x64modulefield(b, 0, 10, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 1); x64moduleguard(b, 0x74, fallback);
	x64modulefield(b, 11, 10, a->arraytype, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x4b); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 0, 11, a->typeref, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x7f, fallback);
	/* Root arrays may reuse their header only when no element owns pointers. */
	x64modulefield(b, 0, 10, a->arrayroot, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x48); x64byte(b, 1);
	x64byte(b, 0xe3); rootcase=b->n; x64byte(b, 0); // JRCXZ recognizes the H sentinel.
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x75, fallback);
	x64modulefield(b, 0, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0xeb); rootdone=b->n; x64byte(b, 0);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[rootcase]=b->n-rootcase-1;
	x64modulefield(b, 0, 11, a->typenp, 4);
	x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x74, fallback);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[rootdone]=b->n-rootdone-1;
	/* Keep all remaining declines within short reach of a second private return. */
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64modulefield(b, 0, 11, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x79, fallback);
	x64modulefield(b, 6, 10, a->arraydata, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	/* Bound multiplication by INT_MAX before native address arithmetic. */
	x64byte(b, 0x49); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 0x7fffffff); x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc9);
	x64byte(b, 0x48); x64byte(b, 0x0f); x64byte(b, 0xaf); x64byte(b, 0xc8);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 0x7fffffff); x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc9); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x56); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xf2);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xca); x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x49); x64byte(b, 0x0f); x64byte(b, 0xaf); x64byte(b, 0xc0);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc6);
	if(b->n>sizeof(b->bytes) || b->n-empty-1>127)return 0;
	b->bytes[empty]=b->n-empty-1;
	/* The operand slot, backing root, element Type and all counters stay intact. */
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xca);
	x64byte(b, 0x4c); x64byte(b, 0x29); x64byte(b, 0xc2);
	x64byte(b, a->wordbytes==8 ? 0x49 : 0x41);
	x64byte(b, 0x89); x64byte(b, 0x92); x64word(b, a->arraylen);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xb2); x64word(b, a->arraydata);
	return 1;
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
	if(p.kind == LeafArraySlice){
		if(!x64arrayslice(&b, abi))return 0;
	}else if(p.kind == LeafModuleCall){
		if(!x64modulecall(&b, abi))return 0;
	}else if(p.kind == LeafReturn){
		if(!x64return(&b, abi))return 0;
	}else if(p.kind == LeafFrame){
		if(!x64frame(&b, abi, p.alu))return 0;
	}else if(p.kind == LeafFormatInteger){
		if(!x64formatinteger(&b, abi, &p)) return 0;
	}else if(p.kind == LeafStringSlice){
		if(!x64stringslice(&b, abi)) return 0;
	}else if(p.kind == LeafStringAppend){
		if(!x64stringappend(&b, abi)) return 0;
	}else if(p.kind == LeafStringStore){
		if(!x64stringstore(&b, abi)) return 0;
	}else if(p.kind == LeafDecimal){
		if(!x64decimal(&b, abi, &p)) return 0;
	}else if(p.kind == LeafSliceCopy){
		if(!x64slicecopy(&b, abi)) return 0;
	}else if(p.kind == LeafMappedCopy){
		if(!x64mapped(&b, abi, &p)) return 0;
	}else if(p.kind == LeafOwner){
		if(!x64owner(&b, abi, &p)) return 0;
	}else if(p.kind == LeafCaseJump){
		if(!x64case(&b, abi, &p)) return 0;
	}else if(p.kind == LeafStringBranch){
		/* Compare captured characters without touching a false branch target. */
		if(abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 ||
		   abi->stringmax < 0 || abi->stringmax > 2047 || abi->stringmax%4 ||
		   abi->stringdata < 0 || abi->stringdata > 2047) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64ptr(&b, 1, abi->m); x64load(&b, 1, 8);
		x64stringheader(&b, 0, 2, 0, abi->stringlen, abi->stringmax, abi->stringdata);
		x64stringheader(&b, 1, 6, 1, abi->stringlen, abi->stringmax, abi->stringdata);
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
	}else if(p.kind == LeafProgramAddress || p.kind == LeafTableJump){
		/* Materialize a Dis PC only after validating the module, index and native span. */
		if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
		   abi->linkmodule < 0 || abi->linkmodule > 2047 || abi->linkmodule%8 ||
		   abi->moduleprog < 0 || abi->moduleprog > 2047 || abi->moduleprog%8 ||
		   abi->modulenprog < 0 || abi->modulenprog > 2047 || abi->modulenprog%4 ||
		   abi->instbytes < 1 || abi->instbytes > 2047) return 0;
		x64ptr(&b, 0, abi->module); x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xc0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		if(p.kind == LeafTableJump){
			if(abi->linkcompiled < 0 || abi->linkcompiled > 2047 || abi->linkcompiled%4) return 0;
			x64field(&b, 2, 0, abi->linkcompiled, 4);
			x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x74);
		}
		x64field(&b, 0, 0, abi->linkmodule, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		x64field(&b, 1, 0, abi->modulenprog, 4);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		if(p.kind == LeafTableJump){
			/* The compiler's table count and destinations are signed 32-bit Dis words. */
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x79);
			x64ptr(&b, 2, abi->d);
			x64byte(&b, 0x49); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 4); x64guard(&b, 0x73);
			x64byte(&b, 0x49); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
			x64field(&b, 2, 10, -4, 4);
			x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x7f);
			x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xd0); x64guard(&b, 0x72);
			x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe0); x64byte(&b, 2);
			x64byte(&b, 0x4c); x64byte(&b, 0x01); x64byte(&b, 0xd0); x64guard(&b, 0x73);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xfc); x64guard(&b, 0x76);
			x64byte(&b, 0x48); x64byte(&b, 0x63); x64byte(&b, 0x00);
		}
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
		x64byte(&b, 0x48); x64byte(&b, 0x69); x64byte(&b, 0xc0); x64word(&b, abi->instbytes);
		x64field(&b, 2, 11, abi->moduleprog, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xd0); x64guard(&b, 0x73);
		if(p.kind == LeafTableJump){
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x87); x64word(&b, abi->pc);
		}else{x64ptr(&b, 2, abi->d); x64store(&b, 0, 8);}
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
		if(abi->stringlen < 0 || abi->stringdata < 0 ||
		   abi->stringmax < 0 || abi->stringmax > 2047 || abi->stringmax%4) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3); /* r11 = owner */
		x64field(&b, 1, 0, abi->stringlen, 4);
		x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, 0x80000000U); x64guard(&b, 0x75);
		x64byte(&b, 0x89); x64byte(&b, 0xca); /* edx = representation sign */
		x64byte(&b, 0x85); x64byte(&b, 0xc9);
		x64byte(&b, 0x79); x64byte(&b, 2); x64byte(&b, 0xf7); x64byte(&b, 0xd9);
		/* Reject a logical length beyond backing capacity before reading characters. */
		x64field(&b, 0, 11, abi->stringmax, 4);
		x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x7d);
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
		/* Bounded Brent scans decline on cycles without publishing a partial length. */
		if(abi->listtail < 0 || abi->listtail > 32760 || abi->listtail%8) return 0;
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3); // r11 is the initial checkpoint.
		x64byte(&b, 0x31); x64byte(&b, 0xc9); /* count = 0 */
		loop = b.n;
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
		x64byte(&b, 0x74); done = b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, 256); x64guard(&b, 0x72);
		x64field(&b, 0, 0, abi->listtail, 8);
		x64byte(&b, 0xff); x64byte(&b, 0xc1);
		x64byte(&b, 0x4c); x64byte(&b, 0x39); x64byte(&b, 0xd8); x64guard(&b, 0x75);
		x64byte(&b, 0x8d); x64byte(&b, 0x51); x64byte(&b, 1); // edx = count+1.
		x64byte(&b, 0x85); x64byte(&b, 0xca);
		x64byte(&b, 0x75); x64byte(&b, 3);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
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
			if(abi->listdata < 0 || abi->listdata > 4095) return 0;
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
		x64byte(&b, 0x48); x64byte(&b, 0x3d); x64word(&b, 256); x64guard(&b, 0x76);
		if(p.kind == LeafHeadCopy){
			x64byte(&b, 0x4d); x64byte(&b, 0x89); x64byte(&b, 0xd8);
			x64byte(&b, 0x49); x64byte(&b, 0x81); x64byte(&b, 0xc0); x64word(&b, abi->listdata);
			x64guard(&b, 0x73); // Carry-clear payload formation.
		}else
			x64ptr(&b, 0, abi->s);
		x64ptr(&b, 2, abi->d);
		/* Check both native spans before either forward or backward copying. */
		x64byte(&b, 0x4d); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x4d); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
		x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xc1);
		x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xc1); x64guard(&b, 0x73);
		x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xd1);
		x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xc1); x64guard(&b, 0x73);
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
			x64realproduct(&b);
		}else{
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc1);
			x64byte(&b, 0x48); x64byte(&b, 0xd1); x64byte(&b, 0xe1);
		}
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
	}else if(p.kind != LeafNop)
		return 0; /* A new classifier kind must never silently become a native no-op. */
	x64byte(&b, 0xb8); x64word(&b, 1); x64byte(&b, 0xc3);
	while(b.n%4 != 0) x64byte(&b, 0x90);
	n = b.n/4;
	if(b.n > sizeof(b.bytes) || n > cap) return 0;
	if(out != nil) for(i = 0; i < n; i++)
		out[i] = (u32int)b.bytes[i*4] | (u32int)b.bytes[i*4+1]<<8 |
			(u32int)b.bytes[i*4+2]<<16 | (u32int)b.bytes[i*4+3]<<24;
	return n;
}
