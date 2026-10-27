#include <lib9.h>
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "pool.h"
#include "kernel.h"

REG	R;			/* Virtual Machine registers */
String	snil;			/* String known to be zero length */

#define OP(fn)	void fn(void)
#define B(r)	*((BYTE*)(R.r))
#define W(r)	*((WORD*)(R.r))
#define UW(r)	*((UWORD*)(R.r))
#define F(r)	*((REAL*)(R.r))
#define V(r)	*((LONG*)(R.r))	
#define UV(r)	*((ULONG*)(R.r))	
#define	S(r)	*((String**)(R.r))
#define	A(r)	*((Array**)(R.r))
#define	L(r)	*((List**)(R.r))
#define P(r)	*((WORD**)(R.r))
#define C(r)	*((Channel**)(R.r))
#define T(r)	*((void**)(R.r))
#define JMP(r)	memmove(&R.PC, R.r, sizeof(R.PC))
#define SH(r)	*((SHORT*)(R.r))
#define SR(r)	*((SREAL*)(R.r))

OP(runt) {}


/* Capture possibly unaligned real bits before evaluating and publishing the result. */
OP(negf)
{
	REAL source;
	REAL result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = -source;
	memmove(R.d, &result, sizeof(result));
}
/* REG.d borrows a native target slot; REG.s/m are ignored. No blocking or
 * ownership transfer occurs. Invalid target spans raise exBounds before reads
 * or PC publication. Native pointer width is independent of the VM WORD width.
 */
OP(jmp)
{
    if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
        error(exBounds);
    }
    JMP(d);
}


/* Validate a Dis program index before turning it into a code pointer. */
OP(movpc)
{
	WORD pc;
	uintptr offset;
	Inst *target;
	Modlink *ml;

	ml = R.M;
	if(ml == H || ml == nil || (uintptr)ml > (uintptr)-1-sizeof(*ml) ||
	   ml->m == H || ml->m == nil || (uintptr)ml->m > (uintptr)-1-sizeof(*ml->m))
		error(exModule);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(pc))
		error(exBounds);
	memmove(&pc, R.s, sizeof(pc));
	if(pc < 0 || pc >= ml->m->nprog || ml->m->prog == nil || ml->m->prog == H)
		error(exBounds);
	if((uvlong)pc > (uintptr)-1/sizeof(Inst))
		error(exBounds);
	offset = (uintptr)pc*sizeof(Inst);
	if((uintptr)ml->m->prog > (uintptr)-1-offset)
		error(exBounds);
	target = (Inst*)((uintptr)ml->m->prog+offset);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(target))
		error(exBounds);
	memmove(R.d, &target, sizeof(target));
}


/* Capture counts and reject invalid nonempty native spans before raw copying. */
OP(movm)
{
	WORD n;

	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(n))
		error(exBounds);
	memmove(&n, R.m, sizeof(n));
	if(n < 0)
		error(exBounds);
	if(n != 0){
		if(R.s == nil || R.s == H || R.d == nil || R.d == H)
			error(exNilref);
		if((uintptr)R.s > (uintptr)-1-(uintptr)n ||
		   (uintptr)R.d > (uintptr)-1-(uintptr)n)
			error(exBounds);
		memmove(R.d, R.s, n);
	}
}


/* Preserve the active VM word-width address representation at any slot alignment. */
OP(lea)
{
	WORD address;

	/* The source is an address value; validate only the writable WORD slot. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	address = (WORD)(uintptr)R.s;
	memmove(R.d, &address, sizeof(address));
}


/* Preserve overlapping scalar slots without requiring host alignment. */
OP(movb)
{
	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(BYTE) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(BYTE))
		error(exBounds);
	memmove(R.d, R.s, sizeof(BYTE));
}


/* Preserve overlapping scalar slots without requiring host alignment. */
OP(movw)
{
	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(WORD) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(R.d, R.s, sizeof(WORD));
}


/* Preserve overlapping scalar slots without requiring host alignment. */
OP(movf)
{
	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL))
		error(exBounds);
	memmove(R.d, R.s, sizeof(REAL));
}


/* Preserve overlapping scalar slots without requiring host alignment. */
OP(movl)
{
	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(LONG) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(LONG))
		error(exBounds);
	memmove(R.d, R.s, sizeof(LONG));
}


/* Capture the BYTE source before converting into a possibly overlapping WORD slot. */
OP(cvtbw)
{
	BYTE source;
	WORD result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(WORD) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(BYTE))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture the WORD source before converting into a possibly overlapping BYTE slot. */
OP(cvtwb)
{
	WORD source;
	BYTE result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(BYTE) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture possibly unaligned real bits before evaluating and publishing the result. */
OP(cvtrf)
{
	SREAL source;
	REAL result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(SREAL))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture possibly unaligned real bits before evaluating and publishing the result. */
OP(cvtfr)
{
	REAL source;
	SREAL result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(SREAL) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture the WORD source before converting into a possibly overlapping SHORT slot. */
OP(cvtws)
{
	WORD source;
	SHORT result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(SHORT) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture the SHORT source before converting into a possibly overlapping WORD slot. */
OP(cvtsw)
{
	SHORT source;
	WORD result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(WORD) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(SHORT))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture the unaligned integer before converting with the current FP environment. */
OP(cvtwf)
{
	WORD source;
	REAL result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(addb)
{
	BYTE left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left + right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(addw)
{
	UWORD left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left + right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(addl)
{
	ULONG left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left + right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture both real operands before an overlapping, possibly unaligned store. */
OP(addf)
{
	REAL left, right, result;

	/* Bound every used slot before operand reads or floating-point effects. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(REAL) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL))
		error(exBounds);

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left + right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(subb)
{
	BYTE left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left - right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(subw)
{
	UWORD left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left - right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(subl)
{
	ULONG left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left - right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture both real operands before an overlapping, possibly unaligned store. */
OP(subf)
{
	REAL left, right, result;

	/* Bound every used slot before operand reads or floating-point effects. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(REAL) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL))
		error(exBounds);

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left - right;
	memmove(R.d, &result, sizeof(result));
}


/* Preserve the VM's divide-by-zero exception for byte operands too. */
OP(divb)
{
	BYTE dividend, divisor, result;

	/* A zero divisor wins before any dividend or output access. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(divisor))
		error(exBounds);
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(dividend))
		error(exBounds);
	memmove(&dividend, R.m, sizeof(dividend));
	result = dividend / divisor;
	memmove(R.d, &result, sizeof(result));
}


/* Avoid both zero division and the signed minimum / -1 C overflow case. */
OP(divw)
{
	WORD dividend, divisor, result;

	/* A zero divisor wins before any dividend or output access. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(divisor))
		error(exBounds);
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(dividend))
		error(exBounds);
	memmove(&dividend, R.m, sizeof(dividend));
	if(dividend < 0 && divisor == -1 &&
	   (UWORD)dividend == ((UWORD)1 << (sizeof(WORD)*8-1)))
		result = dividend;
	else
		result = dividend / divisor;
	memmove(R.d, &result, sizeof(result));
}


/* Apply the same overflow-safe rules to the VM's big integer type. */
OP(divl)
{
	LONG dividend, divisor, result;

	/* A zero divisor wins before any dividend or output access. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(divisor))
		error(exBounds);
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(dividend))
		error(exBounds);
	memmove(&dividend, R.m, sizeof(dividend));
	if(dividend < 0 && divisor == -1 &&
	   (ULONG)dividend == ((ULONG)1 << (sizeof(LONG)*8-1)))
		result = dividend;
	else
		result = dividend / divisor;
	memmove(R.d, &result, sizeof(result));
}


/* Capture both real operands before an overlapping, possibly unaligned store. */
OP(divf)
{
	REAL left, right, result;

	/* Bound every used slot before operand reads or floating-point effects. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(REAL) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL))
		error(exBounds);

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left / right;
	memmove(R.d, &result, sizeof(result));
}


/* Byte remainder by zero must not reach the host C operator. */
OP(modb)
{
	BYTE dividend, divisor, result;

	/* A zero divisor wins before any dividend or output access. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(divisor))
		error(exBounds);
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(dividend))
		error(exBounds);
	memmove(&dividend, R.m, sizeof(dividend));
	result = dividend % divisor;
	memmove(R.d, &result, sizeof(result));
}


/* Avoid zero division and the signed minimum % -1 C overflow case. */
OP(modw)
{
	WORD dividend, divisor, result;

	/* A zero divisor wins before any dividend or output access. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(divisor))
		error(exBounds);
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(dividend))
		error(exBounds);
	memmove(&dividend, R.m, sizeof(dividend));
	if(dividend < 0 && divisor == -1 &&
	   (UWORD)dividend == ((UWORD)1 << (sizeof(WORD)*8-1)))
		result = 0;
	else
		result = dividend % divisor;
	memmove(R.d, &result, sizeof(result));
}


/* Apply the same overflow-safe rules to big-integer remainder. */
OP(modl)
{
	LONG dividend, divisor, result;

	/* A zero divisor wins before any dividend or output access. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(divisor))
		error(exBounds);
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(dividend))
		error(exBounds);
	memmove(&dividend, R.m, sizeof(dividend));
	if(dividend < 0 && divisor == -1 &&
	   (ULONG)dividend == ((ULONG)1 << (sizeof(LONG)*8-1)))
		result = 0;
	else
		result = dividend % divisor;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(mulb)
{
	BYTE left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left * right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(mulw)
{
	UWORD left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left * right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(mull)
{
	ULONG left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left * right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture both real operands before an overlapping, possibly unaligned store. */
OP(mulf)
{
	REAL left, right, result;

	/* Bound every used slot before operand reads or floating-point effects. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(REAL) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL))
		error(exBounds);

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left * right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(andb)
{
	BYTE left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left & right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(andw)
{
	UWORD left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left & right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(andl)
{
	ULONG left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left & right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(xorb)
{
	BYTE left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left ^ right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(xorw)
{
	UWORD left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left ^ right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(xorl)
{
	ULONG left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left ^ right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(orb)
{
	BYTE left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left | right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(orw)
{
	UWORD left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left | right;
	memmove(R.d, &result, sizeof(result));
}


/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(orl)
{
	ULONG left, right, result;

	/* Bound all decoded spans before reading either potentially aliased operand. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(right) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(left))
		error(exBounds);
	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left | right;
	memmove(R.d, &result, sizeof(result));
}
/* Validate a shift count before any C shift expression is evaluated. */
static WORD
shiftcount(WORD count, int width)
{
	if(count < 0 || count >= width)
		error(exInval);
	return count;
}

/* Keep byte shifts within the defined width of the byte value. */
/* Use REG.s as a borrowed WORD count, REG.m as the value and REG.d as result.
 * No ownership transfer or blocking occurs. Invalid slots raise exBounds;
 * invalid counts raise exInval before value/result access. Failure preserves REG
 * and storage; successful reads precede the possibly overlapping result store.
 */
OP(shlb)
{
    WORD n;
    BYTE value, result;

    /* Reject malformed count slots before the first operand read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n)){
        error(exBounds);
    }
    memmove(&n, R.s, sizeof(n));
    n = shiftcount(n, 8);
    /* Count errors win; both remaining slots must fit before value access. */
    if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value) ||
       R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result)){
        error(exBounds);
    }
    memmove(&value, R.m, sizeof(value));
    result = value << n;
    memmove(R.d, &result, sizeof(result));
}
/* Shift word bits unsigned to avoid signed-left-shift overflow. */
/* Use REG.s as a borrowed WORD count, REG.m as the value and REG.d as result.
 * No ownership transfer or blocking occurs. Invalid slots raise exBounds;
 * invalid counts raise exInval before value/result access. Failure preserves REG
 * and storage; successful reads precede the possibly overlapping result store.
 */
OP(shlw)
{
    WORD n;
    UWORD value, result;

    /* Reject malformed count slots before the first operand read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n)){
        error(exBounds);
    }
    memmove(&n, R.s, sizeof(n));
    n = shiftcount(n, sizeof(WORD)*8);
    /* Count errors win; both remaining slots must fit before value access. */
    if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value) ||
       R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result)){
        error(exBounds);
    }
    memmove(&value, R.m, sizeof(value));
    result = value << n;
    memmove(R.d, &result, sizeof(result));
}
/* Shift long bits unsigned to avoid signed-left-shift overflow. */
/* Use REG.s as a borrowed WORD count, REG.m as the value and REG.d as result.
 * No ownership transfer or blocking occurs. Invalid slots raise exBounds;
 * invalid counts raise exInval before value/result access. Failure preserves REG
 * and storage; successful reads precede the possibly overlapping result store.
 */
OP(shll)
{
    WORD n;
    ULONG value, result;

    /* Reject malformed count slots before the first operand read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n)){
        error(exBounds);
    }
    memmove(&n, R.s, sizeof(n));
    n = shiftcount(n, sizeof(LONG)*8);
    /* Count errors win; both remaining slots must fit before value access. */
    if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value) ||
       R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result)){
        error(exBounds);
    }
    memmove(&value, R.m, sizeof(value));
    result = value << n;
    memmove(R.d, &result, sizeof(result));
}
/* Reject invalid byte shift counts before evaluating the shift. */
/* Use REG.s as a borrowed WORD count, REG.m as the value and REG.d as result.
 * No ownership transfer or blocking occurs. Invalid slots raise exBounds;
 * invalid counts raise exInval before value/result access. Failure preserves REG
 * and storage; successful reads precede the possibly overlapping result store.
 */
OP(shrb)
{
    WORD n;
    BYTE value, result;

    /* Reject malformed count slots before the first operand read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n)){
        error(exBounds);
    }
    memmove(&n, R.s, sizeof(n));
    n = shiftcount(n, 8);
    /* Count errors win; both remaining slots must fit before value access. */
    if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value) ||
       R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result)){
        error(exBounds);
    }
    memmove(&value, R.m, sizeof(value));
    result = value >> n;
    memmove(R.d, &result, sizeof(result));
}
/* Preserve arithmetic right shift for signed Limbo words. */
/* Use REG.s as a borrowed WORD count, REG.m as the value and REG.d as result.
 * No ownership transfer or blocking occurs. Invalid slots raise exBounds;
 * invalid counts raise exInval before value/result access. Failure preserves REG
 * and storage; successful reads precede the possibly overlapping result store.
 */
OP(shrw)
{
    WORD n;
    WORD value, result;

    /* Reject malformed count slots before the first operand read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n)){
        error(exBounds);
    }
    memmove(&n, R.s, sizeof(n));
    n = shiftcount(n, sizeof(WORD)*8);
    /* Count errors win; both remaining slots must fit before value access. */
    if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value) ||
       R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result)){
        error(exBounds);
    }
    memmove(&value, R.m, sizeof(value));
    result = value >> n;
    memmove(R.d, &result, sizeof(result));
}
/* Preserve arithmetic right shift for signed Limbo long values. */
/* Use REG.s as a borrowed WORD count, REG.m as the value and REG.d as result.
 * No ownership transfer or blocking occurs. Invalid slots raise exBounds;
 * invalid counts raise exInval before value/result access. Failure preserves REG
 * and storage; successful reads precede the possibly overlapping result store.
 */
OP(shrl)
{
    WORD n;
    LONG value, result;

    /* Reject malformed count slots before the first operand read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n)){
        error(exBounds);
    }
    memmove(&n, R.s, sizeof(n));
    n = shiftcount(n, sizeof(LONG)*8);
    /* Count errors win; both remaining slots must fit before value access. */
    if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value) ||
       R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result)){
        error(exBounds);
    }
    memmove(&value, R.m, sizeof(value));
    result = value >> n;
    memmove(R.d, &result, sizeof(result));
}
/* Validate counts before logical right shifts of unsigned word bits. */
/* Use REG.s as a borrowed WORD count, REG.m as the value and REG.d as result.
 * No ownership transfer or blocking occurs. Invalid slots raise exBounds;
 * invalid counts raise exInval before value/result access. Failure preserves REG
 * and storage; successful reads precede the possibly overlapping result store.
 */
OP(lsrw)
{
    WORD n;
    UWORD value, result;

    /* Reject malformed count slots before the first operand read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n)){
        error(exBounds);
    }
    memmove(&n, R.s, sizeof(n));
    n = shiftcount(n, sizeof(WORD)*8);
    /* Count errors win; both remaining slots must fit before value access. */
    if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value) ||
       R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result)){
        error(exBounds);
    }
    memmove(&value, R.m, sizeof(value));
    result = value >> n;
    memmove(R.d, &result, sizeof(result));
}
/* Validate counts before logical right shifts of unsigned long bits. */
/* Use REG.s as a borrowed WORD count, REG.m as the value and REG.d as result.
 * No ownership transfer or blocking occurs. Invalid slots raise exBounds;
 * invalid counts raise exInval before value/result access. Failure preserves REG
 * and storage; successful reads precede the possibly overlapping result store.
 */
OP(lsrl)
{
    WORD n;
    ULONG value, result;

    /* Reject malformed count slots before the first operand read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n)){
        error(exBounds);
    }
    memmove(&n, R.s, sizeof(n));
    n = shiftcount(n, sizeof(LONG)*8);
    /* Count errors win; both remaining slots must fit before value access. */
    if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value) ||
       R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result)){
        error(exBounds);
    }
    memmove(&value, R.m, sizeof(value));
    result = value >> n;
    memmove(R.d, &result, sizeof(result));
}
/* Capture both BYTE operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(beqb)
{
    BYTE left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left == right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both BYTE operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bneb)
{
    BYTE left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left != right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both BYTE operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bltb)
{
    BYTE left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left < right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both BYTE operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bleb)
{
    BYTE left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left <= right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both BYTE operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bgtb)
{
    BYTE left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left > right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both BYTE operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bgeb)
{
    BYTE left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left >= right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both WORD operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(beqw)
{
    WORD left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left == right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both WORD operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bnew)
{
    WORD left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left != right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both WORD operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bltw)
{
    WORD left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left < right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both WORD operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(blew)
{
    WORD left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left <= right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both WORD operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bgtw)
{
    WORD left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left > right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both WORD operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bgew)
{
    WORD left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left >= right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both LONG operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(beql)
{
    LONG left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left == right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both LONG operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bnel)
{
    LONG left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left != right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both LONG operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bltl)
{
    LONG left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left < right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both LONG operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(blel)
{
    LONG left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left <= right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both LONG operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bgtl)
{
    LONG left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left > right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Capture both LONG operands before conditionally reading the native target. */
/* Borrow REG.s/m as comparison operands and REG.d as a native target slot.
 * No blocking or ownership transfer. Bound both sources before either read;
 * only a taken predicate validates/loads its target and replaces REG.PC.
 * Malformed used slots raise exBounds without publishing PC or changing storage.
 */
OP(bgel)
{
    LONG left, right;

    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    if(left >= right){
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        JMP(d);
    }
}
/* Compare captured real values and read a branch target only when it is taken. */
/* condition selects EQ/NE/LT/LE/GT/GE (0..5); REG.s/m borrow REAL input slots
 * and REG.d borrows a native target slot. No ownership transfer or blocking.
 * Malformed used slots raise exBounds before publication; a false comparison
 * ignores its target. Successful taken predicates alone replace REG.PC.
 */
static void
branchreal(int condition)
{
    REAL left, right;
    int take;

    /* Bound both source slots before either floating-point input is read. */
    if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
       R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right)){
        error(exBounds);
    }
    memmove(&left, R.s, sizeof(left));
    memmove(&right, R.m, sizeof(right));
    take = 0;
    switch(condition){
    case 0:
        take = left == right;
        break;
    case 1:
        take = left != right;
        break;
    case 2:
        take = left < right;
        break;
    case 3:
        take = left <= right;
        break;
    case 4:
        take = left > right;
        break;
    case 5:
        take = left >= right;
        break;
    }
    if(take){
        /* A false predicate must not even validate its unused target slot. */
        if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
            error(exBounds);
        }
        memmove(&R.PC, R.d, sizeof(R.PC));
    }
}

/* Equality remains false for unordered real operands. */
OP(beqf) { branchreal(0); }
/* Inequality remains true for unordered real operands. */
OP(bnef) { branchreal(1); }
/* Ordered real less-than comparison. */
OP(bltf) { branchreal(2); }
/* Ordered real less-than-or-equal comparison. */
OP(blef) { branchreal(3); }
/* Ordered real greater-than comparison. */
OP(bgtf) { branchreal(4); }
/* Ordered real greater-than-or-equal comparison. */
OP(bgef) { branchreal(5); }
/* Capture string owners and defer the target read until the predicate is true. */
static void
branchstring(int condition)
{
	String *left, *right;
	int order, take;

	/* Both decoded owner slots must fit before either comparison input is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(left) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(right))
		error(exHeap);

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	order = stringcmp(left, right);
	take = 0;
	switch(condition){
	case 0: take = order == 0; break;
	case 1: take = order != 0; break;
	case 2: take = order < 0; break;
	case 3: take = order <= 0; break;
	case 4: take = order > 0; break;
	case 5: take = order >= 0; break;
	}
	if(take){
		/* False predicates never inspect or validate their unused target slot. */
		if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC))
			error(exHeap);
		JMP(d);
	}
}

/* Apply the captured string equality predicate without aligned pointer casts. */
OP(beqc) { branchstring(0); }
/* Apply the captured string inequality predicate without aligned pointer casts. */
OP(bnec) { branchstring(1); }
/* Apply the captured string less-than predicate without aligned pointer casts. */
OP(bltc) { branchstring(2); }
/* Apply the captured string less-or-equal predicate without aligned pointer casts. */
OP(blec) { branchstring(3); }
/* Apply the captured string greater-than predicate without aligned pointer casts. */
OP(bgtc) { branchstring(4); }
/* Apply the captured string greater-or-equal predicate without aligned pointer casts. */
OP(bgec) { branchstring(5); }
OP(iexit){ error(""); }


/* Capture the WORD source before converting into a possibly overlapping LONG slot. */
OP(cvtwl)
{
	WORD source;
	LONG result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(LONG) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture the LONG source before converting into a possibly overlapping WORD slot. */
OP(cvtlw)
{
	LONG source;
	WORD result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(WORD) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(LONG))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Capture the unaligned integer before converting with the current FP environment. */
OP(cvtlf)
{
	LONG source;
	REAL result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(LONG))
		error(exBounds);
	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}


/* Round first, then reject non-finite or out-of-range big conversions. */
OP(cvtfl)
{
	REAL f;
	LONG result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(LONG) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL))
		error(exBounds);
	memmove(&f, R.s, sizeof(f));
	f = f < 0 ? f - .5 : f + .5;
	if(!(f >= -9223372036854775808.0 && f < 9223372036854775808.0))
		error(exInval);
	result = (LONG)f;
	memmove(R.d, &result, sizeof(result));
}


/* Check the active VM word width before publishing the rounded integer. */
OP(cvtfw)
{
	REAL f;
	WORD result;

	/* Reject malformed slots before reading values or changing FP status. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(WORD) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL))
		error(exBounds);
	memmove(&f, R.s, sizeof(f));
	f = f < 0 ? f - .5 : f + .5;
	if(sizeof(WORD) == 4){
		if(!(f > -2147483649.0 && f < 2147483648.0))
			error(exInval);
	}else{
		if(!(f >= -9223372036854775808.0 && f < 9223372036854775808.0))
			error(exInval);
	}
	result = (WORD)f;
	memmove(R.d, &result, sizeof(result));
}
/* Capture the owner before parsing and publish a complete possibly unaligned big. */
OP(cvtcl)
{
	String *s;
	LONG value;

	/* Validate both slots before string2c can terminate or allocate a cached view. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(s) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(value))
		error(exHeap);

	memmove(&s, R.s, sizeof(s));
	value = strtoll(string2c(s), nil, 10);
	memmove(R.d, &value, sizeof(value));
}


/* Capture operands before computing a modular power or its negative-exponent rule. */
OP(iexpw)
{
	WORD x, n, answer;
	UWORD base, exponent, result;

	/* Both sources must fit before either potentially aliased value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(x))
		error(exBounds);

	memmove(&x, R.m, sizeof(x));
	memmove(&n, R.s, sizeof(n));
	/* Preserve zero-base error priority without inspecting the unused result. */
	if(n < 0 && x == 0)
		error(exZdiv);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(answer))
		error(exBounds);
	if(n < 0){
		if(x == 1)
			result = 1;
		else if(x == -1)
			result = n&1 ? (UWORD)-1 : 1;
		else
			result = 0;
	}else{
		base = x;
		exponent = n;
		result = 1;
		for(;;){
			if(exponent&1)
				result *= base;
			if((exponent >>= 1) == 0)
				break;
			base *= base;
		}
	}
	answer = result;
	memmove(R.d, &answer, sizeof(answer));
}


/* Big powers use the same word-sized exponent and alignment-safe operand access. */
OP(iexpl)
{
	WORD n;
	LONG x, answer;
	ULONG base, result;
	UWORD exponent;

	/* Both sources must fit before either potentially aliased value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(n) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(x))
		error(exBounds);

	memmove(&x, R.m, sizeof(x));
	memmove(&n, R.s, sizeof(n));
	/* Preserve zero-base error priority without inspecting the unused result. */
	if(n < 0 && x == 0)
		error(exZdiv);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(answer))
		error(exBounds);
	if(n < 0){
		if(x == 1)
			result = 1;
		else if(x == -1)
			result = n&1 ? (ULONG)-1 : 1;
		else
			result = 0;
	}else{
		base = x;
		exponent = n;
		result = 1;
		for(;;){
			if(exponent&1)
				result *= base;
			if((exponent >>= 1) == 0)
				break;
			base *= base;
		}
	}
	answer = result;
	memmove(R.d, &answer, sizeof(answer));
}


/* Handle the minimum negative exponent without signed negation or overflow. */
OP(iexpf)
{
	WORD n;
	UWORD exponent;
	REAL x, result;

	/* Bound every used slot before operand reads or floating-point effects. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(REAL) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL))
		error(exBounds);

	memmove(&x, R.m, sizeof(x));
	memmove(&n, R.s, sizeof(n));
	exponent = n;
	if(n < 0){
		exponent = 0-exponent;
		x = 1/x;
	}
	result = 1;
	for(;;){
		if(exponent&1)
			result *= x;
		if((exponent >>= 1) == 0)
			break;
		x *= x;
	}
	memmove(R.d, &result, sizeof(result));
}


/* Validate complete indices and byte offsets before forming an element address. */
static uchar*
arrayaddress(Array *array, WORD index, int width)
{
	ulong offset;
	WORD length;
	Type *type;
	uchar *data;

	/* Negative indices read no header; other index errors need only length. */
	if(array == H || array == nil || index < 0 ||
	   (uintptr)array > (uintptr)-1-(offsetof(Array, len)+sizeof(length)))
		error(exBounds);
	memmove(&length, (uchar*)array+offsetof(Array, len), sizeof(length));
	if(index >= length)
		error(exBounds);
	if(width < 0){
		if((uintptr)array > (uintptr)-1-(offsetof(Array, t)+sizeof(type)))
			error(exBounds);
		memmove(&type, (uchar*)array+offsetof(Array, t), sizeof(type));
		if(type == nil || type == H ||
		   (uintptr)type > (uintptr)-1-(offsetof(Type, size)+sizeof(width)))
			error(exBounds);
		memmove(&width, (uchar*)type+offsetof(Type, size), sizeof(width));
		if(width < 0)
			error(exBounds);
	}
	if((uintptr)array > (uintptr)-1-(offsetof(Array, data)+sizeof(data)))
		error(exBounds);
	memmove(&data, (uchar*)array+offsetof(Array, data), sizeof(data));
	/* Zero-width records preserve even nil/H data identities without arithmetic. */
	if(width == 0)
		return data;
	if(data == nil || data == H ||
	   (uvlong)index > 0x7fffffff/(uint)width)
		error(exBounds);
	offset = (ulong)index*width;
	if((uintptr)data > (uintptr)-1-offset)
		error(exBounds);
	return (uchar*)((uintptr)data+offset);
}


/* Capture decoded array slots before computing or publishing an aliased address. */
static void
arrayindex(int width)
{
	Array *array;
	WORD index, address;

	/* Capture sources only after all decoded slots fit, including the result. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(array) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(index) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(address))
		error(exBounds);
	memmove(&array, R.s, sizeof(array));
	memmove(&index, R.d, sizeof(index));
	address = (WORD)(uintptr)arrayaddress(array, index, width);
	memmove(R.m, &address, sizeof(address));
}


/* Resolve descriptor-sized elements after checking the array and full index. */
OP(indx)
{
	arrayindex(-1);
}


/* Preserve the active word stride without narrowing the index. */
OP(indw)
{
	arrayindex(sizeof(WORD));
}


/* Real array indexing computes an address without loading a floating value. */
OP(indf)
{
	arrayindex(sizeof(REAL));
}


/* Big array indexing retains the complete native byte offset. */
OP(indl)
{
	arrayindex(sizeof(LONG));
}


/* Byte array indexing uses the same bounds and empty-storage checks. */
OP(indb)
{
	arrayindex(sizeof(BYTE));
}


/* Validate and retain a live reference before replacing an unaligned owner slot. */
OP(movp)
{
	Heap *h;
	void *dv, *sv;

	/* Capture both slots before retain/mark can change aliased metadata. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(sv) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(dv))
		error(exHeap);
	memmove(&sv, R.s, sizeof(sv));
	if(sv == nil)
		error(exNilref);
	h = nil;
	if(sv != H){
		if((uintptr)sv <= sizeof(Heap))
			error(exHeap);
		h = D2H(sv);
		if(h->ref == 0 || h->ref == (ulong)-1)
			error(exHeap);
	}
	memmove(&dv, R.d, sizeof(dv));
	/* A bad previous owner must fail before the source is retained or published. */
	if(dv != H && ((uintptr)dv <= sizeof(Heap) || D2H(dv)->ref == 0))
		error(exHeap);
	if(h != nil){
		h->ref++;
		Setmark(h);
	}
	memmove(R.d, &sv, sizeof(sv));
	destroy(dv);
}


/* Resolve live module tables through checked native spans and unaligned-safe reads. */
static Type*
moduletype(Modlink *ml, WORD index)
{
	Module *m;
	Type **table, *type;
	uintptr address, extent, offset;
	int count, size;

	address = (uintptr)ml;
	if(ml == nil || ml == H || address > (uintptr)-1-(offsetof(Modlink, m)+sizeof(m)))
		error(exModule);
	memmove(&m, (uchar*)ml+offsetof(Modlink, m), sizeof(m));
	address = (uintptr)m;
	extent = offsetof(Module, type)+sizeof(table);
	if(extent < offsetof(Module, ntype)+sizeof(count))
		extent = offsetof(Module, ntype)+sizeof(count);
	if(m == nil || m == H || address > (uintptr)-1-extent)
		error(exModule);
	memmove(&table, (uchar*)m+offsetof(Module, type), sizeof(table));
	memmove(&count, (uchar*)m+offsetof(Module, ntype), sizeof(count));
	if(table == nil || table == H || index < 0 || index >= count ||
	   (uvlong)index > ((uintptr)-1-sizeof(type))/sizeof(type))
		error("invalid type");
	offset = (uintptr)index*sizeof(type);
	if((uintptr)table > (uintptr)-1-offset-sizeof(type))
		error("invalid type");
	memmove(&type, (uchar*)table+offset, sizeof(type));
	if(type == nil || type == H || (uintptr)type > (uintptr)-1-(offsetof(Type, size)+sizeof(size)))
		error("invalid type");
	memmove(&size, (uchar*)type+offsetof(Type, size), sizeof(size));
	if(size < 0)
		error("invalid type");
	return type;
}


/* Validate mapped metadata before payload access or reference-count changes.
 * Empty records need only their size field. Nonempty descriptors must support
 * typed ownership visitors, and every marked pointer must fit the copied bytes.
 */
static int
mappedcopytype(Type *t)
{
	uintptr address;
	int size, pointers, i, bit;

	address = (uintptr)t;
	if(t == nil || t == H || address > (uintptr)-1-(offsetof(Type, size)+sizeof(size)))
		error(exType);
	memmove(&size, (uchar*)t+offsetof(Type, size), sizeof(size));
	if(size < 0)
		error(exType);
	if(size == 0)
		return 0;
	if(address%sizeof(void*) != 0 ||
	   address > (uintptr)-1-(offsetof(Type, np)+sizeof(pointers)))
		error(exHeap);
	memmove(&pointers, (uchar*)t+offsetof(Type, np), sizeof(pointers));
	if(pointers < 0 || pointers > size/(8*sizeof(void*))+(size%(8*sizeof(void*)) != 0))
		error(exHeap);
	if(pointers != 0 && (address > (uintptr)-1 - offsetof(Type, map) ||
	   address+offsetof(Type, map) > (uintptr)-1-(uintptr)pointers))
		error(exHeap);
	for(i = 0; i < pointers; i++)
		for(bit = 0; bit < 8; bit++)
			if((t->map[i] & (0x80>>bit)) &&
			   ((ulong)i*8+bit >= (ulong)size/sizeof(void*)))
				error(exHeap);
	return size;
}


/* Resolve metadata first; empty records neither access data nor change references. */
OP(movmp)
{
	Type *t;
	WORD index;
	int size;

	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.m, sizeof(index));
	t = moduletype(R.M, index);
	size = mappedcopytype(t);
	if(size == 0)
		return;
	if(R.s == nil || R.s == H || R.d == nil || R.d == H)
		error(exNilref);
	if((uintptr)R.s > (uintptr)-1-(uintptr)size ||
	   (uintptr)R.d > (uintptr)-1-(uintptr)size)
		error(exBounds);

	incmem(R.s, t);
	if(t->np)
		freeptrs(R.d, t);
	memmove(R.d, R.s, size);
}


/* Resolve and retain the old owner until zero initialization or allocation is complete. */
static void
newrecord(Modlink *ml, WORD index, int clear)
{
	Type *type;
	Heap *h, oldheap;
	void *old, *object;
	uintptr address, extent;
	int size, pointers, references;
	void (*release)(Heap*, int);
	void (*mark)(Type*, void*);

	type = moduletype(ml, index);
	extent = offsetof(Type, np)+sizeof(type->np);
	if((uintptr)type%sizeof(void*) != 0 || (uintptr)type > (uintptr)-1-extent)
		error(exHeap);
	memmove(&size, (uchar*)type+offsetof(Type, size), sizeof(size));
	memmove(&pointers, (uchar*)type+offsetof(Type, np), sizeof(pointers));
	memmove(&references, (uchar*)type+offsetof(Type, ref), sizeof(references));
	if(references <= 0 || size < 0 || (ulong)size > 0x7fffffff-sizeof(Heap))
		error(exHeap);
	memmove(&old, R.d, sizeof(old));
	if(old == nil)
		error(exNilref);
	if(old != H){
		address = (uintptr)old;
		if(address <= sizeof(Heap))
			error(exHeap);
		memmove(&oldheap, (uchar*)old-sizeof(Heap), sizeof(oldheap));
		if(oldheap.ref == 0)
			error(exHeap);
		memmove(&release, (uchar*)type+offsetof(Type, free), sizeof(release));
		memmove(&mark, (uchar*)type+offsetof(Type, mark), sizeof(mark));
		/* Custom finalizers may own native resources even when their pointer map is empty. */
		if(oldheap.t == type && oldheap.ref == 1 && pointers == 0 && release == freeheap && mark == markheap){
			if(size != 0){
				if(address > (uintptr)-1-(uintptr)size)
					error(exHeap);
				if(clear)
					memset(old, 0, size);
			}
			/* Restore a result slot overlapping the cleared payload. */
			memmove(R.d, &old, sizeof(old));
			return;
		}
	}
	h = clear ? heapz(type) : heap(type);
	object = H2D(void*, h);
	memmove(R.d, &object, sizeof(object));
	destroy(old);
}


/* Reuse unique plain records without reading or initializing unspecified scalar bytes. */
OP(new)
{
	WORD index;

	memmove(&index, R.s, sizeof(index));
	newrecord(R.M, index, 0);
}


/* Capture the full type index before zeroing a possibly overlapping old payload. */
OP(newz)
{
	WORD index;

	memmove(&index, R.s, sizeof(index));
	newrecord(R.M, index, 1);
}


/* Resolve an imported type through its current Module table before creating an object. */
OP(mnewz)
{
	Modlink *ml;
	WORD index;

	memmove(&ml, R.s, sizeof(ml));
	if(ml == nil || ml == H)
		error(exModule);
	memmove(&index, R.m, sizeof(index));
	newrecord(ml, index, 1);
}


/* Resolve imports or named exports without narrowing the Limbo link index. */
static int
moduleentry(Modlink *ml, WORD index, Modl *entry)
{
	Link *l;

	if(ml == nil || ml == H || ml->nlinks < 0 ||
	   (ml->nlinks != 0 && ml->links == nil))
		return 0;
	if(index >= 0){
		if(index >= ml->nlinks)
			return 0;
		*entry = ml->links[index];
		return 1;
	}
	if(ml->m == nil || ml->m == H)
		return 0;
	if(ml->m->ext == H)
		return 0;
	if(ml->m->ext != nil)
		for(l = ml->m->ext; l->name != nil; l++, index++)
			if(index == -1){
				entry->u = l->u;
				entry->frame = l->frame;
				return 1;
			}
	return 0;
}


/* Compare remaining stack bytes before forming a prospective frame pointer. */
static int
framefits(Type *t)
{
	if(t == nil || t == H || t->size < (int)sizeof(Frame) || t->np < 0 ||
	   R.SP == nil || R.SP == H || R.TS == nil || R.TS == H || R.SP > R.TS)
		error(exHeap);
	return (ulong)t->size < (ulong)(R.TS-R.SP);
}

/* Validate the complete descriptor index and stack span before allocating a frame. */
OP(frame)
{
	Type *t;
	Frame *f;
	WORD index;

	memmove(&index, R.s, sizeof(index));
	t = moduletype(R.M, index);
	if(!framefits(t)) {
		R.s = t;
		extend();
		memmove(R.d, &R.s, sizeof(R.s));
		return;
	}
	f = (Frame*)R.SP;
	R.SP += t->size;
	f->t  = t;
	f->mr = nil;
	if (t->np)
		initmem(t, f);
	memmove(R.d, &f, sizeof(f));
}


/* Resolve both imported and exported frame types through checked metadata. */
OP(mframe)
{
	Type *t;
	Frame *f;
	WORD index;
	Modl entry;
	Modlink *ml;

	memmove(&ml, R.s, sizeof(ml));
	if(ml == H || ml == nil)
		error(exModule);

	memmove(&index, R.m, sizeof(index));
	if(!moduleentry(ml, index, &entry))
		error("invalid mframe");
	t = entry.frame;
	if(!framefits(t)) {
		R.s = t;
		extend();
		memmove(R.d, &R.s, sizeof(R.s));
		return;
	}
	f = (Frame*)R.SP;
	R.SP += t->size;
	f->t = t;
	f->mr = nil;
	if (t->np)
		initmem(t, f);
	memmove(R.d, &f, sizeof(f));
}


/* Validate pointer maps before allocation, then initialize scalar and pointer bytes separately. */
static void
newarray(int clear)
{
	WORD count, index;
	Type *type;
	Heap *h, header;
	Array *array, *old;
	uchar *element;
	int i;
	uintptr address;
	ulong bytes;

	memmove(&index, R.m, sizeof(index));
	type = moduletype(R.M, index);
	if((uintptr)type%sizeof(void*) != 0 ||
	   (uintptr)type > (uintptr)-1-(offsetof(Type, np)+sizeof(type->np)))
		error(exHeap);
	memmove(&count, R.s, sizeof(count));
	/* Preserve descriptor-error precedence before checking the full WORD geometry. */
	if(type->ref <= 0 || type->ref > 0x7ffffffe - (type == &Tarray) ||
	   Tarray.ref <= 0 || Tarray.ref == 0x7fffffff)
		error(exHeap);
	acheck(type->size, count);
	memmove(&old, R.d, sizeof(old));
	if(old == nil)
		error(exNilref);
	if(old != H){
		address = (uintptr)old;
		if(address <= sizeof(Heap) || address%sizeof(void*) != 0)
			error(exHeap);
		memmove(&header, (uchar*)old-sizeof(Heap), sizeof(header));
		if(header.ref == 0)
			error(exHeap);
		if(header.t == &Tarray && header.ref == 1 && type->np == 0){
			if(address > (uintptr)-1-sizeof(Array))
				error(exHeap);
			if(old->t == type && old->len == count && old->root == H){
				bytes = (ulong)type->size*count;
				if(bytes != 0){
					if((uintptr)old->data < address+sizeof(Array) ||
					   old->data == H || (uintptr)old->data > (uintptr)-1-bytes)
						error(exHeap);
					if(clear)
						memset(old->data, 0, bytes);
				}
				/* An output operand may overlap the reused scalar payload. */
				memmove(R.d, &old, sizeof(old));
				return;
			}
		}
	}
	/* heaparray checks both descriptor retains and every mapped pointer slot. */
	h = heaparray(type, (int)count);
	array = H2D(Array*, h);
	if(clear && type->size != 0 && count != 0){
		memset(array->data, 0, (ulong)type->size*count);
		/* Restore H in mapped cells without acquiring a second element-Type retain. */
		if(type->np != 0){
			element = array->data;
			for(i = 0; i < count; i++){
				initmem(type, element);
				element += type->size;
			}
		}
	}
	memmove(R.d, &array, sizeof(array));
	destroy(old);
}


/* Construct an array with unspecified scalar bytes and initialized pointer cells. */
OP(newa)
{
	newarray(0);
}


/* Construct an array with zero scalar bytes and initialized pointer cells. */
OP(newaz)
{
	newarray(1);
}

typedef struct ChannelBuild ChannelBuild;

struct ChannelBuild {
	Type *t; // Payload descriptor retained by the buffer and mapped mover.
	void (*mover)(void); // Transfer routine published after construction succeeds.
	int len; // Validated buffer element count.
	Channel *c; // Partial owner for protected cleanup on allocation failure.
};


/* Build resources only after cnewc has reserved all descriptor increments. */
static int
channelbuild(void *arg)
{
	ChannelBuild *b;
	Channel *c;

	b = arg;
	c = H2D(Channel*, heap(&Tchannel));
	b->c = c;
	/* Make the partial channel safe for freechan before any further call. */
	c->buf = H;
	c->send = nil;
	c->recv = nil;
	c->aux = nil;
	c->mover = nil;
	c->mid.w = 0;
	c->front = 0;
	c->size = 0;
	c->send = mallocz(sizeof(Progq), 1);
	if(c->send == nil)
		error(exNomem);
	c->recv = mallocz(sizeof(Progq), 1);
	if(c->recv == nil)
		error(exNomem);
	if(b->len > 0)
		c->buf = H2D(Array*, heaparray(b->t, b->len));
	if(b->mover == movtmp) {
		c->mid.t = b->t;
		b->t->ref++;
	}
	c->mover = b->mover;
	return 1;
}


/* Reserve every descriptor increment before creating any channel resource. */
Channel*
cnewc(Type *t, void (*mover)(void), int len)
{
	ChannelBuild b;
	int retains, i, bit;

	if(len < 0)
		error(exNegsize);
	if((len > 0 || mover == movtmp) && (t == nil || t == H))
		error(exHeap);
	if(len > 0)
		acheck(t->size,len);
	if(Tchannel.ref <= 0 || Tchannel.ref == 0x7fffffff ||
	   (len > 0 && (Tarray.ref <= 0 || Tarray.ref == 0x7fffffff)))
		error(exHeap);
	if(len > 0 || mover == movtmp){
		retains = (len > 0)+(mover == movtmp)+(t == &Tchannel)+
			(len > 0 && t == &Tarray);
		if(t->ref <= 0 || t->ref > 0x7fffffff-retains)
			error(exHeap);
	}
	/* An unbuffered mapped channel still retains and later interprets the payload map. */
	if(len > 0 || mover == movtmp){
		if(t->size < 0 || t->np < 0 ||
		   t->np > t->size/(8*sizeof(void*))+(t->size%(8*sizeof(void*)) != 0))
			error(exHeap);
		if(t->np != 0 && ((uintptr)t > (uintptr)-1 - offsetof(Type, map) ||
		   (uintptr)t+offsetof(Type, map) > (uintptr)-1-(uintptr)t->np))
			error(exHeap);
		for(i = 0; i < t->np; i++)
			for(bit = 0; bit < 8; bit++)
				if((t->map[i] & (0x80>>bit)) &&
				   ((ulong)i*8+bit >= (ulong)t->size/sizeof(void*)))
					error(exHeap);
	}
	b.t = t;
	b.mover = mover;
	b.len = len;
	b.c = H;
	if(!kprotect(channelbuild, &b)) {
		destroy(b.c);
		krethrow();
	}
	return b.c;
}


/* Validate capacity, output storage and replacement ownership before any resource allocation. */
static WORD
channelargs(Channel **old)
{
	WORD len;
	uintptr address;
	Heap header;

	len = 0;
	if(R.m != R.d){
		address = (uintptr)R.m;
		if(R.m == nil || R.m == H || address > (uintptr)-1-sizeof(len))
			error(exHeap);
		memmove(&len, R.m, sizeof(len));
		if(len < 0)
			error(exNegsize);
	}
	if((uvlong)len > 0x7fffffff)
		error(exHeap);
	address = (uintptr)R.d;
	if(R.d == nil || R.d == H || address > (uintptr)-1-sizeof(*old))
		error(exHeap);
	memmove(old, R.d, sizeof(*old));
	if(*old == nil)
		error(exNilref);
	if(*old != H){
		address = (uintptr)*old;
		if(address <= sizeof(Heap) || address%sizeof(void*) != 0)
			error(exHeap);
		memmove(&header, (uchar*)*old-sizeof(Heap), sizeof(header));
		if(header.ref == 0)
			error(exHeap);
	}
	return len;
}


/* Capture channel capacity and replacement ownership before fallible allocation. */
Channel*
newc(Type *t, void (*mover)(void))
{
	Channel *channel, *oldc;
	WORD len;

	len = channelargs(&oldc);
	channel = cnewc(t, mover, len);
	memmove(R.d, &channel, sizeof(channel));
	destroy(oldc);
	return channel;
}


/* Create a big-integer channel with the shared capacity and owner checks. */
OP(newcl)
{
	newc(&Tlong, movl);
}


/* Create a byte channel with the shared capacity and owner checks. */
OP(newcb)
{
	newc(&Tbyte, movb);
}


/* Create a full-WORD channel without narrowing the requested capacity. */
OP(newcw)
{
	newc(&Tword, movw);
}


/* Create a real channel while preserving the existing mover identity. */
OP(newcf)
{
	newc(&Treal, movf);
}


/* Create a pointer channel whose buffer retains the builtin pointer map. */
OP(newcp)
{
	newc(&Tptr, movp);
}


/* Select builtin scalar payloads for native constructors using the shared channel allocator. */
Channel*
cnewcscalar(int op, int len)
{
	switch(op){
	case INEWCB: return cnewc(&Tbyte, movb, len);
	case INEWCW: return cnewc(&Tword, movw, len);
	case INEWCL: return cnewc(&Tlong, movl, len);
	case INEWCF: return cnewc(&Treal, movf, len);
	case INEWCP: return cnewc(&Tptr, movp, len);
	}
	error(exInval);
	return H;
}

typedef struct RawChannel RawChannel;

struct RawChannel {
	Type *t; // Temporary descriptor retained until buffered allocation finishes.
	int len; // Validated element capacity supplied to the common allocator.
	Channel *c; // Fresh unpublished channel returned after successful construction.
};


/* Allocate raw channel resources without publishing through any VM operand. */
static int
rawchannel(void *arg)
{
	RawChannel *b;

	b = arg;
	b->c = cnewc(b->t, movm, b->len);
	return 1;
}


/* Keep a temporary raw descriptor alive across protected queue and buffer allocation. */
Channel*
cnewcraw(int size, int len)
{
	RawChannel b;
	int ok;

	if(size < 0 || len < 0)
		error(exNegsize);
	b.t = nil;
	b.len = len;
	b.c = H;
	if(len > 0){
		b.t = dtype(nil, size, nil, 0);
		if(b.t == nil)
			error(exNomem);
	}
	ok = kprotect(rawchannel, &b);
	freetype(b.t);
	if(!ok)
		krethrow();
	return b.c;
}


/* Complete raw metadata before publishing a replacement or releasing its old source owner. */
OP(newcm)
{
	WORD size, capacity;
	Channel *channel, *old;
	uintptr address;

	address = (uintptr)R.s;
	if(R.s == nil || R.s == H || address > (uintptr)-1-sizeof(size))
		error(exHeap);
	memmove(&size, R.s, sizeof(size));
	if(size < 0)
		error(exNegsize);
	if((uvlong)size > 0x7fffffff)
		error(exHeap);
	capacity = channelargs(&old);
	channel = cnewcraw((int)size, (int)capacity);
	channel->mid.w = size;
	memmove(R.d, &channel, sizeof(channel));
	destroy(old);
}


/* Create a mapped channel only after validating its payload descriptor. */
OP(newcmp)
{
	WORD index;
	uintptr address;

	address = (uintptr)R.s;
	if(R.s == nil || R.s == H || address > (uintptr)-1-sizeof(index))
		error(exHeap);
	memmove(&index, R.s, sizeof(index));
	newc(moduletype(R.M, index), movtmp);
}


/* Resolve a live mapped descriptor before allocating an unpublished channel for native callers. */
Channel*
cnewcmapped(Modlink *module, WORD index, int len)
{
	return cnewc(moduletype(module, index), movtmp, len);
}


/* Resolve a table target without forming an out-of-range Dis instruction pointer. */
static Inst*
casepc(Modlink *ml, int destination)
{
	uintptr offset, program;

	if(ml->compiled)
		return (Inst*)(uintptr)(u32int)destination;
	if(destination < 0 || destination >= ml->m->nprog || ml->m->prog == nil || ml->m->prog == H)
		error(exBounds);
	if((uvlong)destination > (uintptr)-1/sizeof(Inst))
		error(exBounds);
	offset = (uintptr)destination*sizeof(Inst);
	program = (uintptr)ml->m->prog;
	if(program > (uintptr)-1-offset)
		error(exBounds);
	return (Inst*)(program+offset);
}


/* Search declared-cell integer rows using signed 32-bit fields and unaligned reads. */
OP(icase)
{
	Modlink *ml;
	WORD value;
	uintptr table, rows, span, row;
	int count, n, half, low, high, destination;
	int header, stride;

	header = IBY2WD;
	stride = 3*IBY2WD;
	ml = R.M;
	if(ml == nil || ml == H || (uintptr)ml > (uintptr)-1-sizeof(*ml) ||
	   (!ml->compiled && (ml->m == nil || ml->m == H ||
	   (uintptr)ml->m > (uintptr)-1-sizeof(*ml->m))))
		error(exModule);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value))
		error(exBounds);
	memmove(&value, R.s, sizeof(value));
	table = (uintptr)R.d;
	if(table == 0 || R.d == H || table > (uintptr)-1-3)
		error(exBounds);
	memmove(&count, R.d, sizeof(count));
	if(count < 0 || count > (0x7fffffff-header)/stride)
		error(exBounds);
	span = header+(uintptr)count*stride;
	if(table > (uintptr)-1-span-3)
		error(exBounds);
	memmove(&destination, (void*)(table+span), sizeof(destination));
	rows = table+header;
	for(n = count; n > 0;){
		half = n>>1;
		row = rows+(uintptr)half*stride;
		memmove(&low, (void*)row, sizeof(low));
		memmove(&high, (void*)(row+IBY2WD), sizeof(high));
		if(value < low)
			n = half;
		else if(value >= high){
			rows = row+stride;
			n -= half+1;
		}else{
			memmove(&destination, (void*)(row+2*IBY2WD), sizeof(destination));
			break;
		}
	}
	R.PC = casepc(ml, destination);
}


/* Search big rows using compiler padding and fixed-width bounds, independent of host WORD. */
OP(casel)
{
	Modlink *ml;
	LONG value, low, high;
	uintptr table, rows, span, row;
	int count, n, half, destination;
	int header, stride;

	header = 2*IBY2WD;
	stride = 2*IBY2LG+2*IBY2WD;
	ml = R.M;
	if(ml == nil || ml == H || (uintptr)ml > (uintptr)-1-sizeof(*ml) ||
	   (!ml->compiled && (ml->m == nil || ml->m == H ||
	   (uintptr)ml->m > (uintptr)-1-sizeof(*ml->m))))
		error(exModule);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value))
		error(exBounds);
	memmove(&value, R.s, sizeof(value));
	table = (uintptr)R.d;
	if(table == 0 || R.d == H || table > (uintptr)-1-3)
		error(exBounds);
	memmove(&count, R.d, sizeof(count));
	if(count < 0 || count > (0x7fffffff-header)/stride)
		error(exBounds);
	span = header+(uintptr)count*stride;
	if(table > (uintptr)-1-span-3)
		error(exBounds);
	memmove(&destination, (void*)(table+span), sizeof(destination));
	rows = table+header;
	for(n = count; n > 0;){
		half = n>>1;
		row = rows+(uintptr)half*stride;
		memmove(&low, (void*)row, sizeof(low));
		memmove(&high, (void*)(row+IBY2LG), sizeof(high));
		if(value < low)
			n = half;
		else if(value >= high){
			rows = row+stride;
			n -= half+1;
		}else{
			memmove(&destination, (void*)(row+2*IBY2LG), sizeof(destination));
			break;
		}
	}
	R.PC = casepc(ml, destination);
}


/* Read one signed Dis table cell without aligned or host-WORD loads. */
static uvlong
caseccell(uintptr address)
{
	int word;
	vlong wide;

	if(IBY2WD == 4){
		memmove(&word, (void*)address, sizeof(word));
		return (uvlong)(vlong)word;
	}
	memmove(&wide, (void*)address, sizeof(wide));
	return (uvlong)wide;
}


/* Resolve a checked string table using its declared cell width and commit only the selected PC. */
OP(casec)
{
	Modlink *ml;
	String *sv, *low, *high;
	uintptr table, rows, row, span;
	uvlong count, destination;
	int n, half, comparison;

	ml = R.M;
	if(ml == nil || ml == H || (uintptr)ml > (uintptr)-1-sizeof(*ml) ||
	   (!ml->compiled && (ml->m == nil || ml->m == H ||
	   (uintptr)ml->m > (uintptr)-1-sizeof(*ml->m))))
		error(exModule);
	if((IBY2WD != 4 && IBY2WD != 8) || R.s == nil || R.s == H ||
	   (uintptr)R.s > (uintptr)-1-sizeof(sv))
		error(exBounds);
	memmove(&sv, R.s, sizeof(sv));
	table = (uintptr)R.d;
	if(table == 0 || R.d == H || table > (uintptr)-1-IBY2WD)
		error(exBounds);
	count = caseccell(table);
	if(count > (0x7fffffff-2*IBY2WD)/(3*IBY2WD) ||
	   (count != 0 && sizeof(low) > IBY2WD))
		error(exBounds);
	span = ((uintptr)count*3+2)*IBY2WD;
	if(table > (uintptr)-1-span)
		error(exBounds);
	rows = table+IBY2WD;
	destination = caseccell(table+span-IBY2WD);
	for(n = (int)count; n > 0;){
		half = n/2;
		row = rows+(uintptr)half*3*IBY2WD;
		memmove(&low, (void*)row, sizeof(low));
		comparison = stringcmp(sv, low);
		if(comparison < 0){
			n = half;
			continue;
		}
		if(comparison != 0){
			memmove(&high, (void*)(row+IBY2WD), sizeof(high));
			if(high == H || stringcmp(sv, high) > 0){
				rows = row+3*IBY2WD;
				n -= half+1;
				continue;
			}
		}
		destination = caseccell(row+2*IBY2WD);
		break;
	}
	if(ml->compiled){
		R.PC = (Inst*)(uintptr)(IBY2WD == 4 ? (u32int)destination : destination);
		return;
	}
	if(destination > 0x7fffffffULL)
		error(exBounds);
	R.PC = casepc(ml, (int)destination);
}


/* Read fixed-width Dis table entries and commit a validated PC without alignment assumptions. */
OP(igoto)
{
	Modlink *ml;
	WORD index;
	int count, destination;
	uintptr table, offset, program;

	ml = R.M;
	if(ml == nil || ml == H || (uintptr)ml > (uintptr)-1-sizeof(*ml) ||
	   (!ml->compiled && (ml->m == nil || ml->m == H ||
	   (uintptr)ml->m > (uintptr)-1-sizeof(*ml->m))))
		error(exModule);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.s, sizeof(index));
	table = (uintptr)R.d;
	if(index < 0 || table < IBY2WD || R.d == H || (uvlong)index > (uintptr)-1/IBY2WD)
		error(exBounds);
	offset = (uintptr)index*IBY2WD;
	if(table > (uintptr)-1-offset || table+offset > (uintptr)-1-(IBY2WD-1))
		error(exBounds);
	/* Legacy native-PC compilers replace the count with zero while relocating entries. */
	if(!ml->compiled){
		memmove(&count, (void*)(table-IBY2WD), sizeof(count));
		if(count <= 0 || index >= count)
			error(exBounds);
	}
	memmove(&destination, (void*)(table+offset), sizeof(destination));
	if(ml->compiled){
		R.PC = (Inst*)(uintptr)(u32int)destination;
		return;
	}
	if(destination < 0 || destination >= ml->m->nprog || ml->m->prog == nil || ml->m->prog == H)
		error(exBounds);
	if((uvlong)destination > (uintptr)-1/sizeof(Inst))
		error(exBounds);
	offset = (uintptr)destination*sizeof(Inst);
	program = (uintptr)ml->m->prog;
	if(program > (uintptr)-1-offset)
		error(exBounds);
	R.PC = (Inst*)(program+offset);
}
/* Capture both operands before frame-link stores can overwrite an aliased target. */
OP(call)
{
	Frame *f;
	Inst *target;

	memmove(&f, R.s, sizeof(f));
	if(f == nil || f == H)
		error(exNilref);
	memmove(&target, R.d, sizeof(target));
	memmove((uchar*)f+offsetof(Frame, lr), &R.PC, sizeof(f->lr));
	memmove((uchar*)f+offsetof(Frame, fp), &R.FP, sizeof(f->fp));
	R.FP = (uchar*)f;
	R.PC = target;
}
typedef struct SpawnProg SpawnProg;
struct SpawnProg {
	Modlink *module;
	Inst *pc;
	Prog initial;
};

/* Build the child's stack before newprog makes it visible to the scheduler. */
static int
spawnready(void *arg)
{
	SpawnProg *c;
	Prog *p;

	c = arg;
	c->initial.R.M = c->module;
	c->initial.R.MP = c->module->MP;
	c->initial.R.PC = c->pc;
	newstack(&c->initial);
	p = newprog(currun(), c->module);
	p->R = c->initial.R;
	return 1;
}

/* On failure the source frame still owns its arguments; release only stack storage. */
static void
spawnmodule(Modlink *ml, Inst *pc)
{
	SpawnProg c;

	memset(&c, 0, sizeof(c));
	c.module = ml;
	c.pc = pc;
	if(!kprotect(spawnready, &c)){
		free(c.initial.R.EX);
		krethrow();
	}
	unframe();
}

/* Spawn only after the frame and process can both be constructed. */
OP(spawn)
{
	Inst *target;

	if(R.M == nil || R.M == H)
		error(exModule);
	memmove(&target, R.d, sizeof(target));
	spawnmodule(R.M, target);
}

/* Validate module ownership before reading a child entry or creating its stack. */
OP(mspawn)
{
	WORD index;
	Modlink *ml;
	Modl entry;

	memmove(&ml, R.d, sizeof(ml));
	if(ml == H || ml == nil || (uintptr)ml > (uintptr)-1-sizeof(*ml) ||
	   ml->m == H || ml->m == nil || (uintptr)ml->m > (uintptr)-1-sizeof(*ml->m))
		error(exModule);
	if(ml->prog == nil)
		error(exSpawn);
	memmove(&index, R.m, sizeof(index));
	if(!moduleentry(ml, index, &entry))
		error("invalid mspawn");
	spawnmodule(ml, entry.u.pc);
}


/* Validate captured return metadata before changing registers or releasing owners. */
OP(ret)
{
	Frame *f, links;
	Modlink *m;
	Type *type;

	if(R.FP == nil || R.FP == H)
		error(exNilref);
	f = (Frame*)R.FP;
	memmove(&links, f, sizeof(links));
	if(links.fp == nil)
		error("");
	if(links.fp == H || links.lr == nil || links.lr == H)
		error(exHeap);
	m = links.mr;
	if(m == H || (m != nil && (R.M == nil || R.M == H)))
		error(exModule);
	type = links.t;
	if(type == nil)
		memmove(&type, (uchar*)SEXTYPE(f)+O(Stkext, reg.TR), sizeof(type));
	if(type == nil || type == H || type->np < 0)
		error(exHeap);
	/* A self-module return still owns both REG.M and the saved Frame.mr. */
	if(m != nil && (D2H(R.M)->ref == 0 ||
	   (m == R.M && D2H(R.M)->ref == 1)))
		error(exHeap);
	R.FP = links.fp;
	R.SP = (uchar*)f;
	R.PC = links.lr;

	if(links.t == nil)
		unextend(f);
	else if (links.t->np)
		freeptrs(f, links.t);

	if(m != nil) {
		if(R.M->compiled != m->compiled) {
			R.IC = 1;
			R.t = 1;
		}
		destroy(R.M);
		R.M = m;
		R.MP = m->MP;
	}
}
/* Resolve a full-width import index without stepping beyond the table sentinel. */
static Import*
moduleimports(Module *m, WORD index)
{
	Import **table;
	WORD i;

	if(index < 0 || m->ldt == nil || m->ldt == H)
		error(exModule);
	table = m->ldt;
	for(i = 0; ; i++){
		if(table[i] == nil || table[i] == H)
			error(exModule);
		if(i == index)
			return table[i];
	}
}


/* Validate imports before loading and retain nil data for data-free self loads. */
OP(iload)
{
	char *n;
	Import *ldt;
	Module *m;
	Modlink *ml, *t;
	uchar *mp;
	String *name;
	WORD index;
	Heap *h;

	if(R.M == H || R.M == nil || R.M->m == H || R.M->m == nil)
		error(exModule);
	m = R.M->m;
	if(!(m->rt & HASLDT))
		error("obsolete dis");
	memmove(&index, R.m, sizeof(index));
	memmove(&name, R.s, sizeof(name));
	ldt = moduleimports(m, index);
	n = string2c(name);
	if(strcmp(n, "$self") == 0) {
		mp = R.M->MP;
		if(mp == nil || m->ref <= 0 || m->ref == 0x7fffffff)
			error(exModule);
		h = nil;
		if(mp != H){
			h = D2H(mp);
			if(h->ref == 0 || h->ref == (ulong)-1)
				error(exHeap);
		}
		/* Link failure consumes only the acquired Module reference, not MP. */
		m->ref++;
		ml = linkmod(m, ldt, 0);
		if(ml != H) {
			ml->MP = mp;
			if(h != nil){
				h->ref++;
				Setmark(h);
			}
		}
	}
	else {
		m = readmod(n, lookmod(n), 1);
		ml = linkmod(m, ldt, 1);
	}

	memmove(&t, R.d, sizeof(t));
	memmove(R.d, &ml, sizeof(ml));
	destroy(t);
}


/* Validate module references and the complete entry before publishing a callee frame. */
OP(mcall)
{
	WORD index;
	Heap *h;
	Prog *p;
	Frame *f, links;
	Modlink *caller;
	Modl entry;
	Modlink *ml;

	memmove(&ml, R.d, sizeof(ml));
	if(ml == nil || ml == H || R.M == nil || R.M == H)
		error(exModule);
	memmove(&index, R.m, sizeof(index));
	if(!moduleentry(ml, index, &entry))
		error("invalid mcall");
	h = D2H(ml);
	if(h->ref == 0 || h->ref == (ulong)-1)
		error(exHeap);
	if(ml->prog == H ||
	   (ml->prog != nil && (entry.u.pc == nil || entry.u.pc == H)) ||
	   (ml->prog == nil && (entry.u.runt == nil || entry.u.runt == (void(*)(void*))H)))
		error(exModule);
	memmove(&f, R.s, sizeof(f));
	if(f == nil || f == H)
		error(exNilref);
	memmove((uchar*)f+offsetof(Frame, lr), &R.PC, sizeof(f->lr));
	memmove((uchar*)f+offsetof(Frame, fp), &R.FP, sizeof(f->fp));
	memmove((uchar*)f+offsetof(Frame, mr), &R.M, sizeof(f->mr));

	caller = R.M;
	R.FP = (uchar*)f;
	R.M = ml;
	h->ref++;

	if(ml->prog == nil) {
		entry.u.runt(f);
		h->ref--;
		memmove(&links, f, sizeof(links));
		R.M = links.mr;
		R.SP = R.FP;
		R.FP = links.fp;
		if(links.t == nil)
			unextend(f);
		else if (links.t->np)
			freeptrs(f, links.t);
		p = currun();
		if(p->kill != nil)
			error(p->kill);
		R.t = 0;
		return;
	}
	R.MP = R.M->MP;
	R.PC = entry.u.pc;
	R.t = 1;

	if(caller->compiled != R.M->compiled)
		R.IC = 1;
}


/* Capture and publish array lengths without aligned operand casts. */
OP(lena)
{
	WORD l;
	Array *a;

	/* Length queries need only their owner/result slots and the length field. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(a) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(l))
		error(exBounds);
	memmove(&a, R.s, sizeof(a));
	l = 0;
	if(a != H && a != nil){
		if((uintptr)a > (uintptr)-1-(offsetof(Array, len)+sizeof(l)))
			error(exBounds);
		memmove(&l, (uchar*)a+offsetof(Array, len), sizeof(l));
	}
	memmove(R.d, &l, sizeof(l));
}


/* Detect cycles and bound the count before publishing an alignment-safe list length. */
OP(lenl)
{
	UWORD count;
	WORD result;
	List *a, *checkpoint;

	/* Neither the scan nor an empty-list result may use malformed slots. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(a))
		error(exBounds);
	memmove(&a, R.s, sizeof(a));
	checkpoint = a;
	count = 0;
	while(a != H){
		if(a == nil)
			error(exNilref);
		if(count == ((UWORD)~0>>1))
			error(exBounds);
		if((uintptr)a > (uintptr)-1-(offsetof(List, tail)+sizeof(a->tail)))
			error(exBounds);
		count++;
		memmove(&a, (uchar*)a+offsetof(List, tail), sizeof(a));
		if(a == checkpoint)
			error(exHeap);
		/* Brent checkpoints at counts 1, 3, 7, ... require no extra link reads. */
		if((count & (count+1)) == 0)
			checkpoint = a;
	}
	result = count;
	memmove(R.d, &result, sizeof(result));
}
/* Validate and retain buffered storage before changing its ring or destination. */
static int
cgetb(Channel *c, void *v)
{
	Array *a;
	Heap *h;
	void *w;

	if((a = c->buf) == H)
		return 0;
	if(a == nil)
		error(exNilref);
	if(c->size > 0){
		h = D2H(a);
		if(h->ref == 0 || h->ref == (ulong)-1)
			error(exHeap);
		h->ref++;
		w = a->data+(ulong)c->front*a->t->size;
		c->front++;
		if(c->front == c->buf->len)
			c->front = 0;
		c->size--;
		R.s = w;
		R.m = &c->mid;
		R.d = v;
		c->mover();
		if(a->t->np){
			freeptrs(w, a->t);
			initmem(a->t, w);
		}
		destroy(a);
		return 1;
	}
	return 0;
}
/* Compute wrapped indexes without signed sums and use native-width byte offsets. */
static int
cputb(Channel *c, void *v)
{
	Array *a;
	WORD len, r;

	if((a = c->buf) == H)
		return 0;
	if(a == nil)
		error(exNilref);
	len = a->len;
	if(c->size < len){
		/* Subtract the distance to the end before adding an occupied prefix. */
		if(c->front >= len-c->size)
			r = c->front-(len-c->size);
		else
			r = c->front+c->size;
		c->size++;
		R.s = v;
		R.m = &c->mid;
		R.d = a->data+(ulong)r*a->t->size;
		c->mover();
		return 1;
	}
	return 0;
}
/*
int
cqsize(Progq *q)
{
	int n;

	n = 0;
	for( ; q != nil; q = q->next)
		if(q->prog != nil)
			n++;
	return n;
}
*/
void
cqadd(Progq **q, Prog *p)
{
	Progq *n;

	if((*q)->prog == nil){
		(*q)->prog = p;
		return;
	}
	n = (Progq*)malloc(sizeof(Progq));
	if(n == nil)
		error(exNomem);
	n->prog = p;
	n->next = nil;
	for( ; *q != nil; q = &(*q)->next)
		;
	*q = n;
}
void
cqdel(Progq **q)
{
	Progq *f;

	if((*q)->next == nil){
		(*q)->prog = nil;
		return;
	}
	f = *q;
	*q = f->next;
	free(f);
}
/* Remove every registration while retaining the queue's mandatory empty node. */
void
cqdelp(Progq **q, Prog *p)
{
	Progq *f;
	Progq **head;

	head = q;
	while(*q != nil){
		if((*q)->prog != p){
			q = &(*q)->next;
			continue;
		}
		if((*q)->next == nil){
			/* Only an entirely empty queue needs a placeholder node. */
			if(q == head)
				(*q)->prog = nil;
			else{
				free(*q);
				*q = nil;
			}
			return;
		}
		f = *q;
		*q = f->next;
		free(f);
	}
}
/* Queue a blocked sender before removing it from the runnable scheduler list. */
OP(isend)
{
	Channel *c;
 	Prog *p;

	memmove(&c, R.d, sizeof(c));
	if(c == H || c == nil)
		error(exNilref);

	if((p = c->recv->prog) == nil) {
		if(c->buf != H && cputb(c, R.s))
			return;
		p = currun();
		cqadd(&c->send, p);
		p = delrun(Psend);
		p->ptr = R.s;
		p->chan = c;	/* for killprog */
		R.IC = 1;	
		R.t = 1;
		return;
	}

	if(c->buf != H && c->size > 0)
		print("non-empty buffer in isend\n");

	cqdel(&c->recv);
	if(p->state == Palt)
		altdone(p->R.s, p, c, 1);

	R.m = &c->mid;
	R.d = p->ptr;
	p->ptr = nil;
	c->mover();
	addrun(p);
	R.t = 0;
}
/* Queue blocked receivers safely and retain buffered channels through refill. */
/* Validate buffered temporary owners before dequeuing or refilling a sender. */
OP(irecv)
{
	Channel *c;
	Prog *p;
	Heap *h;

	memmove(&c, R.s, sizeof(c));
	if(c == H || c == nil)
		error(exNilref);

	if((p = c->send->prog) == nil) {
		if(c->buf != H && cgetb(c, R.d))
			return;
		p = currun();
		cqadd(&c->recv, p);
		p = delrun(Precv);
		p->ptr = R.d;
		p->chan = c;	/* for killprog */
		R.IC = 1;
		R.t = 1;
		return;
	}

	/* Resolve temporary owner failures before removing or selecting a queued sender. */
	if(c->buf != H){
		if(c->buf == nil)
			error(exNilref);
		h = D2H(c);
		if(h->ref == 0 || h->ref == (ulong)-1)
			error(exHeap);
		if(c->size > 0){
			h = D2H(c->buf);
			if(h->ref == 0 || h->ref == (ulong)-1)
				error(exHeap);
		}
	}

	if(c->buf != H && c->size != c->buf->len)
		print("non-full buffer in irecv\n");

	cqdel(&c->send);
	if(p->state == Palt)
		altdone(p->R.s, p, c, 0);

	if(c->buf != H){
		/* The destination can release c; keep it alive through sender refill. */
		D2H(c)->ref++;
		cgetb(c, R.d);
		cputb(c, p->ptr);
		p->ptr = nil;
		destroy(c);
	}
	else{
		R.m = &c->mid;
		R.s = p->ptr;
		p->ptr = nil;
		c->mover();
	}
	addrun(p);
	R.t = 0;
}
/* Best-effort native sends never block or allocate a nonpositive buffer. */
int
csendalt(Channel *c, void *ip, Type *t, int len)
{
	REG rsav;

	if(c == H || c == nil || c->buf == nil)
		error(exNilref);

	if(c->recv->prog == nil && (c->buf == H || c->size == c->buf->len)){
		if(c->buf != H || len <= 0){
			freeptrs(ip, t);
			return 0;
		}
		c->buf = H2D(Array*, heaparray(t, len));
	}

	rsav = R;
	R.s = ip;
	R.d = &c;
	isend();
	R = rsav;
	freeptrs(ip, t);
	return 1;
}


/* Preflight descriptor retention and tail ownership before allocating a node. */
List*
cons(ulong size, void *slot)
{
	Heap *h, header;
	uintptr address;
	List *lv, *l;

	if(Tlist.ref <= 0 || Tlist.ref == 0x7fffffff ||
	   size > 0x7fffffff-sizeof(Heap)-
	   (sizeof(List)-sizeof(((List*)0)->data)))
		error(exHeap);
	address = (uintptr)slot;
	if(slot == nil || slot == H || address > (uintptr)-1-sizeof(lv))
		error(exHeap);
	memmove(&lv, slot, sizeof(lv));
	if(lv == nil)
		error(exNilref);
	if(lv != H){
		address = (uintptr)lv;
		if(address <= sizeof(Heap) || address%sizeof(void*) != 0)
			error(exHeap);
		memmove(&header, (uchar*)lv-sizeof(Heap), sizeof(header));
		if(header.ref == 0)
			error(exHeap);
	}
	h = nheap(sizeof(List) + size - sizeof(((List*)0)->data));
	h->t = &Tlist;
	Tlist.ref++;
	l = H2D(List*, h);
	l->t = nil;

	if(lv != H) {
		h = D2H(lv);
		Setmark(h);
	}
	l->tail = lv;
	memmove(slot, &l, sizeof(l));
	return l;
}

/* Capture source values before publishing a node into a possibly aliased slot. */
OP(consb)
{
	BYTE value;
	List *l;

	memmove(&value, R.s, sizeof(value));
	l = cons(IBY2WD, R.d);
	memmove(l->data, &value, sizeof(value));
}

/* Host WORD storage must fit the complete interpreter value. */
OP(consw)
{
	WORD value;
	List *l;

	memmove(&value, R.s, sizeof(value));
	l = cons(sizeof(value), R.d);
	memmove(l->data, &value, sizeof(value));
}

/* Retain the source big integer if publishing the list changes its source slot. */
OP(consl)
{
	LONG value;
	List *l;

	memmove(&value, R.s, sizeof(value));
	l = cons(sizeof(value), R.d);
	memmove(l->data, &value, sizeof(value));
}


/* Reject dead or saturated source owners before allocating or publishing a node. */
OP(consp)
{
	List *l;
	Heap *h, header;
	uintptr address;
	WORD *sv;

	memmove(&sv, R.s, sizeof(sv));
	if(sv == nil)
		error(exNilref);
	if(sv != H){
		address = (uintptr)sv;
		if(address <= sizeof(Heap) || address%sizeof(void*) != 0)
			error(exHeap);
		memmove(&header, (uchar*)sv-sizeof(Heap), sizeof(header));
		if(header.ref == 0 || header.ref == (ulong)-1)
			error(exHeap);
	}
	if(Tptr.ref <= 0 || Tptr.ref == 0x7fffffff)
		error(exHeap);
	l = cons(sizeof(sv), R.d);
	if(sv != H){
		h = D2H(sv);
		h->ref++;
		Setmark(h);
	}
	l->t = &Tptr;
	Tptr.ref++;
	memmove(l->data, &sv, sizeof(sv));
}

/* Capture the real argument before the destination slot becomes a list owner. */
OP(consf)
{
	REAL value;
	List *l;

	memmove(&value, R.s, sizeof(value));
	l = cons(sizeof(value), R.d);
	memmove(l->data, &value, sizeof(value));
}


/* Check the complete VM size and copy raw payload before replacing its source slot. */
OP(consm)
{
	WORD size;
	uintptr address;
	List *l, *result;

	memmove(&size, R.m, sizeof(size));
	if(size < 0 || (uvlong)size > 0x7fffffff-sizeof(Heap)-
	   (sizeof(List)-sizeof(((List*)0)->data)))
		error(exHeap);
	address = (uintptr)R.d;
	if(R.d == nil || R.d == H || address > (uintptr)-1-sizeof(result))
		error(exHeap);
	if(size != 0){
		address = (uintptr)R.s;
		if(R.s == nil || R.s == H || address > (uintptr)-1-(uintptr)size)
			error(exHeap);
	}
	memmove(&result, R.d, sizeof(result));
	l = cons((ulong)size, &result);
	if(size != 0)
		memmove(l->data, R.s, (int)size);
	memmove(R.d, &result, sizeof(result));
}


/* Resolve a list descriptor and reserve its payload retain before reading any map byte. */
Type*
cconsmappedtype(Modlink *module, WORD index)
{
	Type *t;
	uintptr address;
	int i, bit;

	t = moduletype(module, index);
	address = (uintptr)t;
	if(address%sizeof(void*) != 0 ||
	   address > (uintptr)-1-(offsetof(Type, np)+sizeof(t->np)))
		error(exHeap);
	if(t->ref <= 0 || t->ref > 0x7ffffffe - (t == &Tlist) ||
	   (ulong)t->size > 0x7fffffff-sizeof(Heap)-
	   (sizeof(List)-sizeof(((List*)0)->data)) || t->np < 0 ||
	   t->np > t->size/(8*sizeof(void*))+(t->size%(8*sizeof(void*)) != 0))
		error(exHeap);
	if(t->np != 0 && (address > (uintptr)-1 - offsetof(Type, map) ||
	   address+offsetof(Type, map) > (uintptr)-1-(uintptr)t->np))
		error(exHeap);
	for(i = 0; i < t->np; i++)
		for(bit = 0; bit < 8; bit++)
			if((t->map[i] & (0x80>>bit)) &&
			   ((ulong)i*8+bit >= (ulong)t->size/sizeof(void*)))
				error(exHeap);
	return t;
}

typedef struct ConsMapped ConsMapped;

struct ConsMapped {
	Type *type; // Live module descriptor retained by a completed node.
	void *source; // Borrowed payload remains unchanged until output publication.
	List *tail; // Existing output owner is transferred only after all head retains succeed.
	List *node; // Partial owner released by the protected caller on a VM error.
};


/* Acquire mapped heads into an unpublished, safely destructible node one field at a time. */
static int
consmapped(void *arg)
{
	ConsMapped *b;
	Type *t;
	List *l, *tail;
	Heap *h, header;
	void *owner;
	uintptr address;
	ulong offset;
	int i, bit;

	b = arg;
	t = b->type;
	tail = b->tail;
	l = cons(t->size, &tail);
	b->node = l;
	/* The output still owns the old tail; rollback must never release it. */
	l->tail = H;
	if(t->size != 0)
		memmove(l->data, b->source, t->size);
	initmem(t, l->data);
	l->t = t;
	t->ref++;
	for(i = 0; i < t->np; i++){
		for(bit = 0; bit < 8; bit++){
			if(!(t->map[i] & (0x80>>bit)))
				continue;
			offset = ((ulong)i*8+bit)*sizeof(owner);
			memmove(&owner, (uchar*)b->source+offset, sizeof(owner));
			if(owner == H)
				continue;
			address = (uintptr)owner;
			if(address <= sizeof(Heap) || address%sizeof(void*) != 0)
				error(exHeap);
			memmove(&header, (uchar*)owner-sizeof(Heap), sizeof(header));
			if(header.ref == 0 || header.ref == (ulong)-1)
				error(exHeap);
			h = D2H(owner);
			h->ref++;
			Setmark(h);
			memmove((uchar*)l->data+offset, &owner, sizeof(owner));
		}
	}
	/* No fallible work remains: transfer the old slot's tail reference without retaining twice. */
	l->tail = b->tail;
	return 1;
}


/* Preserve the destination, tail and all head references if mapped construction fails. */
OP(consmp)
{
	ConsMapped b;
	WORD index;
	uintptr address;
	void *slot;

	address = (uintptr)R.m;
	if(R.m == nil || R.m == H || address > (uintptr)-1-sizeof(index))
		error(exHeap);
	memmove(&index, R.m, sizeof(index));
	b.type = cconsmappedtype(R.M, index);
	slot = R.d;
	address = (uintptr)slot;
	if(slot == nil || slot == H || address > (uintptr)-1-sizeof(b.tail))
		error(exHeap);
	b.source = R.s;
	if(b.type->size != 0){
		address = (uintptr)b.source;
		if(b.source == nil || b.source == H || address > (uintptr)-1-(uintptr)b.type->size)
			error(exHeap);
	}
	memmove(&b.tail, slot, sizeof(b.tail));
	b.node = H;
	if(!kprotect(consmapped, &b)){
		destroy(b.node);
		krethrow();
	}
	memmove(slot, &b.node, sizeof(b.node));
}


/* Capture a possibly four-byte-aligned owner before reading list storage. */
static List*
listhead(void)
{
	List *l;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(l))
		error(exHeap);

	memmove(&l, R.s, sizeof(l));
	if(l == H || l == nil)
		error(exNilref);
	return l;
}


/* A nil list has no head payload. */
OP(headb)
{
	List *l;

	/* Validate the result before consulting potentially invalid list storage. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(BYTE))
		error(exBounds);
	l = listhead();
	if((uintptr)l > (uintptr)-1-(offsetof(List, data)+sizeof(BYTE)))
		error(exBounds);
	memmove(R.d, (uchar*)l+offsetof(List, data), sizeof(BYTE));
}


/* Reject nil before reading a word list head. */
OP(headw)
{
	List *l;

	/* Validate the result before consulting potentially invalid list storage. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	l = listhead();
	if((uintptr)l > (uintptr)-1-(offsetof(List, data)+sizeof(WORD)))
		error(exBounds);
	memmove(R.d, (uchar*)l+offsetof(List, data), sizeof(WORD));
}


/* Reject nil before reading a long list head. */
OP(headl)
{
	List *l;

	/* Validate the result before consulting potentially invalid list storage. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(LONG))
		error(exBounds);
	l = listhead();
	if((uintptr)l > (uintptr)-1-(offsetof(List, data)+sizeof(LONG)))
		error(exBounds);
	memmove(R.d, (uchar*)l+offsetof(List, data), sizeof(LONG));
}


/* Validate the list before copying its pointer-valued head. */
OP(headp)
{
	List *l;

	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(void*))
		error(exHeap);

	l = listhead();
	if((uintptr)l > (uintptr)-1-(offsetof(List, data)+sizeof(void*)))
		error(exBounds);
	R.s = l->data;
	movp();
}


/* Reject nil before reading a real-valued list head. */
OP(headf)
{
	List *l;

	/* Validate the result before consulting potentially invalid list storage. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL))
		error(exBounds);
	l = listhead();
	if((uintptr)l > (uintptr)-1-(offsetof(List, data)+sizeof(REAL)))
		error(exBounds);
	memmove(R.d, (uchar*)l+offsetof(List, data), sizeof(REAL));
}


/* Validate the list, payload address and destination span before raw head copying. */
OP(headm)
{
	List *l;
	WORD n;
	uintptr source, offset;

	l = listhead();
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(n))
		error(exBounds);
	memmove(&n, R.m, sizeof(n));
	if(n < 0)
		error(exBounds);
	if(n != 0){
		if(R.d == nil || R.d == H)
			error(exNilref);
		/* Form the payload address only after checking the native header offset. */
		source = (uintptr)l;
		offset = offsetof(List, data);
		if(source > (uintptr)-1-offset)
			error(exBounds);
		source += offset;
		if(source > (uintptr)-1-(uintptr)n || (uintptr)R.d > (uintptr)-1-(uintptr)n)
			error(exBounds);
		memmove(R.d, (void*)source, n);
	}
}


/* Validate head storage before retaining its owner or publishing its payload. */
OP(headmp)
{
	List *l;
	Type *t;
	Heap *h;
	WORD index;
	uintptr source;
	int size;

	l = listhead();
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.m, sizeof(index));
	t = moduletype(R.M, index);
	size = mappedcopytype(t);
	if(size == 0)
		return;
	if(R.d == nil || R.d == H)
		error(exNilref);
	source = (uintptr)l;
	if(source > (uintptr)-1 - offsetof(List, data) ||
	   source+offsetof(List, data) > (uintptr)-1-(uintptr)size ||
	   (uintptr)R.d > (uintptr)-1-(uintptr)size)
		error(exBounds);
	if(source <= sizeof(Heap) || source%sizeof(void*) != 0)
		error(exHeap);

	/* All fallible address/map checks precede the temporary retention. */
	h = D2H(l);
	if(h->ref == 0 || h->ref == (ulong)-1)
		error(exHeap);
	h->ref++;
	R.s = (void*)(source+offsetof(List, data));
	incmem(R.s, t);
	if(t->np)
		freeptrs(R.d, t);
	memmove(R.d, R.s, size);
	destroy(l);
}


/* The tail operation must raise a VM nil-reference error on an empty list. */
OP(tail)
{
	List *l;

	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(void*))
		error(exHeap);

	l = listhead();
	if((uintptr)l > (uintptr)-1-(offsetof(List, tail)+sizeof(void*)))
		error(exBounds);
	R.s = &l->tail;
	movp();
}
/* Bound slice byte geometry before forming a pointer, including empty records. */
static uchar*
arrayspan(Array *array, Type *type, WORD start, WORD count)
{
	ulong offset, bytes, limit;

	if(type == nil || type == H || type->size < 0 || start < 0 || count < 0)
		error(exBounds);
	if(type->size == 0)
		return array->data;
	limit = 0x7fffffff/(uint)type->size;
	if((uvlong)start > limit || (uvlong)count > limit-(ulong)start)
		error(exBounds);
	offset = (ulong)start*type->size;
	bytes = (ulong)count*type->size;
	if(offset == 0 && bytes == 0)
		return array->data;
	if(array->data == nil || array->data == H ||
	   (uintptr)array->data > (uintptr)-1-offset ||
	   (uintptr)array->data+offset > (uintptr)-1-bytes)
		error(exBounds);
	return (uchar*)((uintptr)array->data+offset);
}


/* Validate spans and backing lifetimes before allocating or publishing a view. */
OP(slicea)
{
	Type *t;
	Heap *h;
	Array *at, *ss, *ds, *root;
	WORD v, n, start;
	uchar *data;

	memmove(&v, R.m, sizeof(v));
	memmove(&start, R.s, sizeof(start));
	memmove(&ds, R.d, sizeof(ds));

	if(ds == H || ds == nil){
		if(v == start){
			if(start == 0)
				return;
			error(exBounds);
		}
		error(exNilref);
	}
	if(start < 0 || v < start || v > ds->len)
		error(exBounds);
	n = v-start;

	t = ds->t;
	data = arrayspan(ds, t, start, n);
	root = ds->root == H ? ds : ds->root;
	if(root == nil)
		error(exNilref);
	if(D2H(ds)->ref == 0)
		error(exHeap);
	h = D2H(root);
	if(h->ref == 0)
		error(exHeap);
	if(t->ref <= 0)
		error(exHeap);
	/* A unique view or pointer-free root can narrow without retiring elements. */
	if(D2H(ds)->ref == 1 && (ds->root != H || t->np == 0)){
		ds->len = n;
		ds->data = data;
		return;
	}
	if(ds->root != H && h->ref == (ulong)-1)
		error(exHeap);
	if(t->ref > 0x7ffffffe - (t == &Tarray) ||
	   Tarray.ref <= 0 || Tarray.ref == 0x7fffffff)
		error(exHeap);
	h = heap(&Tarray);
	ss = H2D(Array*, h);
	ss->len = n;
	ss->data = data;
	ss->t = t;
	ss->root = root;
	t->ref++;

	h = D2H(root);
	if(ds->root != H) {
		h->ref++;
		memmove(&at, R.d, sizeof(at));
		memmove(R.d, &ss, sizeof(ss));
		destroy(at);
	}
	else
		memmove(R.d, &ss, sizeof(ss));
	Setmark(h);
}
/* Validate spans and temporary ownership before copying mapped array elements. */
OP(slicela)
{
	Type *t;
	Heap *h;
	int l;
	WORD dl;
	Array *ss, *ds;
	uchar *sp, *dp, *ep, *source, *destination;

	memmove(&ss, R.s, sizeof(ss));
	memmove(&dl, R.m, sizeof(dl));
	memmove(&ds, R.d, sizeof(ds));
	if(ss == H || ss == nil)
		return;
	if(ds == H || ds == nil)
		error(exNilref);
	if(dl < 0 || dl > ds->len || ss->len < 0 || ss->len > ds->len-dl)
		error(exBounds);
	if(ss->len == 0)
		return;

	t = ds->t;
	source = arrayspan(ss, t, 0, ss->len);
	destination = arrayspan(ds, t, dl, ss->len);
	if(t->size == 0)
		return;
	if(t->np == 0) {
		memmove(destination, source, ss->len*t->size);
		return;
	}
	/* A destination element can own the source array or its backing view. */
	h = D2H(ss);
	if(h->ref == 0 || h->ref == (ulong)-1)
		error(exHeap);
	h->ref++;
	sp = source;
	dp = destination;

	if((uintptr)dp > (uintptr)sp) {
		l = ss->len * t->size;
		sp = ss->data + l;
		ep = dp + l;
		while(ep > dp) {
			ep -= t->size;
			sp -= t->size;
			incmem(sp, t);
			if (t->np)
				freeptrs(ep, t);
		}
	}
	else {
		ep = dp + ss->len*t->size;
		while(dp < ep) {
			incmem(sp, t);
			if (t->np)
				freeptrs(dp, t);
			dp += t->size;
			sp += t->size;
		}
	}
	memmove(destination, source, ss->len*t->size);
	destroy(ss);
}
OP(alt)
{
	R.t = 0;
	xecalt(1);
}
OP(nbalt)
{
	xecalt(0);
}


/* Compare runtime Type fields after checking their backward native spans.
 * [GC:] This borrows immutable Type identities and never retains either owner. */
OP(tcmp)
{
	void *s;
	void *d;
	Type *source;
	Type *destination;
	uintptr distance;

	/* Bound both captured owner slots before inspecting immutable Type fields. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(s) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(d)){
		error(exTcheck);
	}
	memmove(&s, R.s, sizeof(s));
	memmove(&d, R.d, sizeof(d));
	if(s == H){
		return;
	}
	distance = sizeof(Heap) - offsetof(Heap, t);
	if((uintptr)s <= distance || (uintptr)d <= distance || d == H){
		error(exTcheck);
	}
	/* Form only the field address, not an underflowing synthetic Heap pointer. */
	memmove(&source, (uchar*)s-distance, sizeof(source));
	memmove(&destination, (uchar*)d-distance, sizeof(destination));
	if(source != destination){
		error(exTcheck);
	}
}
OP(eclr)
{
	/* spare slot */
}
OP(badop)
{
	error(exOp);
}


/* Validate the owner and prepare its message before publishing an exception value. */
OP(iraise)
{
	void *v;
	String *message;
	char *text;
	Heap *h;
	Prog *p;

	memmove(&v, R.s, sizeof(v));
	if(v == H || v == nil)
		error(exNilref);
	h = D2H(v);
	/* Reserve the exception owner before message preparation can allocate or write a cache. */
	if(h->ref == 0 || h->ref == (ulong)-1)
		error(exHeap);
	if(h->t == &Tstring)
		message = v;
	else{
		if(h->t == nil || h->t == H || h->t->size < (int)sizeof(message))
			error(exType);
		memmove(&message, v, sizeof(message));
	}
	if(message != H && message != nil){
		if(D2H(message)->t != &Tstring)
			error(exType);
		if(D2H(message)->ref == 0)
			error(exHeap);
	}
	text = string2c(message);
	p = currun();
	h->ref++;
	p->exval = v;
	error(text);
}


/* Read a full VM frame word without arithmetic on nil frame identities. */
static WORD
fixedword(int offset)
{
	WORD value;

	if(R.FP == nil || R.FP == H)
		error(exNilref);
	/* Validate the complete field before forming its native address. */
	if(offset < 0 || (uintptr)R.FP > (uintptr)-1-(uintptr)offset-sizeof(value))
		error(exBounds);
	memmove(&value, R.FP+offset, sizeof(value));
	return value;
}

/* Shift a fixed intermediate with bounded counts and unsigned left-shift bits. */
static LONG
fixedshift(LONG value, WORD count)
{
	UWORD amount;

	if(count >= 0){
		if((UWORD)count > 63)
			error(exInval);
		return (LONG)((uvlong)value<<count);
	}
	amount = (UWORD)0-(UWORD)count;
	if(amount > 63)
		error(exInval);
	return value>>amount;
}

/* Divide without a host zero fault or signed minimum/-1 overflow. */
static LONG
fixeddivide(LONG value, LONG divisor)
{
	if(divisor == 0)
		error(exZdiv);
	if(divisor == -1)
		return (LONG)((uvlong)0-(uvlong)value);
	return value/divisor;
}

/* Construct a right-shift rounding mask only for a valid negative exponent. */
static LONG
fixedroundmask(WORD count)
{
	UWORD amount;

	amount = (UWORD)0-(UWORD)count;
	if(count >= 0 || amount > 63)
		error(exInval);
	return (LONG)(((uvlong)1<<amount)-1);
}


/* Multiply and shift fixed values using defined intermediate bit arithmetic. */
OP(mulx)
{
	WORD p, x, y, result;
	LONG r;

	p = fixedword((NREG+2)*IBY2WD);
	if(p < -63 || p > 63)
		error(exInval);
	/* Bound both borrowed source slots before either scalar value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);
	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	r = (LONG)((uvlong)(LONG)x*(uvlong)(LONG)y);
	r = fixedshift(r, p);
	result = (WORD)r;
	memmove(R.d, &result, sizeof(result));
}


/* Shift and divide a fixed numerator with VM errors on invalid parameters. */
OP(divx)
{
	WORD p, value, divisor, result;
	LONG s;

	p = fixedword((NREG+2)*IBY2WD);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(divisor))
		error(exBounds);
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	/* A zero divisor wins before the dividend/result slots are inspected. */
	if(p < -63 || p > 63)
		error(exInval);
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(value))
		error(exBounds);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);
	memmove(&value, R.m, sizeof(value));
	s = fixedshift((LONG)value, p);
	s = fixeddivide(s, (LONG)divisor);
	result = (WORD)s;
	memmove(R.d, &result, sizeof(result));
}


/* Capture unaligned fixed operands and publish only after validating the scale. */
OP(cvtxx)
{
	WORD p, value, result;
	LONG r;

	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(p))
		error(exBounds);
	memmove(&p, R.m, sizeof(p));
	/* Invalid scales decline before the potentially inaccessible input. */
	if(p < -63 || p > 63)
		error(exInval);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value))
		error(exBounds);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);

	memmove(&value, R.s, sizeof(value));
	r = fixedshift((LONG)value, p);
	result = (WORD)r;
	memmove(R.d, &result, sizeof(result));
}


/* Validate the coefficient before a zero shortcut and stage the product. */
OP(mulx0)
{
	WORD result, x, y, p, a;
	LONG r;

	/* Bound both borrowed source slots before either scalar value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	p = fixedword((NREG+2)*IBY2WD);
	a = fixedword(NREG*IBY2WD);
	if(a == 0)
		error(exZdiv);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);

	if(x == 0 || y == 0){
		result = 0;
		memmove(R.d, &result, sizeof(result));
		return;
	}
	r = (LONG)((uvlong)(LONG)x*(uvlong)(LONG)y);
	r = fixedshift(r, p);
	r = fixeddivide(r, (LONG)a);
	result = (WORD)r;
	memmove(R.d, &result, sizeof(result));
}


/* Reject zero divisors even when the fixed numerator is zero. */
OP(divx0)
{
	WORD result, x, y, p, b;
	LONG s;

	/* Bound both borrowed source slots before either scalar value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	p = fixedword((NREG+2)*IBY2WD);
	b = fixedword(NREG*IBY2WD);
	if(y == 0)
		error(exZdiv);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);

	if(x == 0){
		result = 0;
		memmove(R.d, &result, sizeof(result));
		return;
	}
	s = (LONG)((uvlong)(LONG)b*(uvlong)(LONG)x);
	s = fixedshift(s, p);
	s = fixeddivide(s, (LONG)y);
	result = (WORD)s;
	memmove(R.d, &result, sizeof(result));
}


/* Scale a fixed value before publishing the divided result. */
OP(cvtxx0)
{
	WORD result, x, p, a;
	LONG r;

	/* Bound both borrowed source slots before either scalar value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&x, R.s, sizeof(x));
	memmove(&p, R.m, sizeof(p));
	a = fixedword(NREG*IBY2WD);
	if(a == 0)
		error(exZdiv);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);

	if(x == 0){
		result = 0;
		memmove(R.d, &result, sizeof(result));
		return;
	}
	r = (LONG)x;
	r = fixedshift(r, p);
	r = fixeddivide(r, (LONG)a);
	result = (WORD)r;
	memmove(R.d, &result, sizeof(result));
}


/* Apply fixed multiplication rounding with bounded masks and modular corrections. */
OP(mulx1)
{
	WORD result, x, y, p, a, v;
	int vnz, wnz;
	LONG w, r;

	/* Bound both borrowed source slots before either scalar value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	p = fixedword((NREG+2)*IBY2WD);
	a = fixedword(NREG*IBY2WD);
	if(a == 0)
		error(exZdiv);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);

	if(x == 0 || y == 0){
		result = 0;
		memmove(R.d, &result, sizeof(result));
		return;
	}
	vnz = p&2;
	wnz = p&1;
	p >>= 2;
	v = 0;
	w = 0;
	if(vnz){
		v = (WORD)((UWORD)a-1);
		if(x >= 0 && y < 0 || x < 0 && y >= 0)
			v = (WORD)((UWORD)0-(UWORD)v);
	}
	if(wnz){
		if((!vnz && (x > 0 && y < 0 || x < 0 && y > 0)) ||
		(vnz && (x > 0 && y > 0 || x < 0 && y < 0)))
			w = fixedroundmask(p);
	}
	r = (LONG)((uvlong)(LONG)x*(uvlong)(LONG)y+(uvlong)w);
	r = fixedshift(r, p);
	r = (LONG)((uvlong)r+(uvlong)(LONG)v);
	r = fixeddivide(r, (LONG)a);
	result = (WORD)r;
	memmove(R.d, &result, sizeof(result));
}


/* Apply fixed division rounding without signed intermediate overflow. */
OP(divx1)
{
	WORD result, x, y, p, b, v;
	int vnz, wnz;
	LONG w, s;

	/* Bound both borrowed source slots before either scalar value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	p = fixedword((NREG+2)*IBY2WD);
	b = fixedword(NREG*IBY2WD);
	if(y == 0)
		error(exZdiv);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);

	if(x == 0){
		result = 0;
		memmove(R.d, &result, sizeof(result));
		return;
	}
	vnz = p&2;
	wnz = p&1;
	p >>= 2;
	v = 0;
	w = 0;
	if(vnz){
		v = 1;
		if(x >= 0 && y < 0 || x < 0 && y >= 0)
			v = (WORD)((UWORD)0-(UWORD)v);
	}
	if(wnz){
		if(x <= 0)
			w = fixedroundmask(p);
	}
	s = (LONG)((uvlong)(LONG)b*(uvlong)(LONG)x+(uvlong)w);
	s = fixedshift(s, p);
	s = fixeddivide(s, (LONG)y);
	result = (WORD)((UWORD)s+(UWORD)v);
	memmove(R.d, &result, sizeof(result));
}


/* Round and scale a fixed conversion without invalid native shifts. */
OP(cvtxx1)
{
	WORD result, x, p, a, v;
	int vnz, wnz;
	LONG w, r;

	/* Bound both borrowed source slots before either scalar value is read. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(WORD))
		error(exBounds);
	memmove(&x, R.s, sizeof(x));
	memmove(&p, R.m, sizeof(p));
	a = fixedword(NREG*IBY2WD);
	if(a == 0)
		error(exZdiv);
	/* Publish only into a complete result slot, including zero shortcuts. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(result))
		error(exBounds);

	if(x == 0){
		result = 0;
		memmove(R.d, &result, sizeof(result));
		return;
	}
	vnz = p&2;
	wnz = p&1;
	p >>= 2;
	v = 0;
	w = 0;
	if(vnz){
		v = (WORD)((UWORD)a-1);
		if(x < 0)
			v = (WORD)((UWORD)0-(UWORD)v);
	}
	if(wnz){
		if(!vnz && x < 0 || vnz && x > 0)
			w = fixedroundmask(p);
	}
	r = (LONG)((uvlong)(LONG)x+(uvlong)w);
	r = fixedshift(r, p);
	r = (LONG)((uvlong)r+(uvlong)(LONG)v);
	r = fixeddivide(r, (LONG)a);
	result = (WORD)r;
	memmove(R.d, &result, sizeof(result));
}
/*
OP(cvtxx)
{
	REAL v;

	v = (REAL)W(s)*F(m);
	v = v < 0 ? v-0.5: v+0.5;
	W(d) = (WORD)v;
}
*/
/* Reject exceptional real-to-fixed casts before a native integer conversion. */
OP(cvtfx)
{
	REAL v, source, scale;
	WORD result;

	/* Bound every used slot before operand reads or floating-point effects. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(REAL) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(REAL) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(WORD))
		error(exBounds);

	memmove(&source, R.s, sizeof(source));
	memmove(&scale, R.m, sizeof(scale));
	v = source*scale;
	v = v < 0 ? v-0.5: v+0.5;
	if(sizeof(WORD) == 4){
		if(!(v > -2147483649.0 && v < 2147483648.0))
			error(exInval);
	}else{
		if(!(v >= -9223372036854775808.0 && v < 9223372036854775808.0))
			error(exInval);
	}
	result = (WORD)v;
	memmove(R.d, &result, sizeof(result));
}


/* Capture the fixed integer and real scale before publishing their product. */
OP(cvtxf)
{
	WORD source;
	REAL scale, result;

	/* Bound every used slot before operand reads or floating-point effects. */
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(WORD) ||
	   R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(REAL) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(REAL))
		error(exBounds);

	memmove(&source, R.s, sizeof(source));
	memmove(&scale, R.m, sizeof(scale));
	result = (REAL)source*scale;
	memmove(R.d, &result, sizeof(result));
}

/* Reject dead or saturated module owners before retaining and publishing self. */
OP(self)
{
	Modlink *ml, *t;
	Heap *h;

	/* The unused source slot is irrelevant, but the destination must fit first. */
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(t))
		error(exHeap);
	ml = R.M;
	if(ml == nil || ml == H)
		error(exModule);
	if((uintptr)ml <= sizeof(Heap))
		error(exHeap);
	h = D2H(ml);
	if(h->ref == 0 || h->ref == (ulong)-1)
		error(exHeap);
	memmove(&t, R.d, sizeof(t));
	if(t != H && ((uintptr)t <= sizeof(Heap) || D2H(t)->ref == 0))
		error(exHeap);
	h->ref++;
	Setmark(h);
	memmove(R.d, &ml, sizeof(ml));
	destroy(t);
}

/* Capture frame links before releasing owners in possibly four-byte aligned frames. */
void
destroystack(REG *reg)
{
	Type *t;
	Frame *f, *fp;
	Modlink *m;
	Stkext *sx;
	uchar *ex, *next;

	ex = reg->EX;
	reg->EX = nil;
	while(ex != nil) {
		sx = (Stkext*)ex;
		fp = sx->reg.tos.fr;
		do {
			f = (Frame*)reg->FP;
			if(f == nil)
				break;
			memmove(&next, (uchar*)f+O(Frame, fp), sizeof(next));
			memmove(&t, (uchar*)f+O(Frame, t), sizeof(t));
			reg->FP = next;
			if(t == nil)
				t = sx->reg.TR;
			memmove(&m, (uchar*)f+O(Frame, mr), sizeof(m));
			if (t->np)
				freeptrs(f, t);
			if(m != nil) {
				destroy(reg->M);
				reg->M = m;
			}
		} while(f != fp);
		ex = sx->reg.EX;
		free(sx);
	}
	destroy(reg->M);
	reg->M = H;	/* for devprof */
}

Prog*
isave(void)
{
	Prog *p;

	p = delrun(Prelease);
	p->R = R;
	return p;
}

void
irestore(Prog *p)
{
	R = p->R;
	R.IC = 1;
}


/* Check native descriptor and data spans before moving channel records with ownership. */
void
movtmp(void)
{
	Type *t;
	int size;

	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(t))
		error(exBounds);
	memmove(&t, R.m, sizeof(t));
	size = mappedcopytype(t);
	if(size == 0)
		return;
	if(R.s == nil || R.s == H || R.d == nil || R.d == H)
		error(exNilref);
	if((uintptr)R.s > (uintptr)-1-(uintptr)size ||
	   (uintptr)R.d > (uintptr)-1-(uintptr)size)
		error(exBounds);

	incmem(R.s, t);
	if(t->np)
		freeptrs(R.d, t);
	memmove(R.d, R.s, size);
}

extern OP(cvtca);
extern OP(cvtac);
extern OP(cvtwc);
extern OP(cvtcw);
extern OP(cvtfc);
extern OP(cvtcf);
extern OP(insc);
extern OP(indc);
extern OP(addc);
extern OP(lenc);
extern OP(slicec);
extern OP(cvtlc);

#include "optab.h"

void
opinit(void)
{
	int i;

	for(i = 0; i < 256; i++)
		if(optab[i] == nil)
			optab[i] = badop;
}

void
xec(Prog *p)
{
	int op;
	Jit *j;

	R = p->R;
	R.MP = R.M->MP;
	R.IC = p->quanta;

	if(p->kill != nil) {
		char *m;
		m = p->kill;
		p->kill = nil;
		error(m);
	}

// print("%lux %lux %lux %lux %lux\n", (ulong)&R, R.xpc, R.FP, R.MP, R.PC);

	if(R.M->compiled)
		comvec();
	else do {
		/* A C-translated step consumes the same single scheduler instruction. */
		if(R.M->m->cstep != nil && R.M->m->cstep())
			continue;
		j = R.M->m->jit;
		dec[R.PC->add]();
		op = R.PC->op;
		R.PC++;
		if(j != nil && j->entry[op] != nil) {
			if(j->enter != nil)
				j->enter();
			if(!j->entry[op](&R))
				optab[op]();
		}else
			optab[op]();
	} while(--R.IC != 0);

	p->R = R;
}
