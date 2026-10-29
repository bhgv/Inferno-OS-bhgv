#include "leaf64-ops.h"

enum {
	LeafMax = 128, // Four-byte code units reserved for guarded native leaves.
};

typedef struct X64Buf X64Buf;

struct X64Buf {
	uchar bytes[LeafMax*4]; // Private native-byte staging, published only after sizing succeeds.
	int n; // Requested native bytes before bounded publication.
};


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


/* Reject a wrapped field span in rax without changing its owner; rdx is scratch. */
static void
x64readspan(X64Buf *b, int offset, int bytes)
{
	unsigned long long limit;
	int i;

	limit = 0ULL-(unsigned long long)offset-(unsigned int)bytes;
	x64byte(b, 0x48); x64byte(b, 0xba); // movabs rdx, first rejected owner.
	for(i = 0; i < 8; i++)
		x64byte(b, limit >> (i*8));
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd0); // cmp rax, rdx.
	x64guard(b, 0x72); // Continue only below the first overflowing owner.
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


/* Resolve a non-null String representation, retaining a native character stride. */
static void
x64stringheader(X64Buf *b, int owner, int length, int stride, int lenoff, int maxoff, int dataoff, int fallback)
{
	int empty, positive, done;

	x64byte(b, 0x41); x64byte(b, 0xb8+stride); x64word(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8+owner); x64byte(b, 0xff);
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	/* r11 holds the first owner address that cannot bound every logical payload. */
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xd8+owner);
	x64moduleguard(b, 0x72, fallback);
	x64field(b, length, owner, lenoff, 4);
	x64byte(b, 0x85); x64byte(b, 0xc0+length*9);
	x64byte(b, 0x79); positive=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0xf7); x64byte(b, 0xd8+length);
	x64byte(b, 0x41); x64byte(b, 0xb8+stride); x64word(b, 4);
	b->bytes[positive]=b->n-positive-1;
	/* Signed capacity bounds also reject the absolute value of INT_MIN. */
	x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x90+owner); x64word(b, maxoff);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xd0+length); x64moduleguard(b, 0x7e, fallback);
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
	/* Biased scale exponents 1..2046 are precisely the finite normal powers. */
	x64byte(b, 0x8d); x64byte(b, 0x91); x64word(b, 1022);
	x64byte(b, 0x81); x64byte(b, 0xfa); x64word(b, 2045); x64guard(b, 0x76);
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

/* Use full displacements so distant private fields cannot invalidate a guarded case leaf. */
static void
x64caseguard(X64Buf *b, int success, int *patch, int *count)
{
	x64byte(b, 0x0f); x64byte(b, (success^1)+0x10);
	patch[(*count)++]=b->n;
	x64word(b, 0);
}


/* Reject null, H and wrapping spans; positive extents must fit the signed compare limit. */
static void
x64pcspan(X64Buf *b, int owner, int extent)
{
	x64byte(b, owner >= 8 ? 0x49 : 0x48);
	x64byte(b, 0x8d); x64byte(b, 0x50 | (owner&7)); x64byte(b, 0xff);
	x64byte(b, 0x48); x64byte(b, extent <= 126 ? 0x83 : 0x81); x64byte(b, 0xfa);
	if(extent <= 126)
		x64byte(b, -extent-2);
	else
		x64word(b, -extent-2);
	x64guard(b, 0x76);
}


/* Patch a complete relative displacement only inside the private code buffer. */
static int
x64casepatch(X64Buf *b, int at, int destination)
{
	int i, displacement;

	if(at < 0 || at > sizeof(b->bytes)-4 || destination < 0 || destination > b->n)
		return 0;
	displacement = destination-at-4;
	for(i = 0; i < 4; i++)
		b->bytes[at+i] = (u32int)displacement>>(8*i);
	return 1;
}


/* Require an empty String owner in rax; H has no header and negative capacities decline. */
static void
x64caseempty(X64Buf *b, LeafLayout *abi)
{
	int empty;

	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
	x64byte(b, 0x74); empty = b->n; x64byte(b, 0);
	x64pcspan(b, 0, abi->stringlen+4 > abi->stringmax+4 ? abi->stringlen+4 : abi->stringmax+4);
	x64casefield(b, 1, 0, abi->stringlen, 4);
	x64byte(b, 0x85); x64byte(b, 0xc9); x64guard(b, 0x74);
	x64casefield(b, 2, 0, abi->stringmax, 4);
	x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x79);
	if(empty < sizeof(b->bytes))
		b->bytes[empty] = b->n-empty-1;
}


/* Search numeric tables and lower-label string identities without publishing unchecked targets. */
static int
x64case(X64Buf *b, LeafLayout *abi, LeafOp *op)
{
	int reject[32], count, first, i;
	int loop, empty, left, right, found, leftlabel, rightlabel, done;
	int header, stride, identity, positive, same, matched;

	if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
	   abi->linkmodule < 0 || abi->linkmodule > 2047 || abi->linkmodule%8 ||
	   abi->linkcompiled < 0 || abi->linkcompiled > 2047 || abi->linkcompiled%4 ||
	   abi->moduleprog < 0 || abi->moduleprog > 2047 || abi->moduleprog%8 ||
	   abi->modulenprog < 0 || abi->modulenprog > 2047 || abi->modulenprog%4 ||
	   abi->instbytes < 1 || abi->instbytes > 2047)
		return 0;
	if(op->condition && (abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 ||
	   abi->stringmax < 0 || abi->stringmax > 2047 || abi->stringmax%4 ||
	   abi->stringlen == abi->stringmax))
		return 0;
	header=op->alu ? 2*IBY2WD : IBY2WD;
	stride=op->alu ? 2*IBY2LG+2*IBY2WD : 3*IBY2WD; count=0;
	/* Every checked rejection reaches one side-effect-free return. */
	x64byte(b, 0xeb); x64byte(b, 3); first=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, abi->module);
	x64pcspan(b, 8,
		abi->linkmodule+8 > abi->linkcompiled+4 ? abi->linkmodule+8 : abi->linkcompiled+4);
	x64casefield(b, 2, 8, abi->linkcompiled, 4);
	x64byte(b, 0x85); x64byte(b, 0xd2); x64caseguard(b, 0x74, reject, &count);
	x64casefield(b, 0, 8, abi->linkmodule, 8);
	x64pcspan(b, 0,
		abi->moduleprog+8 > abi->modulenprog+4 ? abi->moduleprog+8 : abi->modulenprog+4);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc3); /* r11=module */
	x64caseptr(b, 0, abi->s); x64pcspan(b, 8, op->bytes); x64load(b, 0, op->bytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc6); /* rsi=value */
	x64caseptr(b, 2, abi->d); x64pcspan(b, 10, op->condition ? IBY2WD : 4);
	x64casefield(b, 1, 10, 0, op->condition ? IBY2WD : 4);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 4095); x64caseguard(b, 0x76, reject, &count);
	/* Narrow cells cannot hold native String pointers; reject before reading rows/default. */
	if(op->condition && IBY2WD < 8){
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64caseguard(b, 0x74, reject, &count);
	}
	x64byte(b, 0x48); x64byte(b, 0x6b); x64byte(b, 0xc1); x64byte(b, stride);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc0); x64byte(b, header);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd2);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc2); x64caseguard(b, 0x73, reject, &count);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, op->condition ? -IBY2WD : -4); x64caseguard(b, 0x76, reject, &count);
	x64byte(b, 0x4c); x64byte(b, op->condition && IBY2WD == 8 ? 0x8b : 0x63); x64byte(b, 0x0a); /* r9=default */
	x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x42); x64byte(b, header); /* r8=rows */
	loop=b->n;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	if(op->condition){
		x64byte(b, 0x0f); x64byte(b, 0x84); empty=b->n; x64word(b, 0);
	}else{
		x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	}
	if(op->condition){
		/* Narrow cells cannot contain native String pointers; empty tables still run. */
		if(IBY2WD < 8){
			x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
		}else{
			/* Probe the same midpoint row as C; other comparisons retain its search. */
			x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc8); // mov rax,rcx.
			x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe8); // shr rax,1.
			x64byte(b, 0x48); x64byte(b, 0x6b); x64byte(b, 0xc0); x64byte(b, stride);
			x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xc0); // add r8,rax.
			x64casefield(b, 0, 8, 0, 8);
			x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc6);
			x64byte(b, 0x0f); x64byte(b, 0x84); same=b->n; x64word(b, 0);
			x64caseempty(b, abi);
			x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xf0); // mov rax,rsi captures the source owner.
			x64caseempty(b, abi);
			x64byte(b, 0xe9); matched=b->n; x64word(b, 0);
			if(!x64casepatch(b, same, b->n))return 0;
			x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
			x64byte(b, 0x74); identity=b->n; x64byte(b, 0);
			x64pcspan(b, 0, abi->stringlen+4 > abi->stringmax+4 ? abi->stringlen+4 : abi->stringmax+4);
			x64casefield(b, 1, 0, abi->stringlen, 4);
			x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
			x64byte(b, 0x79); positive=b->n; x64byte(b, 0);
			x64byte(b, 0x48); x64byte(b, 0xf7); x64byte(b, 0xd9);
			b->bytes[positive]=b->n-positive-1;
			x64casefield(b, 2, 0, abi->stringmax, 4);
			x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); x64guard(b, 0x7e);
			b->bytes[identity]=b->n-identity-1;
			if(!x64casepatch(b, matched, b->n))return 0;
			x64byte(b, 0x4d); x64byte(b, 0x8b); x64byte(b, 0x48); x64byte(b, 2*IBY2WD);
		}
	}else{
		x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc8);
		x64byte(b, 0x48); x64byte(b, 0xd1); x64byte(b, 0xe8);
		x64byte(b, 0x48); x64byte(b, 0x6b); x64byte(b, 0xc0); x64byte(b, stride);
		x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x14); x64byte(b, 0x00);
		x64byte(b, 0x48); x64byte(b, op->alu ? 0x8b : 0x63); x64byte(b, 0x02);
		x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc6);
		x64byte(b, 0x7c); left=b->n; x64byte(b, 0);
		x64byte(b, 0x48); x64byte(b, op->alu ? 0x8b : 0x63); x64byte(b, 0x42); x64byte(b, op->alu ? IBY2LG : IBY2WD);
		x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc6);
		x64byte(b, 0x7d); right=b->n; x64byte(b, 0);
		x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x4a); x64byte(b, op->alu ? 2*IBY2LG : 2*IBY2WD);
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
	}
	done=b->n;
	if(b->n > sizeof(b->bytes)) return 0;
	if(op->condition){
		if(!x64casepatch(b, empty, done))return 0;
	}else{
		b->bytes[empty]=done-empty-1;
	}
	if(!op->condition){
		b->bytes[found]=done-found-1;
		b->bytes[left]=leftlabel-left-1; b->bytes[right]=rightlabel-right-1;
	}
	x64casefield(b, 2, 11, abi->modulenprog, 4);
	x64byte(b, 0x85); x64byte(b, 0xd2); x64caseguard(b, 0x7f, reject, &count);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xd1); x64caseguard(b, 0x72, reject, &count);
	x64casefield(b, 0, 11, abi->moduleprog, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0xff);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xfd); x64caseguard(b, 0x76, reject, &count);
	x64byte(b, 0x4d); x64byte(b, 0x69); x64byte(b, 0xc9); x64word(b, abi->instbytes);
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xc8); x64caseguard(b, 0x73, reject, &count);
	x64pcspan(b, 0, abi->instbytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, abi->pc);
	if(b->n > sizeof(b->bytes)) return 0;
	for(i = 0; i < count; i++)
		if(!x64casepatch(b, reject[i], first))return 0;
	return 1;
}

/* Copy and mark live owners into empty or identical slots without helper calls. */
static int
x64owner(X64Buf *b, LeafLayout *abi, LeafOp *p)
{
	unsigned long long color;
	int offset, empty, same, marked, validated;

	if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
	   !leafownergclayout(abi))
		return 0;
	offset=p->alu == 2 ? abi->listdata : abi->listtail;
	if(p->alu >= 2 && (offset < 0 || offset > 2047))
		return 0;
	/* No operand or owner read may precede the writable destination-slot check. */
	x64ptr(b, 2, abi->d); x64pcspan(b, 10, 8);
	if(p->alu != 1){
		x64ptr(b, 0, abi->s); x64pcspan(b, 8, 8);
	}
	if(p->alu == 1){
		x64ptr(b, 0, abi->module);
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0);
	}else{
		x64ptr(b, 0, abi->s); x64load(b, 0, 8);
	}
	if(p->alu >= 2){
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64guard(b, 0x75);
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff); x64guard(b, 0x75);
		x64pcspan(b, 0, offset+8);
		x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x80); x64word(b, offset);
		x64load(b, 0, 8);
	}
	/* Source errors precede the old-owner load, so an unused bad destination is not read. */
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
	validated=-1;
	if(p->alu == 1)
		x64guard(b, 0x75);
	else{
		x64byte(b, 0x74); validated=b->n; x64byte(b, 0);
	}
	x64byte(b, 0x48); x64byte(b, 0x3d);
	x64word(b, abi->heapref > abi->heapcolor ? abi->heapref : abi->heapcolor);
	x64guard(b, 0x77);
	/* Heap metadata requires native alignment even when operand slots are packed. */
	x64byte(b, 0xa8); x64byte(b, 7); x64guard(b, 0x74); // test al,7; decline misaligned owners.
	x64field(b, 1, 0, -abi->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64guard(b, 0x75);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff); x64guard(b, 0x75);
	if(validated >= 0)
		b->bytes[validated]=b->n-validated-1;
	x64ptr(b, 2, abi->d);
	x64byte(b, 0x4d); x64byte(b, 0x8b); x64byte(b, 0x0a); /* r9=previous owner */
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
	x64byte(b, 0x74); same=b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xc1); x64guard(b, 0x74);
	b->bytes[same]=b->n-same-1;
	empty=-1;
	if(p->alu != 1){
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
		x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	}
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


/* Emit an inline overlap-safe copy of captured pointers/count, with no allocation or calls.
 * rax is the captured byte count, r8/r10 are source/destination; rcx and flags are
 * scratch. Pointers/count may advance; REG, r11, stack and FP state are untouched.
 * Complete qwords precede an exact byte tail. Zero counts perform no accesses.
 * Return zero only when private code staging cannot hold the completed kernel.
 */
static int
x64copykernel(X64Buf *b)
{
	int other, done, loop, tail, finish;

	/* Copy downward whenever the destination follows the source. */
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc2);
	x64byte(b, 0x76); other = b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xc0);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xc2);
	/* Capture each full eight-byte block before its overlapping store. */
	loop = b->n;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 8);
	x64byte(b, 0x72); tail = b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xe8); x64byte(b, 8);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xea); x64byte(b, 8);
	x64byte(b, 0x49); x64byte(b, 0x8b); x64byte(b, 0x08);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x0a);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xe8); x64byte(b, 8);
	x64byte(b, 0xeb); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[tail] = b->n-tail-1;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x74); finish = b->n; x64byte(b, 0);
	loop = b->n;
	x64byte(b, 0x49); x64byte(b, 0xff); x64byte(b, 0xc8);
	x64byte(b, 0x49); x64byte(b, 0xff); x64byte(b, 0xca);
	x64byte(b, 0x41); x64byte(b, 0x8a); x64byte(b, 0x08);
	x64byte(b, 0x41); x64byte(b, 0x88); x64byte(b, 0x0a);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc8);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[finish] = b->n-finish-1;
	x64byte(b, 0xeb); done = b->n; x64byte(b, 0);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[other] = b->n-other-1;
	/* Forward qword blocks leave at most seven individually bounded bytes. */
	loop = b->n;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 8);
	x64byte(b, 0x72); tail = b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0x8b); x64byte(b, 0x08);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x0a);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xc0); x64byte(b, 8);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xc2); x64byte(b, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xe8); x64byte(b, 8);
	x64byte(b, 0xeb); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[tail] = b->n-tail-1;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x74); finish = b->n; x64byte(b, 0);
	loop = b->n;
	x64byte(b, 0x41); x64byte(b, 0x8a); x64byte(b, 0x08);
	x64byte(b, 0x41); x64byte(b, 0x88); x64byte(b, 0x0a);
	x64byte(b, 0x49); x64byte(b, 0xff); x64byte(b, 0xc0);
	x64byte(b, 0x49); x64byte(b, 0xff); x64byte(b, 0xc2);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc8);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[finish] = b->n-finish-1;
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[done] = b->n-done-1;
	return 1;
}


/* Resolve live types and bound every accessed span before copying pointer-free records. */
static int
x64mapped(X64Buf *b, LeafLayout *a, LeafOp *p)
{
	int fallback, empty, extent, heapbase;

	/* The real adapter supplies the distance to the first Heap field as well. */
	heapbase=a->heapref;
	if(a->heapcolor > heapbase && a->heapcolor <= 255)
		heapbase=a->heapcolor;
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
		x64caseptr(b, 0, a->s); x64pcspan(b, 8, 8); x64load(b, 0, 8);
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
		if(!x64mappedguard(b, 0x75, fallback)) return 0;
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
		if(!x64mappedguard(b, 0x75, fallback)) return 0;
		x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc3);
	}
	/* The type-index slot is required even when the selected record is empty. */
	x64caseptr(b, 0, a->m); x64pcspan(b, 8, a->wordbytes);
	x64caseptr(b, 0, a->module); x64pcspan(b, 8, a->linkmodule+8);
	x64casefield(b, 0, 8, a->linkmodule, 8);
	extent=a->moduletypes+8 > a->modulenty+4 ? a->moduletypes+8 : a->modulenty+4;
	x64pcspan(b, 0, extent);
	x64casefield(b, 1, 0, a->moduletypes, 8);
	x64casefield(b, 2, 0, a->modulenty, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2);
	x64guard(b, 0x7f);
	/* Reset the local rejection distance before vector/index checks. */
	x64byte(b, 0xeb); x64byte(b, 3); fallback=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->m); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd0);
	if(!x64mappedguard(b, 0x72, fallback)) return 0;
	/* Check the vector identity before addition; then bound the complete cell. */
	x64pcspan(b, 1, 8);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe0); x64byte(b, 3);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc1);
	if(!x64mappedguard(b, 0x73, fallback)) return 0;
	x64pcspan(b, 1, 8);
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x09);
	x64pcspan(b, 1, a->typesize+4);
	x64casefield(b, 0, 1, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, 256);
	if(!x64mappedguard(b, 0x76, fallback)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x0f); x64byte(b, 0x84); empty=b->n; x64word(b, 0);
	/* Empty records do not consult maps, payload addresses or heap storage. */
	x64pcspan(b, 1, a->typenp+4);
	x64byte(b, 0xf6); x64byte(b, 0xc1); x64byte(b, 7); x64guard(b, 0x74);
	x64casefield(b, 2, 1, a->typenp, 4);
	x64byte(b, 0x85); x64byte(b, 0xd2); x64guard(b, 0x74);
	x64byte(b, 0xeb); x64byte(b, 3); fallback=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	if(p->alu){
		/* Bound the backward reference cell and payload base without a load. */
		x64byte(b, 0x49); x64byte(b, 0x81); x64byte(b, 0xfb); x64word(b, heapbase);
		if(!x64mappedguard(b, 0x77, fallback)) return 0;
		x64byte(b, 0x41); x64byte(b, 0xf6); x64byte(b, 0xc3); x64byte(b, 7);
		if(!x64mappedguard(b, 0x74, fallback)) return 0;
		x64pcspan(b, 11, a->listdata+1);
		x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x83); x64word(b, a->listdata);
	}else
		x64caseptr(b, 0, a->s);
	x64caseptr(b, 2, a->d);
	/* (pointer-1)+(size+1) rejects nil, H and a wrapping exclusive end. */
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x48); x64byte(b, 1);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0xff);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xca);
	if(!x64mappedguard(b, 0x73, fallback)) return 0;
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x52); x64byte(b, 0xff);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xca);
	if(!x64mappedguard(b, 0x73, fallback)) return 0;
	if(p->alu){
		x64casefield(b, 1, 11, -a->heapref, 8);
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
		if(!x64mappedguard(b, 0x75, fallback)) return 0;
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
		if(!x64mappedguard(b, 0x75, fallback)) return 0;
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, a->s);
	}
	if(!x64copykernel(b)) return 0;
	if(b->n > sizeof(b->bytes) || !x64casepatch(b, empty, b->n)) return 0;
	return 1;
}


/* Return success when the preceding comparison identifies an empty operation. */
static void
x64sliceempty(X64Buf *b, int notempty)
{
	x64byte(b, notempty); x64byte(b, 6);
	x64byte(b, 0xb8); x64word(b, 1); x64byte(b, 0xc3);
}


/* Copy short pointer-free record slices after complete byte/span preflight. */
static int
x64slicecopy(X64Buf *b, LeafLayout *a)
{
	int fallback, identical, arrayend, typeend;

	if(a->arraylen < 0 || a->arraylen > 2047 || a->arraylen%a->wordbytes ||
	   a->arraydata < 0 || a->arraydata > 2047 || a->arraydata%8 ||
	   a->arraytype < 0 || a->arraytype > 2047 || a->arraytype%8 ||
	   a->typesize < 0 || a->typesize > 2047 || a->typesize%4 ||
	   a->typenp < 0 || a->typenp > 2047 || a->typenp%4)
		return 0;
	arrayend=a->arraylen+a->wordbytes;
	if(arrayend<a->arraydata+8)arrayend=a->arraydata+8;
	if(arrayend<a->arraytype+8)arrayend=a->arraytype+8;
	typeend=a->typesize+4>a->typenp+4 ? a->typesize+4 : a->typenp+4;
	x64byte(b, 0xeb); x64byte(b, 3); fallback=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->s); x64pcspan(b, 8, 8); x64load(b, 0, 8);
	/* Adding one maps C null and H onto the unsigned interval [0,1]. */
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64sliceempty(b, 0x77);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc0);
	x64caseptr(b, 1, a->d); x64pcspan(b, 9, 8); x64load(b, 1, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff);
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc9);
	x64pcspan(b, 8, arrayend); x64pcspan(b, 9, arrayend);
	x64byte(b, 0x41); x64byte(b, 0xf6); x64byte(b, 0xc0); x64byte(b, 7);
	x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x41); x64byte(b, 0xf6); x64byte(b, 0xc1); x64byte(b, 7);
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 0, 8, a->arraylen, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, 256);
	x64moduleguard(b, 0x76, fallback);
	x64caseptr(b, 2, a->m); x64pcspan(b, 10, a->wordbytes); x64load(b, 2, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xd6); /* rsi=index */
	x64casefield(b, 1, 9, a->arraylen, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64moduleguard(b, 0x79, fallback);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xca);
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x48); x64byte(b, 0x29); x64byte(b, 0xd1);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc8);
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64sliceempty(b, 0x75);
	x64byte(b, 0xeb); x64byte(b, 3); fallback=b->n;
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* index is nonnegative and bounded by a signed WORD destination length. */
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc2);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xea); x64byte(b, 31);
	x64byte(b, 0x85); x64byte(b, 0xd2);
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 2, 9, a->arraytype, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2);
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xff);
	x64moduleguard(b, 0x75, fallback);
	/* Keep the descriptor in RCX while the span helper borrows RDX. */
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xd1);
	x64pcspan(b, 1, typeend);
	x64byte(b, 0xf6); x64byte(b, 0xc1); x64byte(b, 7); x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xca);
	x64casefield(b, 1, 2, a->typesize, 4);
	/* Positive bounded stride; products fit 64 bits before their byte guards. */
	x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1);
	x64moduleguard(b, 0x7d, fallback);
	x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 256);
	x64moduleguard(b, 0x7e, fallback);
	x64byte(b, 0x48); x64byte(b, 0x0f); x64byte(b, 0xaf); x64byte(b, 0xc1); // imul rax,rcx: byte count.
	x64byte(b, 0x48); x64byte(b, 0x0f); x64byte(b, 0xaf); x64byte(b, 0xf1); // imul rsi,rcx: byte offset.
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, 256);
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x0c); x64byte(b, 0x06); // lea rcx,[rsi+rax].
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe9); x64byte(b, 31);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 2, a->typenp, 4);
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 8, a->arraydata, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc8);
	x64casefield(b, 1, 9, a->arraydata, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xca);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xf2);
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc2);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc2);
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd2);
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xc2);
	x64moduleguard(b, 0x73, fallback);
	/* Equal physical spans require no stores after the complete preflight. */
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc2);
	x64byte(b, 0x0f); x64byte(b, 0x84); identical=b->n; x64word(b, 0);
	if(!x64copykernel(b))return 0;
	if(!x64casepatch(b, identical, b->n))return 0;
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


/* Check the reference cell and complete len/max span without clobbering either owner. */
static void
x64appendowner(X64Buf *b, int owner, int reference, int extent, int fallback)
{
	/* Heap-backed strings must be aligned before reference or header loads. */
	x64byte(b, 0xf6); x64byte(b, 0xc0 | owner); x64byte(b, 7); // test owner low byte,7.
	x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x80 | owner);
	x64word(b, -reference-1); // lea r8,[owner-reference-1].
	x64byte(b, 0x49); x64byte(b, 0x81); x64byte(b, 0xf8);
	x64word(b, -reference-extent-2);
	x64moduleguard(b, 0x76, fallback);
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
	/* Validate both readable operand slots before capturing either owner. */
	x64pcspan(b, 8, 8);
	x64ptr(b, 2, a->s); x64pcspan(b, 10, 8);
	x64load(b, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2);
	x64ptr(b, 0, a->s); x64load(b, 0, 8);
	x64appendowner(b, 2, a->heapref, a->stringdata, fallback);
	x64appendowner(b, 0, a->heapref, a->stringdata, fallback);
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
	int absent, empty, minus, digits, loop, positive, i, fallback;
	unsigned int distance;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047) return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* Bound both decoded slots before any String load or floating conversion. */
	x64ptr(b, 2, a->d); x64pcspan(b, 10, p->bytes);
	x64ptr(b, 0, a->s); x64pcspan(b, 8, 8); x64load(b, 0, 8);
	x64byte(b, 0x45); x64byte(b, 0x31); x64byte(b, 0xc0); /* r8d=value */
	x64byte(b, 0x45); x64byte(b, 0x31); x64byte(b, 0xc9); /* r9d=negative */
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff);
	x64byte(b, 0x0f); x64byte(b, 0x84); absent=b->n; x64word(b, 0);
	x64pcspan(b, 0, a->stringdata);
	x64field(b, 1, 0, a->stringlen, 4);
	x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, p->input+1); x64moduleguard(b, 0x76, fallback);
	x64field(b, 2, 0, a->stringmax, 4);
	x64byte(b, 0x39); x64byte(b, 0xca); x64moduleguard(b, 0x7d, fallback);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0xb0); x64word(b, a->stringdata);
	/* Include the NUL cell while checking the complete owner-relative span. */
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x54); x64byte(b, 0x0e); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc2); x64moduleguard(b, 0x73, fallback);
	/* string2c would terminate ASCII; decline if that would change VM bytes. */
	x64byte(b, 0x80); x64byte(b, 0x3c); x64byte(b, 0x0e); x64byte(b, 0); x64moduleguard(b, 0x74, fallback);
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
	x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, p->input); x64moduleguard(b, 0x76, fallback);
	/* strtod rejects sign-only input; leave that case to its existing C path. */
	if(p->alu){
		x64byte(b, 0x85); x64byte(b, 0xc9); x64moduleguard(b, 0x75, fallback);
	}
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x74); positive=b->n; x64byte(b, 0);
	loop=b->n;
	x64byte(b, 0x0f); x64byte(b, 0xb6); x64byte(b, 0x16);
	x64byte(b, 0x83); x64byte(b, 0xea); x64byte(b, '0');
	x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 9); x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x6b); x64byte(b, 0xc0); x64byte(b, 10);
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xd0);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc6);
	x64byte(b, 0xff); x64byte(b, 0xc9);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(!p->alu){
		/* At most 19 digits fit unsigned 64 bits; now enforce the selected signed range. */
		if(p->bytes == 4){
			x64byte(b, 0xb8); x64word(b, 0x7fffffff);
		}else{
			x64byte(b, 0x48); x64byte(b, 0xb8);
			x64word(b, 0xffffffffU); x64word(b, 0x7fffffff);
		}
		x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xc8); // ADD rax,r9: MIN permits one extra magnitude.
		x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xc0); x64moduleguard(b, 0x76, fallback);
	}
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


/* Read a captured string cell only after bounding its native byte extent. */
static int
x64stringindex(X64Buf *b, LeafLayout *a)
{
	int other, done, fallback;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047) return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* Preflight every decoded slot before capturing operands or reading headers. */
	x64ptr(b, 0, a->s); x64pcspan(b, 8, 8);
	x64ptr(b, 0, a->m); x64pcspan(b, 8, a->wordbytes);
	x64ptr(b, 0, a->d); x64pcspan(b, 8, a->wordbytes);
	x64ptr(b, 0, a->s); x64load(b, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff); x64moduleguard(b, 0x75, fallback);
	x64pcspan(b, 0, a->stringdata);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc3); /* r11 = owner */
	x64field(b, 1, 0, a->stringlen, 4);
	x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 0x80000000U); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x89); x64byte(b, 0xca); /* edx = representation sign */
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); x64byte(b, 2); x64byte(b, 0xf7); x64byte(b, 0xd9);
	/* Reject a logical length beyond backing capacity before reading characters. */
	x64field(b, 0, 11, a->stringmax, 4);
	x64byte(b, 0x39); x64byte(b, 0xc8); x64moduleguard(b, 0x7d, fallback);
	x64ptr(b, 0, a->m); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc8); x64moduleguard(b, 0x72, fallback);
	x64byte(b, 0x85); x64byte(b, 0xd2);
	x64byte(b, 0x78); other = b->n; x64byte(b, 0);
	/* Form a checked character end before reading through its trailing displacement. */
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0xb4); x64byte(b, 0x03); x64word(b, a->stringdata+1);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xde); x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x0f); x64byte(b, 0xb6); x64byte(b, 0x46); x64byte(b, -1);
	x64byte(b, 0xeb); done = b->n; x64byte(b, 0);
	b->bytes[other] = b->n-other-1;
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0xb4); x64byte(b, 0x83); x64word(b, a->stringdata+4);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xde); x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x48); x64byte(b, 0x63); x64byte(b, 0x46); x64byte(b, -4);
	b->bytes[done] = b->n-done-1;
	x64ptr(b, 2, a->d); x64store(b, 0, a->wordbytes);
	return 1;
}


/* Update or append a character only within valid, uniquely owned capacity. */
static int
x64stringstore(X64Buf *b, LeafLayout *a)
{
	int absolute, wide, done, unchanged, positive, fallback;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8) return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* Preflight every decoded slot before capturing operands or reading headers. */
	x64ptr(b, 0, a->s); x64pcspan(b, 8, a->wordbytes);
	x64ptr(b, 0, a->m); x64pcspan(b, 8, a->wordbytes);
	x64ptr(b, 0, a->d); x64pcspan(b, 8, 8);
	x64ptr(b, 0, a->s); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x41); x64byte(b, 0x89); x64byte(b, 0xc0); /* r8d=Rune(value) */
	x64ptr(b, 1, a->m); x64load(b, 1, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xce); /* rsi=full WORD index */
	x64ptr(b, 2, a->d); x64load(b, 2, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xff); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0xf6); x64byte(b, 0xc2); x64byte(b, 7); // test dl,7 before owner reads.
	x64moduleguard(b, 0x74, fallback);
	/* rax bounds the backward reference cell and forward header; r8 retains the Rune. */
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x82); x64word(b, -a->heapref-1);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, -a->heapref-a->stringdata-2);
	x64moduleguard(b, 0x76, fallback);
	x64field(b, 0, 2, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 1); x64moduleguard(b, 0x74, fallback);
	x64field(b, 1, 2, a->stringlen, 4);
	x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 0x80000000U); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x41); x64byte(b, 0x89); x64byte(b, 0xc9); /* preserve representation */
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); absolute=b->n; x64byte(b, 0);
	x64byte(b, 0xf7); x64byte(b, 0xd9);
	b->bytes[absolute]=b->n-absolute-1;
	x64field(b, 0, 2, a->stringmax, 4);
	x64byte(b, 0x39); x64byte(b, 0xc8); x64moduleguard(b, 0x7d, fallback);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc6); x64moduleguard(b, 0x72, fallback);
	x64byte(b, 0x45); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x78); wide=b->n; x64byte(b, 0);
	x64byte(b, 0x41); x64byte(b, 0x81); x64byte(b, 0xf8); x64word(b, 128); x64moduleguard(b, 0x72, fallback);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x84); x64byte(b, 0x32); x64word(b, a->stringdata+1);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd0); x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x44); x64byte(b, 0x88); x64byte(b, 0x40); x64byte(b, -1); // Store through the checked end minus one.
	x64byte(b, 0xeb); done=b->n; x64byte(b, 0);
	b->bytes[wide]=b->n-wide-1;
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x84); x64byte(b, 0xb2); x64word(b, a->stringdata+4);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd0); x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x44); x64byte(b, 0x89); x64byte(b, 0x40); x64byte(b, -4); // Store through the checked Rune end.
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
	int header, origin, positive, scaled, storemax, fallback, countloop, digitloop;
	unsigned long long identity;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 128 || a->heapref%8 || a->refbytes != 8 ||
	   a->heaptype <= 0 || a->heaptype > 128 || a->heaptype%8 ||
	   a->stringtype == nil || a->stringtype == (void*)-1)
		return 0;
	header = a->stringlen > a->stringmax ? a->stringlen+4 : a->stringmax+4;
	origin = a->heapref > a->heaptype ? a->heapref : a->heaptype;
	if(a->heapcolor > origin && a->heapcolor <= 255)origin=a->heapcolor;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* Validate both decoded cells before consuming either value. */
	x64caseptr(b, 2, a->d); x64pcspan(b, 10, 8);
	x64caseptr(b, 0, a->s); x64pcspan(b, 8, op->bytes); x64load(b, 0, op->bytes);
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
	/* Bound the backward Heap cells and header without changing the captured value. */
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x8a); x64word(b, -origin-1);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, -origin-header-2);
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0xf6); x64byte(b, 0xc2); x64byte(b, 7); x64moduleguard(b, 0x74, fallback);
	/* Decline non-string pointer temporaries before reading a String field. */
	x64casefield(b, 1, 2, -a->heaptype, 8);
	identity = (unsigned long long)a->stringtype;
	x64byte(b, 0x49); x64byte(b, 0xb8); // movabs r8,Tstring
	x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xc1); // cmp rcx,r8
	x64moduleguard(b, 0x74, fallback);
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
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xca); x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xff); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); storemax=b->n; x64byte(b, 0);
	if(a->stringmax<=127){
		x64byte(b, 0x41); x64byte(b, 0x89); x64byte(b, 0x72); x64byte(b, a->stringmax);
	}else{
		x64byte(b, 0x41); x64byte(b, 0x89); x64byte(b, 0xb2); x64word(b, a->stringmax);
	}
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[storemax]=b->n-storemax-1;
	x64caseptr(b, 1, a->d); // Keep the original result slot in r9 before payload writes.
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0xb2); x64word(b, a->stringdata); // lea rsi,[r10+data].
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
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0x11); // Restore owner through captured r9.
	return 1;
}


/* Update a unique prefix or move a bounded substring downward within its owner. */
static int
x64stringslice(X64Buf *b, LeafLayout *a)
{
	int positive, ascii, loop, done, prefix, fallback;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8) return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* Validate all slots before capturing geometry or the destination owner. */
	x64ptr(b, 0, a->s); x64pcspan(b, 8, a->wordbytes);
	x64ptr(b, 0, a->m); x64pcspan(b, 8, a->wordbytes);
	x64ptr(b, 0, a->d); x64pcspan(b, 8, 8);
	x64ptr(b, 0, a->s); x64load(b, 0, a->wordbytes);
	x64ptr(b, 1, a->m); x64load(b, 1, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x79, fallback);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xc1); x64moduleguard(b, 0x77, fallback);
	/* C rejects a negative or non-Dis end before inspecting the owner. */
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 0x7fffffff);
	x64moduleguard(b, 0x76, fallback);
	x64ptr(b, 2, a->d); x64load(b, 2, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0xff); x64moduleguard(b, 0x75, fallback);
	/* Prefixes access only metadata: do not require an unused payload base to fit. */
	x64appendowner(b, 2, a->heapref,
		a->stringlen+4 > a->stringmax+4 ? a->stringlen+4 : a->stringmax+4, fallback);
	/* r8 retains the signed representation; r9 is its native absolute length. */
	x64byte(b, 0x4c); x64byte(b, 0x63); x64byte(b, 0x82); x64word(b, a->stringlen);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0xc1);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x79); positive=b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0xf7); x64byte(b, 0xd9);
	b->bytes[positive]=b->n-positive-1;
	x64field(b, 6, 2, a->stringmax, 4);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xce); x64moduleguard(b, 0x7d, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xc9); x64moduleguard(b, 0x76, fallback);
	x64field(b, 6, 2, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfe); x64byte(b, 1); x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x48); x64byte(b, 0x29); x64byte(b, 0xc1);
	/* Prefixes need only a length update, so their size does not bound a copy loop. */
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0);
	x64byte(b, 0x74); prefix=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 256); x64moduleguard(b, 0x76, fallback);
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
	/* The selected end bounds both source and lower destination copy spans. */
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x04); x64byte(b, 0x08);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xd0); x64moduleguard(b, 0x73, fallback);
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
	int offsets[10], i, extent, origin;

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
		x64ptr(b, 0, a->s); x64pcspan(b, 8, 8); x64load(b, 0, 8);
		extent=a->linknlinks+4>a->linklinks ? a->linknlinks+4 : a->linklinks;
		x64pcspan(b, 0, extent);
		origin=a->heapref;
		if(a->heapcolor>origin && a->heapcolor<=255)origin=a->heapcolor;
		if(a->heaptype>origin && a->heaptype<=255)origin=a->heaptype;
		x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, origin); x64guard(b, 0x77);
		x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64guard(b, 0x77);
		/* Invalid module owners must decline before reading the index operand. */
		x64ptr(b, 1, a->m); x64pcspan(b, 9, a->wordbytes); x64field(b, 6, 9, 0, a->wordbytes);
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xf6); x64guard(b, 0x79);
		x64field(b, 1, 0, a->linknlinks, 4);
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64guard(b, 0x79);
		x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64guard(b, 0x72);
		x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe6); x64byte(b, 4);
		x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xf0); // add rax,rsi detects entry-base carry.
		x64guard(b, 0x73);
		x64pcspan(b, 0, a->linklinks+a->modlbytes);
		x64field(b, 0, 0, a->linklinks+a->modlframe, 8);
	}else{
		x64ptr(b, 0, a->s); x64pcspan(b, 8, a->wordbytes); x64load(b, 0, a->wordbytes);
		x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc6); // rsi is the complete signed index.
		x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xf6); x64guard(b, 0x79);
		x64ptr(b, 0, a->module); x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0);
		for(i=0; i<3; i++){
			/* Pointer+1 is 0/1 for H/null; no address is dereferenced on rejection. */
			x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
			x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64guard(b, 0x77);
			if(i==0){x64pcspan(b, 0, a->linkmodule+8); x64field(b, 0, 0, a->linkmodule, 8);}
			if(i==1){
				extent=a->moduletypes+8>a->modulenty+4 ? a->moduletypes+8 : a->modulenty+4;
				x64pcspan(b, 0, extent);
				x64field(b, 1, 0, a->modulenty, 4);
				x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64guard(b, 0x72);
				x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64guard(b, 0x79);
				x64field(b, 0, 0, a->moduletypes, 8);
			}
		}
		x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xf2); // mov rdx,rsi.
		x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe2); x64byte(b, 3);
		x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xd0); // add rax,rdx detects table-slot carry.
		x64guard(b, 0x73);
		x64pcspan(b, 0, 8);
		x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x00);
	}
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64guard(b, 0x77);
	x64pcspan(b, 0, a->typesize+4>a->typenp+4 ? a->typesize+4 : a->typenp+4);
	x64byte(b, 0xa8); x64byte(b, 7); x64guard(b, 0x74); // Type must have native pointer alignment.
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
	x64ptr(b, 2, a->d); x64pcspan(b, 10, 8);
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
	int offsets[11], i, j, fallback, ordinary, done, extent, origin, early;

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
	origin=a->heapref;
	if(a->heapcolor>origin && a->heapcolor<=255)origin=a->heapcolor;
	if(a->heaptype>origin && a->heaptype<=255)origin=a->heaptype;
	extent=a->frametype;
	if(a->framemr>extent)extent=a->framemr;
	if(a->framefp>extent)extent=a->framefp;
	if(a->framelr>extent)extent=a->framelr;
	x64modulefield(b, 8, 7, a->fp, 8);
	x64pcspan(b, 8, extent+8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 9, 8, a->framefp, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x51); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64pcspan(b, 9, extent+8);
	x64modulefield(b, 10, 8, a->framelr, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x52); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	/* Preserve C root/invalid-link precedence before touching a descriptor. */
	x64modulefield(b, 11, 8, a->framemr, 8);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xfb); x64byte(b, 0xff);
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xdb);
	x64byte(b, 0x0f); x64byte(b, 0x84); early=b->n; x64word(b, 0);
	x64modulefield(b, 6, 7, a->module, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x56); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1);
	x64moduleguard(b, 0x77, fallback);
	if(!x64casepatch(b, early, b->n))return 0;
	x64modulefield(b, 0, 8, a->frametype, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64pcspan(b, 0, a->typenp+4);
	x64modulefield(b, 1, 0, a->typenp, 4);
	x64byte(b, 0x85); x64byte(b, 0xc9); x64moduleguard(b, 0x74, fallback);
	/* A nil saved module skips every current-module and heap-header read. */
	x64modulefield(b, 11, 8, a->framemr, 8);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xdb);
	x64byte(b, 0x0f); x64byte(b, 0x84); ordinary=b->n; x64word(b, 0);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x53); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 0, 7, a->module, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64pcspan(b, 11, a->linkmp+8>a->linkcompiled+4 ? a->linkmp+8 : a->linkcompiled+4);
	x64pcspan(b, 0, a->linkcompiled+4);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, origin);
	x64moduleguard(b, 0x77, fallback);
	x64modulefield(b, 1, 0, a->linkcompiled, 4);
	x64modulefield(b, 2, 11, a->linkcompiled, 4);
	x64byte(b, 0x39); x64byte(b, 0xd1); x64moduleguard(b, 0x74, fallback);
	/* rax/rcx capture the owner and count; rsi captures caller MP before stores. */
	x64modulefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9);
	x64modulefield(b, 6, 11, a->linkmp, 8);
	if(b->n>sizeof(b->bytes) || !x64casepatch(b, ordinary, b->n))return 0;
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
	int offsets[11], i, fallback, extent, origin, native;

	offsets[0]=a->module; offsets[1]=a->fp; offsets[2]=a->regmp; offsets[3]=a->linkprog;
	offsets[4]=a->linkmp; offsets[5]=a->linklinks; offsets[6]=a->framelr;
	offsets[7]=a->framefp; offsets[8]=a->framemr; offsets[9]=a->regtemp; offsets[10]=a->linknlinks;
	for(i=0;i<11;i++)
		if(offsets[i]<0 || offsets[i]>2047 || offsets[i]%(i<9 ? 8 : i==9 ? a->wordbytes : 4))return 0;
	if(a->instbytes<=0 || a->instbytes>2047 ||
	   a->linkcompiled<0 || a->linkcompiled>2047 || a->linkcompiled%4 ||
	   a->modlbytes!=16 || a->modlpc<0 || a->modlpc>8 || a->modlpc%8 || a->heapref<=0 || a->heapref>128 || a->heapref%8 || a->refbytes!=8 ||
	   a->framelr==a->framefp || a->framelr==a->framemr || a->framefp==a->framemr)return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	origin=a->heapref;
	if(a->heapcolor>origin && a->heapcolor<=255)origin=a->heapcolor;
	if(a->heaptype>origin && a->heaptype<=255)origin=a->heaptype;
	extent=a->linkprog+8;
	if(a->linkcompiled+4>extent)extent=a->linkcompiled+4;
	if(a->linknlinks+4>extent)extent=a->linknlinks+4;
	if(a->linkmp+8>extent)extent=a->linkmp+8;
	if(a->linklinks>extent)extent=a->linklinks;
	x64ptr(b, 0, a->d); x64pcspan(b, 8, 8); x64load(b, 0, 8);
	x64pcspan(b, 0, extent);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, origin);
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc0); // r8 is the module owner.
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x9f); x64word(b, a->module);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x53); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64pcspan(b, 11, a->linkcompiled+4);
	x64modulefield(b, 1, 8, a->linkcompiled, 4);
	x64modulefield(b, 2, 11, a->linkcompiled, 4);
	x64byte(b, 0x39); x64byte(b, 0xd1); x64moduleguard(b, 0x74, fallback);
	x64modulefield(b, 1, 0, a->linkprog, 8);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64ptr(b, 1, a->m); x64pcspan(b, 9, a->wordbytes); x64modulefield(b, 6, 9, 0, a->wordbytes);
	x64modulefield(b, 1, 0, a->linknlinks, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64moduleguard(b, 0x79, fallback);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xce); x64moduleguard(b, 0x72, fallback);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe6); x64byte(b, 4);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc2); // mov r10,rax.
	x64byte(b, 0x49); x64byte(b, 0x01); x64byte(b, 0xf2); // add r10,rsi detects entry-base wrap.
	x64moduleguard(b, 0x73, fallback);
	x64pcspan(b, 10, a->linklinks+a->modlpc+8);
	x64modulefield(b, 10, 10, a->linklinks+a->modlpc, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x52); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	/* Compiled callees carry opaque native PC tokens, not Inst objects. */
	x64modulefield(b, 1, 8, a->linkcompiled, 4);
	x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x75); native=b->n; x64byte(b, 0);
	x64pcspan(b, 10, a->instbytes);
	b->bytes[native]=b->n-native-1;
	x64ptr(b, 1, a->s); x64pcspan(b, 9, 8); x64load(b, 1, 8);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc9); // r9 is the captured frame.
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	extent=a->framelr;
	if(a->framefp>extent)extent=a->framefp;
	if(a->framemr>extent)extent=a->framemr;
	x64pcspan(b, 9, extent+8);
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
	int fields[7], i, j, fallback, empty, rootcase, rootdone, arrayend, typeend;

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
	arrayend=a->arraylen+a->wordbytes;
	if(arrayend<a->arraydata+8)arrayend=a->arraydata+8;
	if(arrayend<a->arrayroot+8)arrayend=a->arrayroot+8;
	if(arrayend<a->arraytype+8)arrayend=a->arraytype+8;
	typeend=a->typesize+4;
	if(typeend<a->typeref+4)typeend=a->typeref+4;
	if(typeend<a->typenp+4)typeend=a->typenp+4;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* r8/r9 retain start/end; r10 keeps the view throughout every guard. */
	x64ptr(b, 0, a->s); x64pcspan(b, 8, a->wordbytes); x64modulefield(b, 8, 8, 0, a->wordbytes);
	x64ptr(b, 1, a->m); x64pcspan(b, 9, a->wordbytes); x64modulefield(b, 9, 9, 0, a->wordbytes);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x79, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc1); x64moduleguard(b, 0x7d, fallback);
	x64ptr(b, 2, a->d); x64pcspan(b, 10, 8); x64modulefield(b, 10, 10, 0, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x4a); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64pcspan(b, 10, arrayend);
	x64byte(b, 0x49); x64byte(b, 0x81); x64byte(b, 0xfa); x64word(b, a->heapref+8);
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x41); x64byte(b, 0xf6); x64byte(b, 0xc2); x64byte(b, 7);
	x64moduleguard(b, 0x74, fallback);
	x64modulefield(b, 1, 10, a->arraylen, a->wordbytes);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xc9); x64moduleguard(b, 0x7e, fallback);
	x64modulefield(b, 0, 10, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 1); x64moduleguard(b, 0x74, fallback);
	x64modulefield(b, 11, 10, a->arraytype, 8);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x4b); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 1); x64moduleguard(b, 0x77, fallback);
	x64pcspan(b, 11, typeend);
	x64byte(b, 0x41); x64byte(b, 0xf6); x64byte(b, 0xc3); x64byte(b, 7);
	x64moduleguard(b, 0x74, fallback);
	x64modulefield(b, 0, 11, a->typeref, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x7f, fallback);
	/* Root arrays may reuse their header only when no element owns pointers. */
	x64modulefield(b, 0, 10, a->arrayroot, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x48); x64byte(b, 1);
	x64byte(b, 0xe3); rootcase=b->n; x64byte(b, 0); // JRCXZ recognizes the H sentinel.
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, a->heapref+8);
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0xa8); x64byte(b, 7); x64moduleguard(b, 0x74, fallback);
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


/* Reuse unique ASCII string storage after every source/type/span/byte check succeeds. */
static int
x64bytestring(X64Buf *b, LeafLayout *a)
{
	int fallback, header, scan, backward, forward, loop, done, jump;
	unsigned long long identity;

	if(!leafbyteconversionlayout(a))
		return 0;
	header = a->arraylen;
	if(header < a->arraydata) header = a->arraydata;
	if(header < a->arraytype) header = a->arraytype;
	if(header < a->arrayroot) header = a->arrayroot;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback = b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* Validate both decoded cells before consuming either value. */
	x64caseptr(b, 2, a->d); x64pcspan(b, 10, 8);
	x64caseptr(b, 0, a->s); x64pcspan(b, 8, 8); x64load(b, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, a->heapcolor);
	x64moduleguard(b, 0x77, fallback);
	x64readspan(b, header, 8);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc0); // mov r8, rax
	x64casefield(b, 1, 0, -a->heaptype, 8);
	identity = (unsigned long long)a->arrayidentity;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x75, fallback);
	x64casefield(b, 1, 0, a->arrayroot, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff); // cmp rcx, -1
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 0, 8, a->arraytype, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); // test rax, rax
	x64moduleguard(b, 0x75, fallback);
	x64readspan(b, a->typesize > a->typenp ? a->typesize : a->typenp, 4);
	x64casefield(b, 1, 0, a->typenp, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0x01); // cmp rcx, 1
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 8, a->arraylen, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9); // dec rcx
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64byte(b, 0xff); x64byte(b, 0x00); x64byte(b, 0x00); x64byte(b, 0x00); // cmp rcx, 255
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1); // inc rcx
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xcb); // mov r11, rcx
	x64casefield(b, 1, 8, a->arraydata, 8);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc9); // mov r9, rcx
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xd9); // add rcx, r11
	x64moduleguard(b, 0x73, fallback);
	x64caseptr(b, 2, a->d);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0xd0); // mov r8, r10
	x64casefield(b, 0, 10, 0, 8);
	x64byte(b, 0xa8); x64byte(b, 7); // test al,7 before inspecting an owning result.
	x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, a->heapcolor);
	x64moduleguard(b, 0x77, fallback);
	x64readspan(b, a->stringlen > a->stringmax ? a->stringlen : a->stringmax, 4);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc2); // mov r10, rax
	x64casefield(b, 1, 0, -a->heaptype, 8);
	identity = (unsigned long long)a->stringtype;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0x01); // cmp rcx, 1
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, a->stringlen, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x79, fallback);
	x64casefield(b, 2, 0, a->stringmax, 4);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x7e, fallback);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xd3); // cmp r11, rdx
	x64moduleguard(b, 0x7e, fallback);
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0xb2); x64word(b, a->stringdata);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xd6); // cmp rsi, r10
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xf2); // mov rdx, rsi
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xda); // add rdx, r11
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x31); x64byte(b, 0xc9); // xor ecx, ecx
	scan = b->n;
	x64byte(b, 0x41); x64byte(b, 0x0f); x64byte(b, 0xb6); x64byte(b, 0x04); x64byte(b, 0x09); // movzx eax, byte ptr [r9+rcx]
	x64byte(b, 0x3d); x64byte(b, 0x80); x64byte(b, 0x00); x64byte(b, 0x00); x64byte(b, 0x00); // cmp eax, 128
	x64moduleguard(b, 0x72, fallback);
	x64byte(b, 0xff); x64byte(b, 0xc1); // inc ecx
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xd9); // cmp rcx, r11
	x64byte(b, 0x72); x64byte(b, scan-b->n-1);
	/* All reads which can reject are complete; copying now follows memmove order. */
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xce); // cmp rsi, r9
	x64byte(b, 0x76); forward = b->n; x64byte(b, 0);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd9); // mov rcx, r11
	backward = b->n;
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9); // dec rcx
	x64byte(b, 0x41); x64byte(b, 0x8a); x64byte(b, 0x04); x64byte(b, 0x09); // mov al, byte ptr [r9+rcx]
	x64byte(b, 0x88); x64byte(b, 0x04); x64byte(b, 0x0e); // mov byte ptr [rsi+rcx], al
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64byte(b, 0x75); x64byte(b, backward-b->n-1);
	x64byte(b, 0xeb); jump = b->n; x64byte(b, 0);
	if(b->n > sizeof(b->bytes)) return 0;
	done = b->n; b->bytes[forward] = done-forward-1;
	x64byte(b, 0x31); x64byte(b, 0xc9); // xor ecx, ecx
	loop = b->n;
	x64byte(b, 0x41); x64byte(b, 0x8a); x64byte(b, 0x04); x64byte(b, 0x09); // mov al, byte ptr [r9+rcx]
	x64byte(b, 0x88); x64byte(b, 0x04); x64byte(b, 0x0e); // mov byte ptr [rsi+rcx], al
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1); // inc rcx
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xd9); // cmp rcx, r11
	x64byte(b, 0x72); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[jump] = b->n-jump-1;
	x64byte(b, 0x45); x64byte(b, 0x89); x64byte(b, 0x9a); x64word(b, a->stringlen);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0x10); // mov qword ptr [r8], r10
	return 1;
}


/* Export byte-backed strings into unique byte arrays after complete owner/span preflight. */
static int
x64stringbytes(X64Buf *b, LeafLayout *a)
{
	int fallback, header, backward, forward, loop, done, jump;
	unsigned long long identity;

	if(!leafbyteconversionlayout(a))
		return 0;
	header = a->arraylen;
	if(header < a->arraydata) header = a->arraydata;
	if(header < a->arraytype) header = a->arraytype;
	if(header < a->arrayroot) header = a->arrayroot;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback = b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	/* Validate both decoded cells before consuming either value. */
	x64caseptr(b, 2, a->d); x64pcspan(b, 10, 8);
	x64caseptr(b, 0, a->s); x64pcspan(b, 8, 8); x64load(b, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, a->heapcolor);
	x64moduleguard(b, 0x77, fallback);
	x64readspan(b, a->stringlen > a->stringmax ? a->stringlen : a->stringmax, 4);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc0); // mov r8, rax
	x64casefield(b, 1, 0, -a->heaptype, 8);
	identity = (unsigned long long)a->stringtype;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x75, fallback);
	x64casefield(b, 1, 0, a->stringlen, 4);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9); // dec rcx
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64byte(b, 0xff); x64byte(b, 0x00); x64byte(b, 0x00); x64byte(b, 0x00); // cmp rcx, 255
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1); // inc rcx
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xcb); // mov r11, rcx
	x64casefield(b, 2, 0, a->stringmax, 4);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x7e, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x8d); x64byte(b, 0x88); x64word(b, a->stringdata); // lea r9,[r8+data].
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xc1); // cmp r9, r8
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xca); // mov rdx, r9
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xda); // add rdx, r11
	x64moduleguard(b, 0x73, fallback);
	x64caseptr(b, 2, a->d);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0xd0); // mov r8, r10
	x64casefield(b, 0, 10, 0, 8);
	x64byte(b, 0xa8); x64byte(b, 7); // test al,7 before inspecting an owning result.
	x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, a->heapcolor);
	x64moduleguard(b, 0x77, fallback);
	x64readspan(b, header, 8);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc2); // mov r10, rax
	x64casefield(b, 1, 0, -a->heaptype, 8);
	identity = (unsigned long long)a->arrayidentity;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0x01); // cmp rcx, 1
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, a->arrayroot, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff); // cmp rcx, -1
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, a->arraylen, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0x7f); // cmp rcx, 0x7fffffff
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xcb); // cmp r11, rcx
	x64moduleguard(b, 0x76, fallback);
	x64casefield(b, 0, 10, a->arraytype, 8);
	x64byte(b, 0xa8); x64byte(b, 7); // Reused array descriptors must be naturally aligned.
	x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); // test rax, rax
	x64moduleguard(b, 0x75, fallback);
	header = a->typesize > a->typenp ? a->typesize : a->typenp;
	if(header < a->typeref) header = a->typeref;
	x64readspan(b, header, 4);
	x64casefield(b, 1, 0, a->typeref, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx,rcx.
	x64moduleguard(b, 0x7f, fallback); // A live array descriptor has a positive signed reference count.
	x64casefield(b, 1, 0, a->typenp, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0x01); // cmp rcx, 1
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 10, a->arraydata, 8);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xce); // mov rsi, rcx
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xd9); // add rcx, r11
	x64moduleguard(b, 0x73, fallback);
	/* All reads which can reject are complete; copying now follows memmove order. */
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xce); // cmp rsi, r9
	x64byte(b, 0x76); forward = b->n; x64byte(b, 0);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd9); // mov rcx, r11
	backward = b->n;
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9); // dec rcx
	x64byte(b, 0x41); x64byte(b, 0x8a); x64byte(b, 0x04); x64byte(b, 0x09); // mov al, byte ptr [r9+rcx]
	x64byte(b, 0x88); x64byte(b, 0x04); x64byte(b, 0x0e); // mov byte ptr [rsi+rcx], al
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64byte(b, 0x75); x64byte(b, backward-b->n-1);
	x64byte(b, 0xeb); jump = b->n; x64byte(b, 0);
	if(b->n > sizeof(b->bytes)) return 0;
	done = b->n; b->bytes[forward] = done-forward-1;
	x64byte(b, 0x31); x64byte(b, 0xc9); // xor ecx, ecx
	loop = b->n;
	x64byte(b, 0x41); x64byte(b, 0x8a); x64byte(b, 0x04); x64byte(b, 0x09); // mov al, byte ptr [r9+rcx]
	x64byte(b, 0x88); x64byte(b, 0x04); x64byte(b, 0x0e); // mov byte ptr [rsi+rcx], al
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc1); // inc rcx
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xd9); // cmp rcx, r11
	x64byte(b, 0x72); x64byte(b, loop-b->n-1);
	if(b->n > sizeof(b->bytes)) return 0;
	b->bytes[jump] = b->n-jump-1;
	x64byte(b, a->wordbytes == 8 ? 0x4d : 0x45); x64byte(b, 0x89); x64byte(b, 0x9a); x64word(b, a->arraylen);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0x10); // mov qword ptr [r8], r10
	return 1;
}


/* Reuse exact standard pointer-free records after checked live module/type lookup. */
static int
x64recordobject(X64Buf *b, LeafLayout *a, int mode)
{
	int fallback, header, moduleend, empty, loop, imported;
	unsigned long long identity;

	imported = mode == LeafRecordImportedZero;
	if(!leafrecordlayout(a))
		return 0;
	header=a->typeref+4;
	if(header<a->typesize+4)header=a->typesize+4;
	if(header<a->typenp+4)header=a->typenp+4;
	if(header<a->typefree+8)header=a->typefree+8;
	if(header<a->typemark+8)header=a->typemark+8;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	if(imported){x64caseptr(b, 0, a->s); x64pcspan(b, 8, 8); x64load(b, 0, 8);}
	else{x64caseptr(b, 0, a->module);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0); // mov rax, r8
	}
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01); // lea rdx, [rax+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	x64readspan(b, a->linkmodule, 8);
	x64casefield(b, 0, 0, a->linkmodule, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01); // lea rdx, [rax+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	moduleend=a->moduletypes+8;
	if(moduleend<a->modulenty+4)moduleend=a->modulenty+4;
	x64readspan(b, 0, moduleend);
	/* Span validation borrows RDX; load the module count only after it. */
	x64caseptr(b, 0, imported ? a->m : a->s); x64pcspan(b, 8, a->wordbytes);
	x64casefield(b, 2, 0, a->modulenty, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); // test rdx, rdx
	x64moduleguard(b, 0x7f, fallback);
	x64casefield(b, 1, 0, a->moduletypes, 8);
	x64caseptr(b, 0, imported ? a->m : a->s); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc6); // mov rsi, rax
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd6); // cmp rsi, rdx
	x64moduleguard(b, 0x72, fallback);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x51); x64byte(b, 0x01); // lea rdx, [rcx+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe6); x64byte(b, 0x03); // shl rsi, 3
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xf1); // add rcx, rsi
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc8); // mov rax, rcx
	x64readspan(b, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x00); // mov rax, qword ptr [rax]
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01); // lea rdx, [rax+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0xa8); x64byte(b, 0x07); // test al, 7
	x64moduleguard(b, 0x74, fallback);
	x64readspan(b, 0, header);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc1); // mov r9, rax
	x64casefield(b, 1, 0, a->typeref, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x7f, fallback);
	x64casefield(b, 1, 0, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64byte(b, 0x00); x64byte(b, 0x01); x64byte(b, 0x00); x64byte(b, 0x00); // cmp rcx, 256
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xcb); // mov r11, rcx
	x64casefield(b, 2, 0, a->typenp, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); // test rdx, rdx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, a->typefree, 8);
	identity=(unsigned long long)a->plainfree;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, a->typemark, 8);
	identity=(unsigned long long)a->plainmark;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x74, fallback);
	x64caseptr(b, 2, a->d); x64pcspan(b, 10, 8);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0xd0); // mov r8, r10
	x64casefield(b, 0, 10, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01); // lea rdx, [rax+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, a->heapcolor);
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc2); // mov r10, rax
	x64casefield(b, 1, 0, -a->heaptype, 8);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xc9); // cmp rcx, r9
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0x01); // cmp rcx, 1
	x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xdb); // test r11, r11
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2); // mov rdx, rax
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xda); // add rdx, r11
	x64moduleguard(b, 0x73, fallback);
	if(mode != LeafRecordRaw){
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd9); // mov rcx, r11
		/* No fallible operations remain after the first zero store. */
		loop=b->n;
		x64byte(b, 0xc6); x64byte(b, 0x44); x64byte(b, 0x08); x64byte(b, 0xff); x64byte(b, 0x00); // mov byte ptr [rax+rcx-1], 0
		x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9); // dec rcx
		x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	}
	if(b->n>sizeof(b->bytes) || b->n-empty-1>127)return 0;
	b->bytes[empty]=b->n-empty-1;
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0x10); // mov qword ptr [r8], r10
	return 1;
}


/* Call pure channel preflight, then copy and publish a captured buffered transaction.
 * The 64-byte transaction and saved REG use an aligned 88-byte SysV frame.
 * No runtime call remains after the first payload store. Blocking paths decline.
 */
static int
x64transfer(X64Buf *b, LeafLayout *a, int receive)
{
	unsigned long long entry;

	entry = (unsigned long long)a->channeltransferentry;
	if(entry == 0 || entry == ~0ULL)
		return 0;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xec); x64byte(b, 0x58); // sub rsp, 88
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x7c); x64byte(b, 0x24); x64byte(b, 0x40); // mov qword ptr [rsp+64], rdi
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xe2); // mov rdx, rsp
	x64byte(b, 0xbe); x64word(b, receive); // mov esi, direction.
	x64byte(b, 0x48); x64byte(b, 0xb8); x64word(b, entry); x64word(b, entry>>32);
	x64byte(b, 0xff); x64byte(b, 0xd0); // call rax
	x64byte(b, 0x85); x64byte(b, 0xc0); // test eax, eax
	x64byte(b, 0x75); x64byte(b, 0x07); // Skip the failure epilogue after successful preflight.
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc4); x64byte(b, 0x58); // add rsp, 88
	x64byte(b, 0x31); x64byte(b, 0xc0); // xor eax, eax
	x64byte(b, 0xc3); // ret
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x7c); x64byte(b, 0x24); x64byte(b, 0x40); // mov rdi, qword ptr [rsp+64]
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x04); x64byte(b, 0x24); // mov r8, qword ptr [rsp]
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x54); x64byte(b, 0x24); x64byte(b, 0x08); // mov r10, qword ptr [rsp+8]
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 0x10); // mov rax, qword ptr [rsp+16]
	if(!x64copykernel(b)) return 0;
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x4c); x64byte(b, 0x24); x64byte(b, 0x18); // mov rcx, qword ptr [rsp+24]
	x64byte(b, 0x8b); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 0x28); // mov eax, dword ptr [rsp+40]
	x64byte(b, 0x89); x64byte(b, 0x01); // mov dword ptr [rcx], eax
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x4c); x64byte(b, 0x24); x64byte(b, 0x20); // mov rcx, qword ptr [rsp+32]
	x64byte(b, 0x8b); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 0x30); // mov eax, dword ptr [rsp+48]
	x64byte(b, 0x89); x64byte(b, 0x01); // mov dword ptr [rcx], eax
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x04); x64byte(b, 0x24); // mov rax, qword ptr [rsp]
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, a->s); // Publish REG.s.
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 0x08); // mov rax, qword ptr [rsp+8]
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, a->d); // Publish REG.d.
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 0x38); // mov rax, qword ptr [rsp+56]
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x87); x64word(b, a->m); // Publish REG.m.
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc4); x64byte(b, 0x58); // add rsp, 88
	return 1;
}


/* Capture scalar/raw/mapped arguments and publish a fully initialized channel into an empty slot. */
static int
x64channel(X64Buf *b, LeafLayout *a, int op)
{
	int fallback, unbuffered, raw, mapped;
	unsigned long long entry;

	raw = op == INEWCM;
	mapped = op == INEWCMP;
	entry = (unsigned long long)(raw ? a->rawchannelentry : mapped ? a->mappedchannelentry : a->channelentry);
	if((raw && (a->channelmid < 40 || a->channelmid > 2047 || a->channelmid%8)) ||
	   (mapped && (a->module < 0 || a->module > 2047 || a->module%8)))
		return 0;
	if(entry == 0 || entry == ~0ULL)
		return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 2, a->d);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd0); // mov rax, r10
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01); // lea rdx, [rax+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	x64readspan(b, 0, 8);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0x3a); x64byte(b, 0xff); // cmp qword ptr [r10], -1
	x64moduleguard(b, 0x74, fallback);
	if(raw || mapped){
		x64caseptr(b, 0, a->s);
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0); // mov rax, r8
		x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01);
		x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01);
		x64moduleguard(b, 0x77, fallback);
		x64readspan(b, 0, a->wordbytes);
		x64load(b, 0, a->wordbytes);
		if(raw){
			x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, 0x7fffffff);
			x64moduleguard(b, 0x76, fallback);
		}
		x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc3); // mov r11, captured width/index
	}
	x64caseptr(b, 0, a->m);
	x64byte(b, 0x31); x64byte(b, 0xf6); // xor esi, esi
	x64byte(b, 0x4d); x64byte(b, 0x39); x64byte(b, 0xd0); // cmp r8, r10
	x64byte(b, 0x74); unbuffered=b->n; x64byte(b, 0);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0); // mov rax, r8
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01);
	x64moduleguard(b, 0x77, fallback);
	x64readspan(b, 0, a->wordbytes);
	x64load(b, 0, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, 0x7fffffff); // cmp rax, INT_MAX
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc6); // mov rsi, rax
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[unbuffered]=b->n-unbuffered-1;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xec); x64byte(b, 0x18); // sub rsp, 24
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x14); x64byte(b, 0x24); // mov [rsp], r10
	if(raw || mapped){
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x5c); x64byte(b, 0x24); x64byte(b, 0x08); // mov [rsp+8], r11
		if(mapped){
			x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xf2); // mov rdx, capacity
			x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xde); // mov rsi, index
			x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0xbf); x64word(b, a->module); // mov rdi, REG.M
		}else{
			x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xdf); // mov rdi, raw width
		}
	}else{
		x64byte(b, 0xbf); x64word(b, op); // mov edi, scalar opcode
	}
	x64byte(b, 0x48); x64byte(b, 0xb8); x64word(b, entry); x64word(b, entry>>32);
	x64byte(b, 0xff); x64byte(b, 0xd0); // call rax
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x14); x64byte(b, 0x24); // mov r10, [rsp]
	if(raw){
		x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x4c); x64byte(b, 0x24); x64byte(b, 0x08); // mov rcx, captured width
		x64byte(b, a->wordbytes == 8 ? 0x48 : 0x40); x64byte(b, 0x89); x64byte(b, 0x88); x64word(b, a->channelmid); // mov Channel.mid.w, ecx/rcx
	}
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x02); // Publish only the initialized channel.
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc4); x64byte(b, 0x18); // add rsp, 24
	return 1;
}


/* Capture a scalar before the allocator can publish through an overlapping destination. */
static int
x64scalarcons(X64Buf *b, LeafLayout *a, LeafOp *p)
{
	unsigned long long entry;

	if(!leafscalarconslayout(a, p->bytes))
		return 0;
	x64caseptr(b, 0, a->s); x64pcspan(b, 8, p->bytes); x64load(b, 0, p->bytes);
	x64caseptr(b, 1, a->d); x64pcspan(b, 9, 8);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xce); // mov rsi, r9
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xec); x64byte(b, 0x18); // sub rsp, 24
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x04); x64byte(b, 0x24); // mov qword ptr [rsp], rax
	x64byte(b, 0xbf); x64word(b, p->input); // mov edi, allocation width
	entry=(unsigned long long)a->consentry;
	x64byte(b, 0x48); x64byte(b, 0xb8); x64word(b, entry); x64word(b, entry>>32);
	x64byte(b, 0xff); x64byte(b, 0xd0); // call rax
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x0c); x64byte(b, 0x24); // mov rcx, qword ptr [rsp]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc4); x64byte(b, 0x18); // add rsp, 24
	x64byte(b, 0x48); x64byte(b, 0x05); x64word(b, a->listdata); // add rax, payload offset
	x64byte(b, p->bytes == 8 ? 0x48 : 0x40); x64byte(b, p->bytes == 1 ? 0x88 : 0x89);
	x64byte(b, 0x08); // mov byte/dword/qword ptr [rax], cl/ecx/rcx
	return 1;
}




/* Restore the private constructor frame on every post-callback guard rejection. */
static void
x64consguard(X64Buf *b, int success, int fallback)
{
	x64byte(b, success); x64byte(b, 5);
	x64byte(b, 0xe9); x64word(b, fallback-b->n-4);
}


/* Validate a live descriptor, then allocate/copy/retain a bounded pointer-free list payload. */
static int
x64mappedcons(X64Buf *b, LeafLayout *a)
{
	int fallback, framed, empty, loop;
	unsigned long long entry;

	if(!leafmappedconslayout(a))return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->s);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1);
	x64moduleguard(b, 0x77, fallback); x64readspan(b, 0, 256);
	x64caseptr(b, 2, a->d);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd0);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1);
	x64moduleguard(b, 0x77, fallback); x64readspan(b, 0, 8);
	x64caseptr(b, 1, a->m);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc8);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 1);
	x64moduleguard(b, 0x77, fallback); x64readspan(b, 0, a->wordbytes);
	x64load(b, 1, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xec); x64byte(b, 56);
	x64byte(b, 0xeb); x64byte(b, 7);
	framed=b->n;
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc4); x64byte(b, 56);
	x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 8);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x54); x64byte(b, 0x24); x64byte(b, 16);
	x64byte(b, 0x49); x64byte(b, 0x8b); x64byte(b, 0x02);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x04); x64byte(b, 0x24);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xce); // mov rsi, index
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0xbf); x64word(b, a->module);
	entry=(unsigned long long)a->constypeentry;
	x64byte(b, 0x48); x64byte(b, 0xb8); x64word(b, entry); x64word(b, entry>>32);
	x64byte(b, 0xff); x64byte(b, 0xd0);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 24);
	x64casefield(b, 1, 0, a->typenp, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); x64consguard(b, 0x74, framed);
	x64casefield(b, 1, 0, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64word(b, 256);
	x64consguard(b, 0x76, framed);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x4c); x64byte(b, 0x24); x64byte(b, 32);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xcf);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xe6);
	entry=(unsigned long long)a->consentry;
	x64byte(b, 0x48); x64byte(b, 0xb8); x64word(b, entry); x64word(b, entry>>32);
	x64byte(b, 0xff); x64byte(b, 0xd0);
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x74); x64byte(b, 0x24); x64byte(b, 8);
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x4c); x64byte(b, 0x24); x64byte(b, 32);
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x90); x64word(b, a->listdata);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9);
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	loop=b->n;
	x64byte(b, 0x44); x64byte(b, 0x8a); x64byte(b, 0x1e);
	x64byte(b, 0x45); x64byte(b, 0x88); x64byte(b, 0x1a);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc6);
	x64byte(b, 0x49); x64byte(b, 0xff); x64byte(b, 0xc2);
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9);
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[empty]=b->n-empty-1;
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x54); x64byte(b, 0x24); x64byte(b, 24);
	x64byte(b, 0x83); x64byte(b, 0x82); x64word(b, a->typeref); x64byte(b, 1);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x90); x64word(b, a->listtype);
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 16);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x00);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc4); x64byte(b, 56);
	return 1;
}


/* Stage the old owner locally so raw source bytes are copied before destination publication. */
static int
x64rawcons(X64Buf *b, LeafLayout *a)
{
	int fallback, empty, loop;
	unsigned long long identity;

	if(!leafscalarconslayout(a, 1))return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->m); x64pcspan(b, 8, a->wordbytes); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64byte(b, 0x00); x64byte(b, 0x01); x64byte(b, 0x00); x64byte(b, 0x00); // cmp rax, 256
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc3); // mov r11, rax
	x64caseptr(b, 0, a->s); x64caseptr(b, 2, a->d);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd0); // mov rax, r10
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01); // lea rdx, [rax+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	x64readspan(b, 0, 8);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xdb); // test r11, r11
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0); // mov rax, r8
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x50); x64byte(b, 0x01); // lea rdx, [rax+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xd8); // add rax, r11
	x64moduleguard(b, 0x73, fallback);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[empty]=b->n-empty-1;
	x64casefield(b, 1, 10, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xec); x64byte(b, 0x38); // sub rsp, 56
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x0c); x64byte(b, 0x24); // mov qword ptr [rsp], rcx
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 0x08); // mov qword ptr [rsp+8], r8
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x54); x64byte(b, 0x24); x64byte(b, 0x10); // mov qword ptr [rsp+16], r10
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0x5c); x64byte(b, 0x24); x64byte(b, 0x18); // mov qword ptr [rsp+24], r11
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xdf); // mov rdi, r11
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xe6); // mov rsi, rsp
	identity=(unsigned long long)a->consentry;
	x64byte(b, 0x48); x64byte(b, 0xb8); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0xff); x64byte(b, 0xd0); // call rax
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x74); x64byte(b, 0x24); x64byte(b, 0x08); // mov rsi, qword ptr [rsp+8]
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x4c); x64byte(b, 0x24); x64byte(b, 0x18); // mov rcx, qword ptr [rsp+24]
	x64byte(b, 0x4c); x64byte(b, 0x8d); x64byte(b, 0x90); x64word(b, a->listdata); // lea r10, [rax+payload]
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	loop=b->n;
	x64byte(b, 0x44); x64byte(b, 0x8a); x64byte(b, 0x1e); // mov r11b, byte ptr [rsi]
	x64byte(b, 0x45); x64byte(b, 0x88); x64byte(b, 0x1a); // mov byte ptr [r10], r11b
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc6); // inc rsi
	x64byte(b, 0x49); x64byte(b, 0xff); x64byte(b, 0xc2); // inc r10
	x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9); // dec rcx
	x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[empty]=b->n-empty-1;
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x44); x64byte(b, 0x24); x64byte(b, 0x10); // mov r8, qword ptr [rsp+16]
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x00); // mov qword ptr [r8], rax
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc4); x64byte(b, 0x38); // add rsp, 56
	return 1;
}


/* Validate pointer retention before allocation, then publish live GC and descriptor updates. */
static int
x64pointercons(X64Buf *b, LeafLayout *a)
{
	int fallback, empty, marked;
	unsigned long long identity;

	if(!leafpointerconslayout(a))return 0;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->s); x64pcspan(b, 8, 8); x64load(b, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc0); // test rax, rax
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf8); x64byte(b, 0xff); // cmp rax, -1
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, a->heapcolor);
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0xa8); x64byte(b, 0x07); // test al, 7
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 0, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x75, fallback);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff); // cmp rcx, -1
	x64moduleguard(b, 0x75, fallback);
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[empty]=b->n-empty-1;
	identity=(unsigned long long)a->ptrtype;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64casefield(b, 1, 2, a->typeref, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x7f, fallback);
	x64byte(b, 0x81); x64byte(b, 0xf9); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0x7f); // cmp ecx, 0x7fffffff
	x64moduleguard(b, 0x75, fallback);
	x64caseptr(b, 1, a->d); x64pcspan(b, 9, 8);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xce); // mov rsi, r9
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xec); x64byte(b, 0x18); // sub rsp, 24
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0x04); x64byte(b, 0x24); // mov qword ptr [rsp], rax
	x64byte(b, 0xbf); x64byte(b, 0x08); x64byte(b, 0x00); x64byte(b, 0x00); x64byte(b, 0x00); // mov edi, 8
	identity=(unsigned long long)a->consentry;
	x64byte(b, 0x48); x64byte(b, 0xb8); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0xff); x64byte(b, 0xd0); // call rax
	x64byte(b, 0x4c); x64byte(b, 0x8b); x64byte(b, 0x1c); x64byte(b, 0x24); // mov r11, qword ptr [rsp]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xc4); x64byte(b, 0x18); // add rsp, 24
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc2); // mov r10, rax
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0xfb); x64byte(b, 0xff); // cmp r11, -1
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64byte(b, 0x49); x64byte(b, 0x83); x64byte(b, 0x83); x64word(b, -a->heapref); x64byte(b, 1); // add qword ptr [r11-ref], 1
	identity=(unsigned long long)a->mutator;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x8b); x64byte(b, 0x0a); // mov ecx, dword ptr [rdx]
	x64byte(b, 0x41); x64byte(b, 0x39); x64byte(b, 0x8b); x64word(b, -a->heapcolor); // cmp source color, ecx
	x64byte(b, 0x74); marked=b->n; x64byte(b, 0);
	x64byte(b, 0x41); x64byte(b, 0xc7); x64byte(b, 0x83); x64word(b, -a->heapcolor); x64word(b, a->markcolor);
	identity=(unsigned long long)a->nprop;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0xc7); x64byte(b, 0x02); x64byte(b, 0x01); x64byte(b, 0x00); x64byte(b, 0x00); x64byte(b, 0x00); // mov dword ptr [rdx], 1
	if(b->n>sizeof(b->bytes))return 0;
	b->bytes[marked]=b->n-marked-1;
	b->bytes[empty]=b->n-empty-1;
	identity=(unsigned long long)a->ptrtype;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x83); x64byte(b, 0x82); x64word(b, a->typeref); x64byte(b, 1); // add pointer Type.ref, 1
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0x92); x64word(b, a->listtype); // mov node.Type, rdx
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0x9a); x64word(b, a->listdata); // mov node.payload, r11
	return 1;
}


/* Reuse unique owning pointer-free arrays without retaining or allocating. */
static int
x64arraycreate(X64Buf *b, LeafLayout *a, int clear)
{
	int fallback, header, moduleend, empty, loop, arrayend;
	unsigned long long identity;

	if(!leafarraycreatelayout(a))
		return 0;
	arrayend=a->arraylen+a->wordbytes;
	if(arrayend<a->arraytype+8)arrayend=a->arraytype+8;
	if(arrayend<a->arrayroot+8)arrayend=a->arrayroot+8;
	if(arrayend<a->arraydata+8)arrayend=a->arraydata+8;
	header=a->typeref+4;
	if(header<a->typesize+4)header=a->typesize+4;
	if(header<a->typenp+4)header=a->typenp+4;
	if(header<a->typefree+8)header=a->typefree+8;
	if(header<a->typemark+8)header=a->typemark+8;
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64caseptr(b, 0, a->module);
	x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xc0); // mov rax, r8
	x64pcspan(b, 0, a->linkmodule+8);
	x64casefield(b, 0, 0, a->linkmodule, 8);
	moduleend=a->moduletypes+8;
	if(moduleend<a->modulenty+4)moduleend=a->modulenty+4;
	x64pcspan(b, 0, moduleend);
	x64caseptr(b, 0, a->m); x64pcspan(b, 8, a->wordbytes);
	x64casefield(b, 2, 0, a->modulenty, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); // test rdx, rdx
	x64moduleguard(b, 0x7f, fallback);
	x64casefield(b, 1, 0, a->moduletypes, 8);
	x64caseptr(b, 0, a->m); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc6); // mov rsi, rax
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd6); // cmp rsi, rdx
	x64moduleguard(b, 0x72, fallback);
	x64byte(b, 0x48); x64byte(b, 0x8d); x64byte(b, 0x51); x64byte(b, 0x01); // lea rdx, [rcx+1]
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xfa); x64byte(b, 0x01); // cmp rdx, 1
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x48); x64byte(b, 0xc1); x64byte(b, 0xe6); x64byte(b, 0x03); // shl rsi, 3
	x64byte(b, 0x48); x64byte(b, 0x01); x64byte(b, 0xf1); // add rcx, rsi
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc8); // mov rax, rcx
	x64readspan(b, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x8b); x64byte(b, 0x00); // mov rax, qword ptr [rax]
	x64byte(b, 0xa8); x64byte(b, 0x07); // test al, 7
	x64moduleguard(b, 0x74, fallback);
	x64pcspan(b, 0, header);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc1); // mov r9, rax
	x64casefield(b, 1, 0, a->typeref, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x7f, fallback);
	x64casefield(b, 1, 0, a->typesize, 4);
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xf9); x64byte(b, 0x00); x64byte(b, 0x01); x64byte(b, 0x00); x64byte(b, 0x00); // cmp rcx, 256
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xcb); // mov r11, rcx
	x64casefield(b, 2, 0, a->typenp, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xd2); // test rdx, rdx
	x64moduleguard(b, 0x74, fallback);
	/* Keep rejection returns nearby so guarded leaves remain within their fixed budget. */
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);
	x64casefield(b, 1, 0, a->typeref, 4);
	x64byte(b, 0x81); x64byte(b, 0xf9); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0x7f); // cmp ecx, 0x7fffffff
	x64moduleguard(b, 0x75, fallback);
	identity=(unsigned long long)a->arrayidentity;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x49); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp r9, rdx
	x64moduleguard(b, 0x75, fallback);
	x64casefield(b, 1, 2, a->typeref, 4);
	x64byte(b, 0x48); x64byte(b, 0x85); x64byte(b, 0xc9); // test rcx, rcx
	x64moduleguard(b, 0x7f, fallback);
	x64byte(b, 0x81); x64byte(b, 0xf9); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0x7f); // cmp ecx, 0x7fffffff
	x64moduleguard(b, 0x75, fallback);
	x64caseptr(b, 0, a->s); x64pcspan(b, 8, a->wordbytes); x64load(b, 0, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc6); // mov rsi, rax
	x64byte(b, 0x48); x64byte(b, 0x81); x64byte(b, 0xfe); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0xff); x64byte(b, 0x7f); // cmp rsi, 0x7fffffff
	x64moduleguard(b, 0x76, fallback);
	x64byte(b, 0x4c); x64byte(b, 0x0f); x64byte(b, 0xaf); x64byte(b, 0xde); // imul r11, rsi
	x64byte(b, 0x49); x64byte(b, 0x81); x64byte(b, 0xfb); x64byte(b, 0x00); x64byte(b, 0x01); x64byte(b, 0x00); x64byte(b, 0x00); // cmp r11, 256
	x64moduleguard(b, 0x76, fallback);
	x64caseptr(b, 2, a->d); x64pcspan(b, 10, 8);
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0xd0); // mov r8, r10
	x64casefield(b, 0, 10, 0, 8);
	x64byte(b, 0x48); x64byte(b, 0x3d); x64word(b, a->heapcolor);
	x64moduleguard(b, 0x77, fallback);
	x64byte(b, 0x49); x64byte(b, 0x89); x64byte(b, 0xc2); // mov r10, rax
	x64byte(b, 0xa8); x64byte(b, 0x07); // test al, 7
	x64moduleguard(b, 0x74, fallback);
	x64readspan(b, 0, arrayend);
	/* Keep rejection returns nearby so guarded leaves remain within their fixed budget. */
	x64byte(b, 0xeb); x64byte(b, 3);
	fallback=b->n; x64byte(b, 0x31); x64byte(b, 0xc0); x64byte(b, 0xc3);

	x64casefield(b, 1, 10, -a->heaptype, 8);
	identity=(unsigned long long)a->arrayidentity;
	x64byte(b, 0x48); x64byte(b, 0xba); x64word(b, identity); x64word(b, identity>>32);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd1); // cmp rcx, rdx
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 10, -a->heapref, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0x01); // cmp rcx, 1
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 10, a->arraylen, a->wordbytes);
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xf1); // cmp rcx, rsi
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 10, a->arraytype, 8);
	x64byte(b, 0x4c); x64byte(b, 0x39); x64byte(b, 0xc9); // cmp rcx, r9
	x64moduleguard(b, 0x74, fallback);
	x64casefield(b, 1, 10, a->arrayroot, 8);
	x64byte(b, 0x48); x64byte(b, 0x83); x64byte(b, 0xf9); x64byte(b, 0xff); // cmp rcx, -1
	x64moduleguard(b, 0x74, fallback);
	x64byte(b, 0x4d); x64byte(b, 0x85); x64byte(b, 0xdb); // test r11, r11
	x64byte(b, 0x74); empty=b->n; x64byte(b, 0);
	x64casefield(b, 0, 10, a->arraydata, 8);
	/* Owning payloads begin after the complete array header, including shifted roots. */
	x64byte(b, 0x49); x64byte(b, 0x8d); x64byte(b, 0x92); x64word(b, arrayend); // lea rdx, [r10+arrayend]
	x64byte(b, 0x48); x64byte(b, 0x39); x64byte(b, 0xd0); // cmp rax, rdx
	x64moduleguard(b, 0x73, fallback);
	x64byte(b, 0x48); x64byte(b, 0x89); x64byte(b, 0xc2); // mov rdx, rax
	x64byte(b, 0x4c); x64byte(b, 0x01); x64byte(b, 0xda); // add rdx, r11
	x64moduleguard(b, 0x73, fallback);
	if(clear){
		x64byte(b, 0x4c); x64byte(b, 0x89); x64byte(b, 0xd9); // mov rcx, r11
		loop=b->n;
		x64byte(b, 0xc6); x64byte(b, 0x44); x64byte(b, 0x08); x64byte(b, 0xff); x64byte(b, 0x00); // mov byte ptr [rax+rcx-1], 0
		x64byte(b, 0x48); x64byte(b, 0xff); x64byte(b, 0xc9); // dec rcx
		x64byte(b, 0x75); x64byte(b, loop-b->n-1);
	}
	if(b->n>sizeof(b->bytes) || b->n-empty-1>127)return 0;
	b->bytes[empty]=b->n-empty-1;
	x64byte(b, 0x4d); x64byte(b, 0x89); x64byte(b, 0x10); // mov qword ptr [r8], r10
	return 1;
}


/* Tail-enter shared C dispatch without leaving a return address in module code.
 * rdi retains REG; esi is the opcode and rdx the optional guarded fast entry.
 * The shared dispatcher returns directly to the original caller, even when the
 * C handler has freed this stub's mapping. Emission is fixed-size and atomic.
 */
static int
leafbridgeemit(u32int *out, int cap, void *executor, int op, void *fast)
{
	X64Buf b;
	unsigned long long entry, native;
	int n;

	entry = (unsigned long long)executor;
	native = (unsigned long long)fast;
	if(cap <= 0 || entry == 0 || entry == ~0ULL || native == ~0ULL ||
	   op <= INOP || op >= MAXDIS || op == IECLR)
		return 0;
	b.n = 0;
	x64byte(&b, 0xf3); x64byte(&b, 0x0f); x64byte(&b, 0x1e); x64byte(&b, 0xfa); // ENDBR64.
	x64byte(&b, 0xbe); x64word(&b, op); // mov esi, opcode.
	x64byte(&b, 0x48); x64byte(&b, 0xba); x64word(&b, native); x64word(&b, native>>32); // mov rdx, fast.
	x64byte(&b, 0x48); x64byte(&b, 0xb8); x64word(&b, entry); x64word(&b, entry>>32); // mov rax, executor.
	x64byte(&b, 0xff); x64byte(&b, 0xe0); // jmp rax: no module-owned return address.
	while(b.n%4 != 0)
		x64byte(&b, 0x90);
	n = b.n/4;
	if(n > cap || b.n > sizeof(b.bytes))
		return 0;
	return leafcodecopy(out, cap, b.bytes, n);
}


/* Emit bounded SysV AMD64 handlers, including aligned runtime calls for list allocation. */
static int
leafemit(u32int *out, int cap, LeafLayout *abi, int op)
{
	X64Buf b;
	LeafOp p;
	LeafLayout layout;
	int patch, n, empty, other, done, scale, loop, equal, fallback;
	static uchar arithmetic[] = {0x01, 0x29, 0, 0x21, 0x09, 0x31};
	static uchar inverse[] = {0x75, 0x74, 0x7d, 0x7f, 0x7e, 0x7c};
	static uchar realinverse[] = {0x75, 0x74, 0x73, 0x77, 0x76, 0x72};

	if(cap <= 0 || !leaflayoutcopy(&layout, abi))
		return 0;
	abi = &layout;
	if((abi->wordbytes != 4 && abi->wordbytes != 8) ||
	   abi->pc < 0 || abi->s < 0 || abi->d < 0 || abi->m < 0 ||
	   !leafop(&p, op, abi->wordbytes)) return 0;
	b.n = 0;
	/* ENDBR64 also permits entry when the host enables indirect-branch tracking. */
	x64byte(&b, 0xf3); x64byte(&b, 0x0f); x64byte(&b, 0x1e); x64byte(&b, 0xfa);
	if(p.kind == LeafTransfer){
		if(!x64transfer(&b, abi, p.alu)) return 0;
	}else if(p.kind == LeafArraySlice){
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
	}else if(p.kind == LeafMappedCons){
		if(!x64mappedcons(&b, abi)) return 0;
	}else if(p.kind == LeafRawCons){
		if(!x64rawcons(&b, abi)) return 0;
	}else if(p.kind == LeafPointerCons){
		if(!x64pointercons(&b, abi)) return 0;
	}else if(p.kind == LeafChannel){
		if(!x64channel(&b, abi, p.alu)) return 0;
	}else if(p.kind == LeafScalarCons){
		if(!x64scalarcons(&b, abi, &p)) return 0;
	}else if(p.kind == LeafArrayCreate){
		if(!x64arraycreate(&b, abi, p.alu)) return 0;
	}else if(p.kind == LeafRecordObject){
		if(!x64recordobject(&b, abi, p.alu)) return 0;
	}else if(p.kind == LeafStringBytes){
		if(!x64stringbytes(&b, abi)) return 0;
	}else if(p.kind == LeafByteString){
		if(!x64bytestring(&b, abi)) return 0;
	}else if(p.kind == LeafOwner){
		if(!x64owner(&b, abi, &p)) return 0;
	}else if(p.kind == LeafCaseJump){
		if(!x64case(&b, abi, &p)) return 0;
	}else if(p.kind == LeafStringBranch){
		/* Compare captured characters without touching a false branch target. */
		if(abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 ||
		   abi->stringmax < 0 || abi->stringmax > 2047 || abi->stringmax%4 ||
		   abi->stringlen == abi->stringmax || abi->stringdata < abi->stringlen+4 ||
		   abi->stringdata < abi->stringmax+4 || abi->stringdata > 2047) return 0;
		x64byte(&b, 0xeb); x64byte(&b, 3);
		fallback=b.n; x64byte(&b, 0x31); x64byte(&b, 0xc0); x64byte(&b, 0xc3);
		/* Validate both owner slots before either String can be inspected. */
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
		x64ptr(&b, 0, abi->m); x64pcspan(&b, 8, 8);
		/* The bounded String layout makes all data sums safe below this threshold. */
		x64byte(&b, 0x49); x64byte(&b, 0xbb);
		x64word(&b, (unsigned int)(0ULL-LeafStringWindow));
		x64word(&b, (unsigned int)((0ULL-LeafStringWindow)>>32));
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64ptr(&b, 1, abi->m); x64load(&b, 1, 8);
		/* C detects either nil owner before inspecting either String header. */
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64moduleguard(&b, 0x75, fallback);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64moduleguard(&b, 0x75, fallback);
		x64stringheader(&b, 0, 2, 0, abi->stringlen, abi->stringmax, abi->stringdata, fallback);
		x64stringheader(&b, 1, 6, 1, abi->stringlen, abi->stringmax, abi->stringdata, fallback);
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
		/* The target slot is relevant only after the predicate is true. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, 8);
		x64byte(&b, 0x49); x64byte(&b, 0x8b); x64byte(&b, 0x02); // Capture [r10] in rax; rdx is validation scratch.
		x64pcspan(&b, 0, 1);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x87); x64word(&b, abi->pc);
		b.bytes[patch]=b.n-patch-1;
	}else if(p.kind == LeafTypeCheck){
		/* Capture both owners before inspecting their immutable heap types. */
		if(abi->heaptype <= 0 || abi->heaptype > 255 || abi->heaptype%8) return 0;
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, 8);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64ptr(&b, 1, abi->d); x64load(&b, 1, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
		x64byte(&b, 0x74); done=b.n; x64byte(&b, 0);
        /* An owner at the field distance would read its Type at address zero. */
        x64byte(&b, 0x48); x64byte(&b, 0x3d); x64word(&b, abi->heaptype);
        x64guard(&b, 0x77); /* unsigned owner > backward Type distance */
        x64byte(&b, 0x48); x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, abi->heaptype);
        x64guard(&b, 0x77);
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
		x64pcspan(&b, 0, abi->linkmodule+8);
		if(p.kind == LeafTableJump){
			if(abi->linkcompiled < 0 || abi->linkcompiled > 2047 || abi->linkcompiled%4) return 0;
			x64pcspan(&b, 0, abi->linkcompiled+4);
			x64field(&b, 2, 0, abi->linkcompiled, 4);
			x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x74);
		}
		x64field(&b, 0, 0, abi->linkmodule, 8);
		x64pcspan(&b, 0,
		abi->moduleprog+8 > abi->modulenprog+4 ? abi->moduleprog+8 : abi->modulenprog+4);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		x64field(&b, 1, 0, abi->modulenprog, 4);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, abi->wordbytes); x64load(&b, 0, abi->wordbytes);
		if(p.kind == LeafTableJump){
			/* The compiler's table count and destinations are signed 32-bit Dis words. */
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x79);
			x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, IBY2WD);
			x64byte(&b, 0x49); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, IBY2WD); x64guard(&b, 0x73);
			x64byte(&b, 0x49); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
			x64field(&b, 2, 10, -IBY2WD, 4);
			x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x7f);
			x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xd0); x64guard(&b, 0x72);
			x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe0); x64byte(&b, IBY2WD == 8 ? 3 : 2);
			x64byte(&b, 0x4c); x64byte(&b, 0x01); x64byte(&b, 0xd0); x64guard(&b, 0x73);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, -IBY2WD); x64guard(&b, 0x76);
			x64byte(&b, 0x48); x64byte(&b, 0x63); x64byte(&b, 0x00);
		}
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
		x64byte(&b, 0x48); x64byte(&b, 0x69); x64byte(&b, 0xc0); x64word(&b, abi->instbytes);
		x64field(&b, 2, 11, abi->moduleprog, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xd0); x64guard(&b, 0x73);
		x64pcspan(&b, 0, abi->instbytes);
		if(p.kind == LeafTableJump){
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0x87); x64word(&b, abi->pc);
		}else{
			x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, 8); x64store(&b, 0, 8);
		}
	}else if(p.kind == LeafFixedCoefficient){
		/* Capture frame parameters and both operands before any aliased result write. */
		if(abi->fp < 0 || abi->fp > 2047 || abi->fp%8 || abi->fixedtmp < 0 || abi->fixedtmp > 2047 ||
		   abi->fixedcoef < 0 || abi->fixedcoef > 2047) return 0;
		/* All decoded operands must fit before frame or scalar metadata is read. */
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, abi->wordbytes);
		x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, abi->wordbytes);
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, abi->wordbytes);
		/* One checked prefix covers every used frame field without pointer carry. */
		x64ptr(&b, 1, abi->fp);
		x64pcspan(&b, 9, (p.alu == 2 || abi->fixedcoef > abi->fixedtmp ? abi->fixedcoef : abi->fixedtmp)+abi->wordbytes);
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
		int ordinary;

		/* Validate bytecode spans while preserving opaque compiled target tokens. */
		if(abi->instbytes <= 0 || abi->instbytes > 2047 ||
		   abi->module < 0 || abi->module > 2047 || abi->module%8 ||
		   abi->linkcompiled < 0 || abi->linkcompiled > 2047 || abi->linkcompiled%4 ||
		   abi->fp < 0 || abi->fp > 2047 || abi->fp%8 ||
		   abi->framelr < 0 || abi->framelr > 2047 || abi->framelr%8 ||
		   abi->framefp < 0 || abi->framefp > 2047 || abi->framefp%8 ||
		   abi->framelr == abi->framefp) return 0;
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64pcspan(&b, 0, (abi->framelr > abi->framefp ? abi->framelr : abi->framefp)+8);
		x64ptr(&b, 1, abi->d); x64pcspan(&b, 9, 8); x64load(&b, 1, 8);
		x64pcspan(&b, 1, 1);
		x64byte(&b, 0x48); x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, -abi->instbytes-1);
		x64byte(&b, 0x76); ordinary=b.n; x64byte(&b, 0);
		x64ptr(&b, 0, abi->module); x64pcspan(&b, 8, abi->linkcompiled+4);
		x64field(&b, 2, 8, abi->linkcompiled, 4);
		x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
		b.bytes[ordinary]=b.n-ordinary-1;
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
		/* Bound all decoded slots before reading owners, indices or descriptors. */
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, abi->wordbytes);
		x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, abi->wordbytes);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		/* A negative index must reject before any array header is inspected. */
		x64ptr(&b, 0, abi->d); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x79);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc6); // Preserve the signed index in rsi.
		x64pcspan(&b, 11, abi->arraylen+abi->wordbytes);
		x64field(&b, 1, 11, abi->arraylen, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xf0); // Restore the captured index.
		x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
		x64pcspan(&b, 11, abi->arraytype+8);
		x64field(&b, 2, 11, abi->arraytype, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 0xff); x64guard(&b, 0x75);
		/* Move the descriptor out of rdx, which the span guard uses as scratch. */
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xd1);
		x64pcspan(&b, 1, abi->typesize+4);
		x64field(&b, 1, 1, abi->typesize, 4);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
		x64pcspan(&b, 11, abi->arraydata+8);
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
		x64pcspan(&b, 9, abi->fixedtmp+abi->wordbytes);
		x64byte(&b, 0x49); x64byte(&b, 0x81); x64byte(&b, 0xc1); x64word(&b, abi->fixedtmp);
		x64load(&b, 1, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x8d); x64byte(&b, 0x51); x64byte(&b, 63);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 127); x64guard(&b, 0x72);
		if(!p.alu){
			/* Multiplication bounds both inputs and the result before either load. */
			x64ptr(&b, 0, abi->m); x64pcspan(&b, 8, abi->wordbytes);
			x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, abi->wordbytes);
		}
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, abi->wordbytes);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc2);
		if(p.alu){
			/* Reject zero before loading the dividend; -1 uses modular native negation. */
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xd2); x64guard(&b, 0x75);
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xd3);
		}
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xd3); // Preserve the multiplicand/divisor in r11.
		x64ptr(&b, 0, abi->m); x64pcspan(&b, 8, abi->wordbytes);
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, abi->wordbytes);
		x64ptr(&b, 0, abi->m); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x4c); x64byte(&b, 0x89); x64byte(&b, 0xda); // Restore rdx after span scratch.
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
		/* Preflight the complete result and source before loads or FP effects. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, p.bytes);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, p.input);
		/* Native conversion preserves the host C rounding and sticky FP flags. */
		x64ptr(&b, 0, abi->s);
		x64byte(&b, p.input == 4 ? 0xf3 : 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x10); x64byte(&b, 0x00);
		x64byte(&b, p.input == 4 ? 0xf3 : 0xf2); x64byte(&b, 0x0f); x64byte(&b, 0x5a); x64byte(&b, 0xc0);
		x64ptr(&b, 2, abi->d);
		x64byte(&b, p.bytes == 4 ? 0xf3 : 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x11); x64byte(&b, 0x02);
	}else if(p.kind == LeafIntegerReal){
		/* Preflight the complete result and source before loads or FP effects. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, p.bytes);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, p.input);
		/* Convert the complete signed input, including rounded magnitudes above 2^53. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, p.input);
		x64byte(&b, 0xf2); x64byte(&b, 0x48); x64byte(&b, 0x0f); x64byte(&b, 0x2a); x64byte(&b, 0xc0);
		x64ptr(&b, 2, abi->d);
		x64byte(&b, 0xf2); x64byte(&b, 0x41); x64byte(&b, 0x0f); x64byte(&b, 0x11); x64byte(&b, 0x02);
	}else if(p.kind == LeafFixedShift){
		/* Translating the signed interval avoids overflowing when negating minimum. */
		x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, abi->wordbytes);
		x64ptr(&b, 1, abi->m); x64load(&b, 1, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x8d); x64byte(&b, 0x51); x64byte(&b, 63); /* lea rdx,[rcx+63] */
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xfa); x64byte(&b, 127);
		x64guard(&b, 0x72);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, abi->wordbytes);
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, abi->wordbytes);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); /* test rcx,rcx */
		x64byte(&b, 0x78); x64byte(&b, 5);
		x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xe0);
		x64byte(&b, 0xeb); x64byte(&b, 6);
		x64byte(&b, 0x48); x64byte(&b, 0xf7); x64byte(&b, 0xd9); /* neg rcx */
		x64byte(&b, 0x48); x64byte(&b, 0xd3); x64byte(&b, 0xf8); /* sar rax,cl */
		x64ptr(&b, 2, abi->d); x64store(&b, 0, abi->wordbytes);
	}else if(p.kind == LeafPower){
		/* Check both source spans before the exponent or base is read. */
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, abi->wordbytes);
		x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, p.bytes);
		/* Negative exponents need only zero/one tests and parity, never negation. */
		x64ptr(&b, 0, abi->s); x64load(&b, 0, abi->wordbytes);
		x64ptr(&b, 1, abi->m); x64load(&b, 1, p.bytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
		x64byte(&b, 0x79); scale=b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x75);
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, p.bytes);
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
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, p.bytes);
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
		if(!x64stringindex(&b, abi)) return 0;
	}else if(p.kind == LeafListLength){
		/* Bounded Brent scans decline on cycles without publishing a partial length. */
		if(abi->listtail < 0 || abi->listtail > 32760 || abi->listtail%8) return 0;
		/* Validate decoded result/source slots before any list access. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, abi->wordbytes);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3); // r11 is the initial checkpoint.
		x64byte(&b, 0x31); x64byte(&b, 0xc9); /* count = 0 */
		loop = b.n;
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
		x64byte(&b, 0x74); done = b.n; x64byte(&b, 0);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x81); x64byte(&b, 0xf9); x64word(&b, LeafListLimit); x64guard(&b, 0x72);
		x64readspan(&b, abi->listtail, 8);
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
		/* Validate decoded result/source slots before any list access. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, p.bytes);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
		x64readspan(&b, abi->listdata, p.bytes);
		x64field(&b, 0, 0, abi->listdata, p.bytes);
		x64ptr(&b, 2, abi->d); x64store(&b, 0, p.bytes);
	}else if(p.kind == LeafStringLength){
		if(abi->stringlen < 0) return 0;
		/* Reject an invalid result slot before loading the String pointer. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, abi->wordbytes);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
		x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
		x64byte(&b, 0x74); empty = b.n; x64byte(&b, 0);
		x64readspan(&b, abi->stringlen, 4);
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
			x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
			x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x75);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff); x64guard(&b, 0x75);
			x64byte(&b, 0x49); x64byte(&b, 0x89); x64byte(&b, 0xc3);
		}
		x64ptr(&b, 0, abi->m); x64pcspan(&b, 8, abi->wordbytes);
		/* Capture the signed length before copying can overwrite its operand. */
		x64ptr(&b, 0, abi->m); x64load(&b, 0, abi->wordbytes);
		x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x79);
		x64byte(&b, 0x0f); x64byte(&b, 0x84); empty = b.n; x64word(&b, 0);
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
		if(!x64copykernel(&b)) return 0;
		if(!x64casepatch(&b, empty, b.n)) return 0;
	}else if(p.kind == LeafLength || p.kind == LeafIndex){
		/* Both field extents and their negative imm32 limits must remain representable. */
		if(abi->arraylen < 0 || abi->arraylen > 0x7ffffffe - abi->wordbytes ||
		   abi->arraydata < 0 || abi->arraydata > 0x7ffffffe - 8) return 0;
		/* Only length queries ignore m; address commands return through m. */
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, abi->wordbytes);
		if(p.kind == LeafIndex){
			x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, abi->wordbytes);
		}
		x64ptr(&b, 0, abi->s); x64load(&b, 0, 8);
		if(p.kind == LeafLength){
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0);
			x64byte(&b, 0x74); empty = b.n; x64byte(&b, 0);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf8); x64byte(&b, 0xff);
			x64byte(&b, 0x74); other = b.n; x64byte(&b, 0);
			x64pcspan(&b, 0, abi->arraylen+abi->wordbytes);
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
			/* A negative index must reject before any array header is inspected. */
			x64ptr(&b, 0, abi->d); x64load(&b, 0, abi->wordbytes);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc0); x64guard(&b, 0x79);
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xc6); // Preserve the signed index in rsi.
			x64pcspan(&b, 11, abi->arraylen+abi->wordbytes);
			x64field(&b, 1, 11, abi->arraylen, abi->wordbytes);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x79);
			x64byte(&b, 0x48); x64byte(&b, 0x89); x64byte(&b, 0xf0); // Restore the captured index.
			x64byte(&b, 0x48); x64byte(&b, 0x39); x64byte(&b, 0xc8); x64guard(&b, 0x72);
			scale = p.bytes == 1 ? 0 : p.bytes == 4 ? 2 : 3;
			x64byte(&b, 0x48); x64byte(&b, 0x3d); x64word(&b, 0x7fffffff>>scale); x64guard(&b, 0x76);
			x64pcspan(&b, 11, abi->arraydata+8);
			x64field(&b, 1, 11, abi->arraydata, 8);
			x64byte(&b, 0x48); x64byte(&b, 0x85); x64byte(&b, 0xc9); x64guard(&b, 0x75);
			x64byte(&b, 0x48); x64byte(&b, 0x83); x64byte(&b, 0xf9); x64byte(&b, 0xff); x64guard(&b, 0x75);
			if(scale){ x64byte(&b, 0x48); x64byte(&b, 0xc1); x64byte(&b, 0xe0); x64byte(&b, scale); }
			x64byte(&b, 0x48); x64byte(&b, 0x01); x64byte(&b, 0xc8); x64guard(&b, 0x73);
			x64ptr(&b, 2, abi->m); x64store(&b, 0, abi->wordbytes);
		}
	}else if(p.kind == LeafRealPower){
		/* Preflight complete slots before loads, conversions or FP effects. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, 8);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, abi->wordbytes);
		x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, 8);
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
		/* Validate the scaled and ordinary variants before parsing raw FP bits. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, p.bytes);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, 8);
		if(p.alu){
			x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, 8);
		}
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
		/* Preflight complete slots before loads, conversions or FP effects. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, 8);
		x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, p.alu == 4 ? abi->wordbytes : 8);
		x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, 8);
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
		/* Preflight the complete result and source before loads or FP effects. */
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, p.bytes);
		if(p.kind != LeafAddress){
			x64ptr(&b, 0, abi->s); x64pcspan(&b, 8, p.input);
		}
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
		x64pcspan(&b, 8, p.kind == LeafShift ? abi->wordbytes : p.bytes);
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
		/* Preserve the count/divisor in rax; bounds checks precede the value read. */
		x64ptr(&b, 1, abi->m); x64pcspan(&b, 9, p.bytes);
		x64ptr(&b, 2, abi->d); x64pcspan(&b, 10, p.bytes);
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
        x64ptr(&b, 0, abi->s);
        x64pcspan(&b, 8, 8);
        x64ptr(&b, 1, abi->m);
        x64pcspan(&b, 9, 8);
        x64load(&b, 0, 8);
        x64realguard(&b, 0);
        x64load(&b, 1, 8);
        x64realguard(&b, 1);
        x64byte(&b, 0x66);
        x64byte(&b, 0x48);
        x64byte(&b, 0x0f);
        x64byte(&b, 0x6e);
        x64byte(&b, 0xc0);
        x64byte(&b, 0x66);
        x64byte(&b, 0x48);
        x64byte(&b, 0x0f);
        x64byte(&b, 0x6e);
        x64byte(&b, 0xc9);
        x64byte(&b, 0x66);
        x64byte(&b, 0x0f);
        x64byte(&b, 0x2e);
        x64byte(&b, 0xc1);
        x64byte(&b, realinverse[p.condition]);
        patch=b.n;
        x64byte(&b, 0);
        x64ptr(&b, 2, abi->d);
        x64pcspan(&b, 10, 8);
        x64byte(&b, 0x49);
        x64byte(&b, 0x8b);
        x64byte(&b, 0x02);
        x64pcspan(&b, 0, 1);
        x64byte(&b, 0x48);
        x64byte(&b, 0x89);
        x64byte(&b, 0x87);
        x64word(&b, abi->pc);
        b.bytes[patch]=b.n-patch-1;
    }else if(p.kind == LeafAlu || p.kind == LeafBranch){
        if(p.kind == LeafAlu){
            /* No operand read may precede complete source/result span validation. */
            x64ptr(&b, 2, abi->d);
        x64pcspan(&b, 10, p.bytes);
            x64ptr(&b, 0, abi->s);
        x64pcspan(&b, 8, p.bytes);
            x64ptr(&b, 1, abi->m);
        x64pcspan(&b, 9, p.bytes);
        }
        if(p.kind == LeafBranch){
            x64ptr(&b, 0, abi->s);
            x64pcspan(&b, 8, p.bytes);
            x64ptr(&b, 1, abi->m);
            x64pcspan(&b, 9, p.bytes);
        }
        x64ptr(&b, 0, abi->s);
        x64ptr(&b, 1, abi->m);
        x64load(&b, 0, p.bytes);
        x64load(&b, 1, p.bytes);
        if(p.bytes == 8){
            x64byte(&b, 0x48);
        }
        if(p.kind == LeafAlu){
            if(p.alu == 2){ x64byte(&b, 0x0f);
        x64byte(&b, 0xaf);
        x64byte(&b, 0xc8);
        }
            else { x64byte(&b, arithmetic[p.alu]);
        x64byte(&b, 0xc1);
        }
            x64ptr(&b, 2, abi->d);
        x64store(&b, 1, p.bytes);
        }else{
            x64byte(&b, 0x39);
        x64byte(&b, 0xc8);
            x64byte(&b, inverse[p.condition]);
        patch = b.n;
        x64byte(&b, 0);
            x64ptr(&b, 2, abi->d);
            x64pcspan(&b, 10, 8);
            x64byte(&b, 0x49);
        x64byte(&b, 0x8b);
        x64byte(&b, 0x02);
            x64pcspan(&b, 0, 1);
            x64byte(&b, 0x48);
        x64byte(&b, 0x89);
        x64byte(&b, 0x87);
        x64word(&b, abi->pc);
            b.bytes[patch] = b.n-patch-1;
        }
    }else if(p.kind == LeafJump){
        x64ptr(&b, 2, abi->d);
        x64pcspan(&b, 10, 8);
        x64byte(&b, 0x49);
        x64byte(&b, 0x8b);
        x64byte(&b, 0x02);
        x64pcspan(&b, 0, 1);
        x64byte(&b, 0x48);
        x64byte(&b, 0x89);
        x64byte(&b, 0x87);
        x64word(&b, abi->pc);
    }else if(p.kind != LeafNop)
		return 0; /* A new classifier kind must never silently become a native no-op. */
	x64byte(&b, 0xb8); x64word(&b, 1); x64byte(&b, 0xc3);
	while(b.n%4 != 0) x64byte(&b, 0x90);
	n = b.n/4;
	if(b.n > sizeof(b.bytes) || n > cap) return 0;
	return leafcodecopy(out, cap, b.bytes, n);
}
