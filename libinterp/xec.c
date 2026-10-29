#include <lib9.h>
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "pool.h"
#include "kernel.h"
#include "vmfetch.h"
#include "vmchannel.h"

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
#define JMP(r)	branchpc(R.r, exBounds) // Validate a taken target before publishing PC.
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


/* Capture a taken native target before rejecting sentinels or publishing PC. */
static void
branchpc(void *slot, char *failure)
{
	void *target;

	if(slot == nil || slot == H || (uintptr)slot > (uintptr)-1-sizeof(target))
		error(failure);
	memmove(&target, slot, sizeof(target));
	if(target == nil || target == H)
		error(failure);
	memmove(&R.PC, &target, sizeof(target));
}


/* REG.d borrows a native target slot; REG.s/m are ignored. No blocking or
 * ownership transfer occurs. Invalid cells and nil/H targets raise exBounds before
 * PC publication. Native pointer width is independent of the VM WORD width.
 */
OP(jmp)
{
    if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(R.PC)){
        error(exBounds);
    }
    JMP(d);
}


/* Validate the full Dis instruction span before publishing a program address. */
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
	if(offset > (uintptr)-1-sizeof(Inst) ||
	   (uintptr)ml->m->prog > (uintptr)-1-offset-sizeof(Inst))
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
        JMP(d);
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
		branchpc(R.d, exHeap);
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
		if((uintptr)sv <= sizeof(Heap) || (uintptr)sv%sizeof(void*) != 0)
			error(exHeap);
		h = D2H(sv);
		if(h->ref == 0 || h->ref == (ulong)-1)
			error(exHeap);
	}
	memmove(&dv, R.d, sizeof(dv));
	/* A bad previous owner must fail before the source is retained or published. */
	if(dv != H && ((uintptr)dv <= sizeof(Heap) ||
	   (uintptr)dv%sizeof(void*) != 0 || D2H(dv)->ref == 0))
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
	void *source, *destination;

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

	/* Destructors can borrow REG; the original copy operands remain authoritative. */
	source = R.s;
	destination = R.d;
	incmem(source, t);
	if(t->np)
		freeptrs(destination, t);
	memmove(destination, source, size);
}


/* Capture the result cell and retain its owner through zeroing or allocating C callbacks. */
static void
newrecord(Modlink *ml, WORD index, int clear)
{
	Type *type;
	Heap *h, oldheap;
	void *old, *object, *destination;
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
	destination = R.d;
	if(destination == nil || destination == H || (uintptr)destination > (uintptr)-1-sizeof(old))
		error(exBounds);
	memmove(&old, destination, sizeof(old));
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
			memmove(destination, &old, sizeof(old));
			return;
		}
	}
	h = clear ? heapz(type) : heap(type);
	object = H2D(void*, h);
	memmove(destination, &object, sizeof(object));
	destroy(old);
}


/* Reuse unique plain records without reading or initializing unspecified scalar bytes. */
OP(new)
{
	WORD index;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.s, sizeof(index));
	newrecord(R.M, index, 0);
}


/* Capture the full type index before zeroing a possibly overlapping old payload. */
OP(newz)
{
	WORD index;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.s, sizeof(index));
	newrecord(R.M, index, 1);
}


/* Resolve an imported type through its current Module table before creating an object. */
OP(mnewz)
{
	Modlink *ml;
	WORD index;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(ml))
		error(exBounds);
	memmove(&ml, R.s, sizeof(ml));
	if(ml == nil || ml == H)
		error(exModule);
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.m, sizeof(index));
	newrecord(ml, index, 1);
}


/* Resolve bounded imports and byte-copied named exports without narrowing indices. */
static int
moduleentry(Modlink *ml, WORD index, Modl *entry)
{
	Module *module;
	Link link;
	Link *exports;
	uintptr address, offset;
	int count;

	address = (uintptr)ml;
	if(ml == nil || ml == H || address <= sizeof(Heap) || address > (uintptr)-1-O(Modlink, links))
		return 0;
	memmove(&count, (uchar*)ml+O(Modlink, nlinks), sizeof(count));
	if(count < 0)
		return 0;
	if(index >= 0){
		if(index >= count || (uvlong)index > ((uintptr)-1-O(Modlink, links)-sizeof(*entry))/sizeof(*entry))
			return 0;
		offset = O(Modlink, links)+(uintptr)index*sizeof(*entry);
		if(address > (uintptr)-1-offset-sizeof(*entry))
			return 0;
		memmove(entry, (uchar*)ml+offset, sizeof(*entry));
		return 1;
	}
	memmove(&module, (uchar*)ml+O(Modlink, m), sizeof(module));
	if(module == nil || module == H || (uintptr)module > (uintptr)-1-sizeof(*module))
		return 0;
	memmove(&exports, (uchar*)module+O(Module, ext), sizeof(exports));
	if(exports == nil || exports == H)
		return 0;
	address = (uintptr)exports;
	for(;;){
		if(address > (uintptr)-1-sizeof(link))
			return 0;
		memmove(&link, (void*)address, sizeof(link));
		if(link.name == nil)
			return 0;
		if(index == -1){
			entry->u = link.u;
			entry->frame = link.frame;
			return 1;
		}
		index++;
		address += sizeof(link);
	}
	return 0;
}


/* Validate descriptor maps before comparing native integer stack extents. */
static int
framefits(Type *t)
{
	int i, bit, size, pointers;
	uintptr address;

	if(t == nil || t == H || (uintptr)t%sizeof(void*) != 0 ||
	   (uintptr)t > (uintptr)-1-(O(Type, np)+sizeof(t->np)))
		error(exHeap);
	size = t->size;
	pointers = t->np;
	if(size < (int)sizeof(Frame) || pointers < 0 ||
	   R.SP == nil || R.SP == H || R.TS == nil || R.TS == H ||
	   (uintptr)R.SP > (uintptr)R.TS)
		error(exHeap);
	if(pointers > size/(8*sizeof(void*))+(size%(8*sizeof(void*)) != 0))
		error(exHeap);
	address = (uintptr)t;
	if(pointers != 0 && (address > (uintptr)-1-O(Type, map) ||
	   address+O(Type, map) > (uintptr)-1-(uintptr)pointers))
		error(exHeap);
	for(i = 0; i < pointers; i++)
		for(bit = 0; bit < 8; bit++)
			if((t->map[i] & (0x80>>bit)) &&
			   ((ulong)i*8+bit < sizeof(Frame)/sizeof(void*) ||
			    (ulong)i*8+bit >= (ulong)size/sizeof(void*)))
				error(exHeap);
	return (uintptr)size < (uintptr)R.TS-(uintptr)R.SP;
}


/* Capture the index and destination before stack growth or aliased frame stores. */
OP(frame)
{
	Type *t;
	Frame *f;
	Modlink *module;
	void *destination;
	WORD index;
	int fits, size, pointers;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.s, sizeof(index));
	t = moduletype(R.M, index);
	fits = framefits(t);
	destination = R.d;
	if(destination == nil || destination == H || (uintptr)destination > (uintptr)-1-sizeof(f))
		error(exBounds);
	if(!fits){
		R.s = t;
		extend();
		memmove(destination, &R.s, sizeof(R.s));
		return;
	}
	size = t->size;
	pointers = t->np;
	f = (Frame*)R.SP;
	R.SP = (uchar*)((uintptr)R.SP+(uintptr)size);
	module = nil;
	memmove((uchar*)f+O(Frame, t), &t, sizeof(t));
	memmove((uchar*)f+O(Frame, mr), &module, sizeof(module));
	if(pointers)
		initmem(t, f);
	memmove(destination, &f, sizeof(f));
}


/* Resolve checked import/export metadata before growth or alignment-safe publication. */
OP(mframe)
{
	Type *t;
	Frame *f;
	Modlink *module;
	void *destination;
	WORD index;
	Modl entry;
	Modlink *ml;
	int fits, size, pointers;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(ml))
		error(exBounds);
	memmove(&ml, R.s, sizeof(ml));
	if(ml == H || ml == nil)
		error(exModule);
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.m, sizeof(index));
	if(!moduleentry(ml, index, &entry))
		error("invalid mframe");
	t = entry.frame;
	fits = framefits(t);
	destination = R.d;
	if(destination == nil || destination == H || (uintptr)destination > (uintptr)-1-sizeof(f))
		error(exBounds);
	if(!fits){
		R.s = t;
		extend();
		memmove(destination, &R.s, sizeof(R.s));
		return;
	}
	size = t->size;
	pointers = t->np;
	f = (Frame*)R.SP;
	R.SP = (uchar*)((uintptr)R.SP+(uintptr)size);
	module = nil;
	memmove((uchar*)f+O(Frame, t), &t, sizeof(t));
	memmove((uchar*)f+O(Frame, mr), &module, sizeof(module));
	if(pointers)
		initmem(t, f);
	memmove(destination, &f, sizeof(f));
}


/* Capture result storage before allocation and initialize scalar and mapped bytes separately. */
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
	void *destination;

	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index))
		error(exHeap);
	memmove(&index, R.m, sizeof(index));
	type = moduletype(R.M, index);
	if((uintptr)type%sizeof(void*) != 0 ||
	   (uintptr)type > (uintptr)-1-(offsetof(Type, np)+sizeof(type->np)))
		error(exHeap);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(count))
		error(exHeap);
	memmove(&count, R.s, sizeof(count));
	/* Preserve descriptor-error precedence before checking the full WORD geometry. */
	if(type->ref <= 0 || type->ref > 0x7ffffffe - (type == &Tarray) ||
	   Tarray.ref <= 0 || Tarray.ref == 0x7fffffff)
		error(exHeap);
	acheck(type->size, count);
	destination = R.d;
	if(destination == nil || destination == H || (uintptr)destination > (uintptr)-1-sizeof(old))
		error(exHeap);
	memmove(&old, destination, sizeof(old));
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
				memmove(destination, &old, sizeof(old));
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
	memmove(destination, &array, sizeof(array));
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


/* Capture capacity, result storage and old ownership before allocating C callbacks. */
Channel*
newc(Type *t, void (*mover)(void))
{
	Channel *channel, *oldc;
	WORD len;
	void *destination;

	len = channelargs(&oldc);
	destination = R.d;
	channel = cnewc(t, mover, len);
	memmove(destination, &channel, sizeof(channel));
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
	void *destination;

	address = (uintptr)R.s;
	if(R.s == nil || R.s == H || address > (uintptr)-1-sizeof(size))
		error(exHeap);
	memmove(&size, R.s, sizeof(size));
	if(size < 0)
		error(exNegsize);
	if((uvlong)size > 0x7fffffff)
		error(exHeap);
	capacity = channelargs(&old);
	destination = R.d;
	channel = cnewcraw((int)size, (int)capacity);
	channel->mid.w = size;
	memmove(destination, &channel, sizeof(channel));
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


/* Resolve a table target only when its complete Dis instruction span fits. */
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
	if(offset > (uintptr)-1-sizeof(Inst) ||
	   program > (uintptr)-1-offset-sizeof(Inst))
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


/* Read fixed-width table cells and publish only a complete Dis instruction span. */
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
	if(offset > (uintptr)-1-sizeof(Inst) ||
	   program > (uintptr)-1-offset-sizeof(Inst))
		error(exBounds);
	R.PC = (Inst*)(program+offset);
}


/* Capture aliased operands and validate bytecode spans before publishing call links. */
OP(call)
{
	Frame *f;
	Inst *target;
	int compiled;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(f))
		error(exBounds);
	memmove(&f, R.s, sizeof(f));
	if(f == nil || f == H)
		error(exNilref);
	if((uintptr)f > (uintptr)-1-(O(Frame, fp)+sizeof(f->fp)))
		error(exHeap);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(target))
		error(exBounds);
	memmove(&target, R.d, sizeof(target));
	if(target == nil || target == H)
		error(exBounds);
	if((uintptr)target > (uintptr)-1-sizeof(Inst)){
		if(R.M == nil || R.M == H ||
		   (uintptr)R.M > (uintptr)-1-(O(Modlink, compiled)+sizeof(compiled)))
			error(exModule);
		memmove(&compiled, (uchar*)R.M+O(Modlink, compiled), sizeof(compiled));
		if(!compiled)
			error(exBounds);
	}
	memmove((uchar*)f+offsetof(Frame, lr), &R.PC, sizeof(f->lr));
	memmove((uchar*)f+offsetof(Frame, fp), &R.FP, sizeof(f->fp));
	R.FP = (uchar*)f;
	R.PC = target;
}

typedef struct SpawnProg SpawnProg;

struct SpawnProg {
	Modlink *module; // Borrowed child module retained by scheduler publication.
	Inst *pc; // Original Dis entry captured before child-stack allocation.
	Prog initial; // Private child registers until all allocation succeeds.
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

	if(R.M == nil || R.M == H || (uintptr)R.M%sizeof(void*) != 0 ||
	   (uintptr)R.M > (uintptr)-1-sizeof(*R.M))
		error(exModule);
	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(target))
		error(exBounds);
	memmove(&target, R.d, sizeof(target));
	if(target == nil || target == H ||
	   (!R.M->compiled && (uintptr)target > (uintptr)-1-sizeof(*target)))
		error(exSpawn);
	spawnmodule(R.M, target);
}


/* Validate module ownership before reading a child entry or creating its stack. */
OP(mspawn)
{
	WORD index;
	Modlink *ml;
	Modl entry;

	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(ml))
		error(exBounds);
	memmove(&ml, R.d, sizeof(ml));
	if(ml == H || ml == nil || (uintptr)ml%sizeof(void*) != 0 ||
	   (uintptr)ml > (uintptr)-1-sizeof(*ml) || ml->m == H || ml->m == nil ||
	   (uintptr)ml->m%sizeof(void*) != 0 || (uintptr)ml->m > (uintptr)-1-sizeof(*ml->m))
		error(exModule);
	if(ml->prog == nil || ml->prog == H)
		error(exSpawn);
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.m, sizeof(index));
	if(!moduleentry(ml, index, &entry))
		error("invalid mspawn");
	if(entry.u.pc == nil || entry.u.pc == H ||
	   (!ml->compiled && (uintptr)entry.u.pc > (uintptr)-1-sizeof(*entry.u.pc)))
		error(exSpawn);
	spawnmodule(ml, entry.u.pc);
}


/* Validate captured return metadata before changing registers or releasing owners. */
OP(ret)
{
	Frame *f, links;
	Modlink *m;
	Type *type;
	uchar *mp;
	uintptr extent;
	ulong references;
	int pointers, currentmode, previousmode;

	if(R.FP == nil || R.FP == H)
		error(exNilref);
	f = (Frame*)R.FP;
	if((uintptr)f > (uintptr)-1-sizeof(*f))
		error(exHeap);
	memmove(&links, f, sizeof(links));
	if(links.fp == nil)
		error("");
	if(links.fp == H || (uintptr)links.fp > (uintptr)-1-sizeof(Frame) ||
	   links.lr == nil || links.lr == H)
		error(exHeap);
	m = links.mr;
	if(m == H || (m != nil && (R.M == nil || R.M == H)))
		error(exModule);
	type = links.t;
	if(type == nil){
		if((uintptr)f < OA(Stkext, reg.tos.fu))
			error(exHeap);
		extent = (uintptr)f-OA(Stkext, reg.tos.fu);
		if(extent > (uintptr)-1-(O(Stkext, reg.TR)+sizeof(type)))
			error(exHeap);
		memmove(&type, (void*)(extent+O(Stkext, reg.TR)), sizeof(type));
	}
	if(type == nil || type == H || (uintptr)type > (uintptr)-1-(O(Type, np)+sizeof(pointers)))
		error(exHeap);
	memmove(&pointers, (uchar*)type+O(Type, np), sizeof(pointers));
	if(pointers < 0)
		error(exHeap);
	/* A self-module return still owns both REG.M and the saved Frame.mr. */
	currentmode = previousmode = 0;
	mp = nil;
	if(m != nil){
		if((uintptr)m > (uintptr)-1-(O(Modlink, compiled)+sizeof(previousmode)) ||
		   (uintptr)R.M <= sizeof(Heap) ||
		   (uintptr)R.M > (uintptr)-1-(O(Modlink, compiled)+sizeof(currentmode)))
			error(exModule);
		memmove(&references, (uchar*)D2H(R.M)+O(Heap, ref), sizeof(references));
		if(references == 0 || (m == R.M && references == 1))
			error(exHeap);
		memmove(&currentmode, (uchar*)R.M+O(Modlink, compiled), sizeof(currentmode));
		memmove(&previousmode, (uchar*)m+O(Modlink, compiled), sizeof(previousmode));
		memmove(&mp, (uchar*)m+O(Modlink, MP), sizeof(mp));
	}
	R.FP = links.fp;
	R.SP = (uchar*)f;
	R.PC = links.lr;

	if(links.t == nil)
		unextend(f);
	else if(pointers)
		freeptrs(f, links.t);

	if(m != nil) {
		if(currentmode != previousmode) {
			R.IC = 1;
			R.t = 1;
		}
		destroy(R.M);
		R.M = m;
		R.MP = mp;
	}
}


/* Resolve a full-width import index without stepping beyond the table sentinel. */
static Import*
moduleimports(Module *m, WORD index)
{
	Import **table, *entry;
	WORD i;
	uintptr address;

	if(m == nil || m == H || (uintptr)m > (uintptr)-1-sizeof(*m) || index < 0)
		error(exModule);
	memmove(&table, (uchar*)m+O(Module, ldt), sizeof(table));
	if(table == nil || table == H)
		error(exModule);
	address = (uintptr)table;
	for(i = 0; ; i++){
		if(address > (uintptr)-1-sizeof(entry))
			error(exModule);
		memmove(&entry, (void*)address, sizeof(entry));
		if(entry == nil || entry == H)
			error(exModule);
		if(i == index)
			return entry;
		address += sizeof(entry);
	}
}


/* Validate imports before loading and retain nil data for data-free self loads. */
OP(iload)
{
	char *n;
	Import *ldt;
	Module *m;
	Modlink *ml, *t, *current;
	uchar *mp;
	void *destination;
	String *name;
	WORD index;
	Heap *h;

	current = R.M;
	if(current == nil || current == H || (uintptr)current > (uintptr)-1-sizeof(*current))
		error(exModule);
	memmove(&m, (uchar*)current+O(Modlink, m), sizeof(m));
	if(m == nil || m == H || (uintptr)m%sizeof(void*) != 0 ||
	   (uintptr)m > (uintptr)-1-sizeof(*m))
		error(exModule);
	if(!(m->rt & HASLDT))
		error("obsolete dis");
	/* Bound and capture the result cell before any conversion/linker callback. */
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(name) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(ml))
		error(exBounds);
	destination = R.d;
	memmove(&mp, (uchar*)current+O(Modlink, MP), sizeof(mp));
	memmove(&index, R.m, sizeof(index));
	memmove(&name, R.s, sizeof(name));
	ldt = moduleimports(m, index);
	n = string2c(name);
	if(strcmp(n, "$self") == 0) {
		if(mp == nil || m->ref <= 0 || m->ref == 0x7fffffff)
			error(exModule);
		h = nil;
		if(mp != H){
			if((uintptr)mp <= sizeof(Heap) || ((uintptr)mp-sizeof(Heap))%sizeof(void*) != 0)
				error(exHeap);
			h = (Heap*)((uintptr)mp-sizeof(Heap));
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

	memmove(&t, destination, sizeof(t));
	memmove(destination, &ml, sizeof(ml));
	destroy(t);
}


/* Validate module references and the complete entry before publishing a callee frame. */
OP(mcall)
{
	WORD index;
	Heap *h;
	Prog *p;
	Frame *f, links;
	Modl entry;
	Modlink *ml;
	Inst *program;
	uchar *mp;
	ulong references;
	int callermode, calleemode, pointers;

	if(R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(ml))
		error(exBounds);
	memmove(&ml, R.d, sizeof(ml));
	if(ml == nil || ml == H || R.M == nil || R.M == H)
		error(exModule);
	if((uintptr)ml <= sizeof(Heap) || (uintptr)ml > (uintptr)-1-O(Modlink, links) ||
	   (uintptr)R.M > (uintptr)-1-(O(Modlink, compiled)+sizeof(R.M->compiled)))
		error(exModule);
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(index))
		error(exBounds);
	memmove(&index, R.m, sizeof(index));
	if(!moduleentry(ml, index, &entry))
		error("invalid mcall");
	h = D2H(ml);
	memmove(&references, (uchar*)h+O(Heap, ref), sizeof(references));
	if(references == 0 || references == (ulong)-1)
		error(exHeap);
	memmove(&program, (uchar*)ml+O(Modlink, prog), sizeof(program));
	memmove(&callermode, (uchar*)R.M+O(Modlink, compiled), sizeof(callermode));
	memmove(&calleemode, (uchar*)ml+O(Modlink, compiled), sizeof(calleemode));
	memmove(&mp, (uchar*)ml+O(Modlink, MP), sizeof(mp));
	if(program == H ||
	   (program != nil && (entry.u.pc == nil || entry.u.pc == H ||
	    (!calleemode && (uintptr)entry.u.pc > (uintptr)-1-sizeof(Inst)))) ||
	   (program == nil && (entry.u.runt == nil || entry.u.runt == (void(*)(void*))H)))
		error(exModule);
	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(f))
		error(exBounds);
	memmove(&f, R.s, sizeof(f));
	if(f == nil || f == H)
		error(exNilref);
	if((uintptr)f > (uintptr)-1-(program == nil ? sizeof(*f) : O(Frame, mr)+sizeof(f->mr)))
		error(exHeap);
	memmove((uchar*)f+offsetof(Frame, lr), &R.PC, sizeof(f->lr));
	memmove((uchar*)f+offsetof(Frame, fp), &R.FP, sizeof(f->fp));
	memmove((uchar*)f+offsetof(Frame, mr), &R.M, sizeof(f->mr));

	R.FP = (uchar*)f;
	R.M = ml;
	references++;
	memmove((uchar*)h+O(Heap, ref), &references, sizeof(references));

	if(program == nil) {
		entry.u.runt(f);
		/* Cleanup belongs to the captured callee, even if C changed live REG fields. */
		memmove(&links, f, sizeof(links));
		pointers = 0;
		if(links.t != nil){
			if(links.t == H || (uintptr)links.t > (uintptr)-1-(O(Type, np)+sizeof(pointers)))
				error(exHeap);
			memmove(&pointers, (uchar*)links.t+O(Type, np), sizeof(pointers));
			if(pointers < 0)
				error(exHeap);
		}
		memmove(&references, (uchar*)h+O(Heap, ref), sizeof(references));
		if(references == 0)
			error(exHeap);
		references--;
		memmove((uchar*)h+O(Heap, ref), &references, sizeof(references));
		R.M = links.mr;
		R.SP = (uchar*)f;
		R.FP = links.fp;
		if(links.t == nil)
			unextend(f);
		else if(pointers)
			freeptrs(f, links.t);
		p = currun();
		if(p->kill != nil)
			error(p->kill);
		R.t = 0;
		return;
	}
	R.MP = mp;
	R.PC = entry.u.pc;
	R.t = 1;

	if(callermode != calleemode)
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
typedef struct ChannelBuffer ChannelBuffer;
typedef struct ChannelLeaf ChannelLeaf;

struct ChannelBuffer {
	Array *array; // Borrowed buffered Array whose lifetime stays unchanged during preflight.
	Type *type; // Aligned live element descriptor captured before ring changes.
	void *slot; // Selected ring payload address, including zero-width identities.
	int width; // Validated element width in native bytes.
	int front; // Next front index for a completed receive.
	int size; // Next occupied count for the selected direction.
	char *error; // Borrowed VM error identity on a rejected geometry.
};

struct ChannelLeaf {
	void *source; // Captured copy source at byte offset zero in the native transaction.
	void *destination; // Captured copy destination at native byte offset eight.
	uintptr bytes; // Copy length at byte offset sixteen; native leaves support 0..256.
	int *front; // Ring-front publication address at byte offset twenty-four.
	int *size; // Occupancy publication address at byte offset thirty-two.
	uintptr nextfront; // Captured int front value at byte offset forty.
	uintptr nextsize; // Captured int occupancy value at byte offset forty-eight.
	void *middle; // Channel mover metadata address at byte offset fifty-six.
};


/* Numeric span checks do not establish virtual mapping or allocation ownership. */
static int
channelspan(void *p, uintptr bytes)
{
	return p != nil && p != H && (uintptr)p <= (uintptr)-1-bytes;
}


/* Stage ring geometry without allocation, VM stores, callbacks or reference changes.
 * Return one for an active slot, zero for an unavailable/declined ring, and minus
 * one with a borrowed error identity for malformed metadata. Empty receives and
 * full sends never inspect an unused element descriptor or backing data.
 */
static int
channelbuffer(Channel *c, int receive, int pointerfree, ChannelBuffer *out)
{
	Channel channel;
	Array array;
	Type *type;
	uintptr address, bytes, offset;
	WORD index;
	int width, pointers, i, bit;

	out->error = exHeap;
	if(c == nil || c == H){
		out->error = exNilref;
		return -1;
	}
	if(!channelspan(c, sizeof(channel)) || (uintptr)c%sizeof(void*) != 0)
		return -1;
	memmove(&channel, c, sizeof(channel));
	if(channel.buf == H)
		return 0;
	if(channel.buf == nil){
		out->error = exNilref;
		return -1;
	}
	if(channel.size < 0)
		return -1;
	if(receive && channel.size == 0)
		return 0;
	if(!channelspan(channel.buf, sizeof(array)) ||
	   (uintptr)channel.buf <= sizeof(Heap) || (uintptr)channel.buf%sizeof(void*) != 0)
		return -1;
	memmove(&array, channel.buf, sizeof(array));
	if(array.len < 0 || (uvlong)array.len > 0x7fffffff || channel.size > array.len)
		return -1;
	if(!receive && channel.size == array.len)
		return 0;
	if(array.len == 0 || channel.front < 0 || channel.front >= array.len)
		return -1;
	if(channel.mover == nil || (uintptr)channel.mover == (uintptr)-1)
		return -1;
	type = array.t;
	if(!channelspan(type, offsetof(Type, np)+sizeof(pointers)) ||
	   (uintptr)type%sizeof(void*) != 0)
		return -1;
	memmove(&width, (uchar*)type+offsetof(Type, size), sizeof(width));
	memmove(&pointers, (uchar*)type+offsetof(Type, np), sizeof(pointers));
	/* Decline large/mapped native transfers before walking maps only C will use. */
	if(pointerfree && (width < 0 || width > 256 || pointers != 0))
		return 0;
	if(width < 0 || pointers < 0 ||
	   pointers > width/(8*sizeof(void*))+(width%(8*sizeof(void*)) != 0))
		return -1;
	address = (uintptr)type;
	if(pointers != 0 && (address > (uintptr)-1 - offsetof(Type, map) ||
	   address+offsetof(Type, map) > (uintptr)-1-(uintptr)pointers))
		return -1;
	for(i = 0; i < pointers; i++)
		for(bit = 0; bit < 8; bit++)
			if((((uchar*)type+offsetof(Type, map))[i] & (0x80>>bit)) &&
			   ((ulong)i*8+bit >= (ulong)width/sizeof(void*)))
				return -1;
	if(width != 0 && (uvlong)array.len > (uintptr)-1/(uint)width)
		return -1;
	bytes = (uintptr)array.len*(uint)width;
	if(bytes != 0 && !channelspan(array.data, bytes)){
		out->error = array.data == nil || array.data == H ? exNilref : exBounds;
		return -1;
	}
	if(receive)
		index = channel.front;
	else if(channel.front >= array.len-channel.size)
		index = channel.front-(array.len-channel.size);
	else
		index = channel.front+channel.size;
	offset = (uintptr)index*(uint)width;
	out->array = channel.buf;
	out->type = type;
	out->width = width;
	out->slot = (void*)((uintptr)array.data+offset);
	out->front = receive ? (channel.front == array.len-1 ? 0 : channel.front+1) : channel.front;
	out->size = channel.size+(receive ? -1 : 1);
	return 1;
}


/* Check the channel and peer-queue prefix before any scheduler queue access. */
static Prog*
channelpeer(Channel *c, int receive)
{
	Progq *queue;
	Prog *peer;

	if(c == nil || c == H)
		error(exNilref);
	if(!channelspan(c, sizeof(*c)) || (uintptr)c%sizeof(void*) != 0)
		error(exHeap);
	queue = receive ? c->send : c->recv;
	if(!channelspan(queue, offsetof(Progq, prog)+sizeof(peer)))
		error(exHeap);
	memmove(&peer, (uchar*)queue+offsetof(Progq, prog), sizeof(peer));
	return peer;
}


typedef struct ChannelRead ChannelRead;
typedef struct ChannelRefill ChannelRefill;

/* Retain the original backing owner while a mover or mapped cleanup borrows REG or raises. */
struct ChannelRead
{
	Channel *channel; // Original channel supplies the mover's metadata.
	Array *array; // Temporary reference released on both protected exits.
	void *slot; // Captured ring source, independent of later front/REG changes.
	void *destination; // Original receive output cell.
};

/* Hold a queued sender's channel across both buffered copy stages. */
struct ChannelRefill
{
	Channel *channel; // Temporary reference protects receive and refill callbacks.
	Prog *sender; // Queued sender whose payload is cleared only after successful refill.
	void *destination; // Original receiver cell, not a mover's borrowed REG.d.
	void *source; // Original sender payload captured before the receive callback.
};

/* The refill callback is declared before the authoritative buffered helpers. */
static int cgetb(Channel*, void*);
static int cputb(Channel*, void*);


/* Perform a buffered receive while its caller owns the temporary array reference. */
static int
channelread(void *arg)
{
	ChannelRead *read;
	Array *array;

	read = arg;
	array = read->array;
	R.s = read->slot;
	R.m = &read->channel->mid;
	R.d = read->destination;
	read->channel->mover();
	if(array->t->np){
		freeptrs(read->slot, array->t);
		initmem(array->t, read->slot);
	}
	return 1;
}


/* Preserve the original destination and sender source across receive callbacks. */
static int
channelrefill(void *arg)
{
	ChannelRefill *refill;

	refill = arg;
	cgetb(refill->channel, refill->destination);
	cputb(refill->channel, refill->source);
	refill->sender->ptr = nil;
	return 1;
}


/* Validate and retain buffered storage before changing its ring or destination. */
static int
cgetb(Channel *c, void *v)
{
	ChannelBuffer buffer;
	Array *a;
	Heap *h;
	ChannelRead read;
	int status, ok;

	status = channelbuffer(c, 1, 0, &buffer);
	if(status < 0)
		error(buffer.error);
	if(status == 0)
		return 0;
	if(buffer.width != 0 && !channelspan(v, buffer.width))
		error(v == nil || v == H ? exNilref : exBounds);
	a = buffer.array;
	h = D2H(a);
	if(h->ref == 0 || h->ref == (ulong)-1)
		error(exHeap);
	h->ref++;
	read.channel = c;
	read.array = a;
	read.slot = buffer.slot;
	read.destination = v;
	c->front = buffer.front;
	c->size = buffer.size;
	ok = kprotect(channelread, &read);
	destroy(a);
	if(!ok)
		krethrow();
	return 1;
}


/* Capture validated ring geometry before raw or mapped send publication. */
static int
cputb(Channel *c, void *v)
{
	ChannelBuffer buffer;
	int status;

	status = channelbuffer(c, 0, 0, &buffer);
	if(status < 0)
		error(buffer.error);
	if(status == 0)
		return 0;
	if(buffer.width != 0 && !channelspan(v, buffer.width))
		error(v == nil || v == H ? exNilref : exBounds);
	c->size = buffer.size;
	R.s = v;
	R.m = &c->mid;
	R.d = buffer.slot;
	c->mover();
	return 1;
}


/* Reject payload aliases with metadata whose C transfer order is observable. */
static int
channeloutside(void *payload, uintptr bytes, void *object, uintptr extent)
{
	uintptr first, other;

	if(bytes == 0)
		return 1;
	first = (uintptr)payload;
	other = (uintptr)object;
	return first+bytes <= other || other+extent <= first;
}


/* Stage a bounded pointer-free buffered send/receive without changing the VM.
 * Blocking, queued peers, custom movers, mapped pointers and metadata aliases
 * decline to the existing interpreter. The caller owns a private 64-byte native
 * transaction; success only writes that transaction, never operands or headers.
 */
int
cchanleaf(REG *r, int receive, void *transaction)
{
	Channel *c;
	ChannelBuffer buffer;
	ChannelLeaf leaf;
	Progq *queue;
	Prog *peer;
	void *value, *slot, *source, *destination;
	int width, status, i;
	void *objects[6];
	uintptr extents[6];
	ulong references;

	if(sizeof(void*) != 8 || sizeof(leaf) != 64 ||
	   (receive != 0 && receive != 1) || !channelspan(r, sizeof(*r)) ||
	   !channelspan(transaction, sizeof(leaf)) ||
	   !channeloutside(transaction, sizeof(leaf), r, sizeof(*r)))
		return 0;
	/* A decoded REG image may be unaligned; do not impose a hidden C-only ABI. */
	memmove(&source, (uchar*)r+offsetof(REG, s), sizeof(source));
	memmove(&destination, (uchar*)r+offsetof(REG, d), sizeof(destination));
	slot = receive ? source : destination;
	if(!channelspan(slot, sizeof(c)))
		return 0;
	memmove(&c, slot, sizeof(c));
	if(!channelspan(c, sizeof(*c)) || (uintptr)c <= sizeof(Heap) ||
	   (uintptr)c%sizeof(void*) != 0)
		return 0;
	queue = receive ? c->send : c->recv;
	if(!channelspan(queue, offsetof(Progq, prog)+sizeof(peer)))
		return 0;
	memmove(&peer, (uchar*)queue+offsetof(Progq, prog), sizeof(peer));
	if(peer != nil)
		return 0;
	status = channelbuffer(c, receive, 1, &buffer);
	if(status != 1 || buffer.width > 256 || buffer.type->np != 0)
		return 0;
	width = buffer.width;
	if(c->mover == movb){
		if(width != sizeof(BYTE)) return 0;
	}else if(c->mover == movw){
		if(width != sizeof(WORD)) return 0;
	}else if(c->mover == movl){
		if(width != sizeof(LONG)) return 0;
	}else if(c->mover == movf){
		if(width != sizeof(REAL)) return 0;
	}else if(c->mover == movm){
		if(c->mid.w != width) return 0;
	}else if(c->mover == movtmp){
		if(c->mid.t != buffer.type) return 0;
	}else
		return 0;
	if(receive){
		memmove(&references, (uchar*)buffer.array-sizeof(Heap)+offsetof(Heap, ref), sizeof(references));
		if(references == 0 || references == (ulong)-1)
			return 0;
	}
	value = receive ? destination : source;
	if(width != 0 && !channelspan(value, width))
		return 0;
	leaf.source = receive ? buffer.slot : value;
	leaf.destination = receive ? value : buffer.slot;
	if(!channeloutside(transaction, sizeof(leaf), leaf.source, width) ||
	   !channeloutside(transaction, sizeof(leaf), leaf.destination, width))
		return 0;
	leaf.bytes = width;
	/* C changes ring counters, REG operands and a temporary buffer reference. */
	objects[0] = r; extents[0] = sizeof(*r);
	objects[1] = (uchar*)c-sizeof(Heap); extents[1] = sizeof(Heap)+sizeof(*c);
	objects[2] = (uchar*)buffer.array-sizeof(Heap); extents[2] = sizeof(Heap)+sizeof(Array);
	objects[3] = buffer.type; extents[3] = sizeof(Type);
	objects[4] = queue; extents[4] = offsetof(Progq, prog)+sizeof(peer);
	objects[5] = slot; extents[5] = sizeof(c);
	for(i = 0; i < 6; i++){
		if(!channelspan(objects[i], extents[i]) ||
		   !channeloutside(transaction, sizeof(leaf), objects[i], extents[i]) ||
		   (i < 4 && (!channeloutside(leaf.source, width, objects[i], extents[i]) ||
		   !channeloutside(leaf.destination, width, objects[i], extents[i]))))
			return 0;
	}
	leaf.front = &c->front;
	leaf.size = &c->size;
	leaf.nextfront = buffer.front;
	leaf.nextsize = buffer.size;
	leaf.middle = &c->mid;
	memmove(transaction, &leaf, sizeof(leaf));
	return 1;
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


/* Resolve a checked channel/peer before enqueueing or transferring a sender. */
OP(isend)
{
	Channel *c;
	Prog *p;

	if(!channelspan(R.d, sizeof(c)))
		error(exHeap);
	memmove(&c, R.d, sizeof(c));
	if((p = channelpeer(c, 0)) == nil) {
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


/* Capture a checked channel owner and retain buffered storage through sender refill. */
OP(irecv)
{
	Channel *c;
	Prog *p;
	Heap *h;
	ChannelRefill refill;
	int ok;

	if(!channelspan(R.s, sizeof(c)))
		error(exHeap);
	memmove(&c, R.s, sizeof(c));
	if((p = channelpeer(c, 1)) == nil) {
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
		refill.channel = c;
		refill.sender = p;
		refill.destination = R.d;
		refill.source = p->ptr;
		ok = kprotect(channelrefill, &refill);
		destroy(c);
		if(!ok)
			krethrow();
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


typedef struct CsendAlt CsendAlt;

/* Preserve the native call's addresses and outcome across protected allocator/mover/destructor callbacks. */
struct CsendAlt
{
	Channel *channel; // Original channel owner used after callbacks borrow REG.
	void *payload; // Original source whose references are consumed only on drop or successful send.
	Type *type; // Original payload map used by the existing cleanup contract.
	int capacity; // Positive permits creating a buffer; nonpositive retains best-effort dropping.
	int result; // Published native outcome after cleanup completes.
};


/* Preserve ordinary partial-transfer errors; never consume the input again on an exception. */
static int
csendaltcall(void *arg)
{
	CsendAlt *send;
	VmChannelCall call;
	Channel *channel;

	send = arg;
	channel = send->channel;
	if(!vmchanready(channel, 0)){
		if(channel->buf != H || send->capacity <= 0){
			freeptrs(send->payload, send->type);
			send->result = 0;
			return 1;
		}
		channel->buf = H2D(Array*, heaparray(send->type, send->capacity));
	}
	call.channel = channel;
	call.payload = send->payload;
	call.receive = 0;
	vmchanrun(&call);
	freeptrs(send->payload, send->type);
	send->result = 1;
	return 1;
}


/* Restore REG after allocation, transfer and payload cleanup, including every VM error exit. */
int
csendalt(Channel *c, void *ip, Type *t, int len)
{
	CsendAlt send;
	REG saved;
	int ok;

	send.channel = c;
	send.payload = ip;
	send.type = t;
	send.capacity = len;
	send.result = 0;
	memmove(&saved, &R, sizeof(saved));
	ok = kprotect(csendaltcall, &send);
	memmove(&R, &saved, sizeof(R));
	if(!ok)
		krethrow();
	return send.result;
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

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value))
		error(exHeap);
	memmove(&value, R.s, sizeof(value));
	l = cons(IBY2WD, R.d);
	memmove(l->data, &value, sizeof(value));
}


/* Host WORD storage must fit the complete interpreter value. */
OP(consw)
{
	WORD value;
	List *l;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value))
		error(exHeap);
	memmove(&value, R.s, sizeof(value));
	l = cons(sizeof(value), R.d);
	memmove(l->data, &value, sizeof(value));
}


/* Retain the source big integer if publishing the list changes its source slot. */
OP(consl)
{
	LONG value;
	List *l;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value))
		error(exHeap);
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

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(sv))
		error(exHeap);
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

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(value))
		error(exHeap);
	memmove(&value, R.s, sizeof(value));
	l = cons(sizeof(value), R.d);
	memmove(l->data, &value, sizeof(value));
}


/* Capture raw operands before allocation and copy the head before publishing its owner. */
OP(consm)
{
	WORD size;
	uintptr address;
	List *l, *result;
	void *source, *destination;

	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(size))
		error(exHeap);
	memmove(&size, R.m, sizeof(size));
	source = R.s;
	destination = R.d;
	if(size < 0 || (uvlong)size > 0x7fffffff-sizeof(Heap)-
	   (sizeof(List)-sizeof(((List*)0)->data)))
		error(exHeap);
	address = (uintptr)destination;
	if(destination == nil || destination == H || address > (uintptr)-1-sizeof(result))
		error(exHeap);
	if(size != 0){
		address = (uintptr)source;
		if(source == nil || source == H || address > (uintptr)-1-(uintptr)size)
			error(exHeap);
	}
	memmove(&result, destination, sizeof(result));
	l = cons((ulong)size, &result);
	if(size != 0)
		memmove(l->data, source, (int)size);
	memmove(destination, &result, sizeof(result));
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
	void *payload, *destination;

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
	destination = R.d;
	payload = (void*)(source+offsetof(List, data));
	R.s = payload;
	incmem(payload, t);
	if(t->np)
		freeptrs(destination, t);
	memmove(destination, payload, size);
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

	if(type == nil || type == H || (uintptr)type%sizeof(void*) != 0 ||
	   (uintptr)type > (uintptr)-1-(offsetof(Type, np)+sizeof(type->np)))
		error(exBounds);
	if(type->size < 0 || start < 0 || count < 0)
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
	Array *ss, *ds, *root;
	WORD v, n, start;
	uchar *data;
	void *slot;
	uintptr address;

	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(v) ||
	   R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(start) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(ds))
		error(exHeap);
	slot = R.d;
	memmove(&v, R.m, sizeof(v));
	memmove(&start, R.s, sizeof(start));
	memmove(&ds, slot, sizeof(ds));

	if(ds == H || ds == nil){
		if(v == start){
			if(start == 0)
				return;
			error(exBounds);
		}
		error(exNilref);
	}
	address = (uintptr)ds;
	if(address <= sizeof(Heap) || address%sizeof(void*) != 0 ||
	   address > (uintptr)-1-sizeof(Array))
		error(exHeap);
	if(start < 0 || v < start || v > ds->len)
		error(exBounds);
	n = v-start;

	t = ds->t;
	data = arrayspan(ds, t, start, n);
	root = ds->root == H ? ds : ds->root;
	if(root == nil)
		error(exNilref);
	address = (uintptr)root;
	if(address <= sizeof(Heap) || address%sizeof(void*) != 0)
		error(exHeap);
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
		memmove(slot, &ss, sizeof(ss));
		destroy(ds);
	}
	else
		memmove(slot, &ss, sizeof(ss));
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
	uintptr address;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(ss))
		error(exHeap);
	memmove(&ss, R.s, sizeof(ss));
	/* A missing source copies nothing and does not consume the other operands. */
	if(ss == H || ss == nil)
		return;
	if(R.m == nil || R.m == H || (uintptr)R.m > (uintptr)-1-sizeof(dl) ||
	   R.d == nil || R.d == H || (uintptr)R.d > (uintptr)-1-sizeof(ds))
		error(exHeap);
	memmove(&dl, R.m, sizeof(dl));
	memmove(&ds, R.d, sizeof(ds));
	if(ds == H || ds == nil)
		error(exNilref);
	address = (uintptr)ss;
	if(address%sizeof(void*) != 0 || address > (uintptr)-1-sizeof(Array))
		error(exHeap);
	address = (uintptr)ds;
	if(address%sizeof(void*) != 0 || address > (uintptr)-1-sizeof(Array))
		error(exHeap);
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
	if((uintptr)ss <= sizeof(Heap))
		error(exHeap);
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
	Type *type;
	int size;
	Prog *p;

	if(R.s == nil || R.s == H || (uintptr)R.s > (uintptr)-1-sizeof(v))
		error(exHeap);
	memmove(&v, R.s, sizeof(v));
	if(v == H || v == nil)
		error(exNilref);
	if((uintptr)v <= sizeof(Heap) || (uintptr)v%sizeof(void*) != 0 ||
	   (uintptr)v > (uintptr)-1-sizeof(message))
		error(exHeap);
	h = D2H(v);
	/* Reject unusable borrowed owners before fallible message preparation. */
	if(h->ref == 0 || h->ref == (ulong)-1)
		error(exHeap);
	type = h->t;
	if(type == &Tstring){
		if((uintptr)v > (uintptr)-1-O(String, data))
			error(exHeap);
		message = v;
	}else{
		if(type == nil || type == H ||
		   (uintptr)type > (uintptr)-1-(O(Type, size)+sizeof(size)))
			error(exType);
		memmove(&size, (uchar*)type+O(Type, size), sizeof(size));
		if(size < (int)sizeof(message))
			error(exType);
		if((uintptr)v > (uintptr)-1-sizeof(message))
			error(exHeap);
		memmove(&message, v, sizeof(message));
	}
	if(message != H && message != nil){
		if((uintptr)message <= sizeof(Heap) ||
		   (uintptr)message%sizeof(void*) != 0 ||
		   (uintptr)message > (uintptr)-1-O(String, data))
			error(exHeap);
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
	if((uintptr)ml <= sizeof(Heap) || (uintptr)ml%sizeof(void*) != 0)
		error(exHeap);
	h = D2H(ml);
	if(h->ref == 0 || h->ref == (ulong)-1)
		error(exHeap);
	memmove(&t, R.d, sizeof(t));
	if(t != H && ((uintptr)t <= sizeof(Heap) ||
	   (uintptr)t%sizeof(void*) != 0 || D2H(t)->ref == 0))
		error(exHeap);
	h->ref++;
	Setmark(h);
	memmove(R.d, &ml, sizeof(ml));
	destroy(t);
}


/* Capture extent roots and frame links before cleanup; read packed map counts safely. */
void
destroystack(REG *reg)
{
	Type *t, *root;
	Frame *f, *fp;
	Modlink *m;
	Stkext *sx;
	uchar *ex, *next, *parent;
	int pointers;

	ex = reg->EX;
	reg->EX = nil;
	while(ex != nil) {
		sx = (Stkext*)ex;
		fp = sx->reg.tos.fr;
		/* Destructors must not redirect the remaining extent or root descriptor. */
		parent = sx->reg.EX;
		root = sx->reg.TR;
		do {
			f = (Frame*)reg->FP;
			if(f == nil)
				break;
			memmove(&next, (uchar*)f+O(Frame, fp), sizeof(next));
			memmove(&t, (uchar*)f+O(Frame, t), sizeof(t));
			reg->FP = next;
			if(t == nil)
				t = root;
			memmove(&m, (uchar*)f+O(Frame, mr), sizeof(m));
			memmove(&pointers, (uchar*)t+O(Type, np), sizeof(pointers));
			if(pointers)
				freeptrs(f, t);
			if(m != nil) {
				destroy(reg->M);
				reg->M = m;
			}
		} while(f != fp);
		ex = parent;
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
	void *source, *destination;

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

	/* Destructors can borrow REG; the original copy operands remain authoritative. */
	source = R.s;
	destination = R.d;
	incmem(source, t);
	if(t->np)
		freeptrs(destination, t);
	memmove(destination, source, size);
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


/* Execute a decoded operation after the native tail entry has left module code.
 * The caller already decoded operands and advanced PC. C handlers may block,
 * switch modules or raise through the ordinary VM error frames. They can also
 * release the source Jit mapping: do not dereference it after either callback.
 */
int
cjitexec(REG *r, int op, int (*fast)(REG*))
{
	void (*handler)(void);

	if(r != &R || op <= INOP || op >= MAXDIS || op == IECLR ||
	   (uintptr)fast == (uintptr)-1)
		return 0;
	if(fast != nil && fast(r))
		return 1;
	handler = optab[op];
	if(handler == nil || (uintptr)handler == (uintptr)-1){
		error(exOp);
		return 0;
	}
	handler();
	return 1;
}


void
opinit(void)
{
	int i;

	for(i = 0; i < 256; i++)
		if(optab[i] == nil)
			optab[i] = badop;
}


/* Validate each bytecode fetch before callbacks, then execute one scheduler step.
 * Capture opcode/PC before the decoder publishes REG fields: the instruction
 * image can alias those fields. C/AOT/JIT callbacks may replace the module. */
void
xec(Prog *p)
{
	VmFetch fetched;
	Jit *j;
	void (*enter)(void);
	int (*step)(void), (*fast)(REG*), (*complete)(REG*);
	int op;

	vmstart(p);
	for(;;){
		if(!vmfetch(&fetched)){
			vmnative();
			break;
		}
		op = fetched.op;
		/* Successful translated steps own their register/PC publication. */
		memmove(&step, (uchar*)fetched.module+O(Module, cstep), sizeof(step));
		if(step == (int(*)(void))H)
			error(exCompile);
		if(step != nil && step())
			goto executed;
		vmdecode(&fetched);

		memmove(&j, (uchar*)fetched.module+O(Module, jit), sizeof(j));
		fast = complete = nil;
		enter = nil;
		if(j != nil){
			if(j == H || (uintptr)j > (uintptr)-1-sizeof(*j))
				error(exCompile);
			memmove(&fast, (uchar*)j+O(Jit, entry)+(uintptr)op*sizeof(fast), sizeof(fast));
			memmove(&complete, (uchar*)j+O(Jit, dispatch)+(uintptr)op*sizeof(complete), sizeof(complete));
			memmove(&enter, (uchar*)j+O(Jit, enter), sizeof(enter));
		}
		if(fast != nil || complete != nil){
			if(fast == (int(*)(REG*))H || complete == (int(*)(REG*))H ||
			   enter == (void(*)(void))H)
				error(exCompile);
			if(enter != nil)
				enter();
			/* No Jit/Module reads follow callbacks that can release their owners. */
			if(fast != nil){
				if(fast(&R))
					goto executed;
			}else if(complete(&R))
				goto executed;
		}
		vmrun(&fetched);
executed:
		if(!vmyield())
			break;
	}
	p->R = R;
}
