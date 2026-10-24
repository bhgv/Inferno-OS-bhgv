#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <fenv.h>
#include <stdio.h>

#ifndef REAL_TARGET_ONLY
#define REAL_TARGET_ONLY 0
#endif
REG R;
#define OP(name) void name(void)
#define F(r) (*(REAL*)R.r)
#define JMP(r) (R.PC = *(Inst**)R.r)

/* Process inspection is unavailable; retain address and stack instrumentation. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION

/* Keep each oracle separate so equality and ordered predicates retain their FP flags. */
static int equalreal(REAL a, REAL b) { volatile REAL x=a,y=b; return x==y; }
/* Real inequality is also true for unordered operands. */
static int unequalreal(REAL a, REAL b) { volatile REAL x=a,y=b; return x!=y; }
/* Ordered less-than oracle. */
static int lessreal(REAL a, REAL b) { volatile REAL x=a,y=b; return x<y; }
/* Ordered less-than-or-equal oracle. */
static int lessequalreal(REAL a, REAL b) { volatile REAL x=a,y=b; return x<=y; }
/* Ordered greater-than oracle. */
static int greaterreal(REAL a, REAL b) { volatile REAL x=a,y=b; return x>y; }
/* Ordered greater-than-or-equal oracle. */
static int greaterequalreal(REAL a, REAL b) { volatile REAL x=a,y=b; return x>=y; }

/* Non-normal operands must retain the host's actual comparison behavior. */
static int
nativevalue(uvlong bits)
{
	int exponent;

	exponent = (bits>>52)&2047;
	return (bits&0xfffffffffffffULL) == 0 || (exponent > 0 && exponent < 2047);
}

/* Compare exact PC/data effects across every operand/target byte alignment. */
int
main(void)
{
	void (*operations[])(void) = {beqf, bnef, bltf, blef, bgtf, bgef};
	int (*volatile oracles[])(REAL, REAL) = {equalreal, unequalreal, lessreal, lessequalreal, greaterreal, greaterequalreal};
	uvlong bits[] = {0, 0x8000000000000000ULL, 0x3ff0000000000000ULL, 0xbff0000000000000ULL,
		0x0010000000000000ULL, 0x8010000000000000ULL, 0x7fefffffffffffffULL, 0xffefffffffffffffULL,
		0x7ff0000000000000ULL, 0xfff0000000000000ULL, 1, 0x8000000000000001ULL,
		0xfffffffffffffULL, 0x800fffffffffffffULL, 0x7ff8000000000042ULL, 0xfff8000000000042ULL,
		0x7ff0000000000042ULL, 0xfff0000000000042ULL};
	union { uvlong align; uchar bytes[128]; } slots;
	uchar before[128];
	Inst instructions[3], *target = instructions+2;
	REAL a, b;
	int rounding[] = {FE_TONEAREST, FE_DOWNWARD};
	int savedround, mode, align, op, i, j, take, initialflags, expectedflags;
	unsigned long checks;

	checks = 0;
	savedround = fegetround();
	for(mode = 0; mode < nelem(rounding); mode++){
		assert(fesetround(rounding[mode]) == 0);
		for(align = 0; align < 8; align++)
		for(op = 0; op < nelem(operations); op++)
		for(i = 0; i < nelem(bits); i++)
		for(j = 0; j < nelem(bits); j++){
			memmove(&a, &bits[i], sizeof(a));
			memmove(&b, &bits[j], sizeof(b));
			memset(slots.bytes, 0xa5, sizeof(slots.bytes));
			R.s = slots.bytes+16+(REAL_TARGET_ONLY ? 0 : align);
			R.m = slots.bytes+48+(REAL_TARGET_ONLY ? 0 : align);
			R.d = slots.bytes+80+align;
			memmove(R.s, &bits[i], sizeof(a));
			memmove(R.m, &bits[j], sizeof(b));
			memmove(R.d, &target, sizeof(target));
			memmove(before, slots.bytes, sizeof(before));
			R.PC = instructions;
			feclearexcept(FE_ALL_EXCEPT);
			feraiseexcept(mode == 0 ? FE_INEXACT : FE_ALL_EXCEPT);
			initialflags = fetestexcept(FE_ALL_EXCEPT);
			take = oracles[op](a, b);
			expectedflags = fetestexcept(FE_ALL_EXCEPT);
			feclearexcept(FE_ALL_EXCEPT);
			feraiseexcept(mode == 0 ? FE_INEXACT : FE_ALL_EXCEPT);
			operations[op]();
			assert(fetestexcept(FE_ALL_EXCEPT) == expectedflags);
			assert(R.PC == (take ? target : instructions));
			assert(!memcmp(before, slots.bytes, sizeof(before)) && fegetround() == rounding[mode]);
			if(nativevalue(bits[i]) && nativevalue(bits[j]))
				assert(fetestexcept(FE_ALL_EXCEPT) == initialflags);
			checks++;
			if(!take){
				R.PC = instructions;
				R.d = (void*)1;
				operations[op]();
				assert(R.PC == instructions && !memcmp(before, slots.bytes, sizeof(before)));
				checks++;
			}
		}
	}
	assert(fesetround(savedround) == 0);
	printf("PASS real branches: %lu PC/alignment/FP-state checks, WORD%zu\n", checks, sizeof(WORD)*8);
	return 0;
}
