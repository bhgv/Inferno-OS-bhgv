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

/* Branch over a side-effect-free fallback return when a guard succeeds. */
static void
a64guard(A64Buf *b, u32int condition)
{
	a64put(b, condition);
	a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
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


/* Resolve signed representation lengths and one/four-byte character strides. */
static void
a64stringheader(A64Buf *b, int owner, int length, int stride, int lenoff, int maxoff, int dataoff, int fallback)
{
	int empty, positive, done;

	a64put(b, 0x52800020U | stride);
	a64defer(b, 0xb4000000U | owner, fallback);
	a64put(b, 0xb100041fU | owner<<5);
	empty=b->n; a64put(b, 0x54000000);
	a64put(b, 0xb9800000U | (u32int)(lenoff/4)<<10 | owner<<5 | length);
	a64put(b, 0xf100001fU | length<<5);
	positive=b->n; a64put(b, 0x5400000a);
	a64put(b, 0xcb0003e0U | length<<16 | length);
	a64put(b, 0x52800080U | stride);
	b->code[positive] |= (b->n-positive)<<5;
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
	a64put(b, 0x910041af); a64put(b, 0xf10081ff); a64guard(b, 0x54000069);
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

/* Search bounded numeric tables with one rejection return and no runtime helper ABI. */
static int
a64case(A64Buf *b, LeafLayout *abi, LeafOp *op)
{
	int fallback, header, stride, loop, empty, left, right, found, done;

	if(abi->module < 0 || abi->module > 2047 || abi->module%8 ||
	   abi->linkmodule < 0 || abi->linkmodule > 2047 || abi->linkmodule%8 ||
	   abi->linkcompiled < 0 || abi->linkcompiled > 2047 || abi->linkcompiled%4 ||
	   abi->moduleprog < 0 || abi->moduleprog > 2047 || abi->moduleprog%8 ||
	   abi->modulenprog < 0 || abi->modulenprog > 2047 || abi->modulenprog%4 ||
	   abi->instbytes < 1 || abi->instbytes > 2047)
		return 0;
	header=op->alu ? 8 : 4; stride=op->alu ? 24 : 12;
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 12, abi->module); a64pcguard(b, 0xb500006c, fallback);
	a64put(b, 0xb100059f); a64pcguard(b, 0x54000061, fallback);
	a64field(b, 4, 12, abi->linkcompiled, 4); a64pcguard(b, 0x34000064, fallback);
	a64field(b, 12, 12, abi->linkmodule, 8); a64pcguard(b, 0xb500006c, fallback);
	a64put(b, 0xb100059f); a64pcguard(b, 0x54000061, fallback);
	a64ptr(b, 9, abi->s);
	if(op->bytes == 4)a64put(b, 0xb980012e); /* ldrsw x14,[x9] */
	else a64load(b, 14, 9, 8);
	a64ptr(b, 10, abi->d);
	a64put(b, 0xd1000544); a64put(b, 0xb100149f); a64pcguard(b, 0x54000069, fallback);
	a64put(b, 0xb9800141);
	a64put(b, 0xf100003fU | 4095<<10); a64pcguard(b, 0x54000069, fallback);
	a64put(b, 0xd2800009U | stride<<5);
	a64put(b, 0x9b097c24); a64put(b, 0x91000084U | header<<10);
	a64put(b, 0xab040145); a64pcguard(b, 0x54000063, fallback);
	a64put(b, 0xb1000cbf); a64pcguard(b, 0x54000063, fallback);
	a64put(b, 0xb98000ad); a64put(b, 0x9100014fU | header<<10);
	loop=b->n; empty=b->n; a64put(b, 0xb4000001);
	a64put(b, 0xd341fc22); a64put(b, 0x9b093c43);
	a64put(b, op->alu ? 0xf9400064 : 0xb9800064);
	a64put(b, 0xeb0401df); left=b->n; a64put(b, 0x5400000b);
	a64put(b, op->alu ? 0xf9400464 : 0xb9800464);
	a64put(b, 0xeb0401df); right=b->n; a64put(b, 0x5400000a);
	a64put(b, op->alu ? 0xb980106d : 0xb980086d);
	found=b->n; a64put(b, 0x14000000);
	b->code[left] |= (b->n-left)<<5;
	a64put(b, 0xd341fc21); a64put(b, 0x14000000U | ((loop-b->n)&0x3ffffff));
	b->code[right] |= (b->n-right)<<5;
	a64put(b, 0xcb020021); a64put(b, 0xd1000421);
	a64put(b, 0x9100006fU | stride<<10); a64put(b, 0x14000000U | ((loop-b->n)&0x3ffffff));
	done=b->n; b->code[empty] |= (done-empty)<<5; b->code[found] |= done-found;
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
	int offset, empty, fallback, same, marked, i;

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
	if(p->alu == 1)
		a64ptr(b, 12, abi->module);
	else{
		a64ptr(b, 9, abi->s); a64load(b, 12, 9, 8);
	}
	if(p->alu >= 2){
		a64pcguard(b, 0xb500006c, fallback);
		a64put(b, 0xb100059f); a64pcguard(b, 0x54000061, fallback);
		a64put(b, 0x91000000U | offset<<10 | 12<<5 | 9);
		a64load(b, 12, 9, 8);
	}
	a64ptr(b, 11, abi->d); a64load(b, 10, 11, 8);
	a64put(b, 0xb100055f);
	same=b->n; a64put(b, 0x54000000);
	a64put(b, 0xeb0c015f); a64pcguard(b, 0x54000060, fallback);
	b->code[same] |= (b->n-same)<<5;
	a64pcguard(b, 0xb500006c, fallback);
	a64put(b, 0xb100059f);
	empty=-1;
	if(p->alu == 1)
		a64pcguard(b, 0x54000061, fallback);
	else{
		empty=b->n; a64put(b, 0x54000000);
	}
	a64put(b, 0xf8400000U | ((u32int)-abi->heapref&511)<<12 | 12<<5 | 13);
	a64pcguard(b, 0xb500006d, fallback);
	a64put(b, 0xb10005bf); a64pcguard(b, 0x54000061, fallback);
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

/* Resolve current type metadata and copy a small record only when it owns no pointers. */
static int
a64mapped(A64Buf *b, LeafLayout *a, LeafOp *p)
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
	fallback=b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	if(p->alu){
		a64ptr(b, 9, a->s); a64load(b, 15, 9, 8);
		a64pcguard(b, 0xb500006f, fallback);
		a64put(b, 0xb10005ff); a64pcguard(b, 0x54000061, fallback);
	}
	a64ptr(b, 13, a->module);
	a64pcguard(b, 0xb500006d, fallback);
	a64put(b, 0xb10005bf); a64pcguard(b, 0x54000061, fallback);
	a64field(b, 13, 13, a->linkmodule, 8);
	a64pcguard(b, 0xb500006d, fallback);
	a64put(b, 0xb10005bf); a64pcguard(b, 0x54000061, fallback);
	a64field(b, 14, 13, a->moduletypes, 8);
	a64pcguard(b, 0xb500006e, fallback);
	a64put(b, 0xb10005df); a64pcguard(b, 0x54000061, fallback);
	a64field(b, 2, 13, a->modulenty, 4);
	/* Reject negative counts; unsigned bounds also reject zero counts and negative indexes. */
	a64put(b, 0x37f80002U | ((fallback-b->n)&0x3fff)<<5);
	a64ptr(b, 9, a->m);
	if(a->wordbytes == 4)
		a64put(b, 0xb980012c);
	else
		a64load(b, 12, 9, 8);
	a64put(b, 0xeb02019f); a64pcguard(b, 0x54000063, fallback);
	/* Index is bounded by a positive int, so only native pointer addition can wrap. */
	a64put(b, 0xab0c0dcd); a64pcguard(b, 0x54000063, fallback);
	a64load(b, 13, 13, 8);
	a64pcguard(b, 0xb500006d, fallback);
	a64put(b, 0xb10005bf); a64pcguard(b, 0x54000061, fallback);
	a64put(b, 0xb9800000U | (a->typesize/4)<<10 | 13<<5 | 12);
	a64put(b, 0xf102019f); a64pcguard(b, 0x54000069, fallback);
	empty=b->n; a64put(b, 0xb400000c);
	a64field(b, 14, 13, a->typenp, 4);
	a64pcguard(b, 0x3400006e, fallback);
	if(p->alu){
		a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 15<<5 | 14);
		a64pcguard(b, 0xb500006e, fallback);
		a64put(b, 0xb10005df); a64pcguard(b, 0x54000061, fallback);
		a64put(b, 0x910001e9U | a->listdata<<10);
		a64put(b, 0xf9000000U | (a->s/8)<<10 | 9);
	}else
		a64ptr(b, 9, a->s);
	a64ptr(b, 11, a->d);
	/* One overlap-aware loop uses a native signed stride instead of setting host flags. */
	a64put(b, 0xd2800022);
	a64put(b, 0xeb09017f);
	forward=b->n; a64put(b, 0x54000009);
	a64put(b, 0x92800002);
	a64put(b, 0x8b0c0129); a64put(b, 0xd1000529);
	a64put(b, 0x8b0c016b); a64put(b, 0xd100056b);
	if(b->n > LeafMax) return 0;
	b->code[forward] |= (b->n-forward)<<5;
	loop=b->n;
	a64load(b, 13, 9, 1); a64store(b, 13, 1);
	a64put(b, 0x8b020129); a64put(b, 0x8b02016b);
	a64put(b, 0xf100058c);
	a64put(b, 0x54000001U | ((loop-b->n)&0x7ffff)<<5);
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


/* Append bounded matching representations after checking slots, ownership and capacities. */
static int
a64stringappend(A64Buf *b, LeafLayout *a)
{
	int fallback, overlap, empty, loop;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringdata < 0 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8)
		return 0;
	fallback = b->n+1;
	a64put(b, 0x14000003); a64put(b, 0x52800000); a64put(b, 0xd65f03c0);
	a64ptr(b, 9, a->m); a64ptr(b, 10, a->d);
	a64put(b, 0xeb0a013f); a64defer(b, 0x54000001, fallback);
	a64load(b, 12, 9, 8);
	a64ptr(b, 9, a->s); a64load(b, 13, 9, 8);
	a64defer(b, 0xb400000c, fallback);
	a64put(b, 0xb100059f); a64defer(b, 0x54000000, fallback);
	a64defer(b, 0xb400000d, fallback);
	a64put(b, 0xb10005bf); a64defer(b, 0x54000000, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 12<<5 | 14);
	a64put(b, 0xf10005df); a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 13<<5 | 3);
	a64defer(b, 0xb4000003, fallback);
	a64wordfield(b, 14, 12, a->stringlen, 4);
	a64put(b, 0x93400000U | 63<<16 | 63<<10 | 14<<5 | 6);
	a64put(b, 0xf10001df);
	a64put(b, 0xda80a400U | 14<<16 | 14<<5 | 14);
	a64wordfield(b, 15, 13, a->stringlen, 4);
	a64put(b, 0x93400000U | 63<<16 | 63<<10 | 15<<5 | 3);
	a64put(b, 0xeb06007f); a64defer(b, 0x54000001, fallback);
	a64put(b, 0xf10001ff);
	a64put(b, 0xda80a400U | 15<<16 | 15<<5 | 15);
	a64put(b, 0xf10201ff); a64defer(b, 0x54000008, fallback);
	a64wordfield(b, 3, 13, a->stringmax, 4);
	a64put(b, 0xeb0301ff); a64defer(b, 0x5400000c, fallback);
	a64wordfield(b, 4, 12, a->stringmax, 4);
	a64put(b, 0x8b0f01c5);
	a64put(b, 0xeb0400bf); a64defer(b, 0x5400000c, fallback);
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
	a64put(b, 0xeb09017f);
	overlap = b->n; a64put(b, 0x54000009);
	a64put(b, 0xeb03017f); a64defer(b, 0x54000003, fallback);
	b->code[overlap] |= (b->n-overlap)<<5;
	empty = b->n; a64put(b, 0xb400000f);
	loop = b->n;
	a64load(b, 3, 9, 1); a64store(b, 3, 1);
	a64put(b, 0x91000529); a64put(b, 0x9100056b);
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
	int absent, empty, minus, digits, signonly, loop, positive;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringdata < 0 || a->stringdata > 2047) return 0;
	a64ptr(b, 9, a->s); a64load(b, 12, 9, 8);
	a64put(b, 0x5280000e); a64put(b, 0x5280000f);
	a64guard(b, 0xb500006c);
	a64put(b, 0xb100059f); absent=b->n; a64put(b, 0x54000000);
	a64field(b, 13, 12, a->stringlen, 4);
	a64put(b, 0x710001bfU | (u32int)(p->input+1)<<10); a64guard(b, 0x54000069);
	a64field(b, 10, 12, a->stringmax, 4);
	a64put(b, 0x6b0d015f); a64guard(b, 0x5400006a);
	a64put(b, 0x91000189U | (u32int)a->stringdata<<10);
	/* Preserve string2c's terminator mutation by declining nonterminated ASCII. */
	a64put(b, 0x8b0d012a); a64load(b, 10, 10, 1); a64guard(b, 0x3400006a);
	empty=b->n; a64put(b, 0x3400000d);
	a64load(b, 10, 9, 1); a64put(b, 0x7100b55f);
	a64put(b, 0x1a9f17ef); /* cset w15,eq */
	minus=b->n; a64put(b, 0x54000000);
	a64put(b, 0x7100ad5f); digits=b->n; a64put(b, 0x54000001);
	b->code[minus] |= (b->n-minus)<<5;
	a64put(b, 0x91000529); a64put(b, 0x510005ad);
	b->code[digits] |= (b->n-digits)<<5;
	a64put(b, 0x710001bfU | (u32int)p->input<<10); a64guard(b, 0x54000069);
	/* Decline sign-only real conversions before executing any FP instruction. */
	if(p->alu)
		a64guard(b, 0x3500006d);
	signonly=b->n; a64put(b, 0x3400000d);
	loop=b->n;
	a64load(b, 10, 9, 1); a64put(b, 0x5100c14a);
	a64put(b, 0x7100255f); a64guard(b, 0x54000069);
	a64put(b, 0x8b0e09ce); /* value*=5 */
	a64put(b, 0x8b0e01ce); /* value*=2 */
	a64put(b, 0x8b0a01ce);
	a64put(b, 0x91000529); a64put(b, 0x710005ad);
	a64put(b, 0x54000001U | ((loop-b->n)&0x7ffff)<<5);
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


/* Update or append one ASCII/Rune character within uniquely owned capacity. */
static int
a64stringstore(A64Buf *b, LeafLayout *a)
{
	int wide, done, unchanged;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringdata < 0 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8) return 0;
	a64ptr(b, 9, a->s); a64load(b, 12, 9, a->wordbytes);
	a64ptr(b, 10, a->m); a64load(b, 13, 10, a->wordbytes);
	a64ptr(b, 11, a->d); a64load(b, 14, 11, 8);
	a64guard(b, 0xb500006e);
	a64put(b, 0xb10005df); a64guard(b, 0x54000061);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 14<<5 | 4);
	a64put(b, 0xf100049f); a64guard(b, 0x54000060);
	a64field(b, 15, 14, a->stringlen, 4);
	a64put(b, 0x52b00002); a64put(b, 0x6b0201ff); a64guard(b, 0x54000061);
	a64put(b, 0x2a0f03e2); a64put(b, 0x36f80042); a64put(b, 0x4b0203e2);
	a64field(b, 3, 14, a->stringmax, 4);
	a64put(b, 0x6b02007f); a64guard(b, 0x5400006a);
	a64put(b, 0xeb0201bf); a64guard(b, 0x54000069);
	a64put(b, 0xeb0301bf); a64guard(b, 0x54000063);
	wide=b->n; a64put(b, 0x37f8000f);
	a64put(b, 0x7102019f); a64guard(b, 0x54000063);
	a64put(b, 0x910001cbU | (u32int)a->stringdata<<10);
	a64put(b, 0x8b0d016b); a64store(b, 12, 1);
	done=b->n; a64put(b, 0x14000000);
	if(b->n > LeafMax) return 0;
	b->code[wide] |= (b->n-wide)<<5;
	a64put(b, 0x910001cbU | (u32int)a->stringdata<<10);
	a64put(b, 0x8b0d096b); a64store(b, 12, 4);
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


/* Update a unique prefix or copy a bounded slice after ownership and extent checks. */
static int
a64stringslice(A64Buf *b, LeafLayout *a)
{
	int ascii, done, loop, prefix;

	if(a->stringlen < 0 || a->stringlen > 2047 || a->stringlen%4 ||
	   a->stringmax < 0 || a->stringmax > 2047 || a->stringmax%4 ||
	   a->stringdata < 0 || a->stringdata > 2047 ||
	   a->heapref <= 0 || a->heapref > 255 || a->heapref%8 || a->refbytes != 8) return 0;
	a64ptr(b, 9, a->s); a64load(b, 12, 9, a->wordbytes);
	a64ptr(b, 10, a->m); a64load(b, 13, 10, a->wordbytes);
	if(a->wordbytes == 4){a64put(b, 0x93407d8c); a64put(b, 0x93407dad);}
	a64put(b, 0xf100019f); a64guard(b, 0x5400006a); // start >= 0.
	a64put(b, 0xeb0c01bf); a64guard(b, 0x54000068); // end > start.
	a64ptr(b, 11, a->d); a64load(b, 14, 11, 8);
	a64guard(b, 0xb500006e);
	a64put(b, 0xb10005df); a64guard(b, 0x54000061);
	a64wordfield(b, 15, 14, a->stringlen, 4);
	a64put(b, 0xf10001ff); // Preserve representation while deriving absolute length.
	a64put(b, 0xda8fa5e2); // cneg x2,x15,lt.
	a64wordfield(b, 3, 14, a->stringmax, 4);
	a64put(b, 0xeb02007f); a64guard(b, 0x5400006a);
	a64put(b, 0xeb0201bf); a64guard(b, 0x54000069);
	a64put(b, 0xf8400000U | ((u32int)-a->heapref&511)<<12 | 14<<5 | 6);
	a64put(b, 0xf10004df); a64guard(b, 0x54000060);
	a64put(b, 0xcb0c01a4); // x4 = end-start, validated in the character domain.
	/* A zero-start slice retains its payload and needs no bounded copy loop. */
	prefix=b->n; a64put(b, 0xb400000c);
	a64put(b, 0xf102009f); a64guard(b, 0x54000069);
	b->code[prefix] |= (b->n-prefix)<<5;
	a64put(b, 0xaa0403e5); // x5 retains the resulting signed character count.
	ascii=b->n; a64put(b, 0xb6f8000f);
	a64put(b, 0xcb0503e5);
	a64put(b, 0xd37ef58c); a64put(b, 0xd37ef484); // Scale Rune offsets/counts only after bounds checks.
	b->code[ascii] |= (b->n-ascii)<<5;
	done=b->n; a64put(b, 0xb400000c);
	a64put(b, 0x910001cbU | (u32int)a->stringdata<<10);
	a64put(b, 0x8b0c0169);
	loop=b->n;
	a64load(b, 6, 9, 1); a64store(b, 6, 1);
	a64put(b, 0x91000529); a64put(b, 0x9100056b);
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
	int offsets[10], i, fallback;

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
		a64ptr(b, 9, a->s); a64load(b, 13, 9, 8);
		a64put(b, 0x910005a2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
		/* Match the C error boundary before fetching a possibly inaccessible index. */
		a64ptr(b, 10, a->m); a64load(b, 12, 10, a->wordbytes);
		if(a->wordbytes==4)a64put(b, 0x93407d8c);
		a64wordfield(b, 14, 13, a->linknlinks, 4);
		a64put(b, 0xf10001df); a64defer(b, 0x5400000b, fallback);
		a64put(b, 0xeb0e019f); a64defer(b, 0x54000002, fallback);
		a64put(b, 0x8b0c11ad); // add x13,x13,x12,lsl #4.
		a64field(b, 14, 13, a->linklinks+a->modlframe, 8);
	}else{
		a64ptr(b, 9, a->s); a64load(b, 12, 9, a->wordbytes);
		if(a->wordbytes==4)a64put(b, 0x93407d8c);
		a64ptr(b, 13, a->module);
		for(i=0; i<3; i++){
			a64put(b, 0x910005a2); a64put(b, 0xf100045f);
			a64defer(b, 0x54000009, fallback);
			if(i==0)a64field(b, 13, 13, a->linkmodule, 8);
			if(i==1){
				a64wordfield(b, 14, 13, a->modulenty, 4);
				a64put(b, 0xf10001df); a64defer(b, 0x5400000b, fallback);
				a64put(b, 0xeb0e019f); a64defer(b, 0x54000002, fallback);
				a64field(b, 13, 13, a->moduletypes, 8);
			}
		}
		a64put(b, 0xf86c79ae); // ldr x14,[x13,x12,lsl #3].
	}
	a64put(b, 0x910005c2); a64put(b, 0xf100045f); a64defer(b, 0x54000009, fallback);
	a64wordfield(b, 15, 14, a->typesize, 4);
	a64put(b, 0xd2800002U | (u32int)a->framesize<<5);
	a64put(b, 0xeb0201ff); a64defer(b, 0x5400000b, fallback);
	a64field(b, 2, 14, a->typenp, 4); a64defer(b, 0xb5000002, fallback);
	a64ptr(b, 9, a->sp); a64ptr(b, 10, a->ts);
	a64defer(b, 0xb4000009, fallback);
	a64put(b, 0xb100055f); a64defer(b, 0x54000000, fallback);
	a64put(b, 0xeb09015f); a64defer(b, 0x54000003, fallback);
	a64put(b, 0xcb090142); a64put(b, 0xeb0f005f); a64defer(b, 0x54000009, fallback);
	a64ptr(b, 11, a->d);
	a64put(b, 0x8b0f0122);
	a64put(b, 0xf9000000U | (u32int)(a->sp/8)<<10 | 2);
	a64put(b, 0xf9000000U | (u32int)(a->frametype/8)<<10 | 9<<5 | 14);
	a64put(b, 0xf9000000U | (u32int)(a->framemr/8)<<10 | 9<<5 | 31);
	a64store(b, 9, 8);
	return 1;
}


/* Emit AAPCS64 leaves with x0 as REG*, no stack frame and no callee-saved clobbers. */
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
	if(p.kind == LeafFrame){
		if(!a64frame(&b, abi, p.alu))return 0;
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
	}else if(p.kind == LeafOwner){
		if(!a64owner(&b, abi, &p)) return 0;
	}else if(p.kind == LeafCaseJump){
		if(!a64case(&b, abi, &p)) return 0;
	}else if(p.kind == LeafStringBranch){
		/* Mixed character widths share one bounded loop and a common fallback. */
		if(abi->stringlen < 0 || abi->stringlen > 2047 || abi->stringlen%4 ||
		   abi->stringmax < 0 || abi->stringmax > 2047 || abi->stringmax%4 ||
		   abi->stringdata < 0 || abi->stringdata > 2047) return 0;
		fallback=b.n+1;
		a64put(&b, 0x14000003); a64put(&b, 0x52800000); a64put(&b, 0xd65f03c0);
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, 8);
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
		a64ptr(&b, 11, abi->d); a64load(&b, 12, 11, 8);
		a64put(&b, 0xf9000000U | (abi->pc/8)<<10 | 12);
		b.code[patch] |= (b.n-patch)<<5;
	}else if(p.kind == LeafTypeCheck){
		/* The header displacement is private and independent of the Dis WORD. */
		if(abi->heaptype <= 0 || abi->heaptype > 255 || abi->heaptype%8) return 0;
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64ptr(&b, 10, abi->d); a64load(&b, 13, 10, 8);
		a64put(&b, 0xb100059f);
		empty=b.n; a64put(&b, 0x54000000);
		a64guard(&b, 0xb500006c); a64guard(&b, 0xb500006d);
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
		a64ptr(&b, 12, abi->module); a64pcguard(&b, 0xb500006c, fallback);
		a64put(&b, 0xb100059f); a64pcguard(&b, 0x54000061, fallback);
		if(p.kind == LeafTableJump){
			if(abi->linkcompiled < 0 || abi->linkcompiled > 2047 || abi->linkcompiled%4) return 0;
			a64field(&b, 14, 12, abi->linkcompiled, 4); a64pcguard(&b, 0x3400006e, fallback);
		}
		a64field(&b, 12, 12, abi->linkmodule, 8); a64pcguard(&b, 0xb500006c, fallback);
		a64put(&b, 0xb100059f); a64pcguard(&b, 0x54000061, fallback);
		a64put(&b, 0xb9800000U | (abi->modulenprog/4)<<10 | 12<<5 | 13);
		a64put(&b, 0xf10001bf); a64pcguard(&b, 0x5400006a, fallback);
		a64ptr(&b, 9, abi->s); a64load(&b, 14, 9, abi->wordbytes);
		if(p.kind == LeafTableJump){
			/* Native tables retain four-byte entries and the preceding compiler count. */
			if(abi->wordbytes == 4)a64put(&b, 0x93407dce);
			a64put(&b, 0xf10001df); a64pcguard(&b, 0x5400006a, fallback);
			a64ptr(&b, 9, abi->d);
			a64put(&b, 0xf100113f); a64pcguard(&b, 0x54000062, fallback);
			a64put(&b, 0xb100053f); a64pcguard(&b, 0x54000061, fallback);
			a64put(&b, 0xd100112a); a64put(&b, 0xb980014f);
			a64put(&b, 0xf10001ff); a64pcguard(&b, 0x5400006c, fallback);
			a64put(&b, 0xeb0f01df); a64pcguard(&b, 0x54000063, fallback);
			a64put(&b, 0xd3400000U | 62<<16 | 61<<10 | 14<<5 | 14);
			a64put(&b, 0xab0e012e); a64pcguard(&b, 0x54000063, fallback);
			a64put(&b, 0xb1000ddf); a64pcguard(&b, 0x54000063, fallback);
			a64put(&b, 0xb98001ce);
		}
		a64put(&b, 0xeb0d01df); a64pcguard(&b, 0x54000063, fallback);
		a64put(&b, 0xd280000dU | abi->instbytes<<5); a64put(&b, 0x9b0d7dce);
		a64field(&b, 15, 12, abi->moduleprog, 8); a64pcguard(&b, 0xb500006f, fallback);
		a64put(&b, 0xb10005ff); a64pcguard(&b, 0x54000061, fallback);
		a64put(&b, 0xab0e01ee); a64pcguard(&b, 0x54000063, fallback);
		if(p.kind == LeafTableJump)a64put(&b, 0xf9000000U | (u32int)(abi->pc/8)<<10 | 14);
		else{a64ptr(&b, 11, abi->d); a64store(&b, 14, 8);}
	}else if(p.kind == LeafFixedCoefficient){
		/* Coefficient slots follow the VM frame layout, not native pointer alignment. */
		if(abi->fp < 0 || abi->fp > 2047 || abi->fp%8 || abi->fixedtmp < 0 || abi->fixedtmp > 2047 ||
		   abi->fixedcoef < 0 || abi->fixedcoef > 2047) return 0;
		fallback=b.n+1;
		a64put(&b, 0x14000003); a64put(&b, 0x52800000); a64put(&b, 0xd65f03c0);
		a64ptr(&b, 9, abi->fp); a64defer(&b, 0xb4000009, fallback);
		a64put(&b, 0xb100053f); a64defer(&b, 0x54000000, fallback);
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
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8); a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
		a64ptr(&b, 9, abi->d); a64load(&b, 13, 9, 8);
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
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8); a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
		a64wordfield(&b, 13, 12, abi->arraylen, abi->wordbytes);
		a64put(&b, 0xf10001bf); a64guard(&b, 0x5400006a);
		a64ptr(&b, 9, abi->d); a64load(&b, 14, 9, abi->wordbytes);
		a64put(&b, 0xeb0d01df); a64guard(&b, 0x54000063);
		a64field(&b, 15, 12, abi->arraytype, 8); a64guard(&b, 0xb500006f);
		a64put(&b, 0xb10005ff); a64guard(&b, 0x54000061);
		a64put(&b, 0xb9800000U | (abi->typesize/4)<<10 | 15<<5 | 15); /* ldrsw stride */
		a64put(&b, 0xf10001ff); a64guard(&b, 0x5400006a);
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
		a64ptr(&b, 10, abi->fp); a64guard(&b, 0xb500006a);
		a64put(&b, 0xb100055f); a64guard(&b, 0x54000061);
		a64put(&b, 0x91000000U | abi->fixedtmp<<10 | 10<<5 | 10);
		a64load(&b, 13, 10, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407dad);
		a64put(&b, 0x9100fdae); a64put(&b, 0xf101fddf); a64guard(&b, 0x54000063);
		a64ptr(&b, 9, abi->s); a64load(&b, 15, 9, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407def);
		if(p.alu){ a64guard(&b, 0xb500006f); }
		a64ptr(&b, 9, abi->m); a64load(&b, 12, 9, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407d8c);
		if(!p.alu) a64put(&b, 0x9b0f7d8c);
		a64put(&b, 0xf10001bf); a64put(&b, 0x5400006b);
		a64put(&b, 0x9acd218c); a64put(&b, 0x14000003);
		a64put(&b, 0xcb0d03ed); a64put(&b, 0x9acd298c);
		if(p.alu) a64put(&b, 0x9acf0d8c);
		a64ptr(&b, 11, abi->d); a64store(&b, 12, abi->wordbytes);
	}else if(p.kind == LeafRealResize){
		/* FPCR/FPSR supply the same conversion environment as native C. */
		a64ptr(&b, 9, abi->s);
		a64put(&b, p.input == 4 ? 0xbd400120 : 0xfd400120);
		a64put(&b, p.input == 4 ? 0x1e22c000 : 0x1e624000);
		a64ptr(&b, 11, abi->d);
		a64put(&b, p.bytes == 4 ? 0xbd000160 : 0xfd000160);
	}else if(p.kind == LeafIntegerReal){
		/* SCVTF rounds every signed input with the active C FP environment. */
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, p.input);
		if(p.input == 4)a64put(&b, 0x93407d8c);
		a64put(&b, 0x9e620180); /* scvtf d0,x12 */
		a64ptr(&b, 11, abi->d); a64put(&b, 0xfd000160);
	}else if(p.kind == LeafFixedShift){
		/* Validate the signed interval before fetching a possibly inaccessible source. */
		a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407dad); /* sxtw x13,w13 */
		a64put(&b, 0x9100fdae); /* add x14,x13,#63 */
		a64put(&b, 0xf101fddf); /* cmp x14,#127 */
		a64guard(&b, 0x54000063);
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, abi->wordbytes);
		if(abi->wordbytes == 4) a64put(&b, 0x93407d8c);
		a64put(&b, 0xf10001bf); a64put(&b, 0x5400006b); /* cmp count,0; b.lt negative */
		a64put(&b, 0x9acd218c); a64put(&b, 0x14000003); /* lsl; b done */
		a64put(&b, 0xcb0d03ed); a64put(&b, 0x9acd298c); /* neg count; asr */
		a64ptr(&b, 11, abi->d); a64store(&b, 12, abi->wordbytes);
	}else if(p.kind == LeafPower){
		/* Minimum negative exponents are handled by parity without signed negation. */
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, abi->wordbytes);
		a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, p.bytes);
		a64put(&b, abi->wordbytes == 8 ? 0xf100019fU : 0x7100019fU);
		scale=b.n; a64put(&b, 0x5400000a);
		a64guard(&b, 0xb500006d);
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
		if(abi->stringlen < 0 || abi->stringlen > 16380 || abi->stringlen%4 != 0 ||
		   abi->stringdata < 0 || abi->stringdata > 4095 || abi->stringdata%4 != 0 ||
		   abi->stringmax < 0 || abi->stringmax > 16380 || abi->stringmax%4) return 0;
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
		a64field(&b, 13, 12, abi->stringlen, 4);
		a64put(&b, 0x52b0000e); a64put(&b, 0x6b0e01bf); a64guard(&b, 0x54000061);
		a64put(&b, 0x2a0d03ef); /* preserve the representation sign */
		a64put(&b, 0x36f8004d); a64put(&b, 0x4b0d03ed);
		/* Check signed capacity while x12 still holds the original owner. */
		a64field(&b, 14, 12, abi->stringmax, 4);
		a64put(&b, 0x6b0d01df); a64guard(&b, 0x5400006a);
		a64ptr(&b, 10, abi->m); a64load(&b, 14, 10, abi->wordbytes);
		a64put(&b, 0xeb0d01df); a64guard(&b, 0x54000063);
		other = b.n; a64put(&b, 0x36f8000f); /* tbz w15, #31, ASCII */
		a64put(&b, 0x8b0e098c);
		a64put(&b, 0xb9800000U | (u32int)(abi->stringdata/4)<<10 | 12<<5 | 13);
		done = b.n; a64put(&b, 0x14000000);
		b.code[other] |= (b.n-other)<<5;
		a64put(&b, 0x8b0e018c); a64field(&b, 13, 12, abi->stringdata, 1);
		b.code[done] |= b.n-done;
		a64ptr(&b, 11, abi->d); a64store(&b, 13, abi->wordbytes);
	}else if(p.kind == LeafListLength){
		/* Bounded scans decline without publishing a partial length. */
		if(abi->listtail < 0 || abi->listtail > 32760 || abi->listtail%8 != 0) return 0;
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64put(&b, 0x5280000d);
		loop = b.n;
		a64put(&b, 0xb100059f);
		done = b.n; a64put(&b, 0x54000000);
		a64guard(&b, 0xb500006c);
		a64put(&b, 0x710201bf); a64guard(&b, 0x54000063); /* count < 128 */
		a64field(&b, 12, 12, abi->listtail, 8);
		a64put(&b, 0x110005ad);
		a64put(&b, 0x14000000U | ((u32int)(loop-b.n)&0x3ffffff));
		b.code[done] |= (b.n-done)<<5;
		a64ptr(&b, 11, abi->d); a64store(&b, 13, abi->wordbytes);
	}else if(p.kind == LeafHead){
		if(abi->listdata < 0 || abi->listdata > 4095 || abi->listdata%8 != 0) return 0;
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
		a64field(&b, 12, 12, abi->listdata, p.bytes);
		a64ptr(&b, 11, abi->d); a64store(&b, 12, p.bytes);
	}else if(p.kind == LeafStringLength){
		if(abi->stringlen < 0 || abi->stringlen > 16380 || abi->stringlen%4 != 0) return 0;
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		a64guard(&b, 0xb500006c);
		a64put(&b, 0xb100059f); /* cmn x12, #1 */
		empty = b.n; a64put(&b, 0x54000000);
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
			a64ptr(&b, 9, abi->s); a64load(&b, 15, 9, 8);
			a64guard(&b, 0xb500006f);
			a64put(&b, 0xb10005ff); a64guard(&b, 0x54000061);
		}
		/* Validate the owner first for headm, then capture the complete count. */
		a64ptr(&b, 10, abi->m); a64load(&b, 12, 10, abi->wordbytes);
		a64put(&b, (abi->wordbytes == 8 ? 0xf100019fU : 0x7100019fU));
		a64guard(&b, 0x5400006a); /* b.ge valid */
		empty = b.n; a64put(&b, 0xb400000c);
		a64put(&b, 0xf102019f); /* cmp x12, #128 */
		a64guard(&b, 0x54000069); /* b.ls small */
		if(p.kind == LeafHeadCopy)
			a64put(&b, 0x910001e9U | (u32int)abi->listdata<<10); /* add x9, x15, data */
		else
			a64ptr(&b, 9, abi->s);
		a64ptr(&b, 11, abi->d);
		a64put(&b, 0xeb09017f); /* cmp x11, x9 */
		other = b.n; a64put(&b, 0x54000009); /* b.ls forward */
		a64put(&b, 0x8b0c0129); a64put(&b, 0x8b0c016b);
		loop = b.n;
		a64put(&b, 0xd1000529); a64put(&b, 0xd100056b);
		a64load(&b, 13, 9, 1); a64store(&b, 13, 1);
		a64put(&b, 0xf100058c); /* subs x12, x12, #1 */
		a64put(&b, 0x54000001U | ((u32int)(loop-b.n)&0x7ffff)<<5);
		done = b.n; a64put(&b, 0x14000000);
		b.code[other] |= (b.n-other)<<5;
		loop = b.n;
		a64load(&b, 13, 9, 1); a64store(&b, 13, 1);
		a64put(&b, 0x91000529); a64put(&b, 0x9100056b);
		a64put(&b, 0xf100058c);
		a64put(&b, 0x54000001U | ((u32int)(loop-b.n)&0x7ffff)<<5);
		b.code[empty] |= (b.n-empty)<<5; b.code[done] |= b.n-done;
	}else if(p.kind == LeafLength || p.kind == LeafIndex){
		if(abi->arraylen < 0 || abi->arraydata < 0 || abi->arraylen > 16376 ||
		   abi->arraydata > 16376 || (abi->arraylen|abi->arraydata)%8 != 0) return 0;
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8);
		if(p.kind == LeafLength){
			empty = b.n; a64put(&b, 0xb400000c);
			a64put(&b, 0xb100059f); /* cmn x12, #1 */
			other = b.n; a64put(&b, 0x54000000);
			a64field(&b, 12, 12, abi->arraylen, abi->wordbytes);
			done = b.n; a64put(&b, 0x14000000);
			b.code[empty] |= (b.n-empty)<<5; b.code[other] |= (b.n-other)<<5;
			a64put(&b, 0x5280000c);
			b.code[done] |= b.n-done;
			a64ptr(&b, 11, abi->d); a64store(&b, 12, abi->wordbytes);
		}else{
			a64guard(&b, 0xb500006c);
			a64put(&b, 0xb100059f); a64guard(&b, 0x54000061);
			a64wordfield(&b, 13, 12, abi->arraylen, abi->wordbytes);
			a64put(&b, 0xf10001bf); a64guard(&b, 0x5400006a); /* nonnegative length */
			a64ptr(&b, 9, abi->d); a64load(&b, 14, 9, abi->wordbytes);
			a64put(&b, 0xeb0d01df); a64guard(&b, 0x54000063); /* index < length */
			scale = p.bytes == 1 ? 0 : p.bytes == 4 ? 2 : 3; limit = 0x7fffffff>>scale;
			a64put(&b, 0xd280000fU | (limit&0xffff)<<5);
			a64put(&b, 0xf2a0000fU | (limit>>16)<<5);
			a64put(&b, 0xeb0f01df); a64guard(&b, 0x54000069); /* bounded byte offset */
			a64field(&b, 13, 12, abi->arraydata, 8);
			a64guard(&b, 0xb500006d);
			a64put(&b, 0xb10005bf); a64guard(&b, 0x54000061);
			a64put(&b, 0xab000000U | 14<<16 | scale<<10 | 13<<5 | 14);
			a64guard(&b, 0x54000063); /* no native address carry */
			a64ptr(&b, 11, abi->m); a64store(&b, 14, abi->wordbytes);
		}
	}else if(p.kind == LeafRealPower){
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
	}else if(p.kind == LeafRealBranch){
		/* FCMP observes FPCR flushing and reads the target only for a true predicate. */
		a64ptr(&b, 9, abi->s); a64load(&b, 12, 9, 8); a64realguard(&b, 12);
		a64ptr(&b, 10, abi->m); a64load(&b, 13, 10, 8); a64realguard(&b, 13);
		a64put(&b, 0x9e670180); a64put(&b, 0x9e6701a1); a64put(&b, 0x1e612000);
		a64put(&b, 0x54000080U | inverse[p.condition]);
		a64ptr(&b, 11, abi->d); a64load(&b, 12, 11, 8);
		a64put(&b, 0xf9000000U | (u32int)(abi->pc/8)<<10 | 12);
	}else if(p.kind == LeafAlu || p.kind == LeafBranch){
		a64ptr(&b, 9, abi->s); a64ptr(&b, 10, abi->m);
		a64load(&b, 12, 9, p.bytes);
		a64load(&b, 13, 10, p.bytes);
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
	}else if(p.kind != LeafNop)
		return 0; /* A new classifier kind must never silently become a native no-op. */
	a64put(&b, 0x52800020); a64put(&b, 0xd65f03c0);
	if(b.n > LeafMax || b.n > cap) return 0;
	if(out != nil) for(i = 0; i < b.n; i++) out[i] = b.code[i];
	return b.n;
}
