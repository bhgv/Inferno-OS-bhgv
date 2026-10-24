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

	memmove(&source, R.s, sizeof(source));
	result = -source;
	memmove(R.d, &result, sizeof(result));
}
/* Read native-width targets without imposing host pointer alignment. */
OP(jmp)
{
	JMP(d);
}
/* Validate a Dis program index before turning it into a code pointer. */
OP(movpc)
{
	WORD pc;
	Inst *target;
	Modlink *ml;

	ml = R.M;
	if(ml == H || ml == nil || ml->m == H || ml->m == nil)
		error(exModule);
	memmove(&pc, R.s, sizeof(pc));
	if(pc < 0 || pc >= ml->m->nprog || ml->m->prog == nil)
		error(exBounds);
	target = &ml->m->prog[pc];
	memmove(R.d, &target, sizeof(target));
}
/* Capture possibly four-byte-aligned lengths before validating or copying. */
OP(movm)
{
	WORD n;

	memmove(&n, R.m, sizeof(n));
	if(n < 0)
		error(exBounds);
	if(n != 0)
		memmove(R.d, R.s, n);
}
/* Preserve the active VM word-width address representation at any slot alignment. */
OP(lea)
{
	WORD address;

	address = (WORD)R.s;
	memmove(R.d, &address, sizeof(address));
}
/* Preserve overlapping scalar slots without requiring host alignment. */
OP(movb)
{
	memmove(R.d, R.s, sizeof(BYTE));
}
/* Preserve overlapping scalar slots without requiring host alignment. */
OP(movw)
{
	memmove(R.d, R.s, sizeof(WORD));
}
/* Preserve overlapping scalar slots without requiring host alignment. */
OP(movf)
{
	memmove(R.d, R.s, sizeof(REAL));
}
/* Preserve overlapping scalar slots without requiring host alignment. */
OP(movl)
{
	memmove(R.d, R.s, sizeof(LONG));
}
/* Capture the BYTE source before converting into a possibly overlapping WORD slot. */
OP(cvtbw)
{
	BYTE source;
	WORD result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture the WORD source before converting into a possibly overlapping BYTE slot. */
OP(cvtwb)
{
	WORD source;
	BYTE result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture possibly unaligned real bits before evaluating and publishing the result. */
OP(cvtrf)
{
	SREAL source;
	REAL result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture possibly unaligned real bits before evaluating and publishing the result. */
OP(cvtfr)
{
	REAL source;
	SREAL result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture the WORD source before converting into a possibly overlapping SHORT slot. */
OP(cvtws)
{
	WORD source;
	SHORT result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture the SHORT source before converting into a possibly overlapping WORD slot. */
OP(cvtsw)
{
	SHORT source;
	WORD result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture the unaligned integer before converting with the current FP environment. */
OP(cvtwf)
{
	WORD source;
	REAL result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(addb)
{
	BYTE left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left + right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(addw)
{
	UWORD left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left + right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(addl)
{
	ULONG left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left + right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture both real operands before an overlapping, possibly unaligned store. */
OP(addf)
{
	REAL left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left + right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(subb)
{
	BYTE left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left - right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(subw)
{
	UWORD left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left - right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(subl)
{
	ULONG left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left - right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture both real operands before an overlapping, possibly unaligned store. */
OP(subf)
{
	REAL left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left - right;
	memmove(R.d, &result, sizeof(result));
}
/* Preserve the VM's divide-by-zero exception for byte operands too. */
OP(divb)
{
	BYTE dividend, divisor, result;

	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	memmove(&dividend, R.m, sizeof(dividend));
	result = dividend / divisor;
	memmove(R.d, &result, sizeof(result));
}
/* Avoid both zero division and the signed minimum / -1 C overflow case. */
OP(divw)
{
	WORD dividend, divisor, result;

	memmove(&dividend, R.m, sizeof(dividend));
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
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

	memmove(&dividend, R.m, sizeof(dividend));
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
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

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left / right;
	memmove(R.d, &result, sizeof(result));
}
/* Byte remainder by zero must not reach the host C operator. */
OP(modb)
{
	BYTE dividend, divisor, result;

	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
	memmove(&dividend, R.m, sizeof(dividend));
	result = dividend % divisor;
	memmove(R.d, &result, sizeof(result));
}
/* Avoid zero division and the signed minimum % -1 C overflow case. */
OP(modw)
{
	WORD dividend, divisor, result;

	memmove(&dividend, R.m, sizeof(dividend));
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
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

	memmove(&dividend, R.m, sizeof(dividend));
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
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

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left * right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(mulw)
{
	UWORD left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left * right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture unaligned operands and keep arithmetic independent of signed overflow. */
OP(mull)
{
	ULONG left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left * right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture both real operands before an overlapping, possibly unaligned store. */
OP(mulf)
{
	REAL left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left * right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(andb)
{
	BYTE left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left & right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(andw)
{
	UWORD left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left & right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(andl)
{
	ULONG left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left & right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(xorb)
{
	BYTE left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left ^ right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(xorw)
{
	UWORD left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left ^ right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(xorl)
{
	ULONG left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left ^ right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(orb)
{
	BYTE left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left | right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(orw)
{
	UWORD left, right, result;

	memmove(&left, R.m, sizeof(left));
	memmove(&right, R.s, sizeof(right));
	result = left | right;
	memmove(R.d, &result, sizeof(result));
}
/* Capture integer operands before evaluating modular arithmetic or bit operations. */
OP(orl)
{
	ULONG left, right, result;

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
OP(shlb)
{
	WORD n;
	BYTE value, result;

	memmove(&n, R.s, sizeof(n));
	n = shiftcount(n, 8);
	memmove(&value, R.m, sizeof(value));
	result = value << n;
	memmove(R.d, &result, sizeof(result));
}
/* Shift word bits unsigned to avoid signed-left-shift overflow. */
OP(shlw)
{
	WORD n;
	UWORD value, result;

	memmove(&n, R.s, sizeof(n));
	n = shiftcount(n, sizeof(WORD)*8);
	memmove(&value, R.m, sizeof(value));
	result = value << n;
	memmove(R.d, &result, sizeof(result));
}
/* Shift long bits unsigned to avoid signed-left-shift overflow. */
OP(shll)
{
	WORD n;
	ULONG value, result;

	memmove(&n, R.s, sizeof(n));
	n = shiftcount(n, sizeof(LONG)*8);
	memmove(&value, R.m, sizeof(value));
	result = value << n;
	memmove(R.d, &result, sizeof(result));
}
/* Reject invalid byte shift counts before evaluating the shift. */
OP(shrb)
{
	WORD n;
	BYTE value, result;

	memmove(&n, R.s, sizeof(n));
	n = shiftcount(n, 8);
	memmove(&value, R.m, sizeof(value));
	result = value >> n;
	memmove(R.d, &result, sizeof(result));
}
/* Preserve arithmetic right shift for signed Limbo words. */
OP(shrw)
{
	WORD n;
	WORD value, result;

	memmove(&n, R.s, sizeof(n));
	n = shiftcount(n, sizeof(WORD)*8);
	memmove(&value, R.m, sizeof(value));
	result = value >> n;
	memmove(R.d, &result, sizeof(result));
}
/* Preserve arithmetic right shift for signed Limbo long values. */
OP(shrl)
{
	WORD n;
	LONG value, result;

	memmove(&n, R.s, sizeof(n));
	n = shiftcount(n, sizeof(LONG)*8);
	memmove(&value, R.m, sizeof(value));
	result = value >> n;
	memmove(R.d, &result, sizeof(result));
}
/* Validate counts before logical right shifts of unsigned word bits. */
OP(lsrw)
{
	WORD n;
	UWORD value, result;

	memmove(&n, R.s, sizeof(n));
	n = shiftcount(n, sizeof(WORD)*8);
	memmove(&value, R.m, sizeof(value));
	result = value >> n;
	memmove(R.d, &result, sizeof(result));
}
/* Validate counts before logical right shifts of unsigned long bits. */
OP(lsrl)
{
	WORD n;
	ULONG value, result;

	memmove(&n, R.s, sizeof(n));
	n = shiftcount(n, sizeof(LONG)*8);
	memmove(&value, R.m, sizeof(value));
	result = value >> n;
	memmove(R.d, &result, sizeof(result));
}
/* Capture both BYTE operands before conditionally reading the native target. */
OP(beqb)
{
	BYTE left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left == right)
		JMP(d);
}
/* Capture both BYTE operands before conditionally reading the native target. */
OP(bneb)
{
	BYTE left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left != right)
		JMP(d);
}
/* Capture both BYTE operands before conditionally reading the native target. */
OP(bltb)
{
	BYTE left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left < right)
		JMP(d);
}
/* Capture both BYTE operands before conditionally reading the native target. */
OP(bleb)
{
	BYTE left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left <= right)
		JMP(d);
}
/* Capture both BYTE operands before conditionally reading the native target. */
OP(bgtb)
{
	BYTE left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left > right)
		JMP(d);
}
/* Capture both BYTE operands before conditionally reading the native target. */
OP(bgeb)
{
	BYTE left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left >= right)
		JMP(d);
}
/* Capture both WORD operands before conditionally reading the native target. */
OP(beqw)
{
	WORD left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left == right)
		JMP(d);
}
/* Capture both WORD operands before conditionally reading the native target. */
OP(bnew)
{
	WORD left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left != right)
		JMP(d);
}
/* Capture both WORD operands before conditionally reading the native target. */
OP(bltw)
{
	WORD left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left < right)
		JMP(d);
}
/* Capture both WORD operands before conditionally reading the native target. */
OP(blew)
{
	WORD left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left <= right)
		JMP(d);
}
/* Capture both WORD operands before conditionally reading the native target. */
OP(bgtw)
{
	WORD left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left > right)
		JMP(d);
}
/* Capture both WORD operands before conditionally reading the native target. */
OP(bgew)
{
	WORD left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left >= right)
		JMP(d);
}
/* Capture both LONG operands before conditionally reading the native target. */
OP(beql)
{
	LONG left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left == right)
		JMP(d);
}
/* Capture both LONG operands before conditionally reading the native target. */
OP(bnel)
{
	LONG left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left != right)
		JMP(d);
}
/* Capture both LONG operands before conditionally reading the native target. */
OP(bltl)
{
	LONG left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left < right)
		JMP(d);
}
/* Capture both LONG operands before conditionally reading the native target. */
OP(blel)
{
	LONG left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left <= right)
		JMP(d);
}
/* Capture both LONG operands before conditionally reading the native target. */
OP(bgtl)
{
	LONG left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left > right)
		JMP(d);
}
/* Capture both LONG operands before conditionally reading the native target. */
OP(bgel)
{
	LONG left, right;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	if(left >= right)
		JMP(d);
}
/* Compare captured real values and read a branch target only when it is taken. */
static void
branchreal(int condition)
{
	REAL left, right;
	int take;

	memmove(&left, R.s, sizeof(left));
	memmove(&right, R.m, sizeof(right));
	take = 0;
	switch(condition){
	case 0: take = left == right; break;
	case 1: take = left != right; break;
	case 2: take = left < right; break;
	case 3: take = left <= right; break;
	case 4: take = left > right; break;
	case 5: take = left >= right; break;
	}
	if(take)
		memmove(&R.PC, R.d, sizeof(R.PC));
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
	if(take)
		JMP(d);
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

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture the LONG source before converting into a possibly overlapping WORD slot. */
OP(cvtlw)
{
	LONG source;
	WORD result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Capture the unaligned integer before converting with the current FP environment. */
OP(cvtlf)
{
	LONG source;
	REAL result;

	memmove(&source, R.s, sizeof(source));
	result = source;
	memmove(R.d, &result, sizeof(result));
}
/* Round first, then reject non-finite or out-of-range big conversions. */
OP(cvtfl)
{
	REAL f;
	LONG result;

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
OP(cvtcl)
{
	String *s;

	s = S(s);
	if(s == H)
		V(d) = 0;
	else
		V(d) = strtoll(string2c(s), nil, 10);
}

/* Capture operands before computing a modular power or its negative-exponent rule. */
OP(iexpw)
{
	WORD x, n, answer;
	UWORD base, exponent, result;

	memmove(&x, R.m, sizeof(x));
	memmove(&n, R.s, sizeof(n));
	if(n < 0){
		if(x == 0)
			error(exZdiv);
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

	memmove(&x, R.m, sizeof(x));
	memmove(&n, R.s, sizeof(n));
	if(n < 0){
		if(x == 0)
			error(exZdiv);
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

	if(array == H || array == nil || index < 0 || index >= array->len)
		error(exBounds);
	if(width < 0){
		if(array->t == nil || array->t == H || array->t->size < 0)
			error(exBounds);
		width = array->t->size;
	}
	/* Empty records can have no backing bytes, even at a nonzero index. */
	if(width == 0)
		return array->data;
	if(array->data == nil || array->data == H ||
	   (uvlong)index > 0x7fffffff/(uint)width)
		error(exBounds);
	offset = (ulong)index*width;
	if((uintptr)array->data > (uintptr)-1-offset)
		error(exBounds);
	return (uchar*)((uintptr)array->data+offset);
}

/* Capture decoded array slots before computing or publishing an aliased address. */
static void
arrayindex(int width)
{
	Array *array;
	WORD index, address;

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
/* Acquire a captured reference before replacing a potentially unaligned pointer slot. */
OP(movp)
{
	Heap *h;
	WORD *dv, *sv;

	memmove(&sv, R.s, sizeof(sv));
	if(sv == nil)
		error(exNilref);
	if(sv != H) {
		h = D2H(sv);
		h->ref++;
		Setmark(h);
	}
	memmove(&dv, R.d, sizeof(dv));
	memmove(R.d, &sv, sizeof(sv));
	destroy(dv);
}
/* Resolve the full Limbo index through the owner: Loader.tnew may move its table. */
static Type*
moduletype(Modlink *ml, WORD index)
{
	Module *m;
	Type *t;

	if(ml == H || ml == nil || ml->m == nil || ml->m == H)
		error(exModule);
	m = ml->m;
	if(m->type == nil || index < 0 || index >= m->ntype)
		error("invalid type");
	t = m->type[index];
	if(t == nil || t == H || t->size < 0)
		error("invalid type");
	return t;
}

/* Resolve mapped metadata first; empty records neither access data nor change references. */
OP(movmp)
{
	Type *t;
	WORD index;

	memmove(&index, R.m, sizeof(index));
	t = moduletype(R.M, index);
	if(t->size == 0)
		return;

	incmem(R.s, t);
	if (t->np)
		freeptrs(R.d, t);
	memmove(R.d, R.s, t->size);
}
/* Validate the descriptor before allocating or replacing the destination. */
OP(new)
{
	Heap *h;
	WORD *t, *object;
	WORD index;

	memmove(&index, R.s, sizeof(index));
	h = heap(moduletype(R.M, index));
	memmove(&t, R.d, sizeof(t));
	object = H2D(WORD*, h);
	memmove(R.d, &object, sizeof(object));
	destroy(t);
}
/* Use the current module table for a zero-initialized object. */
OP(newz)
{
	Heap *h;
	WORD *t, *object;
	WORD index;

	memmove(&index, R.s, sizeof(index));
	h = heapz(moduletype(R.M, index));
	memmove(&t, R.d, sizeof(t));
	object = H2D(WORD*, h);
	memmove(R.d, &object, sizeof(object));
	destroy(t);
}
/* Resolve the requested descriptor in the source module before allocation. */
OP(mnewz)
{
	Heap *h;
	WORD *t, *object;
	WORD index;
	Modlink *ml;

	memmove(&ml, R.s, sizeof(ml));
	if(ml == H)
		error(exModule);
	memmove(&index, R.m, sizeof(index));
	h = heapz(moduletype(ml, index));
	memmove(&t, R.d, sizeof(t));
	object = H2D(WORD*, h);
	memmove(R.d, &object, sizeof(object));
	destroy(t);
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
	if(t == nil || t == H || t->size < (int)sizeof(Frame) ||
	   R.SP == nil || R.TS == nil || R.SP > R.TS)
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
/* Validate the current element descriptor before checking array geometry. */
OP(newa)
{
	WORD sz, index;
	Type *t;
	Heap *h;
	Array *a, *at;

	memmove(&index, R.m, sizeof(index));
	t = moduletype(R.M, index);
	memmove(&sz, R.s, sizeof(sz));
	acheck(t->size, sz);
	h = nheap(sizeof(Array) + (t->size*sz));
	h->t = &Tarray;
	Tarray.ref++;
	a = H2D(Array*, h);
	a->t = t;
	a->len = sz;
	a->root = H;
	a->data = (uchar*)a + sizeof(Array);
	initarray(t, a);

	memmove(&at, R.d, sizeof(at));
	memmove(R.d, &a, sizeof(a));
	destroy(at);
}
/* Use current element metadata for zeroed storage and pointer initialization. */
OP(newaz)
{
	WORD sz, index;
	Type *t;
	Heap *h;
	Array *a, *at;

	memmove(&index, R.m, sizeof(index));
	t = moduletype(R.M, index);
	memmove(&sz, R.s, sizeof(sz));
	acheck(t->size, sz);
	h = nheap(sizeof(Array) + (t->size*sz));
	h->t = &Tarray;
	Tarray.ref++;
	a = H2D(Array*, h);
	a->t = t;
	a->len = sz;
	a->root = H;
	a->data = (uchar*)a + sizeof(Array);
	memset(a->data, 0, t->size*sz);
	initarray(t, a);

	memmove(&at, R.d, sizeof(at));
	memmove(R.d, &a, sizeof(a));
	destroy(at);
}
typedef struct ChannelBuild ChannelBuild;
struct ChannelBuild {
	Type *t;
	void (*mover)(void);
	int len;
	Channel *c;
};

static int
channelbuild(void *arg)
{
	ChannelBuild *b;
	Channel *c;
	b=arg;
	b->c=c=H2D(Channel*,heap(&Tchannel));
	/* Make the partial channel safe for freechan before any further call. */
	c->buf=H; c->send=nil; c->recv=nil; c->aux=nil;
	c->mover=nil; c->mid.w=0; c->front=0; c->size=0;
	c->send=mallocz(sizeof(Progq),1);
	if(c->send == nil)
		error(exNomem);
	c->recv=mallocz(sizeof(Progq),1);
	if(c->recv == nil)
		error(exNomem);
	if(b->len > 0)
		c->buf=H2D(Array*,heaparray(b->t,b->len));
	if(b->mover == movtmp) {
		c->mid.t=b->t;
		b->t->ref++;
	}
	c->mover=b->mover;
	return 1;
}

Channel*
cnewc(Type *t, void (*mover)(void), int len)
{
	ChannelBuild b;
	if(len < 0)
		error(exNegsize);
	if((len > 0 || mover == movtmp) && t == nil)
		error(exHeap);
	if(len > 0)
		acheck(t->size,len);
	b.t=t; b.mover=mover; b.len=len; b.c=H;
	if(!kprotect(channelbuild,&b)) {
		destroy(b.c);
		krethrow();
	}
	return b.c;
}
/* Capture channel capacity and replacement ownership before fallible allocation. */
Channel*
newc(Type *t, void (*mover)(void))
{
	Channel *channel, *oldc;
	WORD len;

	len = 0;
	if(R.m != R.d){
		memmove(&len, R.m, sizeof(len));
		if(len < 0)
			error(exNegsize);
	}
	if((uvlong)len > 0x7fffffff)
		error(exHeap);
	memmove(&oldc, R.d, sizeof(oldc));
	channel = cnewc(t, mover, len);
	memmove(R.d, &channel, sizeof(channel));
	destroy(oldc);
	return channel;
}
OP(newcl)  { newc(&Tlong, movl);  }
OP(newcb)  { newc(&Tbyte, movb);  }
OP(newcw)  { newc(&Tword, movw);  }
OP(newcf)  { newc(&Treal, movf);  }
OP(newcp)  { newc(&Tptr, movp);  }
typedef struct RawChannel RawChannel;
struct RawChannel {
	Type *t;
	WORD size;
};

/* Publish the captured raw width even when allocation replaces its source slot. */
static int
rawchannel(void *arg)
{
	RawChannel *b;
	Channel *c;
	b=arg;
	c=newc(b->t,movm);
	c->mid.w=b->size;
	return 1;
}

/* Capture raw geometry before the destination can overwrite either operand. */
OP(newcm)
{
	RawChannel b;
	WORD capacity;
	int ok;
	memmove(&b.size, R.s, sizeof(b.size));
	if(b.size < 0)
		error(exNegsize);
	if((uvlong)b.size > 0x7fffffff)
		error(exHeap);
	b.t=nil;
	capacity = 0;
	if(R.m != R.d)
		memmove(&capacity, R.m, sizeof(capacity));
	if(R.m != R.d && capacity > 0) {
		b.t=dtype(nil,b.size,nil,0);
		if(b.t == nil)
			error(exNomem);
	}
	ok=kprotect(rawchannel,&b);
	freetype(b.t);
	if(!ok)
		krethrow();
}
/* Create a mapped channel only after validating its payload descriptor. */
OP(newcmp)
{
	WORD index;

	memmove(&index, R.s, sizeof(index));
	newc(moduletype(R.M, index), movtmp);
}
OP(icase)
{
	WORD v, *t, *l, d, n, n2;

	v = W(s);
	t = (WORD*)((WORD)R.d + IBY2WD);
	n = t[-1];
	d = t[n*3];

	while(n > 0) {
		n2 = n >> 1;
		l = t + n2*3;
		if(v < l[0]) {
			n = n2;
			continue;
		}
		if(v >= l[1]) {
			t = l+3;
			n -= n2 + 1;
			continue;
		}
		d = l[2];
		break;
	}
	if(R.M->compiled) {
		R.PC = (Inst*)d;
		return;
	}
	R.PC = R.M->prog + d;
}
OP(casel)
{
	WORD *t, *l, d, n, n2;
	LONG v;

	v = V(s);
	t = (WORD*)((WORD)R.d + 2*IBY2WD);
	n = t[-2];
	d = t[n*6];

	while(n > 0) {
		n2 = n >> 1;
		l = t + n2*6;
		if(v < ((LONG*)l)[0]) {
			n = n2;
			continue;
		}
		if(v >= ((LONG*)l)[1]) {
			t = l+6;
			n -= n2 + 1;
			continue;
		}
		d = l[4];
		break;
	}
	if(R.M->compiled) {
		R.PC = (Inst*)d;
		return;
	}
	R.PC = R.M->prog + d;
}
OP(casec)
{
	WORD *l, *t, *e, n, n2, r;
	String *sl, *sh, *sv;
	
	sv = S(s);
	t = (WORD*)((WORD)R.d + IBY2WD);
	n = t[-1];
	e = t + n*3;
	if(n > 2){
		while(n > 0){
			n2 = n>>1;
			l = t + n2*3;
			sl = (String*)l[0];
			r = stringcmp(sv, sl);
			if(r == 0){
				e = &l[2];
				break;
			}
			if(r < 0){
				n = n2;
				continue;
			}
			sh = (String*)l[1];
			if(sh == H || stringcmp(sv, sh) > 0){
				t = l+3;
				n -= n2+1;
				continue;
			}
			e = &l[2];
			break;
		}
		t = e;
	}
	else{
		while(t < e) {
			sl = (String*)t[0];
			sh = (String*)t[1];
			if(sh == H) {
				if(stringcmp(sl, sv) == 0) {
					t = &t[2];
					goto found;
				}
			}
			else
			if(stringcmp(sl, sv) <= 0 && stringcmp(sh, sv) >= 0) {
				t = &t[2];
				goto found;
			}
			t += 3;
		}
	}
found:
	if(R.M->compiled) {
		R.PC = (Inst*)*t;
		return;
	}
	R.PC = R.M->prog + t[0];
}
OP(igoto)
{
	WORD *t;

	t = (WORD*)((WORD)R.d + (W(s) * IBY2WD));
	if(R.M->compiled) {
		R.PC = (Inst*)t[0];
		return;
	}
	R.PC = R.M->prog + t[0];
}
OP(call)
{
	Frame *f;

	f = T(s);
	f->lr = R.PC;
	f->fp = R.FP;
	R.FP = (uchar*)f;
	JMP(d);
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
	spawnmodule(R.M, *(Inst**)R.d);
}

/* Validate module ownership before reading a child entry or creating its stack. */
OP(mspawn)
{
	Modlink *ml;
	Modl entry;

	ml = *(Modlink**)R.d;
	if(ml == H || ml == nil || ml->m == H || ml->m == nil)
		error(exModule);
	if(ml->prog == nil)
		error(exSpawn);
	if(!moduleentry(ml, W(m), &entry))
		error("invalid mspawn");
	spawnmodule(ml, entry.u.pc);
}
OP(ret)
{
	Frame *f;
	Modlink *m;

	f = (Frame*)R.FP;
	R.FP = f->fp;
	if(R.FP == nil) {
		R.FP = (uchar*)f;
		error("");
	}
	R.SP = (uchar*)f;
	R.PC = f->lr;
	m = f->mr;

	if(f->t == nil)
		unextend(f);
	else if (f->t->np)
		freeptrs(f, f->t);

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
	Modlink *ml, **mp, *t;
	Heap *h;

	if(R.M == H || R.M == nil || R.M->m == H || R.M->m == nil)
		error(exModule);
	m = R.M->m;
	if(!(m->rt & HASLDT))
		error("obsolete dis");
	ldt = moduleimports(m, W(m));
	n = string2c(S(s));
	if(strcmp(n, "$self") == 0) {
		if(R.M->MP == nil)
			error(exModule);
		m->ref++;
		ml = linkmod(m, ldt, 0);
		if(ml != H) {
			ml->MP = R.M->MP;
			if(ml->MP != H){
				h = D2H(ml->MP);
				h->ref++;
				Setmark(h);
			}
		}
	}
	else {
		m = readmod(n, lookmod(n), 1);
		ml = linkmod(m, ldt, 1);
	}

	mp = R.d;
	t = *mp;
	*mp = ml;
	destroy(t);
}
/* Resolve the complete link index before publishing a frame or changing modules. */
OP(mcall)
{
	Heap *h;
	Prog *p;
	Frame *f;
	Modl entry;
	Modlink *ml;

	ml = *(Modlink**)R.d;
	if(ml == H)
		error(exModule);
	if(!moduleentry(ml, W(m), &entry))
		error("invalid mcall");
	f = T(s);
	f->lr = R.PC;
	f->fp = R.FP;
	f->mr = R.M;

	R.FP = (uchar*)f;
	R.M = ml;
	h = D2H(ml);
	h->ref++;

	if(ml->prog == nil) {
		entry.u.runt(f);
		h->ref--;
		R.M = f->mr;
		R.SP = R.FP;
		R.FP = f->fp;
		if(f->t == nil)
			unextend(f);
		else if (f->t->np)
			freeptrs(f, f->t);
		p = currun();
		if(p->kill != nil)
			error(p->kill);
		R.t = 0;
		return;
	}
	R.MP = R.M->MP;
	R.PC = entry.u.pc;
	R.t = 1;

	if(f->mr->compiled != R.M->compiled)
		R.IC = 1;
}
/* Capture and publish array lengths without aligned operand casts. */
OP(lena)
{
	WORD l;
	Array *a;

	memmove(&a, R.s, sizeof(a));
	l = 0;
	if(a != H && a != nil)
		l = a->len;
	memmove(R.d, &l, sizeof(l));
}
/* Validate each list link and keep operand accesses independent of alignment. */
OP(lenl)
{
	WORD l;
	List *a;

	memmove(&a, R.s, sizeof(a));
	l = 0;
	while(a != H) {
		if(a == nil)
			error(exNilref);
		l++;
		a = a->tail;
	}
	memmove(R.d, &l, sizeof(l));
}
/* Retain buffered storage if the received value replaces the channel owner. */
static int
cgetb(Channel *c, void *v)
{
	Array *a;
	void *w;

	if((a = c->buf) == H)
		return 0;
	if(c->size > 0){
		D2H(a)->ref++;
		w = a->data+c->front*a->t->size;
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
static int
cputb(Channel *c, void *v)
{
	Array *a;
	WORD len, r;

	if((a = c->buf) == H)
		return 0;
	len = c->buf->len;
	if(c->size < len){
		r = c->front+c->size;
		if(r >= len)
			r -= len;
		c->size++;
		R.s = v;
		R.m = &c->mid;
		R.d = a->data+r*a->t->size;
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
OP(irecv)
{
	Channel *c;
	Prog *p;

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

	if(c == H)
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

/* Validate and capture the old tail before allocating or publishing a node. */
List*
cons(ulong size, void *slot)
{
	Heap *h;
	List *lv, *l;

	if(size > 0x7fffffff-sizeof(Heap)-
	   (sizeof(List)-sizeof(((List*)0)->data)))
		error(exHeap);
	memmove(&lv, slot, sizeof(lv));
	if(lv == nil)
		error(exNilref);
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

/* Allocate native pointer storage and acquire the original source owner. */
OP(consp)
{
	List *l;
	Heap *h;
	WORD *sv;

	memmove(&sv, R.s, sizeof(sv));
	if(sv == nil)
		error(exNilref);
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
	List *l, *result;

	memmove(&size, R.m, sizeof(size));
	if(size < 0 || (uvlong)size > 0x7fffffff-sizeof(Heap)-
	   (sizeof(List)-sizeof(((List*)0)->data)))
		error(exHeap);
	memmove(&result, R.d, sizeof(result));
	l = cons((ulong)size, &result);
	if(size != 0)
		memmove(l->data, R.s, (int)size);
	memmove(R.d, &result, sizeof(result));
}

/* Copy mapped payload and acquire its references before publishing an aliased result. */
OP(consmp)
{
	List *l, *result;
	Type *t;
	WORD index;

	memmove(&index, R.m, sizeof(index));
	t = moduletype(R.M, index);
	memmove(&result, R.d, sizeof(result));
	l = cons(t->size, &result);
	incmem(R.s, t);
	if(t->size != 0)
		memmove(l->data, R.s, t->size);
	l->t = t;
	t->ref++;
	memmove(R.d, &result, sizeof(result));
}
/* Capture a possibly four-byte-aligned owner before reading list storage. */
static List*
listhead(void)
{
	List *l;

	memmove(&l, R.s, sizeof(l));
	if(l == H || l == nil)
		error(exNilref);
	return l;
}

/* A nil list has no head payload. */
OP(headb)
{
	List *l;

	l = listhead();
	memmove(R.d, l->data, sizeof(BYTE));
}
/* Reject nil before reading a word list head. */
OP(headw)
{
	List *l;

	l = listhead();
	memmove(R.d, l->data, sizeof(WORD));
}
/* Reject nil before reading a long list head. */
OP(headl)
{
	List *l;

	l = listhead();
	memmove(R.d, l->data, sizeof(LONG));
}
/* Validate the list before copying its pointer-valued head. */
OP(headp)
{
	List *l;

	l = listhead();
	R.s = l->data;
	movp();
}
/* Reject nil before reading a real-valued list head. */
OP(headf)
{
	List *l;

	l = listhead();
	memmove(R.d, l->data, sizeof(REAL));
}
/* Validate both the list reference and raw payload size before copying. */
OP(headm)
{
	List *l;
	WORD n;

	l = listhead();
	memmove(&n, R.m, sizeof(n));
	if(n < 0)
		error(exBounds);
	if(n != 0)
		memmove(R.d, l->data, n);
}
/* Keep the head storage alive when replacing the destination releases its list. */
OP(headmp)
{
	List *l;
	Type *t;
	WORD index;

	l = listhead();
	memmove(&index, R.m, sizeof(index));
	t = moduletype(R.M, index);
	if(t->size == 0)
		return;

	/* Resolve fallible metadata before acquiring the temporary source owner. */
	D2H(l)->ref++;
	R.s = l->data;
	incmem(R.s, t);
	if(t->np)
		freeptrs(R.d, t);
	memmove(R.d, R.s, t->size);
	destroy(l);
}
/* The tail operation must raise a VM nil-reference error on an empty list. */
OP(tail)
{
	List *l;

	l = listhead();
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

/* Preserve the language slice bounds even when an empty source is VM nil. */
OP(slicea)
{
	Type *t;
	Heap *h;
	Array *at, *ss, *ds;
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
	h = heap(&Tarray);
	ss = H2D(Array*, h);
	ss->len = n;
	ss->data = data;
	ss->t = t;
	t->ref++;

	if(ds->root != H) {			/* slicing a slice */
		ds = ds->root;
		h = D2H(ds);
		h->ref++;
		memmove(&at, R.d, sizeof(at));
		memmove(R.d, &ss, sizeof(ss));
		ss->root = ds;
		destroy(at);
	}
	else {
		h = D2H(ds);
		ss->root = ds;
		memmove(R.d, &ss, sizeof(ss));
	}
	Setmark(h);
}
/* Validate empty spans and retain mapped source storage through destination release. */
OP(slicela)
{
	Type *t;
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
	D2H(ss)->ref++;
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
/* Compare captured runtime owners without dereferencing C-null metadata. */
OP(tcmp)
{
	void *s, *d;

	memmove(&s, R.s, sizeof(s));
	memmove(&d, R.d, sizeof(d));
	if(s != H && (s == nil || d == nil || d == H || D2H(s)->t != D2H(d)->t))
		error(exTcheck);
}
OP(eclr)
{
	/* spare slot */
}
OP(badop)
{
	error(exOp);
}
OP(iraise)
{
	void *v;
	Heap *h;
	Prog *p;

	p = currun();
	v = T(s);
	if(v == H)
		error(exNilref);
	p->exval = v;
	h = D2H(v);
	h->ref++;
	if(h->t == &Tstring)
		error(string2c((String*)v));
	else
		error(string2c(*(String**)v));
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

	memmove(&p, R.FP+(NREG+2)*IBY2WD, sizeof(p));
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

	memmove(&p, R.FP+(NREG+2)*IBY2WD, sizeof(p));
	memmove(&divisor, R.s, sizeof(divisor));
	if(divisor == 0)
		error(exZdiv);
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

	memmove(&p, R.m, sizeof(p));
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

	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	memmove(&p, R.FP+(NREG+2)*IBY2WD, sizeof(p));
	memmove(&a, R.FP+NREG*IBY2WD, sizeof(a));
	if(a == 0)
		error(exZdiv);
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

	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	memmove(&p, R.FP+(NREG+2)*IBY2WD, sizeof(p));
	memmove(&b, R.FP+NREG*IBY2WD, sizeof(b));
	if(y == 0)
		error(exZdiv);
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

	memmove(&x, R.s, sizeof(x));
	memmove(&p, R.m, sizeof(p));
	memmove(&a, R.FP+NREG*IBY2WD, sizeof(a));
	if(a == 0)
		error(exZdiv);
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

	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	memmove(&p, R.FP+(NREG+2)*IBY2WD, sizeof(p));
	memmove(&a, R.FP+NREG*IBY2WD, sizeof(a));
	if(a == 0)
		error(exZdiv);
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

	memmove(&x, R.m, sizeof(x));
	memmove(&y, R.s, sizeof(y));
	memmove(&p, R.FP+(NREG+2)*IBY2WD, sizeof(p));
	memmove(&b, R.FP+NREG*IBY2WD, sizeof(b));
	if(y == 0)
		error(exZdiv);
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

	memmove(&x, R.s, sizeof(x));
	memmove(&p, R.m, sizeof(p));
	memmove(&a, R.FP+NREG*IBY2WD, sizeof(a));
	if(a == 0)
		error(exZdiv);
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

	memmove(&source, R.s, sizeof(source));
	memmove(&scale, R.m, sizeof(scale));
	result = (REAL)source*scale;
	memmove(R.d, &result, sizeof(result));
}

/* Retain the current module before publishing into an unaligned owner slot. */
OP(self)
{
	Modlink *ml, *t;
	Heap *h;

	ml = R.M;
	if(ml == nil || ml == H)
		error(exModule);
	h = D2H(ml);
	h->ref++;
	Setmark(h);
	memmove(&t, R.d, sizeof(t));
	memmove(R.d, &ml, sizeof(ml));
	destroy(t);
}

void
destroystack(REG *reg)
{
	Type *t;
	Frame *f, *fp;
	Modlink *m;
	Stkext *sx;
	uchar *ex;

	ex = reg->EX;
	reg->EX = nil;
	while(ex != nil) {
		sx = (Stkext*)ex;
		fp = sx->reg.tos.fr;
		do {
			f = (Frame*)reg->FP;
			if(f == nil)
				break;
			reg->FP = f->fp;
			t = f->t;
			if(t == nil)
				t = sx->reg.TR;
			m = f->mr;
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

/* Channel metadata is a native Type pointer, independently of the VM word width. */
void
movtmp(void)
{
	Type *t;

	t = *(Type**)R.m;
	if(t->size == 0)
		return;

	incmem(R.s, t);
	if (t->np)
		freeptrs(R.d, t);
	memmove(R.d, R.s, t->size);
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
