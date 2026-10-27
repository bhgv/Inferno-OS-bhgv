#include "leaf64-ops.h"

enum {
	LeafMax = 128, // Four-byte code units reserved for guarded native leaves.
};

typedef struct A64Buf A64Buf;

struct A64Buf {
	u32int code[LeafMax]; // Private instruction staging, published only after sizing succeeds.
	int n; // Requested native instructions before bounded publication.
};


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

/* Branch over a side-effect-free fallback return when a guard succeeds. */
static void
a64guard(A64Buf *b, u32int condition)
{
	a64put(b, condition);
	a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
}


/* Validate a bounded field span in x12; x14 is scratch and the owner is preserved. */
static void
a64readspan(A64Buf *b, int offset, int bytes)
{
	a64put(b, 0xd280000eU | (u32int)(offset+bytes)<<5); // mov x14, extent.
	a64put(b, 0xab0e019f); // cmn x12, x14 detects unsigned address carry.
	a64guard(b, 0x54000063); // Continue when the sum does not wrap.
}

/* Fetch a scalar object field after validating its owner pointer. */
static void
a64field(A64Buf *b, int result, int owner, int offset, int bytes)
{
	a64put(b, (bytes == 8 ? 0xf9400000U : bytes == 1 ? 0x39400000U : 0xb9400000U) |
		(u32int)(offset/bytes)<<10 | owner<<5 | result);
}

/* Sign-extend WORD metadata before testing lengths in the native address domain. */
static void
a64wordfield(A64Buf *b, int result, int owner, int offset, int wordbytes)
{
	if(wordbytes == 4)
		a64put(b, 0xb9800000U | (u32int)(offset/4)<<10 | owner<<5 | result);
	else
		a64field(b, result, owner, offset, 8);
}

/* Emit a short conditional transfer to the shared side-effect-free fallback. */
static void
a64defer(A64Buf *b, u32int condition, int fallback)
{
	a64put(b, condition | ((fallback-b->n)&0x7ffff)<<5);
}

/* Share a distant rejection return for PC-table checks without growing leaf code. */
static void
a64pcguard(A64Buf *b, u32int success, int fallback)
{
	if(fallback < 0){
		a64guard(b, success);
		return;
	}
	/* PC guards use B.cond or CBZ/CBNZ, each with a signed 19-bit displacement. */
	success &= ~(0x7ffffU<<5);
	if((success&0xff000010U) == 0x54000000U)
		success ^= 1;
	else
		success ^= 0x01000000U;
	a64defer(b, success, fallback);
}

/* Stage WORD-width VN corrections before multiplication can change operand signs. */
static void
a64fixedround(A64Buf *b, int alu, int wordbytes)
{
	int skip[3], count, sign, i;

	count=0;
	a64put(b, 0xd2800005); /* x5=correction */
	skip[count++]=b->n; a64put(b, 0x36080001); /* tbz flags,#1 */
	skip[count++]=b->n; a64put(b, 0xb400000c);
	if(alu == 0){skip[count++]=b->n; a64put(b, 0xb400000e);}
	if(alu == 1){
		a64put(b, 0xca0f0185); a64put(b, 0x937ffca5); a64put(b, 0xb24000a5);
	}else{
		a64put(b, wordbytes == 4 ? 0x510005e5 : 0xd10005e5);
		sign=b->n; a64put(b, 0xb6f80002);
		a64put(b, wordbytes == 4 ? 0x4b0503e5 : 0xcb0503e5);
		b->code[sign] |= (b->n-sign)<<5;
		if(wordbytes == 4)a64put(b, 0x93407ca5);
		a64put(b, 0xd2410042); /* reverse WN sign selection when VN is set */
	}
	for(i=0;i<count;i++)b->code[skip[i]] |= (b->n-skip[i])<<5;
}


/* Resolve non-null owners with x4 as the address window and x5 as the Rune stride.
 * H has zero length, so its unspecified stride is never used for a payload read. */
static void
a64stringheader(A64Buf *b, int owner, int length, int stride, int lenoff, int maxoff, int dataoff, int fallback)
{
	int empty, done;

	a64put(b, 0xb100041fU | owner<<5);
	empty=b->n; a64put(b, 0x54000000);
	/* x4 is the conservative payload window; carry rejects its top-address range. */
	a64put(b, 0xab04001fU | owner<<5);
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xb9800000U | (u32int)(lenoff/4)<<10 | owner<<5 | length);
	a64put(b, 0xf100001fU | length<<5);
	a64put(b, 0xda80a400U | length<<16 | length<<5 | length); // CNEG length,length,lt.
	a64put(b, 0x9a9fb4a0U | stride); // CSINC stride,x5,xzr,lt selects four or one.
	/* Compare full signed values so INT_MIN cannot masquerade as a valid length. */
	a64wordfield(b, 9, owner, maxoff, 4);
	a64put(b, 0xeb09001fU | length<<5);
	a64defer(b, 0x5400000c, fallback);
	a64put(b, 0x91000000U | (u32int)dataoff<<10 | owner<<5 | owner);
	done=b->n; a64put(b, 0x14000000);
	b->code[empty] |= (b->n-empty)<<5;
	a64put(b, 0x52800000U | length);
	b->code[done] |= b->n-done;
}


/* Guard raw x12 source/x13 scale and leave the shifted product magnitude in x13. */
static void
a64realproduct(A64Buf *b)
{
	int unit;

	a64put(b, 0xd3400000U | 52<<16 | 51<<10 | 13<<5 | 15); /* lsl x15,x13,#12 */
	a64guard(b, 0xb400006f);
	a64put(b, 0xd3400000U | 52<<16 | 62<<10 | 13<<5 | 13);
	a64put(b, 0xd1000000U | 1023<<10 | 13<<5 | 13);
	/* Biased scale exponents 1..2046 are precisely the finite normal powers. */
	a64put(b, 0x91000000U | 1022<<10 | 13<<5 | 15);
	a64put(b, 0xf100001fU | 2045<<10 | 15<<5); a64guard(b, 0x54000069);
	unit=b->n; a64put(b, 0xb400000d);
	/* Check both exponents independently, excluding NaNs and underflow/overflow. */
	a64put(b, 0xd3400000U | 52<<16 | 62<<10 | 12<<5 | 15);
	a64put(b, 0xd10005e2); a64put(b, 0xf1000000U | 2045<<10 | 2<<5 | 31); a64guard(b, 0x54000069);
	a64put(b, 0x8b0d01ef);
	a64put(b, 0xd10005e2); a64put(b, 0xf1000000U | 2045<<10 | 2<<5 | 31); a64guard(b, 0x54000069);
	b->code[unit] |= (b->n-unit)<<5;
	a64put(b, 0xd3400000U | 12<<16 | 11<<10 | 13<<5 | 13); /* lsl x13,x13,#52 */
	a64put(b, 0x8b0d018d); a64put(b, 0xd37ff9ad);
}

/* Reject NaNs before FP execution while allowing host-controlled subnormal inputs. */
static void
a64realguard(A64Buf *b, int reg)
{
	int ordinary;

	a64put(b, 0xd3400000U | 63<<16 | 62<<10 | reg<<5 | 14);
	a64put(b, 0xd3400000U | 53<<16 | 63<<10 | 14<<5 | 14);
	a64put(b, 0xf100001fU | 2047<<10 | 14<<5);
	ordinary=b->n; a64put(b, 0x54000001);
	a64put(b, 0xd3400000U | 52<<16 | 51<<10 | reg<<5 | 14);
	a64guard(b, 0xb400006e);
	b->code[ordinary] |= (b->n-ordinary)<<5;
}


/* Validate PC-table owners before loads; x4 is scratch and other owners stay live. */
static void
a64pcspan(A64Buf *b, int owner, int extent, int fallback)
{
	a64put(b, 0xd1000004U | 1<<10 | owner<<5); // sub x4,owner,#1.
	a64put(b, 0xb100009fU | (u32int)(extent+1)<<10); // cmn x4,#extent+1.
	a64pcguard(b, 0x54000063, fallback); // Continue only without unsigned carry.
}


/* Require an empty String in x9 without reading characters; H skips the header. */
static void
a64caseempty(A64Buf *b, LeafLayout *abi, int fallback)
{
	int empty;

	a64put(b, 0xb100053f); empty=b->n; a64put(b, 0x54000000);
	a64pcspan(b, 9, abi->stringlen+4 > abi->stringmax+4 ? abi->stringlen+4 : abi->stringmax+4, fallback);
	a64wordfield(b, 4, 9, abi->stringlen, 4);
	a64pcguard(b, 0xb4000064, fallback);
	a64wordfield(b, 5, 9, abi->stringmax, 4);
	a64put(b, 0xf10000bf); a64pcguard(b, 0x5400006a, fallback);
	if(empty < LeafMax)
		b->code[empty] |= (b->n-empty)<<5;
}


/* Search numeric tables and lower-label string identities through one checked rejection return. */
static int
a64case(A64Buf *b, LeafLayout *abi, LeafOp *op)
{
	int fallback, header, stride, loop, empty, left, right, found, done, identity, positive, same, matched;

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
	stride=op->alu ? 2*IBY2LG+2*IBY2WD : 3*IBY2WD;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 12, abi->module);
	a64pcspan(b, 12,
		abi->linkmodule+8 > abi->linkcompiled+4 ? abi->linkmodule+8 : abi->linkcompiled+4, fallback);
	a64field(b, 4, 12, abi->linkcompiled, 4); a64pcguard(b, 0x34000064, fallback);
	a64field(b, 12, 12, abi->linkmodule, 8);
	a64pcspan(b, 12,
		abi->moduleprog+8 > abi->modulenprog+4 ? abi->moduleprog+8 : abi->modulenprog+4, fallback);
	a64ptr(b, 9, abi->s);
	a64pcspan(b, 9, op->bytes, fallback);
	if(op->bytes == 4)a64put(b, 0xb980012e); /* ldrsw x14,[x9] */
	else a64load(b, 14, 9, 8);
	a64ptr(b, 10, abi->d); a64pcspan(b, 10, op->condition ? IBY2WD : 4, fallback);
	a64put(b, op->condition && IBY2WD == 8 ? 0xf9400141 : 0xb9800141);
	a64put(b, 0xf100003fU | (4095)<<10); a64pcguard(b, 0x54000069, fallback);
	/* Reject incompatible nonempty cells before reading rows or the default. */
	if(op->condition && IBY2WD < 8)
		a64pcguard(b, 0xb4000061, fallback); /* cbz x1,continue */
	a64put(b, 0xd2800009U | stride<<5);
	a64put(b, 0x9b097c24); a64put(b, 0x91000084U | header<<10);
	a64put(b, 0xab040145); a64pcguard(b, 0x54000063, fallback);
	a64put(b, 0xb10000bfU | (op->condition ? IBY2WD-1 : 3)<<10); a64pcguard(b, 0x54000063, fallback);
	a64put(b, op->condition && IBY2WD == 8 ? 0xf94000ad : 0xb98000ad); a64put(b, 0x9100014fU | header<<10);
	loop=b->n; empty=b->n; a64put(b, 0xb4000001);
	if(op->condition){
		if(IBY2WD < 8){
			a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
		}else{
			/* Equal owners still validate the same header constraints as stringcmp. */
			/* Probe the same midpoint row as C without comparing character data. */
			a64put(b, 0xd341fc24); // lsr x4,x1,#1.
			a64put(b, 0xd2800009U | stride<<5); // mov x9,#stride.
			a64put(b, 0x9b093c8f); // madd x15,x4,x9,x15.
			a64put(b, 0xf94001e9); // ldr x9,[x15] captures the lower label.
			a64put(b, 0xeb0901df); same=b->n; a64put(b, 0x54000000);
			a64caseempty(b, abi, fallback);
			a64put(b, 0xaa0e03e9); // mov x9,x14 captures the source owner.
			a64caseempty(b, abi, fallback);
			matched=b->n; a64put(b, 0x14000000);
			if(same >= LeafMax)return 0;
			b->code[same] |= (b->n-same)<<5;
			a64put(b, 0xb100053f); identity=b->n; a64put(b, 0x54000000);
			a64pcspan(b, 9, abi->stringlen+4 > abi->stringmax+4 ? abi->stringlen+4 : abi->stringmax+4, fallback);
			a64wordfield(b, 4, 9, abi->stringlen, 4);
			a64put(b, 0xf100009f); positive=b->n; a64put(b, 0x5400000a);
			a64put(b, 0xcb0403e4); // neg x4,x4, in the native width, including INT_MIN.
			b->code[positive] |= (b->n-positive)<<5;
			a64wordfield(b, 5, 9, abi->stringmax, 4);
			a64put(b, 0xeb05009f); a64pcguard(b, 0x5400006d, fallback);
			b->code[identity] |= (b->n-identity)<<5;
			if(matched >= LeafMax)return 0;
			b->code[matched] |= b->n-matched;
			a64field(b, 13, 15, 2*IBY2WD, 8);
		}
	}else{
		a64put(b, 0xd341fc22); a64put(b, 0x9b093c43);
		a64put(b, op->alu ? 0xf9400064 : 0xb9800064);
		a64put(b, 0xeb0401df); left=b->n; a64put(b, 0x5400000b);
		a64put(b, op->alu ? 0xf9400464 : 0xb9800064U | (IBY2WD/4)<<10);
		a64put(b, 0xeb0401df); right=b->n; a64put(b, 0x5400000a);
		a64put(b, op->alu ? 0xb980106d : 0xb980006dU | (2*IBY2WD/4)<<10);
		found=b->n; a64put(b, 0x14000000);
		b->code[left] |= (b->n-left)<<5;
		a64put(b, 0xd341fc21); a64put(b, 0x14000000U | ((loop-b->n)&0x3ffffff));
		b->code[right] |= (b->n-right)<<5;
		a64put(b, 0xcb020021); a64put(b, 0xd1000421);
		a64put(b, 0x9100006fU | stride<<10); a64put(b, 0x14000000U | ((loop-b->n)&0x3ffffff));
	}
	done=b->n; b->code[empty] |= (done-empty)<<5; if(!op->condition)b->code[found] |= done-found;
	a64field(b, 4, 12, abi->modulenprog, 4);
	/* Reject a negative 32-bit instruction count; zero fails the destination bound. */
	a64put(b, 0x37f80004U | ((fallback-b->n)&0x3fff)<<5);
	a64put(b, 0xeb0401bf); a64pcguard(b, 0x54000063, fallback);
	a64field(b, 4, 12, abi->moduleprog, 8);
	a64put(b, 0xd1000485); a64put(b, 0xb10008bf); a64pcguard(b, 0x54000069, fallback);
	a64put(b, 0xd2800009U | abi->instbytes<<5); a64put(b, 0x9b097dad);
	a64put(b, 0xab0d008d); a64pcguard(b, 0x54000063, fallback);
	a64put(b, 0xf900000dU | (u32int)(abi->pc/8)<<10);
	return 1;
}

/* Keep ownership and list-source publication behind all collector and lifetime guards. */
static int
a64owner(A64Buf *b, LeafLayout *abi, LeafOp *p)
{
	unsigned long long color;
	int offset, empty, fallback, same, marked, i, validated;

	if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
	   abi->heapref <= 0 || abi->heapref > 255 || abi->heapref%8 ||
	   abi->heapcolor <= 0 || abi->heapcolor > 255 || abi->heapcolor%4 ||
	   abi->refbytes != 8 || abi->mutator == nil || abi->nprop == nil ||
	   abi->markcolor < 0 || abi->markcolor > 255)
		return 0;
	offset=p->alu == 2 ? abi->listdata : abi->listtail;
	if(p->alu >= 2 && (offset < 0 || offset > 2047))
		return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	/* SELF has no source slot; every operation must validate its destination first. */
	a64ptr(b, 11, abi->d); a64pcspan(b, 11, 8, fallback);
	if(p->alu != 1){
		a64ptr(b, 9, abi->s); a64pcspan(b, 9, 8, fallback);
	}
	if(p->alu == 1)
		a64ptr(b, 12, abi->module);
	else{
		a64ptr(b, 9, abi->s); a64load(b, 12, 9, 8);
	}
	if(p->alu >= 2){
		a64pcguard(b, 0xb500006c, fallback);
		a64put(b, 0xb100059f); a64pcguard(b, 0x54000061, fallback);
		a64pcspan(b, 12, offset+8, fallback);
		a64put(b, 0x91000000U | offset<<10 | 12<<5 | 9);
		a64load(b, 12, 9, 8);
	}
	/* Validate the complete source lifetime before loading the old destination owner. */
	a64pcguard(b, 0xb500006c, fallback);
	a64put(b, 0xb100059f);
	validated=-1;
	if(p->alu == 1)
		a64pcguard(b, 0x54000061, fallback);
	else{
		validated=b->n; a64put(b, 0x54000000);
	}
	a64put(b, 0xf100019fU | (u32int)(abi->heapref > abi->heapcolor ? abi->heapref : abi->heapcolor)<<10);
	a64pcguard(b, 0x54000068, fallback);
	a64put(b, 0xf8400000U | ((u32int)-abi->heapref&511)<<12 | 12<<5 | 13);
	a64pcguard(b, 0xb500006d, fallback);
	a64put(b, 0xb10005bf); a64pcguard(b, 0x54000061, fallback);
	if(validated >= 0)
		b->code[validated] |= (b->n-validated)<<5;
	a64ptr(b, 11, abi->d); a64load(b, 10, 11, 8);
	a64put(b, 0xb100055f);
	same=b->n; a64put(b, 0x54000000);
	a64put(b, 0xeb0c015f); a64pcguard(b, 0x54000060, fallback);
	b->code[same] |= (b->n-same)<<5;
	empty=-1;
	if(p->alu != 1){
		a64put(b, 0xb100059f);
		empty=b->n; a64put(b, 0x54000000);
	}
	/* An identical owner skips the net-zero retain/release after validating its lifetime. */
	a64put(b, 0xb100055f);
	same=b->n; a64put(b, 0x54000001);
	/* Increment the native heap width, independently of the language WORD. */
	a64put(b, 0x910005ad);
	a64put(b, 0xf8000000U | ((u32int)-abi->heapref&511)<<12 | 12<<5 | 13);
	b->code[same] |= (b->n-same)<<5;
	a64put(b, 0xb8400000U | ((u32int)-abi->heapcolor&511)<<12 | 12<<5 | 14);
	color=(unsigned long long)abi->mutator;
	for(i=0; i<4; i++)
		a64put(b, (i == 0 ? 0xd2800000U : 0xf2800000U) | i<<21 | ((color>>(i*16))&65535)<<5 | 15);
	a64load(b, 15, 15, 4);
	a64put(b, 0x6b0f01df);
	marked=b->n; a64put(b, 0x54000000);
	/* Setmark changes only the heap color and the native int propagation flag. */
	a64put(b, 0x52800000U | abi->markcolor<<5 | 14);
	a64put(b, 0xb8000000U | ((u32int)-abi->heapcolor&511)<<12 | 12<<5 | 14);
	color=(unsigned long long)abi->nprop;
	for(i=0; i<4; i++)
		a64put(b, (i == 0 ? 0xd2800000U : 0xf2800000U) | i<<21 | ((color>>(i*16))&65535)<<5 | 15);
	a64put(b, 0x5280002e); a64put(b, 0xb90001ee);
	b->code[marked] |= (b->n-marked)<<5;
	if(empty >= 0)
		b->code[empty] |= (b->n-empty)<<5;
	if(p->alu >= 2)
		a64put(b, 0xf9000000U | (abi->s/8)<<10 | 9);
	a64store(b, 12, 8);
	return 1;
}


/* Emit an inline overlap-safe copy of captured byte pointers and count.
 * x12 is count and x9/x11 are source/destination; x13 and NZCV are scratch.
 * Pointer/count registers advance; REG x0, stack, LR and FP state are untouched.
 * Complete qwords precede the exact byte remainder; zero performs no accesses.
 * Return zero only if private code staging cannot hold the completed kernel.
 */
static int
a64copykernel(A64Buf *b)
{
	int other, done, loop, tail, finish;

	a64put(b, 0xeb09017f); /* cmp x11, x9 */
	other = b->n; a64put(b, 0x54000009); /* b.ls forward */
	a64put(b, 0x8b0c0129); a64put(b, 0x8b0c016b);
	/* Descend by complete qwords, then finish the exact byte prefix. */
	loop = b->n;
	a64put(b, 0xf100219f); /* cmp x12,#8 */
	tail = b->n; a64put(b, 0x54000003); /* b.lo byte tail */
	a64put(b, 0xd1002129); a64put(b, 0xd100216b);
	a64load(b, 13, 9, 8); a64store(b, 13, 8);
	a64put(b, 0xd100218c);
	a64put(b, 0x14000000U | ((u32int)(loop-b->n)&0x3ffffff));
	if(b->n > LeafMax) return 0;
	b->code[tail] |= (b->n-tail)<<5;
	finish = b->n; a64put(b, 0xb400000c);
	loop = b->n;
	a64put(b, 0xd1000529); a64put(b, 0xd100056b);
	a64load(b, 13, 9, 1); a64store(b, 13, 1);
	a64put(b, 0xf100058c); /* subs x12, x12, #1 */
	a64put(b, 0x54000001U | ((u32int)(loop-b->n)&0x7ffff)<<5);
	if(b->n > LeafMax) return 0;
	b->code[finish] |= (b->n-finish)<<5;
	done = b->n; a64put(b, 0x14000000);
	if(b->n > LeafMax) return 0;
	b->code[other] |= (b->n-other)<<5;
	/* Ascend without reading a qword until at least eight bytes remain. */
	loop = b->n;
	a64put(b, 0xf100219f);
	tail = b->n; a64put(b, 0x54000003);
	a64load(b, 13, 9, 8); a64store(b, 13, 8);
	a64put(b, 0x91002129); a64put(b, 0x9100216b);
	a64put(b, 0xd100218c);
	a64put(b, 0x14000000U | ((u32int)(loop-b->n)&0x3ffffff));
	if(b->n > LeafMax) return 0;
	b->code[tail] |= (b->n-tail)<<5;
	finish = b->n; a64put(b, 0xb400000c);
	loop = b->n;
	a64load(b, 13, 9, 1); a64store(b, 13, 1);
	a64put(b, 0x91000529); a64put(b, 0x9100056b);
	a64put(b, 0xf100058c);
	a64put(b, 0x54000001U | ((u32int)(loop-b->n)&0x7ffff)<<5);
	if(b->n > LeafMax) return 0;
	b->code[finish] |= (b->n-finish)<<5;
	if(b->n > LeafMax) return 0;
	b->code[done] |= b->n-done;
	return 1;
}


/* Resolve live metadata and check every used span before pointer-free record copying. */
static int
a64mapped(A64Buf *b, LeafLayout *a, LeafOp *p)
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
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	if(p->alu){
		a64ptr(b, 9, a->s); a64pcspan(b, 9, 8, fallback); a64load(b, 15, 9, 8);
		a64pcguard(b, 0xb500006f, fallback);
		a64put(b, 0xb10005ff); a64pcguard(b, 0x54000061, fallback);
	}
	a64ptr(b, 9, a->m); a64pcspan(b, 9, a->wordbytes, fallback);
	a64ptr(b, 13, a->module); a64pcspan(b, 13, a->linkmodule+8, fallback);
	a64field(b, 13, 13, a->linkmodule, 8);
	extent=a->moduletypes+8 > a->modulenty+4 ? a->moduletypes+8 : a->modulenty+4;
	a64pcspan(b, 13, extent, fallback);
	a64field(b, 14, 13, a->moduletypes, 8);
	a64field(b, 2, 13, a->modulenty, 4);
	/* Reject negative counts; unsigned bounds reject zero counts and negative indices. */
	a64put(b, 0x37f80002U | ((fallback-b->n)&0x3fff)<<5);
	a64ptr(b, 9, a->m);
	if(a->wordbytes == 4)
		a64put(b, 0xb980012c);
	else
		a64load(b, 12, 9, 8);
	a64put(b, 0xeb02019f); a64pcguard(b, 0x54000063, fallback);
	a64pcspan(b, 14, 8, fallback);
	/* A bounded int index cannot overflow its shift; addition and the cell can wrap. */
	a64put(b, 0xab0c0dcd); a64pcguard(b, 0x54000063, fallback);
	a64pcspan(b, 13, 8, fallback);
	a64load(b, 13, 13, 8);
	a64pcspan(b, 13, a->typesize+4, fallback);
	a64put(b, 0xb9800000U | (a->typesize/4)<<10 | 13<<5 | 12);
	a64put(b, 0xf104019f); a64pcguard(b, 0x54000069, fallback);
	empty=b->n; a64put(b, 0xb400000c);
	/* Empty records do not read np, payload bytes or list heap metadata. */
	a64pcspan(b, 13, a->typenp+4, fallback);
	a64put(b, 0xf24009bf); a64pcguard(b, 0x54000060, fallback); // tst x13,#7.
	a64field(b, 14, 13, a->typenp, 4);
	a64pcguard(b, 0x3400006e, fallback);
	if(p->alu){
		a64put(b, 0xf10001ffU | heapbase<<10); a64pcguard(b, 0x54000068, fallback);
		a64put(b, 0xf24009ff); a64pcguard(b, 0x54000060, fallback); // tst x15,#7.
		a64pcspan(b, 15, a->listdata+1, fallback);
		a64put(b, 0x910001e9U | a->listdata<<10);
	}else
		a64ptr(b, 9, a->s);
	a64ptr(b, 11, a->d);
	/* The size-plus-one carry check covers nil, H and wrapping complete payloads. */
	a64put(b, 0x91000585); // add x5,x12,#1.
	a64put(b, 0xd1000524); // sub x4,x9,#1.
	a64put(b, 0xab05009f); a64pcguard(b, 0x54000063, fallback);
	a64put(b, 0xd1000564); // sub x4,x11,#1.
	a64put(b, 0xab05009f); a64pcguard(b, 0x54000063, fallback);
	if(p->alu){
		a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 15<<5 | 14);
		a64pcguard(b, 0xb500006e, fallback);
		a64put(b, 0xb10005df); a64pcguard(b, 0x54000061, fallback);
		a64put(b, 0xf9000000U | (a->s/8)<<10 | 9);
	}
	if(!a64copykernel(b)) return 0;
	if(b->n > LeafMax) return 0;
	b->code[empty] |= (b->n-empty)<<5;
	return 1;
}


/* Copy a short byte-array slice after bounding both element and native address spans. */
static int
a64slicecopy(A64Buf *b, LeafLayout *a)
{
	int fallback, nilsource, nullsource, empty, forward, loop;

	if(a->arraylen < 0 || a->arraylen > 2047 || a->arraylen%a->wordbytes ||
	   a->arraydata < 0 || a->arraydata > 2047 || a->arraydata%8 ||
	   a->arraytype < 0 || a->arraytype > 2047 || a->arraytype%8 ||
	   a->typesize < 0 || a->typesize > 2047 || a->typesize%4 ||
	   a->typenp < 0 || a->typenp > 2047 || a->typenp%4)
		return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 9, a->s); a64load(b, 13, 9, 8);
	nullsource=b->n; a64put(b, 0xb400000d);
	a64put(b, 0xb10005bf);
	nilsource=b->n; a64put(b, 0x54000000);
	a64ptr(b, 9, a->d); a64load(b, 14, 9, 8);
	a64pcguard(b, 0xb500006e, fallback);
	a64put(b, 0xb10005df); a64pcguard(b, 0x54000061, fallback);
	a64wordfield(b, 12, 13, a->arraylen, a->wordbytes);
	a64put(b, 0xf102019f); a64pcguard(b, 0x54000069, fallback);
	a64ptr(b, 9, a->m); a64load(b, 4, 9, a->wordbytes);
	a64wordfield(b, 3, 14, a->arraylen, a->wordbytes);
	a64put(b, 0xb7f80003U | ((fallback-b->n)&0x3fff)<<5);
	/* Unsigned bounds against a nonnegative destination also reject negative indexes. */
	a64put(b, 0xeb03009f); a64pcguard(b, 0x54000069, fallback);
	a64put(b, 0xcb040063);
	a64put(b, 0xeb03019f); a64pcguard(b, 0x54000069, fallback);
	empty=b->n; a64put(b, 0xb400000c);
	/* arrayspan bounds the complete byte geometry, not just the start index. */
	a64put(b, 0x8b0c0083);
	a64put(b, 0xd35ffc63); a64pcguard(b, 0xb4000063, fallback);
	a64field(b, 2, 14, a->arraytype, 8);
	a64pcguard(b, 0xb5000062, fallback);
	a64put(b, 0xb100045f); a64pcguard(b, 0x54000061, fallback);
	a64field(b, 3, 2, a->typesize, 4);
	a64put(b, 0x7100047f); a64pcguard(b, 0x54000060, fallback);
	a64field(b, 3, 2, a->typenp, 4); a64pcguard(b, 0x34000063, fallback);
	a64field(b, 9, 13, a->arraydata, 8);
	a64pcguard(b, 0xb5000069, fallback);

	a64field(b, 11, 14, a->arraydata, 8);
	a64pcguard(b, 0xb500006b, fallback);

	/* Positive-length span additions reject H as well as ordinary address wrap. */
	a64put(b, 0xab04016b); a64pcguard(b, 0x54000063, fallback);
	a64put(b, 0xab0c0123); a64pcguard(b, 0x54000063, fallback);
	a64put(b, 0xab0c0163); a64pcguard(b, 0x54000063, fallback);
	/* One signed-stride loop preserves both directions of memmove overlap. */
	a64put(b, 0xd2800022); a64put(b, 0xeb09017f);
	forward=b->n; a64put(b, 0x54000009);
	a64put(b, 0x92800002);
	a64put(b, 0xd1000583);
	a64put(b, 0x8b030129); a64put(b, 0x8b03016b);
	if(b->n > LeafMax) return 0;
	b->code[forward] |= (b->n-forward)<<5;
	loop=b->n;
	a64load(b, 3, 9, 1); a64store(b, 3, 1);
	a64put(b, 0x8b020129); a64put(b, 0x8b02016b);
	a64put(b, 0xf100058c);
	a64put(b, 0x54000001U | ((loop-b->n)&0x7ffff)<<5);
	if(b->n > LeafMax) return 0;
	b->code[empty] |= (b->n-empty)<<5;
	b->code[nilsource] |= (b->n-nilsource)<<5;
	b->code[nullsource] |= (b->n-nullsource)<<5;
	return 1;
}


/* Bound the reference cell and String header while preserving the captured owner. */
static void
a64appendowner(A64Buf *b, int owner, int reference, int extent, int fallback)
{
	a64put(b, 0xd1000004U | (u32int)(reference+1)<<10 | owner<<5); // sub x4,owner,#reference+1.
	a64put(b, 0xb100009fU | (u32int)(reference+extent+1)<<10); // cmn x4,#reference+extent+1.
	a64defer(b, 0x54000002, fallback); // Reject unsigned carry before any owner read.
}


/* Append bounded matching representations after checking slots, ownership and capacities. */
static int
a64stringappend(A64Buf *b, LeafLayout *a)
{
	int fallback, overlap, empty, loop;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8)
		return 0;
	fallback = b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 9, a->m); a64ptr(b, 10, a->d);
	a64put(b, 0xeb0a013f); a64defer(b, 0x54000001, fallback);
	a64pcspan(b, 9, 8, fallback);
	a64load(b, 12, 9, 8);
	a64ptr(b, 9, a->s); a64pcspan(b, 9, 8, fallback);
	a64load(b, 13, 9, 8);
	a64appendowner(b, 12, a->heapref, a->stringdata, fallback);
	a64appendowner(b, 13, a->heapref, a->stringdata, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 12<<5 | 14);
	a64put(b, 0xf10005df); a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 13<<5 | 3);
	a64defer(b, 0xb4000003, fallback);
	if(a->stringmax == a->stringlen+4 && a->stringlen <= 252)
		a64put(b, 0x69400000U | (u32int)(a->stringlen/4)<<15 | 4<<10 | 12<<5 | 14); // LDPSW x14,x4,[x12].
	else{
		a64wordfield(b, 14, 12, a->stringlen, 4);
		a64wordfield(b, 4, 12, a->stringmax, 4);
	}
	a64put(b, 0x93400000U | 63<<16 | 63<<10 | 14<<5 | 6);
	a64put(b, 0xf10001df);
	a64put(b, 0xda80a400U | 14<<16 | 14<<5 | 14);
	if(a->stringmax == a->stringlen+4 && a->stringlen <= 252)
		a64put(b, 0x69400000U | (u32int)(a->stringlen/4)<<15 | 3<<10 | 13<<5 | 15); // LDPSW x15,x3,[x13].
	else{
		a64wordfield(b, 15, 13, a->stringlen, 4);
		a64wordfield(b, 3, 13, a->stringmax, 4);
	}
	a64put(b, 0xca0601e2); // EOR x2,x15,x6: representations must have equal signs.
	a64put(b, 0xb7f80002U | ((fallback-b->n)&0x3fff)<<5); // TBNZ x2,#63,fallback.
	a64put(b, 0xf10001ff);
	a64put(b, 0xda80a400U | 15<<16 | 15<<5 | 15);
	a64put(b, 0x8b0f01c5); // ADD x5,x14,x15: combined character count.
	/* Failed predicates retain LT through NZCV=8, sharing one rejection branch. */
	a64put(b, 0xf10401ff); // CMP x15,#256.
	a64put(b, 0xfa4f9068); // CCMP x3,x15,#8,ls: source capacity.
	a64put(b, 0xfa45a088); // CCMP x4,x5,#8,ge: destination capacity.
	a64defer(b, 0x5400000b, fallback);
	/* Preserve signed logical length before converting character offsets to bytes. */
	a64put(b, 0xf10000df);
	a64put(b, 0xda80a400U | 5<<16 | 5<<5 | 5);
	a64put(b, 0xb6f80066); // TBZ x6,#63 skips both Rune scaling instructions for ASCII.
	a64put(b, 0xd3400000U | 62<<16 | 61<<10 | 14<<5 | 14);
	a64put(b, 0xd3400000U | 62<<16 | 61<<10 | 15<<5 | 15);
	/* Capacity was checked in characters before forming native byte addresses. */
	a64put(b, 0x910001a9U | (u32int)a->stringdata<<10);
	a64put(b, 0x9100018bU | (u32int)a->stringdata<<10);
	a64put(b, 0x8b0e016b);
	a64put(b, 0x8b0f0123);
	/* End-to-owner comparisons cover both base-offset and extent overflow. */
	a64put(b, 0xeb0d007f); a64defer(b, 0x54000003, fallback);
	a64put(b, 0x8b0f0164); // ADD x4,x11,x15: destination end.
	a64put(b, 0xeb0c009f); a64defer(b, 0x54000003, fallback);
	a64put(b, 0xeb09017f);
	overlap = b->n; a64put(b, 0x54000009);
	a64put(b, 0xeb03017f); a64defer(b, 0x54000003, fallback);
	b->code[overlap] |= (b->n-overlap)<<5;
	empty = b->n; a64put(b, 0xb400000f);
	loop = b->n;
	a64put(b, 0x38401523); // LDRB w3,[x9],#1.
	a64put(b, 0x38001563); // STRB w3,[x11],#1.
	a64put(b, 0xf10005ef);
	a64put(b, 0x54000001U | ((loop-b->n)&0x7ffff)<<5);
	if(b->n > LeafMax)
		return 0;
	b->code[empty] |= (b->n-empty)<<5;
	a64put(b, 0xb9000000U | (u32int)(a->stringlen/4)<<10 | 12<<5 | 5);
	return 1;
}


/* Parse bounded decimal magnitudes in 64 bits and publish only complete results. */
static int
a64decimal(A64Buf *b, LeafLayout *a, LeafOp *p)
{
	int absent, empty, minus, digits, signonly, loop, positive, fallback;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047) return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	/* Reject invalid outputs before a parser can touch memory or FP state. */
	a64ptr(b, 10, a->d); a64pcspan(b, 10, p->bytes, fallback);
	a64ptr(b, 9, a->s); a64pcspan(b, 9, 8, fallback); a64load(b, 12, 9, 8);
	a64put(b, 0x5280000e); a64put(b, 0x5280000f);
	a64pcguard(b, 0xb500006c, fallback);
	a64put(b, 0xb100059f); absent=b->n; a64put(b, 0x54000000);
	a64pcspan(b, 12, a->stringdata, fallback);
	a64field(b, 13, 12, a->stringlen, 4);
	a64put(b, 0x710001bfU | (u32int)(p->input+1)<<10); a64pcguard(b, 0x54000069, fallback);
	a64field(b, 10, 12, a->stringmax, 4);
	a64put(b, 0x6b0d015f); a64pcguard(b, 0x5400006a, fallback);
	a64put(b, 0x91000189U | (u32int)a->stringdata<<10);
	/* Preserve string2c's terminator mutation by declining nonterminated ASCII. */
	a64put(b, 0x8b0d012a);
	/* Bound the trailing NUL cell before reading it, including inline-offset wrap. */
	a64put(b, 0x91000542); // ADD x2,x10,#1.
	a64put(b, 0xeb0c005f); a64defer(b, 0x54000003, fallback);
	a64load(b, 10, 10, 1); a64pcguard(b, 0x3400006a, fallback);
	empty=b->n; a64put(b, 0x3400000d);
	a64load(b, 10, 9, 1); a64put(b, 0x7100b55f);
	a64put(b, 0x1a9f17ef); /* cset w15,eq */
	minus=b->n; a64put(b, 0x54000000);
	a64put(b, 0x7100ad5f); digits=b->n; a64put(b, 0x54000001);
	b->code[minus] |= (b->n-minus)<<5;
	a64put(b, 0x91000529); a64put(b, 0x510005ad);
	b->code[digits] |= (b->n-digits)<<5;
	a64put(b, 0x710001bfU | (u32int)p->input<<10); a64pcguard(b, 0x54000069, fallback);
	/* Decline sign-only real conversions before executing any FP instruction. */
	if(p->alu)
		a64pcguard(b, 0x3500006d, fallback);
	signonly=b->n; a64put(b, 0x3400000d);
	loop=b->n;
	a64load(b, 10, 9, 1); a64put(b, 0x5100c14a);
	a64put(b, 0x7100255f); a64pcguard(b, 0x54000069, fallback);
	a64put(b, 0x8b0e09ce); /* value*=5 */
	a64put(b, 0x8b0e01ce); /* value*=2 */
	a64put(b, 0x8b0a01ce);
	a64put(b, 0x91000529); a64put(b, 0x710005ad);
	a64put(b, 0x54000001U | ((loop-b->n)&0x7ffff)<<5);
	if(!p->alu){
		/* MOVN forms the signed maximum; the captured minus sign permits MIN. */
		a64put(b, p->bytes == 4 ? 0x12b00002 : 0x92f00002);
		a64put(b, 0x8b0f0042); // ADD x2,x2,x15.
		a64put(b, 0xeb0201df); a64defer(b, 0x54000008, fallback);
	}
	positive=b->n; a64put(b, 0x3400000f);
	a64put(b, 0xcb0e03ee);
	if(b->n > LeafMax) return 0;
	b->code[absent] |= (b->n-absent)<<5; b->code[empty] |= (b->n-empty)<<5;
	b->code[signonly] |= (b->n-signonly)<<5; b->code[positive] |= (b->n-positive)<<5;
	if(p->alu){
		a64put(b, 0x9e6201c0); // SCVTF d0,x14 is exact for the bounded decimal accumulator.
		a64put(b, 0x9e66000d); // Move the binary64 representation to the integer store register.
		a64put(b, 0xaa000000U | 15<<16 | 63<<10 | 13<<5 | 13); // Keep negative zero from its captured sign.
	}else{
		a64put(b, 0xaa0e03ed); // Keep the complete signed 64-bit accumulator.
	}
	a64ptr(b, 11, a->d); a64store(b, 13, p->bytes);
	return 1;
}


/* Read a selected ASCII or Rune cell through a checked native end address. */
static int
a64stringindex(A64Buf *b, LeafLayout *a)
{
	int other, done, fallback;

	if(a->stringlen < 0 || a->stringlen > 16380 || a->stringlen%4 != 0 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 || a->stringdata%4 != 0 ||
	   a->stringmax < 0 || a->stringmax > 16380 || a->stringmax%4) return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	/* Validate scalar/pointer slot widths before reading either representation. */
	a64ptr(b, 9, a->s); a64pcspan(b, 9, 8, fallback);
	a64ptr(b, 9, a->m); a64pcspan(b, 9, a->wordbytes, fallback);
	a64ptr(b, 9, a->d); a64pcspan(b, 9, a->wordbytes, fallback);
	a64ptr(b, 9, a->s); a64load(b, 12, 9, 8);
	a64pcguard(b, 0xb500006c, fallback);
	a64put(b, 0xb100059f); a64pcguard(b, 0x54000061, fallback);
	a64pcspan(b, 12, a->stringdata, fallback);
	a64field(b, 13, 12, a->stringlen, 4);
	a64put(b, 0x52b0000e); a64put(b, 0x6b0e01bf); a64pcguard(b, 0x54000061, fallback);
	a64put(b, 0x2a0d03ef); /* preserve the representation sign */
	a64put(b, 0x36f8004d); a64put(b, 0x4b0d03ed);
	/* Check signed capacity while x12 still holds the original owner. */
	a64field(b, 14, 12, a->stringmax, 4);
	a64put(b, 0x6b0d01df); a64pcguard(b, 0x5400006a, fallback);
	a64ptr(b, 10, a->m); a64load(b, 14, 10, a->wordbytes);
	a64put(b, 0xeb0d01df); a64pcguard(b, 0x54000063, fallback);
	other = b->n; a64put(b, 0x36f8000f); /* tbz w15, #31, ASCII */
	a64put(b, 0x8b0e098d); // ADD x13,x12,x14,LSL #2.
	a64put(b, 0x910001adU | (u32int)(a->stringdata+4)<<10);
	a64put(b, 0xeb0c01bf); a64defer(b, 0x54000003, fallback);
	a64put(b, 0xb89fc1ad); // LDURSW x13,[x13,#-4].
	done = b->n; a64put(b, 0x14000000);
	b->code[other] |= (b->n-other)<<5;
	a64put(b, 0x8b0e018d); // ADD x13,x12,x14.
	a64put(b, 0x910001adU | (u32int)(a->stringdata+1)<<10);
	a64put(b, 0xeb0c01bf); a64defer(b, 0x54000003, fallback);
	a64put(b, 0x385ff1ad); // LDURB w13,[x13,#-1].
	b->code[done] |= b->n-done;
	a64ptr(b, 11, a->d); a64store(b, 13, a->wordbytes);
	return 1;
}


/* Update or append one ASCII/Rune character within uniquely owned capacity. */
static int
a64stringstore(A64Buf *b, LeafLayout *a)
{
	int wide, done, unchanged, fallback;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8) return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	/* Validate scalar/pointer slot widths before reading either representation. */
	a64ptr(b, 9, a->s); a64pcspan(b, 9, a->wordbytes, fallback);
	a64ptr(b, 9, a->m); a64pcspan(b, 9, a->wordbytes, fallback);
	a64ptr(b, 9, a->d); a64pcspan(b, 9, 8, fallback);
	a64ptr(b, 9, a->s); a64load(b, 12, 9, a->wordbytes);
	a64ptr(b, 10, a->m); a64load(b, 13, 10, a->wordbytes);
	a64ptr(b, 11, a->d); a64load(b, 14, 11, 8);
	a64pcguard(b, 0xb500006e, fallback);
	a64put(b, 0xb10005df); a64pcguard(b, 0x54000061, fallback);
	a64appendowner(b, 14, a->heapref, a->stringdata, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 14<<5 | 4);
	a64put(b, 0xf100049f); a64pcguard(b, 0x54000060, fallback);
	a64field(b, 15, 14, a->stringlen, 4);
	a64put(b, 0x52b00002); a64put(b, 0x6b0201ff); a64pcguard(b, 0x54000061, fallback);
	a64put(b, 0x2a0f03e2); a64put(b, 0x36f80042); a64put(b, 0x4b0203e2);
	a64field(b, 3, 14, a->stringmax, 4);
	a64put(b, 0x6b02007f); a64pcguard(b, 0x5400006a, fallback);
	a64put(b, 0xeb0201bf); a64pcguard(b, 0x54000069, fallback);
	a64put(b, 0xeb0301bf); a64pcguard(b, 0x54000063, fallback);
	wide=b->n; a64put(b, 0x37f8000f);
	a64put(b, 0x7102019f); a64pcguard(b, 0x54000063, fallback);
	a64put(b, 0x8b0d01cb); // ADD x11,x14,x13.
	a64put(b, 0x9100016bU | (u32int)(a->stringdata+1)<<10);
	a64put(b, 0xeb0e017f); a64defer(b, 0x54000003, fallback);
	a64put(b, 0x381ff16c); // STURB w12,[x11,#-1].
	done=b->n; a64put(b, 0x14000000);
	if(b->n > LeafMax) return 0;
	b->code[wide] |= (b->n-wide)<<5;
	a64put(b, 0x8b0d09cb); // ADD x11,x14,x13,LSL #2.
	a64put(b, 0x9100016bU | (u32int)(a->stringdata+4)<<10);
	a64put(b, 0xeb0e017f); a64defer(b, 0x54000003, fallback);
	a64put(b, 0xb81fc16c); // STUR w12,[x11,#-4].
	if(b->n > LeafMax) return 0;
	b->code[done] |= b->n-done;
	/* Appending within existing capacity changes only the character and signed length. */
	a64put(b, 0xeb0201bf);
	unchanged = b->n; a64put(b, 0x54000001);
	a64put(b, 0x52800023);
	a64put(b, 0x710001ff);
	a64put(b, 0x5a83a463); /* cneg w3,w3,lt */
	a64put(b, 0x0b0301ef);
	a64put(b, 0xb9000000U | (u32int)(a->stringlen/4)<<10 | 14<<5 | 15);
	if(b->n > LeafMax)
		return 0;
	b->code[unchanged] |= (b->n-unchanged)<<5;
	return 1;
}


/* Count digits, validate byte capacity, then fill unique ASCII or Rune storage backwards. */
static int
a64formatinteger(A64Buf *b, LeafLayout *a, LeafOp *op)
{
	int digits, scaled, storemax, fallback, count, decoded;
	unsigned long long identity;
	int half;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 128 || a->heapref%8 || a->refbytes != 8 ||
	   a->heaptype <= 0 || a->heaptype > 128 || a->heaptype%8 || a->stringtype == nil) return 0;
	/* All rejects share one return so the counting pass keeps the 80-unit bound. */
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 9, a->s); a64load(b, 12, 9, op->bytes);
	if(op->bytes == 4) a64put(b, 0x93407d8c); // Sign-extend the complete WORD32 input.
	if(op->alu){
		decoded=b->n; a64put(b, 0xb400000c);
		a64put(b, 0xd340cd82); a64defer(b, 0xb5000002, fallback); // Fraction bits must be zero.
		a64put(b, 0xd374f983); a64put(b, 0xd10ffc63);
		a64put(b, 0xf1004c7f); a64defer(b, 0x54000008, fallback); // Exponent 0..19.
		a64put(b, 0xd2800022); a64put(b, 0x9ac32042);
		a64put(b, 0xf241019f); a64put(b, 0xda82544c); // Apply the original sign to the magnitude.
		b->code[decoded] |= (b->n-decoded)<<5;
	}
	a64ptr(b, 11, a->d); a64load(b, 14, 11, 8);
	a64put(b, 0x910005c2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	/* Check exact heap type before treating a reused pointer slot as String. */
	a64put(b, 0xf8400000U | ((u32int)-a->heaptype&511)<<12 | 14<<5 | 2);
	identity = (unsigned long long)a->stringtype;
	a64put(b, 0xd2800003U | (identity&65535)<<5);
	for(half = 1; half < 4; half++){
		a64put(b, 0xf2800003U | half<<21 | ((identity>>(half*16))&65535)<<5);
	}
	a64put(b, 0xeb03005f); a64defer(b, 0x54000001, fallback);
	a64wordfield(b, 7, 14, a->stringlen, 4);
	a64put(b, 0xf10000ff); a64put(b, 0xda87a4e2); // x2 is absolute length; x7 preserves representation.
	a64wordfield(b, 3, 14, a->stringmax, 4);
	a64put(b, 0xeb02007f); a64defer(b, 0x5400000b, fallback);
	scaled=b->n; a64put(b, 0xb6f80007);
	a64put(b, 0xd37ef463); // Rune capacity becomes byte capacity only if representable.
	a64put(b, 0xd35ffc62); a64defer(b, 0xb5000002, fallback);
	b->code[scaled] |= (b->n-scaled)<<5;
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 14<<5 | 2);
	a64put(b, 0xf100045f); a64defer(b, 0x54000001, fallback);
	/* x6 counts an unsigned magnitude while x12 retains the signed input. */
	a64put(b, 0xf100019f);
	a64put(b, 0xda8ca586); a64put(b, 0x9a9fa7e4);
	a64put(b, 0xd280014d);
	count=b->n;
	a64put(b, 0x9acd08c6); a64put(b, 0x91000484);
	a64put(b, 0xb5000006U | ((count-b->n)&0x7ffff)<<5);
	a64put(b, 0xeb04007f); a64defer(b, 0x5400000b, fallback);
	/* Form the byte payload and its end before publishing converted capacity. */
	a64put(b, 0xb10001cbU | (u32int)a->stringdata<<10); a64defer(b, 0x54000002, fallback);
	a64put(b, 0xab040169); a64defer(b, 0x54000002, fallback);
	storemax=b->n; a64put(b, 0xb6f80007);
	a64put(b, 0xb9000000U | (u32int)(a->stringmax/4)<<10 | 14<<5 | 3);
	b->code[storemax] |= (b->n-storemax)<<5;
	a64put(b, 0x3900013f); // Terminate at the end counted without touching the payload.
	a64put(b, 0x528005a3); a64put(b, 0x39000163); // Positive digits overwrite this temporary minus.
	a64put(b, 0xf100019f); a64put(b, 0xda8ca58c);
	digits=b->n;
	a64put(b, 0x9acd0982); a64put(b, 0x9b0db043);
	a64put(b, 0x1100c063); a64put(b, 0x381ffd23); // Store byte with pre-decrement.
	a64put(b, 0xaa0203ec);
	a64put(b, 0xb500000cU | ((digits-b->n)&0x7ffff)<<5);
	if(b->n > LeafMax) return 0;
	a64put(b, 0xb9000000U | (u32int)(a->stringlen/4)<<10 | 14<<5 | 4);
	return 1;
}


/* Update a unique prefix or copy a bounded slice after ownership and extent checks. */
static int
a64stringslice(A64Buf *b, LeafLayout *a)
{
	int ascii, done, loop, prefix, fallback;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringlen == a->stringmax || a->stringdata < a->stringlen+4 ||
	   a->stringdata < a->stringmax+4 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8) return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	/* Scalar geometry slots and the native owner slot have independent widths. */
	a64ptr(b, 9, a->s); a64pcspan(b, 9, a->wordbytes, fallback);
	a64ptr(b, 9, a->m); a64pcspan(b, 9, a->wordbytes, fallback);
	a64ptr(b, 9, a->d); a64pcspan(b, 9, 8, fallback);
	a64ptr(b, 9, a->s); a64load(b, 12, 9, a->wordbytes);
	a64ptr(b, 10, a->m); a64load(b, 13, 10, a->wordbytes);
	if(a->wordbytes == 4){a64put(b, 0x93407d8c); a64put(b, 0x93407dad);}
	a64put(b, 0xf100019f); a64pcguard(b, 0x5400006a, fallback); // start >= 0.
	a64put(b, 0xeb0c01bf); a64pcguard(b, 0x54000068, fallback); // end > start.
	/* Only unsigned 31-bit end values may reach any owner metadata. */
	a64put(b, 0xd35ffda4); // lsr x4,x13,#31.
	a64pcguard(b, 0xb4000064, fallback);
	a64ptr(b, 11, a->d); a64load(b, 14, 11, 8);
	a64pcguard(b, 0xb500006e, fallback);
	a64put(b, 0xb10005df); a64pcguard(b, 0x54000061, fallback);
	/* Bound exactly the metadata read by prefixes, preserving payload-free reuse. */
	a64appendowner(b, 14, a->heapref,
		a->stringlen+4 > a->stringmax+4 ? a->stringlen+4 : a->stringmax+4, fallback);
	a64wordfield(b, 15, 14, a->stringlen, 4);
	a64put(b, 0xf10001ff); // Preserve representation while deriving absolute length.
	a64put(b, 0xda8fa5e2); // cneg x2,x15,lt.
	a64wordfield(b, 3, 14, a->stringmax, 4);
	a64put(b, 0xeb02007f); a64pcguard(b, 0x5400006a, fallback);
	a64put(b, 0xeb0201bf); a64pcguard(b, 0x54000069, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 14<<5 | 6);
	a64put(b, 0xf10004df); a64pcguard(b, 0x54000060, fallback);
	a64put(b, 0xcb0c01a4); // x4 = end-start, validated in the character domain.
	/* A zero-start slice retains its payload and needs no bounded copy loop. */
	prefix=b->n; a64put(b, 0xb400000c);
	a64put(b, 0xf104009f); a64pcguard(b, 0x54000069, fallback);
	b->code[prefix] |= (b->n-prefix)<<5;
	a64put(b, 0xaa0403e5); // x5 retains the resulting signed character count.
	ascii=b->n; a64put(b, 0xb6f8000f);
	a64put(b, 0xcb0503e5);
	a64put(b, 0xd37ef58c); a64put(b, 0xd37ef484); // Scale Rune offsets/counts only after bounds checks.
	b->code[ascii] |= (b->n-ascii)<<5;
	done=b->n; a64put(b, 0xb400000c);
	a64put(b, 0x910001cbU | (u32int)a->stringdata<<10);
	a64put(b, 0x8b0c0169);
	/* The complete selected source span also bounds the lower destination. */
	a64put(b, 0x8b040123); // ADD x3,x9,x4.
	a64put(b, 0xeb0e007f); a64defer(b, 0x54000003, fallback);
	loop=b->n;
	a64put(b, 0x38401526); // LDRB w6,[x9],#1.
	a64put(b, 0x38001566); // STRB w6,[x11],#1.
	a64put(b, 0xf1000484);
	a64put(b, 0x54000001U | ((loop-b->n)&0x7ffff)<<5);
	if(b->n > LeafMax) return 0;
	b->code[done] |= (b->n-done)<<5;
	a64put(b, 0xb9000000U | (u32int)(a->stringlen/4)<<10 | 14<<5 | 5);
	return 1;
}


/* Publish pointer-free stack frames only after every metadata and extent guard. */
static int
a64frame(A64Buf *b, LeafLayout *a, int imported)
{
	int offsets[10], i, fallback, extent, origin;

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
	a64put(b, 0x14000003);
	fallback=b->n; a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	if(imported){
		/* Keep the full WORD index; negative export ordinals retain C lookup. */
		a64ptr(b, 9, a->s); a64pcspan(b, 9, 8, fallback); a64load(b, 13, 9, 8);
		extent=a->linknlinks+4>a->linklinks ? a->linknlinks+4 : a->linklinks;
		a64pcspan(b, 13, extent, fallback);
		origin=a->heapref;
		if(a->heapcolor>origin && a->heapcolor<=255)origin=a->heapcolor;
		if(a->heaptype>origin && a->heaptype<=255)origin=a->heaptype;
		a64put(b, 0xf10001bfU | (u32int)origin<<10); a64defer(b, 0x54000009, fallback);
		a64put(b, 0x910005a2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
		/* Match the C error boundary before fetching a possibly inaccessible index. */
		a64ptr(b, 10, a->m); a64pcspan(b, 10, a->wordbytes, fallback); a64load(b, 12, 10, a->wordbytes);
		if(a->wordbytes==4)a64put(b, 0x93407d8c);
		a64wordfield(b, 14, 13, a->linknlinks, 4);
		a64put(b, 0xf10001df); a64defer(b, 0x5400000b, fallback);
		a64put(b, 0xeb0e019f); a64defer(b, 0x54000002, fallback);
		a64put(b, 0xab0c11ad); // adds x13,x13,x12,lsl #4 detects entry-base carry.
		a64defer(b, 0x54000002, fallback);
		a64pcspan(b, 13, a->linklinks+a->modlbytes, fallback);
		a64field(b, 14, 13, a->linklinks+a->modlframe, 8);
	}else{
		a64ptr(b, 9, a->s); a64pcspan(b, 9, a->wordbytes, fallback); a64load(b, 12, 9, a->wordbytes);
		if(a->wordbytes==4)a64put(b, 0x93407d8c);
		a64ptr(b, 13, a->module);
		for(i=0; i<3; i++){
			a64put(b, 0x910005a2); a64put(b, 0xf100045f);
			a64defer(b, 0x54000009, fallback);
			if(i==0){a64pcspan(b, 13, a->linkmodule+8, fallback); a64field(b, 13, 13, a->linkmodule, 8);}
			if(i==1){
				extent=a->moduletypes+8>a->modulenty+4 ? a->moduletypes+8 : a->modulenty+4;
				a64pcspan(b, 13, extent, fallback);
				a64wordfield(b, 14, 13, a->modulenty, 4);
				a64put(b, 0xf10001df); a64defer(b, 0x5400000b, fallback);
				a64put(b, 0xeb0e019f); a64defer(b, 0x54000002, fallback);
				a64field(b, 13, 13, a->moduletypes, 8);
			}
		}
		a64put(b, 0xab0c0dad); // adds x13,x13,x12,lsl #3 detects selected-slot carry.
		a64defer(b, 0x54000002, fallback);
		a64pcspan(b, 13, 8, fallback);
		a64field(b, 14, 13, 0, 8);
	}
	a64put(b, 0x910005c2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64pcspan(b, 14, a->typesize+4>a->typenp+4 ? a->typesize+4 : a->typenp+4, fallback);
	a64put(b, 0xf24009df); a64defer(b, 0x54000001, fallback); // Type must have native pointer alignment.
	a64wordfield(b, 15, 14, a->typesize, 4);
	a64put(b, 0xd2800002U | (u32int)a->framesize<<5);
	a64put(b, 0xeb0201ff); a64defer(b, 0x5400000b, fallback);
	a64field(b, 2, 14, a->typenp, 4); a64defer(b, 0xb5000002, fallback);
	a64ptr(b, 9, a->sp); a64ptr(b, 10, a->ts);
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xb100055f); a64defer(b, 0x54000000, fallback);
	a64put(b, 0xeb09015f); a64defer(b, 0x54000003, fallback);
	a64put(b, 0xcb090142); a64put(b, 0xeb0f005f); a64defer(b, 0x54000009, fallback);
	a64ptr(b, 11, a->d); a64pcspan(b, 11, 8, fallback);
	a64put(b, 0x8b0f0122);
	a64put(b, 0xf9000000U | (u32int)(a->sp/8)<<10 | 2);
	a64put(b, 0xf9000000U | (u32int)(a->frametype/8)<<10 | 9<<5 | 14);
	a64put(b, 0xf9000000U | (u32int)(a->framemr/8)<<10 | 9<<5 | 31);
	a64store(b, 9, 8);
	return 1;
}


/* Restore pointer-free frames and release retained same-mode module owners. */
static int
a64return(A64Buf *b, LeafLayout *a)
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
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	/* x9 keeps the frame, x13 the caller FP, x14 the PC and x15 the module. */
	origin=a->heapref;
	if(a->heapcolor>origin && a->heapcolor<=255)origin=a->heapcolor;
	if(a->heaptype>origin && a->heaptype<=255)origin=a->heaptype;
	extent=a->frametype;
	if(a->framemr>extent)extent=a->framemr;
	if(a->framefp>extent)extent=a->framefp;
	if(a->framelr>extent)extent=a->framelr;
	a64ptr(b, 9, a->fp); a64pcspan(b, 9, extent+8, fallback);
	a64put(b, 0x91000522); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64field(b, 13, 9, a->framefp, 8);
	a64put(b, 0x910005a2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64field(b, 14, 9, a->framelr, 8);
	a64put(b, 0x910005c2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	/* Root and invalid-link failures must not read an otherwise unused Type. */
	a64field(b, 15, 9, a->framemr, 8);
	a64put(b, 0xb10005ff); a64defer(b, 0x54000000, fallback); // H saved module.
	early=b->n; a64put(b, 0xb400000f);
	a64ptr(b, 10, a->module);
	a64put(b, 0x91000542); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	if(b->n>LeafMax)return 0;
	b->code[early] |= (b->n-early)<<5;
	a64field(b, 12, 9, a->frametype, 8);
	a64put(b, 0x91000582); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64pcspan(b, 12, a->typenp+4, fallback);
	a64field(b, 2, 12, a->typenp, 4); a64defer(b, 0x35000002, fallback);
	/* Ordinary returns never dereference the unused current-module owner. */
	a64field(b, 15, 9, a->framemr, 8);
	ordinary=b->n; a64put(b, 0xb400000f);
	a64put(b, 0x910005e2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64ptr(b, 10, a->module);
	a64put(b, 0x91000542); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64pcspan(b, 15, a->linkmp+8>a->linkcompiled+4 ? a->linkmp+8 : a->linkcompiled+4, fallback);
	a64pcspan(b, 10, a->linkcompiled+4, fallback);
	a64put(b, 0xf100015fU | (u32int)origin<<10); // cmp x10,heapref.
	a64defer(b, 0x54000009, fallback);
	a64field(b, 2, 10, a->linkcompiled, 4); a64field(b, 3, 15, a->linkcompiled, 4);
	a64put(b, 0x6b03005f); a64defer(b, 0x54000001, fallback);
	/* x10/x11 retain the release target/count; x1 captures the caller MP. */
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 10<<5 | 11);
	a64put(b, 0xf100057f); a64defer(b, 0x54000009, fallback);
	a64put(b, 0xd100056b);
	a64field(b, 1, 15, a->linkmp, 8);
	if(b->n>LeafMax)return 0;
	b->code[ordinary] |= (b->n-ordinary)<<5;
	/* No metadata reads or failure branches follow the first VM store. */
	a64put(b, 0xf9000000U | (u32int)(a->fp/8)<<10 | 13);
	a64put(b, 0xf9000000U | (u32int)(a->sp/8)<<10 | 9);
	a64put(b, 0xf9000000U | (u32int)(a->pc/8)<<10 | 14);
	done=b->n; a64put(b, 0xb400000f);
	a64put(b, 0xf8000000U | ((u32int)-a->heapref&511)<<12 | 10<<5 | 11);
	a64put(b, 0xf9000000U | (u32int)(a->module/8)<<10 | 15);
	a64put(b, 0xf9000000U | (u32int)(a->regmp/8)<<10 | 1);
	if(b->n>LeafMax)return 0;
	b->code[done] |= (b->n-done)<<5;
	return 1;
}


/* Publish same-mode bytecode calls after owner, entry and frame checks. */
static int
a64modulecall(A64Buf *b, LeafLayout *a)
{
	int offsets[11], i, fallback, extent, origin;

	offsets[0]=a->module; offsets[1]=a->fp; offsets[2]=a->regmp; offsets[3]=a->linkprog;
	offsets[4]=a->linkmp; offsets[5]=a->linklinks; offsets[6]=a->framelr;
	offsets[7]=a->framefp; offsets[8]=a->framemr; offsets[9]=a->regtemp; offsets[10]=a->linknlinks;
	for(i=0;i<11;i++)
		if(offsets[i]<0 || offsets[i]>2047 || offsets[i]%(i<9 ? 8 : i==9 ? a->wordbytes : 4))return 0;
	if(a->linkcompiled<0 || a->linkcompiled>2047 || a->linkcompiled%4 ||
	   a->modlbytes!=16 || a->modlpc<0 || a->modlpc>8 || a->modlpc%8 || a->heapref<=0 || a->heapref>128 || a->heapref%8 || a->refbytes!=8 ||
	   a->framelr==a->framefp || a->framelr==a->framemr || a->framefp==a->framemr)return 0;
	a64put(b, 0x14000003);
	fallback=b->n; a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	origin=a->heapref;
	if(a->heapcolor>origin && a->heapcolor<=255)origin=a->heapcolor;
	if(a->heaptype>origin && a->heaptype<=255)origin=a->heaptype;
	extent=a->linkprog+8;
	if(a->linkcompiled+4>extent)extent=a->linkcompiled+4;
	if(a->linknlinks+4>extent)extent=a->linknlinks+4;
	if(a->linkmp+8>extent)extent=a->linkmp+8;
	if(a->linklinks>extent)extent=a->linklinks;
	a64ptr(b, 10, a->d); a64pcspan(b, 10, 8, fallback); a64load(b, 9, 10, 8);
	a64pcspan(b, 9, extent, fallback);
	a64put(b, 0xf100013fU | (u32int)origin<<10); // cmp x9,heapref.
	a64defer(b, 0x54000009, fallback);
	a64put(b, 0x91000523); a64put(b, 0xf100047f); a64defer(b, 0x54000009, fallback);
	a64ptr(b, 5, a->module); a64pcspan(b, 5, a->linkcompiled+4, fallback);
	a64field(b, 13, 9, a->linkcompiled, 4); a64field(b, 14, 5, a->linkcompiled, 4);
	a64put(b, 0x6b0e01bf); a64defer(b, 0x54000001, fallback);
	a64field(b, 12, 9, a->linkprog, 8);
	a64put(b, 0x91000583); a64put(b, 0xf100047f); a64defer(b, 0x54000009, fallback);
	a64ptr(b, 10, a->m); a64pcspan(b, 10, a->wordbytes, fallback); a64load(b, 12, 10, a->wordbytes);
	if(a->wordbytes==4)a64put(b, 0x93407d8c);
	a64wordfield(b, 13, 9, a->linknlinks, 4);
	a64put(b, 0xf10001bf); a64defer(b, 0x5400000b, fallback);
	a64put(b, 0xeb0d019f); a64defer(b, 0x54000002, fallback);
	a64put(b, 0xab0c1123); // adds x3,x9,x12,lsl #4 detects entry-base wrap.
	a64defer(b, 0x54000002, fallback);
	a64pcspan(b, 3, a->linklinks+a->modlpc+8, fallback);
	a64field(b, 11, 3, a->linklinks+a->modlpc, 8);
	a64put(b, 0x91000563); a64put(b, 0xf100047f); a64defer(b, 0x54000009, fallback);
	a64ptr(b, 14, a->s); a64pcspan(b, 14, 8, fallback); a64load(b, 10, 14, 8);
	a64put(b, 0x91000543); a64put(b, 0xf100047f); a64defer(b, 0x54000009, fallback);
	extent=a->framelr;
	if(a->framefp>extent)extent=a->framefp;
	if(a->framemr>extent)extent=a->framemr;
	a64pcspan(b, 10, extent+8, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 9<<5 | 13);
	a64put(b, 0x910005ad); a64put(b, 0xf10005bf); a64defer(b, 0x54000009, fallback);
	a64ptr(b, 14, a->pc); a64ptr(b, 15, a->fp);
	a64field(b, 2, 9, a->linkmp, 8);
	a64put(b, 0xf9000000U | (u32int)(a->framelr/8)<<10 | 10<<5 | 14);
	a64put(b, 0xf9000000U | (u32int)(a->framefp/8)<<10 | 10<<5 | 15);
	a64put(b, 0xf9000000U | (u32int)(a->framemr/8)<<10 | 10<<5 | 5);
	a64put(b, 0xf9000000U | (u32int)(a->fp/8)<<10 | 10);
	a64put(b, 0xf9000000U | (u32int)(a->module/8)<<10 | 9);
	a64put(b, 0xf8000000U | ((u32int)-a->heapref&511)<<12 | 9<<5 | 13);
	a64put(b, 0xf9000000U | (u32int)(a->regmp/8)<<10 | 2);
	a64put(b, 0xf9000000U | (u32int)(a->pc/8)<<10 | 11);
	a64put(b, 0x52800023);
	a64put(b, (a->wordbytes==8 ? 0xf9000000U : 0xb9000000U) |
		(u32int)(a->regtemp/a->wordbytes)<<10 | 3);
	return 1;
}


/* Narrow a unique array view after validating full WORD bounds and native spans. */
static int
a64arrayslice(A64Buf *b, LeafLayout *a)
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
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	/* x12/x13 retain start/end, x14 the view, and x15 its element Type. */
	a64ptr(b, 9, a->s); a64wordfield(b, 12, 9, 0, a->wordbytes);
	a64ptr(b, 10, a->m); a64wordfield(b, 13, 10, 0, a->wordbytes);
	a64put(b, 0xf100019f); a64defer(b, 0x5400000b, fallback);
	a64put(b, 0xeb0c01bf); a64defer(b, 0x5400000b, fallback);
	a64ptr(b, 11, a->d); a64field(b, 14, 11, 0, 8);
	a64put(b, 0x910005c2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64wordfield(b, 2, 14, a->arraylen, a->wordbytes);
	a64put(b, 0xeb0201bf); a64defer(b, 0x5400000c, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 14<<5 | 2);
	a64put(b, 0xf100045f); a64defer(b, 0x54000001, fallback);
	a64field(b, 15, 14, a->arraytype, 8);
	a64put(b, 0x910005e2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64wordfield(b, 2, 15, a->typeref, 4);
	a64put(b, 0xf100005f); a64defer(b, 0x5400000d, fallback);
	/* A view retains its backing owner; a root requires an empty pointer map. */
	a64field(b, 3, 14, a->arrayroot, 8);
	a64put(b, 0x91000462);
	rootcase=b->n; a64put(b, 0xb4000002);
	a64defer(b, 0xb4000003, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 3<<5 | 2);
	a64defer(b, 0xb4000002, fallback);
	rootdone=b->n; a64put(b, 0x14000000);
	if(b->n>LeafMax)return 0;
	b->code[rootcase] |= (b->n-rootcase)<<5;
	a64field(b, 2, 15, a->typenp, 4); a64defer(b, 0x35000002, fallback);
	if(b->n>LeafMax)return 0;
	b->code[rootdone] |= b->n-rootdone;
	a64wordfield(b, 3, 15, a->typesize, 4);
	a64put(b, 0xf100007f); a64defer(b, 0x5400000b, fallback);
	a64field(b, 4, 14, a->arraydata, 8);
	empty=b->n; a64put(b, 0xb4000003);
	/* A positive element width bounds end*size before either pointer addition. */
	a64put(b, 0x12b00002);
	a64put(b, 0xeb0201bf); a64defer(b, 0x54000008, fallback);
	a64put(b, 0x9b037da1);
	a64put(b, 0xeb02003f); a64defer(b, 0x54000008, fallback);
	a64defer(b, 0xb400000d, fallback);
	a64put(b, 0x91000482); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64put(b, 0xab010082); a64defer(b, 0x54000002, fallback);
	a64put(b, 0x9b037d82); a64put(b, 0x8b020084);
	if(b->n>LeafMax)return 0;
	b->code[empty] |= (b->n-empty)<<5;
	/* Only the view length/data change; all owner and descriptor counts survive. */
	a64put(b, 0xcb0c01a2);
	a64put(b, (a->wordbytes==8 ? 0xf9000000U : 0xb9000000U) | (u32int)(a->arraylen/a->wordbytes)<<10 | 14<<5 | 2);
	a64put(b, 0xf9000000U | (u32int)(a->arraydata/8)<<10 | 14<<5 | 4);
	return 1;
}


/* Preflight the complete ASCII input before overlap-aware reuse and owner publication. */
static int
a64bytestring(A64Buf *b, LeafLayout *a)
{
	int fallback, header, scan, forward, backward, loop, jump, half;
	unsigned long long identity;

	if(!leafbyteconversionlayout(a))
		return 0;
	header = a->arraylen;
	if(header < a->arraydata) header = a->arraydata;
	if(header < a->arraytype) header = a->arraytype;
	if(header < a->arrayroot) header = a->arrayroot;
	fallback = b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 8, a->s); a64load(b, 8, 8, 8);
	a64put(b, 0xf100011fU | (u32int)a->heapcolor<<10); // cmp x8, heap extent.
	a64defer(b, 0x54000009, fallback);
	a64put(b, 0xb100011fU | (u32int)(header+8)<<10); // cmn x8, header extent.
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heaptype&511)<<12 | 8<<5);
	identity = (unsigned long long)a->arrayidentity;
	for(half = 0; half < 4; half++)
		a64put(b, (half == 0 ? 0xd280000fU : 0xf280000fU) | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xeb0f01dfU); // cmp x14, x15
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heapref&511)<<12 | 8<<5);
	a64defer(b, 0xb400000e, fallback);
	a64field(b, 14, 8, a->arrayroot, 8);
	a64put(b, 0xb10005dfU); // cmn x14, #1
	a64defer(b, 0x54000001, fallback);
	a64field(b, 14, 8, a->arraytype, 8);
	a64defer(b, 0xb400000e, fallback);
	a64put(b, 0xb10001dfU | (u32int)((a->typesize > a->typenp ? a->typesize : a->typenp)+4)<<10);
	a64defer(b, 0x54000002, fallback);
	a64wordfield(b, 15, 14, a->typenp, 4);
	a64defer(b, 0xb500000f, fallback);
	a64wordfield(b, 15, 14, a->typesize, 4);
	a64put(b, 0xf10005ffU); // cmp x15, #1
	a64defer(b, 0x54000001, fallback);
	a64wordfield(b, 11, 8, a->arraylen, a->wordbytes);
	a64put(b, 0xd100056eU); // sub x14, x11, #1
	a64put(b, 0xf103fddfU); // cmp x14, #255
	a64defer(b, 0x54000008, fallback);
	a64field(b, 9, 8, a->arraydata, 8);
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xab0b013fU); // cmn x9, x11
	a64defer(b, 0x54000002, fallback);
	a64ptr(b, 8, a->d); a64load(b, 10, 8, 8);
	a64put(b, 0xf100015fU | (u32int)a->heapcolor<<10);
	a64defer(b, 0x54000009, fallback);
	a64put(b, 0xb100015fU | (u32int)((a->stringlen > a->stringmax ? a->stringlen : a->stringmax)+4)<<10);
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heaptype&511)<<12 | 10<<5);
	identity = (unsigned long long)a->stringtype;
	for(half = 0; half < 4; half++)
		a64put(b, (half == 0 ? 0xd280000fU : 0xf280000fU) | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xeb0f01dfU); // cmp x14, x15
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heapref&511)<<12 | 10<<5);
	a64put(b, 0xf10005dfU); // cmp x14, #1
	a64defer(b, 0x54000001, fallback);
	a64wordfield(b, 14, 10, a->stringlen, 4);
	a64put(b, 0xf10001dfU); // cmp x14, #0
	a64defer(b, 0x5400000b, fallback);
	a64wordfield(b, 15, 10, a->stringmax, 4);
	a64put(b, 0xeb0f01dfU); // cmp x14, x15
	a64defer(b, 0x5400000c, fallback);
	a64put(b, 0xeb0f017fU); // cmp x11, x15
	a64defer(b, 0x5400000c, fallback);
	a64put(b, 0xb100014cU | (u32int)a->stringdata<<10); // adds x12, x10, data offset.
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xab0b019fU); // cmn x12, x11
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xd280000dU); // mov x13, #0
	scan = b->n;
	a64put(b, 0x386d692eU); // ldrb w14, [x9, x13]
	a64put(b, 0x710201dfU); // cmp w14, #128
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0x910005adU); // add x13, x13, #1
	a64put(b, 0xeb0b01bfU); // cmp x13, x11
	a64put(b, 0x54000003U | ((scan-b->n)&0x7ffff)<<5);
	/* No fallible operations remain after the first payload write. */
	a64put(b, 0xeb09019fU); // cmp x12, x9
	forward = b->n; a64put(b, 0x54000009);
	a64put(b, 0xaa0b03edU); // mov x13, x11
	backward = b->n;
	a64put(b, 0xd10005adU); // sub x13, x13, #1
	a64put(b, 0x386d692eU); // ldrb w14, [x9, x13]
	a64put(b, 0x382d698eU); // strb w14, [x12, x13]
	a64put(b, 0xb500000dU | ((backward-b->n)&0x7ffff)<<5);
	jump = b->n; a64put(b, 0x14000000);
	if(b->n > LeafMax) return 0;
	b->code[forward] |= (b->n-forward)<<5;
	a64put(b, 0xd280000dU); // mov x13, #0
	loop = b->n;
	a64put(b, 0x386d692eU); // ldrb w14, [x9, x13]
	a64put(b, 0x382d698eU); // strb w14, [x12, x13]
	a64put(b, 0x910005adU); // add x13, x13, #1
	a64put(b, 0xeb0b01bfU); // cmp x13, x11
	a64put(b, 0x54000003U | ((loop-b->n)&0x7ffff)<<5);
	if(b->n > LeafMax) return 0;
	b->code[jump] |= b->n-jump;
	a64put(b, 0xb900014bU | (u32int)(a->stringlen/4)<<10); // Store the captured ASCII length.
	a64put(b, 0xf900010aU); // str x10, [x8]
	return 1;
}


/* Capture byte storage before overlap-aware array reuse and final owner publication. */
static int
a64stringbytes(A64Buf *b, LeafLayout *a)
{
	int fallback, header, forward, backward, loop, jump, half;
	unsigned long long identity;

	if(!leafbyteconversionlayout(a))
		return 0;
	header = a->arraylen;
	if(header < a->arraydata) header = a->arraydata;
	if(header < a->arraytype) header = a->arraytype;
	if(header < a->arrayroot) header = a->arrayroot;
	fallback = b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 8, a->s); a64load(b, 8, 8, 8);
	a64put(b, 0xf100011fU | (u32int)a->heapcolor<<10);
	a64defer(b, 0x54000009, fallback);
	a64put(b, 0xb100011fU | (u32int)((a->stringlen > a->stringmax ? a->stringlen : a->stringmax)+4)<<10);
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heaptype&511)<<12 | 8<<5);
	identity = (unsigned long long)a->stringtype;
	for(half = 0; half < 4; half++)
		a64put(b, (half == 0 ? 0xd280000fU : 0xf280000fU) | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xeb0f01dfU); // cmp x14, x15
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heapref&511)<<12 | 8<<5);
	a64defer(b, 0xb400000e, fallback);
	a64wordfield(b, 11, 8, a->stringlen, 4);
	a64put(b, 0xd100056eU); // sub x14, x11, #1
	a64put(b, 0xf103fddfU); // cmp x14, #255
	a64defer(b, 0x54000008, fallback);
	a64wordfield(b, 15, 8, a->stringmax, 4);
	a64put(b, 0xeb0f017fU); // cmp x11, x15
	a64defer(b, 0x5400000c, fallback);
	a64put(b, 0xb1000109U | (u32int)a->stringdata<<10); // adds x9,x8,data.
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xab0b013fU); // cmn x9, x11
	a64defer(b, 0x54000002, fallback);
	a64ptr(b, 8, a->d); a64load(b, 10, 8, 8);
	a64put(b, 0xf100015fU | (u32int)a->heapcolor<<10);
	a64defer(b, 0x54000009, fallback);
	a64put(b, 0xb100015fU | (u32int)(header+8)<<10);
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heaptype&511)<<12 | 10<<5);
	identity = (unsigned long long)a->arrayidentity;
	for(half = 0; half < 4; half++)
		a64put(b, (half == 0 ? 0xd280000fU : 0xf280000fU) | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xeb0f01dfU); // cmp x14, x15
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heapref&511)<<12 | 10<<5);
	a64put(b, 0xf10005dfU); // cmp x14, #1
	a64defer(b, 0x54000001, fallback);
	a64field(b, 14, 10, a->arrayroot, 8);
	a64put(b, 0xb10005dfU); // cmn x14, #1
	a64defer(b, 0x54000001, fallback);
	a64wordfield(b, 14, 10, a->arraylen, a->wordbytes);
	a64put(b, 0xd35ffdcfU); // lsr x15, x14, #31
	a64defer(b, 0xb500000f, fallback);
	a64put(b, 0xeb0e017fU); // cmp x11, x14
	a64defer(b, 0x54000008, fallback);
	a64field(b, 14, 10, a->arraytype, 8);
	a64defer(b, 0xb400000e, fallback);
	header = a->typesize > a->typenp ? a->typesize : a->typenp;
	if(header < a->typeref) header = a->typeref;
	a64put(b, 0xb10001dfU | (u32int)(header+4)<<10);
	a64defer(b, 0x54000002, fallback);
	a64wordfield(b, 15, 14, a->typeref, 4);
	a64put(b, 0xf10001ffU); // cmp x15,#0.
	a64defer(b, 0x5400000d, fallback); // Decline zero or negative descriptor references.
	a64wordfield(b, 15, 14, a->typenp, 4);
	a64defer(b, 0xb500000f, fallback);
	a64wordfield(b, 15, 14, a->typesize, 4);
	a64put(b, 0xf10005ffU); // cmp x15, #1
	a64defer(b, 0x54000001, fallback);
	a64field(b, 12, 10, a->arraydata, 8);
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xab0b019fU); // cmn x12, x11
	a64defer(b, 0x54000002, fallback);
	/* No fallible operations remain after the first payload write. */
	a64put(b, 0xeb09019fU); // cmp x12, x9
	forward = b->n; a64put(b, 0x54000009);
	a64put(b, 0xaa0b03edU); // mov x13, x11
	backward = b->n;
	a64put(b, 0xd10005adU); // sub x13, x13, #1
	a64put(b, 0x386d692eU); // ldrb w14, [x9, x13]
	a64put(b, 0x382d698eU); // strb w14, [x12, x13]
	a64put(b, 0xb500000dU | ((backward-b->n)&0x7ffff)<<5);
	jump = b->n; a64put(b, 0x14000000);
	if(b->n > LeafMax) return 0;
	b->code[forward] |= (b->n-forward)<<5;
	a64put(b, 0xd280000dU); // mov x13, #0
	loop = b->n;
	a64put(b, 0x386d692eU); // ldrb w14, [x9, x13]
	a64put(b, 0x382d698eU); // strb w14, [x12, x13]
	a64put(b, 0x910005adU); // add x13, x13, #1
	a64put(b, 0xeb0b01bfU); // cmp x13, x11
	a64put(b, 0x54000003U | ((loop-b->n)&0x7ffff)<<5);
	if(b->n > LeafMax) return 0;
	b->code[jump] |= b->n-jump;
	a64put(b, (a->wordbytes == 8 ? 0xf900014bU : 0xb900014bU) | (u32int)(a->arraylen/a->wordbytes)<<10); // Store captured byte count.
	a64put(b, 0xf900010aU); // str x10, [x8]
	return 1;
}




/* Capture current descriptor/owner state before bounded zeroing and owner publication. */
static int
a64recordobject(A64Buf *b, LeafLayout *a, int mode)
{
	int fallback, header, moduleend, empty, loop, half, imported;
	unsigned long long identity;

	imported = mode == LeafRecordImportedZero;
	if(!leafrecordlayout(a))
		return 0;
	header=a->typeref+4;
	if(header<a->typesize+4)header=a->typesize+4;
	if(header<a->typenp+4)header=a->typenp+4;
	if(header<a->typefree+8)header=a->typefree+8;
	if(header<a->typemark+8)header=a->typemark+8;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	if(imported){a64ptr(b, 12, a->s); a64load(b, 12, 12, 8);}
	else a64ptr(b, 12, a->module);
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xb100059fU); // cmn x12, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xb100019fU | (u32int)(a->linkmodule+8)<<10);
	a64defer(b, 0x54000002, fallback);
	a64field(b, 12, 12, a->linkmodule, 8);
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xb100059fU); // cmn x12, #1
	a64defer(b, 0x54000000, fallback);
	moduleend=a->moduletypes+8;
	if(moduleend<a->modulenty+4)moduleend=a->modulenty+4;
	a64put(b, 0xb100019fU | (u32int)moduleend<<10);
	a64defer(b, 0x54000002, fallback);
	a64wordfield(b, 13, 12, a->modulenty, 4);
	a64put(b, 0xf10001bfU); // cmp x13, #0
	a64defer(b, 0x5400000d, fallback);
	a64field(b, 12, 12, a->moduletypes, 8);
	a64ptr(b, 14, imported ? a->m : a->s); a64wordfield(b, 14, 14, 0, a->wordbytes);
	a64put(b, 0xeb0d01dfU); // cmp x14, x13
	a64defer(b, 0x54000002, fallback);
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xb100059fU); // cmn x12, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xab0e0d8cU); // adds x12, x12, x14, lsl #3
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xb100219fU); // cmn x12, #8
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xf9400189U); // ldr x9, [x12]
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xb100053fU); // cmn x9, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xf240093fU); // tst x9, #7
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xb100013fU | (u32int)header<<10);
	a64defer(b, 0x54000002, fallback);
	a64wordfield(b, 14, 9, a->typeref, 4);
	a64put(b, 0xf10001dfU); // cmp x14, #0
	a64defer(b, 0x5400000d, fallback);
	a64wordfield(b, 11, 9, a->typesize, 4);
	a64put(b, 0xf104017fU); // cmp x11, #256
	a64defer(b, 0x54000008, fallback);
	a64wordfield(b, 14, 9, a->typenp, 4);
	a64defer(b, 0xb500000e, fallback);
	a64field(b, 14, 9, a->typefree, 8);
	identity=(unsigned long long)a->plainfree;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd280000fU : 0xf280000fU) | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xeb0f01dfU); // cmp x14, x15
	a64defer(b, 0x54000001, fallback);
	a64field(b, 14, 9, a->typemark, 8);
	identity=(unsigned long long)a->plainmark;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd280000fU : 0xf280000fU) | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xeb0f01dfU); // cmp x14, x15
	a64defer(b, 0x54000001, fallback);
	a64ptr(b, 8, a->d); a64load(b, 10, 8, 8);
	a64defer(b, 0xb400000a, fallback);
	a64put(b, 0xb100055fU); // cmn x10, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xf100015fU | (u32int)a->heapcolor<<10);
	a64defer(b, 0x54000009, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heaptype&511)<<12 | 10<<5);
	a64put(b, 0xeb0901dfU); // cmp x14, x9
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heapref&511)<<12 | 10<<5);
	a64put(b, 0xf10005dfU); // cmp x14, #1
	a64defer(b, 0x54000001, fallback);
	empty=b->n; a64put(b, 0xb400000b);
	a64put(b, 0xab0b015fU); // cmn x10, x11
	a64defer(b, 0x54000002, fallback);
	if(mode != LeafRecordRaw){
		a64put(b, 0xaa0b03edU); // mov x13, x11
		/* No callback or validation can fail after the first payload store. */
		loop=b->n;
		a64put(b, 0xd10005adU); // sub x13, x13, #1
		a64put(b, 0x382d695fU); // strb wzr, [x10, x13]
		a64put(b, 0xb500000dU | ((loop-b->n)&0x7ffff)<<5);
	}
	if(b->n>LeafMax)return 0;
	b->code[empty] |= (b->n-empty)<<5;
	a64put(b, 0xf900010aU); // str x10, [x8]
	return 1;
}


/* Keep a private transaction, REG and LR in an aligned 80-byte AAPCS64 frame.
 * Pure preflight may decline; the accepted copy and ring/operand publication
 * perform no allocation, scheduler transition, GC callback or further C call.
 */
static int
a64transfer(A64Buf *b, LeafLayout *a, int receive)
{
	unsigned long long entry;
	int half;

	entry = (unsigned long long)a->channeltransferentry;
	if(entry == 0 || entry == ~0ULL)
		return 0;
	a64put(b, 0xd10143ffU); // sub sp, sp, #80
	a64put(b, 0xa9047be0U); // stp x0, x30, [sp, #64]
	a64put(b, 0x910003e2U); // mov x2, sp
	a64put(b, 0x52800001U | (u32int)receive<<5); // mov w1, direction.
	for(half = 0; half < 4; half++)
		a64put(b, (half == 0 ? 0xd2800010U : 0xf2800010U) | half<<21 | ((entry>>(half*16))&65535)<<5);
	a64put(b, 0xd63f0200U); // blr x16
	a64put(b, 0x350000a0U); // cbnz w0, #20
	a64put(b, 0xf94027feU); // ldr x30, [sp, #72]
	a64put(b, 0x910143ffU); // add sp, sp, #80
	a64put(b, 0x52800000U); // mov w0, #0
	a64put(b, 0xd65f03c0U); // ret
	a64put(b, 0xf94023e0U); // ldr x0, [sp, #64]
	a64put(b, 0xa9402fe9U); // ldp x9, x11, [sp]
	a64put(b, 0xf9400becU); // ldr x12, [sp, #16]
	if(!a64copykernel(b)) return 0;
	a64put(b, 0xf9400fe9U); // ldr x9, [sp, #24]
	a64put(b, 0xb9402beaU); // ldr w10, [sp, #40]
	a64put(b, 0xb900012aU); // str w10, [x9]
	a64put(b, 0xf94013e9U); // ldr x9, [sp, #32]
	a64put(b, 0xb94033eaU); // ldr w10, [sp, #48]
	a64put(b, 0xb900012aU); // str w10, [x9]
	a64put(b, 0xf94003eaU); // ldr x10, [sp, #0]
	a64put(b, 0xf900000aU | (u32int)(a->s/8)<<10); // Publish REG.s.
	a64put(b, 0xf94007eaU); // ldr x10, [sp, #8]
	a64put(b, 0xf900000aU | (u32int)(a->d/8)<<10); // Publish REG.d.
	a64put(b, 0xf9401feaU); // ldr x10, [sp, #56]
	a64put(b, 0xf900000aU | (u32int)(a->m/8)<<10); // Publish REG.m.
	a64put(b, 0xf94027feU); // ldr x30, [sp, #72]
	a64put(b, 0x910143ffU); // add sp, sp, #80
	return 1;
}


/* Preserve the empty destination and LR while the shared allocator creates channel resources. */
static int
a64channel(A64Buf *b, LeafLayout *a, int op)
{
	int fallback, unbuffered, half, raw, mapped;
	unsigned long long entry;

	raw = op == INEWCM;
	mapped = op == INEWCMP;
	entry = (unsigned long long)(raw ? a->rawchannelentry : mapped ? a->mappedchannelentry : a->channelentry);
	if((raw && (a->channelmid < 40 || a->channelmid > 2047 || a->channelmid%8)) ||
	   (mapped && (a->module < 0 || a->module > 2047 || a->module%8)))
		return 0;
	if(entry == 0 || entry == ~0ULL)
		return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 10, a->d);
	a64defer(b, 0xb400000a, fallback);
	a64put(b, 0xb100215fU); // cmn x10, #8
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xf940014cU); // ldr x12, [x10]
	a64put(b, 0xb100059fU); // cmn x12, #1
	a64defer(b, 0x54000001, fallback);
	if(raw || mapped){
		a64ptr(b, 9, a->s);
		a64defer(b, 0xb4000009, fallback);
		a64put(b, 0xb100013fU | (u32int)a->wordbytes<<10);
		a64defer(b, 0x54000002, fallback);
		a64wordfield(b, 4, 9, 0, a->wordbytes);
		if(raw){
			a64put(b, 0x12b0000cU); // mov w12, #INT_MAX
			a64put(b, 0xeb0c009fU); // cmp x4, x12
			a64defer(b, 0x54000008, fallback);
		}
	}
	a64ptr(b, 9, a->m);
	a64put(b, 0xd2800001U); // mov x1, #0
	a64put(b, 0xeb0a013fU); // cmp x9, x10
	unbuffered=b->n; a64put(b, 0x54000000);
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xb100013fU | (u32int)a->wordbytes<<10); // cmn x9, WORD bytes
	a64defer(b, 0x54000002, fallback);
	a64wordfield(b, 1, 9, 0, a->wordbytes);
	a64put(b, 0x12b0000cU); // mov w12, #0x7fffffff
	a64put(b, 0xeb0c003fU); // cmp x1, x12
	a64defer(b, 0x54000008, fallback);
	if(b->n>LeafMax)return 0;
	b->code[unbuffered] |= (b->n-unbuffered)<<5;
	a64put(b, raw || mapped ? 0xa9be7beaU : 0xa9bf7beaU); // Save output/LR in a 32- or 16-byte frame.
	if(raw || mapped){
		a64put(b, 0xf9000be4U); // str x4, [sp, #16]
		if(mapped){
			a64ptr(b, 3, a->module);
			a64put(b, 0xaa0103e2U); // mov x2, capacity
			a64put(b, 0xaa0403e1U); // mov x1, index
			a64put(b, 0xaa0303e0U); // mov x0, module link
		}else{
			a64put(b, 0xaa0403e0U); // mov x0, raw width
		}
	}else{
		a64put(b, 0x52800000U | (u32int)op<<5); // mov w0, scalar opcode
	}
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800010U : 0xf2800010U) | half<<21 | ((entry>>(half*16))&65535)<<5);
	a64put(b, 0xd63f0200U); // blr x16
	if(raw){
		a64put(b, 0xf9400be4U); // ldr x4, [sp, #16]
		a64put(b, (a->wordbytes == 8 ? 0xf9000004U : 0xb9000004U) |
			(u32int)(a->channelmid/a->wordbytes)<<10); // Store captured width before publishing.
	}
	a64put(b, raw || mapped ? 0xa8c27beaU : 0xa8c17beaU); // Restore output/LR and the complete native frame.
	a64put(b, 0xf9000140U); // str x0, [x10]
	return 1;
}


/* Save captured scalar bits and LR in an aligned frame across the runtime allocator. */
static int
a64scalarcons(A64Buf *b, LeafLayout *a, LeafOp *p)
{
	unsigned long long entry;
	int half;

	if(!leafscalarconslayout(a, p->bytes))
		return 0;
	a64ptr(b, 9, a->s); a64load(b, 9, 9, p->bytes);
	a64ptr(b, 1, a->d);
	a64put(b, 0xa9bf7be9U); // stp x9, x30, [sp, #-16]!
	a64put(b, 0xd2800000U | (u32int)p->input<<5); // mov x0, allocation width
	entry=(unsigned long long)a->consentry;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800010U : 0xf2800010U) | half<<21 | ((entry>>(half*16))&65535)<<5);
	a64put(b, 0xd63f0200U); // blr x16
	a64put(b, 0xa8c17be9U); // ldp x9, x30, [sp], #16
	a64put(b, 0x9100000bU | (u32int)a->listdata<<10); // add x11, x0, payload offset
	a64store(b, 9, p->bytes);
	return 1;
}




/* Keep descriptor validation side-effect free, then copy and publish a pointer-free mapped node. */
static int
a64mappedcons(A64Buf *b, LeafLayout *a)
{
	int fallback, framed, empty, loop, half;
	unsigned long long entry;

	if(!leafmappedconslayout(a))return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 9, a->s);
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xb104013fU); // cmn x9, #256
	a64defer(b, 0x54000002, fallback);
	a64ptr(b, 10, a->d);
	a64defer(b, 0xb400000a, fallback);
	a64put(b, 0xb100215fU);
	a64defer(b, 0x54000002, fallback);
	a64ptr(b, 11, a->m);
	a64defer(b, 0xb400000b, fallback);
	a64put(b, 0xb100017fU | (u32int)a->wordbytes<<10);
	a64defer(b, 0x54000002, fallback);
	a64wordfield(b, 1, 11, 0, a->wordbytes);
	a64put(b, 0xf940014cU); // ldr x12, old tail
	a64put(b, 0xd10103ffU); // sub sp, sp, #64
	a64put(b, 0xa9017beaU); // stp output, LR, [sp, #16]
	a64put(b, 0x14000005U);
	framed=b->n;
	a64put(b, 0xf9400ffeU);
	a64put(b, 0x910103ffU);
	a64put(b, 0x52800000U);
	a64put(b, 0xd65f03c0U);
	a64put(b, 0xa90027ecU); // stp old tail, source, [sp]
	a64ptr(b, 0, a->module);
	entry=(unsigned long long)a->constypeentry;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800010U : 0xf2800010U) | half<<21 | ((entry>>(half*16))&65535)<<5);
	a64put(b, 0xd63f0200U);
	a64put(b, 0xf90013e0U); // str Type, [sp, #32]
	a64wordfield(b, 12, 0, a->typenp, 4);
	a64defer(b, 0xb500000c, framed);
	a64wordfield(b, 2, 0, a->typesize, 4);
	a64put(b, 0xf104005fU);
	a64defer(b, 0x54000008, framed);
	a64put(b, 0xf90017e2U); // str size, [sp, #40]
	a64put(b, 0xaa0203e0U);
	a64put(b, 0x910003e1U);
	entry=(unsigned long long)a->consentry;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800010U : 0xf2800010U) | half<<21 | ((entry>>(half*16))&65535)<<5);
	a64put(b, 0xd63f0200U);
	a64put(b, 0xf94007e9U);
	a64put(b, 0xf94017e2U);
	a64put(b, 0x9100000bU | (u32int)a->listdata<<10);
	empty=b->n; a64put(b, 0xb4000002U);
	loop=b->n;
	a64put(b, 0x3840152cU);
	a64put(b, 0x3800156cU);
	a64put(b, 0xf1000442U);
	a64put(b, 0x54000001U | ((loop-b->n)&0x7ffff)<<5);
	if(b->n>LeafMax)return 0;
	b->code[empty] |= (b->n-empty)<<5;
	a64put(b, 0xf94013efU);
	a64field(b, 14, 15, a->typeref, 4);
	a64put(b, 0x110005ceU);
	a64put(b, 0xb90001eeU | (u32int)(a->typeref/4)<<10);
	a64put(b, 0xf900000fU | (u32int)(a->listtype/8)<<10);
	a64put(b, 0xf9400beaU);
	a64put(b, 0xf9400ffeU);
	a64put(b, 0xf9000140U);
	a64put(b, 0x910103ffU);
	return 1;
}


/* Preserve source, count, destination and LR while the allocator owns only a staged tail. */
static int
a64rawcons(A64Buf *b, LeafLayout *a)
{
	int fallback, empty, loop, half;
	unsigned long long identity;

	if(!leafscalarconslayout(a, 1))return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 2, a->m); a64wordfield(b, 2, 2, 0, a->wordbytes);
	a64put(b, 0xf104005fU); // cmp x2, #256
	a64defer(b, 0x54000008, fallback);
	a64ptr(b, 9, a->s); a64ptr(b, 10, a->d);
	a64defer(b, 0xb400000a, fallback);
	a64put(b, 0xb100055fU); // cmn x10, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xb100215fU); // cmn x10, #8
	a64defer(b, 0x54000002, fallback);
	empty=b->n; a64put(b, 0xb4000002);
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xb100053fU); // cmn x9, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xab02013fU); // cmn x9, x2
	a64defer(b, 0x54000002, fallback);
	if(b->n>LeafMax)return 0;
	b->code[empty] |= (b->n-empty)<<5;
	a64put(b, 0xf940014bU); // ldr x11, [x10]
	a64put(b, 0xa9bd27ebU); // stp x11, x9, [sp, #-48]!
	a64put(b, 0xa9010beaU); // stp x10, x2, [sp, #16]
	a64put(b, 0xf90013feU); // str x30, [sp, #32]
	a64put(b, 0xaa0203e0U); // mov x0, x2
	a64put(b, 0x910003e1U); // mov x1, sp
	identity=(unsigned long long)a->consentry;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800000U : 0xf2800000U) | 16 | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xd63f0200U); // blr x16
	a64put(b, 0xa940abe9U); // ldp x9, x10, [sp, #8]
	a64put(b, 0xf9400fe2U); // ldr x2, [sp, #24]
	a64put(b, 0xf94013feU); // ldr x30, [sp, #32]
	a64put(b, 0x9100000bU | (u32int)a->listdata<<10); // add x11, x0, payload offset
	empty=b->n; a64put(b, 0xb4000002);
	loop=b->n;
	a64put(b, 0x3840152cU); // ldrb w12, [x9], #1
	a64put(b, 0x3800156cU); // strb w12, [x11], #1
	a64put(b, 0xf1000442U); // subs x2, x2, #1
	a64put(b, 0x54000001U | ((loop-b->n)&0x7ffff)<<5);
	if(b->n>LeafMax)return 0;
	b->code[empty] |= (b->n-empty)<<5;
	a64put(b, 0xf9000140U); // str x0, [x10]
	a64put(b, 0x9100c3ffU); // add sp, sp, #48
	return 1;
}


/* Retain the captured pointer only after allocation and restore LR before returning. */
static int
a64pointercons(A64Buf *b, LeafLayout *a)
{
	int fallback, empty, marked, half;
	unsigned long long identity;

	if(!leafpointerconslayout(a))return 0;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 9, a->s); a64load(b, 9, 9, 8);
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xb100053fU); // cmn x9, #1
	empty=b->n; a64put(b, 0x54000000);
	a64put(b, 0xf100013fU | (u32int)a->heapcolor<<10);
	a64defer(b, 0x54000009, fallback);
	a64put(b, 0xf240093fU); // tst x9, #7
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf840012cU | ((u32int)-a->heapref&511)<<12); // ldur x12, [x9-ref]
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xb100059fU); // cmn x12, #1
	a64defer(b, 0x54000000, fallback);
	if(b->n>LeafMax)return 0;
	b->code[empty] |= (b->n-empty)<<5;
	identity=(unsigned long long)a->ptrtype;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800000U : 0xf2800000U) | 15 | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64wordfield(b, 13, 15, a->typeref, 4);
	a64put(b, 0xf10001bfU); // cmp x13, #0
	a64defer(b, 0x5400000d, fallback);
	a64put(b, 0x12b0000eU); // mov w14, #0x7fffffff
	a64put(b, 0x6b0e01bfU); // cmp w13, w14
	a64defer(b, 0x54000000, fallback);
	a64ptr(b, 1, a->d);
	a64put(b, 0xa9bf7be9U); // stp x9, x30, [sp, #-16]!
	a64put(b, 0xd2800100U); // mov x0, #8
	identity=(unsigned long long)a->consentry;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800000U : 0xf2800000U) | 16 | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xd63f0200U); // blr x16
	a64put(b, 0xa8c17be9U); // ldp x9, x30, [sp], #16
	a64put(b, 0xb100053fU); // cmn x9, #1
	empty=b->n; a64put(b, 0x54000000);
	a64put(b, 0xf840012cU | ((u32int)-a->heapref&511)<<12);
	a64put(b, 0x9100058cU); // add x12, x12, #1
	a64put(b, 0xf800012cU | ((u32int)-a->heapref&511)<<12);
	a64put(b, 0xb840012cU | ((u32int)-a->heapcolor&511)<<12);
	identity=(unsigned long long)a->mutator;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800000U : 0xf2800000U) | 13 | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xb94001adU); // ldr w13, [x13]
	a64put(b, 0x6b0d019fU); // cmp w12, w13
	marked=b->n; a64put(b, 0x54000000);
	a64put(b, 0x5280000cU | (u32int)a->markcolor<<5);
	a64put(b, 0xb800012cU | ((u32int)-a->heapcolor&511)<<12);
	identity=(unsigned long long)a->nprop;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800000U : 0xf2800000U) | 13 | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0x5280002cU); // mov w12, #1
	a64put(b, 0xb90001acU); // str w12, [x13]
	if(b->n>LeafMax)return 0;
	b->code[marked] |= (b->n-marked)<<5;
	b->code[empty] |= (b->n-empty)<<5;
	identity=(unsigned long long)a->ptrtype;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd2800000U : 0xf2800000U) | 15 | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64field(b, 14, 15, a->typeref, 4);
	a64put(b, 0x110005ceU); // add w14, w14, #1
	a64put(b, 0xb90001eeU | (u32int)(a->typeref/4)<<10); // str w14, [x15+Type.ref]
	a64put(b, 0xf900000fU | (u32int)(a->listtype/8)<<10); // str x15, [x0+List.t]
	a64put(b, 0xf9000009U | (u32int)(a->listdata/8)<<10); // str x9, [x0+payload]
	return 1;
}


/* Reuse unique owning pointer-free arrays without retaining or allocating. */
static int
a64arraycreate(A64Buf *b, LeafLayout *a, int clear)
{
	int fallback, header, moduleend, empty, loop, half, arrayend;
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
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 12, a->module);
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xb100059fU); // cmn x12, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xb100019fU | (u32int)(a->linkmodule+8)<<10);
	a64defer(b, 0x54000002, fallback);
	a64field(b, 12, 12, a->linkmodule, 8);
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xb100059fU); // cmn x12, #1
	a64defer(b, 0x54000000, fallback);
	moduleend=a->moduletypes+8;
	if(moduleend<a->modulenty+4)moduleend=a->modulenty+4;
	a64put(b, 0xb100019fU | (u32int)moduleend<<10);
	a64defer(b, 0x54000002, fallback);
	a64wordfield(b, 13, 12, a->modulenty, 4);
	a64put(b, 0xf10001bfU); // cmp x13, #0
	a64defer(b, 0x5400000d, fallback);
	a64field(b, 12, 12, a->moduletypes, 8);
	a64ptr(b, 14, a->m); a64wordfield(b, 14, 14, 0, a->wordbytes);
	a64put(b, 0xeb0d01dfU); // cmp x14, x13
	a64defer(b, 0x54000002, fallback);
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xb100059fU); // cmn x12, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xab0e0d8cU); // adds x12, x12, x14, lsl #3
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xb100219fU); // cmn x12, #8
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xf9400189U); // ldr x9, [x12]
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xb100053fU); // cmn x9, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xf240093fU); // tst x9, #7
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xb100013fU | (u32int)header<<10);
	a64defer(b, 0x54000002, fallback);
	a64wordfield(b, 14, 9, a->typeref, 4);
	a64put(b, 0xf10001dfU); // cmp x14, #0
	a64defer(b, 0x5400000d, fallback);
	a64wordfield(b, 11, 9, a->typesize, 4);
	a64put(b, 0xf104017fU); // cmp x11, #256
	a64defer(b, 0x54000008, fallback);
	a64wordfield(b, 14, 9, a->typenp, 4);
	a64defer(b, 0xb500000e, fallback);
	a64wordfield(b, 14, 9, a->typeref, 4);
	a64put(b, 0x12b0000dU); // mov w13, #0x7fffffff
	a64put(b, 0x6b0d01dfU); // cmp w14, w13
	a64defer(b, 0x54000000, fallback);
	identity=(unsigned long long)a->arrayidentity;
	for(half=0;half<4;half++)
		a64put(b, (half==0 ? 0xd280000fU : 0xf280000fU) | half<<21 | ((identity>>(half*16))&65535)<<5);
	a64put(b, 0xeb0f013fU); // cmp x9, x15
	a64defer(b, 0x54000000, fallback);
	a64wordfield(b, 14, 15, a->typeref, 4);
	a64put(b, 0xf10001dfU); // cmp x14, #0
	a64defer(b, 0x5400000d, fallback);
	a64put(b, 0x6b0d01dfU); // cmp w14, w13
	a64defer(b, 0x54000000, fallback);
	a64ptr(b, 12, a->s); a64wordfield(b, 12, 12, 0, a->wordbytes);
	a64put(b, 0xeb0d019fU); // cmp x12, x13
	a64defer(b, 0x54000008, fallback);
	a64put(b, 0x9b0c7d6bU); // mul x11, x11, x12
	a64put(b, 0xf104017fU); // cmp x11, #256
	a64defer(b, 0x54000008, fallback);
	a64ptr(b, 8, a->d); a64load(b, 10, 8, 8);
	a64defer(b, 0xb400000a, fallback);
	a64put(b, 0xb100055fU); // cmn x10, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xf100015fU | (u32int)a->heapcolor<<10);
	a64defer(b, 0x54000009, fallback);
	a64put(b, 0xf240095fU); // tst x10, #7
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xb100015fU | (u32int)arrayend<<10);
	a64defer(b, 0x54000002, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heaptype&511)<<12 | 10<<5);
	a64put(b, 0xeb0f01dfU); // cmp x14, x15
	a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf840000eU | ((u32int)-a->heapref&511)<<12 | 10<<5);
	a64put(b, 0xf10005dfU); // cmp x14, #1
	a64defer(b, 0x54000001, fallback);
	a64wordfield(b, 14, 10, a->arraylen, a->wordbytes);
	a64put(b, 0xeb0c01dfU); // cmp x14, x12
	a64defer(b, 0x54000001, fallback);
	a64field(b, 14, 10, a->arraytype, 8);
	a64put(b, 0xeb0901dfU); // cmp x14, x9
	a64defer(b, 0x54000001, fallback);
	a64field(b, 14, 10, a->arrayroot, 8);
	a64put(b, 0xb10005dfU); // cmn x14, #1
	a64defer(b, 0x54000001, fallback);
	empty=b->n; a64put(b, 0xb400000b);
	a64field(b, 14, 10, a->arraydata, 8);
	/* Empty arrays never inspect data; nonempty owning data must follow the header. */
	a64put(b, 0x9100014dU | (u32int)arrayend<<10); // add x13, x10, #arrayend
	a64put(b, 0xeb0d01dfU); // cmp x14, x13
	a64defer(b, 0x54000003, fallback);
	a64defer(b, 0xb400000e, fallback);
	a64put(b, 0xb10005dfU); // cmn x14, #1
	a64defer(b, 0x54000000, fallback);
	a64put(b, 0xab0b01dfU); // cmn x14, x11
	a64defer(b, 0x54000002, fallback);
	if(clear){
		a64put(b, 0xaa0b03edU); // mov x13, x11
		loop=b->n;
		a64put(b, 0xd10005adU); // sub x13, x13, #1
		a64put(b, 0x382d69dfU); // strb wzr, [x14, x13]
		a64put(b, 0xb500000dU | ((loop-b->n)&0x7ffff)<<5);
	}
	if(b->n>LeafMax)return 0;
	b->code[empty] |= (b->n-empty)<<5;
	a64put(b, 0xf900010aU); // str x10, [x8]
	return 1;
}


/* Tail-enter the shared C executor with the original caller's LR and stack.
 * x0 retains REG; w1 is the opcode and x2 the optional guarded fast entry.
 * BR, rather than BLR, allows the C handler to free this module's code mapping.
 */
static int
leafbridgeemit(u32int *out, int cap, void *executor, int op, void *fast)
{
	A64Buf b;
	unsigned long long entry, native;
	int half, i;

	entry = (unsigned long long)executor;
	native = (unsigned long long)fast;
	if(cap <= 0 || entry == 0 || entry == ~0ULL || native == ~0ULL ||
	   op <= INOP || op >= MAXDIS || op == IECLR)
		return 0;
	b.n = 0;
	a64put(&b, 0xd503245f); // BTI c.
	a64put(&b, 0x52800001U | (u32int)op<<5); // mov w1, opcode.
	for(half = 0; half < 4; half++)
		a64put(&b, (half == 0 ? 0xd2800002U : 0xf2800002U) | half<<21 | ((native>>(half*16))&65535)<<5);
	for(half = 0; half < 4; half++)
		a64put(&b, (half == 0 ? 0xd2800010U : 0xf2800010U) | half<<21 | ((entry>>(half*16))&65535)<<5);
	a64put(&b, 0xd61f0200U); // br x16: LR remains the original C caller's return address.
	if(b.n > cap || b.n > LeafMax)
		return 0;
	if(out != nil)
		for(i = 0; i < b.n; i++)
			out[i] = b.code[i];
	return b.n;
}


/* Emit bounded AAPCS64 handlers; allocating constructors stage scalar bits and LR on the stack. */
static int
leafemit(u32int *out, int cap, LeafLayout *abi, int op)
{
	A64Buf b;
	LeafOp p;
	int i, empty, other, done, scale, limit, loop, patch, equal, fallback;
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
	if(p.kind == LeafTransfer){
		if(!a64transfer(&b, abi, p.alu)) return 0;
	}else if(p.kind == LeafArraySlice){
		if(!a64arrayslice(&b, abi))return 0;
	}else if(p.kind == LeafModuleCall){
		if(!a64modulecall(&b, abi))return 0;
	}else if(p.kind == LeafReturn){
		if(!a64return(&b, abi))return 0;
	}else if(p.kind == LeafFrame){
		if(!a64frame(&b, abi, p.alu))return 0;
	}else if(p.kind == LeafFormatInteger){
		if(!a64formatinteger(&b, abi, &p)) return 0;
	}else if(p.kind == LeafStringSlice){
		if(!a64stringslice(&b, abi)) return 0;
	}else if(p.kind == LeafStringAppend){
		if(!a64stringappend(&b, abi)) return 0;
	}else if(p.kind == LeafStringStore){
		if(!a64stringstore(&b, abi)) return 0;
	}else if(p.kind == LeafDecimal){
		if(!a64decimal(&b, abi, &p)) return 0;
	}else if(p.kind == LeafSliceCopy){
		if(!a64slicecopy(&b, abi)) return 0;
	}else if(p.kind == LeafMappedCopy){
		if(!a64mapped(&b, abi, &p)) return 0;
	}else if(p.kind == LeafMappedCons){
		if(!a64mappedcons(&b, abi)) return 0;
	}else if(p.kind == LeafRawCons){
		if(!a64rawcons(&b, abi)) return 0;
	}else if(p.kind == LeafPointerCons){
		if(!a64pointercons(&b, abi)) return 0;
	}else if(p.kind == LeafChannel){
		if(!a64channel(&b, abi, p.alu)) return 0;
	}else if(p.kind == LeafScalarCons){
		if(!a64scalarcons(&b, abi, &p)) return 0;
	}else if(p.kind == LeafArrayCreate){
		if(!a64arraycreate(&b, abi, p.alu)) return 0;
	}else if(p.kind == LeafRecordObject){
		if(!a64recordobject(&b, abi, p.alu)) return 0;
	}else if(p.kind == LeafStringBytes){
		if(!a64stringbytes(&b, abi)) return 0;
	}else if(p.kind == LeafByteString){
		if(!a64bytestring(&b, abi)) return 0;
	}else if(p.kind == LeafOwner){
		if(!a64owner(&b, abi, &p)) return 0;
	}else if(p.kind == LeafCaseJump){
		if(!a64case(&b, abi, &p)) return 0;
	}else if(p.kind == LeafStringBranch){
		/* Mixed character widths share one bounded loop and a common fallback. */
		if(abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 ||
		   abi->stringmax < 0 || abi->stringmax > 2047 || abi->stringmax%4 ||
		   abi->stringlen == abi->stringmax || abi->stringdata < abi->stringlen+4 ||
		   abi->stringdata < abi->stringmax+4 || abi->stringdata > 2047) return 0;
		fallback=b.n+1;
		a64put(&b, 0x14000003); a64put(&b, 0x52800000); a64put(&b, 0xd65f03c0);
		/* Slot checks use x4 before it is initialized as the payload window. */
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, fallback);
		a64ptr(&b, 9, abi->m); a64pcspan(&b, 9, 8, fallback);
		a64put(&b, 0xd2c00004U | (u32int)(LeafStringWindow>>32)<<5); // MOVZ x4,window>>32,lsl #32.
		a64put(&b, 0xd2800085); // x5 supplies the Rune stride to both CSINC selections.
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, 8);
		/* Match stringcmp's nil error before either len/max read. */
		a64defer(&b, 0xb400000c, fallback); a64defer(&b, 0xb400000d, fallback);
		a64stringheader(&b, 12, 14, 1, abi->stringlen, abi->stringmax, abi->stringdata, fallback);
		a64stringheader(&b, 13, 15, 2, abi->stringlen, abi->stringmax, abi->stringdata, fallback);
		a64put(&b, 0xeb0d019f);
		equal=b.n; a64put(&b, 0x54000000);
		loop=b.n;
		empty=b.n; a64put(&b, 0xb400000e);
		other=b.n; a64put(&b, 0xb400000f);
		a64put(&b, 0x39400189);
		a64put(&b, 0x36100041); /* tbz w1,#2,skip wide load */
		a64put(&b, 0xb9800189);
		a64put(&b, 0x394001aa);
		a64put(&b, 0x36100042);
		a64put(&b, 0xb98001aa);
		a64put(&b, 0xeb0a013f);
		done=b.n; a64put(&b, 0x54000001);
		a64put(&b, 0x8b01018c); a64put(&b, 0x8b0201ad);
		a64put(&b, 0xd10005ce); a64put(&b, 0xd10005ef);
		a64put(&b, 0x14000000U | ((loop-b.n)&0x3ffffff));
		b.code[empty] |= (b.n-empty)<<5; b.code[other] |= (b.n-other)<<5;
		b.code[equal] |= (b.n-equal)<<5;
		a64put(&b, 0xeb0f01df);
		b.code[done] |= (b.n-done)<<5;
		patch=b.n; a64put(&b, 0x54000000U | inverse[p.condition]);
		/* Preserve false-target nonaccess while bounding a taken target load. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, 8, fallback); a64load(&b, 12, 11, 8);
		a64put(&b, 0xf9000000U | (abi->pc/8)<<10 | 12);
		b.code[patch] |= (b.n-patch)<<5;
	}else if(p.kind == LeafTypeCheck){
		/* The header displacement is private and independent of the Dis WORD. */
		if(abi->heaptype <= 0 || abi->heaptype > 255 || abi->heaptype%8) return 0;
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1);
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, 8, -1);
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64ptr(&b, 10, abi->d); a64load(&b, 13, 10, 8);
		a64put(&b, 0xb100059f);
		empty=b.n; a64put(&b, 0x54000000);
        /* Reject wrapped or nil Type-field addresses before either load. */
        a64put(&b, 0xf100019fU | (u32int)abi->heaptype<<10); /* cmp x12, distance */
        a64guard(&b, 0x54000068); /* b.hi over decline return */
        a64put(&b, 0xf10001bfU | (u32int)abi->heaptype<<10); /* cmp x13, distance */
        a64guard(&b, 0x54000068);
		a64put(&b, 0xb10005bf); a64guard(&b, 0x54000061);
		a64put(&b, 0xf8400000U | ((u32int)-abi->heaptype&511)<<12 | 12<<5 | 14);
		a64put(&b, 0xf8400000U | ((u32int)-abi->heaptype&511)<<12 | 13<<5 | 15);
		a64put(&b, 0xeb0f01df); a64guard(&b, 0x54000060);
		b.code[empty] |= (b.n-empty)<<5;
	}else if(p.kind == LeafProgramAddress || p.kind == LeafTableJump){
		fallback = -1;
		if(p.kind == LeafTableJump){
			fallback = b.n+1;
			a64put(&b, 0x14000003); a64put(&b, 0x52800000); a64put(&b, 0xd65f03c0);
		}
		/* All metadata offsets are supplied by the compiler's native layout adapter. */
		if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
		   abi->linkmodule < 0 || abi->linkmodule > 2047 || abi->linkmodule%8 ||
		   abi->moduleprog < 0 || abi->moduleprog > 2047 || abi->moduleprog%8 ||
		   abi->modulenprog < 0 || abi->modulenprog > 2047 || abi->modulenprog%4 ||
		   abi->instbytes < 1 || abi->instbytes > 2047) return 0;
		a64ptr(&b, 12, abi->module); a64pcspan(&b, 12, abi->linkmodule+8, fallback);
		if(p.kind == LeafTableJump){
			if(abi->linkcompiled < 0 || abi->linkcompiled > 2047 || abi->linkcompiled%4) return 0;
			a64pcspan(&b, 12, abi->linkcompiled+4, fallback);
			a64field(&b, 14, 12, abi->linkcompiled, 4); a64pcguard(&b, 0x3400006e, fallback);
		}
		a64field(&b, 12, 12, abi->linkmodule, 8);
		a64pcspan(&b, 12,
		abi->moduleprog+8 > abi->modulenprog+4 ? abi->moduleprog+8 : abi->modulenprog+4, fallback);
		a64put(&b, 0xb9800000U | (abi->modulenprog/4)<<10 | 12<<5 | 13);
		a64put(&b, 0xf10001bf); a64pcguard(&b, 0x5400006a, fallback);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, abi->wordbytes, fallback);
		a64load(&b, 14, 9, abi->wordbytes);
		if(p.kind == LeafTableJump){
			/* Native tables retain four-byte entries and the preceding compiler count. */
			if(abi->wordbytes == 4)a64put(&b, 0x93407dce);
			a64put(&b, 0xf10001df); a64pcguard(&b, 0x5400006a, fallback);
			a64ptr(&b, 9, abi->d); a64pcspan(&b, 9, IBY2WD, fallback);
			a64put(&b, 0xf100013fU | IBY2WD<<10); a64pcguard(&b, 0x54000062, fallback);
			a64put(&b, 0xb100053f); a64pcguard(&b, 0x54000061, fallback);
			a64put(&b, 0xd100012aU | IBY2WD<<10); a64put(&b, 0xb980014f);
			a64put(&b, 0xf10001ff); a64pcguard(&b, 0x5400006c, fallback);
			a64put(&b, 0xeb0f01df); a64pcguard(&b, 0x54000063, fallback);
			a64put(&b, 0xd3400000U | (IBY2WD == 8 ? 61 : 62)<<16 | (IBY2WD == 8 ? 60 : 61)<<10 | 14<<5 | 14);
			a64put(&b, 0xab0e012e); a64pcguard(&b, 0x54000063, fallback);
			a64put(&b, 0xb10001dfU | (IBY2WD-1)<<10); a64pcguard(&b, 0x54000063, fallback);
			a64put(&b, 0xb98001ce);
		}
		a64put(&b, 0xeb0d01df); a64pcguard(&b, 0x54000063, fallback);
		a64put(&b, 0xd280000dU | abi->instbytes<<5); a64put(&b, 0x9b0d7dce);
		a64field(&b, 15, 12, abi->moduleprog, 8); a64pcguard(&b, 0xb500006f, fallback);
		a64put(&b, 0xb10005ff); a64pcguard(&b, 0x54000061, fallback);
		a64put(&b, 0xab0e01ee); a64pcguard(&b, 0x54000063, fallback);
		if(p.kind == LeafTableJump)a64put(&b, 0xf9000000U | (u32int)(abi->pc/8)<<10 | 14);
		else{
			a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, 8, fallback); a64store(&b, 14, 8);
		}
	}else if(p.kind == LeafFixedCoefficient){
		/* Coefficient slots follow the VM frame layout, not native pointer alignment. */
		if(abi->fp < 0 || abi->fp > 2047 || abi->fp%8 || abi->fixedtmp < 0 || abi->fixedtmp > 2047 ||
		   abi->fixedcoef < 0 || abi->fixedcoef > 2047) return 0;
		/* All decoded operands must fit before frame or scalar metadata is read. */
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, abi->wordbytes, -1);
		a64ptr(&b, 10, abi->m); a64pcspan(&b, 10, abi->wordbytes, -1);
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, abi->wordbytes, -1);
		fallback=b.n+1;
		a64put(&b, 0x14000003); a64put(&b, 0x52800000); a64put(&b, 0xd65f03c0);
		a64ptr(&b, 9, abi->fp);
		a64pcspan(&b, 9, (p.alu == 2 || abi->fixedcoef > abi->fixedtmp ? abi->fixedcoef : abi->fixedtmp)+abi->wordbytes, fallback);
		/* Form byte offsets explicitly, including four-byte-aligned WORD64 slots. */
		a64put(&b, 0x91000000U | abi->fixedcoef<<10 | 9<<5 | 10); a64load(&b, 14, 10, abi->wordbytes);
		if(abi->wordbytes == 4)a64put(&b, 0x93407dce);
		if(p.alu != 1){a64put(&b, 0xaa0e03ef); a64defer(&b, 0xb400000f, fallback);}
		if(p.alu == 2)a64ptr(&b, 10, abi->m);
		else a64put(&b, 0x91000000U | abi->fixedtmp<<10 | 9<<5 | 10);
		a64load(&b, 13, 10, abi->wordbytes); if(abi->wordbytes == 4)a64put(&b, 0x93407dad);
		if(p.condition){
			/* Decode both rounding flags and the signed scale. */
			a64put(&b, 0x924005a1); a64put(&b, 0x9342fdad);
		}
		a64put(&b, 0x9100fdaa); a64put(&b, 0xf101fd5f); a64defer(&b, 0x54000002, fallback);
		if(p.alu == 1){
			a64ptr(&b, 10, abi->s); a64load(&b, 15, 10, abi->wordbytes);
			if(abi->wordbytes == 4)a64put(&b, 0x93407def);
			a64defer(&b, 0xb400000f, fallback);
		}else if(p.alu == 0){a64ptr(&b, 10, abi->s); a64load(&b, 14, 10, abi->wordbytes);if(abi->wordbytes == 4)a64put(&b, 0x93407dce);}
		a64ptr(&b, 10, p.alu == 2 ? abi->s : abi->m); a64load(&b, 12, 10, abi->wordbytes);
		if(abi->wordbytes == 4)a64put(&b, 0x93407d8c);
		if(p.condition)
			a64put(&b, p.alu == 0 ? 0xca0e0182 : 0xaa0c03e2);
		if(p.condition)a64fixedround(&b, p.alu, abi->wordbytes);
		if(p.alu != 2)a64put(&b, 0x9b0e7d8c);
		if(p.condition){
			/* Add the modular mask using the original operand signs. */
			done=b.n; a64put(&b, 0x36000001);
			/* Preserve invalid-mask errors, but allow left shifts needing no mask. */
			a64put(&b, 0xf100005f);
			other=b.n; a64put(&b, p.alu == 1 ? 0x5400000c : 0x5400000a);
			a64put(&b, 0xf10001bf); a64defer(&b, 0x5400000a, fallback);
			a64put(&b, 0x92800003); a64put(&b, 0xcb0d03e4);
			a64put(&b, 0x9ac42063); a64put(&b, 0xaa2303e3);
			a64put(&b, 0x8b03018c);
			b.code[done] |= (b.n-done)<<5; b.code[other] |= (b.n-other)<<5;
		}
		a64put(&b, 0xf10001bf); a64put(&b, 0x5400006b); a64put(&b, 0x9acd218c); a64put(&b, 0x14000003);
		a64put(&b, 0xcb0d03ed); a64put(&b, 0x9acd298c);
		if(p.condition && p.alu != 1)a64put(&b, 0x8b05018c);
		a64put(&b, 0x9acf0d8c);
		if(p.condition && p.alu == 1)a64put(&b, 0x8b05018c);
		a64ptr(&b, 11, abi->d); a64store(&b, 12, abi->wordbytes);
	}else if(p.kind == LeafCall){
		/* The target may reside in either frame link about to be replaced. */
		if(abi->fp < 0 || abi->fp > 2047 || abi->fp%8 ||
		   abi->framelr < 0 || abi->framelr > 2047 || abi->framelr%8 ||
		   abi->framefp < 0 || abi->framefp > 2047 || abi->framefp%8 ||
		   abi->framelr == abi->framefp) return 0;
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1); a64load(&b, 12, 9, 8); a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
		a64pcspan(&b, 12, (abi->framelr>abi->framefp ? abi->framelr : abi->framefp)+8, -1);
		a64ptr(&b, 9, abi->d); a64pcspan(&b, 9, 8, -1); a64load(&b, 13, 9, 8);
		a64ptr(&b, 14, abi->pc); a64ptr(&b, 15, abi->fp);
		a64put(&b, 0xf9000000U | (abi->framelr/8)<<10 | 12<<5 | 14);
		a64put(&b, 0xf9000000U | (abi->framefp/8)<<10 | 12<<5 | 15);
		a64put(&b, 0xf9000000U | (abi->fp/8)<<10 | 12);
		a64put(&b, 0xf9000000U | (abi->pc/8)<<10 | 13);
	}else if(p.kind == LeafDescriptorIndex){
		/* Descriptor pointers and signed sizes are validated before forming an address. */
		if(abi->arraylen < 0 || abi->arraylen > 2047 || abi->arraydata < 0 || abi->arraydata > 2047 ||
		   abi->arraytype < 0 || abi->arraytype > 2047 || abi->typesize < 0 || abi->typesize > 2047 ||
		   (abi->arraylen|abi->arraydata|abi->arraytype)%8 || abi->typesize%4) return 0;
		/* Bound all decoded slots before reading owners, indices or descriptors. */
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1);
		a64ptr(&b, 10, abi->d); a64pcspan(&b, 10, abi->wordbytes, -1);
		a64ptr(&b, 11, abi->m); a64pcspan(&b, 11, abi->wordbytes, -1);
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8); a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
		/* A negative index must reject before any array header is inspected. */
		a64ptr(&b, 9, abi->d); a64load(&b, 14, 9, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407dce);
		a64put(&b, 0xf10001df); a64guard(&b, 0x5400006a);
		a64pcspan(&b, 12, abi->arraylen+abi->wordbytes, -1);
		a64wordfield(&b, 13, 12, abi->arraylen, abi->wordbytes);
		a64put(&b, 0xf10001bf); a64guard(&b, 0x5400006a);
		/* x14 retains the checked full-width index. */
		a64put(&b, 0xeb0d01df); a64guard(&b, 0x54000063);
		a64pcspan(&b, 12, abi->arraytype+8, -1);
		a64field(&b, 15, 12, abi->arraytype, 8); a64guard(&b, 0xb500006f);
		a64put(&b, 0xb10005ff); a64guard(&b, 0x54000061);
		a64pcspan(&b, 15, abi->typesize+4, -1);
		a64put(&b, 0xb9800000U | (abi->typesize/4)<<10 | 15<<5 | 15); /* ldrsw stride */
		a64put(&b, 0xf10001ff); a64guard(&b, 0x5400006a);
		a64pcspan(&b, 12, abi->arraydata+8, -1);
		a64field(&b, 13, 12, abi->arraydata, 8);
		empty=b.n; a64put(&b, 0xb400000f);
		a64put(&b, 0xd29fffec); a64put(&b, 0xf2afffec); /* x12=INT_MAX */
		a64put(&b, 0xeb0c01df); a64guard(&b, 0x54000069);
		a64put(&b, 0x9b0f7dce); a64put(&b, 0xeb0c01df); a64guard(&b, 0x54000069);
		a64guard(&b, 0xb500006d); a64put(&b, 0xb10005bf); a64guard(&b, 0x54000061);
		a64put(&b, 0xab0e01ae); a64guard(&b, 0x54000063);
		done=b.n; a64put(&b, 0x14000000);
		b.code[empty] |= (b.n-empty)<<5; a64put(&b, 0xaa0d03ee);
		b.code[done] |= b.n-done;
		a64ptr(&b, 11, abi->m); a64store(&b, 14, abi->wordbytes);
	}else if(p.kind == LeafFixedFrame){
		/* Frame temporary slots may have only four-byte alignment, even in WORD64. */
		if(abi->fp < 0 || abi->fp > 2047 || abi->fp%8 || abi->fixedtmp < 0 || abi->fixedtmp > 2047) return 0;
		a64ptr(&b, 10, abi->fp);
		a64pcspan(&b, 10, abi->fixedtmp+abi->wordbytes, -1);
		a64put(&b, 0x91000000U | abi->fixedtmp<<10 | 10<<5 | 10);
		a64load(&b, 13, 10, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407dad);
		a64put(&b, 0x9100fdae); a64put(&b, 0xf101fddf); a64guard(&b, 0x54000063);
		if(!p.alu){
			/* Multiplication bounds both inputs and the result before either load. */
			a64ptr(&b, 9, abi->m); a64pcspan(&b, 9, abi->wordbytes, -1);
			a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, abi->wordbytes, -1);
		}
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, abi->wordbytes, -1);
		a64ptr(&b, 9, abi->s); a64load(&b, 15, 9, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407def);
		if(p.alu){ a64guard(&b, 0xb500006f); }
		a64ptr(&b, 9, abi->m); a64pcspan(&b, 9, abi->wordbytes, -1);
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, abi->wordbytes, -1);
		a64ptr(&b, 9, abi->m); a64load(&b, 12, 9, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407d8c);
		if(!p.alu) a64put(&b, 0x9b0f7d8c);
		a64put(&b, 0xf10001bf); a64put(&b, 0x5400006b);
		a64put(&b, 0x9acd218c); a64put(&b, 0x14000003);
		a64put(&b, 0xcb0d03ed); a64put(&b, 0x9acd298c);
		if(p.alu) a64put(&b, 0x9acf0d8c);
		a64ptr(&b, 11, abi->d); a64store(&b, 12, abi->wordbytes);
	}else if(p.kind == LeafRealResize){
		/* Preflight the complete result and source before loads or FP effects. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, p.bytes, -1);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, p.input, -1);
		/* FPCR/FPSR supply the same conversion environment as native C. */
		a64ptr(&b, 9, abi->s);
		a64put(&b, p.input == 4 ? 0xbd400120 : 0xfd400120);
		a64put(&b, p.input == 4 ? 0x1e22c000 : 0x1e624000);
		a64ptr(&b, 11, abi->d);
		a64put(&b, p.bytes == 4 ? 0xbd000160 : 0xfd000160);
	}else if(p.kind == LeafIntegerReal){
		/* Preflight the complete result and source before loads or FP effects. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, p.bytes, -1);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, p.input, -1);
		/* SCVTF rounds every signed input with the active C FP environment. */
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, p.input);
		if(p.input == 4)a64put(&b, 0x93407d8c);
		a64put(&b, 0x9e620180); /* scvtf d0,x12 */
		a64ptr(&b, 11, abi->d); a64put(&b, 0xfd000160);
	}else if(p.kind == LeafFixedShift){
		/* Validate the signed interval before fetching a possibly inaccessible source. */
		a64ptr(&b, 10, abi->m); a64pcspan(&b, 10, abi->wordbytes, -1);
		a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407dad); /* sxtw x13,w13 */
		a64put(&b, 0x9100fdae); /* add x14,x13,#63 */
		a64put(&b, 0xf101fddf); /* cmp x14,#127 */
		a64guard(&b, 0x54000063);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, abi->wordbytes, -1);
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, abi->wordbytes, -1);
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407d8c);
		a64put(&b, 0xf10001bf); a64put(&b, 0x5400006b); /* cmp count,0; b.lt negative */
		a64put(&b, 0x9acd218c); a64put(&b, 0x14000003); /* lsl; b done */
		a64put(&b, 0xcb0d03ed); a64put(&b, 0x9acd298c); /* neg count; asr */
		a64ptr(&b, 11, abi->d); a64store(&b, 12, abi->wordbytes);
	}else if(p.kind == LeafPower){
		/* Check both source spans before the exponent or base is read. */
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, abi->wordbytes, -1);
		a64ptr(&b, 10, abi->m); a64pcspan(&b, 10, p.bytes, -1);
		/* Minimum negative exponents are handled by parity without signed negation. */
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, abi->wordbytes);
		a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, p.bytes);
		a64put(&b, abi->wordbytes == 8 ? 0xf100019fU : 0x7100019fU);
		scale=b.n; a64put(&b, 0x5400000a);
		a64guard(&b, 0xb500006d);
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, p.bytes, -1);
		a64put(&b, 0xd280002e);
		a64put(&b, p.bytes == 8 ? 0xf10005bf : 0x710005bf);
		empty=b.n; a64put(&b, 0x54000000);
		a64put(&b, 0xd280000e);
		a64put(&b, p.bytes == 8 ? 0xb10005bf : 0x310005bf);
		other=b.n; a64put(&b, 0x54000001);
		a64put(&b, 0x9240018e); a64put(&b, 0xcb0e03ee);
		a64put(&b, 0x8b0e01ce); a64put(&b, 0x910005ce);
		b.code[empty] |= (b.n-empty)<<5; b.code[other] |= (b.n-other)<<5;
		a64ptr(&b, 11, abi->d); a64store(&b, 14, p.bytes);
		a64put(&b, 0x52800020); a64put(&b, 0xd65f03c0);
		b.code[scale] |= (b.n-scale)<<5;
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, p.bytes, -1);
		a64put(&b, 0x5280002e); /* result = 1 */
		loop = b.n;
		a64put(&b, 0x3600004c); a64put(&b, 0x9b0d7dce);
		a64put(&b, 0xd341fd8c);
		done = b.n; a64put(&b, 0xb400000c);
		a64put(&b, 0x9b0d7dad);
		a64put(&b, 0x14000000U | ((u32int)(loop-b.n)&0x3ffffff));
		b.code[done] |= (b.n-done)<<5;
		a64ptr(&b, 11, abi->d); a64store(&b, 14, p.bytes);
	}else if(p.kind == LeafStringIndex){
		if(!a64stringindex(&b, abi)) return 0;
	}else if(p.kind == LeafListLength){
		/* Bounded Brent scans preserve all state until a valid terminal owner is reached. */
		if(abi->listtail < 0 || abi->listtail > 32760 || abi->listtail%8 != 0) return 0;
		/* Validate decoded result/source slots before any list access. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, abi->wordbytes, -1);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1);
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64put(&b, 0xaa0c03ef); // x15 is the initial checkpoint.
		a64put(&b, 0x5280000d);
		loop = b.n;
		a64put(&b, 0xb100059f);
		done = b.n; a64put(&b, 0x54000000);
		a64guard(&b, 0xb500006c);
		a64put(&b, 0x710001bfU | LeafListLimit<<10); a64guard(&b, 0x54000063); // Count remains below the native scan budget.
		a64readspan(&b, abi->listtail, 8);
		a64field(&b, 12, 12, abi->listtail, 8);
		a64put(&b, 0x110005ad);
		a64put(&b, 0xeb0f019f); a64guard(&b, 0x54000061); // Reject a repeated checkpoint.
		a64put(&b, 0x110005ae); a64put(&b, 0x6a0d01df); // (count+1) & count.
		a64put(&b, 0x54000041); a64put(&b, 0xaa0c03ef);
		a64put(&b, 0x14000000U | ((u32int)(loop-b.n)&0x3ffffff));
		b.code[done] |= (b.n-done)<<5;
		a64ptr(&b, 11, abi->d); a64store(&b, 13, abi->wordbytes);
	}else if(p.kind == LeafHead){
		if(abi->listdata < 0 || abi->listdata > 4095 || abi->listdata%8 != 0) return 0;
		/* Validate decoded result/source slots before any list access. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, p.bytes, -1);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1);
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
		a64readspan(&b, abi->listdata, p.bytes);
		a64field(&b, 12, 12, abi->listdata, p.bytes);
		a64ptr(&b, 11, abi->d); a64store(&b, 12, p.bytes);
	}else if(p.kind == LeafStringLength){
		if(abi->stringlen < 0 || abi->stringlen > 16380 || abi->stringlen%4 != 0) return 0;
		/* Length queries must not read through malformed decoded operand slots. */
		a64ptr(&b, 10, abi->d); a64pcspan(&b, 10, abi->wordbytes, -1);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1);
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); /* cmn x12, #1 */
		empty = b.n; a64put(&b, 0x54000000);
		a64readspan(&b, abi->stringlen, 4);
		a64field(&b, 12, 12, abi->stringlen, 4);
		a64put(&b, 0x52b0000d); /* mov w13, #0x80000000 */
		a64put(&b, 0x6b0d019f); a64guard(&b, 0x54000061);
		a64put(&b, 0x36f8004c); /* tbz w12, #31, positive */
		a64put(&b, 0x4b0c03ec); /* neg w12, w12 */
		done = b.n; a64put(&b, 0x14000000);
		b.code[empty] |= (b.n-empty)<<5; a64put(&b, 0x5280000c);
		b.code[done] |= b.n-done;
		a64ptr(&b, 11, abi->d); a64store(&b, 12, abi->wordbytes);
	}else if(p.kind == LeafCopy || p.kind == LeafHeadCopy){
		if(p.kind == LeafHeadCopy){
			if(abi->listdata < 0 || abi->listdata > 4095) return 0;
			a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1);
			a64ptr(&b, 9, abi->s); a64load(&b, 15, 9, 8);
			a64guard(&b, 0xb500006f);
			a64put(&b, 0xb10005ff); a64guard(&b, 0x54000061);
		}
		a64ptr(&b, 9, abi->m); a64pcspan(&b, 9, abi->wordbytes, -1);
		/* Validate the owner first for headm, then capture the complete count. */
		a64ptr(&b, 10, abi->m); a64load(&b, 12, 10, abi->wordbytes);
		a64put(&b, (abi->wordbytes == 8 ? 0xf100019fU : 0x7100019fU));
		a64guard(&b, 0x5400006a); /* b.ge valid */
		empty = b.n; a64put(&b, 0xb400000c);
		a64put(&b, 0xf104019f); /* cmp x12, #256 */
		a64guard(&b, 0x54000069); /* b.ls small */
		if(p.kind == LeafHeadCopy){
			a64put(&b, 0xb10001e9U | (u32int)abi->listdata<<10); // adds x9,x15,data.
			a64guard(&b, 0x54000063); // The payload offset must not carry.
		}else
			a64ptr(&b, 9, abi->s);
		a64ptr(&b, 11, abi->d);
		/* Empty copies skipped these pointer checks; nonempty spans must not wrap. */
		a64guard(&b, 0xb5000069); a64guard(&b, 0xb500006b);
		a64put(&b, 0xab0c0122); a64guard(&b, 0x54000063);
		a64put(&b, 0xab0c0162); a64guard(&b, 0x54000063);
		if(!a64copykernel(&b)) return 0;
		if(b.n > LeafMax) return 0;
		b.code[empty] |= (b.n-empty)<<5;
	}else if(p.kind == LeafLength || p.kind == LeafIndex){
		if(abi->arraylen < 0 || abi->arraydata < 0 || abi->arraylen > 16376 ||
		   abi->arraydata > 16376 || (abi->arraylen|abi->arraydata)%8 != 0) return 0;
		/* Only length queries ignore m; address commands return through m. */
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1);
		a64ptr(&b, 10, abi->d); a64pcspan(&b, 10, abi->wordbytes, -1);
		if(p.kind == LeafIndex){
			a64ptr(&b, 11, abi->m); a64pcspan(&b, 11, abi->wordbytes, -1);
		}
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		if(p.kind == LeafLength){
			empty = b.n; a64put(&b, 0xb400000c);
			a64put(&b, 0xb100059f); /* cmn x12, #1 */
			other = b.n; a64put(&b, 0x54000000);
			a64pcspan(&b, 12, abi->arraylen+abi->wordbytes, -1);
			a64field(&b, 12, 12, abi->arraylen, abi->wordbytes);
			done = b.n; a64put(&b, 0x14000000);
			b.code[empty] |= (b.n-empty)<<5; b.code[other] |= (b.n-other)<<5;
			a64put(&b, 0x5280000c);
			b.code[done] |= b.n-done;
			a64ptr(&b, 11, abi->d); a64store(&b, 12, abi->wordbytes);
		}else{
			a64guard(&b, 0xb500006c);
			a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
			/* A negative index must reject before any array header is inspected. */
			a64ptr(&b, 9, abi->d); a64load(&b, 14, 9, abi->wordbytes);
			if(abi->wordbytes == 4) a64put(&b, 0x93407dce);
			a64put(&b, 0xf10001df); a64guard(&b, 0x5400006a);
			a64pcspan(&b, 12, abi->arraylen+abi->wordbytes, -1);
			a64wordfield(&b, 13, 12, abi->arraylen, abi->wordbytes);
			a64put(&b, 0xf10001bf); a64guard(&b, 0x5400006a); /* nonnegative length */
			/* x14 retains the checked full-width index. */
			a64put(&b, 0xeb0d01df); a64guard(&b, 0x54000063); /* index < length */
			scale = p.bytes == 1 ? 0 : p.bytes == 4 ? 2 : 3; limit = 0x7fffffff>>scale;
			a64put(&b, 0xd280000fU | (limit&0xffff)<<5);
			a64put(&b, 0xf2a0000fU | (limit>>16)<<5);
			a64put(&b, 0xeb0f01df); a64guard(&b, 0x54000069); /* bounded byte offset */
			a64pcspan(&b, 12, abi->arraydata+8, -1);
			a64field(&b, 13, 12, abi->arraydata, 8);
			a64guard(&b, 0xb500006d);
			a64put(&b, 0xb10005bf); a64guard(&b, 0x54000061);
			a64put(&b, 0xab000000U | 14<<16 | scale<<10 | 13<<5 | 14);
			a64guard(&b, 0x54000063); /* no native address carry */
			a64ptr(&b, 11, abi->m); a64store(&b, 14, abi->wordbytes);
		}
	}else if(p.kind == LeafRealPower){
		/* Preflight complete slots before loads, conversions or FP effects. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, 8, -1);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, abi->wordbytes, -1);
		a64ptr(&b, 10, abi->m); a64pcspan(&b, 10, 8, -1);
		/* Match each C multiplication and handle minimum WORD as unsigned bits. */
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, abi->wordbytes);
		if(abi->wordbytes == 4)a64put(&b, 0x93407d8c);
		a64ptr(&b, 10, abi->m); a64put(&b, 0xfd400141);
		a64put(&b, 0xd2e0000dU | 0x3ff0<<5); a64put(&b, 0x9e6701a0);
		done=b.n; a64put(&b, 0xb6f8000c); /* tbz exponent,#63,positive */
		a64put(&b, 0xcb0c03ec); /* neg x12,x12 */
		a64put(&b, 0x1e611801); /* fdiv d1,d0,d1 */
		b.code[done] |= (b.n-done)<<5;
		loop=b.n;
		a64put(&b, 0x3600004c); /* tbz x12,#0,skip multiply */
		a64put(&b, 0x1e610800); /* fmul d0,d0,d1 */
		a64put(&b, 0xd341fd8c); /* lsr x12,x12,#1 */
		done=b.n; a64put(&b, 0xb400000c);
		a64put(&b, 0x1e610821); /* fmul d1,d1,d1 */
		a64put(&b, 0x14000000U | ((u32int)(loop-b.n)&0x3ffffff));
		b.code[done] |= (b.n-done)<<5;
		a64ptr(&b, 11, abi->d); a64put(&b, 0xfd000160);
	}else if(p.kind == LeafRealInteger){
		/* Validate the scaled and ordinary variants before parsing raw FP bits. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, p.bytes, -1);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, 8, -1);
		if(p.alu){
			a64ptr(&b, 10, abi->m); a64pcspan(&b, 10, 8, -1);
		}
		/* Reject unsupported bits without FP exceptions or destination writes. */
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		if(p.alu){
			a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, 8);
			a64realproduct(&b);
		}else a64put(&b, 0xd37ff98d); /* lsl x13,x12,#1 */
		/* Shifted binary64 bounds: 2^31-0.5 or the representable value below 2^63. */
		a64put(&b, 0xd2e0000eU | (p.bytes == 4 ? 0x83bf : 0x87bf)<<5);
		a64put(&b, 0xf2c0000eU | 0xffff<<5);
		a64put(&b, 0xf2a0000eU | (p.bytes == 4 ? 0xffc0 : 0xffff)<<5);
		a64put(&b, 0xf280000eU | (p.bytes == 4 ? 0 : 0xfffe)<<5);
		a64put(&b, 0xeb0e01bf); a64guard(&b, 0x54000063);
		a64put(&b, 0x9e670180); /* fmov d0,x12 */
		if(p.alu){
			a64put(&b, 0xfd400141); a64put(&b, 0x1e610800);
			a64put(&b, 0x9e66000c); /* fmov x12,d0 */
			a64put(&b, 0xd37ff98d); /* magnitude from the scaled result */
		}
		a64put(&b, 0xd2e0000eU | 0x3fe0<<5);
		/* C selects +0.5 for both signs of zero. */
		done=b.n; a64put(&b, 0xb6f8000c); /* tbz x12,#63,positive */
		other=b.n; a64put(&b, 0xb400000d); /* cbz magnitude,positive */
		a64put(&b, 0xd24101ce); /* eor x14,x14,#1<<63 */
		b.code[done] |= (b.n-done)<<5; b.code[other] |= (b.n-other)<<5;
		a64put(&b, 0x9e6701c1); a64put(&b, 0x1e612800); /* fmov d1,x14; fadd */
		a64put(&b, 0x9e78000c); /* fcvtzs x12,d0 */
		a64ptr(&b, 11, abi->d); a64store(&b, 12, p.bytes);
	}else if(p.kind == LeafRealArithmetic){
		/* Preflight complete slots before loads, conversions or FP effects. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, 8, -1);
		a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, p.alu == 4 ? abi->wordbytes : 8, -1);
		a64ptr(&b, 10, abi->m); a64pcspan(&b, 10, 8, -1);
		/* Load both unaligned binary64 operands before storing an aliased result.
		 * AAPCS64 d0/d1 are volatile; FPCR/FPSR remain the host C environment. */
		a64ptr(&b, 9, abi->m); a64put(&b, 0xfd400120); /* ldr d0, [x9] */
		a64ptr(&b, 10, abi->s);
		if(p.alu == 4){
			a64load(&b, 12, 10, abi->wordbytes);
			if(abi->wordbytes == 4)a64put(&b, 0x93407d8c);
			a64put(&b, 0x9e620181); /* scvtf d1, x12 */
		}else a64put(&b, 0xfd400141); /* ldr d1, [x10] */
		a64put(&b, p.alu == 0 ? 0x1e612800 : p.alu == 1 ? 0x1e613800 :
			p.alu == 3 ? 0x1e611800 : 0x1e610800);
		a64ptr(&b, 11, abi->d); a64put(&b, 0xfd000160); /* str d0, [x11] */
	}else if(p.kind == LeafMove || p.kind == LeafAddress || p.kind == LeafNegReal){
		/* Preflight the complete result and source before loads or FP effects. */
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, p.bytes, -1);
		if(p.kind != LeafAddress){
			a64ptr(&b, 9, abi->s); a64pcspan(&b, 9, p.input, -1);
		}
		a64ptr(&b, 9, abi->s);
		if(p.kind != LeafAddress) a64load(&b, 12, 9, p.input);
		else a64put(&b, 0xaa0903ec); /* mov x12, x9 */
		/* EOR changes only the binary64 sign, including zeros and NaNs. */
		if(p.kind == LeafNegReal) a64put(&b, 0xd241018c); /* eor x12, x12, #1<<63 */
		/* LDRSH W sign-extends only to 32 bits; widen when the VM word is 64 bits. */
		if(p.bytes == 8 && (p.input == 2 || (p.input == 4 && op == ICVTWL))) a64put(&b, 0x93407d8c); /* sxtw x12, w12 */
		a64ptr(&b, 11, abi->d); a64store(&b, 12, p.bytes);
	}else if(p.kind == LeafShift || p.kind == LeafDivide){
		a64ptr(&b, 9, abi->s);
		a64pcspan(&b, 9, p.kind == LeafShift ? abi->wordbytes : p.bytes, -1);
		a64load(&b, 12, 9, p.kind == LeafShift ? abi->wordbytes : p.bytes);
		if(p.kind == LeafShift){
			/* Validate the complete count before accessing a possibly invalid value span. */
			a64put(&b, 0xf100001fU | (p.bytes*8)<<10 | 12<<5);
			a64put(&b, 0x54000063); /* b.lo valid */
		}else
			a64put(&b, 0xb500006c); /* cbnz x12, valid */
		a64put(&b, 0x52800000); a64put(&b, 0xd65f03c0);
		/* x4 is guard scratch; retain the count/divisor in x12 until the value read. */
		a64ptr(&b, 10, abi->m); a64pcspan(&b, 10, p.bytes, -1);
		a64ptr(&b, 11, abi->d); a64pcspan(&b, 11, p.bytes, -1);
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
	}else if(p.kind == LeafRealBranch){
        /* FCMP observes FPCR flushing and reads the target only for a true predicate. */
        a64ptr(&b, 9, abi->s);
        a64pcspan(&b, 9, 8, -1);
        a64ptr(&b, 10, abi->m);
        a64pcspan(&b, 10, 8, -1);
        a64load(&b, 12, 9, 8);
        a64realguard(&b, 12);
        a64load(&b, 13, 10, 8);
        a64realguard(&b, 13);
        a64put(&b, 0x9e670180);
        a64put(&b, 0x9e6701a1);
        a64put(&b, 0x1e612000);
        other = b.n;
        a64put(&b, 0x54000000U | inverse[p.condition]);
        a64ptr(&b, 11, abi->d);
        a64pcspan(&b, 11, 8, -1);
        a64load(&b, 12, 11, 8);
        a64put(&b, 0xf9000000U | (u32int)(abi->pc/8)<<10 | 12);
        if(other < LeafMax){
            b.code[other] |= (b.n-other)<<5;
        }
    }else if(p.kind == LeafAlu || p.kind == LeafBranch){
        if(p.kind == LeafAlu){
            /* No operand read may precede complete source/result span validation. */
            a64ptr(&b, 11, abi->d);
        a64pcspan(&b, 11, p.bytes, -1);
            a64ptr(&b, 9, abi->s);
        a64pcspan(&b, 9, p.bytes, -1);
            a64ptr(&b, 10, abi->m);
        a64pcspan(&b, 10, p.bytes, -1);
        }
        if(p.kind == LeafBranch){
            a64ptr(&b, 9, abi->s);
            a64pcspan(&b, 9, p.bytes, -1);
            a64ptr(&b, 10, abi->m);
            a64pcspan(&b, 10, p.bytes, -1);
        }
        a64ptr(&b, 9, abi->s);
        a64ptr(&b, 10, abi->m);
        a64load(&b, 12, 9, p.bytes);
        a64load(&b, 13, 10, p.bytes);
        width = p.bytes == 8 ? 0x80000000U : 0;
        if(p.kind == LeafAlu){
            a64put(&b, arithmetic[p.alu] | width | 12<<16 | 13<<5 | 13);
            a64ptr(&b, 11, abi->d);
        a64store(&b, 13, p.bytes);
        }else{
            a64put(&b, 0x6b00001fU | width | 13<<16 | 12<<5);
            other = b.n;
            a64put(&b, 0x54000000U | inverse[p.condition]);
            a64ptr(&b, 11, abi->d);
            a64pcspan(&b, 11, 8, -1);
            a64load(&b, 12, 11, 8);
            a64put(&b, 0xf9000000U | (u32int)(abi->pc/8)<<10 | 12);
            if(p.kind == LeafBranch && other < LeafMax){
                b.code[other] |= (b.n-other)<<5;
            }
        }
    }else if(p.kind == LeafJump){
        a64ptr(&b, 11, abi->d);
            a64pcspan(&b, 11, 8, -1);
            a64load(&b, 12, 11, 8);
        a64put(&b, 0xf9000000U | (u32int)(abi->pc/8)<<10 | 12);
    }else if(p.kind != LeafNop)
		return 0; /* A new classifier kind must never silently become a native no-op. */
	a64put(&b, 0x52800020); a64put(&b, 0xd65f03c0);
	if(b.n > LeafMax || b.n > cap) return 0;
	if(out != nil) for(i = 0; i < b.n; i++) out[i] = b.code[i];
	return b.n;
}
